//
// Created by xzl on 10/21/17.
//
// refactor from Transforms-multi.h

#include "Transforms.h"

//type4 trans opens a new container in the unordered_containers of type 5
//here:  downt is type 5 trans
// upcon is 4's left_unordered_containers, downt is type 5 trans
int PTransform::openDownstreamContainer_unordered_4_left(bundle_container *const upcon,
																						 PTransform *downt) {
	// TODO
	// be care for about assigning side_info to new containers
	// inherent side_info from upcon: should be 1 or 2

	xzl_assert(downt && upcon);
	//std::cout << __FILE__ << ": " <<  __LINE__ << std::endl;
	//std::cout << "this trans is: " << this->getName() << std::endl;
	//std::cout << "downt is: " << downt->getName() << std::endl;
	//  	/* fast path w/o locking */
	if (upcon->downstream)
		return 0;

	/* slow path
	 *
	 * in *this* trans, walk from the oldest container until @upcon,
	 * open a downstream container if there isn't one.
	 *
	 * (X = no downstream container;  V = has downstream container)
	 * possible:
	 *
	 * X X V V (oldest)
	 *
	 * impossible:
	 * X V X V (oldest)
	 *
	 */
	//HYM	unique_lock<mutex> conlock(left_mtx_unordered_containers);

	//here this is SimpleMapper
	// downt is Join
	//unique_lock<mutex> down_conlock(downt->mtx_container);
	//HYM	unique_lock<mutex> down_conlock(downt->mtx_unordered_containers);

	/*     these locks have been hold in deposit, should not get these locks again
		unique_lock<mutex> conlock(mtx_container);
			unique_lock<mutex> down_conlock(downt->mtx_container);
	*/

	/* reader lock for *this* trans */
	boost::shared_lock<boost::shared_mutex> conlock(mtx_container);
	/* writer lock for downstream trans */
	boost::unique_lock<boost::shared_mutex> down_conlock(downt->mtx_container);

	if (upcon->downstream) /* check again */
		return 0;
	//std::cout << __FILE__ << ": " <<  __LINE__ << std::endl;
	int cnt = 0;
	bool miss = false;
	(void) sizeof(miss);
	// miss = miss;
	//std::cout << __FILE__ << ": " <<  __LINE__ << " this trans is: " << this->getName() << " downt is " << downt->getName() <<std::endl;
	for (auto it = this->left_unordered_containers.rbegin();
			 it != this->left_unordered_containers.rend(); it++) {

		//std::cout << __FILE__ << ": " <<  __LINE__ << std::endl;
		/* also lock all downstream containers. good idea? */
		//  		unique_lock<mutex> down_conlock(downt->mtx_container);

		if (!it->downstream) {
			miss = true;
			//std::cout << __FILE__ << ": " <<  __LINE__ << std::endl;
			/* alloc & link downstream container. NB: it->downstream is an atomic
			 * pointer, so it implies a fence here. */
			downt->unordered_containers.emplace_front();
			it->downstream = &(downt->unordered_containers.front());

			//hym: XXX
			//assigne the side_info to this new container
			//it->downstream->side_info = this->get_side_info(); //0->containrs_, 1->left_containers_in, 2->right_containers_in
			//it->downstream.load()->set_side_info(this->get_side_info());
			it->downstream.load()->set_side_info(it->get_side_info());
			xzl_assert(it->downstream.load()->get_side_info() ==
								 1);  //left to unordere_containers

			cnt++;
		} else { /* downstream link exists */
			assert (!miss && "bug? an older container misses downstream link.");
#ifdef DEBUG
			bool found = false;
			for (auto &con : downt->unordered_containers) {
				if (&con == it->downstream) {
					found = true;
					break;
				}
			}
			/* NB: downstream container, once received & processed the punc
			 * from the upstream, may be cleaned up prior to the upstream
			 * container. In this case, a dangling downstream pointer is possible.
			 */
			if (!found && it->punc_refcnt != PUNCREF_CONSUMED) {
				/*
			E("%s: downstream (%s) container does not exist",
						this->name.c_str(), downt->name.c_str());
				cout << " -------------------------------\n";
				dump_containers("bug");
				downt->dump_containers("bug");
				cout << " -------------------------------\n\n";
				abort();
			*/
				xzl_assert(false && "downstream container does not exist");
			}
#endif
		}
	}

#if 0 /* debug */
	cout << " -------------------------------\n";
					dump_containers("link containers");
					downt->dump_containers("link containers");
					cout << " -------------------------------\n\n";
#endif

	return cnt;
}


