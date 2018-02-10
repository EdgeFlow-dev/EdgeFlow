//
// Created by xzl on 12/28/17.
//

#ifndef CREEK_SOURCEZMQEVAL_H
#define CREEK_SOURCEZMQEVAL_H


#include "pipeline-config.h"
#include "SourceZmq.h"
#include "UnboundedInMemEvaluatorBase.h"

extern void dump_arr_hex(const char name[], uint64_t* arr);

/* xzl: emit RecordSeqBundle of T
 * cf: the class above
 * T: element name, eg kvpair
 *
 * cf UnboundedInMemEval.h, UnboundedInMemEvaluatorBase.h
 *
 * */
template<typename T>
class SourceZmqEval
		: public UnboundedInMemEvaluatorBase<T, RecordSeqBundle, SourceZmq<T>> {
	template<class X> using BundleT = RecordSeqBundle<X>; // compatible with UnboundedInMemEvaluator_2out
	using TransformT = SourceZmq<T>;
	using BaseT = UnboundedInMemEvaluatorBase<T, RecordSeqBundle, SourceZmq<T>>;

	using TicketT = std::future<void>;

public:

	SourceZmqEval(int node) { }

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

//		etime punc_interval = 1000 * (t->punc_interval_ms);

//		const uint64_t records_per_bundle =
//				t->records_per_epoch / config.bundles_per_epoch;

		/* An infi loop that emit bundles to all NUMA nodes periodically.
		 * spawn downstream eval to consume the bundles.
		 *
		 * NB: we can only sleep in the main thread (which is not
		 * managed by the NUMA thread pool).
		 */

		const int num_nodes = creek::num_numa_nodes();

		/* the global offset into each NUMA buffer to avoid repeated content.
		 * (since each buffer has same content)
		 */
		vector<uint64_t> offset(t->num_outputs);
		for (auto & o : offset)
			o = 0;

		xzl_bug_on(t->num_source_tasks <= 0 || t->num_source_tasks > c->num_workers);
		const int total_tasks = t->num_source_tasks;

		/* create an array of zmq sockets and connect them  */
		vector<zmq::socket_t> zmq_s;
		/* high water mark setting */
		uint64_t hwm = 20;
		for (int i = 0; i < total_tasks; i++) {
			zmq_s.emplace_back(t->zcontext, ZMQ_PULL);
			/* the defualt ZMQ_HWM value of zero means "no limit". */
#if ZMQ_VERSION < ZMQ_MAKE_VERSION(3, 3, 0)
			zmq_s.rbegin()->setsockopt(ZMQ_HWM, &hwm, sizeof(hwm));
#else
			sender.setsockopt(ZMQ_SNDHWM, &hwm, sizeof(hwm));
#endif
			zmq_s.rbegin()->connect(t->input_fname);
		}

		/* peek the ts of each bundle, as an adhoc way to determine ingress wm */
		vector<etime> bundle_ts(total_tasks);
		vector <std::future<void>> futures;

		while (true) {

			auto byte_counter_before = t->byte_counter_.load(std::memory_order_relaxed);
			auto rec_counter_before = t->record_counter_.load(std::memory_order_relaxed);

			boost::posix_time::ptime start_tick = \
                      boost::posix_time::microsec_clock::local_time();

#if defined(USE_TZ) && defined(DEBUG)
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
						this, &out, &c, &bundle_ts, &num_nodes, task_id, &zmq_s](int id) {

						auto range = get_range(config.bundles_per_epoch, total_tasks, task_id);

						for (auto oid = 0u; oid < t->num_outputs; oid++) { /* each task produces to all outputs in round robin */

							I("source worker %d: bundle range [%d, %d).",
								task_id, range.first, range.first + range.second);

							for (int i = range.first; i < range.first + range.second; i++) {

								int nodeid = (i % num_nodes);

								/* grab a bundle ... */
								zmq::message_t msg;
								zmq_s[task_id].recv(&msg);

//								volatile auto k = msg.size();
//								volatile auto kk = reclen<Record<T>>();
//								volatile simd_t *pp = (simd_t * )msg.data();
//								volatile simd_t  ss;

								/* sanity check */
								xzl_bug_on_msg(msg.size() % sizeof(Record<T>) != 0, "misaligned record boundary. check # items per record?");

								size_t n_records = msg.size() / sizeof(Record<T>);
								/* dbg */
//								size_t n_simds = msg.size() / sizeof(simd_t);
//								I("worker %d: Got a bundle (msg) of %zu records, %zu simd_t",
//									task_id, n_records, n_simds);

								VV("dump msg in simd_t %08d %08d %08d %08d\n",
									 ((simd_t*)msg.data())[0], ((simd_t*)msg.data())[1], ((simd_t*)msg.data())[2], ((simd_t*)msg.data())[3]);

								/* copy the msg content over. expensive? */
								shared_ptr<BundleT<T>> bundle = make_shared<BundleT<T>>(
										(Record<T> *)msg.data(), n_records /* will construct seqbuf underneath */
								);
								xzl_bug_on(!bundle);
								xzl_bug_on(!bundle->seqbuf.pbuf->xaddr);
								I("source created new sbatch xaddr %08lx", bundle->seqbuf.pbuf->xaddr);

								/* peek the bundle's timestamp
								 * (for simplicity, we assume
								 * all records in the bundle has same timestamps )
								 */
								Record<T> *records = (Record<T> *)(msg.data());
								bundle_ts[task_id] = records[0].ts; /* save it */
								/* hp:checkout record info */
//								VV("%08d %08d %08d ts %d\n",
//									 std::get<0>(records[0].data), std::get<1>(records[0].data), std::get<2>(records[0].data),
//									 records[0].ts);
								VV("%08d %08d ts %d\n",
									 std::get<0>(records[0].data), std::get<1>(records[0].data),
									 records[0].ts);

								I("worker %d: peek ts " ETIME_FMT, task_id, bundle_ts[task_id]);

								out[oid]->consumer->sourceDepositOneBundle(bundle, nodeid);
								c->SpawnConsumer();


								/* regardless of num_outputs, these stat reflect the actual amount of ingested data,
								 * which == data sent to the pipeline */
								t->byte_counter_.fetch_add(msg.size(), std::memory_order_relaxed);
								t->record_counter_.fetch_add(n_records, std::memory_order_relaxed);

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

			for (auto &&f : futures) {
				f.get();
			}
			futures.clear();


//			t->byte_counter_.fetch_add(t->records_per_epoch * t->record_len * t->num_outputs,
//																 std::memory_order_relaxed);
//			t->record_counter_.fetch_add(t->records_per_epoch * t->num_outputs,
//																	 std::memory_order_relaxed);

			boost::posix_time::ptime end_tick = \
              boost::posix_time::microsec_clock::local_time();
			auto elapsed_us = (end_tick - start_tick).total_microseconds();
			xzl_assert(elapsed_us > 0);

			auto byte_recv = t->byte_counter_.load(std::memory_order_relaxed) -
											 byte_counter_before;
			auto rec_recv = t->record_counter_.load(std::memory_order_relaxed) -
											rec_counter_before;

			/* if we meet the target tput, how much time would we spend */
			uint64_t exec_us_per_epoch = 1e6 * rec_recv / t->target_tput;

			EE("Recv'd %.2f M records (%.2f MB) in %lu bundles, %ld ms (budget %ld ms), %d src tasks",
				 rec_recv / 1000.0 / 1000, byte_recv / 1000.0 / 1000,
				 config.bundles_per_epoch,
				 elapsed_us / 1000, exec_us_per_epoch / 1000, total_tasks);

			if ((unsigned long) elapsed_us > exec_us_per_epoch)
				EE("warning: source can't meet target tput.");
			else {
				EE("source to pause for %lu us", exec_us_per_epoch - elapsed_us);
				if (exec_us_per_epoch - elapsed_us > 1 * 1000 * 1000)
					EE_WARN("That seems a long pause for the source. Consider increase tput (-T)");
				usleep(exec_us_per_epoch - elapsed_us);
			}

//			BaseT::current_ts += punc_interval;
//			BaseT::current_ts += 1000 * (t->session_gap_ms);

			cout << "bundle ts:";
			for (auto & t : bundle_ts)
				cout << t << " ";
			cout << endl;

			BaseT::current_ts = *std::min_element(bundle_ts.begin(), bundle_ts.end());
//			BaseT::current_ts += 1; /* make a small gap */

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
				if (++wm_node == creek::num_numa_nodes() - 1)
					wm_node = 0;
			}

		}   // while
	}  // end of eval()

};


#endif //CREEK_SOURCEZMQEVAL_H
