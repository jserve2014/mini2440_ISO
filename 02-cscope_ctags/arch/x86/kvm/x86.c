/*
 * Kernel-based Virtual Machine driver for Linux
 *
 * derived from drivers/kvm/kvm_main.c
 *
 * Copyright (C) 2006 Qumranet, Inc.
 * Copyright (C) 2008 Qumranet, Inc.
 * Copyright IBM Corporation, 2008
 *
 * Authors:
 *   Avi Kivity   <avi@qumranet.com>
 *   Yaniv Kamay  <yaniv@qumranet.com>
 *   Amit Shah    <amit.shah@qumranet.com>
 *   Ben-Ami Yassour <benami@il.ibm.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 */

#include <linux/kvm_host.h>
#include "irq.h"
#include "mmu.h"
#include "i8254.h"
#include "tss.h"
#include "kvm_cache_regs.h"
#include "x86.h"

#include <linux/clocksource.h>
#include <linux/interrupt.h>
#include <linux/kvm.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/mman.h>
#include <linux/highmem.h>
#include <linux/iommu.h>
#include <linux/intel-iommu.h>
#include <linux/cpufreq.h>
#include <trace/events/kvm.h>
#undef TRACE_INCLUDE_FILE
#define CREATE_TRACE_POINTS
#include "trace.h"

#include <asm/uaccess.h>
#include <asm/msr.h>
#include <asm/desc.h>
#include <asm/mtrr.h>
#include <asm/mce.h>

#define MAX_IO_MSRS 256
#define CR0_RESERVED_BITS						\
	(~(unsigned long)(X86_CR0_PE | X86_CR0_MP | X86_CR0_EM | X86_CR0_TS \
			  | X86_CR0_ET | X86_CR0_NE | X86_CR0_WP | X86_CR0_AM \
			  | X86_CR0_NW | X86_CR0_CD | X86_CR0_PG))
#define CR4_RESERVED_BITS						\
	(~(unsigned long)(X86_CR4_VME | X86_CR4_PVI | X86_CR4_TSD | X86_CR4_DE\
			  | X86_CR4_PSE | X86_CR4_PAE | X86_CR4_MCE	\
			  | X86_CR4_PGE | X86_CR4_PCE | X86_CR4_OSFXSR	\
			  | X86_CR4_OSXMMEXCPT | X86_CR4_VMXE))

#define CR8_RESERVED_BITS (~(unsigned long)X86_CR8_TPR)

#define KVM_MAX_MCE_BANKS 32
#define KVM_MCE_CAP_SUPPORTED MCG_CTL_P

/* EFER defaults:
 * - enable syscall per default because its emulated by KVM
 * - enable LME and LMA per default on 64 bit KVM
 */
#ifdef CONFIG_X86_64
static u64 __read_mostly efer_reserved_bits = 0xfffffffffffffafeULL;
#else
static u64 __read_mostly efer_reserved_bits = 0xfffffffffffffffeULL;
#endif

#define VM_STAT(x) offsetof(struct kvm, stat.x), KVM_STAT_VM
#define VCPU_STAT(x) offsetof(struct kvm_vcpu, stat.x), KVM_STAT_VCPU

static void update_cr8_intercept(struct kvm_vcpu *vcpu);
static int kvm_dev_ioctl_get_supported_cpuid(struct kvm_cpuid2 *cpuid,
				    struct kvm_cpuid_entry2 __user *entries);

struct kvm_x86_ops *kvm_x86_ops;
EXPORT_SYMBOL_GPL(kvm_x86_ops);

int ignore_msrs = 0;
module_param_named(ignore_msrs, ignore_msrs, bool, S_IRUGO | S_IWUSR);

struct kvm_stats_debugfs_item debugfs_entries[] = {
	{ "pf_fixed", VCPU_STAT(pf_fixed) },
	{ "pf_guest", VCPU_STAT(pf_guest) },
	{ "tlb_flush", VCPU_STAT(tlb_flush) },
	{ "invlpg", VCPU_STAT(invlpg) },
	{ "exits", VCPU_STAT(exits) },
	{ "io_exits", VCPU_STAT(io_exits) },
	{ "mmio_exits", VCPU_STAT(mmio_exits) },
	{ "signal_exits", VCPU_STAT(signal_exits) },
	{ "irq_window", VCPU_STAT(irq_window_exits) },
	{ "nmi_window", VCPU_STAT(nmi_window_exits) },
	{ "halt_exits", VCPU_STAT(halt_exits) },
	{ "halt_wakeup", VCPU_STAT(halt_wakeup) },
	{ "hypercalls", VCPU_STAT(hypercalls) },
	{ "request_irq", VCPU_STAT(request_irq_exits) },
	{ "irq_exits", VCPU_STAT(irq_exits) },
	{ "host_state_reload", VCPU_STAT(host_state_reload) },
	{ "efer_reload", VCPU_STAT(efer_reload) },
	{ "fpu_reload", VCPU_STAT(fpu_reload) },
	{ "insn_emulation", VCPU_STAT(insn_emulation) },
	{ "insn_emulation_fail", VCPU_STAT(insn_emulation_fail) },
	{ "irq_injections", VCPU_STAT(irq_injections) },
	{ "nmi_injections", VCPU_STAT(nmi_injections) },
	{ "mmu_shadow_zapped", VM_STAT(mmu_shadow_zapped) },
	{ "mmu_pte_write", VM_STAT(mmu_pte_write) },
	{ "mmu_pte_updated", VM_STAT(mmu_pte_updated) },
	{ "mmu_pde_zapped", VM_STAT(mmu_pde_zapped) },
	{ "mmu_flooded", VM_STAT(mmu_flooded) },
	{ "mmu_recycled", VM_STAT(mmu_recycled) },
	{ "mmu_cache_miss", VM_STAT(mmu_cache_miss) },
	{ "mmu_unsync", VM_STAT(mmu_unsync) },
	{ "remote_tlb_flush", VM_STAT(remote_tlb_flush) },
	{ "largepages", VM_STAT(lpages) },
	{ NULL }
};

unsigned long segment_base(u16 selector)
{
	struct descriptor_table gdt;
	struct desc_struct *d;
	unsigned long table_base;
	unsigned long v;

	if (selector == 0)
		return 0;

	kvm_get_gdt(&gdt);
	table_base = gdt.base;

	if (selector & 4) {           /* from ldt */
		u16 ldt_selector = kvm_read_ldt();

		table_base = segment_base(ldt_selector);
	}
	d = (struct desc_struct *)(table_base + (selector & ~7));
	v = get_desc_base(d);
#ifdef CONFIG_X86_64
	if (d->s == 0 && (d->type == 2 || d->type == 9 || d->type == 11))
		v |= ((unsigned long)((struct ldttss_desc64 *)d)->base3) << 32;
#endif
	return v;
}
EXPORT_SYMBOL_GPL(segment_base);

u64 kvm_get_apic_base(struct kvm_vcpu *vcpu)
{
	if (irqchip_in_kernel(vcpu->kvm))
		return vcpu->arch.apic_base;
	else
		return vcpu->arch.apic_base;
}
EXPORT_SYMBOL_GPL(kvm_get_apic_base);

void kvm_set_apic_base(struct kvm_vcpu *vcpu, u64 data)
{
	/* TODO: reserve bits check */
	if (irqchip_in_kernel(vcpu->kvm))
		kvm_lapic_set_base(vcpu, data);
	else
		vcpu->arch.apic_base = data;
}
EXPORT_SYMBOL_GPL(kvm_set_apic_base);

void kvm_queue_exception(struct kvm_vcpu *vcpu, unsigned nr)
{
	WARN_ON(vcpu->arch.exception.pending);
	vcpu->arch.exception.pending = true;
	vcpu->arch.exception.has_error_code = false;
	vcpu->arch.exception.nr = nr;
}
EXPORT_SYMBOL_GPL(kvm_queue_exception);

void kvm_inject_page_fault(struct kvm_vcpu *vcpu, unsigned long addr,
			   u32 error_code)
{
	++vcpu->stat.pf_guest;

	if (vcpu->arch.exception.pending) {
		switch(vcpu->arch.exception.nr) {
		case DF_VECTOR:
			/* triple fault -> shutdown */
			set_bit(KVM_REQ_TRIPLE_FAULT, &vcpu->requests);
			return;
		case PF_VECTOR:
			vcpu->arch.exception.nr = DF_VECTOR;
			vcpu->arch.exception.error_code = 0;
			return;
		default:
			/* replace previous exception with a new one in a hope
			   that instruction re-execution will regenerate lost
			   exception */
			vcpu->arch.exception.pending = false;
			break;
		}
	}
	vcpu->arch.cr2 = addr;
	kvm_queue_exception_e(vcpu, PF_VECTOR, error_code);
}

void kvm_inject_nmi(struct kvm_vcpu *vcpu)
{
	vcpu->arch.nmi_pending = 1;
}
EXPORT_SYMBOL_GPL(kvm_inject_nmi);

void kvm_queue_exception_e(struct kvm_vcpu *vcpu, unsigned nr, u32 error_code)
{
	WARN_ON(vcpu->arch.exception.pending);
	vcpu->arch.exception.pending = true;
	vcpu->arch.exception.has_error_code = true;
	vcpu->arch.exception.nr = nr;
	vcpu->arch.exception.error_code = error_code;
}
EXPORT_SYMBOL_GPL(kvm_queue_exception_e);

/*
 * Checks if cpl <= required_cpl; if true, return true.  Otherwise queue
 * a #GP and return false.
 */
bool kvm_require_cpl(struct kvm_vcpu *vcpu, int required_cpl)
{
	if (kvm_x86_ops->get_cpl(vcpu) <= required_cpl)
		return true;
	kvm_queue_exception_e(vcpu, GP_VECTOR, 0);
	return false;
}
EXPORT_SYMBOL_GPL(kvm_require_cpl);

/*
 * Load the pae pdptrs.  Return true is they are all valid.
 */
int load_pdptrs(struct kvm_vcpu *vcpu, unsigned long cr3)
{
	gfn_t pdpt_gfn = cr3 >> PAGE_SHIFT;
	unsigned offset = ((cr3 & (PAGE_SIZE-1)) >> 5) << 2;
	int i;
	int ret;
	u64 pdpte[ARRAY_SIZE(vcpu->arch.pdptrs)];

	ret = kvm_read_guest_page(vcpu->kvm, pdpt_gfn, pdpte,
				  offset * sizeof(u64), sizeof(pdpte));
	if (ret < 0) {
		ret = 0;
		goto out;
	}
	for (i = 0; i < ARRAY_SIZE(pdpte); ++i) {
		if (is_present_gpte(pdpte[i]) &&
		    (pdpte[i] & vcpu->arch.mmu.rsvd_bits_mask[0][2])) {
			ret = 0;
			goto out;
		}
	}
	ret = 1;

	memcpy(vcpu->arch.pdptrs, pdpte, sizeof(vcpu->arch.pdptrs));
	__set_bit(VCPU_EXREG_PDPTR,
		  (unsigned long *)&vcpu->arch.regs_avail);
	__set_bit(VCPU_EXREG_PDPTR,
		  (unsigned long *)&vcpu->arch.regs_dirty);
out:

	return ret;
}
EXPORT_SYMBOL_GPL(load_pdptrs);

static bool pdptrs_changed(struct kvm_vcpu *vcpu)
{
	u64 pdpte[ARRAY_SIZE(vcpu->arch.pdptrs)];
	bool changed = true;
	int r;

	if (is_long_mode(vcpu) || !is_pae(vcpu))
		return false;

	if (!test_bit(VCPU_EXREG_PDPTR,
		      (unsigned long *)&vcpu->arch.regs_avail))
		return true;

	r = kvm_read_guest(vcpu->kvm, vcpu->arch.cr3 & ~31u, pdpte, sizeof(pdpte));
	if (r < 0)
		goto out;
	changed = memcmp(pdpte, vcpu->arch.pdptrs, sizeof(pdpte)) != 0;
out:

	return changed;
}

void kvm_set_cr0(struct kvm_vcpu *vcpu, unsigned long cr0)
{
	if (cr0 & CR0_RESERVED_BITS) {
		printk(KERN_DEBUG "set_cr0: 0x%lx #GP, reserved bits 0x%lx\n",
		       cr0, vcpu->arch.cr0);
		kvm_inject_gp(vcpu, 0);
		return;
	}

	if ((cr0 & X86_CR0_NW) && !(cr0 & X86_CR0_CD)) {
		printk(KERN_DEBUG "set_cr0: #GP, CD == 0 && NW == 1\n");
		kvm_inject_gp(vcpu, 0);
		return;
	}

	if ((cr0 & X86_CR0_PG) && !(cr0 & X86_CR0_PE)) {
		printk(KERN_DEBUG "set_cr0: #GP, set PG flag "
		       "and a clear PE flag\n");
		kvm_inject_gp(vcpu, 0);
		return;
	}

	if (!is_paging(vcpu) && (cr0 & X86_CR0_PG)) {
#ifdef CONFIG_X86_64
		if ((vcpu->arch.shadow_efer & EFER_LME)) {
			int cs_db, cs_l;

			if (!is_pae(vcpu)) {
				printk(KERN_DEBUG "set_cr0: #GP, start paging "
				       "in long mode while PAE is disabled\n");
				kvm_inject_gp(vcpu, 0);
				return;
			}
			kvm_x86_ops->get_cs_db_l_bits(vcpu, &cs_db, &cs_l);
			if (cs_l) {
				printk(KERN_DEBUG "set_cr0: #GP, start paging "
				       "in long mode while CS.L == 1\n");
				kvm_inject_gp(vcpu, 0);
				return;

			}
		} else
#endif
		if (is_pae(vcpu) && !load_pdptrs(vcpu, vcpu->arch.cr3)) {
			printk(KERN_DEBUG "set_cr0: #GP, pdptrs "
			       "reserved bits\n");
			kvm_inject_gp(vcpu, 0);
			return;
		}

	}

	kvm_x86_ops->set_cr0(vcpu, cr0);
	vcpu->arch.cr0 = cr0;

	kvm_mmu_reset_context(vcpu);
	return;
}
EXPORT_SYMBOL_GPL(kvm_set_cr0);

void kvm_lmsw(struct kvm_vcpu *vcpu, unsigned long msw)
{
	kvm_set_cr0(vcpu, (vcpu->arch.cr0 & ~0x0ful) | (msw & 0x0f));
}
EXPORT_SYMBOL_GPL(kvm_lmsw);

void kvm_set_cr4(struct kvm_vcpu *vcpu, unsigned long cr4)
{
	unsigned long old_cr4 = vcpu->arch.cr4;
	unsigned long pdptr_bits = X86_CR4_PGE | X86_CR4_PSE | X86_CR4_PAE;

	if (cr4 & CR4_RESERVED_BITS) {
		printk(KERN_DEBUG "set_cr4: #GP, reserved bits\n");
		kvm_inject_gp(vcpu, 0);
		return;
	}

	if (is_long_mode(vcpu)) {
		if (!(cr4 & X86_CR4_PAE)) {
			printk(KERN_DEBUG "set_cr4: #GP, clearing PAE while "
			       "in long mode\n");
			kvm_inject_gp(vcpu, 0);
			return;
		}
	} else if (is_paging(vcpu) && (cr4 & X86_CR4_PAE)
		   && ((cr4 ^ old_cr4) & pdptr_bits)
		   && !load_pdptrs(vcpu, vcpu->arch.cr3)) {
		printk(KERN_DEBUG "set_cr4: #GP, pdptrs reserved bits\n");
		kvm_inject_gp(vcpu, 0);
		return;
	}

	if (cr4 & X86_CR4_VMXE) {
		printk(KERN_DEBUG "set_cr4: #GP, setting VMXE\n");
		kvm_inject_gp(vcpu, 0);
		return;
	}
	kvm_x86_ops->set_cr4(vcpu, cr4);
	vcpu->arch.cr4 = cr4;
	vcpu->arch.mmu.base_role.cr4_pge = (cr4 & X86_CR4_PGE) && !tdp_enabled;
	kvm_mmu_reset_context(vcpu);
}
EXPORT_SYMBOL_GPL(kvm_set_cr4);

void kvm_set_cr3(struct kvm_vcpu *vcpu, unsigned long cr3)
{
	if (cr3 == vcpu->arch.cr3 && !pdptrs_changed(vcpu)) {
		kvm_mmu_sync_roots(vcpu);
		kvm_mmu_flush_tlb(vcpu);
		return;
	}

	if (is_long_mode(vcpu)) {
		if (cr3 & CR3_L_MODE_RESERVED_BITS) {
			printk(KERN_DEBUG "set_cr3: #GP, reserved bits\n");
			kvm_inject_gp(vcpu, 0);
			return;
		}
	} else {
		if (is_pae(vcpu)) {
			if (cr3 & CR3_PAE_RESERVED_BITS) {
				printk(KERN_DEBUG
				       "set_cr3: #GP, reserved bits\n");
				kvm_inject_gp(vcpu, 0);
				return;
			}
			if (is_paging(vcpu) && !load_pdptrs(vcpu, cr3)) {
				printk(KERN_DEBUG "set_cr3: #GP, pdptrs "
				       "reserved bits\n");
				kvm_inject_gp(vcpu, 0);
				return;
			}
		}
		/*
		 * We don't check reserved bits in nonpae mode, because
		 * this isn't enforced, and VMware depends on this.
		 */
	}

	/*
	 * Does the new cr3 value map to physical memory? (Note, we
	 * catch an invalid cr3 even in real-mode, because it would
	 * cause trouble later on when we turn on paging anyway.)
	 *
	 * A real CPU would silently accept an invalid cr3 and would
	 * attempt to use it - with largely undefined (and often hard
	 * to debug) behavior on the guest side.
	 */
	if (unlikely(!gfn_to_memslot(vcpu->kvm, cr3 >> PAGE_SHIFT)))
		kvm_inject_gp(vcpu, 0);
	else {
		vcpu->arch.cr3 = cr3;
		vcpu->arch.mmu.new_cr3(vcpu);
	}
}
EXPORT_SYMBOL_GPL(kvm_set_cr3);

void kvm_set_cr8(struct kvm_vcpu *vcpu, unsigned long cr8)
{
	if (cr8 & CR8_RESERVED_BITS) {
		printk(KERN_DEBUG "set_cr8: #GP, reserved bits 0x%lx\n", cr8);
		kvm_inject_gp(vcpu, 0);
		return;
	}
	if (irqchip_in_kernel(vcpu->kvm))
		kvm_lapic_set_tpr(vcpu, cr8);
	else
		vcpu->arch.cr8 = cr8;
}
EXPORT_SYMBOL_GPL(kvm_set_cr8);

unsigned long kvm_get_cr8(struct kvm_vcpu *vcpu)
{
	if (irqchip_in_kernel(vcpu->kvm))
		return kvm_lapic_get_cr8(vcpu);
	else
		return vcpu->arch.cr8;
}
EXPORT_SYMBOL_GPL(kvm_get_cr8);

static inline u32 bit(int bitno)
{
	return 1 << (bitno & 31);
}

/*
 * List of msr numbers which we expose to userspace through KVM_GET_MSRS
 * and KVM_SET_MSRS, and KVM_GET_MSR_INDEX_LIST.
 *
 * This list is modified at module load time to reflect the
 * capabilities of the host cpu. This capabilities test skips MSRs that are
 * kvm-specific. Those are put in the beginning of the list.
 */

#define KVM_SAVE_MSRS_BEGIN	2
static u32 msrs_to_save[] = {
	MSR_KVM_SYSTEM_TIME, MSR_KVM_WALL_CLOCK,
	MSR_IA32_SYSENTER_CS, MSR_IA32_SYSENTER_ESP, MSR_IA32_SYSENTER_EIP,
	MSR_K6_STAR,
#ifdef CONFIG_X86_64
	MSR_CSTAR, MSR_KERNEL_GS_BASE, MSR_SYSCALL_MASK, MSR_LSTAR,
#endif
	MSR_IA32_TSC, MSR_IA32_PERF_STATUS, MSR_IA32_CR_PAT, MSR_VM_HSAVE_PA
};

static unsigned num_msrs_to_save;

static u32 emulated_msrs[] = {
	MSR_IA32_MISC_ENABLE,
};

static void set_efer(struct kvm_vcpu *vcpu, u64 efer)
{
	if (efer & efer_reserved_bits) {
		printk(KERN_DEBUG "set_efer: 0x%llx #GP, reserved bits\n",
		       efer);
		kvm_inject_gp(vcpu, 0);
		return;
	}

	if (is_paging(vcpu)
	    && (vcpu->arch.shadow_efer & EFER_LME) != (efer & EFER_LME)) {
		printk(KERN_DEBUG "set_efer: #GP, change LME while paging\n");
		kvm_inject_gp(vcpu, 0);
		return;
	}

	if (efer & EFER_FFXSR) {
		struct kvm_cpuid_entry2 *feat;

		feat = kvm_find_cpuid_entry(vcpu, 0x80000001, 0);
		if (!feat || !(feat->edx & bit(X86_FEATURE_FXSR_OPT))) {
			printk(KERN_DEBUG "set_efer: #GP, enable FFXSR w/o CPUID capability\n");
			kvm_inject_gp(vcpu, 0);
			return;
		}
	}

	if (efer & EFER_SVME) {
		struct kvm_cpuid_entry2 *feat;

		feat = kvm_find_cpuid_entry(vcpu, 0x80000001, 0);
		if (!feat || !(feat->ecx & bit(X86_FEATURE_SVM))) {
			printk(KERN_DEBUG "set_efer: #GP, enable SVM w/o SVM\n");
			kvm_inject_gp(vcpu, 0);
			return;
		}
	}

	kvm_x86_ops->set_efer(vcpu, efer);

	efer &= ~EFER_LMA;
	efer |= vcpu->arch.shadow_efer & EFER_LMA;

	vcpu->arch.shadow_efer = efer;

	vcpu->arch.mmu.base_role.nxe = (efer & EFER_NX) && !tdp_enabled;
	kvm_mmu_reset_context(vcpu);
}

void kvm_enable_efer_bits(u64 mask)
{
       efer_reserved_bits &= ~mask;
}
EXPORT_SYMBOL_GPL(kvm_enable_efer_bits);


/*
 * Writes msr value into into the appropriate "register".
 * Returns 0 on success, non-0 otherwise.
 * Assumes vcpu_load() was already called.
 */
int kvm_set_msr(struct kvm_vcpu *vcpu, u32 msr_index, u64 data)
{
	return kvm_x86_ops->set_msr(vcpu, msr_index, data);
}

/*
 * Adapt set_msr() to msr_io()'s calling convention
 */
static int do_set_msr(struct kvm_vcpu *vcpu, unsigned index, u64 *data)
{
	return kvm_set_msr(vcpu, index, *data);
}

