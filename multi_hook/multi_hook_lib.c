
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include "multi_hook.h"
#include "const_func_hook.h"
#include <linux/sched.h>

static inline bool index_valid(int type, int index)
{
	return (type == 0 && index >= 0 && index <= HOOK_CTX_PREV_NUM - 2)
	    || (type == 1 && index >= 0 && index <= HOOK_CTX_POST_NUM - 1);
}

int hook_ctx_register_func(struct hook_ctx_t *hook_ctx, int type, int index,
			   unsigned long func_addr, unsigned flag)
{
	struct hook_func_t *hook_func = NULL;
	struct hook_func_t *origin_hook_func = NULL;
	int ret = -EINVAL;

	if (!index_valid(type, index))
		return -EINVAL;

	hook_func =
	    (struct hook_func_t *)kmalloc(sizeof(struct hook_func_t),
					  GFP_KERNEL);
	if (!hook_func)
		return -ENOMEM;

	hook_func->func = func_addr;
	hook_func->flag = flag;
	atomic_set(&hook_func->ref_cnt, 0);

	spin_lock_bh(&hook_ctx->lock);

	if (type == 0) {
		origin_hook_func = hook_ctx->prev_func[index];
		rcu_assign_pointer(hook_ctx->prev_func[index], hook_func);
		ret = 0;
	} else if (type == 1) {
		origin_hook_func = hook_ctx->post_func[index];
		rcu_assign_pointer(hook_ctx->post_func[index], hook_func);
		ret = 0;
	}

	spin_unlock_bh(&hook_ctx->lock);

	if (origin_hook_func) {
		synchronize_rcu();
		while (smp_load_acquire((int *)&origin_hook_func->ref_cnt) > 0) {
			cond_resched();
			cpu_relax();
		}
		kfree(origin_hook_func);
	}

	return ret;
}

EXPORT_SYMBOL(hook_ctx_register_func);

int hook_ctx_unregister_func(struct hook_ctx_t *hook_ctx, int type, int index)
{
	struct hook_func_t *origin_hook_func = NULL;
	int ret = -EINVAL;

	spin_lock_bh(&hook_ctx->lock);

	if (type == 0) {
		origin_hook_func = hook_ctx->prev_func[index];
		rcu_assign_pointer(hook_ctx->prev_func[index], NULL);
		ret = 0;
	} else if (type == 1) {
		origin_hook_func = hook_ctx->post_func[index];
		rcu_assign_pointer(hook_ctx->post_func[index], NULL);
		ret = 0;
	}

	spin_unlock_bh(&hook_ctx->lock);

	if (origin_hook_func) {
		synchronize_rcu();
		while (smp_load_acquire((int *)&origin_hook_func->ref_cnt) > 0) {
			cond_resched();
			cpu_relax();
		}
		kfree(origin_hook_func);
	}

	return ret;
}

EXPORT_SYMBOL(hook_ctx_unregister_func);

int hook_ctx_get_original_func(struct hook_ctx_t* hook_ctx, unsigned long* func_addr)
{
	struct hook_func_t *original_hook_func = NULL;
	int ret = -EINVAL;

    rcu_read_lock();

    original_hook_func = rcu_dereference(hook_ctx->prev_func[HOOK_CTX_PREV_NUM - 1]);
	if  (!original_hook_func)
	{   pr_warn("get original_hook_func failed\n");
	    goto out_unlock;
	}

	*func_addr = original_hook_func->func;
	ret = 0;

out_unlock:
	rcu_read_unlock();

	return ret;
}

EXPORT_SYMBOL(hook_ctx_get_original_func);

int hook_ctx_init(struct hook_ctx_t *hook_ctx, unsigned long func_addr,
		  unsigned long new_func)
{
	int ret = -EINVAL;

	struct hook_func_t *hook_func =
	    (struct hook_func_t *)kmalloc(sizeof(struct hook_func_t),
					  GFP_KERNEL);
	if (!hook_func)
		return -ENOMEM;

	memset(hook_ctx->prev_func, 0, sizeof(hook_ctx->prev_func));
	memset(hook_ctx->post_func, 0, sizeof(hook_ctx->post_func));
	hook_func->func = *(unsigned long *)func_addr;
	hook_func->flag = 0;
	atomic_set(&hook_func->ref_cnt, 0);
	hook_ctx->prev_func[HOOK_CTX_PREV_NUM - 1] = hook_func;

	atomic_set(&hook_ctx->call_ref_cnt, 0);

	// for const_func_hook use xchg, so no need smp_wmb here.
	ret =
	    const_func_hook(func_addr, new_func, NULL,
			    &hook_ctx->mapped_func_addr);
	if (ret < 0) {
		pr_warn("const_func_hook failed: %p\n", (void *)func_addr);
		goto hook_failed;
	}

	return 0;

hook_failed:
	kfree(hook_func);

	return ret;
}

EXPORT_SYMBOL(hook_ctx_init);

int hook_ctx_exit(struct hook_ctx_t *hook_ctx)
{
	int i;

	const_func_unhook(hook_ctx->mapped_func_addr,
			  hook_ctx->prev_func[HOOK_CTX_PREV_NUM - 1]->func);

	synchronize_rcu();

	while (smp_load_acquire((int *)&hook_ctx->call_ref_cnt) > 0) {
		cond_resched();
		cpu_relax();
	}

	// this relies on that after hook_ctx exit, not one uses the prev or post funcs.
	spin_lock_bh(&hook_ctx->lock);
	for (i = 0; i < HOOK_CTX_PREV_NUM; i++)
		if (hook_ctx->prev_func[i]) {
			struct hook_func_t *hook_func = hook_ctx->prev_func[i];
			hook_ctx->prev_func[i] = NULL;
			kfree(hook_func);
		}
	for (i = 0; i < HOOK_CTX_POST_NUM; i++)
		if (hook_ctx->post_func[i]) {
			struct hook_func_t *hook_func = hook_ctx->post_func[i];
			hook_ctx->post_func[i] = NULL;
			kfree(hook_func);
		}
	spin_unlock_bh(&hook_ctx->lock);

	return 0;
}

EXPORT_SYMBOL(hook_ctx_exit);
