cmake_minimum_required(VERSION 2.8.9)

project(xplane_ubench)

# add required libraries
# all libraries are in /shared/hikey

SET(XPLANE_LIB "xplane")

INCLUDE_DIRECTORIES(
	../include
	../Sort-Merge/AVX-512-Sort-Merge
	)

LINK_DIRECTORIES(
	../lib
	)

SET( SOURCE
../sort.c
)

SET(CMAKE_SYSTEM_NAME Linux)
SET(CMAKE_SYSTEM_PROCESSOR aarch64)


######################## arm 32 bit #############################
#SET(COMPILER_PATH "/home/heejin/toolchains/aarch32/bin")
#SET(TARGET_ABI "linux-gnueabihf")
## specify the cross compiler
#SET(CMAKE_C_COMPILER "${COMPILER_PATH}/arm-${TARGET_ABI}-gcc")
##################################################################


if(CMAKE_ENV STREQUAL "arm")
	# add required libraries

	SET(CMAKE_SYSTEM_NAME Linux)
	SET(CMAKE_SYSTEM_PROCESSOR aarch64)

	SET(COMPILER_PATH "/home/heejin/toolchains/aarch64/bin")
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

	SET(CMAKE_C_FLAGS	"${CMAKE_C_FLAGS} -O3 -std=gnu99 -Wall")
	SET(CMAKE_C_FLAGS	"${CMAKE_C_FLAGS} -DCONFIG_KAGE_GLOBAL_DEBUG_LEVEL=30")
	#SET(CMAKE_C_FLAGS	"${CMAKE_C_FLAGS} -fopenmp -pthread")	# set openmp
	# SET(CMAKE_EXE_LINKER_FLAGS	"-fopenmp")
	SET(CMAKE_C_FLAGS	"${CMAKE_C_FLAGS} -march=armv8-a -mtune=cortex-a53 -ftree-vectorize -fopenmp -ffast-math")

	#	elseif(CMAKE_ENV STREQUAL "x86")
else()    # default: x86
	#SET( CMAKE_CXX_COMPILER "clang++" )  # has some issues with template template class...
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
		"${CMAKE_C_FLAGS_DEBUG} -std=c++11 -g -O0 -Wall -DHAVE_NUMA -Wfatal-errors -pthread")

	#SET( CMAKE_CXX_FLAGS_RELEASE  "${CMAKE_CXX_FLAGS_RELEASE} -std=c++11 -g -fcilkplus -Wall -O2" )
	SET(CMAKE_CXX_FLAGS_RELEASE "-std=c++11 -g -Wall -O3 -DHAVE_NUMA")

	SET(CMAKE_CXX_FLAGS_MINSIZEREL
		"${CMAKE_CXX_FLAGS} -std=c++11 -g -fcilkplus -Wall")

endif(CMAKE_ENV STREQUAL "arm")

#FIND_LIBRARY(test NAMES libtest.a PATHS .)

########################################
add_executable(sort.bin
	sort.c
	)

TARGET_LINK_LIBRARIES(sort.bin
	${XPLANE_LIB}
#	${TEEC_LIB}
	)
########################################
add_executable(sumcount.bin
	sumcount.c
	)

TARGET_LINK_LIBRARIES(sumcount.bin
	${XPLANE_LIB}
#	${TEEC_LIB}
	)
########################################
add_executable(median.bin
	median.c
	)
TARGET_LINK_LIBRARIES(median.bin
	${XPLANE_LIB}
#	${TEEC_LIB}
	)
########################################
add_executable(join.bin
	join.c
	)
TARGET_LINK_LIBRARIES(join.bin
	${XPLANE_LIB}
#	${TEEC_LIB}
	)
########################################
add_executable(topk.bin
	topk.c
	)
TARGET_LINK_LIBRARIES(topk.bin
	${XPLANE_LIB}
#	${TEEC_LIB}
	)
########################################
add_executable(filter.bin
	filter.c
	)
TARGET_LINK_LIBRARIES(filter.bin
	${XPLANE_LIB}
#	${TEEC_LIB}
	)
########################################
#add_executable(test.bin
#	test.c
#	)
#TARGET_LINK_LIBRARIES(test.bin
#	${XPLANE_LIB}
##	${TEEC_LIB}
#	)
