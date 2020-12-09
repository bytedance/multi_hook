/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 Bytedance, Inc. All rights reserved.
 *
 * Authors: Qianyu Zhang <zhangqianyu.sys@bytedance.com>
 *
 * this file provides basic operations for hook_ctx.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

// #include <linux/multi_hook/multi_hook.h>
#include "multi_hook.h"
// #include <linux/multi_hook/const_func_hook.h>
#include "const_func_hook.h"
#include <linux/sched.h>


static inline bool index_valid(enum hook_ctx_func_type type, int index)
{
	return (type == HOOK_CTX_FUNC_PREV_TYPE && index >= 0 && index <= HOOK_CTX_PREV_NUM - 2)
		|| (type == HOOK_CTX_FUNC_POST_TYPE && index >= 0 && index <= HOOK_CTX_POST_NUM - 1);
}

static void hook_func_put_destroy(struct percpu_ref *call_ref_cnt)
{
	struct hook_func_t* hook_func = container_of(call_ref_cnt, struct hook_func_t, call_ref_cnt);

	hook_func->could_exit = 1;
	wake_up(&hook_func->wq_head);
}

static int hook_func_init(struct hook_func_t* hook_func, unsigned long  func_addr, unsigned flag)
{
	int ret;

	hook_func->func = func_addr;
	hook_func->flag = flag;
	hook_func->could_exit = 0;
	init_waitqueue_head(&hook_func->wq_head);
	ret = percpu_ref_init(&hook_func->call_ref_cnt, hook_func_put_destroy, 0, GFP_KERNEL);
	if  (ret < 0)
		return ret;

	return 0;
}

static void hook_func_exit(struct hook_func_t* hook_func)
{
	percpu_ref_exit(&hook_func->call_ref_cnt);
}


int hook_ctx_register_func(struct hook_ctx_t *hook_ctx, enum hook_ctx_func_type type, int index,
			   unsigned long func_addr, unsigned int flag)
{
	int ret;
	struct hook_func_t *hook_func;
	struct hook_func_t *origin_hook_func = NULL;

	/* 
	 * the index_valid() must confirm that the type is just HOOK_CTX_FUNC_PREV_TYPE or HOOK_CTX_FUNC_POST_TYPE
	 * and then origin_hook_func will be assigned definitely.
	 */
	if (!index_valid(type, index))
		return -EINVAL;

	hook_func = kmalloc(sizeof(*hook_func), GFP_KERNEL);
	if (!hook_func)
		return -ENOMEM;

	ret = hook_func_init(hook_func, func_addr, flag);
	if  (ret < 0)
		goto hook_func_init_failed_at_hook_ctx_register_func;


	spin_lock_bh(&hook_ctx->lock);

	if (type == HOOK_CTX_FUNC_PREV_TYPE) {
		origin_hook_func = hook_ctx->prev_func[index];
		rcu_assign_pointer(hook_ctx->prev_func[index], hook_func);
	} else if (type == HOOK_CTX_FUNC_POST_TYPE) {
		origin_hook_func = hook_ctx->post_func[index];
		rcu_assign_pointer(hook_ctx->post_func[index], hook_func);
	}

	spin_unlock_bh(&hook_ctx->lock);

	if (origin_hook_func) {		
		synchronize_rcu();
		percpu_ref_kill(&origin_hook_func->call_ref_cnt);
		wait_event(origin_hook_func->wq_head, origin_hook_func->could_exit == 1);

		hook_func_exit(origin_hook_func);
		kfree(origin_hook_func);
	}

	return 0;

hook_func_init_failed_at_hook_ctx_register_func:
	kfree(hook_func);

	return ret;
}

EXPORT_SYMBOL(hook_ctx_register_func);

int hook_ctx_unregister_func(struct hook_ctx_t *hook_ctx, enum hook_ctx_func_type type, int index)
{
	struct hook_func_t *origin_hook_func;

	/* 
	 * the index_valid() must confirm that the type is just HOOK_CTX_FUNC_PREV_TYPE or HOOK_CTX_FUNC_POST_TYPE
	 * and then origin_hook_func will be assigned definitely.
	 */
	if (!index_valid(type, index))
		return -EINVAL;

	spin_lock_bh(&hook_ctx->lock);

	if (type == HOOK_CTX_FUNC_PREV_TYPE) {
		origin_hook_func = hook_ctx->prev_func[index];
		rcu_assign_pointer(hook_ctx->prev_func[index], NULL);
	} else if (type == HOOK_CTX_FUNC_POST_TYPE) {
		origin_hook_func = hook_ctx->post_func[index];
		rcu_assign_pointer(hook_ctx->post_func[index], NULL);
	}

	spin_unlock_bh(&hook_ctx->lock);

	if (origin_hook_func) {
		synchronize_rcu();
		percpu_ref_kill(&origin_hook_func->call_ref_cnt);
		wait_event(origin_hook_func->wq_head, origin_hook_func->could_exit == 1);

		hook_func_exit(origin_hook_func);
		kfree(origin_hook_func);
	}

	return 0;
}

EXPORT_SYMBOL(hook_ctx_unregister_func);

