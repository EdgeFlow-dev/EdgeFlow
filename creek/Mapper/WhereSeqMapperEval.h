//
// Created by xzl on 7/11/17.
//

#ifndef WINNUMMAPPER_EVAL
#define WINNUMMAPPER_EVAL

#include "core/SingleInputTransformEvaluator.h"
#include "Mapper/WhereSeqMapper.h"

template <typename InputT, typename OutputT, int keypos, int vpos>
class WhereSeqMapperEval
		: public SingleInputTransformEvaluator<WhereSeqMapper<InputT,OutputT,keypos,vpos>,
				RecordSeqBundle<InputT>, RecordSeqBundle<OutputT>> {

	using TransformT = WhereSeqMapper<InputT,OutputT,keypos,vpos>;
	using InputBundleT = RecordSeqBundle<InputT>;
	using OutputBundleT = RecordSeqBundle<OutputT>;

public:

	WhereSeqMapperEval(int node) :
			SingleInputTransformEvaluator<TransformT, InputBundleT, OutputBundleT>(node) { }

	bool evaluateSingleInput(TransformT* trans,
													 shared_ptr<InputBundleT> input_bundle,
													 shared_ptr<OutputBundleT> output_bundle) override {

		uint64_t cnt = 0;

		#if 0 /* old impl. per record */
		for (const auto & w : input_bundle->vals) {
			const auto & win = w.first;
			const auto & pfrag = w.second;
			for (const auto & rec : pfrag->vals) {
				cnt += trans->do_map(win, rec, output_bundle);
			}
		}
		#endif

		cnt = trans->do_map(input_bundle, output_bundle);

		if (cnt) {
			return true;
		} else {
			return false;
		}
	}
};


#endif // WINNUMMAPPER_EVAL