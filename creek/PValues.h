//
// Created by xzl on 1/11/18.
//
// From Values.h
// Now this only contains PValues, i.e. pipeline's "value"


#ifndef CREEK_PVALUES_H
#define CREEK_PVALUES_H

#include <deque>
#include <list>
#include <memory>

#include "Bundle.h"

using namespace std;


/**
 * The interface for values that can be input to and output from {@link PTransform PTransforms}.
 */
class PValue {

	using bundle_queue_t = deque<shared_ptr<BundleBase>>;

public:
	virtual Pipeline* getPipeline() = 0;
	virtual basic_string<char> getName() = 0;
	virtual ~PValue() { };

	/**
	* Expands this {@link POutput} into a list of its component output
	* {@link PValue PValues}.
	*
	* <ul>
	*   <li>A {@link PValue} expands to itself.</li>
	*   <li>A tuple or list of {@link PValue PValues} (such as
	*     {@link PCollectionTuple} or {@link PCollectionList})
	*     expands to its component {@code PValue PValues}.</li>
	* </ul>
	*
	* <p>Not intended to be invoked directly by user code.
	*/
	virtual list<PValue *>* expand() = 0;

	// input only
	virtual void finishSpecifying() = 0;

	// output only. nop for input.
	virtual void recordAsOutput(AppliedPTransform* transform) { }
	virtual void finishSpecifyingOutput() { }
	virtual AppliedPTransform* getProducingTransformInternal() { return NULL; }

  PTransform *producer = nullptr;
  PTransform *consumer = nullptr;

  /* @node:
   * >= 0 specific numa nodes
   * -2 any numa node would do (e.g. for join)
   * -1 unspecified (error)
   */
	shared_ptr<BundleBase> getOneBundle(int node = -1) {

		assert(node != -1);

#ifdef INPUT_ALWAYS_ON_NODE0
		node = 0;
#endif

		unique_lock <mutex> lock(_bundle_mutex);

		if (node == -2) {
			for (int n = 0; n < MAX_NUMA_NODES; n ++)
				if (!numa_bundles[n].empty()) {
					node = n;
					break;
				}
		}
		/* if all empty, node == -2 */

		auto & bundles = numa_bundles[node];

		if (bundles.empty() || node == -2) {
			/* debugging */
			EE("%s: bug? : bundles %u %u %u %u; min_ts %s",
			        getName().c_str(),
			        (unsigned int)(numa_bundles[0].size()),
							(unsigned int)(numa_bundles[1].size()),
							(unsigned int)(numa_bundles[2].size()),
							(unsigned int)(numa_bundles[3].size()),
			        to_simplest_string(min_ts).c_str());

			return nullptr;
		} else {
			shared_ptr<BundleBase> ret = bundles.back();
			bundles.pop_back();

			/* xzl: find min_ts among the remaining bundles.
			 * expensive? */
			min_ts = etime_max;
			for (auto& q : numa_bundles) {
				for (auto& b : q) {
					if (b->min_ts < min_ts)
						min_ts = b->min_ts;
				}
			}

#ifndef INPUT_ALWAYS_ON_NODE0
			assert(ret->node == node);
//#else /* we don't touch the nodeid saved in bundle */
//			assert(ret->node == 0);
#endif

#if 0 /* debugging */
			EE("%s: --- okay --- : bundles %u %u %u %u; min_ts %s",
			        getName().c_str(),
			        (unsigned int)(numa_bundles[0].size()),
							(unsigned int)(numa_bundles[1].size()),
							(unsigned int)(numa_bundles[2].size()),
							(unsigned int)(numa_bundles[3].size()),
			        to_simplest_string(min_ts).c_str());
#endif

			return ret;
		}
	}

