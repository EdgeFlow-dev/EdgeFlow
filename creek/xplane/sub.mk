sta-dir = $(arch-dir)/sta
xplane-dir = $(arch-dir)/sta/xplane

X_FLAGS = -std=c99 -march=armv8-a -mtune=cortex-a53 -funsafe-math-optimizations -O3

cflags-y += -Wno-declaration-after-statement
cflags-y += -I$(sta-dir)
cflags-y += -I$(xplane-dir)/include

srcs-$(CFG_TEE_CORE_EMBED_INTERNAL_TESTS) += utils.c
srcs-$(CFG_TEE_CORE_EMBED_INTERNAL_TESTS) += xplane_sort_core.c
srcs-$(CFG_TEE_CORE_EMBED_INTERNAL_TESTS) += xplane_sort_x1.c
srcs-$(CFG_TEE_CORE_EMBED_INTERNAL_TESTS) += xplane_sort_x3.c
srcs-$(CFG_TEE_CORE_EMBED_INTERNAL_TESTS) += xplane_sort_x4.c

srcs-$(CFG_TEE_CORE_EMBED_INTERNAL_TESTS) += xplane_uarray.c
srcs-$(CFG_TEE_CORE_EMBED_INTERNAL_TESTS) += xplane_sort.c
srcs-$(CFG_TEE_CORE_EMBED_INTERNAL_TESTS) += xplane_merge.c
srcs-$(CFG_TEE_CORE_EMBED_INTERNAL_TESTS) += xplane_filter.c
srcs-$(CFG_TEE_CORE_EMBED_INTERNAL_TESTS) += xplane_segment.c
srcs-$(CFG_TEE_CORE_EMBED_INTERNAL_TESTS) += xplane_sumcount.c
srcs-$(CFG_TEE_CORE_EMBED_INTERNAL_TESTS) += xplane_join.c
srcs-$(CFG_TEE_CORE_EMBED_INTERNAL_TESTS) += xplane_median.c
srcs-$(CFG_TEE_CORE_EMBED_INTERNAL_TESTS) += xplane_misc.c

cflags-remove-y += -mgeneral-regs-only
cflags-y += $(X_FLAGS)


ifeq ($(CFG_SE_API),y)
srcs-$(CFG_SE_API_SELF_TEST) += se_api_self_tests.c
cppflags-se_api_self_tests.c-y += -Icore/tee/se
endif

ifeq ($(CFG_WITH_USER_TA),y)
srcs-$(CFG_TEE_FS_KEY_MANAGER_TEST) += tee_fs_key_manager_tests.c
endif