//type4 trans opens a new container in the unordered_containers of type 5
//here:  downt is type 5 trans
// upcon is 4's right_unordered_containers, downt is type 5 trans
int PTransform::openDownstreamContainer_unordered_4_right(bundle_container *const upcon,
																							PTransform *downt) {
	// TODO
	// be care for about assigning side_info to new containers
	// inherent side_info from upcon: should be 1 or 2

	xzl_assert(downt && upcon);
	//std::cout << __FILE__ << ": " <<  __LINE__ << std::endl;
	//std::cout << "this trans is: " << this->getName() << std::endl;
	//std::cout << "downt is: " << downt->getName() << std::endl;
	//  	/* fast path w/o locking */
	if (upcon->downstream)
		return 0;

	/* slow path
	 *
	 * in *this* trans, walk from the oldest container until @upcon,
	 * open a downstream container if there isn't one.
	 *
	 * (X = no downstream container;  V = has downstream container)
	 * possible:
	 *
	 * X X V V (oldest)
	 *
	 * impossible:
	 * X V X V (oldest)
	 *
	 */
	//HYM  	unique_lock<mutex> conlock(right_mtx_unordered_containers);

	//here this is SimpleMapper
	// downt is Join
	//unique_lock<mutex> down_conlock(downt->mtx_container);
	//HYM	unique_lock<mutex> down_conlock(downt->mtx_unordered_containers);

	/*	these locks have been hodl in deposit, should not get them again
		unique_lock<mutex> conlock(mtx_container);
		unique_lock<mutex> down_conlock(downt->mtx_container);
	*/
	/* reader lock for *this* trans */
	boost::shared_lock<boost::shared_mutex> conlock(mtx_container);
	/* writer lock for downstream trans */
	boost::unique_lock<boost::shared_mutex> down_conlock(downt->mtx_container);

	if (upcon->downstream) /* check again */
		return 0;
	//std::cout << __FILE__ << ": " <<  __LINE__ << std::endl;
	int cnt = 0;
	bool miss = false;
	(void) sizeof(miss);
	// miss = miss;
	//std::cout << __FILE__ << ": " <<  __LINE__ << " this trans is: " << this->getName() << " downt is " << downt->getName() <<std::endl;
	for (auto it = this->right_unordered_containers.rbegin();
			 it != this->right_unordered_containers.rend(); it++) {

		//std::cout << __FILE__ << ": " <<  __LINE__ << std::endl;
		/* also lock all downstream containers. good idea? */
		//  		unique_lock<mutex> down_conlock(downt->mtx_container);

		if (!it->downstream) {
			miss = true;
			//std::cout << __FILE__ << ": " <<  __LINE__ << std::endl;
			/* alloc & link downstream container. NB: it->downstream is an atomic
			 * pointer, so it implies a fence here. */
			downt->unordered_containers.emplace_front();
			it->downstream = &(downt->unordered_containers.front());

			//hym: XXX
			//assigne the side_info to this new container
			//it->downstream->side_info = this->get_side_info(); //0->containrs_, 1->left_containers_in, 2->right_containers_in
			//it->downstream.load()->set_side_info(this->get_side_info());
			it->downstream.load()->set_side_info(it->get_side_info());
			xzl_assert(it->downstream.load()->get_side_info() ==
								 2);  //right to unordere_containers

			cnt++;
		} else { /* downstream link exists */
			assert (!miss && "bug? an older container misses downstream link.");
#ifdef DEBUG
			bool found = false;
			for (auto &con : downt->unordered_containers) {
				if (&con == it->downstream) {
					found = true;
					break;
				}
			}
			/* NB: downstream container, once received & processed the punc
			 * from the upstream, may be cleaned up prior to the upstream
			 * container. In this case, a dangling downstream pointer is possible.
			 */
			if (!found && it->punc_refcnt != PUNCREF_CONSUMED) {
				/*E("%s: downstream (%s) container does not exist",
						this->name.c_str(), downt->name.c_str());
				cout << " -------------------------------\n";
				dump_containers("bug");
				downt->dump_containers("bug");
				cout << " -------------------------------\n\n";
				abort();
			*/
				xzl_assert(false && "downstream container does not exist");
			}
#endif
		}
	}

#if 0 /* debug */
	cout << " -------------------------------\n";
					dump_containers("link containers");
					downt->dump_containers("link containers");
					cout << " -------------------------------\n\n";
#endif

	return cnt;
}


