// SPDX-License-Identifier: GPL-2.0

#include "linux/array_size.h"
#include <asm/esr.h>
#include <asm/kvm_pkvm.h>
#include <asm/kvm_pkvm_module.h>
#include <asm/word-at-a-time.h>
#include <kunit/test.h>
#include <linux/dma-heap.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/printk.h>

#include "el2.h"

#ifndef MODULE
BUILD_BUG("Example pKVM dma-buf heap must be compiled as a module");
#endif

int protect_page_hvc, unprotect_page_hvc;

static long pkvm_dma_buf_heap_ioctl(struct file *filp, unsigned int ioctl, unsigned long arg)
{
	struct file *f;
	int ret;

	if (ioctl != PKVM_DMA_BUF_HEAP_IOCTL_ENABLE_SMC)
		return -ENOTSUPP;
	f = fget(arg);
	if (!f)
		return -EINVAL;

	ret = pkvm_enable_smc_forwarding(f);
	fput(f);

	return ret;
}

static struct file_operations pkvm_dma_buf_heap_ops = {
	.unlocked_ioctl = pkvm_dma_buf_heap_ioctl,
};

static struct miscdevice pkvm_dma_buf_heap_dev = {
	MISC_DYNAMIC_MINOR,
	"pkvm_dma_buf_heap",
	&pkvm_dma_buf_heap_ops,
};

static int pkvm_dma_buf_heap_init_config(void)
{
	struct device_node *np;
	struct resource res;
	u32 tokens[MAX_CONFIGURED_VMS * 2];
	int count, i, ret = 0;

	np = of_find_compatible_node(NULL, NULL, "pkvmvendor,auth-token");
	if (!np) {
		pr_warn("pkvm_dma_buf_heap: 'pkvmvendor,auth-token' node not found\n");
		return 0;
	}

	ret = of_address_to_resource(np, 0, &res);
	if (ret) {
		pr_err("pkvm_dma_buf_heap: failed to get address for %pOF (ret=%d)\n",
		       np, ret);
		goto out_put;
	}

	count = of_property_read_variable_u32_array(np, "tokens", tokens, 2,
						    ARRAY_SIZE(tokens));
	if (count < 0) {
		pr_err("pkvm_dma_buf_heap: failed to read 'tokens' in %pOF (ret=%d)\n",
		       np, count);
		ret = count;
		goto out_put;
	}

	if (count % 2 != 0) {
		pr_err("pkvm_dma_buf_heap: 'tokens' property must be pairs in %pOF\n",
		       np);
		ret = -EINVAL;
		goto out_put;
	}

	/*
	 * Validate only in this loop to avoid mutating if we
	 * return an error.
	 */
	for (i = 0; i < count / 2; i++) {
		u32 token_offset = tokens[i * 2];

		if (token_offset > resource_size(&res) ||
		    resource_size(&res) - token_offset < AUTH_TOKEN_SIZE) {
			pr_err("pkvm_dma_buf_heap: token out of bounds for %pOF\n",
			       np);
			ret = -EINVAL;
			goto out_put;
		}
	}

	for (i = 0; i < count / 2; i++) {
		u32 token_offset = tokens[i * 2];
		u32 protection_id = tokens[i * 2 + 1];

		kvm_nvhe_sym(global_pkvm_heap_config).entries[i].token_paddr =
			res.start + token_offset;
		kvm_nvhe_sym(global_pkvm_heap_config).entries[i].protection_id =
			protection_id;
	}

	kvm_nvhe_sym(global_pkvm_heap_config).num_entries = i;

out_put:
	of_node_put(np);
	return ret;
}

static int __init host_init(void)
{
	struct dma_heap_export_info exp_info;
	struct dma_heap *ex_heap;
	int ret;

	exp_info.name = "pkvm_dma_buf_heap";
	exp_info.ops = &system_heap_modified_ops;
	exp_info.priv = NULL;

	ex_heap = dma_heap_add(&exp_info);
	if (IS_ERR(ex_heap))
		return PTR_ERR(ex_heap);

	ret = pkvm_dma_buf_heap_init_config();
	if (ret)
		return ret;

	ret = pkvm_load_el2_module(__kvm_nvhe_hyp_init);
	if (ret) {
		pr_err("pkvm_dma_buf_heap: Failed to load EL2 module: %d\n", ret);
		return ret;
	}

	protect_page_hvc = pkvm_register_el2_mod_call(__kvm_nvhe_protect_page);
	if (protect_page_hvc < 0) {
		pr_err("pkvm_dma_buf_heap: Failed to register __kvm_nvhe_protect_page: %d\n",
		       protect_page_hvc);
		return -EINVAL;
	}

	unprotect_page_hvc =
		pkvm_register_el2_mod_call(__kvm_nvhe_unprotect_page);
	if (unprotect_page_hvc < 0) {
		pr_err("pkvm_dma_buf_heap: Failed to register __kvm_nvhe_unprotect_page: %d\n",
		       unprotect_page_hvc);
		return -EINVAL;
	}

	return misc_register(&pkvm_dma_buf_heap_dev);
}
module_init(host_init);

MODULE_DESCRIPTION("pKVM dma-buf heap");
MODULE_LICENSE("GPL v2");
MODULE_IMPORT_NS("DMA_BUF");
MODULE_IMPORT_NS("DMA_BUF_HEAP");
