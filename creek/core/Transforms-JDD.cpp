//#ifndef TRANSFORMS_TP5_H
//#define TRANSFORMS_TP5_H

#include "Transforms.h"

/* Oct 2017, functions for JDD transforms
 *
 * for xplane submissions
 *
 * hym:
 * this transform: type 5 (JDD)
 * down transform: type 5 (JDD)
 * side_info:
 *	-- unorderd_list: 1 (L) or 2 (R)
 *	-- orderd_list: 0
 **/

/* Functions:
 * A. Deposit a bundle to downstream
 * void depositBundleDownstream_5(PTransform *downt,
 *		shared_ptr<BundleBase> input_bundle,
 *		vector<shared_ptr<BundleBase>> const & output_bundles, int node = -1){
 *
 * B. Deposit a punc to downstream
 * void depositPuncDownstream_5(PTransform* downt,
 *	shared_ptr<Punc> const & input_punc, shared_ptr<Punc> punc, int node = -1){
 *
 **/

/* 1. open container in unordered list*/
int PTransform::openDownstreamContainer_unordered_5(
					bundle_container *const upcon, PTransform *downt)
{
	xzl_assert(downt && upcon);
	if (upcon->downstream)
		return 0;

	/* reader lock for *this* trans */
	boost::shared_lock<boost::shared_mutex> conlock(mtx_container);
	/* writer lock for downstream trans */
	boost::unique_lock<boost::shared_mutex> down_conlock(downt->mtx_container);

	if (upcon->downstream) /* check again */
		return 0;
	int cnt = 0;
	bool miss = false;
	for (auto it = this->unordered_containers.rbegin();
			 it != this->unordered_containers.rend(); it++) {

		if (!it->downstream) {
			miss = true;
			downt->unordered_containers.emplace_front();
			it->downstream = &(downt->unordered_containers.front());

			// hym: assigne the side_info to this new container
			//      unorderd_list's side_info should be 1 or 2
			it->downstream.load()->set_side_info(it->get_side_info());
			xzl_assert(it->downstream.load()->get_side_info() == SIDE_INFO_L ||
								 it->downstream.load()->get_side_info() == SIDE_INFO_R);
			cnt++;
		} else { /* downstream link exists */
			xzl_bug_on_msg(miss, "bug? an older container misses downstream link.");
		}
	}
	return cnt;
}

/* 2. open container in ordered list*/
int PTransform::openDownstreamContainer_ordered_5(bundle_container *const upcon,
	 PTransform *downt) {
	xzl_assert(downt && upcon);
	if (upcon->downstream)
		return 0;

	/* reader lock for *this* trans */
	boost::shared_lock<boost::shared_mutex> conlock(mtx_container);
	/* writer lock for downstream trans */
	boost::unique_lock<boost::shared_mutex> down_conlock(downt->mtx_container);

	if (upcon->downstream) /* check again */
		return 0;
	int cnt = 0;
	bool miss = false;
	for (auto it = this->ordered_containers.rbegin();
			 it != this->ordered_containers.rend(); it++) {

		if (!it->downstream) {
			miss = true;
			downt->ordered_containers.emplace_front();
			it->downstream = &(downt->ordered_containers.front());

			// hym: assigne the side_info to this new container
			//      orderd_list's side_info should be 0
			it->downstream.load()->set_side_info(it->get_side_info());
			xzl_assert(it->downstream.load()->get_side_info() == SIDE_INFO_NONE);
			cnt++;
		} else { /* downstream link exists */
			xzl_bug_on_msg(miss, "bug? an older container misses downstream link.");
		}
	}
	return cnt;
}

/* 3. deposit a bundle to downstream */
void PTransform::depositBundleDownstream_5(PTransform *downt,
				 shared_ptr<BundleBase> input_bundle,
				 vector<shared_ptr<BundleBase>> const &output_bundles, int node) {

	xzl_assert(downt->get_side_info() == SIDE_INFO_JDD);

	bool try_open = false; /* have we tried open downstream containers? */
	xzl_assert(downt);
	bundle_container *upcon = localBundleToContainer(input_bundle);
	xzl_assert(upcon); /* enclosing container */
	deposit:
	{
		if (upcon->downstream) {
			downt->depositBundlesToContainer(upcon->downstream, output_bundles, node);
			downt->IncBundleCounter(output_bundles.size());
			return;
		}
		xzl_assert(!try_open && "bug: already tried to open");

		if (upcon->get_side_info() == SIDE_INFO_L || upcon->get_side_info() == SIDE_INFO_R) { /* from unordered list */
			openDownstreamContainer_unordered_5(upcon, downt);
		} else if (upcon->get_side_info() == SIDE_INFO_NONE) { /* from ordered list */
			openDownstreamContainer_ordered_5(upcon, downt);
		} else {
			xzl_bug(" wrong side info");
		}
	}//end lock

	if (!upcon->downstream)
		EE("side info is %d", upcon->get_side_info());
	xzl_assert(upcon->downstream);
	try_open = true;
	goto deposit;
}


/* 4. deposit a punc to downstream */
void PTransform::depositPuncDownstream_5(PTransform *downt,
				 shared_ptr<Punc> const &input_punc, shared_ptr<Punc> punc, int node) {

	xzl_assert(downt->get_side_info() == SIDE_INFO_JDD);

	bool try_open = false;
	xzl_assert(downt);
	bundle_container *upcon = localBundleToContainer(input_punc);
	xzl_assert(upcon); /* enclosing container */
	deposit:
	{
		if (upcon->downstream) {
			downt->depositOnePuncToContainer(upcon->downstream, punc, node);
			upcon->downstream.load()->has_punc = 1;
			return;
		}
		xzl_assert(!try_open && "bug: already tried to open");

		if (upcon->get_side_info() == SIDE_INFO_L || upcon->get_side_info() == SIDE_INFO_R) {
			openDownstreamContainer_unordered_5(upcon, downt);
		} else if (upcon->get_side_info() == SIDE_INFO_NONE) {
			openDownstreamContainer_ordered_5(upcon, downt);
		} else {
			xzl_assert(false && "wrong side info");
		}
	}//end lock
	xzl_assert(upcon->downstream);
	try_open = true;
	goto deposit;
}

//#endif /* TRANSFORMS_TP5_H */
