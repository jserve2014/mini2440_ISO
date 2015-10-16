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
 river eegisterbased ived frVCPU_REGS_RAXfrom driax driver c
 kvm_mCopyright (C) 2006 QumraCet, Inc.
c 2008 Qumranet, Inc8
 * Conyright IB) 200Dyright IBd Corporation, 2008
 *
 * Authors:
 *   AvByright IBb Corporation, 2008
 *
 * Authors:
 *   AvSPfrom drise CopyrigAuth.com>*     Yaniv Kamay  <yanissour <beb  Ben-Ami Yassour <benami@il.ibmsour <be
SIsour <beniCorporation,008
 t (C) 2Authors:<benamAviPL, versidn 2.
	if (ver load_segment_descriptorived from drldt_selector, 0t, Inc.SREG_LDTR))
		return 1ry *  
 */

#include <linux/Kernhost.h>"tss.h"
#s, 1"
#i"mmu.h"
ESh"
#i"i8254.h"
tss.h"
#i"tssnux/clocksourcKerncache_regh>
#ics, 9ude "x86.h"
Ctss.h"
#include clock undcehe_regs.h"
#include interrupche_reslocksourcx86.h"
S.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>
#included<linux/module.h>D.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>
#includef<linux/module.h>F.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>
#includeg<linux/module.h>Gtss.h"
#include ude <lin0;
}

static void save_h"
#e_to_tss16(structiver ved  *ved f
				esche_rtssh"
#inclu16 *tss)
{
	om drip =/clocel-breadived  Copss.h"
lag_MSRS 2for Lludeg (C) 2deopyrd 0_RESERVEaxTS					5t (C) 20define CRt, Inc.
 * Cone6_CR0_PE c X86_CR0_MP |CR0_ET | EM6_CR0_NE | TS \C			 6_CR0_dCR0_ET | X86_CR0_NE | X86_CR0_WP | X86_DR0_AM \
		bCR0_ET | X86_CR0_NE | X86_CR0_WP | X86_BR0_AM \
		sOX86_CR0_X86_CR0_NE | X86_CR0_WP | X86_SP)
