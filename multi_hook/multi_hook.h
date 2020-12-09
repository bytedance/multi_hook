/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 Bytedance, Inc. All rights reserved.
 *
 * Authors: Qianyu Zhang <zhangqianyu.sys@bytedance.com>
 *
 * the multi_hook provide the ability to expand one function pointer to multi user hooked functionos.
 */

#ifndef MULTI_HOOK_H
#define MULTI_HOOK_H

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/srcu.h>
#include <linux/percpu-refcount.h>

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
	struct percpu_ref call_ref_cnt;
	struct wait_queue_head wq_head;
	int could_exit;
	unsigned flag;
	unsigned long func;
};

#define HOOK_CTX_PREV_NUM 7
#define HOOK_CTX_POST_NUM 7

enum hook_ctx_func_type {
	HOOK_CTX_FUNC_PREV_TYPE = 0,
	HOOK_CTX_FUNC_POST_TYPE = 1,
};

struct hook_ctx_t {
	struct percpu_ref call_ref_cnt;
	struct wait_queue_head wq_head;
	int could_exit;
	spinlock_t lock;	/* rcu_write_lock */
	unsigned long mapped_func_addr;

	struct hook_func_t *prev_func[HOOK_CTX_PREV_NUM];
	struct hook_func_t *post_func[HOOK_CTX_POST_NUM];

	uint64_t __percpu * call_count;
};

static __always_inline long hook_generic_func(struct hook_ctx_t *hook_ctx,
						  long arg0, long arg1, long arg2,
						  long arg3, long arg4, long arg5)
{
	int i;
	uint64_t* call_count_p;
	long ret = -1;
	struct hook_pt_regs post_ctx = {
		.args = { arg0, arg1, arg2, arg3, arg4, arg5 },
	};

	/*
	 * There is an implicit rcu_read_lock critical region 
	 * beginning before upper caller calling this hook_generic_func 
	 * and ending after getting the call_ref_cnt 
	 * which(the rcu_read_lock) relies on our kernel not enabling preemption.
	 * 
	 * The hook_ctx_exit also waits for an rcu period 
	 * after removing the hook_generic_func from the hooked func pointer.
	 * 
	 * So the hook_generic_func's entering and getting reference count is protected by rcu mechanism.
	 */
	percpu_ref_get(&hook_ctx->call_ref_cnt);

	call_count_p = this_cpu_ptr(hook_ctx->call_count);
	(*call_count_p)++;

	for (i = 0; i < HOOK_CTX_PREV_NUM; i++) {
		struct hook_func_t *hook_func;
		bool enable_post = true;

		rcu_read_lock();
		hook_func = rcu_dereference(hook_ctx->prev_func[i]);
		if  (hook_func)
			percpu_ref_get(&hook_func->call_ref_cnt);
		rcu_read_unlock();

		if (hook_func) {
			enable_post = !!(hook_func->flag & HOOK_ENABLE_POST_RUN);

			ret = ((func_t) (hook_func->func)) (arg0, arg1, arg2,
							  arg3, arg4, arg5);
		
			percpu_ref_put(&hook_func->call_ref_cnt);

			if  (!enable_post)
				break;
		}
	}

	post_ctx.ret = ret;
	for (i = 0; i < HOOK_CTX_POST_NUM; i++) {
		struct hook_func_t *hook_func;
		bool enable_post = true;

		rcu_read_lock();
		hook_func = rcu_dereference(hook_ctx->post_func[i]);
		if  (hook_func)
			percpu_ref_get(&hook_func->call_ref_cnt);
		rcu_read_unlock();

		if (hook_func) {
			enable_post = !!(hook_func->flag & HOOK_ENABLE_POST_RUN);

			((post_func_t) (hook_func->func)) (&post_ctx);

			percpu_ref_put(&hook_func->call_ref_cnt);

			if  (!enable_post)
				break;
		}
	}

	percpu_ref_put(&hook_ctx->call_ref_cnt);

	return ret;
}

int hook_ctx_register_func(struct hook_ctx_t *hook_ctx, enum hook_ctx_func_type type, int index,
			   unsigned long func_addr, unsigned flag);

int hook_ctx_unregister_func(struct hook_ctx_t *hook_ctx, enum hook_ctx_func_type type, int index);

int hook_ctx_get_original_func(struct hook_ctx_t *hook_ctx,
				   unsigned long *func_addr);

int hook_ctx_init(struct hook_ctx_t *hook_ctx, unsigned long func_addr,
		  unsigned long new_func);

int hook_ctx_exit(struct hook_ctx_t *hook_ctx);

struct hook_ctx_t *multi_hook_manager_get(unsigned long addr, const char *desc);

int multi_hook_manager_put(unsigned long addr);

#endif /* MULTI_HOOK_H */
