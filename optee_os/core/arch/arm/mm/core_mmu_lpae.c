/*
 * Copyright (c) 2015-2016, Linaro Limited
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 2014, ARM Limited and Contributors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * Neither the name of ARM nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific
 * prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <platform_config.h>

#include <arm.h>
#include <assert.h>
#include <compiler.h>
#include <inttypes.h>
#include <kernel/thread.h>
#include <kernel/panic.h>
#include <kernel/misc.h>
#include <mm/core_memprot.h>
#include <mm/pgt_cache.h>
#include <mm/core_memprot.h>
#include <mm/batch.h>
#include <mm/sbuff.h>
#include <string.h>
#include <trace.h>
#include <types_ext.h>
#include <util.h>
#include <stdatomic.h>
#include <kernel/abort.h>
#include <tee_api_types.h>
#include <kernel/tee_time.h>
#include <kernel/spinlock.h>
#include "core_mmu_private.h"
#include "../sta/xplane/include/xfunc.h"
//#define DEBUG_XLAT_TABLE	 1 // xzl: will dump the core's init pgtable.

#ifndef DEBUG_XLAT_TABLE
#define DEBUG_XLAT_TABLE 0
#endif

#if DEBUG_XLAT_TABLE
#define debug_print(...) DMSG_RAW(__VA_ARGS__)
#else
#define debug_print(...) ((void)0)
#endif

/*
 * Miscellaneous MMU related constants
 */

#define INVALID_DESC		0x0
#define BLOCK_DESC		0x1
#define L3_BLOCK_DESC		0x3
#define TABLE_DESC		0x3
#define DESC_ENTRY_TYPE_MASK	0x3

#define HIDDEN_DESC		0x4
#define HIDDEN_DIRTY_DESC	0x8

#define XN			(1ull << 2)
#define PXN			(1ull << 1)
#define CONT_HINT		(1ull << 0)

#define UPPER_ATTRS(x)		(((x) & 0x7) << 52)
#define NON_GLOBAL		(1ull << 9)
#define ACCESS_FLAG		(1ull << 8)
#define NSH			(0x0 << 6)
#define OSH			(0x2 << 6)
#define ISH			(0x3 << 6)

#define AP_RO			(0x1 << 5)
#define AP_RW			(0x0 << 5)
#define AP_UNPRIV		(0x1 << 4)

#define NS				(0x1 << 3)
#define LOWER_ATTRS_SHIFT		2
#define LOWER_ATTRS(x)			(((x) & 0xfff) << LOWER_ATTRS_SHIFT)

#define ATTR_DEVICE_INDEX		0x0
#define ATTR_IWBWA_OWBWA_NTR_INDEX	0x1
#define ATTR_INDEX_MASK			0x7

#define ATTR_DEVICE			(0x4)
#define ATTR_IWBWA_OWBWA_NTR		(0xff)

#define MAIR_ATTR_SET(attr, index)	(((uint64_t)attr) << ((index) << 3))

#define OUTPUT_ADDRESS_MASK	(0x0000FFFFFFFFF000ULL)

/* (internal) physical address size bits in EL3/EL1 */
#define TCR_PS_BITS_4GB		(0x0)
#define TCR_PS_BITS_64GB	(0x1)
#define TCR_PS_BITS_1TB		(0x2)
#define TCR_PS_BITS_4TB		(0x3)
#define TCR_PS_BITS_16TB	(0x4)
#define TCR_PS_BITS_256TB	(0x5)

#define ADDR_MASK_48_TO_63	0xFFFF000000000000ULL
#define ADDR_MASK_44_TO_47	0x0000F00000000000ULL
#define ADDR_MASK_42_TO_43	0x00000C0000000000ULL
#define ADDR_MASK_40_TO_41	0x0000030000000000ULL
#define ADDR_MASK_36_TO_39	0x000000F000000000ULL
#define ADDR_MASK_32_TO_35	0x0000000F00000000ULL

#define UNSET_DESC		((uint64_t)-1)

#define FOUR_KB_SHIFT		12
#define PAGE_SIZE_SHIFT		FOUR_KB_SHIFT
#define PAGE_SIZE		(1 << PAGE_SIZE_SHIFT)
#define PAGE_SIZE_MASK		(PAGE_SIZE - 1)
#define IS_PAGE_ALIGNED(addr)	(((addr) & PAGE_SIZE_MASK) == 0)

#define XLAT_ENTRY_SIZE_SHIFT	3 /* Each MMU table entry is 8 bytes (1 << 3) */
#define XLAT_ENTRY_SIZE		(1 << XLAT_ENTRY_SIZE_SHIFT)

#define XLAT_TABLE_SIZE_SHIFT	PAGE_SIZE_SHIFT
#define XLAT_TABLE_SIZE		(1 << XLAT_TABLE_SIZE_SHIFT)

/* Values for number of entries in each MMU translation table */
#define XLAT_TABLE_ENTRIES_SHIFT (XLAT_TABLE_SIZE_SHIFT - XLAT_ENTRY_SIZE_SHIFT) /* 9 */
#define XLAT_TABLE_ENTRIES	(1 << XLAT_TABLE_ENTRIES_SHIFT)
#define XLAT_TABLE_ENTRIES_MASK	(XLAT_TABLE_ENTRIES - 1)

/* Values to convert a memory address to an index into a translation table */
#define L3_XLAT_ADDRESS_SHIFT	PAGE_SIZE_SHIFT /* 12 */
#define L2_XLAT_ADDRESS_SHIFT	(L3_XLAT_ADDRESS_SHIFT + \
				 XLAT_TABLE_ENTRIES_SHIFT) /* 21 */
#define L1_XLAT_ADDRESS_SHIFT	(L2_XLAT_ADDRESS_SHIFT + \
				 XLAT_TABLE_ENTRIES_SHIFT) /* 30 */



#define ADDR_SPACE_SIZE		(1ull << 39)  /* xzl: why this limitation? */
/* #define ADDR_SPACE_SIZE		(1ull << 48) /\* xzl: boot will fail. may need deeper change *\/ */
#define MAX_MMAP_REGIONS	16
#define NUM_L1_ENTRIES		(ADDR_SPACE_SIZE >> L1_XLAT_ADDRESS_SHIFT)

#ifndef MAX_XLAT_TABLES
#define MAX_XLAT_TABLES		5
#endif

/* MMU L1 table, one for each core */
// xzl: only 4 entries
static uint64_t l1_xlation_table[CFG_TEE_CORE_NB_CORE][NUM_L1_ENTRIES]
	__aligned(NUM_L1_ENTRIES * XLAT_ENTRY_SIZE) __section(".nozi.mmu.l1");

// xzl: a fixed pool used for kernel's mapping (on l2). 512 entries
static uint64_t xlat_tables[MAX_XLAT_TABLES][XLAT_TABLE_ENTRIES]
	__aligned(XLAT_TABLE_SIZE) __section(".nozi.mmu.l2");

/* xzl: why are these in nozi (non zero initd)? shouldn't we clear them? */
static uint64_t xzl_l2_pgtbl[XLAT_TABLE_ENTRIES]
  __aligned(XLAT_TABLE_SIZE) = {0};
//__section(".nozi.mmu.l2");

static uint64_t xzl_l2_pgtbl_high[XLAT_TABLE_ENTRIES]
  __aligned(XLAT_TABLE_SIZE) = {0};

/* MMU L2 table for TAs, one for each thread */
// xzl: this is l2 (one for each thread); l3 is from pgt cache.
static uint64_t xlat_tables_ul1[CFG_NUM_THREADS][XLAT_TABLE_ENTRIES]
	__aligned(XLAT_TABLE_SIZE) __section(".nozi.mmu.l2");

/* xzl: per core pgt_cache */
static struct pgt_cache xzl_pgt_cache[CFG_TEE_CORE_NB_CORE];

static unsigned next_xlat __early_bss;
static uint64_t tcr_ps_bits __early_bss;  // xzl: phys addr bits?? probably related to aarch64 lpae
static int user_va_idx = -1;
#ifdef MEASURE_TIME
uint64_t mm_time = 0;
uint64_t cmp_time = 0;
#endif
static uint32_t desc_to_mattr(unsigned level, uint64_t desc)
{
	uint32_t a;

	if (!(desc & 1)) {
		if (desc & HIDDEN_DESC)
			return TEE_MATTR_HIDDEN_BLOCK;
		if (desc & HIDDEN_DIRTY_DESC)
			return TEE_MATTR_HIDDEN_DIRTY_BLOCK;
		return 0;
	}

	if (level == 3) {
		if ((desc & DESC_ENTRY_TYPE_MASK) != L3_BLOCK_DESC)
			return 0;
	} else {
		if ((desc & DESC_ENTRY_TYPE_MASK) == TABLE_DESC)
			return TEE_MATTR_TABLE;
	}

	a = TEE_MATTR_VALID_BLOCK;

	if (desc & LOWER_ATTRS(ACCESS_FLAG))
		a |= TEE_MATTR_PRX | TEE_MATTR_URX;

	if (!(desc & LOWER_ATTRS(AP_RO)))
		a |= TEE_MATTR_PW | TEE_MATTR_UW;

	if (!(desc & LOWER_ATTRS(AP_UNPRIV)))
		a &= ~TEE_MATTR_URWX;

	if (desc & UPPER_ATTRS(XN))
		a &= ~(TEE_MATTR_PX | TEE_MATTR_UX);

	if (desc & UPPER_ATTRS(PXN))
		a &= ~TEE_MATTR_PX;

	COMPILE_TIME_ASSERT(ATTR_DEVICE_INDEX == TEE_MATTR_CACHE_NONCACHE);
	COMPILE_TIME_ASSERT(ATTR_IWBWA_OWBWA_NTR_INDEX ==
			    TEE_MATTR_CACHE_CACHED);

	a |= ((desc & LOWER_ATTRS(ATTR_INDEX_MASK)) >> LOWER_ATTRS_SHIFT) <<
	     TEE_MATTR_CACHE_SHIFT;

	if (!(desc & LOWER_ATTRS(NON_GLOBAL)))
		a |= TEE_MATTR_GLOBAL;

	if (!(desc & LOWER_ATTRS(NS)))
		a |= TEE_MATTR_SECURE;

	return a;
}

