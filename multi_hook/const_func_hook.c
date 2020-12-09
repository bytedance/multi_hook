
#include "const_func_hook.h"

#include <linux/mm.h>
#include <asm/page.h>

int const_func_hook(unsigned long func_addr, unsigned long new_func,
		    unsigned long *old_func_p,
		    unsigned long *mapped_func_addr_p)
{
	unsigned long mapped_func_addr, mapped_page_addr, old_func;
	pte_t *pte_p;
	int level;
	struct page *page;

	if (!mapped_func_addr_p)
		return -EINVAL;

	pte_p = lookup_address(func_addr, &level);
	if (!pte_p)
		return -EINVAL;

	// now only for x86
	if (level == PG_LEVEL_4K) {	// pr_debug("%s: PG_LEVEL_4K\n", __func__);
		page = pte_page(*pte_p);
	} else if (level == PG_LEVEL_2M) {	// pr_debug("%s: PG_LEVEL_2M\n", __func__);
		page = pmd_page(*((pmd_t *) pte_p));
		page += pte_index(func_addr);
	} else {
		pr_debug("%s: pte_p: %d\n", __func__, level);
		return -EINVAL;
	}

	mapped_page_addr = (unsigned long)vmap(&page, 1, VM_MAP, PAGE_KERNEL);
	if (!mapped_page_addr)
		return -EINVAL;

	mapped_func_addr = (mapped_page_addr + (func_addr & ~PAGE_MASK));
	*mapped_func_addr_p = mapped_func_addr;

	old_func = xchg((unsigned long *)mapped_func_addr, new_func);
	if (old_func_p)
		*old_func_p = old_func;

	return 0;
}

void const_func_unhook(unsigned long mapped_func_addr, unsigned long old_func)
{
	unsigned long mapped_page_addr = (mapped_func_addr & PAGE_MASK);

	xchg((unsigned long *)mapped_func_addr, old_func);

	vunmap((void *)mapped_page_addr);
}
