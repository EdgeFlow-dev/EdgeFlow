//
// Created by xzl on 8/30/17.
//

#ifndef CREEK_UNBOUNDEDSEQEVAL_H
#define CREEK_UNBOUNDEDSEQEVAL_H

#include "UnboundedSeq.h"
#include "UnboundedInMemEvaluator.h"

/* xzl: emit record Seq bundle. specialize to kvpair.
 * cf: the class above */
template<>
class UnboundedInMemEvaluator<creek::kvpair, RecordSeqBundle>
		: public UnboundedInMemEvaluatorBase<creek::kvpair, RecordSeqBundle> {
	using T = creek::kvpair;
//	using BundleT = RecordSeqBundle<creek::kvpair>;
	template<class X> using BundleT = RecordSeqBundle<X>; // compatible with UnboundedInMemEvaluator_2out
	using TransformT = typename UnboundedInMemEvaluatorBase<T, RecordSeqBundle>::TransformT;
	using BaseT = UnboundedInMemEvaluatorBase<T, RecordSeqBundle>;

	using TicketT = std::future<void>;

public:

	UnboundedInMemEvaluator(int node) { }

private:
	/* moved from WindowKeyedReducerEval (XXX merge later)
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

	//     emit 2 same output streams. cf: "UnboundedInMemEvaluator_2out.h"
	void evaluate_2outputs(TransformT* t, EvaluationBundleContext *c,
												 shared_ptr<BundleBase> bundle_ptr = nullptr) //override
	{

		PValue* out[] = { t->getFirstOutput(), t->getSecondOutput() };
		assert(out[0]);

		int num_outputs = 1;
		if (out[1]) {
			num_outputs = 2;
		}

		//# of bundles between two puncs
		const uint64_t bundle_per_interval = 1 * numa_num_configured_cpus(); /*# cores*/
		etime punc_interval =
				1000 * (t->punc_interval_ms);

		etime delta =
				1000 * (t->punc_interval_ms) / bundle_per_interval;

		const uint64_t records_per_bundle =
				t->records_per_interval / bundle_per_interval;

		EE(" ---- punc internal is %d sec (ev time) --- ",
			 t->punc_interval_ms / 1000);

		/* an infi loop that emit bundles to all NUMA nodes periodically.
		 * spawn downstream eval to consume the bundles.
		 * NB: we can only sleep in the main thread (which is not
		 * managed by the NUMA thread pool).
		 */

		const int num_nodes = numa_max_node() + 1;

		uint64_t us_per_iteration =
				1e6 * t->records_per_interval * 2 / t->target_tput; /* the target us */

//version 1
//hym: create bundle pointer for 2 streams
//     two streams will not share the bundle pointer
		uint64_t offset[2] = {0};//support 10 input streams now
		while(true){
			boost::posix_time::ptime start_tick =
					boost::posix_time::microsec_clock::local_time();

			for(unsigned int i = 0; i < bundle_per_interval; i++){
				/* construct the bundles by reading NUMA buffers round-robin */
				int nodeid = i % num_nodes;

				for(int oid = 0; oid < num_outputs; oid++){
					/* Assumble a bundle by drawing records from the corresponding
					* NUMA buffer. */
					shared_ptr<BundleT<T>>
							bundle(make_shared<BundleT<T>>(
							records_per_bundle /* reserved capacity */)
					);
					xzl_assert(bundle);
					VV("pack records in bundle ts %s:",
						 to_simple_string(current_ts + delta * i).c_str());

					for(unsigned int j = 0; j < records_per_bundle; j++, offset[oid]++){

						if(offset[oid] == t->buffer_size_records){
							offset[oid] = 0; //wrap around
						}
						t->record_buffers[nodeid][offset[oid]].ts = BaseT::current_ts + delta * i;
						bundle->add_record(t->record_buffers[nodeid][offset[oid]]);
					}//end for: records_per_bundle
					out[oid]->consumer->depositOneBundle(bundle, nodeid);
					c->SpawnConsumer();
				} //end for: num_outputs
			} //end for: bundle_per_interval

			t->byte_counter_.fetch_add(t->records_per_interval * t->record_len * 2,
																 std::memory_order_relaxed);

			t->record_counter_.fetch_add(t->records_per_interval * 2,
																	 std::memory_order_relaxed);

			boost::posix_time::ptime end_tick =
					boost::posix_time::microsec_clock::local_time();
			auto elapsed_us = (end_tick - start_tick).total_microseconds();
			assert(elapsed_us > 0);

			if ((unsigned long)elapsed_us > us_per_iteration)
				EE("warning: source runs at full speed.");
			else {
				usleep(us_per_iteration - elapsed_us);
				I("source pauses for %lu us", us_per_iteration - elapsed_us);
			}

			BaseT::current_ts += punc_interval;
			BaseT::current_ts += 1000 * (t->session_gap_ms);

			/* Make sure all data have left the source before
			 * advancing the watermark. otherwise, downstream transforms may see
			 * watermark advance before they see the (old) records.
			 */

			/*
			 * update source watermark immediately, but propagate the watermark
			 * to downstream asynchronously.
			 */
			c->UpdateSourceWatermark(BaseT::current_ts);

			/* Useful before the sink sees the 1st watermark */
			if (c->GetTargetWm() == max_date_time) { /* unassigned */
				c->SetTargetWm(BaseT::current_ts);
			}

			/* where we process wm.
			 * we spread wm among numa nodes
			 * NB this->_node may be -1
			 */
			static int wm_node = 0;
			for(int oid = 0; oid < num_outputs; oid++){
				out[oid]->consumer->depositOnePunc(
						make_shared<Punc>(BaseT::current_ts, wm_node), wm_node);
				//std::cout << "deposit one punc --------" << std::endl;
				c->SpawnConsumer();
				if (++wm_node == numa_max_node()){
					wm_node = 0;
				}
			}
		}//end while
	}//end evaluate


	void evaluate(TransformT* t, EvaluationBundleContext *c,
								shared_ptr<BundleBase> bundle_ptr = nullptr) override
	{
		if (t->num_outputs == 2) {
			evaluate_2outputs(t, c, bundle_ptr);
			return;
		}

		auto out = t->getFirstOutput();
		assert(out);

		/* # of bundles between two puncs */
		const uint64_t bundle_per_interval
				//= 1 * numa_num_configured_cpus(); /* # cores */
				= 1 * c->num_workers; /* # cores */

		etime punc_interval =
				1000 * (t->punc_interval_ms);

		etime delta =
				1000 * (t->punc_interval_ms) / bundle_per_interval;

		//    const int bundle_count = 2;	// debugging
		const uint64_t records_per_bundle
				= t->records_per_interval / bundle_per_interval;

		EE(" ---- punc internal is %d sec (ev time) --- ",
			 t->punc_interval_ms / 1000);

		/* an infi loop that emit bundles to all NUMA nodes periodically.
		 * spawn downstream eval to consume the bundles.
		 * NB: we can only sleep in the main thread (which is not
		 * managed by the NUMA thread pool).
		 */

		const int num_nodes = numa_max_node() + 1;

		/* the global offset into each NUMA buffer to avoid repeated content.
		 * (since each buffer has same content)
		 */
		uint64_t offset = 0;
		uint64_t us_per_iteration =
				1e6 * t->records_per_interval / t->target_tput; /* the target us */

		/* source is also MT. 3 seems good for win-grep */
		const int total_tasks = CONFIG_SOURCE_THREADS;

		vector <std::future<void>> futures;

		while (true) {
			boost::posix_time::ptime start_tick = \
											boost::posix_time::microsec_clock::local_time();

			/*
			 * each bundle is to be consumed by a new task.
			 *
			 * will NOT block
			 */
			for (int task_id = 0; task_id < total_tasks; task_id++) {

				/* each worker works on a range of bundles in the epoch */
				//			auto source_task_lambda = [t, &total_tasks, &bundle_per_internval, task_id](int id)
				auto source_task_lambda = [t, &total_tasks, &bundle_per_interval,
						this, &delta, &out, &c, &records_per_bundle, &num_nodes,
						task_id, offset](int id)
				{
						auto range = get_range(bundle_per_interval, total_tasks, task_id);

						auto local_offset = (offset + records_per_bundle * range.first) % t->buffer_size_records;

						I("source worker %d: bundle range [%d, %d). record offset %lu; total records %lu",
							task_id, range.first, range.first + range.second, local_offset,
							t->buffer_size_records);

						for (int i = range.first; i < range.first + range.second; i++) {
							/* construct the bundles by reading NUMA buffers round-robin */
							int nodeid = (i % num_nodes);

							/* assemble a bundle by drawing records from the corresponding
								* NUMA record buffer. */

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

							out->consumer->depositOneBundle(bundle, nodeid);
							c->SpawnConsumer();
						} // done sending @bundle_count bundles.
				};

				/* exec the N-1 task inline */
				if (task_id == total_tasks - 1) {
					source_task_lambda(0);
					continue;
				}

				futures.push_back( // store a future
						c->executor_.push(source_task_lambda) /* submit to task queue */
				);
			}  // end of tasks

			for (auto && f : futures) {
				f.get();
			}
			futures.clear();

			/* advance the global record offset */
			offset += records_per_bundle * bundle_per_interval;
			offset %= t->buffer_size_records;

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

					 out->consumer->depositOneBundle(bundle, nodeid);
					 c->SpawnConsumer();
				} // done sending @bundle_count bundles.
#endif


			t->byte_counter_.fetch_add(t->records_per_interval * t->record_len,
																 std::memory_order_relaxed);
			t->record_counter_.fetch_add(t->records_per_interval,
																	 std::memory_order_relaxed);

			boost::posix_time::ptime end_tick = \
							boost::posix_time::microsec_clock::local_time();
			auto elapsed_us = (end_tick - start_tick).total_microseconds();
			xzl_assert(elapsed_us > 0);
			if ((unsigned long)elapsed_us > us_per_iteration)
				EE("warning: source runs at full speed.");
			else {
				usleep(us_per_iteration - elapsed_us);
				EE("source pauses for %lu us", us_per_iteration - elapsed_us);
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
			c->UpdateSourceWatermark(BaseT::current_ts);

			/* Useful before the sink sees the 1st watermark */
			if (c->GetTargetWm() == max_date_time) { /* unassigned */
				c->SetTargetWm(BaseT::current_ts);
			}

			/* where we process wm.
			 * we spread wm among numa nodes
			 * NB this->_node may be -1 */
			static int wm_node = 0;
			out->consumer->depositOnePunc(make_shared<Punc>(BaseT::current_ts, wm_node),
																		wm_node);
			c->SpawnConsumer();
			if (++wm_node == numa_max_node())
				wm_node = 0;

		}   // while
	}  // end of eval()

};

#endif //CREEK_UNBOUNDEDSEQEVAL_H