static uint64_t mattr_to_desc(unsigned level, uint32_t attr)
{
	uint64_t desc;
	uint32_t a = attr;

	if (a & TEE_MATTR_HIDDEN_BLOCK)
		return INVALID_DESC | HIDDEN_DESC;

	if (a & TEE_MATTR_HIDDEN_DIRTY_BLOCK)
		return INVALID_DESC | HIDDEN_DIRTY_DESC;

	if (a & TEE_MATTR_TABLE)
		return TABLE_DESC;

	if (!(a & TEE_MATTR_VALID_BLOCK))
		return 0;

	if (a & (TEE_MATTR_PX | TEE_MATTR_PW))
		a |= TEE_MATTR_PR;
	if (a & (TEE_MATTR_UX | TEE_MATTR_UW))
		a |= TEE_MATTR_UR;
	if (a & TEE_MATTR_UR)
		a |= TEE_MATTR_PR;
	if (a & TEE_MATTR_UW)
		a |= TEE_MATTR_PW;

	if (level == 3)
		desc = L3_BLOCK_DESC;
	else
		desc = BLOCK_DESC;

	if (!(a & (TEE_MATTR_PX | TEE_MATTR_UX)))
		desc |= UPPER_ATTRS(XN);
	if (!(a & TEE_MATTR_PX))
		desc |= UPPER_ATTRS(PXN);

	if (a & TEE_MATTR_UR)
		desc |= LOWER_ATTRS(AP_UNPRIV);

	if (!(a & TEE_MATTR_PW))
		desc |= LOWER_ATTRS(AP_RO);

	/* Keep in sync with core_mmu.c:core_mmu_mattr_is_ok */
	switch ((a >> TEE_MATTR_CACHE_SHIFT) & TEE_MATTR_CACHE_MASK) {
	case TEE_MATTR_CACHE_NONCACHE:
		desc |= LOWER_ATTRS(ATTR_DEVICE_INDEX | OSH);
		break;
	case TEE_MATTR_CACHE_CACHED:
		desc |= LOWER_ATTRS(ATTR_IWBWA_OWBWA_NTR_INDEX | ISH);
		break;
	default:
		/*
		 * "Can't happen" the attribute is supposed to be checked
		 * with core_mmu_mattr_is_ok() before.
		 */
		panic();
	}

	if (a & (TEE_MATTR_UR | TEE_MATTR_PR))
		desc |= LOWER_ATTRS(ACCESS_FLAG);

	if (!(a & TEE_MATTR_GLOBAL))
		desc |= LOWER_ATTRS(NON_GLOBAL);

	desc |= a & TEE_MATTR_SECURE ? 0 : LOWER_ATTRS(NS);

	return desc;
}

static uint64_t mmap_desc(uint32_t attr, uint64_t addr_pa,
					unsigned level)
{
	return mattr_to_desc(level, attr) | addr_pa;
}

static int mmap_region_attr(struct tee_mmap_region *mm, uint64_t base_va,
					uint64_t size)
{
	uint32_t attr = mm->attr;

	for (;;) {
		mm++;

		if (!mm->size)
			return attr; /* Reached end of list */

		if (mm->va >= base_va + size)
			return attr; /* Next region is after area so end */

		if (mm->va + mm->size <= base_va)
			continue; /* Next region has already been overtaken */

		if (mm->attr == attr)
			continue; /* Region doesn't override attribs so skip */

		if (mm->va > base_va ||
			mm->va + mm->size < base_va + size)
			return -1; /* Region doesn't fully cover our area */
	}
}

// xzl: this init a whole new table (e.g for the entire core)
// so each region does not necessarily map to an entry (one L1 entry is big)
// it will go down to lower levels (2/3...) as needed
// @base_va == ? (for recursion??)
//
// @ region: the mm region
// @ area: the current pgtable level (from base_va)

static struct tee_mmap_region *init_xlation_table(struct tee_mmap_region *mm,
			uint64_t base_va, uint64_t *table, unsigned level)
{
	unsigned int level_size_shift = L1_XLAT_ADDRESS_SHIFT - (level - 1) *
						XLAT_TABLE_ENTRIES_SHIFT;
	unsigned int level_size = BIT32(level_size_shift);
	uint64_t level_index_mask = SHIFT_U64(XLAT_TABLE_ENTRIES_MASK,
					      level_size_shift);

	assert(level <= 3);

	debug_print("New xlat table (level %u):", level);

	do  {
		uint64_t desc = UNSET_DESC;

		if (mm->va + mm->size <= base_va) {
			/* Area now after the region so skip it (xzl: skip the region) */
			mm++;
			continue;
		}

		// xzl: "Next region" means current region (?) from mm->va
		if (mm->va >= base_va + level_size) {
			/* Next region is after area so nothing to map yet */
			desc = INVALID_DESC; // xzl: also writes invalid entries
			debug_print("%*s%010" PRIx64 " %8x",
					level * 2, "", base_va, level_size);
		} else if (mm->va <= base_va &&
			   mm->va + mm->size >= base_va + level_size &&
			   !(mm->pa & (level_size - 1))) {
			/* Next region covers all of area
			 * xzl: this means that the current pgtable level all belongs to the
			 * mm region.
			 * */
			int attr = mmap_region_attr(mm, base_va, level_size);

			if (attr >= 0) {
				desc = mmap_desc(attr,
						 base_va - mm->va + mm->pa,
						 level);
				debug_print("%*s%010" PRIx64 " %8x %s-%s-%s-%s",
					level * 2, "", base_va, level_size,
					attr & (TEE_MATTR_CACHE_CACHED <<
						TEE_MATTR_CACHE_SHIFT) ?
						"MEM" : "DEV",
					attr & TEE_MATTR_PW ? "RW" : "RO",
					attr & TEE_MATTR_PX ? "X" : "XN",
					attr & TEE_MATTR_SECURE ? "S" : "NS");
			} else {
				debug_print("%*s%010" PRIx64 " %8x",
					level * 2, "", base_va, level_size);
			}
		}
		/* else Next region only partially covers area, so need */

		if (desc == UNSET_DESC) {
			/* Area not covered by a region so need finer table */
			// xzl: meaning that this level is not fully occupied by the region
			// xzl: xlat_tables is pre-allocated as an array
			uint64_t *new_table = xlat_tables[next_xlat++];
			/* Clear table before use */
			if (next_xlat > MAX_XLAT_TABLES)
				panic("running out of xlat tables");
			memset(new_table, 0, XLAT_TABLE_SIZE);

			// xzl: points current entry to the new, next-level table
			desc = TABLE_DESC | virt_to_phys(new_table);

			/* Recurse to fill in new table */
			mm = init_xlation_table(mm, base_va, new_table,
					   level + 1);
		}

		*table++ = desc;
		base_va += level_size;
	} while (mm->size && (base_va & level_index_mask));

	return mm;
}

#if 0
static struct tee_mmap_region *init_xlation_table_xzl(struct tee_mmap_region *mm,
			uint64_t base_va, uint64_t *table, unsigned level)
{
	unsigned int level_size_shift = L1_XLAT_ADDRESS_SHIFT - (level - 1) *
						XLAT_TABLE_ENTRIES_SHIFT;
	unsigned int level_size = BIT32(level_size_shift);
	uint64_t level_index_mask = SHIFT_U64(XLAT_TABLE_ENTRIES_MASK,
					      level_size_shift);

	assert(level <= 3);

	debug_print("New xlat table (level %u):", level);

	do  {
		uint64_t desc = UNSET_DESC;

		if (mm->va + mm->size <= base_va) {
			/* Area now after the region so skip it (xzl: skip the region) */
			mm++;
			continue;
		}

		// xzl: "Next region" means current region (?) from mm->va
		if (mm->va >= base_va + level_size) {
			/* Next region is after area so nothing to map yet.
			 * xzl: we don't write any entries */
			desc = INVALID_DESC;
			debug_print("%*s%010" PRIx64 " %8x",
					level * 2, "", base_va, level_size);
		} else if (mm->va <= base_va &&
			   mm->va + mm->size >= base_va + level_size &&
			   !(mm->pa & (level_size - 1))) {
			/* Next region covers all of area
			 * xzl: this means that the current pgtable level all belongs to the
			 * mm region.
			 * */
			int attr = mmap_region_attr(mm, base_va, level_size);

			if (attr >= 0) {
				desc = mmap_desc(attr,
						 base_va - mm->va + mm->pa,
						 level);
				debug_print("%*s%010" PRIx64 " %8x %s-%s-%s-%s",
					level * 2, "", base_va, level_size,
					attr & (TEE_MATTR_CACHE_CACHED <<
						TEE_MATTR_CACHE_SHIFT) ?
						"MEM" : "DEV",
					attr & TEE_MATTR_PW ? "RW" : "RO",
					attr & TEE_MATTR_PX ? "X" : "XN",
					attr & TEE_MATTR_SECURE ? "S" : "NS");
			} else {
				debug_print("%*s%010" PRIx64 " %8x",
					level * 2, "", base_va, level_size);
			}
		}
		/* else Next region only partially covers area, so need */

		if (desc == UNSET_DESC) {
			/* Area not covered by a region so need finer table */
			// xzl: meaning that this level is not fully occupied by the region
			// xzl: xlat_tables is pre-allocated as an array
			uint64_t *new_table = xlat_tables[next_xlat++];
			/* Clear table before use */
			if (next_xlat > MAX_XLAT_TABLES)
				panic("running out of xlat tables");
			memset(new_table, 0, XLAT_TABLE_SIZE);

			// xzl: points current entry to the new, next-level table
			desc = TABLE_DESC | virt_to_phys(new_table);

			/* Recurse to fill in new table */
			mm = init_xlation_table(mm, base_va, new_table,
					   level + 1);
		}

		*table++ = desc;
		base_va += level_size;
	} while (mm->size && (base_va & level_index_mask));

	return mm;
}
#endif // xzl

static unsigned int calc_physical_addr_size_bits(uint64_t max_addr)
{
	/* Physical address can't exceed 48 bits */
	assert(!(max_addr & ADDR_MASK_48_TO_63));

	/* 48 bits address */
	if (max_addr & ADDR_MASK_44_TO_47)
		return TCR_PS_BITS_256TB;

	/* 44 bits address */
	if (max_addr & ADDR_MASK_42_TO_43)
		return TCR_PS_BITS_16TB;

	/* 42 bits address */
	if (max_addr & ADDR_MASK_40_TO_41)
		return TCR_PS_BITS_4TB;

	/* 40 bits address */
	if (max_addr & ADDR_MASK_36_TO_39)
		return TCR_PS_BITS_1TB;

	/* 36 bits address */
	if (max_addr & ADDR_MASK_32_TO_35)
		return TCR_PS_BITS_64GB;

	return TCR_PS_BITS_4GB;
}

/* xzl: for testing ... */

#define PA_START 0x40000000ull
#define PA_SIZE 0x30000000
#define VA_START 0x6000000000
#define VA_SIZE 0x4000

struct tee_ta_ctx *tee_mmu_get_ctx(void);

struct tee_mmap_region myregions[2] = {
		{
				.type = MEM_AREA_TEE_RAM,
				.region_size = SMALL_PAGE_SIZE,
//				.pa = 0x39000000,		// will alloc from pool
				.va = 0x80000000,
				.size = 0x200000,
				/* attr to be filled later */
		},
		{
				.size = 0
		}
};

