cmake_minimum_required(VERSION 2.8.9)

project(xplane_ubench)

# add required libraries
# all libraries are in /shared/hikey

SET(XPLANE_LIB "xplane")
SET(TEEC_LIB "teec")


INCLUDE_DIRECTORIES(
	../host/include
	../../optee_client/public
	)

LINK_DIRECTORIES(
	#	/home/heejin/toolchains/aarch64/lib
	../host
	../../optee_client/out/libteec
	)

SET( SOURCE
)

SET(CMAKE_SYSTEM_NAME Linux)
SET(CMAKE_SYSTEM_PROCESSOR aarch64)


######################## arm 32 bit #############################
#SET(COMPILER_PATH "/home/heejin/toolchains/aarch32/bin")
#SET(TARGET_ABI "linux-gnueabihf")
## specify the cross compiler
#SET(CMAKE_C_COMPILER "${COMPILER_PATH}/arm-${TARGET_ABI}-gcc")
##################################################################


######################## arm 64 bit #############################
#SET(COMPILER_PATH "/home/heejin/toolchains/aarch64/bin")
SET(COMPILER_PATH "../../../toolchains/aarch64/bin")
SET(TARGET_ABI "linux-gnu")
# specify the cross compiler
SET(CMAKE_C_COMPILER "${COMPILER_PATH}/aarch64-${TARGET_ABI}-gcc")
##################################################################

SET(CMAKE_FIND_ROOT_PATH)
# search for programs in the build host directories
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

#SET(CMAKE_C_FLAGS	"${CMAKE_C_FLAGS} -Ofast -std=gnu99 -Wall")
SET(CMAKE_C_FLAGS	"${CMAKE_C_FLAGS} -O3 -std=gnu99 -Wall")
SET(CMAKE_C_FLAGS	"${CMAKE_C_FLAGS} -g -pthread")
# SET(CMAKE_EXE_LINKER_FLAGS	"-fopenmp")
SET(CMAKE_C_FLAGS	"${CMAKE_C_FLAGS} -march=armv8-a -mtune=cortex-a53 -ftree-vectorize -ffast-math")

### for cortex a7
#SET(CMAKE_C_FLAGS	"${CMAKE_C_FLAGS} -mcpu=cortex-a7 -mfpu=neon-vfpv4 -ftree-vectorize -ffast-math -funsafe-math-optimizations")
#SET(CMAKE_C_FLAGS	"${CMAKE_C_FLAGS} -march=armv7-a -mfpu=neon -ftree-vectorize -ffast-math -funsafe-math-optimizations")

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -finline-functions")

SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -std=c++11 -g -O0 -Wall -Wfatal-errors")
SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS} -g -O0 -Wall -Wfatal-errors")


SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -std=c++11 -g -Wall -O3 -DHAVE_NUMA")
SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS} -g -O3 -DHAVE_NUMA")


#SET(CMAKE_EXE_LINKER_FLAGS	)

#FIND_LIBRARY(test NAMES libtest.a PATHS .)

add_executable(sort.bin	EXCLUDE_FROM_ALL
	sort.c
	)

TARGET_LINK_LIBRARIES(sort.bin
	${XPLANE_LIB} ${TEEC_LIB}
	)

add_executable(merge.bin
	merge.c
	${SOURCE}
	)

TARGET_LINK_LIBRARIES(merge.bin
	${XPLANE_LIB} ${TEEC_LIB}
	)

add_executable(sumcount.bin
	sumcount.c
	)

TARGET_LINK_LIBRARIES(sumcount.bin
	${XPLANE_LIB} ${TEEC_LIB}
	)

add_executable(median.bin
	median.c
	)
TARGET_LINK_LIBRARIES(median.bin
	${XPLANE_LIB} ${TEEC_LIB}
	)

add_executable(join.bin
	join.c
	)
TARGET_LINK_LIBRARIES(join.bin
	${XPLANE_LIB} ${TEEC_LIB}
	)

add_executable(topk.bin
	topk.c
	)
TARGET_LINK_LIBRARIES(topk.bin
	${XPLANE_LIB} ${TEEC_LIB}
	)
add_executable(filter.bin
	filter.c
	)
TARGET_LINK_LIBRARIES(filter.bin
	${XPLANE_LIB} ${TEEC_LIB}
	)
add_executable(test.bin	EXCLUDE_FROM_ALL
	test.c
	)
TARGET_LINK_LIBRARIES(test.bin
	${XPLANE_LIB} ${TEEC_LIB}
	)