	// @node: -1 does not care
  // after this, the bundle is owned by the downstream transform
  void depositOneBundle(shared_ptr<BundleBase> bundle, int node = -1) {
    assert(bundle);

#ifdef INPUT_ALWAYS_ON_NODE0
    /* we enqueue to bundle to node0, w/o changing the nodeid saved in
     * the bundle, which will be used to determine task home node. */
    node = 0;
//    bundle->node = 0;
#endif

    if (node == -1)
    	node = bundle->node; /* best effort determining the dest node */

    ASSERT_VALID_NUMA_NODE(node);

    /* sanity check */
#ifndef INPUT_ALWAYS_ON_NODE0
#if 1
    if (bundle->node != node)
			EE("bundle->node %d node %d", bundle->node, node);
#endif
    assert(bundle->node == node);
#endif

    unique_lock <mutex> lock(_bundle_mutex);

    auto & bundles = numa_bundles[node];

    bundle->value = this;
    bundles.push_front(bundle);
    if (bundle->min_ts < min_ts)
      min_ts = bundle->min_ts;

    W("%s: bundle %s, after: bundles %u %u %u %u; min_ts %s",
        getName().c_str(),
        to_simplest_string(bundle->min_ts).c_str(),
        (unsigned int)(numa_bundles[0].size()),
				(unsigned int)(numa_bundles[1].size()),
				(unsigned int)(numa_bundles[2].size()),
				(unsigned int)(numa_bundles[3].size()),
        to_simplest_string(min_ts).c_str());
  }

  /* return total # of bundles. peek w/o lock.
   * -1: return the sum of bundles on all nodes */
  int size(int node = -1) {

  	assert(node < MAX_NUMA_NODES);

  	int sz = 0;
  	if (node == -1) {
  		for (auto & b : numa_bundles)
  			sz += b.size();
  		return sz;
  	} else
  		return numa_bundles[node].size();
  }

  // the ts of the oldest record across all bundles (across all nodes)
  creek::etime min_ts;

protected:
  /* so far one lock for all NUMA queues. this is because each deposit/get
   * action will have to update min_ts, which needs to lock all queues anyway.
   * XXX revisit if the # of bundles becomes high
   */
  mutex _bundle_mutex;
//  mutex _bundle_mutex[MAX_NUMA_NODES];

public:
  /* existing Bundles to be consumed (by the next transform)
   * XXX the queues are accessed from different core/nodes. do cacheline
   * alignment
   * XXX using lockfree d/s
   */
  bundle_queue_t numa_bundles[MAX_NUMA_NODES];

#if 0
  PValue() {
  	for (auto & t : min_ts) {
  		t = max_date_time;
  	}
  }
#endif
};

class PValueBase : public PValue {
public:
	Pipeline* getPipeline() {
		return _pipeline;
	}

	AppliedPTransform* getProducingTransformInternal() {
		return _producingTransform;
	}

	void recordAsOutput (AppliedPTransform* transform) {

	}

	void finishSpecifyingOutput() { }

	// return a copy
	basic_string<char> getName() {
//	  assert(!_name.empty()); return _name;
	  if (_name.empty())
	    _name = "[" + string(typeid(*this).name()) + "]";
	  return _name;
	}

	// create a copy
	void setName(basic_string<char> name) { assert(!_finishedSpecifying); this->_name = name; }

	bool isFinishedSpecifyingInternal() {
		return _finishedSpecifying;
	}

	void finishSpecifying() {
		_finishedSpecifying = true;
	}

	basic_string<char> toString() {
		return (_name.empty() ? "<unamed>"
				: getName() + "[" + getKindString() +"]");
	}

	list<PValue *>* expand() { return new list<PValue *>({this}); }

protected:
	Pipeline* _pipeline;
public:
  basic_string<char> _name;
private:
	AppliedPTransform* _producingTransform ;
	bool _finishedSpecifying;

protected:

	PValueBase(Pipeline *pipeline, basic_string<char> name)
		: _pipeline(pipeline), _name(name), _producingTransform(NULL),
		  _finishedSpecifying(false) { }

  PValueBase(Pipeline *pipeline)
    : _pipeline(pipeline), _producingTransform(NULL),
      _finishedSpecifying(false) { }

	void recordAsOutput(AppliedPTransform* t, const basic_string<char> & outName) {
		if (_producingTransform) // already have one producing transform
			return;

		_producingTransform = t;

		if (_name.empty()) {
			_name = t->getFullName() + "." + outName;
		}
	}

	basic_string<char> getKindString() {
		// TODO: see Beam impl.
		return string(typeid(*this).name());
	}
};

class PDone : PValueBase {
private:
	PDone(Pipeline* pipeline) : PValueBase(pipeline, "done") {
	}

public:
	static PDone* in(Pipeline* pipeline) {
		return new PDone(pipeline);
	}

