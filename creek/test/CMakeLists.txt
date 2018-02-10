#
# ref: http://stackoverflow.com/questions/7724569/debug-vs-release-in-cmake
#
cmake_minimum_required(VERSION 2.8.9)

project(creek)

include_directories(
../
../CTPL
../Kaskade
#../tbb/include  # not used
/ssd/local/include
#../../libcuckoo
#../../libcuckoo/cityhash-1.1.1/src
)

# common source shared
SET (SRC_WINSUM
		../WinSum/WinSumBase.cpp
		../WinSum/WinSum_addlong.cpp
		../WinSum/WinSum_mergeset.cpp
		../WinSum/WinSum_mergevector.cpp
		)

SET( SOURCES
		../core/Pipeline.cpp ../BoundedWindow.cpp ../Values.cpp ../core/Transform.cpp
		../Source/Unbounded.cpp
		../Mapper/WordCountMapper.cpp
		../Win/WinGBK.cpp
		../WinKeyReducer/WinKeyReducer.cpp
		../Mapper/GrepMapper.cpp
		../Mapper/WindowedGrepMapper.cpp
		../Sink/Sink.cpp
		../Sink/WindowsBundleSink.cpp
../LookAheadPipelineVisitor.cpp
		../Win/FixedWindowInto.cpp
../measure.c
		../core/EvaluationBundleContext.cpp
		../Mapper/SimpleMapper.cpp
		../Join/Join.cpp
		../Source/NetworkLatencySource.cpp
#../Executor.cpp
#../Kaskade/utilities/kalloc.cpp         # for numa allocator
#../Kaskade/utilities/threading.cpp      # for numa allocator
test-common.cpp
        ../WinKeyReducer/WinKeyReducerEval-wc-winbundle.cpp
		../WinKeyReducer/WinKeyReducerEval-netmon.cpp
		../Source/Unbounded_Join.cpp
		${SRC_WINSUM}
)

SET( CMAKE_CXX_COMPILER "clang++" )  # has some issues with template template class...
#SET( CMAKE_CXX_COMPILER "g++-5" )
#SET( CMAKE_CXX_COMPILER "icc" )