static void kvm_write_wall_clock(struct kvm *kvm, gpa_t wall_clock)
{
	static int version;
	struct pvclock_wall_clock wc;
	struct timespec now, sys, boot;

	if (!wall_clock)
		return;

	version++;

	kvm_write_guest(kvm, wall_clock, &version, sizeof(version));

	/*
	 * The guest calculates current wall clock time by adding
	 * system time (updated by kvm_write_guest_time below) to the
	 * wall clock specified here.  guest system time equals host
	 * system time for us, thus we must fill in host boot time here.
	 */
	now = current_kernel_time();
	ktime_get_ts(&sys);
	boot = ns_to_timespec(timespec_to_ns(&now) - timespec_to_ns(&sys));

	wc.sec = boot.tv_sec;
	wc.nsec = boot.tv_nsec;
	wc.version = version;

	kvm_write_guest(kvm, wall_clock, &wc, sizeof(wc));

	version++;
	kvm_write_guest(kvm, wall_clock, &version, sizeof(version));
}

static uint32_t div_frac(uint32_t dividend, uint32_t divisor)
{
	uint32_t quotient, remainder;

	/* Don't try to replace with do_div(), this one calculates
	 * "(dividend << 32) / divisor" */
	__asm__ ( "divl %4"
		  : "=a" (quotient), "=d" (remainder)
		  : "0" (0), "1" (dividend), "r" (divisor) );
	return quotient;
}

static void kvm_set_time_scale(uint32_t tsc_khz, struct pvclock_vcpu_time_info *hv_clock)
{
	uint64_t nsecs = 1000000000LL;
	int32_t  shift = 0;
	uint64_t tps64;
	uint32_t tps32;

	tps64 = tsc_khz * 1000LL;
	while (tps64 > nsecs*2) {
		tps64 >>= 1;
		shift--;
	}

	tps32 = (uint32_t)tps64;
	while (tps32 <= (uint32_t)nsecs) {
		tps32 <<= 1;
		shift++;
	}

	hv_clock->tsc_shift = shift;
	hv_clock->tsc_to_system_mul = div_frac(nsecs, tps32);

	pr_debug("%s: tsc_khz %u, tsc_shift %d, tsc_mul %u\n",
		 __func__, tsc_khz, hv_clock->tsc_shift,
		 hv_clock->tsc_to_system_mul);
}

static DEFINE_PER_CPU(unsigned long, cpu_tsc_khz);

static void kvm_write_guest_time(struct kvm_vcpu *v)
{
	struct timespec ts;
	unsigned long flags;
	struct kvm_vcpu_arch *vcpu = &v->arch;
	void *shared_kaddr;
	unsigned long this_tsc_khz;

	if ((!vcpu->time_page))
		return;

	this_tsc_khz = get_cpu_var(cpu_tsc_khz);
	if (unlikely(vcpu->hv_clock_tsc_khz != this_tsc_khz)) {
		kvm_set_time_scale(this_tsc_khz, &vcpu->hv_clock);
		vcpu->hv_clock_tsc_khz = this_tsc_khz;
	}
	put_cpu_var(cpu_tsc_khz);

	/* Keep irq disabled to prevent changes to the clock */
	local_irq_save(flags);
	kvm_get_msr(v, MSR_IA32_TSC, &vcpu->hv_clock.tsc_timestamp);
	ktime_get_ts(&ts);
	local_irq_restore(flags);

	/* With all the info we got, fill in the values */

	vcpu->hv_clock.system_time = ts.tv_nsec +
				     (NSEC_PER_SEC * (u64)ts.tv_sec);
	/*
	 * The interface expects us to write an even number signaling that the
	 * update is finished. Since the guest won't see the intermediate
	 * state, we just increase by 2 at the end.
	 */
	vcpu->hv_clock.version += 2;

	shared_kaddr = kmap_atomic(vcpu->time_page, KM_USER0);

	memcpy(shared_kaddr + vcpu->time_offset, &vcpu->hv_clock,
	       sizeof(vcpu->hv_clock));

	kunmap_atomic(shared_kaddr, KM_USER0);

	mark_page_dirty(v->kvm, vcpu->time >> PAGE_SHIFT);
}

static int kvm_request_guest_time_update(struct kvm_vcpu *v)
{
	struct kvm_vcpu_arch *vcpu = &v->arch;

	if (!vcpu->time_page)
		return 0;
	set_bit(KVM_REQ_KVMCLOCK_UPDATE, &v->requests);
	return 1;
}

static bool msr_mtrr_valid(unsigned msr)
{
	switch (msr) {
	case 0x200 ... 0x200 + 2 * KVM_NR_VAR_MTRR - 1:
	case MSR_MTRRfix64K_00000:
	case MSR_MTRRfix16K_80000:
	case MSR_MTRRfix16K_A0000:
	case MSR_MTRRfix4K_C0000:
	case MSR_MTRRfix4K_C8000:
	case MSR_MTRRfix4K_D0000:
	case MSR_MTRRfix4K_D8000:
	case MSR_MTRRfix4K_E0000:
	case MSR_MTRRfix4K_E8000:
	case MSR_MTRRfix4K_F0000:
	case MSR_MTRRfix4K_F8000:
	case MSR_MTRRdefType:
	case MSR_IA32_CR_PAT:
		return true;
	case 0x2f8:
		return true;
	}
	return false;
}

static bool valid_pat_type(unsigned t)
{
	return t < 8 && (1 << t) & 0xf3; /* 0, 1, 4, 5, 6, 7 */
}

static bool valid_mtrr_type(unsigned t)
{
	return t < 8 && (1 << t) & 0x73; /* 0, 1, 4, 5, 6 */
}

static bool mtrr_valid(struct kvm_vcpu *vcpu, u32 msr, u64 data)
{
	int i;

	if (!msr_mtrr_valid(msr))
		return false;

	if (msr == MSR_IA32_CR_PAT) {
		for (i = 0; i < 8; i++)
			if (!valid_pat_type((data >> (i * 8)) & 0xff))
				return false;
		return true;
	} else if (msr == MSR_MTRRdefType) {
		if (data & ~0xcff)
			return false;
		return valid_mtrr_type(data & 0xff);
	} else if (msr >= MSR_MTRRfix64K_00000 && msr <= MSR_MTRRfix4K_F8000) {
		for (i = 0; i < 8 ; i++)
			if (!valid_mtrr_type((data >> (i * 8)) & 0xff))
				return false;
		return true;
	}

	/* variable MTRRs */
	return valid_mtrr_type(data & 0xff);
}

static int set_msr_mtrr(struct kvm_vcpu *vcpu, u32 msr, u64 data)
{
	u64 *p = (u64 *)&vcpu->arch.mtrr_state.fixed_ranges;

	if (!mtrr_valid(vcpu, msr, data))
		return 1;

	if (msr == MSR_MTRRdefType) {
		vcpu->arch.mtrr_state.def_type = data;
		vcpu->arch.mtrr_state.enabled = (data & 0xc00) >> 10;
	} else if (msr == MSR_MTRRfix64K_00000)
		p[0] = data;
	else if (msr == MSR_MTRRfix16K_80000 || msr == MSR_MTRRfix16K_A0000)
		p[1 + msr - MSR_MTRRfix16K_80000] = data;
	else if (msr >= MSR_MTRRfix4K_C0000 && msr <= MSR_MTRRfix4K_F8000)
		p[3 + msr - MSR_MTRRfix4K_C0000] = data;
	else if (msr == MSR_IA32_CR_PAT)
		vcpu->arch.pat = data;
	else {	/* Variable MTRRs */
		int idx, is_mtrr_mask;
		u64 *pt;

		idx = (msr - 0x200) / 2;
		is_mtrr_mask = msr - 0x200 - 2 * idx;
		if (!is_mtrr_mask)
			pt =
			  (u64 *)&vcpu->arch.mtrr_state.var_ranges[idx].base_lo;
		else
			pt =
			  (u64 *)&vcpu->arch.mtrr_state.var_ranges[idx].mask_lo;
		*pt = data;
	}

	kvm_mmu_reset_context(vcpu);
	return 0;
}

static int set_msr_mce(struct kvm_vcpu *vcpu, u32 msr, u64 data)
{
	u64 mcg_cap = vcpu->arch.mcg_cap;
	unsigned bank_num = mcg_cap & 0xff;

	switch (msr) {
	case MSR_IA32_MCG_STATUS:
		vcpu->arch.mcg_status = data;
		break;
	case MSR_IA32_MCG_CTL:
		if (!(mcg_cap & MCG_CTL_P))
			return 1;
		if (data != 0 && data != ~(u64)0)
			return -1;
		vcpu->arch.mcg_ctl = data;
		break;
	default:
		if (msr >= MSR_IA32_MC0_CTL &&
		    msr < MSR_IA32_MC0_CTL + 4 * bank_num) {
			u32 offset = msr - MSR_IA32_MC0_CTL;
			/* only 0 or all 1s can be written to IA32_MCi_CTL */
			if ((offset & 0x3) == 0 &&
			    data != 0 && data != ~(u64)0)
				return -1;
			vcpu->arch.mce_banks[offset] = data;
			break;
		}
		return 1;
	}
	return 0;
}

int kvm_set_msr_common(struct kvm_vcpu *vcpu, u32 msr, u64 data)
{
	switch (msr) {
	case MSR_EFER:
		set_efer(vcpu, data);
		break;
	case MSR_K7_HWCR:
		data &= ~(u64)0x40;	/* ignore flush filter disable */
		if (data != 0) {
			pr_unimpl(vcpu, "unimplemented HWCR wrmsr: 0x%llx\n",
				data);
			return 1;
		}
		break;
	case MSR_FAM10H_MMIO_CONF_BASE:
		if (data != 0) {
			pr_unimpl(vcpu, "unimplemented MMIO_CONF_BASE wrmsr: "
				"0x%llx\n", data);
			return 1;
		}
		break;
	case MSR_AMD64_NB_CFG:
		break;
	case MSR_IA32_DEBUGCTLMSR:
		if (!data) {
			/* We support the non-activated case already */
			break;
		} else if (data & ~(DEBUGCTLMSR_LBR | DEBUGCTLMSR_BTF)) {
			/* Values other than LBR and BTF are vendor-specific,
			   thus reserved and should throw a #GP */
			return 1;
		}
		pr_unimpl(vcpu, "%s: MSR_IA32_DEBUGCTLMSR 0x%llx, nop\n",
			__func__, data);
		break;
	case MSR_IA32_UCODE_REV:
	case MSR_IA32_UCODE_WRITE:
	case MSR_VM_HSAVE_PA:
	case MSR_AMD64_PATCH_LOADER:
		break;
	case 0x200 ... 0x2ff:
		return set_msr_mtrr(vcpu, msr, data);
	case MSR_IA32_APICBASE:
		kvm_set_apic_base(vcpu, data);
		break;
	case APIC_BASE_MSR ... APIC_BASE_MSR + 0x3ff:
		return kvm_x2apic_msr_write(vcpu, msr, data);
	case MSR_IA32_MISC_ENABLE:
		vcpu->arch.ia32_misc_enable_msr = data;
		break;
	case MSR_KVM_WALL_CLOCK:
		vcpu->kvm->arch.wall_clock = data;
		kvm_write_wall_clock(vcpu->kvm, data);
		break;
	case MSR_KVM_SYSTEM_TIME: {
		if (vcpu->arch.time_page) {
			kvm_release_page_dirty(vcpu->arch.time_page);
			vcpu->arch.time_page = NULL;
		}

		vcpu->arch.time = data;

		/* we verify if the enable bit is set... */
		if (!(data & 1))
			break;

		/* ...but clean it before doing the actual write */
		vcpu->arch.time_offset = data & ~(PAGE_MASK | 1);

		vcpu->arch.time_page =
				gfn_to_page(vcpu->kvm, data >> PAGE_SHIFT);

		if (is_error_page(vcpu->arch.time_page)) {
			kvm_release_page_clean(vcpu->arch.time_page);
			vcpu->arch.time_page = NULL;
		}

		kvm_request_guest_time_update(vcpu);
		break;
	}
	case MSR_IA32_MCG_CTL:
	case MSR_IA32_MCG_STATUS:
	case MSR_IA32_MC0_CTL ... MSR_IA32_MC0_CTL + 4 * KVM_MAX_MCE_BANKS - 1:
		return set_msr_mce(vcpu, msr, data);

	/* Performance counters are not protected by a CPUID bit,
	 * so we should check all of them in the generic path for the sake of
	 * cross vendor migration.
	 * Writing a zero into the event select MSRs disables them,
	 * which we perfectly emulate ;-). Any other value should be at least
	 * reported, some guests depend on them.
	 */
	case MSR_P6_EVNTSEL0:
	case MSR_P6_EVNTSEL1:
	case MSR_K7_EVNTSEL0:
	case MSR_K7_EVNTSEL1:
	case MSR_K7_EVNTSEL2:
	case MSR_K7_EVNTSEL3:
		if (data != 0)
			pr_unimpl(vcpu, "unimplemented perfctr wrmsr: "
				"0x%x data 0x%llx\n", msr, data);
		break;
	/* at least RHEL 4 unconditionally writes to the perfctr registers,
	 * so we ignore writes to make it happy.
	 */
	case MSR_P6_PERFCTR0:
	case MSR_P6_PERFCTR1:
	case MSR_K7_PERFCTR0:
	case MSR_K7_PERFCTR1:
	case MSR_K7_PERFCTR2:
	case MSR_K7_PERFCTR3:
		pr_unimpl(vcpu, "unimplemented perfctr wrmsr: "
			"0x%x data 0x%llx\n", msr, data);
		break;
	default:
		if (!ignore_msrs) {
			pr_unimpl(vcpu, "unhandled wrmsr: 0x%x data %llx\n",
				msr, data);
			return 1;
		} else {
			pr_unimpl(vcpu, "ignored wrmsr: 0x%x data %llx\n",
				msr, data);
			break;
		}
	}
	return 0;
}
EXPORT_SYMBOL_GPL(kvm_set_msr_common);


/*
 * Reads an msr value (of 'msr_index') into 'pdata'.
 * Returns 0 on success, non-0 otherwise.
 * Assumes vcpu_load() was already called.
 */
int kvm_get_msr(struct kvm_vcpu *vcpu, u32 msr_index, u64 *pdata)
{
	return kvm_x86_ops->get_msr(vcpu, msr_index, pdata);
}

static int get_msr_mtrr(struct kvm_vcpu *vcpu, u32 msr, u64 *pdata)
{
	u64 *p = (u64 *)&vcpu->arch.mtrr_state.fixed_ranges;

	if (!msr_mtrr_valid(msr))
		return 1;

	if (msr == MSR_MTRRdefType)
		*pdata = vcpu->arch.mtrr_state.def_type +
			 (vcpu->arch.mtrr_state.enabled << 10);
	else if (msr == MSR_MTRRfix64K_00000)
		*pdata = p[0];
	else if (msr == MSR_MTRRfix16K_80000 || msr == MSR_MTRRfix16K_A0000)
		*pdata = p[1 + msr - MSR_MTRRfix16K_80000];
	else if (msr >= MSR_MTRRfix4K_C0000 && msr <= MSR_MTRRfix4K_F8000)
		*pdata = p[3 + msr - MSR_MTRRfix4K_C0000];
	else if (msr == MSR_IA32_CR_PAT)
		*pdata = vcpu->arch.pat;
	else {	/* Variable MTRRs */
		int idx, is_mtrr_mask;
		u64 *pt;

		idx = (msr - 0x200) / 2;
		is_mtrr_mask = msr - 0x200 - 2 * idx;
		if (!is_mtrr_mask)
			pt =
			  (u64 *)&vcpu->arch.mtrr_state.var_ranges[idx].base_lo;
		else
			pt =
			  (u64 *)&vcpu->arch.mtrr_state.var_ranges[idx].mask_lo;
		*pdata = *pt;
	}

	return 0;
}

static int get_msr_mce(struct kvm_vcpu *vcpu, u32 msr, u64 *pdata)
{
	u64 data;
	u64 mcg_cap = vcpu->arch.mcg_cap;
	unsigned bank_num = mcg_cap & 0xff;

	switch (msr) {
	case MSR_IA32_P5_MC_ADDR:
	case MSR_IA32_P5_MC_TYPE:
		data = 0;
		break;
	case MSR_IA32_MCG_CAP:
		data = vcpu->arch.mcg_cap;
		break;
	case MSR_IA32_MCG_CTL:
		if (!(mcg_cap & MCG_CTL_P))
			return 1;
		data = vcpu->arch.mcg_ctl;
		break;
	case MSR_IA32_MCG_STATUS:
		data = vcpu->arch.mcg_status;
		break;
	default:
		if (msr >= MSR_IA32_MC0_CTL &&
		    msr < MSR_IA32_MC0_CTL + 4 * bank_num) {
			u32 offset = msr - MSR_IA32_MC0_CTL;
			data = vcpu->arch.mce_banks[offset];
			break;
		}
		return 1;
	}
	*pdata = data;
	return 0;
}

int kvm_get_msr_common(struct kvm_vcpu *vcpu, u32 msr, u64 *pdata)
{
	u64 data;

	switch (msr) {
	case MSR_IA32_PLATFORM_ID:
	case MSR_IA32_UCODE_REV:
	case MSR_IA32_EBL_CR_POWERON:
	case MSR_IA32_DEBUGCTLMSR:
	case MSR_IA32_LASTBRANCHFROMIP:
	case MSR_IA32_LASTBRANCHTOIP:
	case MSR_IA32_LASTINTFROMIP:
	case MSR_IA32_LASTINTTOIP:
	case MSR_K8_SYSCFG:
	case MSR_K7_HWCR:
	case MSR_VM_HSAVE_PA:
	case MSR_P6_PERFCTR0:
	case MSR_P6_PERFCTR1:
	case MSR_P6_EVNTSEL0:
	case MSR_P6_EVNTSEL1:
	case MSR_K7_EVNTSEL0:
	case MSR_K7_PERFCTR0:
	case MSR_K8_INT_PENDING_MSG:
	case MSR_AMD64_NB_CFG:
	case MSR_FAM10H_MMIO_CONF_BASE:
		data = 0;
		break;
	case MSR_MTRRcap:
		data = 0x500 | KVM_NR_VAR_MTRR;
		break;
	case 0x200 ... 0x2ff:
		return get_msr_mtrr(vcpu, msr, pdata);
	case 0xcd: /* fsb frequency */
		data = 3;
		break;
	case MSR_IA32_APICBASE:
		data = kvm_get_apic_base(vcpu);
		break;
	case APIC_BASE_MSR ... APIC_BASE_MSR + 0x3ff:
		return kvm_x2apic_msr_read(vcpu, msr, pdata);
		break;
	case MSR_IA32_MISC_ENABLE:
		data = vcpu->arch.ia32_misc_enable_msr;
		break;
	case MSR_IA32_PERF_STATUS:
		/* TSC increment by tick */
		data = 1000ULL;
		/* CPU multiplier */
		data |= (((uint64_t)4ULL) << 40);
		break;
	case MSR_EFER:
		data = vcpu->arch.shadow_efer;
		break;
	case MSR_KVM_WALL_CLOCK:
		data = vcpu->kvm->arch.wall_clock;
		break;
	case MSR_KVM_SYSTEM_TIME:
		data = vcpu->arch.time;
		break;
	case MSR_IA32_P5_MC_ADDR:
	case MSR_IA32_P5_MC_TYPE:
	case MSR_IA32_MCG_CAP:
	case MSR_IA32_MCG_CTL:
	case MSR_IA32_MCG_STATUS:
	case MSR_IA32_MC0_CTL ... MSR_IA32_MC0_CTL + 4 * KVM_MAX_MCE_BANKS - 1:
		return get_msr_mce(vcpu, msr, pdata);
	default:
		if (!ignore_msrs) {
			pr_unimpl(vcpu, "unhandled rdmsr: 0x%x\n", msr);
			return 1;
		} else {
			pr_unimpl(vcpu, "ignored rdmsr: 0x%x\n", msr);
			data = 0;
		}
		break;
	}
	*pdata = data;
	return 0;
}
EXPORT_SYMBOL_GPL(kvm_get_msr_common);

/*
 * Read or write a bunch of msrs. All parameters are kernel addresses.
 *
 * @return number of msrs set successfully.
 */
static int __msr_io(struct kvm_vcpu *vcpu, struct kvm_msrs *msrs,
		    struct kvm_msr_entry *entries,
		    int (*do_msr)(struct kvm_vcpu *vcpu,
				  unsigned index, u64 *data))
{
	int i;

	vcpu_load(vcpu);

	down_read(&vcpu->kvm->slots_lock);
	for (i = 0; i < msrs->nmsrs; ++i)
		if (do_msr(vcpu, entries[i].index, &entries[i].data))
			break;
	up_read(&vcpu->kvm->slots_lock);

	vcpu_put(vcpu);

	return i;
}

/*
 * Read or write a bunch of msrs. Parameters are user addresses.
 *
 * @return number of msrs set successfully.
 */
static int msr_io(struct kvm_vcpu *vcpu, struct kvm_msrs __user *user_msrs,
		  int (*do_msr)(struct kvm_vcpu *vcpu,
				unsigned index, u64 *data),
		  int writeback)
{
	struct kvm_msrs msrs;
	struct kvm_msr_entry *entries;
	int r, n;
	unsigned size;

	r = -EFAULT;
	if (copy_from_user(&msrs, user_msrs, sizeof msrs))
		goto out;

	r = -E2BIG;
	if (msrs.nmsrs >= MAX_IO_MSRS)
		goto out;

	r = -ENOMEM;
	size = sizeof(struct kvm_msr_entry) * msrs.nmsrs;
	entries = vmalloc(size);
	if (!entries)
		goto out;

	r = -EFAULT;
	if (copy_from_user(entries, user_msrs->entries, size))
		goto out_free;

	r = n = __msr_io(vcpu, &msrs, entries, do_msr);
	if (r < 0)
		goto out_free;

	r = -EFAULT;
	if (writeback && copy_to_user(user_msrs->entries, entries, size))
		goto out_free;

	r = n;

out_free:
	vfree(entries);
out:
	return r;
}