//type4 trans opens a new container in the ordered_containers of type 5
//here:  downt is type 5 trans
int PTransform::openDownstreamContainer_ordered_4(bundle_container *const upcon,
																			PTransform *downt) {
	//TODO
	// be care for about assigning side_info to new containers
	// set side_info to 0, all containers in oerdered_containers should have side_info 0

	xzl_assert(downt && upcon);
	//std::cout << __FILE__ << ": " <<  __LINE__ << std::endl;
	//std::cout << "this trans is: " << this->getName() << std::endl;
	//std::cout << "downt is: " << downt->getName() << std::endl;
	//  	/* fast path w/o locking */
	//  	if (upcon->downstream)
	//  		return 0;

	/* slow path
	 *
	 * in *this* trans, walk from the oldest container until @upcon,
	 * open a downstream container if there isn't one.
	 *
	 * (X = no downstream container;  V = has downstream container)
	 * possible:
	 *
	 * X X V V (oldest)
	 *
	 * impossible:
	 * X V X V (oldest)
	 *
	 */
	//HYM  	unique_lock<mutex> conlock(mtx_ordered_containers);

	//unique_lock<mutex> down_conlock(downt->mtx_container);
	//HYM 	unique_lock<mutex> down_conlock(downt->mtx_ordered_containers);

	/*	these locks have been hold in deposit, should not get them again
		unique_lock<mutex> conlock(mtx_container);
		 unique_lock<mutex> down_conlock(downt->mtx_container);
	*/
	/* reader lock for *this* trans */
	boost::shared_lock<boost::shared_mutex> conlock(mtx_container);
	/* writer lock for downstream trans */
	boost::unique_lock<boost::shared_mutex> down_conlock(downt->mtx_container);

	if (upcon->downstream) /* check again */
		return 0;
	//std::cout << __FILE__ << ": " <<  __LINE__ << std::endl;
	int cnt = 0;
	bool miss = false;
	(void) sizeof(miss);
	// miss = miss;
	//std::cout << __FILE__ << ": " <<  __LINE__ << " this trans is: " << this->getName() << " downt is " << downt->getName() <<std::endl;
	for (auto it = this->ordered_containers.rbegin();
			 it != this->ordered_containers.rend(); it++) {

		//std::cout << __FILE__ << ": " <<  __LINE__ << std::endl;
		/* also lock all downstream containers. good idea? */
		//  		unique_lock<mutex> down_conlock(downt->mtx_container);

		if (!it->downstream) {
			miss = true;
			std::cout << __FILE__ << ": " << __LINE__ << std::endl;
			/* alloc & link downstream container. NB: it->downstream is an atomic
			 * pointer, so it implies a fence here. */
			downt->ordered_containers.emplace_front();
			it->downstream = &(downt->ordered_containers.front());

			//hym: XXX
			//assigne the side_info to this new container
			//it->downstream->side_info = this->get_side_info(); //0->containrs_, 1->left_containers_in, 2->right_containers_in
			//it->downstream.load()->set_side_info(this->get_side_info());
			it->downstream.load()->set_side_info(it->get_side_info());
			xzl_assert(it->downstream.load()->get_side_info() ==
								 0);  //XXX containers in ordered_containers should have side_info 0

			cnt++;
		} else { /* downstream link exists */
			//XXX comment this??XXX
			//			assert (!miss && "bug? an older container misses downstream link.");
#ifdef DEBUG
			bool found = false;
			for (auto &con : downt->ordered_containers) {
				if (&con == it->downstream) {
					found = true;
					break;
				}
			}
			/* NB: downstream container, once received & processed the punc
			 * from the upstream, may be cleaned up prior to the upstream
			 * container. In this case, a dangling downstream pointer is possible.
			 */
			if (!found && it->punc_refcnt != PUNCREF_CONSUMED) {
				/*E("%s: downstream (%s) container does not exist",
						this->name.c_str(), downt->name.c_str());
				cout << " -------------------------------\n";
				dump_containers("bug");
				downt->dump_containers("bug");
				cout << " -------------------------------\n\n";
				abort();
			*/

				xzl_assert(false && "downstream container does not exist");
			}
#endif
		}
	}

#if 0 /* debug */
	cout << " -------------------------------\n";
					dump_containers("link containers");
					downt->dump_containers("link containers");
					cout << " -------------------------------\n\n";
#endif
	return cnt;
}