# -- apply to all configurations
#SET( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -std=c++11 -g -fcilkplus -Wall" )
SET( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -include config.h" )

SET( CMAKE_CXX_FLAGS_DEBUG
	"${CMAKE_CXX_FLAGS_DEBUG} -std=c++11 -g -O0 -Wall -DHAVE_NUMA -Wfatal-errors" )

#SET( CMAKE_CXX_FLAGS_RELEASE  "${CMAKE_CXX_FLAGS_RELEASE} -std=c++11 -g -fcilkplus -Wall -O2" )
SET( CMAKE_CXX_FLAGS_RELEASE  "-std=c++11 -g -Wall -O2 -DHAVE_NUMA" )

SET( CMAKE_CXX_FLAGS_MINSIZEREL
			"${CMAKE_CXX_FLAGS} -std=c++11 -g -fcilkplus -Wall" )

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_definitions(-DCONFIG_KAGE_GLOBAL_DEBUG_LEVEL=30)	
#	add_definitions(-D_GLIBCXX_DEBUG)	   # will fail compilation in folly
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
	add_definitions(-DCONFIG_KAGE_GLOBAL_DEBUG_LEVEL=50)
	add_definitions(-DNDEBUG)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
	add_definitions(-DCONFIG_KAGE_GLOBAL_DEBUG_LEVEL=20)
endif()

# SET( CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -lboost_date_time" )
SET( CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -pthread" )
#SET( CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS}" )
#SET( CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -pthread -L/home/xzl/tmp/boost_1_61_0/stage/lib -static" )

#add_executable(CreateTest.bin CreateTest.cpp ${SOURCES})

add_executable(test-grep.bin 
      test-grep.cpp
		../Sink/RecordBundleSink.cpp
      ${SOURCES}
)

add_executable(test-join.bin test-join.cpp ${SOURCES})

add_executable(
test-wingrep.bin test-wingrep.cpp
		../Sink/RecordBundleSink.cpp
${SOURCES}
)
add_executable( test-distinct.bin 
test-distinct.cpp 
${SOURCES}
		../Sink/RecordBundleSink.cpp
)

add_executable(test-join-2.bin test-join-2.cpp ${SOURCES})

add_executable(networklatency.bin 
	networklatency.cpp
		../core/Pipeline.cpp ../BoundedWindow.cpp ../Values.cpp ../core/Transform.cpp
		../Source/Unbounded.cpp
		../Win/WinGBK.cpp
	#../WindowKeyedReducer.cpp
		../WinKeyReducer/WinKeyReducer-netmon.cpp
	#../Sink.cpp
		../Sink/WindowsBundleSink.cpp
	../LookAheadPipelineVisitor.cpp
		../Win/FixedWindowInto.cpp
	../measure.c
		../core/EvaluationBundleContext.cpp
		../Source/NetworkLatencySource.cpp
	#../Kaskade/utilities/kalloc.cpp         # for numa allocator
	#../Kaskade/utilities/threading.cpp      # for numa allocator
	test-common.cpp
		../WinKeyReducer/WinKeyReducerEval-netmon.cpp
)	

add_executable(test-wc.bin 
	test-wc.cpp
		../core/Pipeline.cpp ../BoundedWindow.cpp ../Values.cpp ../core/Transform.cpp
		../Source/Unbounded.cpp
		../Mapper/WordCountMapper.cpp
		../Win/WinGBK.cpp
	#../WindowKeyedReducer.cpp
        ../WinKeyReducer/WinKeyReducer-wc-winbundle.cpp
	${SRC_WINSUM}
		../WinKeyReducer/WinKeyReducer.cpp
		../Mapper/GrepMapper.cpp
		../Mapper/WindowedGrepMapper.cpp
	#../Sink.cpp	
		../Sink/WindowsBundleSink.cpp
	../LookAheadPipelineVisitor.cpp
		../Win/FixedWindowInto.cpp
	../measure.c
		../core/EvaluationBundleContext.cpp
	#../SimpleMapper.cpp
	#../Join.cpp
	#../NetworkLatencySource.cpp
	#../Executor.cpp
	#../Kaskade/utilities/kalloc.cpp         # for numa allocator
	#../Kaskade/utilities/threading.cpp      # for numa allocator
	test-common.cpp
        ../WinKeyReducer/WinKeyReducerEval-wc-winbundle.cpp
	#../WindowKeyedReducerEval-netmon.cpp
)

add_executable(test-tweet.bin 
	test-tweet.cpp
		../core/Pipeline.cpp ../BoundedWindow.cpp ../Values.cpp ../core/Transform.cpp
		../Source/Unbounded.cpp
		../Mapper/WordCountMapper.cpp
		../Win/WinGBK.cpp
		../Mapper/WinMapper.cpp
	#../WindowKeyedReducer.cpp
        ../WinKeyReducer/WinKeyReducer-wc-winbundle.cpp
	#../WindowKeyedReducer-netmon.cpp
		../WinSum/WindowedSumEvaluator1-tvpair.cpp
	${SRC_WINSUM}
	#../WindowKeyedReducer.cpp
	#../GrepMapper.cpp
	#../WindowedGrepMapper.cpp
		../Sink/Sink.cpp
		../Sink/WindowsBundleSink.cpp
		../Sink/RecordBundleSink.cpp
	../LookAheadPipelineVisitor.cpp
	#../FixedWindowInto.cpp
	../measure.c
		../core/EvaluationBundleContext.cpp
	#../SimpleMapper.cpp
		../Join/Join.cpp
	#../NetworkLatencySource.cpp
	#../Executor.cpp
	#../Kaskade/utilities/kalloc.cpp         # for numa allocator
	#../Kaskade/utilities/threading.cpp      # for numa allocator
	test-common.cpp
	#../WindowKeyedReducerEval-wc.cpp
	#../WindowKeyedReducerEval-netmon.cpp
)

set_target_properties(test-tweet.bin
         PROPERTIES COMPILE_FLAGS "-DWORKAROUND_JOIN_JDD=1 -DWORKAROUND_WINKEYREDUCER_RECORDBUNDLE=1"
)                                
                                
#add_executable(test-WordCountMapper.bin test-WordCountMapper.cpp ${SOURCES})

# -- uncomment to turn on the target
#add_executable(test-kaskade test-kaskade.cpp
#../Kaskade/utilities/kalloc.cpp
#../Kaskade/utilities/threading.cpp
#)

FIND_LIBRARY(tbb NAMES libtbb.so PATHS ../tbb/lib/intel64/gcc4.7)
FIND_LIBRARY(tbb_malloc NAMES libtbbmalloc.so PATHS ../tbb/lib/intel64/gcc4.7)
FIND_LIBRARY(folly NAMES libfolly.a PATHS /ssd/local/lib)
#FIND_LIBRARY(cityhash NAMES libcityhash.a PATHS /ssd/local/lib)

#TARGET_LINK_LIBRARIES(CreateTest.bin boost_date_time boost_system boost_thread numa ${tbb})
#TARGET_LINK_LIBRARIES(test-join.bin boost_date_time boost_system boost_thread numa ${tbb})


TARGET_LINK_LIBRARIES(test-join-2.bin 
  boost_date_time 
  boost_system 
  boost_thread 
  boost_program_options
  numa ${tbb}
  ${folly}
  re2
  pcre
)


TARGET_LINK_LIBRARIES(networklatency.bin 
  boost_date_time 
  boost_system 
  boost_thread 
  boost_program_options
  numa ${tbb}
  ${folly} glog double-conversion  # needed by atomic hash map  
  re2
  pcre
)

TARGET_LINK_LIBRARIES(test-join.bin 
  boost_date_time 
  boost_system 
  boost_thread 
  boost_program_options
  numa ${tbb}
  ${folly}
  re2
  pcre
)

TARGET_LINK_LIBRARIES(test-wc.bin
  boost_date_time
  boost_system
  boost_thread
  boost_program_options
  numa ${tbb}
  jemalloc
#  ${tbb_malloc}
  ${folly}
  re2
  pcre
)

TARGET_LINK_LIBRARIES(test-tweet.bin
  boost_date_time
  boost_system
  boost_thread
  boost_program_options
  numa ${tbb}
  jemalloc
  ${folly}
  re2
  pcre
)

TARGET_LINK_LIBRARIES(test-grep.bin
	boost_date_time
	boost_system
	boost_thread
	boost_program_options
	numa ${tbb}
	${folly}
	re2
	pcre
)

TARGET_LINK_LIBRARIES(test-wingrep.bin
  boost_date_time
  boost_system
  boost_thread
  boost_program_options
  numa ${tbb}
  ${folly}
  re2
  pcre
)

TARGET_LINK_LIBRARIES(test-distinct.bin
  boost_date_time
  boost_system
  boost_thread
  boost_program_options
  numa ${tbb}
  ${folly} glog double-conversion  # needed by atomic hash map
  ${cityhash} # needed by libcuckoo
  re2
  pcre
)

#TARGET_LINK_LIBRARIES(test-WordCountMapper.bin
#	boost_date_time
#	boost_system
#	boost_thread
#	numa ${tbb}
#	re2
#	pcre
#)
#TARGET_LINK_LIBRARIES(test-kaskade boost_date_time boost_system boost_thread numa)

# can't name test
#add_custom_target(all 
#	DEPENDS test-wc.bin test-grep.bin test-distinct.bin test-wingrep.bin
#	)