/* finer mapping */
struct tee_mmap_region myregions2[2] = {
		{
				.type = MEM_AREA_TEE_RAM,
				.region_size = SMALL_PAGE_SIZE,
				.va = 0x81000000,
				.size = 0x4000,
				/* attr to be filled later */
		},
		{
				.size = 0
		}
};

// shoot down mapping: disconnect a lv3 pgtbl from l2.
// this should be done lazily (for a whole lv3 pgtbl (2M))
static void xzl_drop_lv3pgtbl(struct core_mmu_table_info* tbl_info_lv2,
		unsigned long vabase, unsigned long size)
{
	unsigned idx;

	assert(!(vabase & CORE_MMU_PGDIR_MASK)); // must be 2MB aligned
	assert(!(size & CORE_MMU_PGDIR_MASK));

	idx = core_mmu_va2idx(tbl_info_lv2, vabase);

	core_mmu_set_entry(tbl_info_lv2, idx, 0 /* pa */, 0 /* mattr */);

	/* Invalidate secure TLB */
	core_tlb_maintenance(TLBINV_UNIFIEDTLB, 0);
}

/* for simplicity, we probably don't need this */
#if 0
void xzl_clear_lv3entries(unsigned long vabase, unsigned long size)
{

}
#endif

void xzl_populate_map(void)
{
	int level = 1; /* pgdir level */

#if 0
	struct core_mmu_table_info dir_info = {
			.table = l1_xlation_table[get_core_pos()],
			.va_base = 0,
			.level = 1,
			.shift = L1_XLAT_ADDRESS_SHIFT - (level - 1) * XLAT_TABLE_ENTRIES_SHIFT,
			.num_entries =
	};
#endif
	struct tee_ta_ctx* ctx;
	struct core_mmu_table_info dir_info, pg_info;
	struct pgt_cache *pgt_cache = &thread_get_tsd()->pgt_cache;
	struct pgt *pgt;
	paddr_t pa;
	uint32_t attr;

	tee_mm_pool_t mypool;
	tee_mm_entry_t* pentry;

	// create a pool (for on-demand mapped phys pages)
	tee_mm_init(&mypool, PA_START, PA_START + PA_SIZE, 20 /* shift, 1M */,
			TEE_MM_POOL_NO_FLAGS);

	// alloc from the pool
	pentry = tee_mm_alloc(&mypool, 0x200000);
	assert(pentry);
	myregions[0].pa = tee_mm_get_smem(pentry);
	DMSG("xzl: allocated pa from pool: %lx", myregions[0].pa);

	/* the lv1 pgdir (per core). they've already allocated, but we still cr
	  tbl info for using optee api */
	core_mmu_set_info_table(&dir_info, level,
			0 /* va base */, l1_xlation_table[get_core_pos()]);

	// can be called for static TA?
	ctx = tee_mmu_get_ctx();
//	assert(ctx);
	if (!ctx)
		DMSG("xzl: warning: ctx seems null. proceed...\n");

//	pgt_alloc(pgt_cache, ctx, VA_START, VA_START + VA_SIZE - 1);
	// trick pgt cache to alloc more pgtbls
	pgt_alloc(pgt_cache, ctx, VA_START, VA_START + 0x04000000 - 1);

	pgt = SLIST_FIRST(pgt_cache);
	assert(pgt);

	myregions[0].attr = core_mmu_type_to_attr(myregions[0].type);

	/* l2 pgtbl: init for populating the actual pgtbl */
	core_mmu_set_info_table(&pg_info, dir_info.level + 1, 0 /* to be filled */,
			NULL);

	set_pg_region_wrapper(&dir_info, myregions, &pgt, &pg_info);

	/* check */
	core_mmu_get_entry(&dir_info, 2 /* idx */, &pa, &attr);
	DMSG("xzl:set pg region done. lv1 pgtbl entry 2: pa %lx, attr %x\n",
				pa, attr);

	core_mmu_get_entry(&pg_info, 0 /* idx */, &pa, &attr);
	DMSG("xzl:set pg region done. lv2 pgtbl entry 0: pa %lx, attr %x\n",
			pa, attr);

	/* touch the newly mapped memory */
	memset((void *)VA_START, 0xee, 128);
	DMSG("xzl: touch done. %lx\n", *(unsigned long *)VA_START);


	/* l3 pgtable, 4K mapping */
	{
		tee_mm_entry_t* pentry1 = NULL;
		struct core_mmu_table_info pg_info_lv3;

		pentry1 = tee_mm_alloc(&mypool, 0x4000);
		assert(pentry1);
		myregions2[0].pa = tee_mm_get_smem(pentry1);
		DMSG("xzl: allocated pa from pool: %lx", myregions2[0].pa);

		myregions2[0].attr = core_mmu_type_to_attr(myregions2[0].type);

		/* pgt is auto advanced by prevoius set_pg_region() */
		assert(pgt);

		core_mmu_set_info_table(&pg_info_lv3, 3 /* lv */, 0 /* to be filled */, NULL);
		set_pg_region_wrapper(&pg_info, myregions2, &pgt, &pg_info_lv3);

		/* check pgtbl entry */
		core_mmu_get_entry(&pg_info_lv3, 0 /* idx */, &pa, &attr);
		DMSG("xzl:set pg lv3 region done. entry 0: pa %lx, attr %x\n", pa, attr);

		memset((void *)0x81000000, 0xee, 128);
		DMSG("xzl: touch done. %lx\n", *(unsigned long *)0x81000000);

	}

	/* now free the l3 pgtable */
	{
		int idx;

		xzl_drop_lv3pgtbl(&pg_info, 0x81000000, CORE_MMU_PGDIR_SIZE);

		idx = core_mmu_va2idx(&pg_info, 0x81000000);
		core_mmu_get_entry(&pg_info, idx, &pa, &attr);

		DMSG("xzl:l3 pgtable cleared from l2. l2 entry %d: pa %lx, attr %x\n",
				idx, pa, attr);

		// the following shoudl trigger xslat fault.
//		DMSG("xzl: now touch...\n");
//		memset((void *)0x81000000, 0xee, 128);
	}
}

struct tee_mmap_region mm_sz[2] = {
    {
        .type = MEM_AREA_TEE_RAM,
        .region_size = SMALL_PAGE_SIZE,
        .pa = PA_START,		// will alloc from pool
        .va = VA_START,
        .size = PA_SIZE,
        /* attr to be filled later */
    },
    {
        .size = 0
    }
};

void xzl_populate_map2(void)
{
//	struct pgt_cache *pgt_cache = &thread_get_tsd()->pgt_cache;
	struct pgt_cache *pgt_cache =  NULL;
	struct tee_ta_ctx* ctx;
	struct pgt *pgt;

#if 0
    volatile unsigned int ret;
    struct batch_t b;
    const uint64_t array[] = {1, 12, 32, 43, 54};

    TEE_Time start, end;
    TEE_Result res;
#endif
	pgt_cache = xzl_pgt_cache + get_core_pos();

        DMSG("pgt_cache = %p\n", (void *)pgt_cache);
	ctx = tee_mmu_get_ctx();
//	assert(ctx);
	if (!ctx)
		DMSG("xzl: warning: ctx seems null. proceed...\n");
	/* get 1x pgtbl */
	pgt_alloc(pgt_cache, ctx, 0x80000000,  0x80000000 + 4 * CORE_MMU_PGDIR_SIZE - 1);

	pgt = SLIST_FIRST(pgt_cache);
	assert(pgt);

	xzl_set_l2_pgtbl();  /* only do this once */

        mm_sz[0].attr = core_mmu_type_to_attr(mm_sz[0].type);
        init_xlation_table(mm_sz, VA_START, xzl_l2_pgtbl_high, 2);

#if 0
	xzl_set_l3_pgtbl(0x39000000 /* pa */, 0x80000000 /* va */,
			0x1000 /* size */, &pgt);

	/* a second mapping okay? */
	xzl_set_l3_pgtbl(0x39001000 /* pa */, 0x80001000 /* va */,
				0x1000 /* size */, &pgt);

	/* (remapping w/o shootdown the previous one)  */
	xzl_set_l3_pgtbl(0x39000000 /* pa */, 0x80002000 /* va */,
					0x1000 /* size */, &pgt);

	/* on demand paging */
	memset((void *)0x80003000, 0, 128);

        test_get_time_cost();
        res = tee_time_get_ree_time(&start);
        if(res != TEE_SUCCESS)
            return;
        batch_init(&b, (uint64_t*)0x6000400000);
        ret = batch_append_array(&b, array, 5);
        /* DMSG("append ret = %d\n", ret); */
        ret = batch_processed(&b);
        res = tee_time_get_ree_time(&end);
        if(end.micros < start.micros)
            DMSG("batch with page fault takes %d.%06d secs.\n", end.seconds - start.seconds - 1,
                 1000000 - (start.micros - end.micros));
        else
            DMSG("batch with page fault takes %d.%06d secs.\n", end.seconds - start.seconds,
             end.micros - start.micros);
        DMSG("process ret = %d\n", ret);
        DMSG("sz: return = %ld.\n", batch_read(&b, 2));
        DMSG("sz: return = %ld.\n", batch_read(&b, 10));
#endif
        
	DMSG("finished\n");
}
void test_get_time_cost(void)
{
    TEE_Time start, end;
    TEE_Result res;

    res = tee_time_get_ree_time(&start);
    if (res != TEE_SUCCESS) {
        EMSG("Cannot get time.\n");
        return;
    }

    res = tee_time_get_ree_time(&end);
    if (res != TEE_SUCCESS) {
        EMSG("Cannot get time.\n");
        return;
    }
    
    if(end.micros < start.micros)
        DMSG("get_time takes %d.%06d secs.\n", end.seconds - start.seconds - 1,
             1000000 - (start.micros - end.micros));
    else
        DMSG("get_time takes %d.%06d secs.\n", end.seconds - start.seconds,
             end.micros - start.micros);

}

void my_map_one_region(void)
{
	myregions[0].attr = core_mmu_type_to_attr(myregions[0].type);

	DMSG("xzl: installing custom pgtable...\n");

	init_xlation_table(myregions,
			0x80000000 /* base_va */,
			l1_xlation_table[get_core_pos()], /* this core */
			1 /* level */ );
}

/* link the lv2 pgtbl in lv1.
 * shared among all cores. statically allocated. */
static struct core_mmu_table_info lv2_tblinfo;
static struct core_mmu_table_info lv2_tblinfo_high;

