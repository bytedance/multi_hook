
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

// hello test -------------------------------------------------------------------

typedef long (*hello_func_t)(long arg0, long arg1, long arg2);

hello_func_t hello_addr;

long hello_func(long arg0, long arg1, long arg2)
{
	pr_debug("hello_func: arg0: %ld, arg1: %ld, arg2: %ld\n",
		 arg0, arg1, arg2);

	return (arg0 + arg1 + arg2);
}

long hello_hook_func(long arg0, long arg1, long arg2)
{
	pr_debug("hello_hook_func: arg0: %ld, arg1, %ld, arg2: %ld\n",
		 arg0, arg1, arg2);

	return (arg0 + arg1 + arg2) * 2;
}

long hello_post_func(struct hook_pt_regs *ctx)
{
	pr_debug("hello_post_func: ret: %ld\n", ctx->ret);
	return 0;
}


int hello_test_init(void)
{
	int ret;
	long val, val1, val2, val3;
	struct hook_ctx_t *ctx = NULL;

	pr_debug("hello_test_init: 1\n");
	*(unsigned long *)&hello_addr = (unsigned long)hello_func;

	pr_debug("hello_test_init: 2\n");
	val = hello_addr(1, 2, 3);
	pr_debug("val: %ld\n", val);



	pr_debug("hello_test_init: 3\n");
	if (!(ctx = multi_hook_manager_get((unsigned long)&hello_addr, "hello")))
		return -1;

	pr_debug("hello_test_init: 4\n");
	ret = hook_ctx_register_func(ctx, HOOK_CTX_FUNC_PREV_TYPE, 5, (unsigned long)hello_hook_func,
			/* HOOK_ENABLE_POST_RUN */ 0);
	if  (ret < 0)
	{   pr_warn("hello_test_init: hello_hook_func register failed\n");
		return -1;
	}
		
	pr_debug("hello_test_init: 5\n");
	ret = hook_ctx_register_func(ctx, HOOK_CTX_FUNC_POST_TYPE, 0, (unsigned long)hello_post_func,
				   HOOK_ENABLE_POST_RUN);
	if  (ret < 0)
	{   pr_warn("hello_test_init: hello_post_func register failed\n");
		return -1;
	}

	pr_debug("hello_test_init: 6\n");
	val1 = hello_addr(1, 2, 3);
	pr_debug("val1: %ld\n", val1);



	pr_debug("hello_test_init: 7\n");
	ret = hook_ctx_unregister_func(ctx, HOOK_CTX_FUNC_PREV_TYPE, 5);
	if  (ret < 0)
	{   pr_warn("hello_test_init: hello_hook_func unregister failed\n");
		return -1;
	}

	pr_debug("hello_test_init: 8\n");
	ret = hook_ctx_unregister_func(ctx, HOOK_CTX_FUNC_POST_TYPE, 0);
	if  (ret < 0)
	{   pr_warn("hello_test_init: hello_hook_func unregister failed\n");
		return -1;
	}

	pr_debug("hello_test_init: 9\n");
	val2 = hello_addr(1, 2, 3);
	pr_debug("val2: %ld\n", val2);

	pr_debug("hello_test_init: 10\n");
	ret = multi_hook_manager_put((unsigned long)&hello_addr);
	if  (ret < 0)
	{   pr_warn("hello_test_init: multi_hook_manager_put failed\n");
		return -1;
	}

	pr_debug("hello_test_init: 11\n");
	val3 = hello_addr(1, 2, 3);
	pr_debug("val3: %ld\n", val3);
 
	pr_debug("hello_test_init: 11\n");
	return 0;
}

void hello_test_exit(void)
{
	
}

// tcp_v4_syn_recv_sock test -----------------------------------------------------------

unsigned long syn_recv_sock_v4_func_p;
struct hook_ctx_t *syn_recv_sock_v4_hook_ctx;