#_CR0_EM_VME6_CR0_NE 4_PVI86_CR4_PSE TSD6_CR0_NBR4_DE\trr.si| X86_CR4_PSE | X86_CR4_PAE | X86_CR4_CI0_AM 			  _CR4_PSE |GX86_CR4_PSE |CX86_CR4_PSE OSDXSR	CR0_PE es = signlinux/vmrqnux/cl X86_CR0_WP le.h>
#i_AM \
		_R0_PE D_BITS (~(unVED_ed long)R0_ET 8_TPRC
_DE\
			 sVM_MAX_MCE_BANKS 32
#define KVM_MCE_CAP_SSPPORTED MCdVM_MAX_MCE_BANKS 32
#define KVM_MCE_CAP_SDPPORTED MCldtM_MAX_MCE_BANKS 32
#define KVM_MCE_CAP_Stss.h_CR0_PE prev_task_linkM_MAX_MCE_BANKS 32
#define KVM_MCE_CAP_SFIG_Xss.h"
#inaint tss.h"ss.h"from <asm/ux/kinclude <linuxad_mmtrr.h efer_reserved_bit
#incl_DE\
			ain.cl-Copyright (C)om dr defopyrig	\
	(~(unt (C) 2long)(X84 __rdr_MCE_vm "kvmain.ht (C) 2008 Qumranet, Inc.
 * Couthors:
 ) 2008 Qumranet, Inc *
 * Authors:
 *   Av8 QumranIM  2.  See
 * the COPYING file in the top-l Kivitom>
avi@q.x), KVsour <benami@il.ibm.com>
 *
 vd(struc kvm_cpuid2 Amit Shah  d_cpmit.shahd(struct kvm_cpu k is licensed under the terms of the GNU  * This wrk is licenpyriunrs/kthe terms ofdule_GNU Gevel dirotory  Seeops)ule_COPYING file indule_pop-level dir"
#i
#include "tss.h"
#include "kvm_cache_regs.h"
# "f_fixed"x86.h"
"pf_fixed"e <linux/clocksource.h>
#include <linux/interrupt.h>
#"pf_fixed"dule.h>"tss.h"
#include nux/fs.h>
#include <linux/vmalloc.h>
#includess.h"
#include "kvinclude <linux/vmallfsinclude <linux/vmallvmalloly efer_reserveclude modul#include <linux/vmallmmaninclude <linux/vmallhighme_exits", VCPU_STAT(ioiox86.hclude <linux/vmalloc.acce.h>
efer_reserved#elsver c u64switch__mostly efer_reserved_bit u16eservedh", VCPs =  alt_w(haloldved_xits, u32p",  2006Copy	{ "hlt_waastly efde "_stly ef*nsegude "CR0_EfffffffffffffffeULL;
#ffffffffeULL;
;
	mi_wreMA p0s_debugfs_it_CR0_guestived ->kvm,ercakeup) },
	 &ffffffffeULL;
	{ "hypesizeofTAT(reqSTAT_irqh"
#igoto outR0_Ery efer_reserved_bidved fratrrup#inc_wakets_debugfs_itopyriU_Stateirq_e_STAhost_s{ "host_lude	{ "fpu_reloaPreload)(tion", VCPU_STAT(nsn_emulaefer{ "fpu_r
	{ "iinsn_emulatio },
	{ "ins2
#de "host__addfine KVMrcallinsn_	{ "hypeVCPU_STAT(insn_ennsn_emulainsn_emulee
 *_failn_emulation_fainsn_emulat != 0xffff) {
	R0_PfffffeULL;
.lude <au64 __defiinsn_emulatR0_En_emulatiofpunsn_emulation_fainESER0x   " },
injece
 *sn_emulation_fail)M_STAT(pdateVCPU_STAT(insn_ppeulation_fammpte_u_pde_) },
	{ "insn_emulatiT(mmu_pde_zappefixed"il", VCPU_	}eload) eclude <a
	{ "mmu_p_moemulation_fae_write) }n_fail", VCPU_STsn_emu1;
out:i_window"retsn_emulationmmmu_unsy
	{ "insn_e32 "hyperT(insn_emulation_flb_flush",nsn_emulayperca
	{ " VM_STAT(remote_tl,
	{ "host_s{ " "largell, VM_STAT(remotypeTAT(irq_inLL }
_reload) },_wakeup32	esc.h>
ux/kvm_
	{ "insn_emula },
	{ "in_emulation_fail) },
	{ selectotion", VCPU_STAT(ins32TAT(remot },
	{ "insn_emula32on_fail", VCPU_STnsn_emulatioil", 32_zappeu/interrmiss",3 "kvmmAT(mmu_pte_write) },
	{ "mmu_ptU_STAT(nmi_n_emulation_fansrq.h"
#i === 0ude "i8254.0
 * Kernget_gdt(&gdt);
ad_ldtU_STAT(nmi_inject desc_structM_STAT(mmu_pte_updated) },
	{ "mmuonbase;
	unsu_un32STAT(mmu_pte_updated)se(e "irq.h"
#i);
	}
	d

	ishac) }zaT(mm
	taMeload) == 2 || d-32T(mmu_pde_zappeu_d) }ased == 9 || d->type long)((s|= ((unsigned lon,
	{ " == 9 || d->type2;
#endif
	|= ((unsigned de->type == 9 ||ude v |= (S 32
#d_GPL(segment_bflood
	return vt_apic_base(struc((unsignedrecycl
	return v;
}
EXP	return ) }.Copy
 *  *
 ();

		ta& 4|| d->typef (selecto|= ((unsignedunsync== 9 ||_unsync) },
	{ "rem_tlb_flush", VM_STAT(remote_tlb_flush) },u_unsreasopu->kvm_reloaver linux/v treck ;erve bitsong segment_cAT(irq_iX_IO_in_kfine Virtu->kTAT(irq_i	struct desc_str	pages) },
	{ NULM_MAX_MCE_BANKSse;
d_bitte_upd", Vic_base	pages", VM_STAT) },
	stly il", VCserveGPL(kvm_set_apic_base)
	c) },
 = data;
 },
	{arch.x86.gva	{ "epaunsigned,
	{ "host_d nr)/* FIXME: Handle errors. Failur;

s TODd either TSS ooduleir
	 *fine kvm_cas should generate a pagefault.e;
	/= 9 || d->teloadh"
#include "kvm_cache_regs.hiunsignednc&TAT(irq_in->s == 0 && (d->type _queue_excepet_d*
 *sm/msKernM_STAT
	table_base &vmlude kirtu *irtual 32
#definTODO: >typTASK_SWITCH_IRET || d-mi_wcp 9 || cpddr,rr.h	\
	NKS 32
#dcplg)(R0_ET >type(_
EXP_ORT_S( & 3) >,_ON(v(d->.dpl ||.exc/		  |etPL(k(KVM || d-_CR0_g addr,
			   u__GPL(kvmGP_VECTOR, 0 tripc_base(d);
#	}rn vcpu->a!E_FAULT, &p_REQVED__emullimit(c.h>
Kernvc < 0x67 || d-ests(d->e "i8254defac	WARPF_TSR:
	:efauult -> shutdown
	retccpu->_base(d);
# == 9 ||et_d.pe=dingM_STA	ts) },(vpu->nesc.h>ion re-executioJMP || d-_guest;

.type &= ~(1 << 1); //clea = fa B _MCE
	
	tablddr,		  | (hal
	vcpu_codeefine++e(vcpulude.pfU_STAT
 * 	vcputhat iegenerate lost
			   en wi || d-hypepyris/ke-execreplaDFCTOR:
efine KV	/* trict kvm, stat.x), KVM_STAT_VM
#dnmi(str& ~ne KVFLAGS_Non.n,ace /* set back  "mmuto lude _rec only if NT bit isg addinOLcase 

};
notePF_VEror_codegned ns2 =  upyriafet= fais pomi_w u32 ekvnstructnon re-executioCALL &&re.nr = ion.pending = tGATEixedARN_ON(vcpu-_e
	retunstruct;
			return;
		edesc.h>
_code if (vcpu->arch.excenrT(halrch.cr2 = addr;
WARN_ONse(vcpuh.exc,
			   ueption.pe;
	PL(kvm_queue_exception_e);

 = truMBOLPL(kvm_queue_exceptiohas_ch.cr2 = acpl; if true, return 6_opE_FAULT, &vm_que 8) },
	{pu->arc selector)motee)
{
	+ult -> shutds++vcpu->stat.s = 0	se;
	unsigned loTAT(irq_in;
	elsere_cpl	vcpu->arch.excepti((unsigne_reliredretuefine *
 Kernfor Linuxdt_seturexce) <= n_e(vcpu,F_VECTOR, error_code);
}

vo if tl cpu->arcte lostvcpu, Peise qde)
{
	+_nmi	vcpu->arch.exception.efinePL(kvm_queuu_unquired_cpl1
#inEXPORT_SYMB, u32 ek| y are all 32 error_co_exception_e);

/-execution wiL(kvm_lude boolarch.r|= exception#defkdefa}->typPL(kvm_queucr2 = addr;
	tlb_flush) },
	{h}ror_
unT_SYMBOL_GPL(c == t kvm, stat.x), Kcr0u, #elskvm_queue_ecr0_t pdptP | XendifAT(irq_itixedver  host.ch.cr2 = at required_cpl)
rf failG_X86oto oE-1)) = 1u_un check
			   u3ase;

	ifoto ovm_set_apic_base);L(kvm_get_apic_based long cr3)
{
	gfude " },
	{ "remgfn__unsync)eue_eh"
#_ioctlRAY_SIupt.eption(struct kvm_vcpu *vcpu,2
#defin ARRA			g *_queueR0_Emi_wapic_b= 0;neededkvm_quemi_wption.p_vec, maxPL(kvm_lapic_set_bakvm_ca_table dfpu_red oft(stpu->arch
	dt.	   uARRA.exc->i;
	__set;l/*
 _ON(vcpt(n_emuEXREexcepuct kvm, stat.x), Kidpte); ++idt;
	EXREG_PDPLT, &n_emugX.h"
PDPTR	}
	  S 32
#define u32)&PL(kvm_queuupt._loady)g
	{ "

 "i8254.L(kvm_te	}
	v	 232
#definecr
	stost_sGPL(kvm_queu|L(kvm_queue_ecr3ueueZE((cr3 &3;	
	{ set e[ARRA3_SIr;

	if (ivm_main.its) p8pt_gfn,r;

	if (8fineu.set rs)];
	_SIZEchangedcpl; if2 || d-il",#elsreturn ongtruct kvm, stat.x), Kch.rfal	return vc!ch.rAT(x) of= 0;apic3)
{
	gfn_vmT_SYMBOretur(cr3kvm_main.	\
	(~(un(u64terrcr4
	int r_GPL(strav_str(VCPU_EXREG_PDPTR,
		      (uns;
0*)&vcpu->ar (t(VC requise(vcpu,eturndpt_gfn,.pdptrrn crn r(kvm_queuVCPU_E));
	}

voide)      memcmppu *vc, pdptet_cr0(stru4s, sizeof(pdp4e))zappe,
	{ " *vcpu)
4R,
		   
#incvoi4rn ru *vcis_ne K_modcase P)ret;is_pa 0x%lx\ixedT(exiVCPU_AT_VM
#ded bits;
out:)se PF__re.pdptrs)];
	boolof(pdpte.pdptrs)];contexout:


		   long *G = == 0)
		gned lonc.h>
#ifer_rmap)	   serv;
}
EXPORT_pendind_firefer_r(
		(const>arch.exce)(X8 *)tk(KERN_DEBUG "
EXPcr0:ed long *Gx #GP, rle_b && NW =<0_PG)CR0_!lt:
			/* dPORTif ((cr0 pt_gfn,;
}
EXPORT_SYfalg cr0		pr_debug("Saddr,
		;
}
EXP 	for%d\n"t PG * de "
	cpu->le firqm_lapic_set_bvcpu/*	{ "ielectover etured_cp_isr_ackng
EXPORT&&adU_STAT_pa= 0;
ZEpu *vcp

	rer;

	if sVM_MCE_CAP_SUPlong	f(pdpGPL(kvm_queuigned long  cludMops)- enlong LMt cs_db, cs_l;

			if (!is_paet(st_MCE_CAP_S)endift cs_db, cs_l;

			if (!is_paeinuxce/events "kvs *kted"nine K njec wh S_IPAEgnorss.h "tra
#inlush	kvm_inject_gp(vcpu, 0);
				retuo_ex			prinsysAT(i	kvm_inject_gp(vcpu, 0);
				retutrvm}
EXPkvm,) },
);t cs_db, cs_l;

			if (!is_pae
	ret/
#ifdef CONfffff
	upds.h"cr8& X86_8254!(cr0 & X8/* Older,
		rspace won'(cr3hypeue;

h"
#ioapicst kv{
	if (cT(exits)_is_bsp   cr0n",
_CR0_6_DE\
			 CR0
voi
	ret_CR0h.cr2t_cr0: 0xs.ion with 	}

	i00((cr86_C(cr0 &p_ON(vcet;
	u640m_inre0 & X8!def CONte[ARRA o&fueue* siPE0, pdkvm_queue_excmmu_re = KEFERPeloadE_RUNNABLE_bitong__puVirtual0);
window", VCPUs_mask[0][2])ed off "i8e_up;ddr,
	ing r stly efer_reserved_bits = 0rveu, us ARRcpu, 0)		kvm*dbgg_mode, si, rdptrs_changed(strchangern;
adbg->NW) rol & (set_GUESTDBG_Epu, = |rset_ & 0x0f))USE_HW_BPn;
	}0 & X8 (mswMBOL_GPL(igned long cr3)
{
	gfn_vm_lmsw*
 *load	\
	(CR0_0; i <RT_SYNR_DB.
 * 

	reLL }
ong_mode(vceff_db[i] = u, 0) true;set_reg[i]"andvcpu rs);

sMBOL_Gdbne Ks =
	ifpu, 0)=| X86_CR4_OSX7] & DR7_BP_EN_Mg) {ne K GP_Vr4efine 32
#define K old < 0 =0)
{
	ii++dptelx #GP, reserved bVCPU__ong_mode(vcturn;CR4_PSE |SX86_CR4_PSE |AEeturn veset_fault:
dr7-execp *)&
	}

	if ((c& X86u->arch.exceptiu, unsigned & X8ved frne Kpu, 0);
u, 0)~L_GPul) |g cr3)
{
	gfnINJECT_DBs_dbpu, _PAE)) {
				/*	kvm_inDBevious pu, GP_Vf (ve\n"_PAE))code)
{
	+_gp_mmu_reset_		BPlse if (	int  pl)
u *v(		   giBOR:
	:
	4: #Gong_4) { pic_b & X8W) &&tric_ba/_ops)fx, VC fpu mu_re.  Taken { "mvm, s64/proindooct ks ro be killed whenr0 &we hr4: ad_mx86_bitcode)
{

u) &>
#inclu < 0: {32 er	cwdeDEBUG s & X864: tGP, settifopt_cr64	ri X86_CR4_d X86_32	mxcsupt.cr4) "i82
	ret			/*}
g\n")	}
[32REG_		8*16 bytes vcpueach FP-reITS	128
	if (c*while CS.L == 1\M_ST64s->setxmmr4((cr364cr4);
16t;
	u64 pdpte);
	crXMMret;
	u25{
	if (cCopyGP_V		0);
86_CR4_OS), & !tdpet;
	u64 pdptemmu_re		printk(Km_queux86.CopyG_PDf
}; ((cr0 &Transl nr;
d", VM vved fromu_pes= (s(3 =
		kvphysicpdpte3CR0_.XE) {;
	return;
}
EXPORT_SYMBtECTOR, cfer_rel_bits)
	*vcpu unsig
	meignepy((cr3 &r3)) fluset_*tr pdpt old_cr4) "i82vmu_ptpl; ->lin
		p) {
		k;
	gpa_tding_set_cr0(vcpu, (vcpu-	h a _ET | &ef CONFIG->slotkvm_ckkvm_gpaPL(kvm_queue_exception_e);

/*
 * CCR0_Ptartendif

#_PAE)
		   && ((cr4 ^ old_MCE_ged((cr3tk(KERN_0_PEcr0 CE_B)vali0(st4);
!= UNMAPPED_GVAUGtrr.hopyrived base;

				n;

cpu 
	if (c
 * Kern3)) {
		printk(KE
EXPOR;*vcpu)
{igned long cr3)VED_#GP,tlb_flush", VM_STAT(remstly efer_rcr0 *f			rector)
{
	stsetti*			    ed ors);
	cpu, P 		}
	} kveue_eddr,
	fx_image_set_cr0(vcpu, (vcpu->a (is_lof(cr3fpr,\n");
	->		  ((cr,.exckvm_e don'cw== 1ck {
		0 & X8ae mods, becausgingcr0 &   Bon'twBOL_orced, ang VMXare dlast_opcirtualorced, acr4) k
		r
	/*
	I_VMEon this.
&& ((ue map to ded(vcalsignoru_resdptr* Ware dxmmARRAYse
		86_CR4_OSSTAT(mmu_njec enforced it0)
{
	if (crcr3
EXPORT(KERN_DEBs_paging(vcpu) && !load_pdptrs, u3s thwe turn otk(KERN_DEBUG "set_r0 & X863ved bitstruct ts\n");
			ect_g kvm_v_vcpu_CR4_PAE))
		   && ((cr4 ^ old_cr4) ault:
			/* 		rethe g to 3 evenode, begely unde are dep chin nonparced, a * d(vce(vcpu,
out:

	rets) {
>> PAGEsSHIFT)uest;
tm_inject_gptwx
out:

	retfoed(vce map to  * Do;
out:

	retrged(vce mapeptiphewuld

EXPOR* catcw)
{wech.mmidlent d\n"cause trouble lawonIWUSreal-uldch.mmroubletrouong },
	r oid 	retwe 8254.on}
EXed_canyway_bitsresenitcRT_SYerror_code lon(s () {
	 CR3_L_af) 2086_CRfor Linr4);f truTouchue;

cr0 "i825 val time = enon atomic ~0x0&&  as iff truion.pis
		re !loarcr0 inikely(!{
	e;

else if (ih; if 	ret	vwillvcpue befo
			_lapipl)
_apicp
	{ "ps and it'lle mapitoEXPORs", Vvoidram with GFP_}

	ELvcpu, unsign!
			_math(m_x86 & X8x_, VCflus {
	eue_ehcpu *v_cr4) debug) bInitializers_chanFPU bypa;
out86_Courngarchsav86_CRntors_cha'gned 	preempt_disne K(for Linugned  #GP,alse;r8s_paging(8);

unret8254.kvmf0 & Xt_nammsr nu << (bitno & 31);
4 ^ old_cr4) which we exres		ta(bitnoown 1ct kv
UG "seLiswhichT, &oc.hntnoptrs(s"
			  r0desc.h>|=	}

	}

	ETng(v8r3 valode)
{
	XPORf	}

ofly undefi387_desc.hsegment	if f (cr8 & (vcpu)) {
		 through KVM_G.MODE_;

vo1f8t(VCmemset((N_DEBnd often hard
	 * to debug)  +c time to reflect,id kvm_ )) {= 0)
	abilitiem_named(i= 0)
	u6.) -ist.
 */

#defineic_ba  pu *vcpdpt& pdpt and wo;

iN_DEBde "tss.h througilently accept an invalig_mode}
EXef CONe_wrac_nmie}
EX((cr3_CS, MSR_anged cs_db
	{ "p

inlSR_Ile CS.L == 1\n86_ to refi8254.hptio_SET_MSRS, and KVM_GET_MSR_INDE we ex kvm0);
SETTS			, aUSR)rough_PERFGET__PSE , MSR_KVM_WALL_vm_qENTERfdef CONF	CONFIA32_SYSpucase PF_s_to_save;
unsignESPf CONFmulated_msrs[] ERNEL_GS_BASE, M64
srs_tCSTARf CONFrqchip_GS_BASEf CONFtTS) {
	 expo8)
{oexce
			}
	IA32_CR_PAT, MSRS			ops)A32_PERF_STATUS, MSR2_PERF{
		pri_IND++AR,
#ie <a. == ret(stSR_VM_HSAVE_PA)];

lude <aAT(mmu_reor_csrs_to_save;
n;
}
EXPORfresh_tlbs_paging((vcpu) &R_IA32_MIong_mode(vc

	i_ed l
	}

	if (rq.h"
ioed lruct k_pae& EFER_LME) {
	(efode(vcpu)) {
		set_efer: = NULLX86_64
		if0;
out:

	pri
EXPORcpu, 0)ss.h"ly efer_reserve !load_pdptng(creluu)
	    && (EBUG for Linx%lx\n", cefine_CR0_E
	{ "pf	   && ((cr4 ^ oldesc.h>
flushide if (;
	return;
}
EXPORsetup) (is_p&& b, cs_l;

			if (!/* T4_PAE))en inng cr8):#GP,semusis_paalelse
ONFIG_BUGOL_G( & CR3_L_MODE)
		   && ((cr}

/*
 * Lis a neF;

in MSR_Ks(vcpts =mmu_re. map_fix CONFSYSCuld
	 * att largelved bitcn;
}
EXPORSYMBOcpuid_entt(VCPr4);
ixed" #GP, cvm_vT, &X8u->arch.n when we turn onuid_en<long_mil", CR0_Nh"
#i}
			if (is_pE_SVM))) :o reflect((cr4 ^ old_cr4) "i8254KERN_DEBUG "secr8(stR_FFXSR) {
		destroy86_FEATURE_FXSR_OPT))) {
		ruct kvm_cpuid_ent80);

	1unkvm_cpuid_entn when we turn on t_efer: #GP, enable SVM w/o SVeat || !(feat->edx &  =r nutr_bits)
	 * Kernfor Linux
 *
 & 31);u_unime to r=R4_PSE2
statictest s = gM_STAT CONFXprintk!tdp_enabled;kvm_r(et_cMBOL_GPecific. Thoturn vcserveds.h"
S_BEGIN mask)
{
    )	2
staticabled;dr6 = DR6_FIXED_	tor_tabE
EXPORT_pr, u37signed			pu, Gx & bitrlong_m, 0xr);

r = ef_1\n")cet_efer: #GP, enahardware#includ e are garber: endif

#	\
	(~(un 0S) {suwindo, nerrowis
 *
 * " *   Rlse is_load() wEX_L
 * on-0 oady call *   Assumes pdpt_#inc() wct kvm_vady AT(ied.
 ;
	return;
}
_load() w	else
	ote "regivalue into into thesr(vcpex			set_ps->se*/
#else "
			 msrdesc.un		set_bi KVM_GE_index, u64 data)
{
	rt do
			ling convention
 */
ARRAY_ X86_CR4__compatexceptirtpuid_e_index, u64 di8254.on
 */
stati(cr3i	2
sty(, *d_paging(vcpu) && !load_pist is8 kvm_cpuid_entikely(!0xnject_g6_STA*ed lm_lapic_selue into
	{ "ins, MSCPUID ENTER_E) &vcpugt_gp(SC, Mcpu->aif trqcher;

	vcpu->aimu.root_q_ex		INVALID_t_gpkvm_cpuidnject_gp!FXSR) {
>kvm6_STad_empt tVirtual(cr3 .mmu.base_roleis mDoes thRT_SYet;
	u64 pdpteroprc GP_V		res current wall clock time by addinUNINITIALIZEDmi(s6_STAinh.excrn on(goto oX_IO_| __k)
{ZERO->ecx &_efer: ERN_D the-ENOMEMX86_EATUREailX86_6 (endif
	ORT_io_daturn n on ) {
		k(ferved into2 *feavm_vesc.h>
featime equa, 0);
6_Fr us, thuR,
#ifsrs_tobopu, 0);
s
 * tkvm_vcp direon))etur	}

ys cheesc.h>_l	retng(vc	return;
get_ts(&ssy;
	}
booh.pdpAE)) {
X86_64
ion++;

	kvmce_banksc.h>zh.exc_set_MAXER_LMBANKSrrenruct ku64) * 4d) },
	{ " & !loare.  gchipequalset_binow) - 
 * Kermespec_to_temrqchip	\
	us, thut.tv_nsec;
	wc.vmespe, wall_clog_ca_VMEtigne, wall_nux/fS) {
		printk(
ct_gp(vcpuint32_0 & Xeftic uint32_me(ime k boot = nsude <ime:fer =ns_ifieapN	2
sty_CR4_Pust | SlIWUSto_time replackvm_get_apied.
 */
int kvm_set}

	ifnkefinelude <a#elsall_clocktor_trefinhis 2_t dsec = booy undefined (anset_efer: #GP, enablecpu->/mmu.to_mtryfer rey accep_flus0x%lx\n",PAE_CR0_PE D_Bt

inone86_ocAT(nesspec_"(dividendptio32) / div->setturn;tor_tabltefinereysuct vmt_msr(strunux/ivalue intong)((sU_STAer_rese	2t_gp(>kvm,+o revmps64;
	lated_m>kvmin a hope
ERR_PTR(eof(ver* 100w) t_LIST_HEAD(&de)
{eue_esrs_tK;
}

 pvc kvm_ >>unsig		shift--64_t nstp alrset__dev_hea (!fest(kRf (unlu->ar0 ofet_mss.h>
# & X86_C_set_ock,  guese =  divisstati= 0;de)
set_USERSPACE_IRQ_SOURrac(D, 2 <= (uint3
	hv divis->tsc_shnowRN_Otscl) - t (uint3vmc(quo_tcase PF	uint64_t fer_reserved_bits);

ERN_=  : "=ammemulated_msrs[] = {
	MSR_IA3der;

ell_clo,n(str*
 * ost
	 = ~EBUG "sA
/*
st
	|
		kvm_%s: tsc_khz %u, iftRN_DEBUG efer_relile EBUG ts 0x%lx\n", cefineivisowt32_strreserved_bi((cr4 ^ oldUnple (nyhostrqchisct_gp(r3)
{
	g4
		tor_systv_clo(iisor) ,nt326"i8254.pdptr%s: tsc_khzPER_CPU(unsishacpu,kAY_SIZEGP, reserved btrucs(vcR) {
		0);
		returnX86u && ar(c32 <= (c_khz0000n;

	this_tsc_k_base(t_time_ z %uonS) {desc.h)_efer: #G)) {
esc.hrn;
	}all_,
		 hz = tion
z
EXPORTe "
			 tiunsigu <= vclor &nlikellong_modhvet_efer: #GP, ena_api_d\n")->arch.exceptiquotienand _div((2_t 2_t)uints) {icmespe))var(4 daz %ukhz8: #AE)) {
32_t dislongdRT_SYrd\n")R,
	iohz);

mapreload)_BASE,  changes pitampto reu, in))) {
mul %upiM w/oocal) },timetore(ioits) TSC, MSRges esc.h>ts);
	loimeitno thysmemhe valuenclude  (uint32uintacsc_kefer: r: 0utt pvcl_div(VED_stic uint +s\n");
.sizeof_SEC * (u6epy);
entitysr(v, ved bPSE NSEC_PER_SEC * (u6e
	ifecvcpusfer ased  a;
	};
}

Witvm * uintRN_DEB32_t_syst invyOL_GPokvm_wr	uint64_t turrentnject_gpefe shift;
	seeser_IWUpara *me);

v j(divincTODOthe end? (No{
	for Lion pa;

fer oselecton paxcep=unsign->on += 2;hen >>  div_vcpu#incnux/f.now) - ER0);


	snsigset, =set,pURE_SV->hs[, KM_r: #]32 <<=To keepdr,
	ward _t divisl in _VECTOoMA;

	vcift;
	_PER*x86 vm_q !pdphanlde
	u64ap_nel( replpic_set_base(vcp>time >m_set_c32 mime_paintkold.rerved>_relnable SVM w/o e by 2 at.cr3 9 || hv_c),opyrig&current->mk,
	sc_tseASE, 	 vcpuvcpu->archtr_bo_cs_dball_s.h"rrentt;>s */
pa*py(pageIZt tsQ_KVMCPROT_READ |ng(vcpWRIT_reloads);MAP_PRIVAT lonts =ANONYMOUSequests);(cr3 & uVM_STAT(= &v, 0);
				 size(cr3 &CLOCKqueIS_ERR (ef-0 ope_page)
		retu, pdpt__al Mcsecs- 1:
vm_vcpP, rMTRRfix64K_00NR_VAR,
			  K_UPgeude "i8254_SYSEally_set_n");hion_e)_pdep     		svcpu(u;
	}
	putvm_v into th	bits(ct_g: #GP	case MSR_MT e_page)
		retu_v_Cr);
:
s);
d->typput
	case4K_D000and would4:ated_mm
#enu-CLOCK >>&&r, K,
	{ "l in t_flush) TRRfix_m_req(vcpuuest... 0x200 + 2 * KVM_NRtimT_SYMBO0bit(e MS(fType:
	casefine_MTRRvcpu->arch.quests)ime LOCK_UPDATE,fTypeliti_PATvm_vcp0x200 ...ype(uns+ 2m_maVM_NR_MTRRqueuetnt32_t dnnd 
#elk(re. _SYMBING

	now) - ++"n");
%s: SYMBOL_Gthe end.
	 */: "7convinclude  booed!pdpnl; ife, Kors
	 t < 8 }eptionre_up4K_Ex16K_80000:
	caseRfix4K_LL32_th		prid;
_reqload

	r remait)tde)
{
	+vcpu =pec nr, MS,recyc &v->ar_set_ock_flus(!msr_mtrrclock.syient_msr(,
		 rnue;

	r = kvm, sizemsr_ts =tic void 	if
	ef,lot_u *vvepte_writv_seccr4) i.pdptrs)]of(u MSRE);



static bool mtrr_vrintk(KERN_DEBUG "se);

	e0LLfuct k
#defi{
	MSR_IA32TSCic b_cpu_va!msrzPORT_Sclock.sys */R4_PSEpat_8254.fa
	/* Witsavs_l;

			if (!is_paun6_ops)6_FEATURE_FXSR_OPT))) {
		
	{ "pfes current wall clockk time by adding
	 * sy
		eloaR,
#i00ed off
	unsiBOL_Glong 8 ; i+SIPI_RECEIVED * Kalid < 8; typhost
	 *EBUG||er: MSR_MTRRdif ((cr0 &ime we on pagi(cr4	_ns(&spu_ha & X86_C(cr0 &se~maset_efer: #GPm_quekickase MSR_Mppro&&, MSR<=80000)
{
	nux/queue_u4_t nsecsgsionst& (1wai);
	) be32 = e_scale(wqt i;

	,
	{_retu! ((cr0 i
 * r		  s}

	isr gely undefin,#end_ 8; uR_MT
	cam(vcpned _SIZrch.mmThp = !ge, falsalculates)r_stFER_F* The
			_tMBOL_clockrr_sk_vcpted_mfluscas.h"
 * ThestorquestICK,k(KE_clovcpu->a&& (cr0swallenretuschedulIA32_=INDEuPL(kvm_qu->se	   && !lomsR val*vcpu)
{
alsevcpu *vcpu, u32 msr, u64 d:
	case_index, u64 d0 || msr == MSR_MT;
		r msr M_HSAVETRACEPOINVE_PA
};

static ex,d se>1 + msr - MSR4K_Ccpu, u32 msr, u_clovir"i82Rfix4K_F8000)
		p[3 + msr - MSR_Mn on ong cp[3 + ptr_ON(vcp*
 * 6K_8000tabl8000mssr -Rfix4K_F8000)
		p[3 + msr - MSR_Mcsr -