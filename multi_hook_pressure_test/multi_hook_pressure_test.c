
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include "multi_hook.h"

#include <linux/skbuff.h>
#include <linux/netdevice.h>

#include <net/tcp.h>
#include <net/inet_common.h>
#include <net/net_namespace.h>
#include <net/ipv6.h>
#include <net/transp_v6.h>
#include <net/sock.h>

#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>
#include <linux/semaphore.h>


// hello test -------------------------------------------------------------------

typedef long (*hello_func_t)(long arg0, long arg1, long arg2);

hello_func_t hello_addr;


long hello_func(long arg0, long arg1, long arg2)
{
	// pr_debug("hello_func: arg0: %ld, arg1: %ld, arg2: %ld\n",
	// 	 arg0, arg1, arg2);

	return (arg0 + arg1 + arg2);
}


struct hook_ctx_t *hello_ctx = NULL;


long hello_hook_func(long arg0, long arg1, long arg2)
{
	// pr_debug("hello_hook_func: arg0: %ld, arg1, %ld, arg2: %ld\n",
	// 	 arg0, arg1, arg2);

	return (arg0 + arg1 + arg2) * 2;
}

long hello_post_func(struct hook_pt_regs *ctx)
{
	// pr_debug("hello_post_func: ret: %ld\n", ctx->ret);
	return 0;
}


int hello_test_init(void)
{
	int ret;
	long val, val1;
	// struct hook_ctx_t *ctx = NULL;

	pr_debug("hello_test_init: 1\n");
	*(unsigned long *)&hello_addr = (unsigned long)hello_func;

	pr_debug("hello_test_init: 2\n");
	val = hello_addr(1, 2, 3);
	pr_debug("val: %ld\n", val);



	pr_debug("hello_test_init: 3\n");
	if (!(hello_ctx = multi_hook_manager_get((unsigned long)&hello_addr, "hello")))
		return -1;

	pr_debug("hello_test_init: 4\n");
	ret = hook_ctx_register_func(hello_ctx, HOOK_CTX_FUNC_PREV_TYPE, 5, (unsigned long)hello_hook_func,
			/* HOOK_ENABLE_POST_RUN */ 0);
	if  (ret < 0)
	{   pr_warn("hello_test_init: hello_hook_func register failed\n");
		return -1;
	}
		
	pr_debug("hello_test_init: 5\n");
	ret = hook_ctx_register_func(hello_ctx, HOOK_CTX_FUNC_POST_TYPE, 0, (unsigned long)hello_post_func,
				   HOOK_ENABLE_POST_RUN);
	if  (ret < 0)
	{   pr_warn("hello_test_init: hello_post_func register failed\n");
		return -1;
	}

	pr_debug("hello_test_init: 6\n");
	val1 = hello_addr(1, 2, 3);
	pr_debug("val1: %ld\n", val1);



 
	pr_debug("hello_test_init: 11\n");
	return 0;
}

void hello_test_exit(void)
{
	int ret;
	long val2, val3;
	
	pr_debug("hello_test_exit: 1\n");
	ret = hook_ctx_unregister_func(hello_ctx, HOOK_CTX_FUNC_PREV_TYPE, 5);
	if  (ret < 0)
	{   pr_warn("hello_test_exit: hello_hook_func unregister failed\n");
		return;
	}

	pr_debug("hello_test_exit: 2\n");
	ret = hook_ctx_unregister_func(hello_ctx, HOOK_CTX_FUNC_POST_TYPE, 0);
	if  (ret < 0)
	{   pr_warn("hello_test_exit: hello_hook_func unregister failed\n");
		return;
	}

	pr_debug("hello_test_exit: 3\n");
	val2 = hello_addr(1, 2, 3);
	pr_debug("val2: %ld\n", val2);

	pr_debug("hello_test_exit: 4\n");
	ret = multi_hook_manager_put((unsigned long)&hello_addr);
	if  (ret < 0)
	{   pr_warn("hello_test_exit: multi_hook_manager_put failed\n");
		return;
	}

	pr_debug("hello_test_exit: 5\n");
	val3 = hello_addr(1, 2, 3);
	pr_debug("val3: %ld\n", val3);
	
}