int kvm_dev_ioctl_check_extension(long ext)
{
	int r;

	switch (ext) {
	case KVM_CAP_IRQCHIP:
	case KVM_CAP_HLT:
	case KVM_CAP_MMU_SHADOW_CACHE_CONTROL:
	case KVM_CAP_SET_TSS_ADDR:
	case KVM_CAP_EXT_CPUID:
	case KVM_CAP_CLOCKSOURCE:
	case KVM_CAP_PIT:
	case KVM_CAP_NOP_IO_DELAY:
	case KVM_CAP_MP_STATE:
	case KVM_CAP_SYNC_MMU:
	case KVM_CAP_REINJECT_CONTROL:
	case KVM_CAP_IRQ_INJECT_STATUS:
	case KVM_CAP_ASSIGN_DEV_IRQ:
	case KVM_CAP_IRQFD:
	case KVM_CAP_IOEVENTFD:
	case KVM_CAP_PIT2:
	case KVM_CAP_PIT_STATE2:
	case KVM_CAP_SET_IDENTITY_MAP_ADDR:
		r = 1;
		break;
	case KVM_CAP_COALESCED_MMIO:
		r = KVM_COALESCED_MMIO_PAGE_OFFSET;
		break;
	case KVM_CAP_VAPIC:
		r = !kvm_x86_ops->cpu_has_accelerated_tpr();
		break;
	case KVM_CAP_NR_VCPUS:
		r = KVM_MAX_VCPUS;
		break;
	case KVM_CAP_NR_MEMSLOTS:
		r = KVM_MEMORY_SLOTS;
		break;
	case KVM_CAP_PV_MMU:
		r = !tdp_enabled;
		break;
	case KVM_CAP_IOMMU:
		r = iommu_found();
		break;
	case KVM_CAP_MCE:
		r = KVM_MAX_MCE_BANKS;
		break;
	default:
		r = 0;
		break;
	}
	return r;

}

long kvm_arch_dev_ioctl(struct file *filp,
			unsigned int ioctl, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	long r;

	switch (ioctl) {
	case KVM_GET_MSR_INDEX_LIST: {
		struct kvm_msr_list __user *user_msr_list = argp;
		struct kvm_msr_list msr_list;
		unsigned n;

		r = -EFAULT;
		if (copy_from_user(&msr_list, user_msr_list, sizeof msr_list))
			goto out;
		n = msr_list.nmsrs;
		msr_list.nmsrs = num_msrs_to_save + ARRAY_SIZE(emulated_msrs);
		if (copy_to_user(user_msr_list, &msr_list, sizeof msr_list))
			goto out;
		r = -E2BIG;
		if (n < msr_list.nmsrs)
			goto out;
		r = -EFAULT;
		if (copy_to_user(user_msr_list->indices, &msrs_to_save,
				 num_msrs_to_save * sizeof(u32)))
			goto out;
		if (copy_to_user(user_msr_list->indices + num_msrs_to_save,
				 &emulated_msrs,
				 ARRAY_SIZE(emulated_msrs) * sizeof(u32)))
			goto out;
		r = 0;
		break;
	}
	case KVM_GET_SUPPORTED_CPUID: {
		struct kvm_cpuid2 __user *cpuid_arg = argp;
		struct kvm_cpuid2 cpuid;

		r = -EFAULT;
		if (copy_from_user(&cpuid, cpuid_arg, sizeof cpuid))
			goto out;
		r = kvm_dev_ioctl_get_supported_cpuid(&cpuid,
						      cpuid_arg->entries);
		if (r)
			goto out;

		r = -EFAULT;
		if (copy_to_user(cpuid_arg, &cpuid, sizeof cpuid))
			goto out;
		r = 0;
		break;
	}
	case KVM_X86_GET_MCE_CAP_SUPPORTED: {
		u64 mce_cap;

		mce_cap = KVM_MCE_CAP_SUPPORTED;
		r = -EFAULT;
		if (copy_to_user(argp, &mce_cap, sizeof mce_cap))
			goto out;
		r = 0;
		break;
	}
	default:
		r = -EINVAL;
	}
out:
	return r;
}

void kvm_arch_vcpu_load(struct kvm_vcpu *vcpu, int cpu)
{
	kvm_x86_ops->vcpu_load(vcpu, cpu);
	kvm_request_guest_time_update(vcpu);
}

void kvm_arch_vcpu_put(struct kvm_vcpu *vcpu)
{
	kvm_x86_ops->vcpu_put(vcpu);
	kvm_put_guest_fpu(vcpu);
}

static int is_efer_nx(void)
{
	unsigned long long efer = 0;

	rdmsrl_safe(MSR_EFER, &efer);
	return efer & EFER_NX;
}

static void cpuid_fix_nx_cap(struct kvm_vcpu *vcpu)
{
	int i;
	struct kvm_cpuid_entry2 *e, *entry;

	entry = NULL;
	for (i = 0; i < vcpu->arch.cpuid_nent; ++i) {
		e = &vcpu->arch.cpuid_entries[i];
		if (e->function == 0x80000001) {
			entry = e;
			break;
		}
	}
	if (entry && (entry->edx & (1 << 20)) && !is_efer_nx()) {
		entry->edx &= ~(1 << 20);
		printk(KERN_INFO "kvm: guest NX capability removed\n");
	}
}

/* when an old userspace process fills a new kernel module */
static int kvm_vcpu_ioctl_set_cpuid(struct kvm_vcpu *vcpu,
				    struct kvm_cpuid *cpuid,
				    struct kvm_cpuid_entry __user *entries)
{
	int r, i;
	struct kvm_cpuid_entry *cpuid_entries;

	r = -E2BIG;
	if (cpuid->nent > KVM_MAX_CPUID_ENTRIES)
		goto out;
	r = -ENOMEM;
	cpuid_entries = vmalloc(sizeof(struct kvm_cpuid_entry) * cpuid->nent);
	if (!cpuid_entries)
		goto out;
	r = -EFAULT;
	if (copy_from_user(cpuid_entries, entries,
			   cpuid->nent * sizeof(struct kvm_cpuid_entry)))
		goto out_free;
	for (i = 0; i < cpuid->nent; i++) {
		vcpu->arch.cpuid_entries[i].function = cpuid_entries[i].function;
		vcpu->arch.cpuid_entries[i].eax = cpuid_entries[i].eax;
		vcpu->arch.cpuid_entries[i].ebx = cpuid_entries[i].ebx;
		vcpu->arch.cpuid_entries[i].ecx = cpuid_entries[i].ecx;
		vcpu->arch.cpuid_entries[i].edx = cpuid_entries[i].edx;
		vcpu->arch.cpuid_entries[i].index = 0;
		vcpu->arch.cpuid_entries[i].flags = 0;
		vcpu->arch.cpuid_entries[i].padding[0] = 0;
		vcpu->arch.cpuid_entries[i].padding[1] = 0;
		vcpu->arch.cpuid_entries[i].padding[2] = 0;
	}
	vcpu->arch.cpuid_nent = cpuid->nent;
	cpuid_fix_nx_cap(vcpu);
	r = 0;
	kvm_apic_set_version(vcpu);

out_free:
	vfree(cpuid_entries);
out:
	return r;
}

static int kvm_vcpu_ioctl_set_cpuid2(struct kvm_vcpu *vcpu,
				     struct kvm_cpuid2 *cpuid,
				     struct kvm_cpuid_entry2 __user *entries)
{
	int r;

	r = -E2BIG;
	if (cpuid->nent > KVM_MAX_CPUID_ENTRIES)
		goto out;
	r = -EFAULT;
	if (copy_from_user(&vcpu->arch.cpuid_entries, entries,
			   cpuid->nent * sizeof(struct kvm_cpuid_entry2)))
		goto out;
	vcpu->arch.cpuid_nent = cpuid->nent;
	kvm_apic_set_version(vcpu);
	return 0;

out:
	return r;
}

static int kvm_vcpu_ioctl_get_cpuid2(struct kvm_vcpu *vcpu,
				     struct kvm_cpuid2 *cpuid,
				     struct kvm_cpuid_entry2 __user *entries)
{
	int r;

	r = -E2BIG;
	if (cpuid->nent < vcpu->arch.cpuid_nent)
		goto out;
	r = -EFAULT;
	if (copy_to_user(entries, &vcpu->arch.cpuid_entries,
			 vcpu->arch.cpuid_nent * sizeof(struct kvm_cpuid_entry2)))
		goto out;
	return 0;

out:
	cpuid->nent = vcpu->arch.cpuid_nent;
	return r;
}

static void do_cpuid_1_ent(struct kvm_cpuid_entry2 *entry, u32 function,
			   u32 index)
{
	entry->function = function;
	entry->index = index;
	cpuid_count(entry->function, entry->index,
		    &entry->eax, &entry->ebx, &entry->ecx, &entry->edx);
	entry->flags = 0;
}

#define F(x) bit(X86_FEATURE_##x)

static void do_cpuid_ent(struct kvm_cpuid_entry2 *entry, u32 function,
			 u32 index, int *nent, int maxnent)
{
	unsigned f_nx = is_efer_nx() ? F(NX) : 0;
	unsigned f_gbpages = kvm_x86_ops->gb_page_enable() ? F(GBPAGES) : 0;
#ifdef CONFIG_X86_64
	unsigned f_lm = F(LM);
#else
	unsigned f_lm = 0;
#endif

	/* cpuid 1.edx */
	const u32 kvm_supported_word0_x86_features =
		F(FPU) | F(VME) | F(DE) | F(PSE) |
		F(TSC) | F(MSR) | F(PAE) | F(MCE) |
		F(CX8) | F(APIC) | 0 /* Reserved */ | F(SEP) |
		F(MTRR) | F(PGE) | F(MCA) | F(CMOV) |
		F(PAT) | F(PSE36) | 0 /* PSN */ | F(CLFLSH) |
		0 /* Reserved, DS, ACPI */ | F(MMX) |
		F(FXSR) | F(XMM) | F(XMM2) | F(SELFSNOOP) |
		0 /* HTT, TM, Reserved, PBE */;
	/* cpuid 0x80000001.edx */
	const u32 kvm_supported_word1_x86_features =
		F(FPU) | F(VME) | F(DE) | F(PSE) |
		F(TSC) | F(MSR) | F(PAE) | F(MCE) |
		F(CX8) | F(APIC) | 0 /* Reserved */ | F(SYSCALL) |
		F(MTRR) | F(PGE) | F(MCA) | F(CMOV) |
		F(PAT) | F(PSE36) | 0 /* Reserved */ |
		f_nx | 0 /* Reserved */ | F(MMXEXT) | F(MMX) |
		F(FXSR) | F(FXSR_OPT) | f_gbpages | 0 /* RDTSCP */ |
		0 /* Reserved */ | f_lm | F(3DNOWEXT) | F(3DNOW);
	/* cpuid 1.ecx */
	const u32 kvm_supported_word4_x86_features =
		F(XMM3) | 0 /* Reserved, DTES64, MONITOR */ |
		0 /* DS-CPL, VMX, SMX, EST */ |
		0 /* TM2 */ | F(SSSE3) | 0 /* CNXT-ID */ | 0 /* Reserved */ |
		0 /* Reserved */ | F(CX16) | 0 /* xTPR Update, PDCM */ |
		0 /* Reserved, DCA */ | F(XMM4_1) |
		F(XMM4_2) | F(X2APIC) | F(MOVBE) | F(POPCNT) |
		0 /* Reserved, XSAVE, OSXSAVE */;
	/* cpuid 0x80000001.ecx */
	const u32 kvm_supported_word6_x86_features =
		F(LAHF_LM) | F(CMP_LEGACY) | F(SVM) | 0 /* ExtApicSpace */ |
		F(CR8_LEGACY) | F(ABM) | F(SSE4A) | F(MISALIGNSSE) |
		F(3DNOWPREFETCH) | 0 /* OSVW */ | 0 /* IBS */ | F(SSE5) |
		0 /* SKINIT */ | 0 /* WDT */;

	/* all calls to cpuid_count() should be made on the same cpu */
	get_cpu();
	do_cpuid_1_ent(entry, function, index);
	++*nent;

	switch (function) {
	case 0:
		entry->eax = min(entry->eax, (u32)0xb);
		break;
	case 1:
		entry->edx &= kvm_supported_word0_x86_features;
		entry->ecx &= kvm_supported_word4_x86_features;
		/* we support x2apic emulation even if host does not support
		 * it since we emulate x2apic in software */
		entry->ecx |= F(X2APIC);
		break;
	/* function 2 entries are STATEFUL. That is, repeated cpuid commands
	 * may return different values. This forces us to get_cpu() before
	 * issuing the first command, and also to emulate this annoying behavior
	 * in kvm_emulate_cpuid() using KVM_CPUID_FLAG_STATE_READ_NEXT */
	case 2: {
		int t, times = entry->eax & 0xff;

		entry->flags |= KVM_CPUID_FLAG_STATEFUL_FUNC;
		entry->flags |= KVM_CPUID_FLAG_STATE_READ_NEXT;
		for (t = 1; t < times && *nent < maxnent; ++t) {
			do_cpuid_1_ent(&entry[t], function, 0);
			entry[t].flags |= KVM_CPUID_FLAG_STATEFUL_FUNC;
			++*nent;
		}
		break;
	}
	/* function 4 and 0xb have additional index. */
	case 4: {
		int i, cache_type;

		entry->flags |= KVM_CPUID_FLAG_SIGNIFCANT_INDEX;
		/* read more entries until cache_type is zero */
		for (i = 1; *nent < maxnent; ++i) {
			cache_type = entry[i - 1].eax & 0x1f;
			if (!cache_type)
				break;
			do_cpuid_1_ent(&entry[i], function, i);
			entry[i].flags |=
			       KVM_CPUID_FLAG_SIGNIFCANT_INDEX;
			++*nent;
		}
		break;
	}
	case 0xb: {
		int i, level_type;

		entry->flags |= KVM_CPUID_FLAG_SIGNIFCANT_INDEX;
		/* read more entries until level_type is zero */
		for (i = 1; *nent < maxnent; ++i) {
			level_type = entry[i - 1].ecx & 0xff00;
			if (!level_type)
				break;
			do_cpuid_1_ent(&entry[i], function, i);
			entry[i].flags |=
			       KVM_CPUID_FLAG_SIGNIFCANT_INDEX;
			++*nent;
		}
		break;
	}
	case 0x80000000:
		entry->eax = min(entry->eax, 0x8000001a);
		break;
	case 0x80000001:
		entry->edx &= kvm_supported_word1_x86_features;
		entry->ecx &= kvm_supported_word6_x86_features;
		break;
	}
	put_cpu();
}

#undef F

static int kvm_dev_ioctl_get_supported_cpuid(struct kvm_cpuid2 *cpuid,
				     struct kvm_cpuid_entry2 __user *entries)
{
	struct kvm_cpuid_entry2 *cpuid_entries;
	int limit, nent = 0, r = -E2BIG;
	u32 func;

	if (cpuid->nent < 1)
		goto out;
	if (cpuid->nent > KVM_MAX_CPUID_ENTRIES)
		cpuid->nent = KVM_MAX_CPUID_ENTRIES;
	r = -ENOMEM;
	cpuid_entries = vmalloc(sizeof(struct kvm_cpuid_entry2) * cpuid->nent);
	if (!cpuid_entries)
		goto out;

	do_cpuid_ent(&cpuid_entries[0], 0, 0, &nent, cpuid->nent);
	limit = cpuid_entries[0].eax;
	for (func = 1; func <= limit && nent < cpuid->nent; ++func)
		do_cpuid_ent(&cpuid_entries[nent], func, 0,
			     &nent, cpuid->nent);
	r = -E2BIG;
	if (nent >= cpuid->nent)
		goto out_free;

	do_cpuid_ent(&cpuid_entries[nent], 0x80000000, 0, &nent, cpuid->nent);
	limit = cpuid_entries[nent - 1].eax;
	for (func = 0x80000001; func <= limit && nent < cpuid->nent; ++func)
		do_cpuid_ent(&cpuid_entries[nent], func, 0,
			     &nent, cpuid->nent);
	r = -E2BIG;
	if (nent >= cpuid->nent)
		goto out_free;

	r = -EFAULT;
	if (copy_to_user(entries, cpuid_entries,
			 nent * sizeof(struct kvm_cpuid_entry2)))
		goto out_free;
	cpuid->nent = nent;
	r = 0;

out_free:
	vfree(cpuid_entries);
out:
	return r;
}

static int kvm_vcpu_ioctl_get_lapic(struct kvm_vcpu *vcpu,
				    struct kvm_lapic_state *s)
{
	vcpu_load(vcpu);
	memcpy(s->regs, vcpu->arch.apic->regs, sizeof *s);
	vcpu_put(vcpu);

	return 0;
}

static int kvm_vcpu_ioctl_set_lapic(struct kvm_vcpu *vcpu,
				    struct kvm_lapic_state *s)
{
	vcpu_load(vcpu);
	memcpy(vcpu->arch.apic->regs, s->regs, sizeof *s);
	kvm_apic_post_state_restore(vcpu);
	update_cr8_intercept(vcpu);
	vcpu_put(vcpu);

	return 0;
}

static int kvm_vcpu_ioctl_interrupt(struct kvm_vcpu *vcpu,
				    struct kvm_interrupt *irq)
{
	if (irq->irq < 0 || irq->irq >= 256)
		return -EINVAL;
	if (irqchip_in_kernel(vcpu->kvm))
		return -ENXIO;
	vcpu_load(vcpu);

	kvm_queue_interrupt(vcpu, irq->irq, false);

	vcpu_put(vcpu);

	return 0;
}

static int kvm_vcpu_ioctl_nmi(struct kvm_vcpu *vcpu)
{
	vcpu_load(vcpu);
	kvm_inject_nmi(vcpu);
	vcpu_put(vcpu);

	return 0;
}

static int vcpu_ioctl_tpr_access_reporting(struct kvm_vcpu *vcpu,
					   struct kvm_tpr_access_ctl *tac)
{
	if (tac->flags)
		return -EINVAL;
	vcpu->arch.tpr_access_reporting = !!tac->enabled;
	return 0;
}

static int kvm_vcpu_ioctl_x86_setup_mce(struct kvm_vcpu *vcpu,
					u64 mcg_cap)
{
	int r;
	unsigned bank_num = mcg_cap & 0xff, bank;

	r = -EINVAL;
	if (!bank_num || bank_num >= KVM_MAX_MCE_BANKS)
		goto out;
	if (mcg_cap & ~(KVM_MCE_CAP_SUPPORTED | 0xff | 0xff0000))
		goto out;
	r = 0;
	vcpu->arch.mcg_cap = mcg_cap;
	/* Init IA32_MCG_CTL to all 1s */
	if (mcg_cap & MCG_CTL_P)
		vcpu->arch.mcg_ctl = ~(u64)0;
	/* Init IA32_MCi_CTL to all 1s */
	for (bank = 0; bank < bank_num; bank++)
		vcpu->arch.mce_banks[bank*4] = ~(u64)0;
out:
	return r;
}

static int kvm_vcpu_ioctl_x86_set_mce(struct kvm_vcpu *vcpu,
				      struct kvm_x86_mce *mce)
{
	u64 mcg_cap = vcpu->arch.mcg_cap;
	unsigned bank_num = mcg_cap & 0xff;
	u64 *banks = vcpu->arch.mce_banks;

	if (mce->bank >= bank_num || !(mce->status & MCI_STATUS_VAL))
		return -EINVAL;
	/*
	 * if IA32_MCG_CTL is not all 1s, the uncorrected error
	 * reporting is disabled
	 */
	if ((mce->status & MCI_STATUS_UC) && (mcg_cap & MCG_CTL_P) &&
	    vcpu->arch.mcg_ctl != ~(u64)0)
		return 0;
	banks += 4 * mce->bank;
	/*
	 * if IA32_MCi_CTL is not all 1s, the uncorrected error
	 * reporting is disabled for the bank
	 */
	if ((mce->status & MCI_STATUS_UC) && banks[0] != ~(u64)0)
		return 0;
	if (mce->status & MCI_STATUS_UC) {
		if ((vcpu->arch.mcg_status & MCG_STATUS_MCIP) ||
		    !(vcpu->arch.cr4 & X86_CR4_MCE)) {
			printk(KERN_DEBUG "kvm: set_mce: "
			       "injects mce exception while "
			       "previous one is in progress!\n");
			set_bit(KVM_REQ_TRIPLE_FAULT, &vcpu->requests);
			return 0;
		}
		if (banks[1] & MCI_STATUS_VAL)
			mce->status |= MCI_STATUS_OVER;
		banks[2] = mce->addr;
		banks[3] = mce->misc;
		vcpu->arch.mcg_status = mce->mcg_status;
		banks[1] = mce->status;
		kvm_queue_exception(vcpu, MC_VECTOR);
	} else if (!(banks[1] & MCI_STATUS_VAL)
		   || !(banks[1] & MCI_STATUS_UC)) {
		if (banks[1] & MCI_STATUS_VAL)
			mce->status |= MCI_STATUS_OVER;
		banks[2] = mce->addr;
		banks[3] = mce->misc;
		banks[1] = mce->status;
	} else
		banks[1] |= MCI_STATUS_OVER;
	return 0;
}

long kvm_arch_vcpu_ioctl(struct file *filp,
			 unsigned int ioctl, unsigned long arg)
{
	struct kvm_vcpu *vcpu = filp->private_data;
	void __user *argp = (void __user *)arg;
	int r;
	struct kvm_lapic_state *lapic = NULL;

	switch (ioctl) {
	case KVM_GET_LAPIC: {
		lapic = kzalloc(sizeof(struct kvm_lapic_state), GFP_KERNEL);

		r = -ENOMEM;
		if (!lapic)
			goto out;
		r = kvm_vcpu_ioctl_get_lapic(vcpu, lapic);
		if (r)
			goto out;
		r = -EFAULT;
		if (copy_to_user(argp, lapic, sizeof(struct kvm_lapic_state)))
			goto out;
		r = 0;
		break;
	}
	case KVM_SET_LAPIC: {
		lapic = kmalloc(sizeof(struct kvm_lapic_state), GFP_KERNEL);
		r = -ENOMEM;
		if (!lapic)
			goto out;
		r = -EFAULT;
		if (copy_from_user(lapic, argp, sizeof(struct kvm_lapic_state)))
			goto out;
		r = kvm_vcpu_ioctl_set_lapic(vcpu, lapic);
		if (r)
			goto out;
		r = 0;
		break;
	}
	case KVM_INTERRUPT: {
		struct kvm_interrupt irq;

		r = -EFAULT;
		if (copy_from_user(&irq, argp, sizeof irq))
			goto out;
		r = kvm_vcpu_ioctl_interrupt(vcpu, &irq);
		if (r)
			goto out;
		r = 0;
		break;
	}
	case KVM_NMI: {
		r = kvm_vcpu_ioctl_nmi(vcpu);
		if (r)
			goto out;
		r = 0;
		break;
	}
	case KVM_SET_CPUID: {
		struct kvm_cpuid __user *cpuid_arg = argp;
		struct kvm_cpuid cpuid;

		r = -EFAULT;
		if (copy_from_user(&cpuid, cpuid_arg, sizeof cpuid))
			goto out;
		r = kvm_vcpu_ioctl_set_cpuid(vcpu, &cpuid, cpuid_arg->entries);
		if (r)
			goto out;
		break;
	}
	case KVM_SET_CPUID2: {
		struct kvm_cpuid2 __user *cpuid_arg = argp;
		struct kvm_cpuid2 cpuid;

		r = -EFAULT;
		if (copy_from_user(&cpuid, cpuid_arg, sizeof cpuid))
			goto out;
		r = kvm_vcpu_ioctl_set_cpuid2(vcpu, &cpuid,
					      cpuid_arg->entries);
		if (r)
			goto out;
		break;
	}
	case KVM_GET_CPUID2: {
		struct kvm_cpuid2 __user *cpuid_arg = argp;
		struct kvm_cpuid2 cpuid;

		r = -EFAULT;
		if (copy_from_user(&cpuid, cpuid_arg, sizeof cpuid))
			goto out;
		r = kvm_vcpu_ioctl_get_cpuid2(vcpu, &cpuid,
					      cpuid_arg->entries);
		if (r)
			goto out;
		r = -EFAULT;
		if (copy_to_user(cpuid_arg, &cpuid, sizeof cpuid))
			goto out;
		r = 0;
		break;
	}
	case KVM_GET_MSRS:
		r = msr_io(vcpu, argp, kvm_get_msr, 1);
		break;
	case KVM_SET_MSRS:
		r = msr_io(vcpu, argp, do_set_msr, 0);
		break;
	case KVM_TPR_ACCESS_REPORTING: {
		struct kvm_tpr_access_ctl tac;

		r = -EFAULT;
		if (copy_from_user(&tac, argp, sizeof tac))
			goto out;
		r = vcpu_ioctl_tpr_access_reporting(vcpu, &tac);
		if (r)
			goto out;
		r = -EFAULT;
		if (copy_to_user(argp, &tac, sizeof tac))
			goto out;
		r = 0;
		break;
	};
	case KVM_SET_VAPIC_ADDR: {
		struct kvm_vapic_addr va;

		r = -EINVAL;
		if (!irqchip_in_kernel(vcpu->kvm))
			goto out;
		r = -EFAULT;
		if (copy_from_user(&va, argp, sizeof va))
			goto out;
		r = 0;
		kvm_lapic_set_vapic_addr(vcpu, va.vapic_addr);
		break;
	}
	case KVM_X86_SETUP_MCE: {
		u64 mcg_cap;

		r = -EFAULT;
		if (copy_from_user(&mcg_cap, argp, sizeof mcg_cap))
			goto out;
		r = kvm_vcpu_ioctl_x86_setup_mce(vcpu, mcg_cap);
		break;
	}
	case KVM_X86_SET_MCE: {
		struct kvm_x86_mce mce;

		r = -EFAULT;
		if (copy_from_user(&mce, argp, sizeof mce))
			goto out;
		r = kvm_vcpu_ioctl_x86_set_mce(vcpu, &mce);
		break;
	}
	default:
		r = -EINVAL;
	}
out:
	kfree(lapic);
	return r;
}

