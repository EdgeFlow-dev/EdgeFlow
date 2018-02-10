cmake_minimum_required(VERSION 2.8.9)

project(x_interface)

set(deploy_include_path "/shared/include")

if(CMAKE_ENV STREQUAL "arm")
	# add required libraries

	SET(CMAKE_SYSTEM_NAME Linux)
	SET(CMAKE_SYSTEM_PROCESSOR aarch64)

	#	SET(COMPILER_PATH "/home/heejin/toolchains/aarch64/bin")
	SET(COMPILER_PATH "/shared/hikey/toolchains/aarch64/bin")
	SET(TARGET_ABI "linux-gnu")
	# specify the cross compiler
	######################## arm 64 bit #############################
	SET(CMAKE_C_COMPILER "${COMPILER_PATH}/aarch64-${TARGET_ABI}-gcc")
	SET(CMAKE_CXX_COMPILER "${COMPILER_PATH}/aarch64-${TARGET_ABI}-g++")
	##################################################################

	SET(CMAKE_FIND_ROOT_PATH)
	# search for programs in the build host directories
	SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
	# for libraries and headers in the target directories
	SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
	SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
	SET(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

	#	SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations" ) # remove warning of auto_ptr

	SET(CMAKE_C_FLAGS	"${CMAKE_C_FLAGS} -std=gnu99 -Wall -Wfatal-errors")
	SET(CMAKE_C_FLAGS	"${CMAKE_C_FLAGS} -march=armv8-a -mtune=cortex-a53 -ftree-vectorize -fopenmp -ffast-math")

	SET(CMAKE_C_FLAGS_DEBUG
			"${CMAKE_C_FLAGS_DEBUG} -g -O0")
	SET(CMAKE_C_FLAGS_RELEASE
			"${CMAKE_C_FLAGS_RELEASE} -g -O3")  # xzl: -g helps profiling

	SET(CMAKE_CXX_FLAGS_DEBUG
			"${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
	SET(CMAKE_CXX_FLAGS_RELEASE
			"${CMAKE_CXX_FLAGS_RELEASE} -g -O3") # xzl: -g helps profiling

	LINK_DIRECTORIES(
			/home/heejin/toolchains/aarch64/lib
			#	/shared/hikey/lib
			#	/shared/hikey/lib/aarch64-linux-gnu
	)

	set(deploy_target_path "/shared/arm")

elseif(CMAKE_ENV STREQUAL "knl")
	SET( CMAKE_CXX_COMPILER "g++"  )
	SET( CMAKE_C_COMPILER "gcc"  )
	#SET( CMAKE_CXX_COMPILER "icc" )

	#############################	AVX 512		#########################################
	# -- apply to all configurations
	#SET( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -std=c++11 -g -fcilkplus -Wall" )
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=auto")

	# for AVX #
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx512f -mavx512pf -mavx512er -mavx512cd")
	SET(CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} -mavx512f -mavx512pf -mavx512er -mavx512cd")

	SET(CMAKE_CXX_FLAGS_DEBUG
			"${CMAKE_CXX_FLAGS_DEBUG} -std=c++11 -g -O0 -Wall -DHAVE_NUMA -Wfatal-errors")
	SET(CMAKE_C_FLAGS_DEBUG
			"${CMAKE_C_FLAGS_DEBUG} -g -O0 -Wall -DHAVE_NUMA -Wfatal-errors")

	#SET( CMAKE_CXX_FLAGS_RELEASE  "${CMAKE_CXX_FLAGS_RELEASE} -std=c++11 -g -fcilkplus -Wall -O2" )
	SET(CMAKE_CXX_FLAGS_RELEASE "-std=c++11 -g -Wall -O3 -DHAVE_NUMA")
	SET(CMAKE_C_FLAGS_RELEASE "-g -O3 -Wall -DHAVE_NUMA -Wfatal-errors")

	#SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_MEM_POOL")
	#SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUSE_MEM_POOL")
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DKNL")
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DKNL")
	
	set(deploy_target_path "/shared/knl")

elseif(CMAKE_ENV STREQUAL "avx2")
	SET( CMAKE_CXX_COMPILER "g++-5"  )
	SET( CMAKE_C_COMPILER "gcc-5"  )
	#SET( CMAKE_CXX_COMPILER "icc" )

	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=auto")

	# for AVX #
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2")
	SET(CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} -mavx2")

	SET(CMAKE_CXX_FLAGS_DEBUG
			"${CMAKE_CXX_FLAGS_DEBUG} -std=c++11 -g -O0 -Wall -DHAVE_NUMA -Wfatal-errors")
	SET(CMAKE_C_FLAGS_DEBUG
			"${CMAKE_C_FLAGS_DEBUG} -std=gnu11 -g -O0 -Wall -DHAVE_NUMA -Wfatal-errors")

	#SET( CMAKE_CXX_FLAGS_RELEASE  "${CMAKE_CXX_FLAGS_RELEASE} -std=c++11 -g -fcilkplus -Wall -O2" )
	SET(CMAKE_CXX_FLAGS_RELEASE "-std=c++11 -g -Wall -O3 -DHAVE_NUMA")
	SET(CMAKE_C_FLAGS_RELEASE
			"${CMAKE_C_FLAGS_RELEASE} -std=gnu11 -g -O3 -Wall -Wfatal-errors") # -g helps profiling

    set(deploy_target_path "/shared/avx2")

	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_MEM_POOL")
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUSE_MEM_POOL")

	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DARCH_AVX2")
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DARCH_AVX2")

else()    # default: x86 -- TODO change this
	#SET( CMAKE_CXX_COMPILER "clang++" )  # has some issues with template template class...
	#SET( CMAKE_CXX_COMPILER "icc" )
	SET( CMAKE_CXX_COMPILER "g++"  )
	SET( CMAKE_C_COMPILER "gcc"  )

	SET(CMAKE_CXX_FLAGS_DEBUG
		"${CMAKE_CXX_FLAGS_DEBUG} -std=c++11 -g -O0 -Wall -DHAVE_NUMA -Wfatal-errors")
	SET(CMAKE_C_FLAGS_DEBUG
		"${CMAKE_C_FLAGS_DEBUG} -g -O0 -Wall -DHAVE_NUMA -Wfatal-errors")

	# -- apply to all configurations
	#SET( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -std=c++11 -g -fcilkplus -Wall" )
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=auto")

	#SET( CMAKE_CXX_FLAGS_RELEASE  "${CMAKE_CXX_FLAGS_RELEASE} -std=c++11 -g -fcilkplus -Wall -O2" )
	SET(CMAKE_CXX_FLAGS_RELEASE "-std=c++11 -g -Wall -O3 -DHAVE_NUMA")
	set(deploy_target_path "/shared/x86")

endif(CMAKE_ENV STREQUAL "arm")

# in the code we want to reflect the arch #
SET(CMAKE_C_FLAGS	"${CMAKE_C_FLAGS} -DXPLANE_LIB_ARCH=${CMAKE_ENV}")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DXPLANE_LIB_ARCH=${CMAKE_ENV}")

# add required libraries
# all libraries are in /shared/hikey
INCLUDE_DIRECTORIES(
		${CMAKE_CURRENT_SOURCE_DIR}/include
		${CMAKE_CURRENT_SOURCE_DIR}/include/mm
		${CMAKE_CURRENT_SOURCE_DIR}/include/sort
#	./include/xplane		# xzl: don't do this. include xplane/XXX.h instead
		${CMAKE_CURRENT_SOURCE_DIR}/Sort-Merge/AVX-512-Sort-Merge
	)

SET(SOURCE_COMMON_SORT_MERGE
	Sort-Merge/COMMON-Sort-Merge/common_sort_merge.c
	)

SET(SOURCE_NEON_SORT_MERGE
	Sort-Merge/NEON-Sort-Merge/xplane_sort.c
	Sort-Merge/NEON-Sort-Merge/xplane_sort_core.c
	Sort-Merge/NEON-Sort-Merge/xplane_sort_x1.c
	Sort-Merge/NEON-Sort-Merge/xplane_sort_x3.c
	Sort-Merge/NEON-Sort-Merge/xplane_sort_x4.c
	Sort-Merge/NEON-Sort-Merge/xplane_merge.c
	)


SET(SOURCE_AVX_512_SORT_MERGE
	Sort-Merge/AVX-512-Sort-Merge/xplane_sort_merge.cpp
	Sort-Merge/AVX-512-Sort-Merge/x2_sort_merge.cpp
	Sort-Merge/AVX-512-Sort-Merge/x2_bitonic_sort_merge_core.cpp
	)

SET(SOURCE_AVX2_SORT_MERGE
	Sort-Merge/AVX2-Sort-Merge/avxsort.cpp
	Sort-Merge/AVX2-Sort-Merge/utils.cpp
	Sort-Merge/AVX2-Sort-Merge/avxmerge.cpp
	Sort-Merge/COMMON-Sort-Merge/common_sort_stable.cpp
	Sort-Merge/COMMON-Sort-Merge/common_sort_stable_kp.c
	)

if(CMAKE_ENV STREQUAL "arm")
	SET(SOURCE_SORT_MERGE ${SOURCE_NEON_SORT_MERGE})
elseif(CMAKE_ENV STREQUAL "knl")
	SET(SOURCE_SORT_MERGE ${SOURCE_AVX_512_SORT_MERGE})
elseif(CMAKE_ENV STREQUAL "avx2")
	SET(SOURCE_SORT_MERGE ${SOURCE_AVX2_SORT_MERGE})
else()
	SET(SOURCE_SORT_MERGE ${SOURCE_COMMON_SORT_MERGE}
			Sort-Merge/COMMON-Sort-Merge/common_sort_stable.cpp
			Sort-Merge/COMMON-Sort-Merge/common_sort_stable_kp.c
		)
endif(CMAKE_ENV STREQUAL "arm")

if(CMAKE_ENV STREQUAL "arm")
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DARM_NEON")
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DARM_NEON")
else()
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DX86")
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DX86")
endif()

file (GLOB HEADERS
		include/*.h
		include/mm/*.h
		include/sort/*.h
		include/xplane/*.h
)

SET(SOURCE_XPLANE
	xplane/xplane_interface.c
	xplane/xplane_segment.c
	xplane/xplane_uarray.c
	xplane/xplane_filter.c
	xplane/xplane_join.c
	xplane/xplane_sumcount.c
	xplane/xplane_median.c
	xplane/xplane_misc.c
	xplane/utils.c
	${HEADERS}
	)

	# hp: arm version doesn't support pointer aggregation
if(CMAKE_ENV STREQUAL "avx2" OR CMAKE_ENV STREQUAL "knl")
SET(SOURCE_XPLANE
	${SOURCE_XPLANE}
	xplane/index.c
	xplane/parallel_merge_kp.cpp
	xplane/parallel_agg_kp.cpp
	)
endif(CMAKE_ENV STREQUAL "avx2" OR CMAKE_ENV STREQUAL "knl")

SET(SOURCE
	mm/batch.c
	mm/mempool.cpp
	${SOURCE_XPLANE}
	${SOURCE_SORT_MERGE}

	xplane_lib.cpp
	measure.c
	)

#ADD_SUBDIRECTORY(ubench)

# for dbg
#MESSAGE( AUTHOR_WARNING "----------------> CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE} )


if (CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_definitions(-DCONFIG_KAGE_GLOBAL_DEBUG_LEVEL=50)
	#	add_definitions(-D_GLIBCXX_DEBUG)	   # will fail compilation in folly
endif ()

if (CMAKE_BUILD_TYPE STREQUAL "Release")
	add_definitions(-DCONFIG_KAGE_GLOBAL_DEBUG_LEVEL=50)
	add_definitions(-DNDEBUG)
endif ()
####################################################################################


#SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/library")
#SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/library")

# for store static library to diff directory -- xzl: why do we do this?
#SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib")



#SET(CMAKE_C_FLAGS	"${CMAKE_C_FLAGS} -finline-functions")

#SET(CMAKE_EXE_LINKER_FLAGS	)

#FIND_LIBRARY(test NAMES libtest.a PATHS .)

ADD_LIBRARY(xplane STATIC
	${SOURCE}
	)

#add_custom_command(TARGET xplane POST_BUILD
#		COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/lib/libxplane.a ${deploy_target_path}/lib/libxplane-${CMAKE_BUILD_TYPE}.a
#		COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/include/xplane_lib.h ${deploy_include_path}
#		COMMAND cp -r ${CMAKE_CURRENT_SOURCE_DIR}/include/xplane/* ${deploy_include_path}/xplane/
#		)
#
#if(CMAKE_ENV STREQUAL "knl")
#	add_custom_command(TARGET xplane POST_BUILD
#		COMMAND scp ${CMAKE_CURRENT_SOURCE_DIR}/lib/libxplane.a knl:${deploy_target_path}/lib/libxplane-${CMAKE_BUILD_TYPE}.a
#		COMMAND scp ${CMAKE_CURRENT_SOURCE_DIR}/include/xplane_lib.h knl:${deploy_include_path}
#		COMMAND scp -r ${CMAKE_CURRENT_SOURCE_DIR}/include/xplane/* knl:${deploy_include_path}/xplane/
#		)
#endif ()

#add_executable(neon.bin
#	${SOURCE}
#	main.c
#	)
#
#TARGET_LINK_LIBRARIES(neon.bin
#	#	${test}
#	)

########################################
if(CMAKE_ENV STREQUAL "knl")
	add_executable(merge_parallel_kp.bin EXCLUDE_FROM_ALL
		ubench/merge_parallel_kp.c
		)

	TARGET_LINK_LIBRARIES(merge_parallel_kp.bin
		xplane
		pthread
		)
########################################
	add_executable(merge_parallel.bin EXCLUDE_FROM_ALL
		ubench/merge_parallel.c
		)

	TARGET_LINK_LIBRARIES(merge_parallel.bin
		xplane
		pthread
		)
########################################
	add_executable(knl_sort_merge_kp.bin EXCLUDE_FROM_ALL
		ubench/knl_sort_merge_kp.c
		)

	TARGET_LINK_LIBRARIES(knl_sort_merge_kp.bin
		xplane
		pthread
		)
########################################
	add_executable(test_expand.bin EXCLUDE_FROM_ALL
		ubench/test_expand.c
		)

	TARGET_LINK_LIBRARIES(test_expand.bin
		xplane
		pthread
		)
########################################
	add_executable(test_parallel_agg_kp.bin EXCLUDE_FROM_ALL
		ubench/test_parallel_agg_kp.c
		)

	TARGET_LINK_LIBRARIES(test_parallel_agg_kp.bin
		xplane
		pthread
		)
endif ()
########################################
add_executable(sort.bin EXCLUDE_FROM_ALL
		ubench/sort.c
		)

TARGET_LINK_LIBRARIES(sort.bin
		xplane
		pthread
		)
########################################
add_executable(merge.bin EXCLUDE_FROM_ALL
		ubench/merge.c
		)

TARGET_LINK_LIBRARIES(merge.bin
		xplane
		pthread
		)
########################################
add_executable(sumcount.bin EXCLUDE_FROM_ALL
		ubench/sumcount.c
		)
TARGET_LINK_LIBRARIES(sumcount.bin
		xplane
		pthread
		)
########################################
add_executable(median.bin EXCLUDE_FROM_ALL
		ubench/median.c
		)
TARGET_LINK_LIBRARIES(median.bin
		xplane
		pthread
		)
########################################
add_executable(join.bin EXCLUDE_FROM_ALL
		ubench/join.c
		)
TARGET_LINK_LIBRARIES(join.bin
		xplane
		pthread
		)
########################################
add_executable(topk.bin EXCLUDE_FROM_ALL
		ubench/topk.c
		)
TARGET_LINK_LIBRARIES(topk.bin
		xplane
		pthread
		)
########################################
add_executable(filter.bin EXCLUDE_FROM_ALL
		ubench/filter.c
		)
TARGET_LINK_LIBRARIES(filter.bin
		xplane
		pthread
		)

if(CMAKE_ENV STREQUAL "avx2")
	########################################
	add_executable(sort_kp.bin EXCLUDE_FROM_ALL
		ubench/sort_kp.c
		)
	TARGET_LINK_LIBRARIES(sort_kp.bin
		xplane
		pthread
		)
	########################################
	add_executable(merge_kp.bin EXCLUDE_FROM_ALL
		ubench/merge_kp.c
		)
	TARGET_LINK_LIBRARIES(merge_kp.bin
		xplane
		pthread
		)
	########################################
	add_executable(join_kp.bin EXCLUDE_FROM_ALL
		ubench/join_kp.c
		)
	TARGET_LINK_LIBRARIES(join_kp.bin
		xplane
		pthread
		)
	add_executable(test_parallel_agg_kp.bin EXCLUDE_FROM_ALL
		ubench/test_parallel_agg_kp.c
		)

	TARGET_LINK_LIBRARIES(test_parallel_agg_kp.bin
		xplane
		pthread
		)

	add_executable(test_stable_sort_kp.bin EXCLUDE_FROM_ALL
		ubench/test_stable_sort_kp.c
		)

	TARGET_LINK_LIBRARIES(test_stable_sort_kp.bin
		xplane
		pthread
		)

	add_executable(test_segment_kp.bin EXCLUDE_FROM_ALL
		ubench/test_segment_kp.c
		)

	TARGET_LINK_LIBRARIES(test_segment_kp.bin
		xplane
		pthread
		)
endif ()
