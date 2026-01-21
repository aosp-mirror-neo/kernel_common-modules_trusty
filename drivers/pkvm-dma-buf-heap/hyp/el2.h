#ifndef __PKVM_DMA_BUF_HEAP_EL2_H
#define __PKVM_DMA_BUF_HEAP_EL2_H

#ifdef __KVM_NVHE_HYPERVISOR__
int hyp_init(const struct pkvm_module_ops *__ops);
/**
 * protect_page() - Protect memory pages from the host.
 * @regs: Pointer to the CPU registers structure containing the hypercall arguments.
 *
 * Revokes host access to a memory range by stripping permissions in the host
 * stage-2 page tables. Expected arguments are the base PFN in regs->regs[1]
 * and the number of pages in regs->regs[2]. The result of the operation is
 * stored back in regs->regs[1].
 */
void protect_page(struct user_pt_regs *regs);

/**
 * unprotect_page() - Restore host access to memory pages.
 * @regs: Pointer to the CPU registers structure containing the hypercall arguments.
 *
 * Restores Read, Write, and Execute (RWX) permissions for the host over a
 * specified memory range in the host stage-2 page tables. Expected arguments
 * are the base PFN in regs->regs[1] and the number of pages in regs->regs[2].
 * The result of the operation is stored back in regs->regs[1].
 */
void unprotect_page(struct user_pt_regs *regs);
#else
int __kvm_nvhe_hyp_init(const struct pkvm_module_ops *__ops);
/**
 * __kvm_nvhe_protect_page() - EL1 alias for protect_page
 */
void __kvm_nvhe_protect_page(struct user_pt_regs *regs);
/**
 * __kvm_nvhe_unprotect_page() - EL1 alias for unprotect_page
 */
void __kvm_nvhe_unprotect_page(struct user_pt_regs *regs);

extern unsigned long protect_page_hvc;
extern unsigned long unprotect_page_hvc;
extern const struct dma_heap_ops system_heap_modified_ops;

#define PKVM_DMA_BUF_HEAP_IOC_MAGIC		'P'
#define PKVM_DMA_BUF_HEAP_IOCTL_ENABLE_SMC	_IOWR(PKVM_DMA_BUF_HEAP_IOC_MAGIC, 0x0, __u32)
#endif /*  __KVM_NVHE_HYPERVISOR__ */

#endif /* __PKVM_DMA_BUF_HEAP_EL2_H */