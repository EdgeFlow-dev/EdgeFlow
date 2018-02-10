# README #

## instructions on building for aarch64/xplane

# dev machine
sudo mkdir -p /shared/
chown ....
scp -r k2:/shared/creek-shared /shared/ # contains toolchain and all libs/dependency

# new build, or after changing tee
cd ~/tee
../build/install-libxplane.sh    # copy tee/libxplane lib and headers to /shared/

# build creek -- see below


Stream processing on shared memory machine. Follow the Google dataflow (Apache Beam) programming model.

## Dependency

### can be installed from distro repo
sudo apt-get install libtbb-dev 

### required by folly
sudo apt-get install \
    g++ \
    automake \
    autoconf \
    autoconf-archive \
    libtool \
    libboost-all-dev \
    libevent-dev \
    libdouble-conversion-dev \
    libgoogle-glog-dev \
    libgflags-dev \
    liblz4-dev \
    liblzma-dev \
    libsnappy-dev \
    make \
    zlib1g-dev \
    binutils-dev \
    libjemalloc-dev \
    libssl-dev

## on ARM target
apt-get install \ 
    libboost-all-dev

libboost
libnuma
jemalloc
pcre


### source
## folly. # assuming prefix = /ssd/local/
# ref: https://github.com/facebook/folly
autoreconf -ivf
./configure --prefix=/ssd/local/
make -j20
make install


## to rebuild

Warning: slow. takes 5 min on a quad core i5 machine 
TODO: make all targets use libcreek.a

# x86 Release
rm -f CMakeCache.txt
/usr/bin/cmake -DCMAKE_BUILD_TYPE=Release -G "CodeBlocks - Unix Makefiles" /home/xzl/creek/
make -j40

# # x86 Debug
rm -f CMakeCache.txt
/usr/bin/cmake -DCMAKE_BUILD_TYPE=Debug -G "CodeBlocks - Unix Makefiles" /home/xzl/creek/
make -j40

# run cmake for arm
rm -f CMakeCache.txt
/usr/bin/cmake \
-DCMAKE_ENV=arm \
-DCMAKE_BUILD_TYPE=Release \
-G "CodeBlocks - Unix Makefiles" /home/xzl/creek/
make -j40

# build for a target
/usr/bin/cmake --build /home/xzl/creek/cmake-build-debug --target test-wingbk.bin -- -j 10

# clean
/usr/bin/cmake --build /home/xzl/creek/cmake-build-debug --target clean -- -j 10


mkdir Debug
cd Debug
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j4

cmake -DCMAKE_BUILD_TYPE=Release ..


## the following notes may be outdated

## Todo
make mtx_container shared mutex. Split it into rlock/wlock. So that the fast path in getOneBundle() only takes rlock. 

## Transform code structure

We split each transform into .h and .cpp.

This is because of that one transform's ExecEvaluator() and OnWatermarkRefresh() needs to invoke the corresponding evaluator.
Thus, the transform's header file (e.g. WindowedSum.h) includes the evaluator's header file (e.g. WindowedSumEvaluator.h).

On the other hand, an evaluator also needs to refer to its transform, e.g. for manipulating the transform state. As such, the evaluator's header file (e.g. WindowedSumEvaluator.h) needs to include the transform's header file.

This will lead to circular dependency.

Our solution: we let the evaluator's header include the transform's header. In the transform's header, we only *declare* ExecEvaluator() and OnWatermarkRefresh(). Their implementations go to a separate transform.cpp file (e.g. WindowedSum.cpp).

## Watermark propagation call chain

Source: SeedWatermarkBlocking()
--> [EvaluationBundleContext] OnNewUpstreamWatermark
--> [transform]
		RefreshWatermark
		OnNewUpstreamWatermark (this knows the corresponding evaluator class)
--> [eval] OnNewUpstreamWatermark

## Watermark refresh timing

/* the upstream wm should be larger than records that are yet to be
 * processed. otherwise, we will observe the wm before processing records
 * that are prior to the wm. */

wm propagation must happen after proir records are *consumed* by downstream
(not just *observed*). this is because first observing wm and then consume records
earlier than wm is also an violation