static int kvm_vm_ioctl_set_tss_addr(struct kvm *kvm, unsigned long addr)
{
	int ret;

	if (addr > (unsigned int)(-3 * PAGE_SIZE))
		return -1;
	ret = kvm_x86_ops->set_tss_addr(kvm, addr);
	return ret;
}

static int kvm_vm_ioctl_set_identity_map_addr(struct kvm *kvm,
					      u64 ident_addr)
{
	kvm->arch.ept_identity_map_addr = ident_addr;
	return 0;
}

static int kvm_vm_ioctl_set_nr_mmu_pages(struct kvm *kvm,
					  u32 kvm_nr_mmu_pages)
{
	if (kvm_nr_mmu_pages < KVM_MIN_ALLOC_MMU_PAGES)
		return -EINVAL;

	down_write(&kvm->slots_lock);
	spin_lock(&kvm->mmu_lock);

	kvm_mmu_change_mmu_pages(kvm, kvm_nr_mmu_pages);
	kvm->arch.n_requested_mmu_pages = kvm_nr_mmu_pages;

	spin_unlock(&kvm->mmu_lock);
	up_write(&kvm->slots_lock);
	return 0;
}

static int kvm_vm_ioctl_get_nr_mmu_pages(struct kvm *kvm)
{
	return kvm->arch.n_alloc_mmu_pages;
}

gfn_t unalias_gfn(struct kvm *kvm, gfn_t gfn)
{
	int i;
	struct kvm_mem_alias *alias;

	for (i = 0; i < kvm->arch.naliases; ++i) {
		alias = &kvm->arch.aliases[i];
		if (gfn >= alias->base_gfn
		    && gfn < alias->base_gfn + alias->npages)
			return alias->target_gfn + gfn - alias->base_gfn;
	}
	return gfn;
}

/*
 * Set a new alias region.  Aliases map a portion of physical memory into
 * another portion.  This is useful for memory windows, for example the PC
 * VGA region.
 */
static int kvm_vm_ioctl_set_memory_alias(struct kvm *kvm,
					 struct kvm_memory_alias *alias)
{
	int r, n;
	struct kvm_mem_alias *p;

	r = -EINVAL;
	/* General sanity checks */
	if (alias->memory_size & (PAGE_SIZE - 1))
		goto out;
	if (alias->guest_phys_addr & (PAGE_SIZE - 1))
		goto out;
	if (alias->slot >= KVM_ALIAS_SLOTS)
		goto out;
	if (alias->guest_phys_addr + alias->memory_size
	    < alias->guest_phys_addr)
		goto out;
	if (alias->target_phys_addr + alias->memory_size
	    < alias->target_phys_addr)
		goto out;

	down_write(&kvm->slots_lock);
	spin_lock(&kvm->mmu_lock);

	p = &kvm->arch.aliases[alias->slot];
	p->base_gfn = alias->guest_phys_addr >> PAGE_SHIFT;
	p->npages = alias->memory_size >> PAGE_SHIFT;
	p->target_gfn = alias->target_phys_addr >> PAGE_SHIFT;

	for (n = KVM_ALIAS_SLOTS; n > 0; --n)
		if (kvm->arch.aliases[n - 1].npages)
			break;
	kvm->arch.naliases = n;

	spin_unlock(&kvm->mmu_lock);
	kvm_mmu_zap_all(kvm);

	up_write(&kvm->slots_lock);

	return 0;

out:
	return r;
}

static int kvm_vm_ioctl_get_irqchip(struct kvm *kvm, struct kvm_irqchip *chip)
{
	int r;

	r = 0;
	switch (chip->chip_id) {
	case KVM_IRQCHIP_PIC_MASTER:
		memcpy(&chip->chip.pic,
			&pic_irqchip(kvm)->pics[0],
			sizeof(struct kvm_pic_state));
		break;
	case KVM_IRQCHIP_PIC_SLAVE:
		memcpy(&chip->chip.pic,
			&pic_irqchip(kvm)->pics[1],
			sizeof(struct kvm_pic_state));
		break;
	case KVM_IRQCHIP_IOAPIC:
		memcpy(&chip->chip.ioapic,
			ioapic_irqchip(kvm),
			sizeof(struct kvm_ioapic_state));
		break;
	default:
		r = -EINVAL;
		break;
	}
	return r;
}

static int kvm_vm_ioctl_set_irqchip(struct kvm *kvm, struct kvm_irqchip *chip)
{
	int r;

	r = 0;
	switch (chip->chip_id) {
	case KVM_IRQCHIP_PIC_MASTER:
		spin_lock(&pic_irqchip(kvm)->lock);
		memcpy(&pic_irqchip(kvm)->pics[0],
			&chip->chip.pic,
			sizeof(struct kvm_pic_state));
		spin_unlock(&pic_irqchip(kvm)->lock);
		break;
	case KVM_IRQCHIP_PIC_SLAVE:
		spin_lock(&pic_irqchip(kvm)->lock);
		memcpy(&pic_irqchip(kvm)->pics[1],
			&chip->chip.pic,
			sizeof(struct kvm_pic_state));
		spin_unlock(&pic_irqchip(kvm)->lock);
		break;
	case KVM_IRQCHIP_IOAPIC:
		mutex_lock(&kvm->irq_lock);
		memcpy(ioapic_irqchip(kvm),
			&chip->chip.ioapic,
			sizeof(struct kvm_ioapic_state));
		mutex_unlock(&kvm->irq_lock);
		break;
	default:
		r = -EINVAL;
		break;
	}
	kvm_pic_update_irq(pic_irqchip(kvm));
	return r;
}

static int kvm_vm_ioctl_get_pit(struct kvm *kvm, struct kvm_pit_state *ps)
{
	int r = 0;

	mutex_lock(&kvm->arch.vpit->pit_state.lock);
	memcpy(ps, &kvm->arch.vpit->pit_state, sizeof(struct kvm_pit_state));
	mutex_unlock(&kvm->arch.vpit->pit_state.lock);
	return r;
}

static int kvm_vm_ioctl_set_pit(struct kvm *kvm, struct kvm_pit_state *ps)
{
	int r = 0;

	mutex_lock(&kvm->arch.vpit->pit_state.lock);
	memcpy(&kvm->arch.vpit->pit_state, ps, sizeof(struct kvm_pit_state));
	kvm_pit_load_count(kvm, 0, ps->channels[0].count, 0);
	mutex_unlock(&kvm->arch.vpit->pit_state.lock);
	return r;
}

static int kvm_vm_ioctl_get_pit2(struct kvm *kvm, struct kvm_pit_state2 *ps)
{
	int r = 0;

	mutex_lock(&kvm->arch.vpit->pit_state.lock);
	memcpy(ps->channels, &kvm->arch.vpit->pit_state.channels,
		sizeof(ps->channels));
	ps->flags = kvm->arch.vpit->pit_state.flags;
	mutex_unlock(&kvm->arch.vpit->pit_state.lock);
	return r;
}

static int kvm_vm_ioctl_set_pit2(struct kvm *kvm, struct kvm_pit_state2 *ps)
{
	int r = 0, start = 0;
	u32 prev_legacy, cur_legacy;
	mutex_lock(&kvm->arch.vpit->pit_state.lock);
	prev_legacy = kvm->arch.vpit->pit_state.flags & KVM_PIT_FLAGS_HPET_LEGACY;
	cur_legacy = ps->flags & KVM_PIT_FLAGS_HPET_LEGACY;
	if (!prev_legacy && cur_legacy)
		start = 1;
	memcpy(&kvm->arch.vpit->pit_state.channels, &ps->channels,
	       sizeof(kvm->arch.vpit->pit_state.channels));
	kvm->arch.vpit->pit_state.flags = ps->flags;
	kvm_pit_load_count(kvm, 0, kvm->arch.vpit->pit_state.channels[0].count, start);
	mutex_unlock(&kvm->arch.vpit->pit_state.lock);
	return r;
}

static int kvm_vm_ioctl_reinject(struct kvm *kvm,
				 struct kvm_reinject_control *control)
{
	if (!kvm->arch.vpit)
		return -ENXIO;
	mutex_lock(&kvm->arch.vpit->pit_state.lock);
	kvm->arch.vpit->pit_state.pit_timer.reinject = control->pit_reinject;
	mutex_unlock(&kvm->arch.vpit->pit_state.lock);
	return 0;
}

/*
 * Get (and clear) the dirty memory log for a memory slot.
 */
int kvm_vm_ioctl_get_dirty_log(struct kvm *kvm,
				      struct kvm_dirty_log *log)
{
	int r;
	int n;
	struct kvm_memory_slot *memslot;
	int is_dirty = 0;

	down_write(&kvm->slots_lock);

	r = kvm_get_dirty_log(kvm, log, &is_dirty);
	if (r)
		goto out;

	/* If nothing is dirty, don't bother messing with page tables. */
	if (is_dirty) {
		spin_lock(&kvm->mmu_lock);
		kvm_mmu_slot_remove_write_access(kvm, log->slot);
		spin_unlock(&kvm->mmu_lock);
		memslot = &kvm->memslots[log->slot];
		n = ALIGN(memslot->npages, BITS_PER_LONG) / 8;
		memset(memslot->dirty_bitmap, 0, n);
	}
	r = 0;
out:
	up_write(&kvm->slots_lock);
	return r;
}

long kvm_arch_vm_ioctl(struct file *filp,
		       unsigned int ioctl, unsigned long arg)
{
	struct kvm *kvm = filp->private_data;
	void __user *argp = (void __user *)arg;
	int r = -EINVAL;
	/*
	 * This union makes it completely explicit to gcc-3.x
	 * that these two variables' stack usage should be
	 * combined, not added together.
	 */
	union {
		struct kvm_pit_state ps;
		struct kvm_pit_state2 ps2;
		struct kvm_memory_alias alias;
		struct kvm_pit_config pit_config;
	} u;

	switch (ioctl) {
	case KVM_SET_TSS_ADDR:
		r = kvm_vm_ioctl_set_tss_addr(kvm, arg);
		if (r < 0)
			goto out;
		break;
	case KVM_SET_IDENTITY_MAP_ADDR: {
		u64 ident_addr;

		r = -EFAULT;
		if (copy_from_user(&ident_addr, argp, sizeof ident_addr))
			goto out;
		r = kvm_vm_ioctl_set_identity_map_addr(kvm, ident_addr);
		if (r < 0)
			goto out;
		break;
	}
	case KVM_SET_MEMORY_REGION: {
		struct kvm_memory_region kvm_mem;
		struct kvm_userspace_memory_region kvm_userspace_mem;

		r = -EFAULT;
		if (copy_from_user(&kvm_mem, argp, sizeof kvm_mem))
			goto out;
		kvm_userspace_mem.slot = kvm_mem.slot;
		kvm_userspace_mem.flags = kvm_mem.flags;
		kvm_userspace_mem.guest_phys_addr = kvm_mem.guest_phys_addr;
		kvm_userspace_mem.memory_size = kvm_mem.memory_size;
		r = kvm_vm_ioctl_set_memory_region(kvm, &kvm_userspace_mem, 0);
		if (r)
			goto out;
		break;
	}
	case KVM_SET_NR_MMU_PAGES:
		r = kvm_vm_ioctl_set_nr_mmu_pages(kvm, arg);
		if (r)
			goto out;
		break;
	case KVM_GET_NR_MMU_PAGES:
		r = kvm_vm_ioctl_get_nr_mmu_pages(kvm);
		break;
	case KVM_SET_MEMORY_ALIAS:
		r = -EFAULT;
		if (copy_from_user(&u.alias, argp, sizeof(struct kvm_memory_alias)))
			goto out;
		r = kvm_vm_ioctl_set_memory_alias(kvm, &u.alias);
		if (r)
			goto out;
		break;
	case KVM_CREATE_IRQCHIP:
		r = -ENOMEM;
		kvm->arch.vpic = kvm_create_pic(kvm);
		if (kvm->arch.vpic) {
			r = kvm_ioapic_init(kvm);
			if (r) {
				kfree(kvm->arch.vpic);
				kvm->arch.vpic = NULL;
				goto out;
			}
		} else
			goto out;
		r = kvm_setup_default_irq_routing(kvm);
		if (r) {
			kfree(kvm->arch.vpic);
			kfree(kvm->arch.vioapic);
			goto out;
		}
		break;
	case KVM_CREATE_PIT:
		u.pit_config.flags = KVM_PIT_SPEAKER_DUMMY;
		goto create_pit;
	case KVM_CREATE_PIT2:
		r = -EFAULT;
		if (copy_from_user(&u.pit_config, argp,
				   sizeof(struct kvm_pit_config)))
			goto out;
	create_pit:
		down_write(&kvm->slots_lock);
		r = -EEXIST;
		if (kvm->arch.vpit)
			goto create_pit_unlock;
		r = -ENOMEM;
		kvm->arch.vpit = kvm_create_pit(kvm, u.pit_config.flags);
		if (kvm->arch.vpit)
			r = 0;
	create_pit_unlock:
		up_write(&kvm->slots_lock);
		break;
	case KVM_IRQ_LINE_STATUS:
	case KVM_IRQ_LINE: {
		struct kvm_irq_level irq_event;

		r = -EFAULT;
		if (copy_from_user(&irq_event, argp, sizeof irq_event))
			goto out;
		if (irqchip_in_kernel(kvm)) {
			__s32 status;
			mutex_lock(&kvm->irq_lock);
			status = kvm_set_irq(kvm, KVM_USERSPACE_IRQ_SOURCE_ID,
					irq_event.irq, irq_event.level);
			mutex_unlock(&kvm->irq_lock);
			if (ioctl == KVM_IRQ_LINE_STATUS) {
				irq_event.status = status;
				if (copy_to_user(argp, &irq_event,
							sizeof irq_event))
					goto out;
			}
			r = 0;
		}
		break;
	}
	case KVM_GET_IRQCHIP: {
		/* 0: PIC master, 1: PIC slave, 2: IOAPIC */
		struct kvm_irqchip *chip = kmalloc(sizeof(*chip), GFP_KERNEL);

		r = -ENOMEM;
		if (!chip)
			goto out;
		r = -EFAULT;
		if (copy_from_user(chip, argp, sizeof *chip))
			goto get_irqchip_out;
		r = -ENXIO;
		if (!irqchip_in_kernel(kvm))
			goto get_irqchip_out;
		r = kvm_vm_ioctl_get_irqchip(kvm, chip);
		if (r)
			goto get_irqchip_out;
		r = -EFAULT;
		if (copy_to_user(argp, chip, sizeof *chip))
			goto get_irqchip_out;
		r = 0;
	get_irqchip_out:
		kfree(chip);
		if (r)
			goto out;
		break;
	}
	case KVM_SET_IRQCHIP: {
		/* 0: PIC master, 1: PIC slave, 2: IOAPIC */
		struct kvm_irqchip *chip = kmalloc(sizeof(*chip), GFP_KERNEL);

		r = -ENOMEM;
		if (!chip)
			goto out;
		r = -EFAULT;
		if (copy_from_user(chip, argp, sizeof *chip))
			goto set_irqchip_out;
		r = -ENXIO;
		if (!irqchip_in_kernel(kvm))
			goto set_irqchip_out;
		r = kvm_vm_ioctl_set_irqchip(kvm, chip);
		if (r)
			goto set_irqchip_out;
		r = 0;
	set_irqchip_out:
		kfree(chip);
		if (r)
			goto out;
		break;
	}
	case KVM_GET_PIT: {
		r = -EFAULT;
		if (copy_from_user(&u.ps, argp, sizeof(struct kvm_pit_state)))
			goto out;
		r = -ENXIO;
		if (!kvm->arch.vpit)
			goto out;
		r = kvm_vm_ioctl_get_pit(kvm, &u.ps);
		if (r)
			goto out;
		r = -EFAULT;
		if (copy_to_user(argp, &u.ps, sizeof(struct kvm_pit_state)))
			goto out;
		r = 0;
		break;
	}
	case KVM_SET_PIT: {
		r = -EFAULT;
		if (copy_from_user(&u.ps, argp, sizeof u.ps))
			goto out;
		r = -ENXIO;
		if (!kvm->arch.vpit)
			goto out;
		r = kvm_vm_ioctl_set_pit(kvm, &u.ps);
		if (r)
			goto out;
		r = 0;
		break;
	}
	case KVM_GET_PIT2: {
		r = -ENXIO;
		if (!kvm->arch.vpit)
			goto out;
		r = kvm_vm_ioctl_get_pit2(kvm, &u.ps2);
		if (r)
			goto out;
		r = -EFAULT;
		if (copy_to_user(argp, &u.ps2, sizeof(u.ps2)))
			goto out;
		r = 0;
		break;
	}
	case KVM_SET_PIT2: {
		r = -EFAULT;
		if (copy_from_user(&u.ps2, argp, sizeof(u.ps2)))
			goto out;
		r = -ENXIO;
		if (!kvm->arch.vpit)
			goto out;
		r = kvm_vm_ioctl_set_pit2(kvm, &u.ps2);
		if (r)
			goto out;
		r = 0;
		break;
	}
	case KVM_REINJECT_CONTROL: {
		struct kvm_reinject_control control;
		r =  -EFAULT;
		if (copy_from_user(&control, argp, sizeof(control)))
			goto out;
		r = kvm_vm_ioctl_reinject(kvm, &control);
		if (r)
			goto out;
		r = 0;
		break;
	}
	default:
		;
	}
out:
	return r;
}

static void kvm_init_msr_list(void)
{
	u32 dummy[2];
	unsigned i, j;

	/* skip the first msrs in the list. KVM-specific */
	for (i = j = KVM_SAVE_MSRS_BEGIN; i < ARRAY_SIZE(msrs_to_save); i++) {
		if (rdmsr_safe(msrs_to_save[i], &dummy[0], &dummy[1]) < 0)
			continue;
		if (j < i)
			msrs_to_save[j] = msrs_to_save[i];
		j++;
	}
	num_msrs_to_save = j;
}

static int vcpu_mmio_write(struct kvm_vcpu *vcpu, gpa_t addr, int len,
			   const void *v)
{
	if (vcpu->arch.apic &&
	    !kvm_iodevice_write(&vcpu->arch.apic->dev, addr, len, v))
		return 0;

	return kvm_io_bus_write(&vcpu->kvm->mmio_bus, addr, len, v);
}

static int vcpu_mmio_read(struct kvm_vcpu *vcpu, gpa_t addr, int len, void *v)
{
	if (vcpu->arch.apic &&
	    !kvm_iodevice_read(&vcpu->arch.apic->dev, addr, len, v))
		return 0;

	return kvm_io_bus_read(&vcpu->kvm->mmio_bus, addr, len, v);
}

static int kvm_read_guest_virt(gva_t addr, void *val, unsigned int bytes,
			       struct kvm_vcpu *vcpu)
{
	void *data = val;
	int r = X86EMUL_CONTINUE;

	while (bytes) {
		gpa_t gpa = vcpu->arch.mmu.gva_to_gpa(vcpu, addr);
		unsigned offset = addr & (PAGE_SIZE-1);
		unsigned toread = min(bytes, (unsigned)PAGE_SIZE - offset);
		int ret;

		if (gpa == UNMAPPED_GVA) {
			r = X86EMUL_PROPAGATE_FAULT;
			goto out;
		}
		ret = kvm_read_guest(vcpu->kvm, gpa, data, toread);
		if (ret < 0) {
			r = X86EMUL_UNHANDLEABLE;
			goto out;
		}

		bytes -= toread;
		data += toread;
		addr += toread;
	}
out:
	return r;
}

static int kvm_write_guest_virt(gva_t addr, void *val, unsigned int bytes,
				struct kvm_vcpu *vcpu)
{
	void *data = val;
	int r = X86EMUL_CONTINUE;

	while (bytes) {
		gpa_t gpa = vcpu->arch.mmu.gva_to_gpa(vcpu, addr);
		unsigned offset = addr & (PAGE_SIZE-1);
		unsigned towrite = min(bytes, (unsigned)PAGE_SIZE - offset);
		int ret;

		if (gpa == UNMAPPED_GVA) {
			r = X86EMUL_PROPAGATE_FAULT;
			goto out;
		}
		ret = kvm_write_guest(vcpu->kvm, gpa, data, towrite);
		if (ret < 0) {
			r = X86EMUL_UNHANDLEABLE;
			goto out;
		}

		bytes -= towrite;
		data += towrite;
		addr += towrite;
	}
out:
	return r;
}