void xzl_set_l2_pgtbl(void)
{
	struct core_mmu_table_info dir_info;
//	lv2_tblinfo;
//	struct tee_ta_ctx* ctx;
	int idx;
	int i;

	/* the lv1 pgdir (per core). they've already allocated, but we still cr
	  a tblinfo for using optee api */
	core_mmu_set_info_table(&dir_info, 1,
			0 /* va base */, l1_xlation_table[get_core_pos()]);

	idx = core_mmu_va2idx(&dir_info, 0x80000000);
	for (i = 0; i < CFG_TEE_CORE_NB_CORE; i++) {
		dir_info.table = l1_xlation_table + i;
		core_mmu_set_entry(&dir_info, idx, virt_to_phys(xzl_l2_pgtbl),
				TEE_MATTR_SECURE | TEE_MATTR_TABLE);
	}

	/* fill in lv2 tbl info */
	core_mmu_set_info_table(&lv2_tblinfo, 2, 0x80000000 /* va base */,
			xzl_l2_pgtbl);

        /* sz: try another large va */
        idx = core_mmu_va2idx(&dir_info, 0x6000000000);
        DMSG("sz: set high entry idx=%d num_entries=%d\n", idx, dir_info.num_entries);
	for (i = 0; i < CFG_TEE_CORE_NB_CORE; i++) {
		dir_info.table = l1_xlation_table + i;
		core_mmu_set_entry(&dir_info, idx, virt_to_phys(xzl_l2_pgtbl_high),
				TEE_MATTR_SECURE | TEE_MATTR_TABLE);
	}

	core_mmu_set_info_table(&lv2_tblinfo_high, 2, 0x6000000000 /* va base */,
			xzl_l2_pgtbl_high);

	core_tlb_maintenance(TLBINV_UNIFIEDTLB, 0); // shall we wait?

//	ctx = tee_mmu_get_ctx();
//	if (!ctx)
//		DMSG("xzl: warning: ctx seems null. proceed...\n");
}

/* return: if okay.
 * @pgt: pgt cache owned by the caller. we may or may not allocate pgt */
bool xzl_set_l3_pgtbl(paddr_t pa, unsigned long va, unsigned long size, struct pgt **ppgt)
{
	struct core_mmu_table_info pg_info_lv3;
        struct core_mmu_table_info *lv2_tbl;
	uint32_t attr;
	int idx;
	void *pgtbl_lv3 = NULL;

	struct tee_mmap_region r[2] = {
			{
				.type = MEM_AREA_TEE_RAM,
				.region_size = SMALL_PAGE_SIZE,
				.pa = pa,
				.va = va,
				.size = size
				/* attr will be filled in later */
			},
			{
				.size = 0
			}
	};

	r[0].attr = core_mmu_type_to_attr(r[0].type);

        if (va < 0xc0000000) {
            lv2_tbl = &lv2_tblinfo;
            DMSG("sz: use low mem\n");
        }
        else {
            lv2_tbl = &lv2_tblinfo_high;
            /* DMSG("sz: use high mem, va %lx, pa %lx\n", va, pa); */
        }
	assert(lv2_tbl->table);

	/* chk lv2 entry: does the lv3 pgtbl exist already? */
	idx = core_mmu_va2idx(lv2_tbl, va);
	core_mmu_get_entry(lv2_tbl, idx, &pa, &attr);
	if (pa) { /* entry already exists in l2 */
		DMSG("xzl: l2 entry exists. pa %lx old attr %x \n", pa, attr);
		assert(attr & TEE_MATTR_TABLE);  /* the l2 entry points to a tbl! */
		pgtbl_lv3 = phys_to_virt(pa, MEM_AREA_TEE_RAM);
	} /* else pgtbl_lv3 == NULL, new pgt will be allocated */

	assert(*ppgt);
	core_mmu_set_info_table(&pg_info_lv3, 3 /* lv */,
			core_mmu_idx2va(lv2_tbl, idx) /* this lv3 pgtbl's vabase */,
			pgtbl_lv3);

	set_pg_region_wrapper(lv2_tbl, r, ppgt, &pg_info_lv3);

	/* check pgtbl entry */
	idx = core_mmu_va2idx(&pg_info_lv3, va);
	core_mmu_get_entry(&pg_info_lv3, idx, &pa, &attr);
	/* DMSG("xzl:set pg lv3 region done. entry %d: pa %lx, attr %x\n", idx, pa, attr); */

	/* memset((void *)va, 0xee, 128); */
	/* DMSG("xzl: touch done. %lx\n", *(unsigned long *)va); */

	return true;
}

/* cf: tee_pager_handle_fault() */
bool xzl_handle_fault(struct abort_info *ai)
{
	vaddr_t page_va = ai->va & ~SMALL_PAGE_MASK;
	struct pgt *pgt;
	struct pgt_cache *pgt_cache;

	// dbg
	abort_print(ai);

	if (abort_is_user_exception(ai))
		panic("not implemented");

	if (page_va == 0x80003000) {
		pgt_cache = xzl_pgt_cache + get_core_pos();
 		pgt = SLIST_FIRST(pgt_cache);
		assert(pgt);
		xzl_set_l3_pgtbl(0x39003000, page_va, 0x1000 /* size */, &pgt);
	} else if (page_va >= 0x6000000000 && page_va < 0x6020000000) {
		pgt_cache = xzl_pgt_cache + get_core_pos();
		pgt = SLIST_FIRST(pgt_cache);
		assert(pgt);
		xzl_set_l3_pgtbl(0x39003000 + (page_va - 0x6000400000), page_va, 0x1000 /* size */, &pgt);            
	}
	else
		panic("don't know how to handle");

	return true;
}

/* ---------- end xzl ---------------  */
/* sz: new chapter */

/* #define SZ_DEBUG_PRINT 1 */
/* #define NB_PAGE_LOCAL_CACHE LOCAL_CACHE_SIZE >> CORE_MMU_PGDIR_SHIFT */
#define SIZE_BIT_SHIFT CORE_MMU_PGDIR_SHIFT /* 2MB */
#define NB_INT32_BMAP (PA_SIZE >> SIZE_BIT_SHIFT >> 5)
/* static uint8_t local_small_cache_flag[CFG_TEE_CORE_NB_CORE][NB_SMALL_PAGE_LOCAL_CACHE] = {0}; */
/* static uint8_t local_large_cache_flag[CFG_TEE_CORE_NB_CORE][NB_LARGE_PAGE_LOCAL_CACHE] = {0}; */

/* static struct pgt_cache sz_pgt_cache[8]; */
/* static struct pgt *pgt_ptr[CFG_TEE_CORE_NB_CORE]; */
/* static uint8_t global_page_flag[NB_PAGE_GLOBAL_POOL] = {0}; */
/* sz: allocate page on demand, populate l1/l2 entries if necessary */

#define NUM_OP 19
#define NUM_EVENT 3
uint32_t mem_bmap[NB_INT32_BMAP];

/* xzl: must start w/ X_CMD_
 * sz_show_resource() depends on it to skip the prefix
 */
#define OP_DESC_PREFIX_LEN (sizeof("X_CMD_") - 1)
static const char *op_desc[NUM_OP] = {
    "X_CMD_MAP       ",
    "X_CMD_KILL      ",
    "X_CMD_UA_RAND   ",
    "X_CMD_UA_NSBUF  ",
    "X_CMD_UA_CREATE ",
    "X_CMD_UA_RETIRE*",
    "X_CMD_UA_PSEUDO ",
    "X_CMD_UA_TO_NS  ",
    "X_CMD_UA_DEBUG  ",
    "X_CMD_SHOW_SBUFF",
    "X_CMD_SEGMENT   ",
    "X_CMD_SORT      ",
    "X_CMD_MERGE     ",
    "X_CMD_JOIN      ",
    "X_CMD_FILTER    ",
    "X_CMD_MEDIAN    ",
    "X_CMD_SUMCOUNT  ",
    "X_CMD_CONCATENAT",
    "X_CMD_MISC      "
};

static const char *event_desc[NUM_EVENT] = {
    "  alloc",
    " killed",
    "escaped"
};

static const uint8_t select_ops[] = {
	X_CMD_UA_PSEUDO,
	X_CMD_UA_CREATE,

	X_CMD_SEGMENT,
	X_CMD_SORT,
	X_CMD_MERGE,
	X_CMD_JOIN,
	X_CMD_FILTER,
	X_CMD_MEDIAN,
	X_CMD_SUMCOUNT,
	X_CMD_CONCATENATE,

	X_CMD_UA_RETIRE, /* put this at the end for easy eyeball */
};

int32_t batch_count[NUM_OP][NUM_EVENT] = {0};
struct core_mem_struct core_mem_arr[CFG_TEE_CORE_NB_CORE] = {0};
struct core_mem_shared core_mem_shared = {0};
unsigned int allocated = 0;
unsigned mem_spin_lock; /* init to be 0? */
inline static void sz_set_l2_pgtbl(struct core_mmu_table_info *dir_info,
                            unsigned int idx, struct pgt *pgt)
{
    unsigned int i;
    for (i = 0; i < CFG_TEE_CORE_NB_CORE; i++) {
		dir_info->table = l1_xlation_table + i;
		core_mmu_set_entry(dir_info, idx, virt_to_phys(pgt->tbl),
				TEE_MATTR_SECURE | TEE_MATTR_TABLE);
    }
}

inline uint32_t mem_lock_irq(void)
{
    uint32_t irq = thread_mask_exceptions(THREAD_EXCP_ALL);
    cpu_spin_lock(&mem_spin_lock);
    return irq;
}

inline void mem_unlock_irq(uint32_t irq)
{
    cpu_spin_unlock(&mem_spin_lock);
    thread_unmask_exceptions(irq);
}