int hook_ctx_get_original_func(struct hook_ctx_t *hook_ctx,
				   unsigned long *func_addr)
{
	struct hook_func_t *original_hook_func;
	int ret = -EINVAL;

	rcu_read_lock();

	original_hook_func = rcu_dereference(hook_ctx->prev_func[HOOK_CTX_PREV_NUM - 1]);
	if (!original_hook_func) {
		pr_warn("get original_hook_func failed\n");
		goto out_unlock;
	}

	*func_addr = original_hook_func->func;
	ret = 0;

out_unlock:
	rcu_read_unlock();

	return ret;
}

EXPORT_SYMBOL(hook_ctx_get_original_func);


static void hook_ctx_put_destroy(struct percpu_ref *call_ref_cnt)
{
	struct hook_ctx_t* hook_ctx = container_of(call_ref_cnt, struct hook_ctx_t, call_ref_cnt);

	hook_ctx->could_exit = 1;
	wake_up(&hook_ctx->wq_head);
}

int hook_ctx_init(struct hook_ctx_t *hook_ctx, unsigned long func_addr,
		  unsigned long new_func)
{
	int ret;
	int i;

	struct hook_func_t *hook_func = kmalloc(sizeof(*hook_func), GFP_KERNEL);
	if (!hook_func) {
		ret = -ENOMEM;
		pr_warn("hook_ctx_init: hook_func alloc failed\n");
		goto hook_func_alloc_failed;
	}

	ret = hook_func_init(hook_func, *(unsigned long *)func_addr, 0);
	if  (ret < 0) {
		pr_warn("hook_ctx_init: hook_func init failed\n");
		goto hook_func_init_failed_at_hook_ctx_init;
	}

	memset(hook_ctx->prev_func, 0, sizeof(hook_ctx->prev_func));
	memset(hook_ctx->post_func, 0, sizeof(hook_ctx->post_func));
	hook_ctx->prev_func[HOOK_CTX_PREV_NUM - 1] = hook_func;


	hook_ctx->call_count = alloc_percpu(uint64_t);
	if  (!hook_ctx->call_count) {
		ret = -ENOMEM;
		pr_warn("hook_ctx_init: call_count init failed\n");
		goto call_count_init_failed;
	}
	for_each_possible_cpu(i) {
		uint64_t* call_count_p = per_cpu_ptr(hook_ctx->call_count, i);
		*call_count_p = 0;
	}


	hook_ctx->could_exit = 0;
	init_waitqueue_head(&hook_ctx->wq_head);

	ret = percpu_ref_init(&hook_ctx->call_ref_cnt, hook_ctx_put_destroy, 0, GFP_KERNEL);
	if  (ret < 0) {
		pr_warn("hook_ctx_init: percpu_ref_init failed, ret: %d\n", ret);
		goto percpu_ref_failed;
	}

	/* for const_func_hook use xchg, so no need smp_wmb here. */
	ret = const_func_hook(func_addr, new_func, NULL, &hook_ctx->mapped_func_addr);
	if (ret < 0) {
		pr_warn("hook_ctx_init: const_func_hook failed: %pS\n", (void *)func_addr);
		goto hook_failed;
	}

	return 0;

hook_failed:
 	percpu_ref_exit(&hook_ctx->call_ref_cnt);
percpu_ref_failed:
	free_percpu(hook_ctx->call_count);
call_count_init_failed:
	hook_func_exit(hook_func);
hook_func_init_failed_at_hook_ctx_init:
	kfree(hook_func);
hook_func_alloc_failed:

	return ret;
}

EXPORT_SYMBOL(hook_ctx_init);

int hook_ctx_exit(struct hook_ctx_t *hook_ctx)
{
	int i;

	const_func_unhook(hook_ctx->mapped_func_addr,
			  hook_ctx->prev_func[HOOK_CTX_PREV_NUM - 1]->func);

	synchronize_rcu();

	percpu_ref_kill(&hook_ctx->call_ref_cnt);
	wait_event(hook_ctx->wq_head, hook_ctx->could_exit == 1);

	percpu_ref_exit(&hook_ctx->call_ref_cnt);

	/* at this stage, there should be not one using the hook_ctx. */

	/* this relies on that after hook_ctx exit, not one uses the prev or post funcs. */
	spin_lock_bh(&hook_ctx->lock);
	for (i = 0; i < HOOK_CTX_PREV_NUM; i++) {
		if (hook_ctx->prev_func[i]) {
			struct hook_func_t *hook_func = hook_ctx->prev_func[i];
			hook_ctx->prev_func[i] = NULL;

			hook_func_exit(hook_func);
			kfree(hook_func);
		}
	}
	for (i = 0; i < HOOK_CTX_POST_NUM; i++) {
		if (hook_ctx->post_func[i]) {
			struct hook_func_t *hook_func = hook_ctx->post_func[i];
			hook_ctx->post_func[i] = NULL;

			hook_func_exit(hook_func);
			kfree(hook_func);
		}
	}
	spin_unlock_bh(&hook_ctx->lock);

	free_percpu(hook_ctx->call_count);
	hook_ctx->call_count = NULL;

	return 0;
}

EXPORT_SYMBOL(hook_ctx_exit);
