//
// Created by xzl on 7/11/17.
//
// implement the basic WHERE operator
// filter records based on thersholds (low, high)
//
// Does not contain full def of ExecEvaluator() to avoid circular dep

#ifndef CREEK_WINDOWEDNUMMAPPER_H
#define CREEK_WINDOWEDNUMMAPPER_H

// cf: SimpleMapper.h
// stateless

#include "Mapper/Mapper.h"
#include "Values.h"
#include "SeqBundle.h"

using namespace std;
using namespace creek;

template <typename InputT, typename OutputT, int keypos, int vpos>
class WhereSeqMapperEval;

template <typename InputT, typename OutputT, int keypos, int vpos>
class WhereSeqMapper : public Mapper<InputT> {
	using InputBundleT = RecordSeqBundle<OutputT>;
	using OutputBundleT = RecordSeqBundle<OutputT>;

private:
	unsigned long low_, high_;

public:
	WhereSeqMapper(unsigned long low, unsigned long high, int sideinfo,
		string sname = "num_mapper")
			: Mapper<InputT>(sname, sideinfo), low_(low), high_(high) {
		xzl_assert(keypos < reclen<Record<InputT>>());
		xzl_assert(vpos < reclen<Record<InputT>>());
	}

	#if 0
	/* return: # of records emitted */
	uint64_t do_map(Window const & win, Record<InputT> const & in,
									shared_ptr<OutputBundleT> output_bundle) {

		uint64_t count = 0;

		if (in.data < low_ || in.data > high_) {
			output_bundle->add_record(win, in);
			count ++;
		}

		return count;
	}
	#endif

	/* return: # of records emitted */
	uint64_t do_map(shared_ptr<InputBundleT> input_bundle,
									shared_ptr<OutputBundleT> output_bundle) {

		/* thresholds, as expected by SeqBuf. only v matters */
		InputT l, h; /* tuples */
		std::get<vpos>(l)  = low_;
		std::get<vpos>(h)  = high_;
//		Record<InputT> l(make_tuple(0, low_, 0));
//		Record<InputT> h(make_tuple(0, high_, 0));

		xzl_bug_on_msg(output_bundle->seqbuf.Size(), "bug: buffer already allocated?");
		output_bundle->seqbuf = input_bundle->seqbuf. template FilterBand<vpos>(l, h);

		VV("output %lu items. xaddr %lx", output_bundle->seqbuf.Size(), output_bundle->seqbuf.pbuf->xaddr);
//		output_bundle->seqbuf.Dump();

		return output_bundle->seqbuf.Size();
	}

	void ExecEvaluator(int nodeid, EvaluationBundleContext *c,
										 shared_ptr<BundleBase> bundle = nullptr) override;
		/* why this does not work here? */
//		{
//
//			/* instantiate an evaluator */
//			WhereSeqMapperEval<InputT, OutputT, keypos, vpos> eval(nodeid);
//			eval.evaluate(this, c, bundle);
//		}

};


#endif //CREEK_WINDOWEDNUMMAPPER_H