static paddr_t sz_alloc_phys_mem(void) {
    unsigned int i, pos;
    uint32_t expected, desired;
    bool success;
    for (i = 0;;i++) {
        i %= NB_INT32_BMAP;
        expected = mem_bmap[i];
        while (mem_bmap[i] != 0x0) {
            pos = __builtin_clz(expected);
            desired = expected & ~((uint32_t)1 << (31 - pos));
#ifdef SZ_DEBUG_PRINT
            DMSG("sz: expected %x desired %x pos %d", expected, desired, pos);
#endif
            success = atomic_compare_exchange_strong(&mem_bmap[i], &expected, desired);
            if (success) {
#ifdef SZ_DEBUG_PRINT
                DMSG("sz: i %d pos %d alloc to %llx", i, pos,
                     PA_START + (i * 32 + pos) * CORE_MMU_PGDIR_SIZE);
#endif
                assert(PA_START + (i * 32 + pos) * CORE_MMU_PGDIR_SIZE < 0x80000000ull &&
                		PA_START + (i * 32 + pos) * CORE_MMU_PGDIR_SIZE >= 0x40000000ull);
                return PA_START + (i * 32 + pos) * CORE_MMU_PGDIR_SIZE;
            }
        }
        if (i == NB_INT32_BMAP - 1) {
            EMSG("Warning: Core %lu No physical memory available this round.", get_core_pos());
        }
    }
}
/* sz: Only apply to 2mb granularity */
void sz_reset_phys_mem(vaddr_t start, vaddr_t end) {
    uint32_t idx, idx_pa, pos;
    paddr_t pa;
    struct core_mmu_table_info pgt_info;
    /* uint32_t irq; */
    
    /* irq = mem_lock_irq(); */
    core_mmu_find_table(start, 2, &pgt_info);
    idx = core_mmu_va2idx(&pgt_info, start);

    /* if (start >= 0x6200000000) */
    /*     EMSG("reset: %lx ->%lx", start, end); */
    while(start < end) {
        core_mmu_get_entry(&pgt_info, idx, &pa, NULL);
        idx_pa = (pa - PA_START) >> CORE_MMU_PGDIR_SHIFT;
        pos = idx_pa % 32;
        idx_pa = idx_pa >> 5;
        atomic_fetch_or(&mem_bmap[idx_pa], (uint32_t)1 << (31 - pos));
        core_mmu_set_entry(&pgt_info, idx, 0, 0);
        start += CORE_MMU_PGDIR_SIZE;
        idx++;
    }
    core_tlb_maintenance(TLBINV_UNIFIEDTLB, 0);
    /* mem_unlock_irq(irq); */
}
/* extern bool finish; */
void sz_show_resource(void)
{
    unsigned int n, i, avail, dead;

#define LINEBUF_LEN 48
    char linebuf[LINEBUF_LEN];
    unsigned len = 0;
    int total_esc = 0;
    /* finish = true; */
    /* unsigned int count_pager[NUM_SBUFF] = {0}; */
    /* struct pgt *pgt; */
    EMSG("---------------------------------------");

    for (i = 0; i < CFG_TEE_CORE_NB_CORE; ++i) {
        avail = 0;
        dead = 0;
        for (n = 0; n < core_mem_arr[i].num_sbuff; n++)
            if (!core_mem_arr[i].sbuff_bmap[n])
                avail++;
            else if (atomic_load(&core_mem_arr[i].sbuff[n].last->state) == BATCH_STATE_DEAD)
                dead++;
        EMSG("sz: core %d avail sbuff %2d + %2d    %2d/%2d", i, avail, dead,
             avail+dead, core_mem_arr[i].num_sbuff);
    }

    avail = 0;
    dead = 0;
    for (n = 0; n < core_mem_shared.num_sbuff; n++)
        if (!core_mem_shared.sbuff_bmap[n])
            avail++;
        else if (atomic_load(&core_mem_shared.sbuff[n].last->state) == BATCH_STATE_DEAD)
            dead++;
    EMSG("sz: global avail sbuff %2d + %2d    %2d/%2d", avail, dead,
         avail+dead, core_mem_shared.num_sbuff);

    avail = 0;
    for (i = 0; i < NB_INT32_BMAP; ++i) {
        avail += __builtin_popcount(mem_bmap[i]);
    }
    EMSG("sz: avail 2MB phy mem     %3d/%3d", avail, NB_INT32_BMAP * 32);

    /*
     * xzl: pretty print mm ev counters
     */
    /* header - ev names */
    snprintf(linebuf, LINEBUF_LEN, "%10s", "");
    for (n = 0; n < NUM_EVENT; ++n) {
    	len = strnlen(linebuf, LINEBUF_LEN);
    	snprintf(linebuf + len, LINEBUF_LEN - len, "%8s", event_desc[n]);
    }
    EMSG("%s", linebuf);

    /* each row: one op & its events */
    for (i = 0; i < sizeof(select_ops); ++i) {
    	/* op name, truncated */
    	snprintf(linebuf, LINEBUF_LEN, "%.10s",
    			op_desc[select_ops[i]] + OP_DESC_PREFIX_LEN /* skip "X_CMD_..." */ );
			for (n = 0; n < NUM_EVENT; ++n) {
				len = strnlen(linebuf, LINEBUF_LEN);
				snprintf(linebuf + len, LINEBUF_LEN - len, "%8d",
						atomic_load(&batch_count[select_ops[i]][n]));
			}
			/* count esc. XXX we assume esc idx == 2
			 * it's okay if X_CMD_UA_RETIRE is selected, since it's escape == 0
			 */
			total_esc += batch_count[select_ops[i]][2];
			EMSG("%s", linebuf);
    }
    {
    	int32_t r = batch_count[X_CMD_UA_RETIRE][1];
    	EMSG("total esc %d, total retire %d, outstanding %d", total_esc,
    		r, total_esc - r);
    }

#if 0 /* shuang's */
    for (i = 0; i < sizeof(select_ops); ++i) {
        EMSG("\n** sz: Batch event counter for %s", op_desc[select_ops[i]]);
        for (n = 0; n < NUM_EVENT; ++n)
            EMSG("******** %s: %8d", event_desc[n], atomic_load(&batch_count[select_ops[i]][n]));
    }
#endif

}
struct pgt_cache global_pgt_cache;
void sz_populate_map(void)
{
    /* vaddr_t n; */
    struct tee_ta_ctx* ctx;
    /* unsigned int n; */
  
    ctx = tee_mmu_get_ctx();
    if (!ctx)
        DMSG("xzl: warning: ctx seems null. proceed...\n");

    sbuff_init();
#ifdef MEASURE_TIME
    mm_time = 0;
    cmp_time = 0;
#endif
    memset(mem_bmap, 0xff, NB_INT32_BMAP * sizeof(uint32_t));
    pgt_alloc(&global_pgt_cache, ctx, 0x80000000,
              0x80000000 + 480 * CORE_MMU_PGDIR_SIZE - 1);
  /* for (n = 0; n < CFG_TEE_CORE_NB_CORE; n++) { */
  /*       pgt_alloc(&core_mem_arr[n].pgt_cache, ctx, */
  /*                 0x80000000,  0x80000000 + 76 * CORE_MMU_PGDIR_SIZE - 1); */
        /* core_mem_arr[n].sbuff_bmap = {0}; */
        /* core_mem_arr[n].mem_bmap = {0}; */
        /* core_mem_arr[n].last_pos = 0; */
    /* } */
}

void sz_destroy_map(void)
{
    unsigned int i, j;
    /* struct pgt *pgt; */
    struct sbuff *sbuff;
    for (i = 0; i <CFG_TEE_CORE_NB_CORE; i++) {
        for (j = 0; j < NUM_SBUFF; j++) {
            sbuff = &core_mem_arr[i].sbuff[j];
            if (sbuff->l2_pgt) {
                memset((void *)sbuff->l2_pgt->tbl, 0x00, PGT_SIZE);
                SLIST_INSERT_HEAD(&global_pgt_cache, sbuff->l2_pgt, link);
            }
        }
    }
#ifdef MEASURE_TIME
    EMSG("mm time %lu", mm_time);
    EMSG("compute time %lu", cmp_time);
#endif
    for (i = 0; i < NUM_SBUFF_SHARED; i++) {
        sbuff = &core_mem_shared.sbuff[i];
        if (sbuff->l2_pgt) {
            memset((void *)sbuff->l2_pgt->tbl, 0x00, PGT_SIZE);
            SLIST_INSERT_HEAD(&global_pgt_cache, sbuff->l2_pgt, link);
        }
    }
    pgt_free(&global_pgt_cache, false);

    core_tlb_maintenance(TLBINV_UNIFIEDTLB, 0);
}


void sz_test_paging(void)
{
    sz_populate_map();

    DMSG("Core %zu, begin to touch 0x6000001000", get_core_pos());

    memset((void *)0x6000001000, 0xcc, 128);
    DHEXDUMP((void *)0x6000001000, 128);

    memset((void *)0x6001001000, 0xcc, 128);
    DHEXDUMP((void *)0x6001001000, 128);
    memset((void *)0x6051001000, 0xcc, 128); /* sz: test on new l2pgtbl */
    DHEXDUMP((void *)0x6051001000, 128);
}

#define RAND_MAX ((1u << 5) - 1)
static int32_t rseed = 0;

static inline int32_t rand(void)
{
    return rseed = (rseed * 1103515245 + 12345) & RAND_MAX;

}

void sz_test_sbuff(void)
{
    uint32_t n, ct = 0;
    struct batch *bat1, *bat2;
    int32_t *data, *filter;

    sz_populate_map();
    bat1 = batch_new(0);
    data = bat1->start;
    for (n = 0; n < 256 *4; ++n) {
        *data++ = rand();
    }
    batch_close(bat1, data);
    bat2 = batch_new(0);
    filter = bat2->start;
    data = bat1->start;

    DMSG("sz: bat1.size=%u\n", bat1->size);
    for (n = 0; n < bat1->size; n++) {
        if (*data > 5 && *data < 14) {
            *filter++ = *data;
            ct++;
        }
        data++;
    }
    BATCH_KILL(bat1, 0);
    batch_close(bat2, filter);
    bat1 = batch_new(0);
    data = bat1->start;
    DMSG("sz: use recycled pages\n");
    for (n = 0; n < 256 *4; ++n) {
        *data++ = rand();
    }

    DMSG("sz: ct = %d\n", ct);
    /* DHEXDUMP(bat1->start, 32*4); */
    /* DHEXDUMP(bat2->start, ct * 4); */

}