//type 4 trans deposit a  bundle to downstream(type 5 trans' unordered_contaienrs or ordered_containers)
void PTransform::depositBundleDownstream_4(PTransform *downt,
															 shared_ptr<BundleBase> input_bundle,
															 vector <shared_ptr<BundleBase>> const &output_bundles,
															 int node)
{
	//TODO

	xzl_assert(downt->get_side_info() == 5);

	bool try_open = false; /* have we tried open downstream containers? */
	xzl_assert(downt);
	bundle_container *upcon = localBundleToContainer(input_bundle);
	xzl_assert(upcon); /* enclosing container */

	/*
		 unique_lock<mutex> conlock(mtx_container);
		unique_lock<mutex> down_conlock(downt->mtx_container);
	*/
#if 0
	/* reader lock for *this* trans */
				boost::shared_lock<boost::shared_mutex> conlock(mtx_container);
				/* writer lock for downstream trans */
				boost::unique_lock<boost::shared_mutex> down_conlock(downt->mtx_container);
#endif

	__attribute__((__unused__)) int flag = 100;
	deposit:
	{
		// 	unique_lock<mutex> conlock(mtx_container);
		//	unique_lock<mutex> down_conlock(downt->mtx_container);
		if (upcon->downstream) {
			/* fast path. NB @downstream is atomic w/ seq consistency so it's
			 * safe. */
			downt->depositBundlesToContainer(upcon->downstream, output_bundles, node);
			downt->IncBundleCounter(output_bundles.size());
			return;
		}

		xzl_assert(!try_open && "bug: already tried to open");

		/* it's possible somebody slipped in & opened the downstream container
		 * already. So ret == 0 then.
		 */
		/*
		//  	int ret =
						openDownstreamContainers(upcon, downt);
		//  	xzl_assert(ret && "no downstream container opened?");
		*/

		/*
			if(this->get_side_info() == 0 || this->get_side_info() == 3){ //defaut transforms and join
				openDownstreamContainers(upcon, downt);
			}else if(this->get_side_info() == 1){ //Simplemapper 1, left
				openDownstreamContainers_left(upcon, downt); //downt should be join, open a container in join's left_containers_in
			}else if(this->get_side_info() == 2){ //Simplemapper 2, right
				openDownstreamContainers_right(upcon, downt); //downt should be join, open a container in join's right_containers_in
			}else{
				xzl_assert("Bug: Unknown know side info");
			}
		*/
		//{
		/*
			unique_lock<mutex> conlock1(left_mtx_unordered_containers);
			 unique_lock<mutex> conlock2(right_mtx_unordered_containers);
			 unique_lock<mutex> conlock5(mtx_ordered_containers);
			 unique_lock<mutex> conlock3(downt->mtx_unordered_containers);
			 unique_lock<mutex> conlock4(downt->mtx_ordered_containers);
		*/
		//unique_lock<mutex> conlock(mtx_container);
		//unique_lock<mutex> conlock(downt->mtx_container);

		if (upcon->get_side_info() == 1) {
			openDownstreamContainer_unordered_4_left(upcon, downt);
			flag = 1;
		} else if (upcon->get_side_info() == 2) {
			openDownstreamContainer_unordered_4_right(upcon, downt);
			flag = 2;
		} else if (upcon->get_side_info() == 0) { //from 4's ordered_containers
			openDownstreamContainer_ordered_4(upcon, downt);
			flag = 0;
		} else {
			xzl_assert(false && "Join: wrong side info");
		}
	}//end lock
	/*	if(!upcon->downstream){
	 std::cout << "side info is " << upcon->get_side_info() << " flag is " << flag <<  std::endl;
 }
	*/
	xzl_bug_on(!upcon->downstream);
	try_open = true;
	goto deposit;
}

