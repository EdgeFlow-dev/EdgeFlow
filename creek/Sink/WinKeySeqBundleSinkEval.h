//
// Created by xzl on 8/25/17.
//

#ifndef CREEK_WINKEYSEQBUNDLESINKEVAL_H
#define CREEK_WINKEYSEQBUNDLESINKEVAL_H

#include "core/SingleInputTransformEvaluator.h"
#include <creek-types.h>
#include "Sink/WinKeySeqBundleSink.h"

/* @T: element. e.g. kvpair or k2v */
template <typename T>
class WinKeySeqBundleSinkEval
		: public SingleInputTransformEvaluator<WinKeySeqBundleSink<T>,
				WinKeySeqBundle<T>, WinKeySeqBundle<T>>
{
	using TransformT = WinKeySeqBundleSink<T>;
	using InputBundleT = WinKeySeqBundle<T>;
	using OutputBundleT = WinKeySeqBundle<T>;

public:
	WinKeySeqBundleSinkEval(int node)
			: SingleInputTransformEvaluator<TransformT, InputBundleT, OutputBundleT>(node) { }


	bool evaluateSingleInput (TransformT* trans,
														shared_ptr<InputBundleT> input_bundle,
														shared_ptr<OutputBundleT> output_bundle) override {
		TransformT::printBundle(*input_bundle);
		return false; /* no output bundle */
	}

};

// TBD - for J->JD->JDD, if we dont define WORKAROUND_JOIN_JDD
template <typename T>
class WinKeySeqBundleSinkEvalJD
		: public SingleInputTransformEvaluator<WinKeySeqBundleSink<T>,
				WinKeySeqBundle<T>, WinKeySeqBundle<T>> {
	using TransformT = WinKeySeqBundleSink<T>;
	using InputBundleT = WinKeySeqBundle<T>;
	using OutputBundleT = WinKeySeqBundle<T>;

public:
	WinKeySeqBundleSinkEvalJD(int node)
			: SingleInputTransformEvaluator<TransformT, InputBundleT, OutputBundleT>(node) {
//		xzl_bug("XXX");
	}

	bool evaluateSingleInput (TransformT* trans,
														shared_ptr<InputBundleT> input_bundle,
														shared_ptr<OutputBundleT> output_bundle) override {
		TransformT::printBundle(*input_bundle);
//		xzl_bug("XXX");
		return false; /* no output bundle */
	}
};

#endif //CREEK_WINKEYSEQBUNDLESINKEVAL_H