static int emulator_read_emulated(unsigned long addr,
				  void *val,
				  unsigned int bytes,
				  struct kvm_vcpu *vcpu)
{
	gpa_t                 gpa;

	if (vcpu->mmio_read_completed) {
		memcpy(val, vcpu->mmio_data, bytes);
		trace_kvm_mmio(KVM_TRACE_MMIO_READ, bytes,
			       vcpu->mmio_phys_addr, *(u64 *)val);
		vcpu->mmio_read_completed = 0;
		return X86EMUL_CONTINUE;
	}

	gpa = vcpu->arch.mmu.gva_to_gpa(vcpu, addr);

	/* For APIC access vmexit */
	if ((gpa & PAGE_MASK) == APIC_DEFAULT_PHYS_BASE)
		goto mmio;

	if (kvm_read_guest_virt(addr, val, bytes, vcpu)
				== X86EMUL_CONTINUE)
		return X86EMUL_CONTINUE;
	if (gpa == UNMAPPED_GVA)
		return X86EMUL_PROPAGATE_FAULT;

mmio:
	/*
	 * Is this MMIO handled locally?
	 */
	if (!vcpu_mmio_read(vcpu, gpa, bytes, val)) {
		trace_kvm_mmio(KVM_TRACE_MMIO_READ, bytes, gpa, *(u64 *)val);
		return X86EMUL_CONTINUE;
	}

	trace_kvm_mmio(KVM_TRACE_MMIO_READ_UNSATISFIED, bytes, gpa, 0);

	vcpu->mmio_needed = 1;
	vcpu->mmio_phys_addr = gpa;
	vcpu->mmio_size = bytes;
	vcpu->mmio_is_write = 0;

	return X86EMUL_UNHANDLEABLE;
}

int emulator_write_phys(struct kvm_vcpu *vcpu, gpa_t gpa,
			  const void *val, int bytes)
{
	int ret;

	ret = kvm_write_guest(vcpu->kvm, gpa, val, bytes);
	if (ret < 0)
		return 0;
	kvm_mmu_pte_write(vcpu, gpa, val, bytes, 1);
	return 1;
}

static int emulator_write_emulated_onepage(unsigned long addr,
					   const void *val,
					   unsigned int bytes,
					   struct kvm_vcpu *vcpu)
{
	gpa_t                 gpa;

	gpa = vcpu->arch.mmu.gva_to_gpa(vcpu, addr);

	if (gpa == UNMAPPED_GVA) {
		kvm_inject_page_fault(vcpu, addr, 2);
		return X86EMUL_PROPAGATE_FAULT;
	}

	/* For APIC access vmexit */
	if ((gpa & PAGE_MASK) == APIC_DEFAULT_PHYS_BASE)
		goto mmio;

	if (emulator_write_phys(vcpu, gpa, val, bytes))
		return X86EMUL_CONTINUE;

mmio:
	trace_kvm_mmio(KVM_TRACE_MMIO_WRITE, bytes, gpa, *(u64 *)val);
	/*
	 * Is this MMIO handled locally?
	 */
	if (!vcpu_mmio_write(vcpu, gpa, bytes, val))
		return X86EMUL_CONTINUE;

	vcpu->mmio_needed = 1;
	vcpu->mmio_phys_addr = gpa;
	vcpu->mmio_size = bytes;
	vcpu->mmio_is_write = 1;
	memcpy(vcpu->mmio_data, val, bytes);

	return X86EMUL_CONTINUE;
}

int emulator_write_emulated(unsigned long addr,
				   const void *val,
				   unsigned int bytes,
				   struct kvm_vcpu *vcpu)
{
	/* Crossing a page boundary? */
	if (((addr + bytes - 1) ^ addr) & PAGE_MASK) {
		int rc, now;

		now = -addr & ~PAGE_MASK;
		rc = emulator_write_emulated_onepage(addr, val, now, vcpu);
		if (rc != X86EMUL_CONTINUE)
			return rc;
		addr += now;
		val += now;
		bytes -= now;
	}
	return emulator_write_emulated_onepage(addr, val, bytes, vcpu);
}
EXPORT_SYMBOL_GPL(emulator_write_emulated);

static int emulator_cmpxchg_emulated(unsigned long addr,
				     const void *old,
				     const void *new,
				     unsigned int bytes,
				     struct kvm_vcpu *vcpu)
{
	printk_once(KERN_WARNING "kvm: emulating exchange as write\n");
#ifndef CONFIG_X86_64
	/* guests cmpxchg8b have to be emulated atomically */
	if (bytes == 8) {
		gpa_t gpa;
		struct page *page;
		char *kaddr;
		u64 val;

		gpa = vcpu->arch.mmu.gva_to_gpa(vcpu, addr);

		if (gpa == UNMAPPED_GVA ||
		   (gpa & PAGE_MASK) == APIC_DEFAULT_PHYS_BASE)
			goto emul_write;

		if (((gpa + bytes - 1) & PAGE_MASK) != (gpa & PAGE_MASK))
			goto emul_write;

		val = *(u64 *)new;

		page = gfn_to_page(vcpu->kvm, gpa >> PAGE_SHIFT);

		kaddr = kmap_atomic(page, KM_USER0);
		set_64bit((u64 *)(kaddr + offset_in_page(gpa)), val);
		kunmap_atomic(kaddr, KM_USER0);
		kvm_release_page_dirty(page);
	}
emul_write:
#endif

	return emulator_write_emulated(addr, new, bytes, vcpu);
}

static unsigned long get_segment_base(struct kvm_vcpu *vcpu, int seg)
{
	return kvm_x86_ops->get_segment_base(vcpu, seg);
}

int emulate_invlpg(struct kvm_vcpu *vcpu, gva_t address)
{
	kvm_mmu_invlpg(vcpu, address);
	return X86EMUL_CONTINUE;
}

int emulate_clts(struct kvm_vcpu *vcpu)
{
	kvm_x86_ops->set_cr0(vcpu, vcpu->arch.cr0 & ~X86_CR0_TS);
	return X86EMUL_CONTINUE;
}

int emulator_get_dr(struct x86_emulate_ctxt *ctxt, int dr, unsigned long *dest)
{
	struct kvm_vcpu *vcpu = ctxt->vcpu;

	switch (dr) {
	case 0 ... 3:
		*dest = kvm_x86_ops->get_dr(vcpu, dr);
		return X86EMUL_CONTINUE;
	default:
		pr_unimpl(vcpu, "%s: unexpected dr %u\n", __func__, dr);
		return X86EMUL_UNHANDLEABLE;
	}
}

int emulator_set_dr(struct x86_emulate_ctxt *ctxt, int dr, unsigned long value)
{
	unsigned long mask = (ctxt->mode == X86EMUL_MODE_PROT64) ? ~0ULL : ~0U;
	int exception;

	kvm_x86_ops->set_dr(ctxt->vcpu, dr, value & mask, &exception);
	if (exception) {
		/* FIXME: better handling */
		return X86EMUL_UNHANDLEABLE;
	}
	return X86EMUL_CONTINUE;
}

void kvm_report_emulation_failure(struct kvm_vcpu *vcpu, const char *context)
{
	u8 opcodes[4];
	unsigned long rip = kvm_rip_read(vcpu);
	unsigned long rip_linear;

	if (!printk_ratelimit())
		return;

	rip_linear = rip + get_segment_base(vcpu, VCPU_SREG_CS);

	kvm_read_guest_virt(rip_linear, (void *)opcodes, 4, vcpu);

	printk(KERN_ERR "emulation failed (%s) rip %lx %02x %02x %02x %02x\n",
	       context, rip, opcodes[0], opcodes[1], opcodes[2], opcodes[3]);
}
EXPORT_SYMBOL_GPL(kvm_report_emulation_failure);

static struct x86_emulate_ops emulate_ops = {
	.read_std            = kvm_read_guest_virt,
	.read_emulated       = emulator_read_emulated,
	.write_emulated      = emulator_write_emulated,
	.cmpxchg_emulated    = emulator_cmpxchg_emulated,
};

static void cache_all_regs(struct kvm_vcpu *vcpu)
{
	kvm_register_read(vcpu, VCPU_REGS_RAX);
	kvm_register_read(vcpu, VCPU_REGS_RSP);
	kvm_register_read(vcpu, VCPU_REGS_RIP);
	vcpu->arch.regs_dirty = ~0;
}

int emulate_instruction(struct kvm_vcpu *vcpu,
			struct kvm_run *run,
			unsigned long cr2,
			u16 error_code,
			int emulation_type)
{
	int r, shadow_mask;
	struct decode_cache *c;

	kvm_clear_exception_queue(vcpu);
	vcpu->arch.mmio_fault_cr2 = cr2;
	/*
	 * TODO: fix emulate.c to use guest_read/write_register
	 * instead of direct ->regs accesses, can save hundred cycles
	 * on Intel for instructions that don't read/change RSP, for
	 * for example.
	 */
	cache_all_regs(vcpu);

	vcpu->mmio_is_write = 0;
	vcpu->arch.pio.string = 0;

	if (!(emulation_type & EMULTYPE_NO_DECODE)) {
		int cs_db, cs_l;
		kvm_x86_ops->get_cs_db_l_bits(vcpu, &cs_db, &cs_l);

		vcpu->arch.emulate_ctxt.vcpu = vcpu;
		vcpu->arch.emulate_ctxt.eflags = kvm_x86_ops->get_rflags(vcpu);
		vcpu->arch.emulate_ctxt.mode =
			(vcpu->arch.emulate_ctxt.eflags & X86_EFLAGS_VM)
			? X86EMUL_MODE_REAL : cs_l
			? X86EMUL_MODE_PROT64 :	cs_db
			? X86EMUL_MODE_PROT32 : X86EMUL_MODE_PROT16;

		r = x86_decode_insn(&vcpu->arch.emulate_ctxt, &emulate_ops);

		/* Only allow emulation of specific instructions on #UD
		 * (namely VMMCALL, sysenter, sysexit, syscall)*/
		c = &vcpu->arch.emulate_ctxt.decode;
		if (emulation_type & EMULTYPE_TRAP_UD) {
			if (!c->twobyte)
				return EMULATE_FAIL;
			switch (c->b) {
			case 0x01: /* VMMCALL */
				if (c->modrm_mod != 3 || c->modrm_rm != 1)
					return EMULATE_FAIL;
				break;
			case 0x34: /* sysenter */
			case 0x35: /* sysexit */
				if (c->modrm_mod != 0 || c->modrm_rm != 0)
					return EMULATE_FAIL;
				break;
			case 0x05: /* syscall */
				if (c->modrm_mod != 0 || c->modrm_rm != 0)
					return EMULATE_FAIL;
				break;
			default:
				return EMULATE_FAIL;
			}

			if (!(c->modrm_reg == 0 || c->modrm_reg == 3))
				return EMULATE_FAIL;
		}

		++vcpu->stat.insn_emulation;
		if (r)  {
			++vcpu->stat.insn_emulation_fail;
			if (kvm_mmu_unprotect_page_virt(vcpu, cr2))
				return EMULATE_DONE;
			return EMULATE_FAIL;
		}
	}

	if (emulation_type & EMULTYPE_SKIP) {
		kvm_rip_write(vcpu, vcpu->arch.emulate_ctxt.decode.eip);
		return EMULATE_DONE;
	}

	r = x86_emulate_insn(&vcpu->arch.emulate_ctxt, &emulate_ops);
	shadow_mask = vcpu->arch.emulate_ctxt.interruptibility;

	if (r == 0)
		kvm_x86_ops->set_interrupt_shadow(vcpu, shadow_mask);

	if (vcpu->arch.pio.string)
		return EMULATE_DO_MMIO;

	if ((r || vcpu->mmio_is_write) && run) {
		run->exit_reason = KVM_EXIT_MMIO;
		run->mmio.phys_addr = vcpu->mmio_phys_addr;
		memcpy(run->mmio.data, vcpu->mmio_data, 8);
		run->mmio.len = vcpu->mmio_size;
		run->mmio.is_write = vcpu->mmio_is_write;
	}

	if (r) {
		if (kvm_mmu_unprotect_page_virt(vcpu, cr2))
			return EMULATE_DONE;
		if (!vcpu->mmio_needed) {
			kvm_report_emulation_failure(vcpu, "mmio");
			return EMULATE_FAIL;
		}
		return EMULATE_DO_MMIO;
	}

	kvm_x86_ops->set_rflags(vcpu, vcpu->arch.emulate_ctxt.eflags);

	if (vcpu->mmio_is_write) {
		vcpu->mmio_needed = 0;
		return EMULATE_DO_MMIO;
	}

	return EMULATE_DONE;
}
EXPORT_SYMBOL_GPL(emulate_instruction);

static int pio_copy_data(struct kvm_vcpu *vcpu)
{
	void *p = vcpu->arch.pio_data;
	gva_t q = vcpu->arch.pio.guest_gva;
	unsigned bytes;
	int ret;

	bytes = vcpu->arch.pio.size * vcpu->arch.pio.cur_count;
	if (vcpu->arch.pio.in)
		ret = kvm_write_guest_virt(q, p, bytes, vcpu);
	else
		ret = kvm_read_guest_virt(q, p, bytes, vcpu);
	return ret;
}

int complete_pio(struct kvm_vcpu *vcpu)
{
	struct kvm_pio_request *io = &vcpu->arch.pio;
	long delta;
	int r;
	unsigned long val;

	if (!io->string) {
		if (io->in) {
			val = kvm_register_read(vcpu, VCPU_REGS_RAX);
			memcpy(&val, vcpu->arch.pio_data, io->size);
			kvm_register_write(vcpu, VCPU_REGS_RAX, val);
		}
	} else {
		if (io->in) {
			r = pio_copy_data(vcpu);
			if (r)
				return r;
		}

		delta = 1;
		if (io->rep) {
			delta *= io->cur_count;
			/*
			 * The size of the register should really depend on
			 * current address size.
			 */
			val = kvm_register_read(vcpu, VCPU_REGS_RCX);
			val -= delta;
			kvm_register_write(vcpu, VCPU_REGS_RCX, val);
		}
		if (io->down)
			delta = -delta;
		delta *= io->size;
		if (io->in) {
			val = kvm_register_read(vcpu, VCPU_REGS_RDI);
			val += delta;
			kvm_register_write(vcpu, VCPU_REGS_RDI, val);
		} else {
			val = kvm_register_read(vcpu, VCPU_REGS_RSI);
			val += delta;
			kvm_register_write(vcpu, VCPU_REGS_RSI, val);
		}
	}

	io->count -= io->cur_count;
	io->cur_count = 0;

	return 0;
}

static int kernel_pio(struct kvm_vcpu *vcpu, void *pd)
{
	/* TODO: String I/O for in kernel device */
	int r;

	if (vcpu->arch.pio.in)
		r = kvm_io_bus_read(&vcpu->kvm->pio_bus, vcpu->arch.pio.port,
				    vcpu->arch.pio.size, pd);
	else
		r = kvm_io_bus_write(&vcpu->kvm->pio_bus, vcpu->arch.pio.port,
				     vcpu->arch.pio.size, pd);
	return r;
}

static int pio_string_write(struct kvm_vcpu *vcpu)
{
	struct kvm_pio_request *io = &vcpu->arch.pio;
	void *pd = vcpu->arch.pio_data;
	int i, r = 0;

	for (i = 0; i < io->cur_count; i++) {
		if (kvm_io_bus_write(&vcpu->kvm->pio_bus,
				     io->port, io->size, pd)) {
			r = -EOPNOTSUPP;
			break;
		}
		pd += io->size;
	}
	return r;
}

