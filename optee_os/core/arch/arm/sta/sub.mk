sta-dir		= $(arch-dir)/sta
xplane-dir	= $(arch-dir)/sta/xplane

cflags-y += -Wno-declaration-after-statement
#cflags-y += -I$(sta-dir)/include
cflags-y += -I$(xplane-dir)/include

srcs-$(CFG_TEE_CORE_EMBED_INTERNAL_TESTS) += sta_self_tests.c
srcs-$(CFG_TEE_CORE_EMBED_INTERNAL_TESTS) += core_self_tests.c
srcs-$(CFG_TEE_CORE_EMBED_INTERNAL_TESTS) += interrupt_tests.c

srcs-$(CFG_WITH_STATS) += stats.c

#cflags-remove-neon_test.c-y += -mgeneral-regs-only -std=c90 -std=c89
#cflags-remove-worker.c-y += -mgeneral-regs-only -ansi -std=c90 -std=c89
#cflags-remove-neon_sort.c-y += -mgeneral-regs-only -std=c90 -std=c89
#cflags-remove-neon_sort_core.c-y += -mgeneral-regs-only -std=c90 -std=c89
#cflags-remove-neon_merge.c-y += -mgeneral-regs-only -std=c90 -std=c89

#cflags-neon_test.c-y += -std=c99 -Wno-declaration-after-statement -O3
#cflags-operations.c-y += -std=c99 -Wno-declaration-after-statement -O3
#cflags-worker.c-y += -std=c99 -Wno-declaration-after-statement -O3
#cflags-neon_sort.c-y += -std=c99 -Wno-declaration-after-statement -O3
#cflags-neon_sort_core.c-y += -std=c99 -Wno-declaration-after-statement -O3
#cflags-neon_merge.c-y += -std=c99 -Wno-declaration-after-statement -O3


#cflags-remove-./neon_hikey/neon_test.c-y += -mgeneral-regs-only
#cflags-./neon_hikey/neon_common.h-y += -std=gnu99
#cflags-./neon_hikey/neon_common.h-y += -Ineon_hikey

ifeq ($(CFG_SE_API),y)
srcs-$(CFG_SE_API_SELF_TEST) += se_api_self_tests.c
cppflags-se_api_self_tests.c-y += -Icore/tee/se
endif

ifeq ($(CFG_WITH_USER_TA),y)
srcs-$(CFG_TEE_FS_KEY_MANAGER_TEST) += tee_fs_key_manager_tests.c
endif