#if 0
static int sz_recycle_mem(struct sbuff *sbuff, vaddr_t va_app)
{
    unsigned int n_free_pages, idx_rec, idx_app, n, attr;
    struct core_mmu_table_info pgt_rec, pgt_app;
    paddr_t pa;
    vaddr_t va_rec;
    /* struct pgt *pgt; */

#ifdef SZ_DEBUG_PRINT
    DMSG("sz: sbuff=%p idx=%lu\n", (void *)sbuff, (va_app - SBUFF_BASE) >> SBUFF_VA_SHIFT);
    /* move recycle forward */
    DMSG("something");
    DMSG("sz: count %d %p", sbuff->count, (void*)sbuff->valid);
#endif
    while (sbuff->count && sbuff->valid->state == BATCH_STATE_DEAD) {
        sbuff->valid = sbuff->valid->next;
        atomic_fetch_sub(&sbuff->count, 1);
    }
    va_rec = (vaddr_t)sbuff->recycle;
    n_free_pages = ((vaddr_t)sbuff->valid - va_rec)
        >> CORE_MMU_PGDIR_SHIFT;
    if (!n_free_pages)
        return 0;
    /* TODO: Simplify to single pgt info */
    core_mmu_find_table(va_rec, 2, &pgt_rec);
    core_mmu_find_table(va_app, 2, &pgt_app);
    idx_rec = core_mmu_va2idx(&pgt_rec, va_rec);
    idx_app = core_mmu_va2idx(&pgt_app, va_app);
    /* DMSG("sz va_rec %lx va_app %lx\n", va_rec, va_app); */
    for (n = 0; n < n_free_pages; ++n) {
        core_mmu_get_entry(&pgt_rec, idx_rec, &pa, &attr);
        core_mmu_set_entry(&pgt_app, idx_app, pa, attr);
        core_mmu_set_entry(&pgt_rec, idx_rec, 0, 0);
        /* DMSG("sz: recycle from idx %u to idx %u, pa %lx\n", idx_rec, idx_app, pa); */
        if (++idx_rec == XLAT_TABLE_ENTRIES) {
            /* idx_rec = 0; */
            /* va_rec = ROUNDUP(va_rec + SMALL_PAGE_MASK, CORE_MMU_PGDIR_SIZE); */
            /* core_mmu_find_table(va_rec, 3, &pgt_rec); */
            panic("sbuff overflows in va space");
        }
        if (++idx_app == XLAT_TABLE_ENTRIES) {
            /* n++; */
            /* pgt = SLIST_FIRST(&sbuff->pgt_list); */
            /* SLIST_REMOVE_HEAD(&sbuff->pgt_list, link); */
            /* SLIST_INSERT_HEAD(&core_mem_arr[get_core_pos()].pgt_cache, pgt, link); */
            panic("sbuff overflows in va space");
            break;
        }
    }
    sbuff->recycle = (void *)((uint64_t)sbuff->recycle + (n << CORE_MMU_PGDIR_SHIFT));
    core_tlb_maintenance(TLBINV_UNIFIEDTLB, 0);

    return n;
}
#endif

unsigned int alloc_grain_page = 1;

/* caller must hold the lock */
void sz_allocate_page(vaddr_t va){
    vaddr_t page_va = va & ~SMALL_PAGE_MASK;
    struct pgt *pgt;
    struct core_mmu_table_info pgt_info;
    unsigned int /* n, */ sbuff_idx;
    struct sbuff *sbuff;
    uint32_t attr;

    if (page_va >= 0x0200000000 && page_va < 0x8000000000) { /* 256GB va */
        /* DMSG("sz: %ld, %p",((page_va - SBUFF_BASE) >> SBUFF_VA_SHIFT), (void*)sbuff); */
        /* DMSG("sz: sbuff base %p %ld", sbuff->base, ((page_va - SBUFF_BASE) >> SBUFF_VA_SHIFT)); */
        /* DMSG("va : %lx", page_va);	//hp */
        if (core_mmu_find_table(page_va, 3, &pgt_info)) {
            switch (pgt_info.level) {
            case 1:
                pgt = SLIST_FIRST(&global_pgt_cache);
                assert(pgt);
                sz_set_l2_pgtbl(&pgt_info, core_mmu_va2idx(&pgt_info, page_va), pgt);
                SLIST_REMOVE_HEAD(&global_pgt_cache, link); /* remove from available pgt */
                SLIST_NEXT(pgt, link) = NULL;
                core_mmu_set_info_table(&pgt_info, 2,
                                        page_va & ~((1ul << L1_XLAT_ADDRESS_SHIFT) - 1),
                                        pgt->tbl);
                sbuff_idx = ((page_va - SBUFF_BASE) >> SBUFF_VA_SHIFT);
                if (sbuff_idx < NUM_SBUFF * 8)
                    sbuff = &core_mem_arr[sbuff_idx / NUM_SBUFF].sbuff[sbuff_idx % NUM_SBUFF];
                else
                    sbuff = &core_mem_shared.sbuff[sbuff_idx - NUM_SBUFF * 8];
                sbuff_insert_l2_pgt(sbuff, pgt);
                /* DMSG("sz: add a new l2 pgtbl\n"); */
            case 2:

                /* n = sz_recycle_mem(sbuff, page_va); */
                /* if (n) break; */
                core_mmu_get_entry(&pgt_info, core_mmu_va2idx(&pgt_info, page_va),
                                   NULL, &attr);
                if (!(attr & 0x03))
                    core_mmu_set_entry(&pgt_info, core_mmu_va2idx(&pgt_info, page_va),
                                       sz_alloc_phys_mem(),
                                       core_mmu_type_to_attr(MEM_AREA_TEE_RAM));
                break;

            default:
                panic("sz: invalid pgt level");
            }
            core_tlb_maintenance(TLBINV_UNIFIEDTLB, 0);
        } else {
            panic("sz: invalid va. abort.");
        }
    } else
        panic ("cant handle");
}
bool sz_handle_fault_v2(struct abort_info *ai)
{
    vaddr_t page_va = ai->va & ~SMALL_PAGE_MASK;
#ifdef MEASURE_TIME
    TEE_Time start, end;
#endif
    uint32_t irq;
#ifdef SZ_DEBUG_PRINT
    abort_print(ai);
#endif

#ifdef MEASURE_TIME
    tee_time_get_sys_time(&start);
#endif
    irq = mem_lock_irq();
    if (abort_is_user_exception(ai))
        panic("not implemented");
    if (page_va >= 0x0200000000 && page_va < 0x8000000000) { /* 256GB va */
        /* DMSG("sz: %ld, %p",((page_va - SBUFF_BASE) >> SBUFF_VA_SHIFT), (void*)sbuff); */
        /* DMSG("sz: sbuff base %p %ld", sbuff->base, ((page_va - SBUFF_BASE) >> SBUFF_VA_SHIFT)); */
        /* DMSG("va : %lx", page_va);	//hp */
        sz_allocate_page(ai->va);
    } else {
      	abort_print(ai);
        panic ("cant handle");
    }
    mem_unlock_irq(irq);
#ifdef MEASURE_TIME
    tee_time_get_sys_time(&end);
    atomic_fetch_add(&mm_time, sz_time_diff(&start, &end));
#endif
    return true;
}
#if 0
bool sz_handle_fault_2mb(struct abort_info *ai)
{
    vaddr_t page_va = ai->va & ~SMALL_PAGE_MASK;
    struct pgt *pgt;
    struct core_mmu_table_info dir_info, pgt_info;
    paddr_t pa;
    uint32_t attr;
    unsigned int idx, i, j;

#ifdef SZ_DEBUG_PRINT
    abort_print(ai);
#endif

    if (abort_is_user_exception(ai))
        panic("not implemented");

    if (page_va >= 0x6000000000 && page_va < 0x6080000000) { /* 2GB va */
        core_mmu_set_info_table(&dir_info, 1,
                                0 /* va base */, l1_xlation_table[get_core_pos()]);
        idx = core_mmu_va2idx(&dir_info, page_va); /* idx in l1 pgtbl */

        if ((l1_xlation_table[get_core_pos()][idx] & 0x01) != 1) { /* invalid entry */
            // Allocate a new l2 pgtbl
            pgt = pgt_ptr[get_core_pos()];
            assert(pgt);
            sz_set_l2_pgtbl(&dir_info, idx, pgt);
            pgt_ptr[get_core_pos()] = SLIST_NEXT(pgt, link); /* remove from available pgt */
            core_mmu_set_info_table(&pgt_info, 2,
                                    page_va & ~((1ul << L1_XLAT_ADDRESS_SHIFT) - 1),
                                    pgt->tbl);
#ifdef SZ_DEBUG_PRINT
            DMSG("sz: va_base for l2 %lx\n", page_va & ~CORE_MMU_PGDIR_MASK);
            DMSG("sz: create new l2 pgt, l1 idx = %d\n", idx);
#endif
            /* need to add option to check the entry */
        } else {            /* l2 table exists */
#ifdef SZ_DEBUG_PRINT
            DMSG("sz: l2 pgt in l1 entry exists\n");
#endif
            core_mmu_get_entry(&dir_info, idx, &pa, &attr);
            core_mmu_set_info_table(&pgt_info, 2,
                                    page_va & ~((1ul << L1_XLAT_ADDRESS_SHIFT) - 1),
                                    phys_to_virt(pa, MEM_AREA_TEE_RAM));
        }

        idx = core_mmu_va2idx(&pgt_info, page_va); /* idx in l2 pgtbl */
        /* table = (uint64_t *)pgt_info.table; */

        for (j = 0; j < CFG_TEE_CORE_NB_CORE; ++j){
            for (i = 0; i < NB_SMALL_PAGE_LOCAL_CACHE; ++i) {
                if (!local_small_cache_flag[j][i]) {
                    local_small_cache_flag[j][i] = 1;
                    break;
                }
            }
            if(i != NB_SMALL_PAGE_LOCAL_CACHE) break;
        }
        if (i == NB_SMALL_PAGE_LOCAL_CACHE) panic("No local cache available");
        core_mmu_set_entry(&pgt_info, idx, PA_START +
                           LOCAL_CACHE_SIZE * j + i * CORE_MMU_PGDIR_SIZE,
                           core_mmu_type_to_attr(MEM_AREA_TEE_RAM));

        core_tlb_maintenance(TLBINV_UNIFIEDTLB, 0);

    } else
        panic ("cant handle");
    return true;
}
bool sz_handle_fault(struct abort_info *ai)
{
    vaddr_t page_va = ai->va & ~SMALL_PAGE_MASK;
    struct pgt *pgt;
    struct core_mmu_table_info dir_info, pgt_info;
    paddr_t pa;
    uint32_t attr;
    unsigned int idx/* , i, j */;
    uint64_t *table;
    unsigned int i = ig, j = jg;

#ifdef SZ_DEBUG_PRINT
    abort_print(ai);
#endif

    /* if (allocated) */
    /*     panic(); */
    if (abort_is_user_exception(ai))
        panic("not implemented");

    if (page_va >= 0x6000000000 && page_va < 0x6080000000) { /* 2GB va */
        core_mmu_set_info_table(&dir_info, 1,
                                0 /* va base */, l1_xlation_table[get_core_pos()]);
        idx = core_mmu_va2idx(&dir_info, page_va); /* idx in l1 pgtbl */

        if ((l1_xlation_table[get_core_pos()][idx] & 0x01) != 1) { /* invalid entry */
            // Allocate a new l2 pgtbl
            pgt = pgt_ptr[get_core_pos()];
            assert(pgt);
            sz_set_l2_pgtbl(&dir_info, idx, pgt);
            pgt_ptr[get_core_pos()] = SLIST_NEXT(pgt, link); /* remove from available pgt */
            core_mmu_set_info_table(&pgt_info, 2,
                                    page_va & ~((1ul << L1_XLAT_ADDRESS_SHIFT) - 1),
                                    pgt->tbl);
#ifdef SZ_DEBUG_PRINT
            DMSG("sz: va_base for l2 %lx\n", page_va & ~CORE_MMU_PGDIR_MASK);
            DMSG("sz: create new l2 pgt, l1 idx = %d\n", idx);
#endif
            /* need to add option to check the entry */
        } else {            /* l2 table exists */
#ifdef SZ_DEBUG_PRINT
            DMSG("sz: l2 pgt in l1 entry exists\n");
#endif
            core_mmu_get_entry(&dir_info, idx, &pa, &attr);
            core_mmu_set_info_table(&pgt_info, 2,
                                    page_va & ~((1ul << L1_XLAT_ADDRESS_SHIFT) - 1),
                                    phys_to_virt(pa, MEM_AREA_TEE_RAM));
        }

        idx = core_mmu_va2idx(&pgt_info, page_va); /* idx in l2 pgtbl */
        table = (uint64_t *)pgt_info.table;
        if ((table[idx] & 0x01) != 1) {
            pgt = pgt_ptr[get_core_pos()];
            assert(pgt);
            /* if (!pgt) { */
            /*     DMSG("sz: error: %d pgt used\n", used_pgt); */
            /*     assert(pgt); */
            /* } */
            core_mmu_set_entry(&pgt_info, idx, virt_to_phys(pgt->tbl),
                               TEE_MATTR_SECURE | TEE_MATTR_TABLE);
            pgt_ptr[get_core_pos()] = SLIST_NEXT(pgt, link);
            core_mmu_set_info_table(&pgt_info, 3, page_va & ~CORE_MMU_PGDIR_MASK, pgt->tbl);
#ifdef SZ_DEBUG_PRINT
            DMSG("sz: create new l3 pgt, l2 idx = %d\n", idx);
#endif
        } else {
            core_mmu_get_entry(&pgt_info, idx, &pa, &attr);
            core_mmu_set_info_table(&pgt_info, 3, page_va & ~CORE_MMU_PGDIR_MASK,
                phys_to_virt(pa, MEM_AREA_TEE_RAM));
        }

        /* print pgt_info */
#ifdef SZ_DEBUG_PRINT
        DMSG("sz: level %d va_base %lx shift %d\n", pgt_info.level,
             pgt_info.va_base, pgt_info.shift);
#endif
        /* begin to fill l3 entry */
        idx = core_mmu_va2idx(&pgt_info, page_va);
        /* for (j = 0; j < CFG_TEE_CORE_NB_CORE; ++j){ */
        /*     for (i = 0; i < NB_SMALL_PAGE_LOCAL_CACHE; ++i) { */
        /*         if (!local_small_cache_flag[j][i]) { */
        /*             local_small_cache_flag[j][i] = 1; */
        /*             break; */
        /*         } */
        /*     } */
        /*     if(i != NB_SMALL_PAGE_LOCAL_CACHE) break; */
        /* } */
        /* if (i == NB_SMALL_PAGE_LOCAL_CACHE) panic("No local cache available"); */
        do {
            if (++i == NB_SMALL_PAGE_LOCAL_CACHE) {
                i = 0;
                j++;
            }
            if (j == CFG_TEE_CORE_NB_CORE) {
                j = 0;
            }
            if (jg == j && ig == i)
                panic("no pa available");
        } while (local_small_cache_flag[j][i]);
        ig = i;
        jg = j;

#ifdef SZ_DEBUG_PRINT
        DMSG("sz: l3 entry: idx %d %lx -> %llx", idx, page_va,
             PA_START + LOCAL_CACHE_SIZE * j + i * SMALL_PAGE_SIZE);
#endif
        core_mmu_set_entry(&pgt_info, idx, PA_START + 
                           LOCAL_CACHE_SIZE * j + i * SMALL_PAGE_SIZE,
                           core_mmu_type_to_attr(MEM_AREA_TEE_RAM));
        core_mmu_get_entry(&pgt_info, idx, &pa, &attr);
#ifdef SZ_DEBUG_PRINT
        DMSG("sz: set pg lv3 region done. entry %d: pa %lx, attr %x\n", idx, pa, attr);
#endif
        core_tlb_maintenance(TLBINV_UNIFIEDTLB, 0);

    } else
        panic ("cant handle");
    return true;
}
#endif
void core_init_mmu_tables(struct tee_mmap_region *mm)
{
	paddr_t max_pa = 0;
	uint64_t max_va = 0;
	size_t n;

	for (n = 0; mm[n].size; n++) {
		paddr_t pa_end;
		vaddr_t va_end;

		debug_print(" %010" PRIxVA " %010" PRIxPA " %10zx %x",
			    mm[n].va, mm[n].pa, mm[n].size, mm[n].attr);

		if (!IS_PAGE_ALIGNED(mm[n].pa) || !IS_PAGE_ALIGNED(mm[n].size))
			panic("unaligned region");

		pa_end = mm[n].pa + mm[n].size - 1;
		va_end = mm[n].va + mm[n].size - 1;
		if (pa_end > max_pa)
			max_pa = pa_end;
		if (va_end > max_va)
			max_va = va_end;
	}

	/* Clear table before use */
	memset(l1_xlation_table[0], 0, NUM_L1_ENTRIES * XLAT_ENTRY_SIZE);
	// xzl: populating for all regions starting at @mm
	// why base_va == 0?
	init_xlation_table(mm, 0, l1_xlation_table[0], 1);
	for (n = 1; n < CFG_TEE_CORE_NB_CORE; n++)
		memcpy(l1_xlation_table[n], l1_xlation_table[0],
			XLAT_ENTRY_SIZE * NUM_L1_ENTRIES);

	// xzl: init the global @user_va_idx
	for (n = 1; n < NUM_L1_ENTRIES; n++) {
		if (!l1_xlation_table[0][n]) {
			user_va_idx = n;
			break;
		}
	}

	assert(user_va_idx != -1);

	DMSG("xzl: user_va_idx %d NUM_L1_ENTRIES %llu\n", user_va_idx, NUM_L1_ENTRIES);

	tcr_ps_bits = calc_physical_addr_size_bits(max_pa);
	COMPILE_TIME_ASSERT(ADDR_SPACE_SIZE > 0);
	assert(max_va < ADDR_SPACE_SIZE);
}