int kvm_emulate_pio(struct kvm_vcpu *vcpu, struct kvm_run *run, int in,
		  int size, unsigned port)
{
	unsigned long val;

	vcpu->run->exit_reason = KVM_EXIT_IO;
	vcpu->run->io.direction = in ? KVM_EXIT_IO_IN : KVM_EXIT_IO_OUT;
	vcpu->run->io.size = vcpu->arch.pio.size = size;
	vcpu->run->io.data_offset = KVM_PIO_PAGE_OFFSET * PAGE_SIZE;
	vcpu->run->io.count = vcpu->arch.pio.count = vcpu->arch.pio.cur_count = 1;
	vcpu->run->io.port = vcpu->arch.pio.port = port;
	vcpu->arch.pio.in = in;
	vcpu->arch.pio.string = 0;
	vcpu->arch.pio.down = 0;
	vcpu->arch.pio.rep = 0;

	trace_kvm_pio(vcpu->run->io.direction == KVM_EXIT_IO_OUT, port,
		      size, 1);

	val = kvm_register_read(vcpu, VCPU_REGS_RAX);
	memcpy(vcpu->arch.pio_data, &val, 4);

	if (!kernel_pio(vcpu, vcpu->arch.pio_data)) {
		complete_pio(vcpu);
		return 1;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(kvm_emulate_pio);

int kvm_emulate_pio_string(struct kvm_vcpu *vcpu, struct kvm_run *run, int in,
		  int size, unsigned long count, int down,
		  gva_t address, int rep, unsigned port)
{
	unsigned now, in_page;
	int ret = 0;

	vcpu->run->exit_reason = KVM_EXIT_IO;
	vcpu->run->io.direction = in ? KVM_EXIT_IO_IN : KVM_EXIT_IO_OUT;
	vcpu->run->io.size = vcpu->arch.pio.size = size;
	vcpu->run->io.data_offset = KVM_PIO_PAGE_OFFSET * PAGE_SIZE;
	vcpu->run->io.count = vcpu->arch.pio.count = vcpu->arch.pio.cur_count = count;
	vcpu->run->io.port = vcpu->arch.pio.port = port;
	vcpu->arch.pio.in = in;
	vcpu->arch.pio.string = 1;
	vcpu->arch.pio.down = down;
	vcpu->arch.pio.rep = rep;

	trace_kvm_pio(vcpu->run->io.direction == KVM_EXIT_IO_OUT, port,
		      size, count);

	if (!count) {
		kvm_x86_ops->skip_emulated_instruction(vcpu);
		return 1;
	}

	if (!down)
		in_page = PAGE_SIZE - offset_in_page(address);
	else
		in_page = offset_in_page(address) + size;
	now = min(count, (unsigned long)in_page / size);
	if (!now)
		now = 1;
	if (down) {
		/*
		 * String I/O in reverse.  Yuck.  Kill the guest, fix later.
		 */
		pr_unimpl(vcpu, "guest string pio down\n");
		kvm_inject_gp(vcpu, 0);
		return 1;
	}
	vcpu->run->io.count = now;
	vcpu->arch.pio.cur_count = now;

	if (vcpu->arch.pio.cur_count == vcpu->arch.pio.count)
		kvm_x86_ops->skip_emulated_instruction(vcpu);

	vcpu->arch.pio.guest_gva = address;

	if (!vcpu->arch.pio.in) {
		/* string PIO write */
		ret = pio_copy_data(vcpu);
		if (ret == X86EMUL_PROPAGATE_FAULT) {
			kvm_inject_gp(vcpu, 0);
			return 1;
		}
		if (ret == 0 && !pio_string_write(vcpu)) {
			complete_pio(vcpu);
			if (vcpu->arch.pio.count == 0)
				ret = 1;
		}
	}
	/* no string PIO read support yet */

	return ret;
}
EXPORT_SYMBOL_GPL(kvm_emulate_pio_string);

static void bounce_off(void *info)
{
	/* nothing */
}

static unsigned int  ref_freq;
static unsigned long tsc_khz_ref;

static int kvmclock_cpufreq_notifier(struct notifier_block *nb, unsigned long val,
				     void *data)
{
	struct cpufreq_freqs *freq = data;
	struct kvm *kvm;
	struct kvm_vcpu *vcpu;
	int i, send_ipi = 0;

	if (!ref_freq)
		ref_freq = freq->old;

	if (val == CPUFREQ_PRECHANGE && freq->old > freq->new)
		return 0;
	if (val == CPUFREQ_POSTCHANGE && freq->old < freq->new)
		return 0;
	per_cpu(cpu_tsc_khz, freq->cpu) = cpufreq_scale(tsc_khz_ref, ref_freq, freq->new);

	spin_lock(&kvm_lock);
	list_for_each_entry(kvm, &vm_list, vm_list) {
		kvm_for_each_vcpu(i, vcpu, kvm) {
			if (vcpu->cpu != freq->cpu)
				continue;
			if (!kvm_request_guest_time_update(vcpu))
				continue;
			if (vcpu->cpu != smp_processor_id())
				send_ipi++;
		}
	}
	spin_unlock(&kvm_lock);

	if (freq->old < freq->new && send_ipi) {
		/*
		 * We upscale the frequency.  Must make the guest
		 * doesn't see old kvmclock values while running with
		 * the new frequency, otherwise we risk the guest sees
		 * time go backwards.
		 *
		 * In case we update the frequency for another cpu
		 * (which might be in guest context) send an interrupt
		 * to kick the cpu out of guest context.  Next time
		 * guest context is entered kvmclock will be updated,
		 * so the guest will not see stale values.
		 */
		smp_call_function_single(freq->cpu, bounce_off, NULL, 1);
	}
	return 0;
}

static struct notifier_block kvmclock_cpufreq_notifier_block = {
        .notifier_call  = kvmclock_cpufreq_notifier
};

int kvm_arch_init(void *opaque)
{
	int r, cpu;
	struct kvm_x86_ops *ops = (struct kvm_x86_ops *)opaque;

	if (kvm_x86_ops) {
		printk(KERN_ERR "kvm: already loaded the other module\n");
		r = -EEXIST;
		goto out;
	}

	if (!ops->cpu_has_kvm_support()) {
		printk(KERN_ERR "kvm: no hardware support\n");
		r = -EOPNOTSUPP;
		goto out;
	}
	if (ops->disabled_by_bios()) {
		printk(KERN_ERR "kvm: disabled by bios\n");
		r = -EOPNOTSUPP;
		goto out;
	}

	r = kvm_mmu_module_init();
	if (r)
		goto out;

	kvm_init_msr_list();

	kvm_x86_ops = ops;
	kvm_mmu_set_nonpresent_ptes(0ull, 0ull);
	kvm_mmu_set_base_ptes(PT_PRESENT_MASK);
	kvm_mmu_set_mask_ptes(PT_USER_MASK, PT_ACCESSED_MASK,
			PT_DIRTY_MASK, PT64_NX_MASK, 0);

	for_each_possible_cpu(cpu)
		per_cpu(cpu_tsc_khz, cpu) = tsc_khz;
	if (!boot_cpu_has(X86_FEATURE_CONSTANT_TSC)) {
		tsc_khz_ref = tsc_khz;
		cpufreq_register_notifier(&kvmclock_cpufreq_notifier_block,
					  CPUFREQ_TRANSITION_NOTIFIER);
	}

	return 0;

out:
	return r;
}

void kvm_arch_exit(void)
{
	if (!boot_cpu_has(X86_FEATURE_CONSTANT_TSC))
		cpufreq_unregister_notifier(&kvmclock_cpufreq_notifier_block,
					    CPUFREQ_TRANSITION_NOTIFIER);
	kvm_x86_ops = NULL;
	kvm_mmu_module_exit();
}

int kvm_emulate_halt(struct kvm_vcpu *vcpu)
{
	++vcpu->stat.halt_exits;
	if (irqchip_in_kernel(vcpu->kvm)) {
		vcpu->arch.mp_state = KVM_MP_STATE_HALTED;
		return 1;
	} else {
		vcpu->run->exit_reason = KVM_EXIT_HLT;
		return 0;
	}
}
EXPORT_SYMBOL_GPL(kvm_emulate_halt);

static inline gpa_t hc_gpa(struct kvm_vcpu *vcpu, unsigned long a0,
			   unsigned long a1)
{
	if (is_long_mode(vcpu))
		return a0;
	else
		return a0 | ((gpa_t)a1 << 32);
}

int kvm_emulate_hypercall(struct kvm_vcpu *vcpu)
{
	unsigned long nr, a0, a1, a2, a3, ret;
	int r = 1;

	nr = kvm_register_read(vcpu, VCPU_REGS_RAX);
	a0 = kvm_register_read(vcpu, VCPU_REGS_RBX);
	a1 = kvm_register_read(vcpu, VCPU_REGS_RCX);
	a2 = kvm_register_read(vcpu, VCPU_REGS_RDX);
	a3 = kvm_register_read(vcpu, VCPU_REGS_RSI);

	trace_kvm_hypercall(nr, a0, a1, a2, a3);

	if (!is_long_mode(vcpu)) {
		nr &= 0xFFFFFFFF;
		a0 &= 0xFFFFFFFF;
		a1 &= 0xFFFFFFFF;
		a2 &= 0xFFFFFFFF;
		a3 &= 0xFFFFFFFF;
	}

	if (kvm_x86_ops->get_cpl(vcpu) != 0) {
		ret = -KVM_EPERM;
		goto out;
	}

	switch (nr) {
	case KVM_HC_VAPIC_POLL_IRQ:
		ret = 0;
		break;
	case KVM_HC_MMU_OP:
		r = kvm_pv_mmu_op(vcpu, a0, hc_gpa(vcpu, a1, a2), &ret);
		break;
	default:
		ret = -KVM_ENOSYS;
		break;
	}
out:
	kvm_register_write(vcpu, VCPU_REGS_RAX, ret);
	++vcpu->stat.hypercalls;
	return r;
}
EXPORT_SYMBOL_GPL(kvm_emulate_hypercall);

int kvm_fix_hypercall(struct kvm_vcpu *vcpu)
{
	char instruction[3];
	int ret = 0;
	unsigned long rip = kvm_rip_read(vcpu);


	/*
	 * Blow out the MMU to ensure that no other VCPU has an active mapping
	 * to ensure that the updated hypercall appears atomically across all
	 * VCPUs.
	 */
	kvm_mmu_zap_all(vcpu->kvm);

	kvm_x86_ops->patch_hypercall(vcpu, instruction);
	if (emulator_write_emulated(rip, instruction, 3, vcpu)
	    != X86EMUL_CONTINUE)
		ret = -EFAULT;

	return ret;
}

static u64 mk_cr_64(u64 curr_cr, u32 new_val)
{
	return (curr_cr & ~((1ULL << 32) - 1)) | new_val;
}

void realmode_lgdt(struct kvm_vcpu *vcpu, u16 limit, unsigned long base)
{
	struct descriptor_table dt = { limit, base };

	kvm_x86_ops->set_gdt(vcpu, &dt);
}

void realmode_lidt(struct kvm_vcpu *vcpu, u16 limit, unsigned long base)
{
	struct descriptor_table dt = { limit, base };

	kvm_x86_ops->set_idt(vcpu, &dt);
}

void realmode_lmsw(struct kvm_vcpu *vcpu, unsigned long msw,
		   unsigned long *rflags)
{
	kvm_lmsw(vcpu, msw);
	*rflags = kvm_x86_ops->get_rflags(vcpu);
}

unsigned long realmode_get_cr(struct kvm_vcpu *vcpu, int cr)
{
	unsigned long value;

	kvm_x86_ops->decache_cr4_guest_bits(vcpu);
	switch (cr) {
	case 0:
		value = vcpu->arch.cr0;
		break;
	case 2:
		value = vcpu->arch.cr2;
		break;
	case 3:
		value = vcpu->arch.cr3;
		break;
	case 4:
		value = vcpu->arch.cr4;
		break;
	case 8:
		value = kvm_get_cr8(vcpu);
		break;
	default:
		vcpu_printf(vcpu, "%s: unexpected cr %u\n", __func__, cr);
		return 0;
	}

	return value;
}

void realmode_set_cr(struct kvm_vcpu *vcpu, int cr, unsigned long val,
		     unsigned long *rflags)
{
	switch (cr) {
	case 0:
		kvm_set_cr0(vcpu, mk_cr_64(vcpu->arch.cr0, val));
		*rflags = kvm_x86_ops->get_rflags(vcpu);
		break;
	case 2:
		vcpu->arch.cr2 = val;
		break;
	case 3:
		kvm_set_cr3(vcpu, val);
		break;
	case 4:
		kvm_set_cr4(vcpu, mk_cr_64(vcpu->arch.cr4, val));
		break;
	case 8:
		kvm_set_cr8(vcpu, val & 0xfUL);
		break;
	default:
		vcpu_printf(vcpu, "%s: unexpected cr %u\n", __func__, cr);
	}
}

static int move_to_next_stateful_cpuid_entry(struct kvm_vcpu *vcpu, int i)
{
	struct kvm_cpuid_entry2 *e = &vcpu->arch.cpuid_entries[i];
	int j, nent = vcpu->arch.cpuid_nent;

	e->flags &= ~KVM_CPUID_FLAG_STATE_READ_NEXT;
	/* when no next entry is found, the current entry[i] is reselected */
	for (j = i + 1; ; j = (j + 1) % nent) {
		struct kvm_cpuid_entry2 *ej = &vcpu->arch.cpuid_entries[j];
		if (ej->function == e->function) {
			ej->flags |= KVM_CPUID_FLAG_STATE_READ_NEXT;
			return j;
		}
	}
	return 0; /* silence gcc, even though control never reaches here */
}

/* find an entry with matching function, matching index (if needed), and that
 * should be read next (if it's stateful) */
static int is_matching_cpuid_entry(struct kvm_cpuid_entry2 *e,
	u32 function, u32 index)
{
	if (e->function != function)
		return 0;
	if ((e->flags & KVM_CPUID_FLAG_SIGNIFCANT_INDEX) && e->index != index)
		return 0;
	if ((e->flags & KVM_CPUID_FLAG_STATEFUL_FUNC) &&
	    !(e->flags & KVM_CPUID_FLAG_STATE_READ_NEXT))
		return 0;
	return 1;
}

struct kvm_cpuid_entry2 *kvm_find_cpuid_entry(struct kvm_vcpu *vcpu,
					      u32 function, u32 index)
{
	int i;
	struct kvm_cpuid_entry2 *best = NULL;

	for (i = 0; i < vcpu->arch.cpuid_nent; ++i) {
		struct kvm_cpuid_entry2 *e;

		e = &vcpu->arch.cpuid_entries[i];
		if (is_matching_cpuid_entry(e, function, index)) {
			if (e->flags & KVM_CPUID_FLAG_STATEFUL_FUNC)
				move_to_next_stateful_cpuid_entry(vcpu, i);
			best = e;
			break;
		}
		/*
		 * Both basic or both extended?
		 */
		if (((e->function ^ function) & 0x80000000) == 0)
			if (!best || e->function > best->function)
				best = e;
	}
	return best;
}

int cpuid_maxphyaddr(struct kvm_vcpu *vcpu)
{
	struct kvm_cpuid_entry2 *best;

	best = kvm_find_cpuid_entry(vcpu, 0x80000008, 0);
	if (best)
		return best->eax & 0xff;
	return 36;
}

void kvm_emulate_cpuid(struct kvm_vcpu *vcpu)
{
	u32 function, index;
	struct kvm_cpuid_entry2 *best;

	function = kvm_register_read(vcpu, VCPU_REGS_RAX);
	index = kvm_register_read(vcpu, VCPU_REGS_RCX);
	kvm_register_write(vcpu, VCPU_REGS_RAX, 0);
	kvm_register_write(vcpu, VCPU_REGS_RBX, 0);
	kvm_register_write(vcpu, VCPU_REGS_RCX, 0);
	kvm_register_write(vcpu, VCPU_REGS_RDX, 0);
	best = kvm_find_cpuid_entry(vcpu, function, index);
	if (best) {
		kvm_register_write(vcpu, VCPU_REGS_RAX, best->eax);
		kvm_register_write(vcpu, VCPU_REGS_RBX, best->ebx);
		kvm_register_write(vcpu, VCPU_REGS_RCX, best->ecx);
		kvm_register_write(vcpu, VCPU_REGS_RDX, best->edx);
	}
	kvm_x86_ops->skip_emulated_instruction(vcpu);
	trace_kvm_cpuid(function,
			kvm_register_read(vcpu, VCPU_REGS_RAX),
			kvm_register_read(vcpu, VCPU_REGS_RBX),
			kvm_register_read(vcpu, VCPU_REGS_RCX),
			kvm_register_read(vcpu, VCPU_REGS_RDX));
}
EXPORT_SYMBOL_GPL(kvm_emulate_cpuid);

/*
 * Check if userspace requested an interrupt window, and that the
 * interrupt window is open.
 *
 * No need to exit to userspace if we already have an interrupt queued.
 */
static int dm_request_for_irq_injection(struct kvm_vcpu *vcpu,
					  struct kvm_run *kvm_run)
{
	return (!irqchip_in_kernel(vcpu->kvm) && !kvm_cpu_has_interrupt(vcpu) &&
		kvm_run->request_interrupt_window &&
		kvm_arch_interrupt_allowed(vcpu));
}

static void post_kvm_run_save(struct kvm_vcpu *vcpu,
			      struct kvm_run *kvm_run)
{
	kvm_run->if_flag = (kvm_x86_ops->get_rflags(vcpu) & X86_EFLAGS_IF) != 0;
	kvm_run->cr8 = kvm_get_cr8(vcpu);
	kvm_run->apic_base = kvm_get_apic_base(vcpu);
	if (irqchip_in_kernel(vcpu->kvm))
		kvm_run->ready_for_interrupt_injection = 1;
	else
		kvm_run->ready_for_interrupt_injection =
			kvm_arch_interrupt_allowed(vcpu) &&
			!kvm_cpu_has_interrupt(vcpu) &&
			!kvm_event_needs_reinjection(vcpu);
}

static void vapic_enter(struct kvm_vcpu *vcpu)
{
	struct kvm_lapic *apic = vcpu->arch.apic;
	struct page *page;

	if (!apic || !apic->vapic_addr)
		return;

	page = gfn_to_page(vcpu->kvm, apic->vapic_addr >> PAGE_SHIFT);

	vcpu->arch.apic->vapic_page = page;
}

static void vapic_exit(struct kvm_vcpu *vcpu)
{
	struct kvm_lapic *apic = vcpu->arch.apic;

	if (!apic || !apic->vapic_addr)
		return;

	down_read(&vcpu->kvm->slots_lock);
	kvm_release_page_dirty(apic->vapic_page);
	mark_page_dirty(vcpu->kvm, apic->vapic_addr >> PAGE_SHIFT);
	up_read(&vcpu->kvm->slots_lock);
}

static void update_cr8_intercept(struct kvm_vcpu *vcpu)
{
	int max_irr, tpr;

	if (!kvm_x86_ops->update_cr8_intercept)
		return;

	if (!vcpu->arch.apic)
		return;

	if (!vcpu->arch.apic->vapic_addr)
		max_irr = kvm_lapic_find_highest_irr(vcpu);
	else
		max_irr = -1;

	if (max_irr != -1)
		max_irr >>= 4;

	tpr = kvm_lapic_get_cr8(vcpu);

	kvm_x86_ops->update_cr8_intercept(vcpu, tpr, max_irr);
}

static void inject_pending_event(struct kvm_vcpu *vcpu, struct kvm_run *kvm_run)
{
	/* try to reinject previous events if any */
	if (vcpu->arch.exception.pending) {
		kvm_x86_ops->queue_exception(vcpu, vcpu->arch.exception.nr,
					  vcpu->arch.exception.has_error_code,
					  vcpu->arch.exception.error_code);
		return;
	}

	if (vcpu->arch.nmi_injected) {
		kvm_x86_ops->set_nmi(vcpu);
		return;
	}

	if (vcpu->arch.interrupt.pending) {
		kvm_x86_ops->set_irq(vcpu);
		return;
	}

	/* try to inject new event if pending */
	if (vcpu->arch.nmi_pending) {
		if (kvm_x86_ops->nmi_allowed(vcpu)) {
			vcpu->arch.nmi_pending = false;
			vcpu->arch.nmi_injected = true;
			kvm_x86_ops->set_nmi(vcpu);
		}
	} else if (kvm_cpu_has_interrupt(vcpu)) {
		if (kvm_x86_ops->interrupt_allowed(vcpu)) {
			kvm_queue_interrupt(vcpu, kvm_cpu_get_interrupt(vcpu),
					    false);
			kvm_x86_ops->set_irq(vcpu);
		}
	}
}

static int vcpu_enter_guest(struct kvm_vcpu *vcpu, struct kvm_run *kvm_run)
{
	int r;
	bool req_int_win = !irqchip_in_kernel(vcpu->kvm) &&
		kvm_run->request_interrupt_window;

	if (vcpu->requests)
		if (test_and_clear_bit(KVM_REQ_MMU_RELOAD, &vcpu->requests))
			kvm_mmu_unload(vcpu);

	r = kvm_mmu_reload(vcpu);
	if (unlikely(r))
		goto out;

	if (vcpu->requests) {
		if (test_and_clear_bit(KVM_REQ_MIGRATE_TIMER, &vcpu->requests))
			__kvm_migrate_timers(vcpu);
		if (test_and_clear_bit(KVM_REQ_KVMCLOCK_UPDATE, &vcpu->requests))
			kvm_write_guest_time(vcpu);
		if (test_and_clear_bit(KVM_REQ_MMU_SYNC, &vcpu->requests))
			kvm_mmu_sync_roots(vcpu);
		if (test_and_clear_bit(KVM_REQ_TLB_FLUSH, &vcpu->requests))
			kvm_x86_ops->tlb_flush(vcpu);
		if (test_and_clear_bit(KVM_REQ_REPORT_TPR_ACCESS,
				       &vcpu->requests)) {
			kvm_run->exit_reason = KVM_EXIT_TPR_ACCESS;
			r = 0;
			goto out;
		}
		if (test_and_clear_bit(KVM_REQ_TRIPLE_FAULT, &vcpu->requests)) {
			kvm_run->exit_reason = KVM_EXIT_SHUTDOWN;
			r = 0;
			goto out;
		}
	}

	preempt_disable();

	kvm_x86_ops->prepare_guest_switch(vcpu);
	kvm_load_guest_fpu(vcpu);

	local_irq_disable();

	clear_bit(KVM_REQ_KICK, &vcpu->requests);
	smp_mb__after_clear_bit();

	if (vcpu->requests || need_resched() || signal_pending(current)) {
		set_bit(KVM_REQ_KICK, &vcpu->requests);
		local_irq_enable();
		preempt_enable();
		r = 1;
		goto out;
	}

	inject_pending_event(vcpu, kvm_run);

	/* enable NMI/IRQ window open exits if needed */
	if (vcpu->arch.nmi_pending)
		kvm_x86_ops->enable_nmi_window(vcpu);
	else if (kvm_cpu_has_interrupt(vcpu) || req_int_win)
		kvm_x86_ops->enable_irq_window(vcpu);

	if (kvm_lapic_enabled(vcpu)) {
		update_cr8_intercept(vcpu);
		kvm_lapic_sync_to_vapic(vcpu);
	}

	up_read(&vcpu->kvm->slots_lock);

	kvm_guest_enter();

	if (unlikely(vcpu->arch.switch_db_regs)) {
		set_debugreg(0, 7);
		set_debugreg(vcpu->arch.eff_db[0], 0);
		set_debugreg(vcpu->arch.eff_db[1], 1);
		set_debugreg(vcpu->arch.eff_db[2], 2);
		set_debugreg(vcpu->arch.eff_db[3], 3);
	}

	trace_kvm_entry(vcpu->vcpu_id);
	kvm_x86_ops->run(vcpu, kvm_run);

	if (unlikely(vcpu->arch.switch_db_regs || test_thread_flag(TIF_DEBUG))) {
		set_debugreg(current->thread.debugreg0, 0);
		set_debugreg(current->thread.debugreg1, 1);
		set_debugreg(current->thread.debugreg2, 2);
		set_debugreg(current->thread.debugreg3, 3);
		set_debugreg(current->thread.debugreg6, 6);
		set_debugreg(current->thread.debugreg7, 7);
	}

	set_bit(KVM_REQ_KICK, &vcpu->requests);
	local_irq_enable();

	++vcpu->stat.exits;

	/*
	 * We must have an instruction between local_irq_enable() and
	 * kvm_guest_exit(), so the timer interrupt isn't delayed by
	 * the interrupt shadow.  The stat.exits increment will do nicely.
	 * But we need to prevent reordering, hence this barrier():
	 */
	barrier();

	kvm_guest_exit();

	preempt_enable();

	down_read(&vcpu->kvm->slots_lock);

	/*
	 * Profile KVM exit RIPs:
	 */
	if (unlikely(prof_on == KVM_PROFILING)) {
		unsigned long rip = kvm_rip_read(vcpu);
		profile_hit(KVM_PROFILING, (void *)rip);
	}


	kvm_lapic_sync_from_vapic(vcpu);

	r = kvm_x86_ops->handle_exit(kvm_run, vcpu);
out:
	return r;
}


static int __vcpu_run(struct kvm_vcpu *vcpu, struct kvm_run *kvm_run)
{
	int r;

	if (unlikely(vcpu->arch.mp_state == KVM_MP_STATE_SIPI_RECEIVED)) {
		pr_debug("vcpu %d received sipi with vector # %x\n",
			 vcpu->vcpu_id, vcpu->arch.sipi_vector);
		kvm_lapic_reset(vcpu);
		r = kvm_arch_vcpu_reset(vcpu);
		if (r)
			return r;
		vcpu->arch.mp_state = KVM_MP_STATE_RUNNABLE;
	}

	down_read(&vcpu->kvm->slots_lock);
	vapic_enter(vcpu);

	r = 1;
	while (r > 0) {
		if (vcpu->arch.mp_state == KVM_MP_STATE_RUNNABLE)
			r = vcpu_enter_guest(vcpu, kvm_run);
		else {
			up_read(&vcpu->kvm->slots_lock);
			kvm_vcpu_block(vcpu);
			down_read(&vcpu->kvm->slots_lock);
			if (test_and_clear_bit(KVM_REQ_UNHALT, &vcpu->requests))
			{
				switch(vcpu->arch.mp_state) {
				case KVM_MP_STATE_HALTED:
					vcpu->arch.mp_state =
						KVM_MP_STATE_RUNNABLE;
				case KVM_MP_STATE_RUNNABLE:
					break;
				case KVM_MP_STATE_SIPI_RECEIVED:
				default:
					r = -EINTR;
					break;
				}
			}
		}

		if (r <= 0)
			break;

		clear_bit(KVM_REQ_PENDING_TIMER, &vcpu->requests);
		if (kvm_cpu_has_pending_timer(vcpu))
			kvm_inject_pending_timer_irqs(vcpu);

		if (dm_request_for_irq_injection(vcpu, kvm_run)) {
			r = -EINTR;
			kvm_run->exit_reason = KVM_EXIT_INTR;
			++vcpu->stat.request_irq_exits;
		}
		if (signal_pending(current)) {
			r = -EINTR;
			kvm_run->exit_reason = KVM_EXIT_INTR;
			++vcpu->stat.signal_exits;
		}
		if (need_resched()) {
			up_read(&vcpu->kvm->slots_lock);
			kvm_resched(vcpu);
			down_read(&vcpu->kvm->slots_lock);
		}
	}

	up_read(&vcpu->kvm->slots_lock);
	post_kvm_run_save(vcpu, kvm_run);

	vapic_exit(vcpu);

	return r;
}

int kvm_arch_vcpu_ioctl_run(struct kvm_vcpu *vcpu, struct kvm_run *kvm_run)
{
	int r;
	sigset_t sigsaved;

	vcpu_load(vcpu);

	if (vcpu->sigset_active)
		sigprocmask(SIG_SETMASK, &vcpu->sigset, &sigsaved);

	if (unlikely(vcpu->arch.mp_state == KVM_MP_STATE_UNINITIALIZED)) {
		kvm_vcpu_block(vcpu);
		clear_bit(KVM_REQ_UNHALT, &vcpu->requests);
		r = -EAGAIN;
		goto out;
	}

	/* re-sync apic's tpr */
	if (!irqchip_in_kernel(vcpu->kvm))
		kvm_set_cr8(vcpu, kvm_run->cr8);

	if (vcpu->arch.pio.cur_count) {
		r = complete_pio(vcpu);
		if (r)
			goto out;
	}
#if CONFIG_HAS_IOMEM
	if (vcpu->mmio_needed) {
		memcpy(vcpu->mmio_data, kvm_run->mmio.data, 8);
		vcpu->mmio_read_completed = 1;
		vcpu->mmio_needed = 0;

		down_read(&vcpu->kvm->slots_lock);
		r = emulate_instruction(vcpu, kvm_run,
					vcpu->arch.mmio_fault_cr2, 0,
					EMULTYPE_NO_DECODE);
		up_read(&vcpu->kvm->slots_lock);
		if (r == EMULATE_DO_MMIO) {
			/*
			 * Read-modify-write.  Back to userspace.
			 */
			r = 0;
			goto out;
		}
	}
#endif
	if (kvm_run->exit_reason == KVM_EXIT_HYPERCALL)
		kvm_register_write(vcpu, VCPU_REGS_RAX,
				     kvm_run->hypercall.ret);

	r = __vcpu_run(vcpu, kvm_run);

out:
	if (vcpu->sigset_active)
		sigprocmask(SIG_SETMASK, &sigsaved, NULL);

	vcpu_put(vcpu);
	return r;
}

int kvm_arch_vcpu_ioctl_get_regs(struct kvm_vcpu *vcpu, struct kvm_regs *regs)
{
	vcpu_load(vcpu);

	regs->rax = kvm_register_read(vcpu, VCPU_REGS_RAX);
	regs->rbx = kvm_register_read(vcpu, VCPU_REGS_RBX);
	regs->rcx = kvm_register_read(vcpu, VCPU_REGS_RCX);
	regs->rdx = kvm_register_read(vcpu, VCPU_REGS_RDX);
	regs->rsi = kvm_register_read(vcpu, VCPU_REGS_RSI);
	regs->rdi = kvm_register_read(vcpu, VCPU_REGS_RDI);
	regs->rsp = kvm_register_read(vcpu, VCPU_REGS_RSP);
	regs->rbp = kvm_register_read(vcpu, VCPU_REGS_RBP);
#ifdef CONFIG_X86_64
	regs->r8 = kvm_register_read(vcpu, VCPU_REGS_R8);
	regs->r9 = kvm_register_read(vcpu, VCPU_REGS_R9);
	regs->r10 = kvm_register_read(vcpu, VCPU_REGS_R10);
	regs->r11 = kvm_register_read(vcpu, VCPU_REGS_R11);
	regs->r12 = kvm_register_read(vcpu, VCPU_REGS_R12);
	regs->r13 = kvm_register_read(vcpu, VCPU_REGS_R13);
	regs->r14 = kvm_register_read(vcpu, VCPU_REGS_R14);
	regs->r15 = kvm_register_read(vcpu, VCPU_REGS_R15);
#endif

	regs->rip = kvm_rip_read(vcpu);
	regs->rflags = kvm_x86_ops->get_rflags(vcpu);

	/*
	 * Don't leak debug flags in case they were set for guest debugging
	 */
	if (vcpu->guest_debug & KVM_GUESTDBG_SINGLESTEP)
		regs->rflags &= ~(X86_EFLAGS_TF | X86_EFLAGS_RF);

	vcpu_put(vcpu);

	return 0;
}

int kvm_arch_vcpu_ioctl_set_regs(struct kvm_vcpu *vcpu, struct kvm_regs *regs)
{
	vcpu_load(vcpu);

	kvm_register_write(vcpu, VCPU_REGS_RAX, regs->rax);
	kvm_register_write(vcpu, VCPU_REGS_RBX, regs->rbx);
	kvm_register_write(vcpu, VCPU_REGS_RCX, regs->rcx);
	kvm_register_write(vcpu, VCPU_REGS_RDX, regs->rdx);
	kvm_register_write(vcpu, VCPU_REGS_RSI, regs->rsi);
	kvm_register_write(vcpu, VCPU_REGS_RDI, regs->rdi);
	kvm_register_write(vcpu, VCPU_REGS_RSP, regs->rsp);
	kvm_register_write(vcpu, VCPU_REGS_RBP, regs->rbp);
#ifdef CONFIG_X86_64
	kvm_register_write(vcpu, VCPU_REGS_R8, regs->r8);
	kvm_register_write(vcpu, VCPU_REGS_R9, regs->r9);
	kvm_register_write(vcpu, VCPU_REGS_R10, regs->r10);
	kvm_register_write(vcpu, VCPU_REGS_R11, regs->r11);
	kvm_register_write(vcpu, VCPU_REGS_R12, regs->r12);
	kvm_register_write(vcpu, VCPU_REGS_R13, regs->r13);
	kvm_register_write(vcpu, VCPU_REGS_R14, regs->r14);
	kvm_register_write(vcpu, VCPU_REGS_R15, regs->r15);

#endif

	kvm_rip_write(vcpu, regs->rip);
	kvm_x86_ops->set_rflags(vcpu, regs->rflags);


	vcpu->arch.exception.pending = false;

	vcpu_put(vcpu);

	return 0;
}

void kvm_get_segment(struct kvm_vcpu *vcpu,
		     struct kvm_segment *var, int seg)
{
	kvm_x86_ops->get_segment(vcpu, var, seg);
}

void kvm_get_cs_db_l_bits(struct kvm_vcpu *vcpu, int *db, int *l)
{
	struct kvm_segment cs;

	kvm_get_segment(vcpu, &cs, VCPU_SREG_CS);
	*db = cs.db;
	*l = cs.l;
}
EXPORT_SYMBOL_GPL(kvm_get_cs_db_l_bits);

int kvm_arch_vcpu_ioctl_get_sregs(struct kvm_vcpu *vcpu,
				  struct kvm_sregs *sregs)
{
	struct descriptor_table dt;

	vcpu_load(vcpu);

	kvm_get_segment(vcpu, &sregs->cs, VCPU_SREG_CS);
	kvm_get_segment(vcpu, &sregs->ds, VCPU_SREG_DS);
	kvm_get_segment(vcpu, &sregs->es, VCPU_SREG_ES);
	kvm_get_segment(vcpu, &sregs->fs, VCPU_SREG_FS);
	kvm_get_segment(vcpu, &sregs->gs, VCPU_SREG_GS);
	kvm_get_segment(vcpu, &sregs->ss, VCPU_SREG_SS);

	kvm_get_segment(vcpu, &sregs->tr, VCPU_SREG_TR);
	kvm_get_segment(vcpu, &sregs->ldt, VCPU_SREG_LDTR);

	kvm_x86_ops->get_idt(vcpu, &dt);
	sregs->idt.limit = dt.limit;
	sregs->idt.base = dt.base;
	kvm_x86_ops->get_gdt(vcpu, &dt);
	sregs->gdt.limit = dt.limit;
	sregs->gdt.base = dt.base;

	kvm_x86_ops->decache_cr4_guest_bits(vcpu);
	sregs->cr0 = vcpu->arch.cr0;
	sregs->cr2 = vcpu->arch.cr2;
	sregs->cr3 = vcpu->arch.cr3;
	sregs->cr4 = vcpu->arch.cr4;
	sregs->cr8 = kvm_get_cr8(vcpu);
	sregs->efer = vcpu->arch.shadow_efer;
	sregs->apic_base = kvm_get_apic_base(vcpu);

	memset(sregs->interrupt_bitmap, 0, sizeof sregs->interrupt_bitmap);

	if (vcpu->arch.interrupt.pending && !vcpu->arch.interrupt.soft)
		set_bit(vcpu->arch.interrupt.nr,
			(unsigned long *)sregs->interrupt_bitmap);

	vcpu_put(vcpu);

	return 0;
}

int kvm_arch_vcpu_ioctl_get_mpstate(struct kvm_vcpu *vcpu,
				    struct kvm_mp_state *mp_state)
{
	vcpu_load(vcpu);
	mp_state->mp_state = vcpu->arch.mp_state;
	vcpu_put(vcpu);
	return 0;
}

int kvm_arch_vcpu_ioctl_set_mpstate(struct kvm_vcpu *vcpu,
				    struct kvm_mp_state *mp_state)
{
	vcpu_load(vcpu);
	vcpu->arch.mp_state = mp_state->mp_state;
	vcpu_put(vcpu);
	return 0;
}

static void kvm_set_segment(struct kvm_vcpu *vcpu,
			struct kvm_segment *var, int seg)
{
	kvm_x86_ops->set_segment(vcpu, var, seg);
}

static void seg_desct_to_kvm_desct(struct desc_struct *seg_desc, u16 selector,
				   struct kvm_segment *kvm_desct)
{
	kvm_desct->base = get_desc_base(seg_desc);
	kvm_desct->limit = get_desc_limit(seg_desc);
	if (seg_desc->g) {
		kvm_desct->limit <<= 12;
		kvm_desct->limit |= 0xfff;
	}
	kvm_desct->selector = selector;
	kvm_desct->type = seg_desc->type;
	kvm_desct->present = seg_desc->p;
	kvm_desct->dpl = seg_desc->dpl;
	kvm_desct->db = seg_desc->d;
	kvm_desct->s = seg_desc->s;
	kvm_desct->l = seg_desc->l;
	kvm_desct->g = seg_desc->g;
	kvm_desct->avl = seg_desc->avl;
	if (!selector)
		kvm_desct->unusable = 1;
	else
		kvm_desct->unusable = 0;
	kvm_desct->padding = 0;
}

static void get_segment_descriptor_dtable(struct kvm_vcpu *vcpu,
					  u16 selector,
					  struct descriptor_table *dtable)
{
	if (selector & 1 << 2) {
		struct kvm_segment kvm_seg;

		kvm_get_segment(vcpu, &kvm_seg, VCPU_SREG_LDTR);

		if (kvm_seg.unusable)
			dtable->limit = 0;
		else
			dtable->limit = kvm_seg.limit;
		dtable->base = kvm_seg.base;
	}
	else
		kvm_x86_ops->get_gdt(vcpu, dtable);
}

/* allowed just for 8 bytes segments */
static int load_guest_segment_descriptor(struct kvm_vcpu *vcpu, u16 selector,
					 struct desc_struct *seg_desc)
{
	struct descriptor_table dtable;
	u16 index = selector >> 3;

	get_segment_descriptor_dtable(vcpu, selector, &dtable);

	if (dtable.limit < index * 8 + 7) {
		kvm_queue_exception_e(vcpu, GP_VECTOR, selector & 0xfffc);
		return 1;
	}
	return kvm_read_guest_virt(dtable.base + index*8, seg_desc, sizeof(*seg_desc), vcpu);
}

/* allowed just for 8 bytes segments */
static int save_guest_segment_descriptor(struct kvm_vcpu *vcpu, u16 selector,
					 struct desc_struct *seg_desc)
{
	struct descriptor_table dtable;
	u16 index = selector >> 3;

	get_segment_descriptor_dtable(vcpu, selector, &dtable);

	if (dtable.limit < index * 8 + 7)
		return 1;
	return kvm_write_guest_virt(dtable.base + index*8, seg_desc, sizeof(*seg_desc), vcpu);
}

static gpa_t get_tss_base_addr(struct kvm_vcpu *vcpu,
			     struct desc_struct *seg_desc)
{
	u32 base_addr = get_desc_base(seg_desc);

	return vcpu->arch.mmu.gva_to_gpa(vcpu, base_addr);
}

static u16 get_segment_selector(struct kvm_vcpu *vcpu, int seg)
{
	struct kvm_segment kvm_seg;

	kvm_get_segment(vcpu, &kvm_seg, seg);
	return kvm_seg.selector;
}

static int load_segment_descriptor_to_kvm_desct(struct kvm_vcpu *vcpu,
						u16 selector,
						struct kvm_segment *kvm_seg)
{
	struct desc_struct seg_desc;

	if (load_guest_segment_descriptor(vcpu, selector, &seg_desc))
		return 1;
	seg_desct_to_kvm_desct(&seg_desc, selector, kvm_seg);
	return 0;
}

static int kvm_load_realmode_segment(struct kvm_vcpu *vcpu, u16 selector, int seg)
{
	struct kvm_segment segvar = {
		.base = selector << 4,
		.limit = 0xffff,
		.selector = selector,
		.type = 3,
		.present = 1,
		.dpl = 3,
		.db = 0,
		.s = 1,
		.l = 0,
		.g = 0,
		.avl = 0,
		.unusable = 0,
	};
	kvm_x86_ops->set_segment(vcpu, &segvar, seg);
	return 0;
}

static int is_vm86_segment(struct kvm_vcpu *vcpu, int seg)
{
	return (seg != VCPU_SREG_LDTR) &&
		(seg != VCPU_SREG_TR) &&
		(kvm_x86_ops->get_rflags(vcpu) & X86_EFLAGS_VM);
}

int kvm_load_segment_descriptor(struct kvm_vcpu *vcpu, u16 selector,
				int type_bits, int seg)
{
	struct kvm_segment kvm_seg;

	if (is_vm86_segment(vcpu, seg) || !(vcpu->arch.cr0 & X86_CR0_PE))
		return kvm_load_realmode_segment(vcpu, selector, seg);
	if (load_segment_descriptor_to_kvm_desct(vcpu, selector, &kvm_seg))
		return 1;
	kvm_seg.type |= type_bits;

	if (seg != VCPU_SREG_SS && seg != VCPU_SREG_CS &&
	    seg != VCPU_SREG_LDTR)
		if (!kvm_seg.s)
			kvm_seg.unusable = 1;

	kvm_set_segment(vcpu, &kvm_seg, seg);
	return 0;
}

static void save_state_to_tss32(struct kvm_vcpu *vcpu,
				struct tss_segment_32 *tss)
{
	tss->cr3 = vcpu->arch.cr3;
	tss->eip = kvm_rip_read(vcpu);
	tss->eflags = kvm_x86_ops->get_rflags(vcpu);
	tss->eax = kvm_register_read(vcpu, VCPU_REGS_RAX);
	tss->ecx = kvm_register_read(vcpu, VCPU_REGS_RCX);
	tss->edx = kvm_register_read(vcpu, VCPU_REGS_RDX);
	tss->ebx = kvm_register_read(vcpu, VCPU_REGS_RBX);
	tss->esp = kvm_register_read(vcpu, VCPU_REGS_RSP);
	tss->ebp = kvm_register_read(vcpu, VCPU_REGS_RBP);
	tss->esi = kvm_register_read(vcpu, VCPU_REGS_RSI);
	tss->edi = kvm_register_read(vcpu, VCPU_REGS_RDI);
	tss->es = get_segment_selector(vcpu, VCPU_SREG_ES);
	tss->cs = get_segment_selector(vcpu, VCPU_SREG_CS);
	tss->ss = get_segment_selector(vcpu, VCPU_SREG_SS);
	tss->ds = get_segment_selector(vcpu, VCPU_SREG_DS);
	tss->fs = get_segment_selector(vcpu, VCPU_SREG_FS);
	tss->gs = get_segment_selector(vcpu, VCPU_SREG_GS);
	tss->ldt_selector = get_segment_selector(vcpu, VCPU_SREG_LDTR);
}

static int load_state_from_tss32(struct kvm_vcpu *vcpu,
				  struct tss_segment_32 *tss)
{
	kvm_set_cr3(vcpu, tss->cr3);

	kvm_rip_write(vcpu, tss->eip);* Kernx86_ops->set_rflagsVirtual Machi* der | 2*
 * Kerneegisterbased VirtualVCPU_REGS_RAXal Machiax driver c
 *
 * Copyright (C) 2006 QumraCet, Inc.
c* Copyright (C) 2008 Qumranet, Inc.
 * CoDet, Inc.
d* Copyright (C) 2008 Qumranet, Inc.
 * CoBet, Inc.
b* Copyright (C) 2008 Qumranet, Inc.
 * CoSPal Machise driver anet.com>
 *   Yaniv Kamay  <yanit.com>
 *b  Ben-Ami Yassour <benami@il.ibm.com>
 *
SI.com>
 * iCorporation, 2008
 *
 * Authors:
 *   AviPL, versidn 2.
	if (Kernload_segment_descriptorVirtual Machldt_selector, 0C) 2006SREG_LDTR))
		return 1ry.
 *
 */

#include <linux/kvm_host.h>
#includes, 1ude "mmu.h"
ESlude "i8254.h"
#include "tss.h"
#include "kvm_cache_regs.h"
cs, 9ude "mmu.h"
C#include <linux/clocksource.h>
#include <linux/interrupt.h>
#sinclude "x86.h"
S#include <linux/clocksource.h>
#include <linux/interrupt.h>
#dinclude "x86.h"
D#include <linux/clocksource.h>
#include <linux/interrupt.h>
#finclude "x86.h"
F#include <linux/clocksource.h>
#include <linux/interrupt.h>
#ginclude "x86.h"
G#include <linux/ "i8254.0;
}