upstream wm can only ensure downstream "observe" but not "consume".
reason: before propagating a wm, the upstream cannot ensure all prior records
are consumed (otherwise it has to track the *consumption* of all oustanding bundles)

however, observation is easier to guarantee: as long as upstream flushes its
internal state and deposits to downstream's input, it is okay.

the input queue is *owned* by downstream.

---

vanilla: on a transform, wm held back by upstream, input, and inflights

when wm propagates, it recalculates the local wm, flushes state, and propagates
wm downwards (does not wait for input & inflights to finish)

---

T -- transforms
US -- upstream
DS -- downstream

punctuation design to increase parallelism:

A punc is a special record (bundle). It carries a snapshot of the local wm.

T emits bundles sequentially.
By emiting a punc:  B' --> punc --> B --> (t)
US provides guarantees to DS that all bundles emitted later than the punc (B') will not carry ts earlier than wm.
For DS, it is vital to keep the order between punc -> B; otherwise the guarantee is broken.

US emits 1) all bundles prior to wm and 2) punc. US enqueues them in order to DS's input queue.
The evaluator of DS, which run multiple threads, deques B and then wm from its input queue.
However, when the evaluator processes wm, B (which may carry ts <= wm) may still be "in flight", i.e. being processed by other threads of DS.

Upon seeing a punc, the evaluator can do two things:
1. it immediately updates the DS's local wm to be min(inflight, up_wm); it sends the new local wm as punc to downstream
2. it waits until all inflight bundles are processed; it updates the local wm to be up_wm and sends as punc

or, the evaulator can do 1+2.

1 incurs no delay, but does not advance the local wm as much (i.e. as much as up_wm)
2 incurs longer delay (wait for inflight bundles), but advances local wm all the way to up_wm.

it's worth noting that, in wait of 2 the transform does not halt; the evaluator continues to process bundles that arrive than the up_wm punc.

punc depends on that all prior bundles are consumed by downstream
	later bundles do *not* depend on punc. we do not keep bundles->punc
	they can be processed while punch is on hold


-----

punc should be processed after all prior bundles are *processed*
	these bundles left the current transform
	the derived state enters downstream
	can be implemented using c++ future

when punc is processed, a local wm is evaluated based on the wm snapshot and current local inflight/inputs.

local wm becomes a punc inserted to the downstream input
	this is safe since all dependant bundles have already left the transform

throughout the pipeline
for stateless transform: keep wm --> prior bundles

## stateful transform

bundles2 [wm] bundles1

Upon seeing [wm], evaluator often flushes the internal state accordingly.

evaluator actions
1. observing [wm]
2. wait until all inflight bundles prior to [wm] done
	otherwise, step 3 cannot guarantee to emit all state prior to [wm]
3. purge state
4. output a punc [wm] to downstream


processing bundles2 does not have to wait for [wm].

Reason: transform has to organizes its internal state as windows anyway. Otherwise, bundles1 may contain records whose time > wm, and flushing may purge these records.



baseline design: bundles2 cannot be processed until the transform processes wm. since wm may cause flushing.
	the flushing is supposed to purge bundles1 (and earlier), but now may mistakenly flush the state of bundles2.
	this can be avoid, e.g. if the timestamps are still kept inside transform, and the flushing considers each record's timestamps.
	however, this may not always be the case, e.g. aggregation.

better design: while wm is pending, bundles2 can be processed on a "clone" transform. the state will be separated the existing ones and is therefore protected from flushing. once wm is processed, the clone state can be combined into the transform's main copy.


## add Google RE2

sudo apt-get install libre2-dev \
	libboost-thread-dev \


## measure latency
1. insert a special URL to the first line of input file, like "http://www.hongyumiao.com"
2. use URL regex in test-grep.cpp: "(((http[s]{0,1}|ftp)://)[a-zA-Z0-9\\.\\-]+\\.([a-z]|[A-Z]|[0-9]|[/.]|[~]|[-])*)" 
3. add add_definitions(-DMEASURE_LATENCY) to CMakeLists.txt

## debug concurrency
1. define DEBUG_CONCURRENCY in config.h
2. search DEBUG_CONCURRENCY in code, where we can define # of bundles and # of worker threads
