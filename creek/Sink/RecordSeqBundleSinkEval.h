//
// Created by xzl on 9/1/17.
//
// cf: Sink/WinKeySeqBundleSinkEval.h

#ifndef CREEK_SEQBUNDLESINKEVAL_H
#define CREEK_SEQBUNDLESINKEVAL_H

#include <creek-types.h>
#include "core/SingleInputTransformEvaluator.h"
#include "Sink/RecordSeqBundleSink.h"

/* T: element type, e.g. kvpair */
template<typename T>
class RecordSeqBundleSinkEval
		: public SingleInputTransformEvaluator<RecordSeqBundleSink<T>,
				RecordSeqBundle<T>, RecordSeqBundle<T>>
{
	using TransformT = RecordSeqBundleSink<T>;
	using InputBundleT = RecordSeqBundle<T>;
	using OutputBundleT = RecordSeqBundle<T>;

public:
	RecordSeqBundleSinkEval(int node)
			: SingleInputTransformEvaluator<TransformT, InputBundleT, OutputBundleT>(node) { }


	bool evaluateSingleInput (TransformT* trans,
														shared_ptr<InputBundleT> input_bundle,
														shared_ptr<OutputBundleT> output_bundle) override {
		TransformT::printBundle(*input_bundle);
		return false; /* no output bundle */
	}

};

// TBD - for J->JD->JDD, if we dont define WORKAROUND_JOIN_JDD
// class RecordSeqBundleSinkEvalJD

#endif //CREEK_SEQBUNDLESINKEVAL_H