static void save_ludee_to_tss16(struct Kernirtu *irtua
				esc.h>
tsslude <lin16 *tss)
{
	 Machip =
#incel-breadVirtu driincludlag_MSRS 2for Linuxg *
 * derived 0_RESERVEax_MSRS 25
 *
 * Cdefine CRC) 2006 Qumrane0_RESERVEc X86_CR0_MP | X86_CR0_EM | X86_CR0_TS \C			  | X86d X86_CR0_MP | X86_CR0_EM | X86_CR0_TS \D			  | X86b X86_CR0_MP | X86_CR0_EM | X86_CR0_TS \B			  | X86sO_MSRS 25MP | X86_CR0_EM | X86_CR0_TS \SP)
#define _VME | X86_CR4_PVI | X86_CR4_TSD | X86_BR4_DE\
			siVME | X86_CR4_PVI | X86_CR4_TSD | X86_CI0_AM \
			 X86_CR4_PGE | X86_CR4_PCE | X86_CR4_OSDXSR	RESERVEes = signude <linrq.h"
#i_EM | X86_CR6.h"

#i	  | X86_ESERVED_BITS (~(unsigned long)X86_CR8_TPRC

#define sESERVED_BITS (~(unsigned long)X86_CR8_TPRS

#define dESERVED_BITS (~(unsigned long)X86_CR8_TPRD

#define ldtSERVED_BITS (~(unsigned long)X86_CR8_TPR#incl_RESERVEprev_task_linkSERVED_BITS (~(unsigned long)X86_CR8_TPRFIG_Xinclude <aint #incluinclufrom <asm/desc.h>
#include <asm/mtrr.h >
#include <asm/mce.h>

#defineKernel-based Virtual Machne driver for Linux
 *
 * derived from drD_BITvm/kvmmain.c
 *
 * Copyright (C) 2006 Qumranet, Inc. * Copyright (C) 2008 Qumranet, Inc.
 * Copyright IM Corporation, 2008
 *
 * Authors:
 *   Avi Kivity  <avi@qumranet.com>
 *   Yaniv Kamay  <yaniv@qumrant.com>
 *   Amit Shah    <amit.shah@qumranet.com>
    Ben-Ami Yassour <benami@il.ibm.com>
 *
 * This wrk is licensed under the terms of the GNU GPL, verson 2.  See
 * the COPYING file in the top-level dirctory.
 *
 */

#include <linux/kvm_host.h>
#include "nclude "mmu.h"
#include "i8254.h"
#include "tss.h"
#include "kvm_cache_regs.h"
#include "x86.h"

#include <linux/clocksource.h>
#include <linux/interrupt.h>
#include <linux/kvm.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/mman.h>
#include <linux/highmem.h>
#include <linux/iommu.h>
#include <linux/intaccess.h>
#include <a#elsKernc u64switch_m/desc.h>
#include <asm/m u16lude <a.h"
#intrr. alt_w(halold <asxits, u32p", VCPU_base	{ "halt_waesc.h>
ux/k_esc.h>
*nsegnux/kefine>
#include <asm/mce.h>ude <asm/mce.h;
	#elsreMA p0ry.
 *
 */

defi_guestVirtu->kvm,t_wakeup) },
	 &ude <asm/mce.h	{ "haltsizeofTAT(request_irqlude goto outfiner.h>
#include <asm/dirtualate_reload", VCory.
 *
 */

ased U_STAT(irq_exits) },
	{ "host_state_reload", VCPU_STAT((host_state_reload) },
	{ "efer_reload"_exits", VCPU_STAT(irq_exits) signeup) },
_addd long)Xrcalls) },	{ "haltate_reload", VCPn) },
	{ "insn_emulation_fail", VCPU_STAT(i", VCPU_STA != 0xffff) {
	ESER <asm/mce.h.static u64 __read", VCPU_STAfineVCPU_STAT(fpu_reload) },
	{ "ins = 0x   "irq_injections", VCPU_STAT(irq_injectipdateate_reload", VCpped) },
	{ "mmpte_updatehost_state_reload) },pped) },
	{ "mmlude "efer_reloa	}_STAT(ie
static u64 __read_mooad) },
	{ "fpu_reload	{ "efer_reload" },
	{1;
out:i_window"ret VCPU_STAT(nmi_window_exits) },
32 "halt_exits", VCPU_STAT(halt_exits) },
	{ "alt_waakeup", VCPU_STAT(halt_wakeup) },
	{ " "largells", VCPU_STAT(hypercalls) },
	{ "request_irq", VCPU32	struct descrip_exits) },
	{ "irq_exits", VCPU_STAT(irq_exits) },
	{ "host_state_reload", 32PU_STAT(host_state_reload) 32
	{ "efer_reload", VCPU_STAT(efer_32	{ "mmu_cache_miss",3/kvm_mCPU_STAT(fpu_reload) },
	{ "insn_emulation", VCPU_STAT(insselector === 0)
		return 0;

	kvm_get_gdt(&gdt);
AT(insn_emulation_fail) },
	{ "irq_injections", VCPU_STAT(irq_injections) },
	{ "nmi_32jections", VCPU_STAT(se(ldt_selector);
	}
	dmmu_shadow_zapped", VM_STAT(mmu_shadow_32pped) },
	{ "mmu_pte_write", VM_STAT(mmu_pte_write) },
	{ "mmu_pte_updated", VM_STAT(mmu_pte_updated) },
	{ "mmu_pde_zapped", VM_S)
		v |= ((unsignd) },
	{ "mmu_flooded", VM_ST)
		v |= ((unsign,
	{ "mmu_recycled", VM_STAT(mmu_recycled) }.base;

	if (selector & 4_STAT(mmu_cache_miss) },
	{ "mmu_unsync", VM_Smi_window_exits) }, "halt_exits", VCPU_STAT(halt_exits) },
	nmi_wreason,
	{ "requesKernude <li treck ;{ "requesCPU_STAT(hypccalls) }chip_in_kernel(vcpu->krcalls) }_exits) },
	{ "i	halt_wakeup) },
SERVED_BITS (~( },
d_bits = 0xfffffffffff	keup", VCPU_STAead_mostly efer_reserved_bits = 0xfffffffffff
	c_base = data;
irq_exarch.mmu.gvaude gpa	{ "mmu_wakeup) },
d nr)/* FIXME: Handle errors. Failure to TODd either TSS or their
	 *ernelvm_hoss should generate a pagefault.e;
	/, VM_STAT(m_STATlude <linux/kvm_host.h>
#inclirq.h"
#inc&rcalls) },ldt_selector);
	}
	dm_queue_exception);

void kvm_inject", VCPU_STAT(&vm))
		kvcpu *vcpu, unsigned lTODO: zappTASK_SWITCH_IRETM_STAT#elscp VM_STcpue_e					\
	(~(unsigncplg)(X86_CR	}
	d(_page_fault( & 3) >, data);
	.dpl ||.exc/
			set_bit(KVMM_STATdefinqueue_exception__GPL(kvmGP_VECTOR, 0 trip },
	{ "nmi_	}led", VM_S!		set_bit(p_REQsignCPU_Slimit(ruct kvm_vc < 0x67M_STATests);
			return;
		case PF_TSCTOR:
			ult -> shutdowned", c trip},
	{ "nmi_d", VM_Stion.pe=ding) {
		switch(v_REQnstruction re-executioJMPM_STATvm))
		kv.type &= ~(1 << 1); //clea = fa B D_BI
	", VCPddr,
			   u32 error_code)
{
	++vcpu->stat.pf_guest;

			   that instruction re-execution wiM_STAThaltivers/k) {
		case DF_VECTOed long)(X86_CRiver for Linux
 *
 * derived frivers/k& ~X86_EFLAGS_NTcpu, PF_/* set back  __reto stat c u6 only if NT bit isqueueinOL_GPL(

};
note thatoid kvm_queueis_cod used afet = fis po#elsL_GPL(kvtion.pending) {
		switCALL &&regeneranding) {
		switGATEude id kvm_queue_eed", Vtion.peueue_exception_e(struct kvm_vcpu *vcpu, unsigned nr, u32 error_code)
{
	WARN_ON(vcpu->arch.exception.pending);
	vcpu->arch.exception.pending = true;
	vcpu->arch.exception.has_error_code = true;
	vcpu->arch. *
 		set_bit(->arch 8ude "i8ruct kv },
	{ "remote_inject_page_fault(st", VCPU_STATtrr.h	 },
	{ "host_starcalls) },;
	elsere_cpl(struct kvm_vcpu *v,
	{ "mmurequired_cpl)
{
	if (kvm_x86_ops->get_cpl(vcpu) <= required_that instruction re-executiorue;
l regenerate lost
			   e_errom_inject_nmi(struct kvm_vcpu *vcpu)
{
	vcpu->arch.nmi_pending = 1;
}
EXPORT_SYMBOL_GPL(k| inject_nmi);

void kvxception.pending) {
		switch(vcpu->a
 */
bool kvm_r|= ception.pbreak;
		}
	}
	vcpu->arch.cr2 = addr;
	t_exits) },
	{ "h}
};

un <= required_cd", ver for Linux
 *
cr0u, int u->arch.exccr0_t pdptCR0_T

#decalls) }tude Kern size.error_codect_page_fault(strf (irqG_X86 (irq kvm_r= 1nmi_ checkxception)	{ "mmu_c (irqs = 0xfffffffffff	{ "mmu_unsync", VM_EXPORT_SYMBOL_GPL */

w_exits) },PL(kmi_windoh.exclude_ioctlRAY_SIregsostly efer_reserved_bits = 0xsigned l chec			g *arch.efine#elsmmu_reAY_Sneededu->arch#elspending_vec, max_bitschip_in_kernelvm_hos_table dload") {
	#incg)(X86_C
	dt.ption = arch.->i;
	__set;l);
	 data;
