//
// Created by xzl on 8/30/17.
//

#ifndef CREEK_UNBOUNDEDSEQEVAL_H
#define CREEK_UNBOUNDEDSEQEVAL_H

#include "pipeline-config.h"
#include "SourceSeq.h"
#include "UnboundedInMemEvaluatorBase.h"

/* xzl: emit RecordSeqBundle of T
 * cf: the class above
 * T: element name, eg kvpair
 *
 * cf UnboundedInMemEval.h, UnboundedInMemEvaluatorBase.h
 *
 * */
template<typename T>
class SourceSeqEval
		: public UnboundedInMemEvaluatorBase<T, RecordSeqBundle, SourceSeq<T>> {
	template<class X> using BundleT = RecordSeqBundle<X>; // compatible with UnboundedInMemEvaluator_2out
//	using TransformT = typename UnboundedInMemEvaluatorBase<T, RecordSeqBundle>::TransformT;
	using TransformT = SourceSeq<T>;
	using BaseT = UnboundedInMemEvaluatorBase<T, RecordSeqBundle, SourceSeq<T>>;

	using TicketT = std::future<void>;

public:

	SourceSeqEval(int node) { }

private:
	/* Given a bunch of items and a bunch of tasks, return the range of items for task X
	 *
	 * moved from WindowKeyedReducerEval (XXX merge later)
	 * task_id: zero based.
	 * return: <start, cnt> */
	static pair<int,int> get_range(int num_items, int num_tasks, int task_id) {
		/* not impl yet */
		xzl_bug_on(num_items == 0);

		xzl_bug_on(task_id > num_tasks - 1);

		int items_per_task  = num_items / num_tasks;

		/* give first @num_items each 1 item */
		if (num_items < num_tasks) {
			if (task_id <= num_items - 1)
				return make_pair(task_id, 1);
			else
				return make_pair(0, 0);
		}

		/* task 0..n-2 has items_per_task items. */
		if (task_id != num_tasks - 1)
			return make_pair(task_id * items_per_task, items_per_task);

		if (task_id == num_tasks - 1) {
			int nitems = num_items - task_id * items_per_task;
			xzl_bug_on(nitems < 0);
			return make_pair(task_id * items_per_task, nitems);
		}

		xzl_bug("bug. never reach here");
		return make_pair(-1, -1);
	}

public:

	void evaluate(TransformT* t, EvaluationBundleContext *c,
								shared_ptr<BundleBase> bundle_ptr = nullptr) override
	{
		std::vector<PValue*> out(t->outputs);
		xzl_bug_on(t->num_outputs > out.size());
		for (auto i = 0u; i < t->num_outputs; i++)
			xzl_bug_on(!out[i]);

		etime punc_interval = 1000 * (t->punc_interval_ms);

		etime delta = 1000 * (t->punc_interval_ms) / config.bundles_per_epoch;  /* etime increment between two adjacent bundles */

		const uint64_t records_per_bundle =
			t->records_per_interval / config.bundles_per_epoch;

		/* An infi loop that emit bundles to all NUMA nodes periodically.
		 * spawn downstream eval to consume the bundles.
		 *
		 * NB: we can only sleep in the main thread (which is not
		 * managed by the NUMA thread pool).
		 */

		const int num_nodes = numa_max_node() + 1;

		/* the global offset into each NUMA buffer to avoid repeated content.
		 * (since each buffer has same content)
		 */
		vector<uint64_t> offset(t->num_outputs);
		for (auto & o : offset)
			o = 0;

		uint64_t exec_us_per_epoch = 1e6 * t->records_per_interval / t->target_tput; /* source exec time for each epoch */

		xzl_bug_on(t->num_source_tasks <= 0 || t->num_source_tasks > c->num_workers);
		const int total_tasks = t->num_source_tasks;

		vector <std::future<void>> futures;

		EE("set the initial BaseT::current_ts");
		BaseT::current_ts = 10 * 1000 * 1000;

		while (true) {

			boost::posix_time::ptime start_tick = \
                      boost::posix_time::microsec_clock::local_time();

#if defined(USE_TZ) && defined(DEBUG)
//#if defined(USE_TZ)
			show_sbuff_resource();  /* ask tz to print mem usage */
#endif

			/*
			 * Each bundle is to be consumed by a new task.
			 *
			 * Will NOT block
			 */
			for (int task_id = 0; task_id < total_tasks; task_id++) {

				/* Define a lambda func inline (XXX can be out of line)
				 *
				 * each worker works on a range of bundles in the epoch */
				//			auto source_task_lambda = [t, &total_tasks, &bundle_per_internval, task_id](int id)
				auto source_task_lambda = [t, &total_tasks,
						this, &delta, &out, &c, &records_per_bundle, &num_nodes, task_id, &offset](int id) {
						auto range = get_range(config.bundles_per_epoch, total_tasks, task_id);

						for (auto oid = 0u; oid < t->num_outputs; oid++) { /* each task produces to all outputs */
							auto local_offset = (offset[oid] + records_per_bundle * range.first) % t->buffer_size_records;

							I("source worker %d: bundle range [%d, %d). record offset %lu; total records %lu",
								task_id, range.first, range.first + range.second, local_offset,
								t->buffer_size_records);

							for (int i = range.first; i < range.first + range.second; i++) {
								/* construct the bundles by reading NUMA buffers round-robin */
								int nodeid = (i % num_nodes);

								/* assemble a bundle by drawing records from the corresponding
									* NUMA record buffer. */
#ifdef USE_UARRAY
								xzl_bug_on_msg(records_per_bundle > t->buffer_size_records,
															 "buffer no enough records for even 1 bundle");

								shared_ptr<BundleT<T>> bundle = make_shared<BundleT<T>>(1/*capacity, does not matter*/);
								if (local_offset + records_per_bundle >= t->buffer_size_records) {
									/* wrap around. we will go out of record buffer */
									local_offset = 0;
								}

								I("going to call pseudo_source...");

							auto ret = pseudo_source(&bundle->seqbuf.pbuf->xaddr,
														t->record_buffers_sec[nodeid].pbuf->xaddr,
														local_offset, records_per_bundle,
														BaseT::current_ts, delta / records_per_bundle,
														tspos<Record<T>>(), reclen<Record<T>>());
							xzl_bug_on(ret < 0);
							xzl_bug_on(!bundle->seqbuf.pbuf->xaddr);

							I("pseudo_source got new sbatch xaddr %08lx", bundle->seqbuf.pbuf->xaddr);
							local_offset += records_per_bundle;
#else
							shared_ptr<BundleT<T>> bundle = make_shared<BundleT<T>>(records_per_bundle  /* capacity */);

							xzl_assert(bundle);

							/* limitation: XXX
							 * 1. we may miss the last @record_size length in the bundle range
							 * 2. we don't split records at the word boundary.
							 * 3. we don't force each string_range's later char to be \0
							 */

							VV("pack records in bundle ts %s:",
								 to_simple_string(current_ts + delta * i).c_str());

							for (unsigned int j = 0; j < records_per_bundle; j++, local_offset++) {
								if (local_offset == t->buffer_size_records)
									local_offset = 0; /* wrap around */
								/* rewrite the record ts */
								t->record_buffers[nodeid][local_offset].ts
										= BaseT::current_ts + delta * i;
								/* same ts for all records in the bundle */
								bundle->add_record(t->record_buffers[nodeid][local_offset]);
							}
#endif
								out[oid]->consumer->sourceDepositOneBundle(bundle, nodeid);
								c->SpawnConsumer();
							} // done sending @bundle_count bundles.
						} // done # output
				};

				/* exec the N-1 task inline */
				if (task_id == total_tasks - 1) {
					source_task_lambda(0);
					continue;
				}

				futures.push_back( // store a future
						/* submit to task queue */
						c->executor_.push(TASK_QUEUE_SOURCE, source_task_lambda)
				);
			}  // end of tasks

			for (auto && f : futures) {
				f.get();
			}
			futures.clear();

			/* advance all record offsets for all outputs */
			for (auto &o : offset) {
				o += records_per_bundle * config.bundles_per_epoch;
				o %= t->buffer_size_records;
			}

#if 0 /* single thread */
			for (unsigned int i = 0; i < bundle_per_interval; i++) {
					 /* construct the bundles by reading NUMA buffers round-robin */
					 int nodeid = (i % num_nodes);
//          	 auto & offset = offsets[nodeid];

					 /* assemble a bundle by drawing records from the corresponding
						* NUMA record buffer. */

					 shared_ptr<BundleT<T>>
						 bundle(make_shared<BundleT<T>>(
								 records_per_bundle,  /* reserved capacity */
								 nodeid));

					 xzl_assert(bundle);

					 /* limitation: XXX
						* 1. we may miss the last @record_size length in the bundle range
						* 2. we don't split records at the word boundary.
						* 3. we don't force each string_range's later char to be \0
						*/

					 VV("pack records in bundle ts %s:",
							 to_simple_string(current_ts + delta * i).c_str());

					 for (unsigned int j = 0; j < records_per_bundle; j++, offset++) {
						 if (offset == t->buffer_size_records)
							 offset = 0; /* wrap around */
							 /* rewrite the record ts */
							 t->record_buffers[nodeid][offset].ts
														 = BaseT::current_ts + delta * i;
							 /* same ts for all records in the bundle */
							 bundle->add_record(t->record_buffers[nodeid][offset]);
					 }

					 out->consumer->sourceDepositOneBundle(bundle, nodeid);
					 c->SpawnConsumer();
				} // done sending @bundle_count bundles.
#endif


			t->byte_counter_.fetch_add(t->records_per_interval * t->record_len * t->num_outputs,
																 std::memory_order_relaxed);
			t->record_counter_.fetch_add(t->records_per_interval * t->num_outputs,
																	 std::memory_order_relaxed);

			boost::posix_time::ptime end_tick = \
							boost::posix_time::microsec_clock::local_time();
			auto elapsed_us = (end_tick - start_tick).total_microseconds();
			xzl_assert(elapsed_us > 0);

			EE("%.2f M records, %lu bundles, %ld ms (budget %ld ms), %d src tasks",
				 t->records_per_interval / 1000.0 / 1000,
				 config.bundles_per_epoch,
				 elapsed_us / 1000, exec_us_per_epoch / 1000, total_tasks);
			if ((unsigned long) elapsed_us > exec_us_per_epoch)
				EE("warning: source runs at full speed.");
			else {
				EE("source to pause for %lu us", exec_us_per_epoch - elapsed_us);
				usleep(exec_us_per_epoch - elapsed_us);
			}

			BaseT::current_ts += punc_interval;
			BaseT::current_ts += 1000 * (t->session_gap_ms);

			/* Make sure all data have left the source before
			advancing the watermark. otherwise, downstream transforms may see
			watermark advance before they see the (old) records. */

			/*
			 * update source watermark immediately, but propagate the watermark
			 * to downstream asynchronously.
			 */
			W("source: to update watermark " ETIME_FMT, BaseT::current_ts);
			c->UpdateSourceWatermark(BaseT::current_ts);

			/* Useful before the sink sees the 1st watermark */
			if (c->GetTargetWm() == max_date_time) { /* unassigned */
				c->SetTargetWm(BaseT::current_ts);
			}

			/* where we process wm.
			 * we spread wm among numa nodes
			 * NB this->_node may be -1 */
			static int wm_node = 0;
			for (auto oid = 0u; oid < t->num_outputs; oid++) {
				out[oid]->consumer->sourceDepositOnePunc(make_shared<Punc>(BaseT::current_ts, wm_node),
																		wm_node);
				c->SpawnConsumer();
				if (++wm_node == numa_max_node())
					wm_node = 0;
			}
		}   // while
	}  // end of eval()

};

#endif //CREEK_UNBOUNDEDSEQEVAL_H
