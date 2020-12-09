#ifndef CONST_FUNC_HOOK_H
#define CONST_FUNC_HOOK_H

int const_func_hook(unsigned long func_addr, unsigned long new_func,
		    unsigned long *old_func_p,
		    unsigned long *mapped_func_addr_p);

void const_func_unhook(unsigned long mapped_func_addr, unsigned long old_func);

#endif // CONST_FUNC_HOOK_H