t(VCPU_EXRE },
driver for Linux
 *
idpte); ++idt;
	i;
	__set_bit(VCPU_gXREG_PDPTR,
		  (unsigned loL_GP)&vcpu->arch.regs_dirty)g
out:

	return cpu->ate,
				 2nsigned locr
	stizeof(vcpu->arch.|cpu->arch.exccr3ch.eZE(vcpu->3;	u64 pdpte[ARRA3_SIZE(vcpu->3
 * Kernkvm, p8u, int ZE(vcpu->8
{
	u.pdptrs)];
	bool changed = trueshadow_eferint r;

	ifong driver for Linux
 *
ong false;

	if (!ong AT(x) ofAY_SapicMBOL_GPL(kvmt(VCPU_kvm, vcpu*
 * Kernfor Linuxdecache_cr4		}
	}
XREGegs_avail).pdptrs)];
	bool changed = true;
0int r;

	if (archt_page(vcpu->kvm, pdpt_gfn,izeof(pdpt;
	ipu->arch.pdptrs sizeof(pdpte)ged = memcmp(pdpte, vcpu->arch.pdpt4int r;

	if (4e)) != 0;
out:

	retur4 changed;
}

voi4;
	iDF_VEis_long_mod_GPL(k)
	vcis_pa 0x%lx\ude #inclpdptrrived frong_mode(vcpu)PL(kvm_reizeof(vcpu->archude Kernizeof(vcpucontexpte); hangedU_EXREG = (host_stt(VCPU_Enterrupoto omap)ptiois_l	__set_bit( = find_firgoto o(
		(const unsigned ved  *)tk(KERN_DEBUG "set_cr0:VCPU_EXREGx #GP, r== 0 && NW =<0_PG) && !return;
		defau_DEBUG "su, int 	__set_bit(VCfalpte, 		pr_debug("Seue_exce	__set_ irq %d\n"t PG flag "
	 triple firqchip_in_kerne
			/*exits,
	{ "Kernvm, ing =_isr_ackng(vcpu) &&ad_guest_paAY_SIZE(pdpte); ++iZE(vcpu-sVM_MCE_CAP_SUPPORT		if ((vcpu->arch.shadow_efer linuM
 * - enable LM		if ((vcpu->arch.shadow_efer #inc86_CR8_TPR)

#de		if ((vcpu->arch.shadow_efer de <ce/events/kvm      "in long mode while PAE is incl "trace.h"

#      "in long mode while PAE is  <lienable syscall      "in long mode while PAE is trvm_set_apic_base);		if ((vcpu->arch.shadow_efer ed", /
#ifdef CONFIG_X
	updnclucr8et_cr0turn!(cr0 & X8/* Olderexcerspace won'vcpuhalt falslude oync"set.cpu->arch#include_is_bsp0x%lx\n",
RS 256
#define CR0tioned", 0 &&rror_ed;
}

vos.-> shutdo(KERN_00BUG cr0: #GP, p data;	vcpu->a0   "re"set_c!ng(vcpu,
				  o&fset * siPE0, vcu->arch.excepatic u = KVM_MP_STATE_RUNNABLE{
	u64 p_pu(vcpu, 0);
ccess.h>
#incs_mask[0][2])) {
			ret = 0;ue_excclear esc.h>
#include <asm/mtrr.hrve bits chePL(kvm_set_ *dbg pdpte, si, r&vcpu->arch.regs_avail)le fadbg->NW) rol & (0);
GUESTDBG_Ecr0 = |r0);
 & 0x0f))USE_HW_BP)tk(K"set_c (msw & 0x0f));
}
EXPORT_SYMBOL_GPL(kvm_lmsw);

STATfor ( X860; i <r0);
NR_DB6 Qum; ++i
	{ "64 pdpte[AReff_db[i] = r0 & h.excclearreg[i]"and	      (unsis) },
dbX86_s =u->acr0 & = X86_CR4_PGE 7] & DR7_BP_EN_MASK_X86_ pl)
r4)
{
	unsigned long old_cr4 = vcpu->i++h.cr4;
	unsigned long pdptr_64 pdpte[ARg pdpX86_CR4_PSE | X86_CR4_PAE;

	if  0);
			retudr7 {
		printk(KERN_DEBUGet_cruct kvm_vcpu *vOL_GPL(kvm_set_cirtuallongL(kvm_rer0 & ~0x0ful) |T_SYMBOL_GPL(INJECT_DBf ((cr0 );
			return;
	      "DBCTOR:
	ed_cpl)
u *ve\n");
			kvm_inject_gp(vcpu, 0);
		BPeturn;
		}
	} else if (is_pagiBECTOR:
	
{
	u64 pkvm_mmu_reset_contextrffffa/*
 * fxr.h> fpu tic u.  Taken 4 __ for 64/processor.hrs ro be killed when "sewe hr4: asm/x86;
		kvm_inje
cpu-esc.h>
t_cr4: {);

v	cwde);

v	sset_cr4: tset_cr4: fope);
64	ri
		kvm_ind
		kv32	mxcsregs);
		retu_maskrn;
	}
exce		}
[32];
			8*16 bytes 
	uneach FP-reITS	128u->arch*/
#ifdef CONFIG_inje64n;
	}
xmmr4(vcpu64cr4);
16vcpu->arch.cr4 = crXMM
	vcpu-25pu->archbasepl)
		& X86_CR4_PGE), cr4);
	vcpu->arch.cr4 = crset_contex>arch.mmu.base__sef
};EBUG "seTransl nr;
}_STAT virtual ns",essn_e(3 == vcpphysicch.cr3 && .XE) {s_mask[0][2])) {
			ret t{
	if (coto out;
		}
	}
	ret = 1;

	mememcpy(vcpu->_mmu_fluion *trefinepu, 0);
		retuvns", = tr->lin_PG)cr3 && ;
	gpa_t gpa&vcpu->arch.regs_avai	down6_CR0_&g(vcpu) &->sloterveckervegpavcpu->arch.exception.pending);
	vcpRESERse);

#defin;
			kvm_inject_gp(vcpu, 0_BITged(vcpu		printkSERVG "sBITS)valih.pd);
	!= UNMAPPED_GVAUG
				ased long ) },
BITS)n;

its u->arch;

	kvm_mmu_reset_context(vcpu);
	return;
}
EXPORT_SYMBsignfpu "halt_exits", VCPU_STATesc.h>
#inc#GP,*fpu,
	{ "request_cr4: *t_cr4: ) {
rs "
				     );
			kvh.excue_excfx_image&vcpu->arch.regs_avail)memcpy(fvcpufpr,				   ->_cr4(vcp,_vcpervee don'cw== 1ck rese"set_ce don's, because
		#GP, se don'tw X86cause
		ng VMXe donlast_opcvcpu, cause
		);
		k
	}

	/*
	IO_MScause
		nject_
	}

	/*
	dhysical memorpu, 0
		 * We donxmmcheck rese6_CR4_PGEjections"mode, because it vcpu->arch.cr3)) {
		printk(KE(vcpu);
	return;
}
EXPORT_SYMBOL_Gu, cr3)) {
				printk(KERN_DEBUG "set_cr3: #GP, pdptrs "
				       "reserved bits\n");
				kvm_inject_gp(vcpu, 0);
				return;
			}
		}
		/*
		 * Weck reserved bits e don't chin nonpaause
		 * ysice mode,(vcpu->kvm,scr3 >> PAGEsSHIFT)))
		ktcr3 >> PAGEtwx(vcpu->kvm,fohysic	}

	/*
	 * Doe(vcpu->kvm,rphysice map to phew_cr3(vcpu)* catcte, we
	 * id cr3 evenode, because it won in real-uld
	 * cause trouble later on when we turn on paging anywaysm/ms, 0)nitcr0);

void kvm_lmsw(s (cr3 & CR3_L_af * C	kvm_x86_ops;
		e;
	vTouch fals#GP,retur
		k time nr,non atomic NW) &&  as ife;
	vndingis
	}
	if (ir#GP,ind bits {
	falsreturn;
	 htrue;se;
	vwill	if e befoxcephip_ielse
		vcp"i8254s and it'll	}

	itoe;
	vallocpu, ram with GFP_KERNELYMBOL_GPL(kv!xcep_math(0, vcset_cx_r.h>f (cr3 &h.exchocpu, 0);
		 0);
			Initialize == vcpFPU bypae(vct	kvmourng kvmsav	kvm_nto == vc'
}
EX	preempt_dislong(x86_ops-kvm_lapic_get_cr8(vcpu);
	else
		retturn kvmf"set_t of msr nu_lapic_get_cr8(vcp(vcpu, 0);
		t of msr nurestor (bitno & 31);
}

/*
 * List of bit(intentno)
{
	vm_set_cr0(struct|=fset * siET;
	r8);
		kvm_injecu_ptffsetofed bits\i387_(strucSTAT(hyN_DEble later_CR4_PSE | Xp(vcpu, 0);
		.	retu
	vcp1f8archmemset((sm/ms		kvm_inject_gp(vcpu, 0);
		 +cr8);
		kvm_injec,"set_c  cludhost_sabilities of the host cpu.) -cr8);
		kvm_injecfffff  (pdpte[i] & vcpuUG "setThissm/ms*/

#inclp(vcpu,, cr3)) {
				printk(KER pdptel(vcg(vcpufpu_activee pag(vcpu_CS, MSR_rch.r	if (("i8254his list ifdef CONFIG_X86_		kvm_ieturn 1 << (bitno & 31);
}

/*
 * List of msr nuand KVM_SET_MSRS, an through KVM_GET_    (pdpte[i] & vcpu->arENTER_CS, MSR_	MSR_IA32_SYSpu_GPL(kvmSR_IA32_SYSENTER_ESP, MSR_IA32_SYSENTER_Efdef CONFIG_X86_64
	MSR_CSTAR, MSR_KERNEL_GS_BASE, MSR_te)) !=  expose to userspace through KVM_GET_MSRS
 * and KVM_SET_MSRS, and KVM_GET_MSR_IND++g(vcputic .ONFIre#incSR_VM_HSAVE_PA
};

static 
static u32 e	MSR_IA32_SYS][2])) {
	fresh_tlb(vcpu);
		return;_IA32_SYS64 pdpte[ARqchi_
EXPk(KERN_DEBrelectio
EXP_dirtyfer & EFER_LME) != (efX86_CR4_PSE | XME) != (e = NULLad_guest_page(vcpu->ging(vcpu)(cr0 & incluc.h>
#include <if (is_paging(crelush_tlb(vcpu)EFER_x86_ops3 & CR3_L_)
{
	define"i8254.m_inject_gp(vcpu, struct its) idurn;
	s_mask[0][2])) {
	setup)
	    && (vcpu->arch.shadow/* T0);
			We do* cause:pic_semust(vcpal 0);
pu) &&BUG_ON((pu, 0);
		ret	kvm_inject_gpu);
	else
		 a neFThis list is modmtrratic u.}

	_fix MSR_SYSC_cr3: #GP, reserve: #GP, c][2])) {
	ae(vc, reservearch.;
			ude "ruct kvizeobit(X8)(X86_CR>arch.cr3)) {
		puid_en<y(vcpuefer_vcpucludeset_context(vcE_SVM))) :kvm_inject_gp(vcpu, 0);
		returnrintk(KERN_DEB	}

	if (is_paging(destroy)
	    && (vcpu->arch.shado_cr3: #GP, reserve80000001un #GP, reserve>arch.cr3)) {
		prm_inject_gp(vcpu, 0);
		return;
	s_mask[0][2])) {
	 = kvmrn;
		}
	}

	kvm_x86_ops->set__cr8(vcnmi_);
		kvm=      ilities test s(efeinject MSR_X) && !lities test sif (!(cr4 & X86_Cecific. Tho

	if (is_lonncludS_BEGIN

	if (is_lon)bilities test sdr6 = DR6_FIXED_		structE)) {
			prOL_G7L(kvm_enabd_cpuid_entry(vcpu, 0x80000 = kvm_find_cvm_inject_gp(vcpuhardware.
 *
 * e are garb (ef

#definfor Linux 0 on success, ntherwisegister".
 * Returns 0 on suc bitno)
on-0 otherwise.
 * Assumes vcpu_load() w bitno)
ady called.
 s_mask[0][2]) 0 on suc, 0);
	om_find_cpuid_entry(vcpu, 0xsr_index, data)led.
 */
int kvm_set_msr(struun data);
}

/*
  Assumes vcpu_load() wt do_setled.
 */
int kvm_setcheck_
		kvm_in_compatvcpu *vrt reser Assumes vcpueturn kvm_set_msr(vcpuibility(, *dvcpu);
	return;
}
EXPORTset_cr8: #GP, reserved bits 0x}

	if e pag*
EXPchip_in_keid_entry_exits)  msrCPUID cg(vcpu) &;
		g\n"t of ms
	}

	if t timrn;
		}
	}

	imu.root_h;
			INVALID_PAGE #GP, res}

	if (!is_pagin) &&e paad_pdptrs(vcpu, vcpu-_x86_ops->set_cr0(vcpu, cr0);
	vcpu->arch.cr0 = ccpl)
		r_ops->set_cr0(vcpu, cr0);
	vcpu->arUNINITIALIZEDverse paginm_vcp{
		p(f (irqchip | __f (iZERO->ecx & != (efer & , 0x-ENOMEM"andEATUREailad_gu (updated bypio_dat
			
		prcr3 && (fer: #Gntry2 *feaizeostruct feat->ecx & bit(X86_FEATUREail(vcpu_ host boL(kvm_resion, sizeof(version))st
	 * sysKernstruct_lkvm,			/* triple fbit(X86_Fsys);
	bootizeo
			retad_gues;
		}
	}

	ice_banktructzm_vcp (mswMAX_MCE_BANKS *fer_reseu64) * 4pte_update  if (irqchipime equalion = version;

	kvmst
	 * system time for us, thut.tv_nsec;
	wc.vesion = versiong_caO_MSt(kvm, wall_clock on paging any


static uint32_"set_ef.tv_nsec;
	me();
	k	boot = ns_to_time:
	 = ns_ifieapability\n");
ust fill in host bo replac"mmu_unsyncgister".
 * Returns>archunk)
{
	static int version;
	strur)
{
= ns
	wc.sec = bood bits\n");
			kvm_inject_gp(vcpu, 0r;

	/* Don't try to re) {
			if (cr3 & CR3_PAE_RESERVED_Bthis one calculates
	 * "(dividend << 32) / div;
	}

	if 	struct t)
{
	reys));
vm);
}

/*
 kvm_cpuid_entry_write_guesS_BEGIN	2
stat) &&,+;
	kvm_write_32_SYSE) &&in a hope
ERR_PTR(tem tim* 100w) t_LIST_HEAD(&m_injh.exc	MSR_K
	/* one  !(cr >>= 1;
		shift--;
	}

	tpss, 0);
_dev_hea (!f;
			Reserveu, un0 ofm_in_sourceset_cr0:h.cr4n;

			}
	_inj_clockcpu->AY_Sm_in0);
USERSPACE_IRQ_SOURrac(D, --;
	}

	tp
	hv_clock->tsc_shnow = dtsclsion)	}

	tpvmck)
{_t_GPL(kvcpuid_entr
#include <asm/ms0000er &=  : "=amm_IA32_SYSENTER_ESP, MSR_IA32set_efer(vcpu, efer);

	efer &= ~EFER_LMA;
	efer |= vcpu-hv_clock->tsc_shiftE_SVM))) goto out;
		EFER_ (cr3 & CR3_L_)
{
	lock wc;
	strclude <asm/t_gp(vcpu, Unpin anysize times	if (iYMBOL_GPest_tor_ = c : "=(i	kvm_i,	tps6return ,
		 hv_clock->tcpu, efer);
shared_kaddr;
	unsigned long this_is_paging(vcpu)(cr0 & X86utexp(vcpt--;
	}c_khz, st	unsigned long nel(vc
			if  tsc_onS) {(struc)_inject_g tsc_strucpdptr_g\n");
	 this_t kvmz)) {
		kvm_set_ti		vcpu-;
	if er &nlikely(vcpu->hvvm_inject_gp(vcpusync_eventct kvm_vcpu *v)
{
	strest_time(all_2_t)nsecs) {icesion))var(cpu_tsc_khz);


			ret32_t disabled to prevent chaio
	efermapU_STAT(FIG_X86est_time(pitamp);
	ktvcpu) tsc_mul %upi		retocal_irq_restore(iokvm,t of msr ime(structmp);
	ktime_get_thysmemhe value *
 */
	}

	tpsvm, ackvm_!= (efand ut one c_time = ts.tv_nsec +
				  .system_time = tsepy);
entityt32_t long    (NSEC_PER_SEC * (u6e expects us to write as);

	/* Witvm *kvm, gpa_t wallAY_SmemoryX86_Con kvm_cpuid_entry2 *fea}

	if (efen;

			}
see the interm *mee, we just increasee the ect_
{
	x86_op	prin;

 to o},
	{ 	princpu = = mem->on += 2;ch.c>> _clo_SHIFloadclock.version += 2;

	s	vcp;

	s=;

	pr_vcpu->hs[, KM_ect_]32 <<=To keepe_excward ll_clock(stru)
{
	ioreturn;

			}
 KVM*x86 ->ar !pdphanlde cpu-ap_atom case_in_kernel(vcpu-ap_atomipu->arse.
cpu = && !old.rr0: #>requpu, 0);
		retue by 2 at ns",VM_STdend),ased V&current->mk,
	sc_tseG_X86		
{
	struct kvmtr_bo_if ((g\n"nclu *feat;>time_pa*py(sharIZE_REQ_KVMCPROT_READ |;
	retWRITrequests);MAP_PRIVATXPORmtrrANONYMOUS_REQ_KVMCvcpu->au-based V= &v->arch;

	if (!vcpu->>timequeIS_ERRose are p
{
	struct kvm,
	{ "__, tsc_PTR- 1:
	case MSR_MTRRfix64K_00NR_VARexceptie_page)
		returnel(vcallyh.cr4ed lhn.pendpdatpcpu->		spin (unlikely(vizeo(vcpu, 0	fic. lotect_gpge)
		return 
{
	struct kvm_v_C8000:
_khz;
	}
	putTRRfix4K_D000G "set_cr4:2_SYSEme_upu->time >>&&r, Kupdate(strucxits) },NR_VAR_arch *vcpu = &v->arch;

	if (!vcpu->tim_miss) 0;
	.tsc_( &v->arch;
)
{
	Rfix4struct kvm_REQ_KVM
		rLOCK_UPDATE, &v->r_CR_PAT	case 0x200 ... 0x200 + 2 * KVM_NRRfix4rch.etec;
	wc.nnd aintk(rqch_WARNING

	version++"ed lohv_cret = 0;ee the interm: "7 */
}

stati	booedn_e(n truee, Kory\n"t < 8 }tion.nr = 000:
	case MSR_MTRRfix4K_D000LL;
	whnabled;
_reqSTAT->kvint32_t)tm_inject timespec nr msr, u64 current_kernalcuf (c msr, u64 he valueient;
}

changrn false;

	if ,if (!msr_mtrr_X86_64
		if0001,lot_removeT(fpu_rsec +
);
		iizeof(vcpof(u64K_E0000:
	case MSR_MTRRfix4 {
		printk(KERN_DEB0000000LLflush_igned , MSR_IA32_TSC, &vcpu->hvmsr,zap to he values */      pat_turn facal_irq_savu->arch.shadow_eferun *
 * 
	    && (vcpu->arch.shado"i8254._ops->set_cr0(vcpu, ccr0);
	vcpu->arch.cr0 =
		_STAR,
#i00) {
		for (i = 0; i < 8 ; i+SIPI_RECEIVED (!valid_mtrr_typ(efer & EFER||ject0000000LL_DEBUG "se_atowe		printk_gp(	_ns(&spu_haset_cr0: #GP, se~masvm_inject_gp>archkickfix64K_00000 && msr <= MSR_e, siclocrch.exu
	}

	if gs;
	st& (1wait);
				MSR_Kf (cr3 &wqimespecwake_uf (!EBUG "sino)
ranges;

	msr rved bits\n",#end_trr_ukvm__cr4mta;
}
EXdr;
*
	 * Thp = !ge, 0:
	apability)p = <if (ptrs(dest_t & 0		kvm_rr_sculat_SYSEt_excancludystem_mul REQ_KICK, ;
			kvvcpu, us,
	{ "sr0(venrr_tschedulbled =INDEuvcpu->arc;
	} else if (msRs */
	return valiix64K_00000 && msr <= MSR_MTRRfix Assumes vcpuRs */
	return valid_mtrSR_MT  (pdptTRACEPOINpte[i] & vcpu->arex,
	MS>= MSR_MTRRfix4K_C0000 && msr <= inj_virretu>= MSR_MTRRfix4K_C0000 && msr <= 
		prORT_S_C0000] = data;
	else if (msr == MSRms_MTR>= MSR_MTRRfix4K_C0000 && msr <= c_MTR