bool core_mmu_place_tee_ram_at_top(paddr_t paddr)
{
	size_t l1size = (1 << L1_XLAT_ADDRESS_SHIFT);
	paddr_t l1mask = l1size - 1;

	return (paddr & l1mask) > (l1size / 2);
}

#ifdef ARM32
void core_init_mmu_regs(void)
{
	uint32_t ttbcr = TTBCR_EAE;
	uint32_t mair;
	paddr_t ttbr0;

	ttbr0 = virt_to_phys(l1_xlation_table[get_core_pos()]);

	mair  = MAIR_ATTR_SET(ATTR_DEVICE, ATTR_DEVICE_INDEX);
	mair |= MAIR_ATTR_SET(ATTR_IWBWA_OWBWA_NTR, ATTR_IWBWA_OWBWA_NTR_INDEX);
	write_mair0(mair);

	ttbcr |= TTBCR_XRGNX_WBWA << TTBCR_IRGN0_SHIFT;
	ttbcr |= TTBCR_XRGNX_WBWA << TTBCR_ORGN0_SHIFT;
	ttbcr |= TTBCR_SHX_ISH << TTBCR_SH0_SHIFT;

	/* Disable the use of TTBR1 */
	ttbcr |= TTBCR_EPD1;

	/* TTBCR.A1 = 0 => ASID is stored in TTBR0 */

	write_ttbcr(ttbcr);
	write_ttbr0_64bit(ttbr0);
	write_ttbr1_64bit(0);
}
#endif /*ARM32*/

#ifdef ARM64
// xzl: this configures the MMU. called by (asm) cpu_on_handler()
// is this called on each cpu core?
void core_init_mmu_regs(void)
{
	uint64_t mair;
	uint64_t tcr;
	paddr_t ttbr0;

	// xzl: seems only use ttbr0 (not ttbr1)?
	ttbr0 = virt_to_phys(l1_xlation_table[get_core_pos()]);

	mair  = MAIR_ATTR_SET(ATTR_DEVICE, ATTR_DEVICE_INDEX);
	mair |= MAIR_ATTR_SET(ATTR_IWBWA_OWBWA_NTR, ATTR_IWBWA_OWBWA_NTR_INDEX);
	write_mair_el1(mair);

	tcr = TCR_RES1;
	tcr |= TCR_XRGNX_WBWA << TCR_IRGN0_SHIFT;
	tcr |= TCR_XRGNX_WBWA << TCR_ORGN0_SHIFT;
	tcr |= TCR_SHX_ISH << TCR_SH0_SHIFT;
	tcr |= tcr_ps_bits << TCR_EL1_IPS_SHIFT;
	tcr |= 64 - __builtin_ctzl(ADDR_SPACE_SIZE);
	/* xzl: bit0-5 T0SZ, size offset of memory addressed by ttbr0 */

	/* Disable the use of TTBR1 */
	tcr |= TCR_EPD1;

	/*
	 * TCR.A1 = 0 => ASID is stored in TTBR0
	 * TCR.AS = 0 => Same ASID size as in Aarch32/ARMv7
	 */

        DMSG("sz: final tcr = %lx.\n", tcr);
	write_tcr_el1(tcr);
	write_ttbr0_el1(ttbr0);
	write_ttbr1_el1(0);
}
#endif /*ARM64*/

// xzl: populating a core_mmu_table_info (desc for a pgtable)
void core_mmu_set_info_table(struct core_mmu_table_info *tbl_info,
		unsigned level, vaddr_t va_base, void *table)
{
	tbl_info->level = level;
	tbl_info->table = table;
	tbl_info->va_base = va_base;
	tbl_info->shift = L1_XLAT_ADDRESS_SHIFT -
			  (level - 1) * XLAT_TABLE_ENTRIES_SHIFT;
	assert(level <= 3);
	if (level == 1)
		tbl_info->num_entries = NUM_L1_ENTRIES;
	else
		tbl_info->num_entries = XLAT_TABLE_ENTRIES;
}

// xzl: this seems to get a l2 pgtable (the user's)
void core_mmu_get_user_pgdir(struct core_mmu_table_info *pgd_info)
{
	vaddr_t va_range_base;
	void *tbl = xlat_tables_ul1[thread_get_id()];

	core_mmu_get_user_va_range(&va_range_base, NULL);
	core_mmu_set_info_table(pgd_info, 2, va_range_base, tbl); // xzl: lv=2
}

void core_mmu_create_user_map(struct user_ta_ctx *utc,
			      struct core_mmu_user_map *map)
{
	struct core_mmu_table_info dir_info;

	DMSG("xzl: called\n");

	COMPILE_TIME_ASSERT(sizeof(uint64_t) * XLAT_TABLE_ENTRIES == PGT_SIZE);