// -----------------------------------------------------------


static struct semaphore sem_exit;


struct example_thread_ctx {
	int id;
	uint64_t cnt;
	unsigned long start_jiffies, end_jiffies;

	struct semaphore* sem_exit_p;

};

static int example_thread_ctx_init(struct example_thread_ctx* ctx, int id, struct semaphore* sem_exit_p)
{
	ctx->id = id;
	ctx->cnt = 0;
	ctx->sem_exit_p = sem_exit_p;
	return 0;
}


/*
 * Prevent the kthread exits directly, and make sure when kthread_stop()
 * is called to stop a kthread, it is still alive. If a kthread might be
 * stopped by CACHE_SET_IO_DISABLE bit set, wait_for_kthread_stop() is
 * necessary before the kthread returns.
 */
static inline void wait_for_kthread_stop(void)
{
	pr_debug("wait_for_kthread_stop: 1\n");
	while (!kthread_should_stop()) {

		pr_debug("wait_for_kthread_stop: 2\n");
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
	}

	pr_debug("wait_for_kthread_stop: 3\n");
}



static int example_thread_run(void *arg)
{
	int i, j;
	int cnt = 1e7;
	struct example_thread_ctx* ctx = (struct example_thread_ctx*)arg;

	pr_debug("example_thread: 1, id: %d\n", ctx->id);
	ctx->start_jiffies = jiffies;


	for (i = 0; i < cnt && !kthread_should_stop(); i++)
	{
		// pr_debug("example_thread: 2, i: %d\n", i);

		for (j = 0; j < 1000; j++)
		{
			hello_addr(1, 2, 3);
		}

		ctx->cnt += 1000;
		cond_resched();
	}

	ctx->end_jiffies = jiffies;

	up(ctx->sem_exit_p);

	pr_debug("example_thread: 3\n");
	wait_for_kthread_stop();

	pr_debug("example_thread: 5\n");
	return 0;
}


#define NR_THREADS 4
static struct task_struct* example_threads[NR_THREADS];
static struct example_thread_ctx example_thread_ctxs[NR_THREADS];



static int __init multi_hook_test_init(void)
{
	int i;
	pr_info("multi_hook_test_init begin\n");

	if  (hello_test_init() < 0)
		return -1;
	

	sema_init(&sem_exit, 0);


	for (i = 0; i < NR_THREADS; i++)
		example_thread_ctx_init(&example_thread_ctxs[i], i, &sem_exit);

	for (i = 0; i < NR_THREADS; i++)
	{
		example_threads[i] = kthread_run(example_thread_run, &example_thread_ctxs[i], "kthread_example");
		if  (!example_threads[i])
			return -1;
	}

	// for (i = 0; i < NR_THREADS; i++)
	// {
	//	 while (down_interruptible(&sem_exit) < 0)
	//		 pr_debug("Interrupted during semaphore wait\n");
	// }


	pr_info("multi_hook_test_init end\n");
	return 0;
}

static void __exit multi_hook_test_exit(void)
{
	int i;
	uint64_t total_cnt = 0;
	unsigned long total_jiffies = 0;
	pr_info("multi_hook_test_exit begin\n");

  
	for (i = 0; i < NR_THREADS; i++)
	{   kthread_stop(example_threads[i]);
	}


	hello_test_exit();

	for (i = 0; i < NR_THREADS; i++)
	{
		total_cnt += example_thread_ctxs[i].cnt;
		total_jiffies += example_thread_ctxs[i].end_jiffies - example_thread_ctxs[i].start_jiffies;
	}
		

	pr_info("total call count: %llu\n", total_cnt);
	pr_info("total jiffies: %lu\n", total_jiffies);
	pr_info("call count per second: %llu\n", total_cnt / (total_jiffies / HZ));


	pr_info("multi_hook_test_exit end\n");
	pr_debug("-------------------------------------------------\n");
}

module_init(multi_hook_test_init);
module_exit(multi_hook_test_exit);
MODULE_LICENSE("GPL");