	basic_string<char> getName() {
		return "pdone";
	}

    // A PDone contains no PValues.
	list<PValue *>* expand() { return new list<PValue *>(); }

	void finishSpecifyingOutput() {}
};

class PCollection : public PValueBase {
public:
	// xzl: c++11 feature: scoped enum
	enum class IsBounded {
		BOUNDED,
		UNBOUNDED
	};

	IsBounded And(IsBounded that) {
		if (this->_isBounded == IsBounded::BOUNDED
				&& that == IsBounded::BOUNDED)
			return IsBounded::BOUNDED;
		else
			return IsBounded::UNBOUNDED;
	}

	basic_string<char> getName() { return PValueBase::getName(); }

	// return a ref
	PCollection* setName(basic_string<char> name) { PValueBase::setName(name); return this; }

#if 1
	PValue* apply(basic_string<char> name, PTransform* t) {
	  return Pipeline::applyTransform(name, this, t);
	}

  PValue* apply(PTransform* t) { return Pipeline::applyTransform(this, t); }
#endif

	/* xzl: simply making the connection */
  PCollection* apply1(PTransform* t);

  vector<PCollection*> apply2(PTransform* t);

	IsBounded isBounded() { return _isBounded; }

	WindowingStrategy* getWindowingStrategy() {
		return _windowingStrategy;
	}

	PCollection* setWindowingStrategyInternal(WindowingStrategy* s) {
		this->_windowingStrategy = s;  // overwrite
		return this;
	}

	PCollection* setIsBoundedInternal(IsBounded isBounded) {
		this->_isBounded = isBounded;
		return this;
	}

#if 0
	static PCollection<T>* createPrimitiveOutputInternal(Pipeline* p, WindowingStrategy* s, IsBounded isBounded) {
		PCollection<T> *c = new PCollection<T> (p);
		return c->setWindowingStrategyInternal(s)->setIsBoundedInternal(isBounded);
	}
#endif

  static PCollection* createPrimitiveOutputInternal(Pipeline* p) {
    PCollection *c = new PCollection (p);
    return c;
  }

	list<PValue *>* expand() { return new list<PValue *>({this}); }

	// needed as PInput
	Pipeline* getPipeline() { return PValueBase::getPipeline(); }

	// needed as PInput
	void finishSpecifying() {
		PValueBase::finishSpecifying();  // as POutput
		// TODO: as input??
	}

#if 0
	BundleBase* getOneBundle() {
	  unique_lock <mutex> lock(_bundle_mutex);
	  if (bundles.empty())
	    return nullptr;
	  else {
	      BundleBase* ret = bundles.back();
	      bundles.pop_back();
	      return ret;
	  }
	}

	void depositOneBundle(BundleBase* bundle) {
	  assert(bundle);
	  unique_lock <mutex> lock(_bundle_mutex);
	  bundles.push_front(bundle);
	  bundle->collection = this;
	}
#endif

private:
	IsBounded _isBounded;
	WindowingStrategy* _windowingStrategy;

public:
	PCollection(Pipeline* p)
		: PValueBase(p),
		  _isBounded(IsBounded::BOUNDED),
		  _windowingStrategy(NULL)
			{ }

	PCollection(Pipeline* p, basic_string<char> name)
	    : PValueBase(p, name),
	      _isBounded(IsBounded::BOUNDED),
	      _windowingStrategy(NULL)
	      { }
};

class PBegin : public PValueBase {
public:
	static PBegin* in(Pipeline *p) {
		return new PBegin(p);
	}

	PValue* apply(PTransform *p) { return Pipeline::applyTransform(this, p); }

	PValue* apply(basic_string<char> name, PTransform *p) { return Pipeline::applyTransform(name, this, p); }

	PCollection* apply1(PTransform *p);

	vector<PCollection*> apply2(PTransform* t);

	vector<PCollection*> applyK(PTransform* t, int k);

	void finishSpecifying() { }

	list<PValue *>* expand() { return new list<PValue *>(); }

	basic_string<char> getName() { return "pbegin"; }

protected:
	PBegin(Pipeline *p) : PValueBase(p, "begin") { }
};

#include "Values.h"

#endif //CREEK_PVALUES_H