	core_mmu_get_user_pgdir(&dir_info);
	memset(dir_info.table, 0, PGT_SIZE);
	core_mmu_populate_user_map(&dir_info, utc);
	map->user_map = virt_to_phys(dir_info.table) | TABLE_DESC;
	map->asid = utc->context & TTBR_ASID_MASK;
}

bool core_mmu_find_table(vaddr_t va, unsigned max_level,
		struct core_mmu_table_info *tbl_info)
{
	uint64_t *tbl = l1_xlation_table[get_core_pos()];
	uintptr_t ntbl;
	unsigned level = 1;
	vaddr_t va_base = 0;
	unsigned num_entries = NUM_L1_ENTRIES;

	while (true) {
		unsigned level_size_shift =
			L1_XLAT_ADDRESS_SHIFT - (level - 1) *
						XLAT_TABLE_ENTRIES_SHIFT;
		unsigned long n = (va - va_base) >> level_size_shift;
                /* EMSG("sz: n %ld >= num_entries %d level %d va %lx va_base %lx shift %d\n", */
                /*      n, num_entries, level, va, va_base, level_size_shift); */
		if (n >= num_entries) {
                    return false;

                }
		if (level == max_level || level == 3 ||
			(tbl[n] & TABLE_DESC) != TABLE_DESC) {
			/*
			 * We've either reached max_level, level 3, a block
			 * mapping entry or an "invalid" mapping entry.
			 */
			tbl_info->table = tbl;
			tbl_info->va_base = va_base;
			tbl_info->level = level;
			tbl_info->shift = level_size_shift;
			tbl_info->num_entries = num_entries;
			return true;
		}

		/* Copy bits 39:12 from tbl[n] to ntbl */
		ntbl = (tbl[n] & ((1ULL << 40) - 1)) & ~((1 << 12) - 1);

		tbl = phys_to_virt(ntbl, MEM_AREA_TEE_RAM);
		if (!tbl)
			return false;

		va_base += n << level_size_shift;
		level++;
		num_entries = XLAT_TABLE_ENTRIES;
	}
}

bool core_mmu_divide_block(struct core_mmu_table_info *tbl_info,
			   unsigned int idx)
{
	uint64_t *new_table;
	uint64_t *entry;
	uint64_t new_table_desc;
	size_t new_entry_size;
	paddr_t paddr;
	uint32_t attr;
	int i;

	if (tbl_info->level >= 3)
		return false;

	if (next_xlat >= MAX_XLAT_TABLES)
		return false;

	if (tbl_info->level == 1 && idx >= NUM_L1_ENTRIES)
		return false;

	if (tbl_info->level > 1 && idx >= XLAT_TABLE_ENTRIES)
		return false;

	entry = (uint64_t *)tbl_info->table + idx;
	assert((*entry & DESC_ENTRY_TYPE_MASK) == BLOCK_DESC);

	new_table = xlat_tables[next_xlat++];
	new_table_desc = TABLE_DESC | (uint64_t)(uintptr_t)new_table;

	/* store attributes of original block */
	attr = desc_to_mattr(tbl_info->level, *entry);
	paddr = *entry & OUTPUT_ADDRESS_MASK;
	new_entry_size = 1 << (tbl_info->shift - XLAT_TABLE_ENTRIES_SHIFT);

	/* Fill new xlat table with entries pointing to the same memory */
	for (i = 0; i < XLAT_TABLE_ENTRIES; i++) {
		*new_table = paddr | mattr_to_desc(tbl_info->level + 1, attr);
		paddr += new_entry_size;
		new_table++;
	}

	/* Update descriptor at current level */
	*entry = new_table_desc;
	return true;
}

void core_mmu_set_entry_primitive(void *table, size_t level, size_t idx,
				  paddr_t pa, uint32_t attr)
{
	uint64_t *tbl = table;
	uint64_t desc = mattr_to_desc(level, attr);

	tbl[idx] = desc | pa;
}

void core_mmu_get_entry_primitive(const void *table, size_t level,
				  size_t idx, paddr_t *pa, uint32_t *attr)
{
	const uint64_t *tbl = table;

	if (pa)
		*pa = (tbl[idx] & ((1ull << 40) - 1)) & ~((1 << 12) - 1);

	if (attr)
		*attr = desc_to_mattr(level, tbl[idx]);
}

bool core_mmu_user_va_range_is_defined(void)
{
	return user_va_idx != -1;
}

void core_mmu_get_user_va_range(vaddr_t *base, size_t *size)
{
	assert(user_va_idx != -1);

	if (base)
		*base = (vaddr_t)user_va_idx << L1_XLAT_ADDRESS_SHIFT;
	if (size)
		*size = 1 << L1_XLAT_ADDRESS_SHIFT;
}

bool core_mmu_user_mapping_is_active(void)
{
	assert(user_va_idx != -1);
	return !!l1_xlation_table[get_core_pos()][user_va_idx];
}

#ifdef ARM32
void core_mmu_get_user_map(struct core_mmu_user_map *map)
{
	assert(user_va_idx != -1);

	map->user_map = l1_xlation_table[get_core_pos()][user_va_idx];
	if (map->user_map) {
		map->asid = (read_ttbr0_64bit() >> TTBR_ASID_SHIFT) &
			    TTBR_ASID_MASK;
	} else {
		map->asid = 0;
	}
}

void core_mmu_set_user_map(struct core_mmu_user_map *map)
{
	uint64_t ttbr;
	uint32_t exceptions = thread_mask_exceptions(THREAD_EXCP_ALL);

	assert(user_va_idx != -1);

	ttbr = read_ttbr0_64bit();
	/* Clear ASID */
	ttbr &= ~((uint64_t)TTBR_ASID_MASK << TTBR_ASID_SHIFT);
	write_ttbr0_64bit(ttbr);
	isb();

	/* Set the new map */
	if (map && map->user_map) {
		l1_xlation_table[get_core_pos()][user_va_idx] = map->user_map;
		dsb();	/* Make sure the write above is visible */
		ttbr |= ((uint64_t)map->asid << TTBR_ASID_SHIFT);
		write_ttbr0_64bit(ttbr);
		isb();
	} else {
		l1_xlation_table[get_core_pos()][user_va_idx] = 0;
		dsb();	/* Make sure the write above is visible */
	}

	core_tlb_maintenance(TLBINV_UNIFIEDTLB, 0);

	thread_unmask_exceptions(exceptions);
}

enum core_mmu_fault core_mmu_get_fault_type(uint32_t fault_descr)
{
	assert(fault_descr & FSR_LPAE);

	switch (fault_descr & FSR_STATUS_MASK) {
	case 0x21: /* b100001 Alignment fault */
		return CORE_MMU_FAULT_ALIGNMENT;
	case 0x11: /* b010001 Asynchronous extern abort (DFSR only) */
		return CORE_MMU_FAULT_ASYNC_EXTERNAL;
	case 0x12: /* b100010 Debug event */
		return CORE_MMU_FAULT_DEBUG_EVENT;
	default:
		break;
	}

	switch ((fault_descr & FSR_STATUS_MASK) >> 2) {
	case 0x1: /* b0001LL Translation fault */
		return CORE_MMU_FAULT_TRANSLATION;
	case 0x2: /* b0010LL Access flag fault */
	case 0x3: /* b0011LL Permission fault */
		if (fault_descr & FSR_WNR)
			return CORE_MMU_FAULT_WRITE_PERMISSION;
		else
			return CORE_MMU_FAULT_READ_PERMISSION;
	default:
		return CORE_MMU_FAULT_OTHER;
	}
}
#endif /*ARM32*/

#ifdef ARM64
void core_mmu_get_user_map(struct core_mmu_user_map *map)
{
	assert(user_va_idx != -1);

	map->user_map = l1_xlation_table[get_core_pos()][user_va_idx];
	if (map->user_map) {
		map->asid = (read_ttbr0_el1() >> TTBR_ASID_SHIFT) &
			    TTBR_ASID_MASK;
	} else {
		map->asid = 0;
	}
}

// update the TA table.
// xzl: only concerns ttbr0. the entry @user_va_idx points to the user map
void core_mmu_set_user_map(struct core_mmu_user_map *map)
{
	uint64_t ttbr;
	uint32_t daif = read_daif();

//	DMSG("xzl: enter %s\n", __func__);		// causes infi loop... why?

	write_daif(daif | DAIF_AIF);

	ttbr = read_ttbr0_el1();
	/* Clear ASID */
	ttbr &= ~((uint64_t)TTBR_ASID_MASK << TTBR_ASID_SHIFT);
	write_ttbr0_el1(ttbr);
	isb();

	/* Set the new map */
	if (map && map->user_map) {
		l1_xlation_table[get_core_pos()][user_va_idx] = map->user_map;
		dsb();	/* Make sure the write above is visible */
		ttbr |= ((uint64_t)map->asid << TTBR_ASID_SHIFT);
		write_ttbr0_el1(ttbr);
		isb();
	} else {
		l1_xlation_table[get_core_pos()][user_va_idx] = 0;
		dsb();	/* Make sure the write above is visible */
	}

	// xzl: flush the entire tlb (including core). too expensive?
	core_tlb_maintenance(TLBINV_UNIFIEDTLB, 0);

	write_daif(daif);
}

enum core_mmu_fault core_mmu_get_fault_type(uint32_t fault_descr)
{
	switch ((fault_descr >> ESR_EC_SHIFT) & ESR_EC_MASK) {
	case ESR_EC_SP_ALIGN:
	case ESR_EC_PC_ALIGN:
		return CORE_MMU_FAULT_ALIGNMENT;
	case ESR_EC_IABT_EL0:
	case ESR_EC_DABT_EL0:
	case ESR_EC_IABT_EL1:
	case ESR_EC_DABT_EL1:
		switch (fault_descr & ESR_FSC_MASK) {
		case ESR_FSC_TRANS_L0:
		case ESR_FSC_TRANS_L1:
		case ESR_FSC_TRANS_L2:
		case ESR_FSC_TRANS_L3:
			return CORE_MMU_FAULT_TRANSLATION;
		case ESR_FSC_ACCF_L1:
		case ESR_FSC_ACCF_L2:
		case ESR_FSC_ACCF_L3:
		case ESR_FSC_PERMF_L1:
		case ESR_FSC_PERMF_L2:
		case ESR_FSC_PERMF_L3:
			if (fault_descr & ESR_ABT_WNR)
				return CORE_MMU_FAULT_WRITE_PERMISSION;
			else
				return CORE_MMU_FAULT_READ_PERMISSION;
		case ESR_FSC_ALIGN:
			return CORE_MMU_FAULT_ALIGNMENT;
		default:
			return CORE_MMU_FAULT_OTHER;
		}
	default:
		return CORE_MMU_FAULT_OTHER;
	}
}
#endif /*ARM64*/