struct sock *(*tcp_v4_syn_recv_sock_original_func)(const struct sock *sk,
		struct sk_buff *skb, struct request_sock *req, struct dst_entry *dst, 
		struct request_sock *req_unhash, bool *own_req);

struct sock *tcp_v4_syn_recv_sock_hook_func(const struct sock *sk,
		struct sk_buff *skb, struct request_sock *req, struct dst_entry *dst, 
		struct request_sock *req_unhash, bool *own_req)
{
	pr_debug("tcp_v4_syn_recv_sock_hook_func\n");

	// return tcp_v4_syn_recv_sock(sk, skb, req, dst, req_unhash, own_req);
	return tcp_v4_syn_recv_sock_original_func(sk, skb, req, dst, req_unhash, own_req);
}

void tcp_v4_syn_recv_sock_hook_post_func(struct hook_pt_regs *ctx)
{
	pr_debug("tcp_v4_syn_recv_sock_hook_post_func: ret: %lx\n",
			ctx->ret);
}

int syn_recv_sock_test_init(void)
{
	struct inet_connection_sock_af_ops *ipv4_specific_p;
	ipv4_specific_p = (struct inet_connection_sock_af_ops *)
			kallsyms_lookup_name("ipv4_specific");
	if (!ipv4_specific_p) {
		pr_debug("ipv4_specific not found\n");
		return -1;
	}
	syn_recv_sock_v4_func_p = (unsigned long)&ipv4_specific_p->syn_recv_sock;
	pr_debug("func_addr: %lx\n", syn_recv_sock_v4_func_p);

	syn_recv_sock_v4_hook_ctx = multi_hook_manager_get(
			syn_recv_sock_v4_func_p, "tcp_v4_syn_recv_sock");
	if (!syn_recv_sock_v4_hook_ctx)
		return -1;

	if  (hook_ctx_get_original_func(syn_recv_sock_v4_hook_ctx, 
			(unsigned long*)&tcp_v4_syn_recv_sock_original_func) < 0)
		return -1;

	if  (hook_ctx_register_func(syn_recv_sock_v4_hook_ctx, HOOK_CTX_FUNC_PREV_TYPE, 0,
			(unsigned long)tcp_v4_syn_recv_sock_hook_func, 0) < 0)
		return -1;

	if  (hook_ctx_register_func(syn_recv_sock_v4_hook_ctx, HOOK_CTX_FUNC_POST_TYPE, 0,
			(unsigned long)tcp_v4_syn_recv_sock_hook_post_func, HOOK_ENABLE_POST_RUN) < 0)
		return -1;;

	return 0;
}

int syn_recv_sock_test_exit(void)
{
	hook_ctx_unregister_func(syn_recv_sock_v4_hook_ctx, HOOK_CTX_FUNC_PREV_TYPE, 0);
	hook_ctx_unregister_func(syn_recv_sock_v4_hook_ctx, HOOK_CTX_FUNC_POST_TYPE, 0);

	multi_hook_manager_put(syn_recv_sock_v4_func_p);

	return 0;
}

// -----------------------------------------------------------

bool enable_hello_test = false;
bool enable_syn_recv_sock_test = true;

static int __init multi_hook_test_init(void)
{
	pr_info("multi_hook_test_init begin\n");

	if  (enable_hello_test) 
	{   if  (hello_test_init() < 0)
			return -1;
	}

	if  (enable_syn_recv_sock_test)
	{   if (syn_recv_sock_test_init() < 0)
			return -1;
	}

	pr_info("multi_hook_test_init end\n");
	return 0;
}

static void __exit multi_hook_test_exit(void)
{
	pr_info("multi_hook_test_exit begin\n");

	if  (enable_hello_test)
		hello_test_exit();
  
	if  (enable_syn_recv_sock_test)
		syn_recv_sock_test_exit();

	pr_info("multi_hook_test_exit end\n");
	pr_debug("-------------------------------------------------\n");
}

module_init(multi_hook_test_init);
module_exit(multi_hook_test_exit);
MODULE_LICENSE("GPL");
