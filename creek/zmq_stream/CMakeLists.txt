cmake_minimum_required(VERSION 2.8.9)

project(zmq_stream)

#set(deploy_include_path "/shared/include")

#SET( CMAKE_CXX_COMPILER "clang++" )

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

	SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations" ) # remove warning of auto_ptr

#	set(deploy_target_path "localhost:/tmp/$ENV{USER}")
	set(deploy_target_path $ENV{BOARD}":~/arm")

else() # default x86
	SET( CMAKE_CXX_COMPILER "g++"  )
	SET( CMAKE_C_COMPILER "gcc"  )

	set(deploy_target_path "localhost:/tmp/$ENV{USER}")

endif(CMAKE_ENV STREQUAL "arm" OR CMAKE_ENV STREQUAL "armtz")


SET(CMAKE_CXX_FLAGS_DEBUG
	"${CMAKE_CXX_FLAGS_DEBUG} -std=c++11 -g -O0 -Wall -DHAVE_NUMA -Wfatal-errors")
SET(CMAKE_C_FLAGS_DEBUG
	"${CMAKE_C_FLAGS_DEBUG} -g -O0 -Wall -DHAVE_NUMA -Wfatal-errors")

SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread")

# -- apply to all configurations
#SET( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -std=c++11 -g -fcilkplus -Wall" )
#SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=auto")

#SET( CMAKE_CXX_FLAGS_RELEASE  "${CMAKE_CXX_FLAGS_RELEASE} -std=c++11 -g -fcilkplus -Wall -O2" )
SET(CMAKE_CXX_FLAGS_RELEASE "-std=c++11 -g -Wall -O3 -DHAVE_NUMA")

## toggle this to choose from 32- or 64-bit element
SET(32BIT_ELEMENT "1")

if (32BIT_ELEMENT)
	add_definitions(-DUSE_32BIT_ELEMENT)
endif ()

# add required libraries
# all libraries are in /shared/hikey
INCLUDE_DIRECTORIES(
	./Source
	./Utils
#	/shared/libzmq/include/
	/shared/fast-cpp-csv-parser
	)
#set(shared_path "k2:/tmp/")

file(GLOB HEADERS
		"${CMAKE_CURRENT_SOURCE_DIR}/Source/*.hpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
		options-config.hpp
		)

SET(SOURCE
	./sender.cpp
	./receiver.cpp
	)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_definitions(-DCONFIG_KAGE_GLOBAL_DEBUG_LEVEL=10)
endif ()

if (CMAKE_BUILD_TYPE STREQUAL "Release")
	add_definitions(-DCONFIG_KAGE_GLOBAL_DEBUG_LEVEL=50)
	add_definitions(-DNDEBUG)
endif ()
####################################################################################


#SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/library")
#SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/library")
# for store static library to diff directory
#SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib")



#SET(CMAKE_C_FLAGS	"${CMAKE_C_FLAGS} -finline-functions")

#SET(CMAKE_EXE_LINKER_FLAGS	)

#FIND_LIBRARY(test NAMES libtest.a PATHS .)

#find cppzmq wrapper, installed by make of cppzmq

#######################################
#FIND_LIBRARY(zmq NAMES libzmq.a PATHS /home/heejin/local/lib)
#FIND_LIBRARY(zmq NAMES libzmq.a PATHS /shared/libzmq/lib/)

#if(CMAKE_ENV STREQUAL "armtz")
#	FIND_LIBRARY(zmq NAMES libzmq.a PATHS /shared/hikey-heejin/lib/aarch64-linux-gnu)
#else()
set(zmq zmq)	 # xzl: use OS libzmq
#endif()

########################################
add_executable(sender
		sender.cpp
		${HEADERS}
		)

TARGET_LINK_LIBRARIES(sender
#		boost_program_options
		zmq
		)

if (32BIT_ELEMENT)
	set (target_bin_postfix "-32")
else()
	set (target_bin_postfix "")
endif(32BIT_ELEMENT)

set (target_bin_name "sender")
add_custom_command(TARGET ${target_bin_name} POST_BUILD
		COMMAND scp ${target_bin_name} ${deploy_target_path}/${CMAKE_BUILD_TYPE}/${target_bin_name}${target_bin_postfix}
		COMMAND md5sum ${target_bin_name}
		)
########################################
add_executable(receiver
		receiver.cpp
		)

TARGET_LINK_LIBRARIES(receiver
		zmq
		)

set (target_bin_name "receiver")
add_custom_command(TARGET ${target_bin_name} POST_BUILD
		COMMAND scp ${target_bin_name} ${deploy_target_path}/${CMAKE_BUILD_TYPE}/${target_bin_name}${target_bin_postfix}
		COMMAND md5sum ${target_bin_name}
		)

########################################
# perf
########################################

# xzl: the following programs require recent libzmq (4.4?)
add_executable(local_thr EXCLUDE_FROM_ALL
	./perf/local_thr.cpp
	)

TARGET_LINK_LIBRARIES(local_thr
	zmq
	)

add_executable(remote_thr EXCLUDE_FROM_ALL
	./perf/remote_thr.cpp
	)

TARGET_LINK_LIBRARIES(remote_thr
	zmq
	)
add_executable(inproc_thr EXCLUDE_FROM_ALL
	./perf/inproc_thr.cpp
	)

TARGET_LINK_LIBRARIES(inproc_thr
	zmq
	)

# find cppzmq wrapper, installed by make of cppzmq
#find_package(cppzmq)
#if(cppzmq_FOUND)
#	include_directories(${cppzmq_INCLUDE_DIR})
#	target_link_libraries(sender ${cppzmq_LIBRARY})
#	target_link_libraries(receiver ${cppzmq_LIBRARY})
#endif()
