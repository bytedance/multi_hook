#ifndef MULTI_HOOK_H
#define MULTI_HOOK_H

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>

#include <linux/slab.h>

typedef long (*func_t)(long arg0, long arg1, long arg2, long arg3, long arg4,
		       long arg5);

struct hook_pt_regs {
	long args[6];
	long ret;
	long blank;
};

typedef long (*post_func_t)(struct hook_pt_regs * ctx);

#define HOOK_ENABLE_POST_RUN 0x1

struct hook_func_t {
	unsigned long func;
	unsigned flag;
	atomic_t ref_cnt;
};

#define HOOK_CTX_PREV_NUM 7
#define HOOK_CTX_POST_NUM 7

struct hook_ctx_t {
	atomic_t call_ref_cnt;
	spinlock_t lock;	// rcu_write_lock;
	unsigned long mapped_func_addr;

	struct hook_func_t *prev_func[HOOK_CTX_PREV_NUM];
	struct hook_func_t *post_func[HOOK_CTX_POST_NUM];
};

static __always_inline long hook_generic_func(struct hook_ctx_t *hook_ctx,
					      long arg0, long arg1, long arg2,
					      long arg3, long arg4, long arg5)
{
	int i;
	long ret = -1;
	struct hook_pt_regs post_ctx = {
		.args = { arg0, arg1, arg2, arg3, arg4, arg5 },
		.ret = 0,
		.blank = 0
	};

	atomic_inc(&hook_ctx->call_ref_cnt);

	for (i = 0; i < HOOK_CTX_PREV_NUM; i++) {
		struct hook_func_t *hook_func = NULL;
		rcu_read_lock();
		hook_func = rcu_dereference(hook_ctx->prev_func[i]);
		if (hook_func)
			atomic_inc(&hook_func->ref_cnt);
		rcu_read_unlock();

		if (hook_func) {
			unsigned flag = hook_func->flag;

			ret =
			    ((func_t) (hook_func->func)) (arg0, arg1, arg2,
							  arg3, arg4, arg5);

			smp_mb__before_atomic();
			atomic_dec(&hook_func->ref_cnt);

			if (!(flag & HOOK_ENABLE_POST_RUN))
				break;
		}
	}

	post_ctx.ret = ret;
	for (i = 0; i < HOOK_CTX_POST_NUM; i++) {
		struct hook_func_t *hook_func = NULL;
		rcu_read_lock();
		hook_func = rcu_dereference(hook_ctx->post_func[i]);
		if (hook_func)
			atomic_inc(&hook_func->ref_cnt);
		rcu_read_unlock();

		if (hook_func) {
			unsigned flag = hook_func->flag;

			((post_func_t) (hook_func->func)) (&post_ctx);

			smp_mb__before_atomic();
			atomic_dec(&hook_func->ref_cnt);

			if (!(flag & HOOK_ENABLE_POST_RUN))
				break;
		}
	}

	smp_mb__before_atomic();
	atomic_dec(&hook_ctx->call_ref_cnt);

	return ret;
}

int hook_ctx_register_func(struct hook_ctx_t *hook_ctx, int type, int index,
			   unsigned long func_addr, unsigned flag);

int hook_ctx_unregister_func(struct hook_ctx_t *hook_ctx, int type, int index);

int hook_ctx_get_original_func(struct hook_ctx_t* hook_ctx, unsigned long* func_addr);

int hook_ctx_init(struct hook_ctx_t *hook_ctx, unsigned long func_addr,
		  unsigned long new_func);

int hook_ctx_exit(struct hook_ctx_t *hook_ctx);

struct hook_ctx_t *multi_hook_manager_get(unsigned long addr, const char *desc);

int multi_hook_manager_put(unsigned long addr);

#endif