//type 4 trans deposit a punc to downstream(type 5's trans ordered_containers)
//becase type 4 cannot get punc from left/right_unordered_containers, it can
//only get punc from its ordered_containers, so the punc should be deposited to
// type 5 trans' ordered_containers
void PTransform::depositPuncDownstream_4(PTransform *downt,
														 shared_ptr<Punc> const &input_punc,
														 shared_ptr<Punc> punc, int node) {
	//TODO

	bool try_open = false;

	xzl_assert(downt);
	bundle_container *upcon = localBundleToContainer(input_punc);
	xzl_assert(upcon); /* enclosing container */

	//XXX rw
	/*	unique_lock<mutex> conlock(mtx_container);
		unique_lock<mutex> down_conlock(downt->mtx_container);
	*/
#if 0
	/* reader lock for *this* trans */
				boost::shared_lock<boost::shared_mutex> conlock(mtx_container);
				/* writer lock for downstream trans */
				boost::unique_lock<boost::shared_mutex> down_conlock(downt->mtx_container);
#endif
	deposit:
	{    //{
		//	unique_lock<mutex> conlock(mtx_container);
		//	unique_lock<mutex> down_conlock(downt->mtx_container);
		if (upcon->downstream) { /* fast path. NB @downstream is atomic, implying fence */
			downt->depositOnePuncToContainer(upcon->downstream, punc, node);
			//upcon->has_punc = 1; //the upcon has a container already
			upcon->downstream.load()->has_punc = 1;
			//std::cout << __FILE__ << ": " <<  __LINE__ << " " << this->getName() << " deposit a punc to " << downt->getName() << " to side: " << upcon->get_side_info()  << std::endl;
			return;
		}

		xzl_assert(!try_open && "bug: already tried to open");

		if (upcon->get_side_info() == 1) {
			openDownstreamContainer_unordered_4_left(upcon, downt);
		} else if (upcon->get_side_info() == 2) {
			openDownstreamContainer_unordered_4_right(upcon, downt);
		} else if (upcon->get_side_info() == 0) {
			openDownstreamContainer_ordered_4(upcon, downt);
		} else {
			xzl_assert(false && "Join: wrong side info");
		}
	}//end lock
	xzl_assert(upcon->downstream);
	try_open = true;
	goto deposit;

}

/***************************************************************
						hym: end of type 4 trans
****************************************************************/
