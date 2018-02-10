#
# ref: http://stackoverflow.com/questions/7724569/debug-vs-release-in-cmake
#
# moved
cmake_minimum_required(VERSION 2.8.9)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

project(creek-arm)

include_directories(
        ./
        /shared/CTPL
        /shared/Kaskade
        /shared/fast-cpp-csv-parser
        /shared/include
)

include_directories(${CMAKE_BINARY_DIR}/generated)

set(shared_path "k2:/tmp/")

add_subdirectory(zmq_stream)

if(CMAKE_ENV STREQUAL "arm")
		include_directories(
			./xplane/include
			./xplane/include/mm
			./xplane/include/
		)
	set(arm_target_path $ENV{BOARD}":~/arm")
    add_subdirectory(xplane)
elseif(CMAKE_ENV STREQUAL "armtz")
	set(arm_target_path $ENV{BOARD}":~/armtz")
endif(CMAKE_ENV STREQUAL "arm")

#
# CMAKE_ENV to be passed to cmake, switch it through command line.
#

# in case of use on arm cross toolchain
if(CMAKE_ENV STREQUAL "arm" OR CMAKE_ENV STREQUAL "armtz")
    # add required libraries
    # all libraries are in /shared/hikey
    include_directories(
            /shared/hikey/include
    )
    link_directories(
            /shared/hikey/lib
            /shared/hikey/lib/aarch64-linux-gnu
#            /shared/lib
    )
    SET(CMAKE_SYSTEM_NAME Linux)
    SET(CMAKE_SYSTEM_PROCESSOR aarch64)

    SET(COMPILER_PATH "/shared/hikey/toolchains/aarch64/bin")
    SET(TARGET_ABI "linux-gnu")
    # specify the cross compiler
    SET(CMAKE_C_COMPILER "${COMPILER_PATH}/aarch64-${TARGET_ABI}-gcc")
    SET(CMAKE_CXX_COMPILER "${COMPILER_PATH}/aarch64-${TARGET_ABI}-g++")

    SET(CMAKE_FIND_ROOT_PATH)
    # search for programs in the build host directories
    SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
    # for libraries and headers in the target directories
    SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
    SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
    SET(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_UARRAY")
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations") # remove warning of auto_ptr

#elseif(CMAKE_ENV STREQUAL "x86")
else()    # default: x86
    #SET( CMAKE_CXX_COMPILER "clang++" )  # has some issues with template template class...
    SET( CMAKE_CXX_COMPILER "g++-5" )
    #SET( CMAKE_CXX_COMPILER "icc" )
endif(CMAKE_ENV STREQUAL "arm" OR CMAKE_ENV STREQUAL "armtz")

# CLion needs to explicitly know all the headers, per its FAQ

# Get the current working branch
execute_process(
        COMMAND git rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_BRANCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get the latest abbreviated commit hash of the working branch
execute_process(
        COMMAND git log -1 --format=%h
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

#SET(HEADERS
file(GLOB HEADERS
        "Source/*.h"
        "core/*.h"
        "ParDo/*.h"
        "Sink/*.h"
        "Win/*.h"
        "Join/*.h"
        "WinSum/*.h"
        "Mapper/*.h"
        "Select/*.h"
        AppliedPTransform.h
        BoundedWindow.h
        Create.h
        DoFn.h
        #Join/JoinEvaluator.h
        LookAheadPipelineVisitor.h
        MinimalContext.h
        MinimalProcessContext.h
        NopEvaluator.h
        OffsetRangeTracker.h
        PaneInfo.h
        PerBundleContext.h
        StringUtils.h
        ValueContainer.h
        Values.h
        PValues.h
        Bundle.h
        BundleContainer.h
        SeqBundle.h
        SeqBuf.h
        Record.h
        WindowFn.h
        WinKeyFrag.h
        WinKeyReducer/WinKeyReducer.h
        WinKeyReducer/WinKeyReducerEval.h
        #WinKeyReducer/WinKeyReducerEval-MT.h
        #WinKeyReducer/WinKeyReducerEvalRecordBundle.h
        WinKeyReducer/WinKeyReducer-wc-common.h
        WinKeyReducer/WinKeyReducerEval-wc-common.h
        WindowingStrategy.h
#        Sink/WindowsBundleSinkEvaluator.h
        WindowsHolder.h
        config.h
        creek-map.h
        creek-types.h
        linux-sizes.h
        pipeline-config.h
        log.h
        measure.h
        WinKeySeqFrag.h
        uarray.h
        util.h
        )

# these .cpps go to the lib. so they require external dependency on demand
#

# common source shared
file(GLOB SRC_WINSUM
        "WinSum/*.cpp"
#        WinSum/WinSum_addlong.cpp
#        WinSum/WinSum_mergeset.cpp
#        WinSum/WinSum_mergevector.cpp
#        WinSum/WinSum_addlong_tvpair.cpp
        )

# black list some files
list(REMOVE_ITEM SRC_WINSUM ${CMAKE_CURRENT_SOURCE_DIR}/WinSum/WindowedSumEvaluator1-tvpair.cpp) # not maintained?
list(REMOVE_ITEM SRC_WINSUM ${CMAKE_CURRENT_SOURCE_DIR}/WinSum/WinSum_addlong.cpp) # not maintained?
list(REMOVE_ITEM SRC_WINSUM ${CMAKE_CURRENT_SOURCE_DIR}/WinSum/WinSum_mergevector.cpp) # not maintained?
list(REMOVE_ITEM SRC_WINSUM ${CMAKE_CURRENT_SOURCE_DIR}/WinSum/WinSum_mergeset.cpp) # not maintained?
list(REMOVE_ITEM SRC_WINSUM ${CMAKE_CURRENT_SOURCE_DIR}/WinSum/WinSum_addlong_tvpair.cpp) # not maintained?


SET(SRC_JOIN
#        Join/Join.cpp
        Join/JoinSeq.cpp)

SET(SRC_REDUCER
        WinKeyReducer/WinKeyReducer.cpp
        WinKeyReducer/WinKeyReducer-wc-winbundle.cpp
        WinKeyReducer/WinKeyReducer-wc-recordbundle.cpp
        WinKeyReducer/WinKeyReducer-netmon.cpp
        WinKeyReducer/WinKeyReducerEval-wc-winbundle.cpp
        WinKeyReducer/WinKeyReducerEval-wc-recordbundle.cpp
        WinKeyReducer/WinKeyReducerEval-netmon.cpp
        )

file(GLOB SRC_MAPPER
        "Mapper/*.cpp")

file(GLOB LIB_SOURCES
        ${HEADERS}
        core/Pipeline.cpp
        BoundedWindow.cpp
        Values.cpp
        PValues.cpp
        BundleContainer.cpp
        util.cpp
        core/Transform.cpp
#        core/Transforms-JD.cpp
#        core/Transforms-JDD.cpp
        SeqBuf.cpp
        SeqBuf2.cpp
        LookAheadPipelineVisitor.cpp
        measure.c
        stacktrace.cxx
        core/EvaluationBundleContext.cpp
        #../Executor.cpp
        #../Kaskade/utilities/kalloc.cpp         # for numa allocator
        #../Kaskade/utilities/threading.cpp      # for numa allocator
        test/test-common.cpp
        #Source/Unbounded_Join.cpp      # xzl: don't maintain
        #${SRC_WINSUM}      # xzl: NB --- clang won't compile
        ${SRC_JOIN}
        #${SRC_REDUCER}
        "Win/*.cpp"
        "Source/*.cpp"
        #${SRC_MAPPER}
        "Sink/*.cpp"
        SetArray.cpp)

list(REMOVE_ITEM LIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Source/Unbounded.cpp) # obsoleted.
list(REMOVE_ITEM LIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Source/UnboundedSeq.cpp) # obsoleted. see SourceSeq.cpp
list(REMOVE_ITEM LIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Source/Unbounded_Join.cpp) # not maintained?
list(REMOVE_ITEM LIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Source/NetworkLatencySource.cpp) # not maintained?
list(REMOVE_ITEM LIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Sink/RecordBundleSink.cpp) # obsoleted. see SourceSeq.cpp
list(REMOVE_ITEM LIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Sink/Sink.cpp) # obsoleted. see SourceSeq.cpp
list(REMOVE_ITEM LIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Sink/WindowsBundleSink.cpp) # obsoleted. see SourceSeq.cpp

#######################################
# flag manipulation
#######################################

# -- apply to all configurations
# must come before set CMAKE_CXX_FLAGS_DEBUG/RELEASE
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -include config.h -fdiagnostics-color=auto -Wuninitialized")
if(CMAKE_ENV STREQUAL "armtz")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_TZ")
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUSE_TZ")
endif()

# xzl: -Wfatal-errors to stop compile after 1st error.
SET(CMAKE_CXX_FLAGS_DEBUG
        "${CMAKE_CXX_FLAGS_DEBUG} -std=c++11 -g -O0 -Wall -DHAVE_NUMA -fmax-errors=10 -fstack-protector-all")

#SET( CMAKE_CXX_FLAGS_RELEASE  "${CMAKE_CXX_FLAGS_RELEASE} -std=c++11 -g -fcilkplus -Wall -O2" )
SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -std=c++11 -Wall -O2 -DHAVE_NUMA")

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DCONFIG_KAGE_GLOBAL_DEBUG_LEVEL=40)        # often 30
    add_definitions(-D_GLIBCXX_DEBUG)	   # will fail compilation in folly
    add_definitions(-DDEBUG)
endif ()

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    add_definitions(-DCONFIG_KAGE_GLOBAL_DEBUG_LEVEL=50)
    add_definitions(-DNDEBUG)
endif ()

### -- git stuffs: configuration file #
#add_definitions("-DGIT_COMMIT_HASH=${GIT_COMMIT_HASH}")
#add_definitions("-DGIT_BRANCH=${GIT_BRANCH}")

configure_file(
        ${CMAKE_SOURCE_DIR}/version.h.in
        ${CMAKE_BINARY_DIR}/generated/version.h
)


SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread")
#SET( CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS}" )


###########################
# libs
# singling these out as their compliation is slow.
###########################
add_library(creek STATIC
        ${LIB_SOURCES}
        )

add_library(winsum STATIC
        ${SRC_WINSUM}
        )

#add_library(reducer STATIC
#        ${SRC_REDUCER}
#        )

add_library(mapper STATIC
        ${SRC_MAPPER}
)

if(CMAKE_ENV STREQUAL "arm")
	#FIND_LIBRARY(xplane NAMES libxplane.a PATHS /shared/lib)
	SET(XPLANE_LIB "xplane")
	#SET(TEEC_LIB "teec")
elseif(CMAKE_ENV STREQUAL "armtz")
	#FIND_LIBRARY(xplane NAMES libxplane.a PATHS /shared/lib)
	SET(XPLANE_LIB "xplane-tz")
	SET(TEEC_LIB "teec")
endif()

SET(COMMON_LIB
        creek
        ${XPLANE_LIB} ${TEEC_LIB}
        boost_date_time
        boost_system
        boost_thread
#        boost_program_options
        numa
        zmq
)
###########################

#add_executable(test-winsum-long.bin
#        test/test-winsum-long.cpp
#)

add_executable(test-whereseq.bin
        test/seq/test-whereseq.cpp
        #Sink/RecordBundleSink.cpp
        )

# since we use special flag for join, we can't use the generic libcreek.a
set(SRC_WINSUM1 ${SRC_WINSUM})
list(REMOVE_ITEM SRC_WINSUM1 ${CMAKE_CURRENT_SOURCE_DIR}/WinSum/WinSumSeq.cpp) # mask the -seq version

# --------- seq test -------------- #
add_executable(test-wingbk.bin
        test/seq/test-wingbk.cpp
        )
TARGET_LINK_LIBRARIES(test-wingbk.bin
        ${COMMON_LIB}
        dl
        )
add_custom_command(TARGET test-wingbk.bin POST_BUILD
        COMMAND scp test-wingbk.bin ${arm_target_path}/${CMAKE_BUILD_TYPE}/
#        COMMAND scp test-wingbk.bin ${shared_path}
        )
# ------------------------------------- #

add_executable(test-wingbk-sum.bin
        test/seq/test-wingbk-sum.cpp
        )
TARGET_LINK_LIBRARIES(test-wingbk-sum.bin
        ${COMMON_LIB}
        dl
        )
add_custom_command(TARGET test-wingbk-sum.bin POST_BUILD
        COMMAND scp test-wingbk-sum.bin ${arm_target_path}/${CMAKE_BUILD_TYPE}/
#        COMMAND scp test-wingbk-sum.bin ${shared_path}
        )
# ------------------------------------- #

add_executable(test-win-sum.bin
        test/seq/test-win-sum.cpp
        )

add_executable(test-wingbk-med.bin
        test/seq/test-wingbk-med.cpp
        )

# ------------------------------------- #
add_executable(test-wingbk-topk.bin
        test/seq/test-wingbk-topv.cpp
        )
TARGET_LINK_LIBRARIES(test-wingbk-topk.bin
        ${COMMON_LIB}
        dl
        )
add_custom_command(TARGET test-wingbk-topk.bin POST_BUILD
        COMMAND scp test-wingbk-topk.bin ${arm_target_path}/${CMAKE_BUILD_TYPE}/
        COMMAND md5sum test-wingbk-topk.bin
        )
# ------------------------------------- #

add_executable(test-wingbk-gbk.bin
        test/seq/test-wingbk-gbk.cpp
        )
TARGET_LINK_LIBRARIES(test-wingbk-gbk.bin
        ${COMMON_LIB}
        dl
        )
add_custom_command(TARGET test-wingbk-gbk.bin POST_BUILD
        COMMAND scp test-wingbk-gbk.bin ${arm_target_path}/${CMAKE_BUILD_TYPE}/
        )

# ------------------------------------- #
add_executable(test-wingbk-uniquecount.bin
        test/seq/test-wingbk-uniquecount.cpp
        )
TARGET_LINK_LIBRARIES(test-wingbk-uniquecount.bin
        ${COMMON_LIB}
        dl
        )
add_custom_command(TARGET test-wingbk-uniquecount.bin POST_BUILD
        COMMAND scp test-wingbk-uniquecount.bin ${arm_target_path}/${CMAKE_BUILD_TYPE}/
        COMMAND md5sum test-wingbk-uniquecount.bin
        )

# ------------------------------------- #

add_executable(test-wingbk-topcount.bin
        test/seq/test-wingbk-topcount.cpp
        )
TARGET_LINK_LIBRARIES(test-wingbk-topcount.bin
        ${COMMON_LIB}
        dl
        )
add_custom_command(TARGET test-wingbk-topcount.bin POST_BUILD
        COMMAND scp test-wingbk-topcount.bin ${arm_target_path}/${CMAKE_BUILD_TYPE}/
        COMMAND md5sum test-wingbk-topcount.bin
        )

# ------------------------------------- #

#add_executable(test-wingbk-k2v.bin
#        test/seq/test-wingbk-k2v.cpp
#        )

# since we use special flag for join, we can't use the generic libcreek.a
add_executable(test-joinseq.bin
        test/seq/test-joinseq.cpp
        #core/Pipeline.cpp
        #BoundedWindow.cpp
        #core/Transform.cpp
        #Values.cpp
        #SeqBuf.cpp
        #SeqBuf2.cpp
        #Source/SourceSeq.cpp
        #Win/WinGBKSeq.cpp
        #Sink/WinKeySeqBundleSink.cpp
        #LookAheadPipelineVisitor.cpp
        #measure.c
        #core/EvaluationBundleContext.cpp
        #Join/JoinSeq.cpp
        #test/test-common.cpp
        )

# ------------------------------------- #
add_executable(test-joinfilter.bin 
        test/seq/test-joinfilter.cpp
        )
TARGET_LINK_LIBRARIES(test-joinfilter.bin
        ${COMMON_LIB}
        dl
        )
add_custom_command(TARGET test-joinfilter.bin POST_BUILD
        COMMAND scp test-joinfilter.bin ${arm_target_path}/${CMAKE_BUILD_TYPE}/
        )
# ------------------------------------- #
add_executable(test-power.bin
        test/seq/test-power.cpp
        )
TARGET_LINK_LIBRARIES(test-power.bin
        ${COMMON_LIB}
        dl
        )
add_custom_command(TARGET test-power.bin POST_BUILD
        COMMAND scp test-power.bin ${arm_target_path}/${CMAKE_BUILD_TYPE}/
        )
# ------------------------------------- #

add_executable(test-sourcesink.bin
        test/seq/test-sourcesink.cpp
        )

# ------------------------------------- #

add_executable(test-source-3out.bin
        test/seq/test-source-3out.cpp
        )

TARGET_LINK_LIBRARIES(test-source-3out.bin
        ${COMMON_LIB}
        dl
        )

add_custom_command(TARGET test-source-3out.bin POST_BUILD
        COMMAND scp test-source-3out.bin ${arm_target_path}/${CMAKE_BUILD_TYPE}/
        )
# ------------------------------------- #

#set_target_properties(seq-joinseq.bin
#        PROPERTIES COMPILE_FLAGS "-DWORKAROUND_JOIN_JDD=1"
#        )

# -- uncomment to turn on the target
#add_executable(test-kaskade test-kaskade.cpp
#Kaskade/utilities/kalloc.cpp
#Kaskade/utilities/threading.cpp
#)

# xzl: tbb will be in the default lib path
#FIND_LIBRARY(tbb NAMES libtbb.so)
#FIND_LIBRARY(tbb NAMES libtbb.so PATHS tbb/lib/intel64/gcc4.7)
#FIND_LIBRARY(tbb_malloc NAMES libtbbmalloc.so PATHS tbb/lib/intel64/gcc4.7)

# x86 libs
#if(CMAKE_ENV STREQUAL "x86")
FIND_LIBRARY(folly NAMES libfolly.a PATHS /shared/lib)
#endif()

#TARGET_LINK_LIBRARIES(test-winsum-long.bin
#        winsum
 #       creek
  #      tbb numa        # still needs this for ARM. compatibility
   #     boost_date_time
    #    boost_system
     #   boost_thread
      #  boost_program_options
      #  ${folly}
      #  )
#add_custom_command(TARGET test-winsum-long.bin POST_BUILD
 #       COMMAND scp test-winsum-long.bin ${arm_target_path}/${CMAKE_BUILD_TYPE}/
  #      )

TARGET_LINK_LIBRARIES(test-whereseq.bin
        ${COMMON_LIB}
        )
add_custom_command(TARGET test-whereseq.bin POST_BUILD
        COMMAND scp test-whereseq.bin ${arm_target_path}/${CMAKE_BUILD_TYPE}/
        )

TARGET_LINK_LIBRARIES(test-sourcesink.bin
        ${COMMON_LIB}
        )
add_custom_command(TARGET test-sourcesink.bin POST_BUILD
        COMMAND scp test-sourcesink.bin ${arm_target_path}/${CMAKE_BUILD_TYPE}/
        )

# ---------- seq test ------------ #


TARGET_LINK_LIBRARIES(test-win-sum.bin
        ${COMMON_LIB}
        dl
        )
add_custom_command(TARGET test-win-sum.bin POST_BUILD
        COMMAND scp test-win-sum.bin ${arm_target_path}/${CMAKE_BUILD_TYPE}/
        )

TARGET_LINK_LIBRARIES(test-wingbk-med.bin
        ${COMMON_LIB}
        dl
        )
add_custom_command(TARGET test-wingbk-med.bin POST_BUILD
        COMMAND scp test-wingbk-med.bin ${arm_target_path}/${CMAKE_BUILD_TYPE}/
#        COMMAND scp test-wingbk-med.bin ${shared_path}
        )


#TARGET_LINK_LIBRARIES(test-wingbk-k2v.bin
#        ${COMMON_LIB}
#        )
#add_custom_command(TARGET test-wingbk-k2v.bin POST_BUILD
#        COMMAND scp test-wingbk-k2v.bin ${arm_target_path}/${CMAKE_BUILD_TYPE}/
#        )


TARGET_LINK_LIBRARIES(test-joinseq.bin
        ${COMMON_LIB}
        dl
        )
add_custom_command(TARGET test-joinseq.bin POST_BUILD
        COMMAND scp test-joinseq.bin ${arm_target_path}/${CMAKE_BUILD_TYPE}/
        )
