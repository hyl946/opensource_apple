/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * @OSF_FREE_COPYRIGHT@
 */
/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988,1987 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 */
/*
 *	File:	kern/thread.c
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young, David Golub
 *	Date:	1986
 *
 *	Thread/thread_shuttle management primitives implementation.
 */
/*
 * Copyright (c) 1993 The University of Utah and
 * the Computer Systems Laboratory (CSL).  All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 */

#include <cpus.h>
#include <mach_host.h>
#include <simple_clock.h>
#include <mach_debug.h>
#include <mach_prof.h>
#include <stack_usage.h>

#include <mach/boolean.h>
#include <mach/policy.h>
#include <mach/thread_info.h>
#include <mach/thread_special_ports.h>
#include <mach/thread_status.h>
#include <mach/time_value.h>
#include <mach/vm_param.h>
#include <kern/ast.h>
#include <kern/cpu_data.h>
#include <kern/counters.h>
#include <kern/etap_macros.h>
#include <kern/ipc_mig.h>
#include <kern/ipc_tt.h>
#include <kern/mach_param.h>
#include <kern/machine.h>
#include <kern/misc_protos.h>
#include <kern/processor.h>
#include <kern/queue.h>
#include <kern/sched.h>
#include <kern/sched_prim.h>
#include <kern/sf.h>
#include <kern/mk_sp.h>	/*** ??? fix so this can be removed ***/
#include <kern/task.h>
#include <kern/thread.h>
#include <kern/thread_act.h>
#include <kern/thread_swap.h>
#include <kern/host.h>
#include <kern/zalloc.h>
#include <vm/vm_kern.h>
#include <ipc/ipc_kmsg.h>
#include <ipc/ipc_port.h>
#include <machine/thread.h>		/* for MACHINE_STACK */
#include <kern/profile.h>
#include <kern/assert.h>
#include <sys/kdebug.h>

/*
 * Exported interfaces
 */

#include <mach/thread_act_server.h>
#include <mach/mach_host_server.h>

/*
 * Per-Cpu stashed global state
 */
vm_offset_t			active_stacks[NCPUS];	/* per-cpu active stacks	*/
vm_offset_t			kernel_stack[NCPUS];	/* top of active stacks		*/
thread_act_t		active_kloaded[NCPUS];	/*  + act if kernel loaded	*/

struct zone			*thread_shuttle_zone;

queue_head_t		reaper_queue;
decl_simple_lock_data(,reaper_lock)
thread_call_t		thread_reaper_call;

extern int		tick;

extern void		pcb_module_init(void);

/* private */
static struct thread_shuttle	thr_sh_template;

#if	MACH_DEBUG
#if	STACK_USAGE
static void	stack_init(vm_offset_t stack, unsigned int bytes);
void		stack_finalize(vm_offset_t stack);
vm_size_t	stack_usage(vm_offset_t stack);
#else	/*STACK_USAGE*/
#define stack_init(stack, size)
#define stack_finalize(stack)
#define stack_usage(stack) (vm_size_t)0
#endif	/*STACK_USAGE*/

#ifdef	MACHINE_STACK
extern
#endif
    void	stack_statistics(
			unsigned int	*totalp,
			vm_size_t	*maxusagep);

#define	STACK_MARKER	0xdeadbeef
#if	STACK_USAGE
boolean_t		stack_check_usage = TRUE;
#else	/* STACK_USAGE */
boolean_t		stack_check_usage = FALSE;
#endif	/* STACK_USAGE */
decl_simple_lock_data(,stack_usage_lock)
vm_size_t		stack_max_usage = 0;
vm_size_t		stack_max_use = KERNEL_STACK_SIZE - 64;
#endif	/* MACH_DEBUG */

/* Forwards */
void		thread_collect_scan(void);

kern_return_t thread_create_shuttle(
	thread_act_t			thr_act,
	integer_t				priority,
	void					(*start)(void),
	thread_t				*new_thread);

extern void		Load_context(
	thread_t                thread);


/*
 *	Machine-dependent code must define:
 *		thread_machine_init
 *		thread_machine_terminate
 *		thread_machine_collect
 *
 *	The thread->pcb field is reserved for machine-dependent code.
 */

#ifdef	MACHINE_STACK
/*
 *	Machine-dependent code must define:
 *		stack_alloc_try
 *		stack_alloc
 *		stack_free
 *		stack_collect
 *	and if MACH_DEBUG:
 *		stack_statistics
 */
#else	/* MACHINE_STACK */
/*
 *	We allocate stacks from generic kernel VM.
 *	Machine-dependent code must define:
 *		machine_kernel_stack_init
 *
 *	The stack_free_list can only be accessed at splsched,
 *	because stack_alloc_try/thread_invoke operate at splsched.
 */

decl_simple_lock_data(,stack_lock_data)         /* splsched only */
#define stack_lock()	simple_lock(&stack_lock_data)
#define stack_unlock()	simple_unlock(&stack_lock_data)

vm_offset_t stack_free_list;		/* splsched only */
unsigned int stack_free_max = 0;
unsigned int stack_free_count = 0;	/* splsched only */
unsigned int stack_free_limit = 1;	/* patchable */

unsigned int stack_alloc_hits = 0;	/* debugging */
unsigned int stack_alloc_misses = 0;	/* debugging */

unsigned int stack_alloc_total = 0;
unsigned int stack_alloc_hiwater = 0;

/*
 *	The next field is at the base of the stack,
 *	so the low end is left unsullied.
 */

#define stack_next(stack) (*((vm_offset_t *)((stack) + KERNEL_STACK_SIZE) - 1))

/*
 *	stack_alloc:
 *
 *	Allocate a kernel stack for an activation.
 *	May block.
 */
vm_offset_t
stack_alloc(
	thread_t thread,
	void (*start_pos)(thread_t))
{
	vm_offset_t stack;
	spl_t	s;

	/*
	 *	We first try the free list.  It is probably empty,
	 *	or stack_alloc_try would have succeeded, but possibly
	 *	a stack was freed before the swapin thread got to us.
	 */

	s = splsched();
	stack_lock();
	stack = stack_free_list;
	if (stack != 0) {
		stack_free_list = stack_next(stack);
		stack_free_count--;
	}
	stack_unlock();
	splx(s);

	if (stack == 0) {
		/*
		 *	Kernel stacks should be naturally aligned,
		 *	so that it is easy to find the starting/ending
		 *	addresses of a stack given an address in the middle.
		 */

		if (kmem_alloc_aligned(kernel_map, &stack,
				round_page(KERNEL_STACK_SIZE)) != KERN_SUCCESS)
			panic("stack_alloc");

		stack_alloc_total++;
		if (stack_alloc_total > stack_alloc_hiwater)
		  stack_alloc_hiwater = stack_alloc_total;

#if	MACH_DEBUG
		stack_init(stack, round_page(KERNEL_STACK_SIZE));
#endif	/* MACH_DEBUG */

		/*
		 * If using fractional pages, free the remainder(s)
		 */
		if (KERNEL_STACK_SIZE < round_page(KERNEL_STACK_SIZE)) {
		    vm_offset_t ptr  = stack + KERNEL_STACK_SIZE;
		    vm_offset_t endp = stack + round_page(KERNEL_STACK_SIZE);
		    while (ptr < endp) {
#if	MACH_DEBUG
			    /*
			     * We need to initialize just the end of the 
			     * region.
			     */
			    stack_init(ptr, (unsigned int) (endp - ptr));
#endif
				stack_lock();
				stack_next(stack) = stack_free_list;
				stack_free_list = stack;
				if (++stack_free_count > stack_free_max)
				  stack_free_max = stack_free_count;
				stack_unlock();
			    ptr += KERNEL_STACK_SIZE;
		    }
		}
	}
	stack_attach(thread, stack, start_pos);
	return (stack);
}

/*
 *	stack_free:
 *
 *	Free a kernel stack.
 *	Called at splsched.
 */

void
stack_free(
	thread_t thread)
{
    vm_offset_t stack = stack_detach(thread);
	assert(stack);
	if (stack != thread->stack_privilege) {
	  stack_lock();
	  stack_next(stack) = stack_free_list;
	  stack_free_list = stack;
	  if (++stack_free_count > stack_free_max)
		stack_free_max = stack_free_count;
	  stack_unlock();
	}
}

/*
 *	stack_collect:
 *
 *	Free excess kernel stacks.
 *	May block.
 */

void
stack_collect(void)
{
	register vm_offset_t stack;
	spl_t	s;

	/* If using fractional pages, Cannot just call kmem_free(),
	 * and we're too lazy to coalesce small chunks.
	 */
	if (KERNEL_STACK_SIZE < round_page(KERNEL_STACK_SIZE))
		return;

	s = splsched();
	stack_lock();
	while (stack_free_count > stack_free_limit) {
		stack = stack_free_list;
		stack_free_list = stack_next(stack);
		stack_free_count--;
		stack_unlock();
		splx(s);

#if	MACH_DEBUG
		stack_finalize(stack);
#endif	/* MACH_DEBUG */
		kmem_free(kernel_map, stack, KERNEL_STACK_SIZE);

		s = splsched();
		stack_alloc_total--;
		stack_lock();
	}
	stack_unlock();
	splx(s);
}


#if	MACH_DEBUG
/*
 *	stack_statistics:
 *
 *	Return statistics on cached kernel stacks.
 *	*maxusagep must be initialized by the caller.
 */

void
stack_statistics(
	unsigned int	*totalp,
	vm_size_t	*maxusagep)
{
	spl_t	s;

	s = splsched();
	stack_lock();

#if	STACK_USAGE
	if (stack_check_usage) {
		vm_offset_t stack;

		/*
		 *	This is pretty expensive to do at splsched,
		 *	but it only happens when someone makes
		 *	a debugging call, so it should be OK.
		 */

		for (stack = stack_free_list; stack != 0;
		     stack = stack_next(stack)) {
			vm_size_t usage = stack_usage(stack);

			if (usage > *maxusagep)
				*maxusagep = usage;
		}
	}
#endif	/* STACK_USAGE */

	*totalp = stack_free_count;
	stack_unlock();
	splx(s);
}
#endif	/* MACH_DEBUG */

#endif	/* MACHINE_STACK */


stack_fake_zone_info(int *count, vm_size_t *cur_size, vm_size_t *max_size, vm_size_t *elem_size,
		     vm_size_t *alloc_size, int *collectable, int *exhaustable)
{
        *count      = stack_alloc_total - stack_free_count;
	*cur_size   = KERNEL_STACK_SIZE * stack_alloc_total;
	*max_size   = KERNEL_STACK_SIZE * stack_alloc_hiwater;
	*elem_size  = KERNEL_STACK_SIZE;
	*alloc_size = KERNEL_STACK_SIZE;
	*collectable = 1;
	*exhaustable = 0;
}


/*
 *	stack_privilege:
 *
 *	stack_alloc_try on this thread must always succeed.
 */

void
stack_privilege(
	register thread_t thread)
{
	/*
	 *	This implementation only works for the current thread.
	 */

	if (thread != current_thread())
		panic("stack_privilege");

	if (thread->stack_privilege == 0)
		thread->stack_privilege = current_stack();
}

/*
 *	stack_alloc_try:
 *
 *	Non-blocking attempt to allocate a kernel stack.
 *	Called at splsched with the thread locked.
 */

boolean_t stack_alloc_try(
	thread_t	thread,
	void		(*start_pos)(thread_t))
{
	register vm_offset_t stack;

	if ((stack = thread->stack_privilege) == (vm_offset_t)0) {
	  stack_lock();
	  stack = stack_free_list;
	  if (stack != (vm_offset_t)0) {
	    stack_free_list = stack_next(stack);
	    stack_free_count--;
	  }
	  stack_unlock();
	}

	if (stack != 0) {
		stack_attach(thread, stack, start_pos);
		stack_alloc_hits++;
		return TRUE;
	} else {
		stack_alloc_misses++;
		return FALSE;
	}
}

natural_t			min_quantum_abstime;
extern natural_t	min_quantum_ms;

void
thread_init(void)
{
	thread_shuttle_zone = zinit(
			sizeof(struct thread_shuttle),
			THREAD_MAX * sizeof(struct thread_shuttle),
			THREAD_CHUNK * sizeof(struct thread_shuttle),
			"threads");

	/*
	 *	Fill in a template thread_shuttle for fast initialization.
	 *	[Fields that must be (or are typically) reset at
	 *	time of creation are so noted.]
	 */

	/* thr_sh_template.links (none) */
	thr_sh_template.runq = RUN_QUEUE_NULL;


	/* thr_sh_template.task (later) */
	/* thr_sh_template.thread_list (later) */
	/* thr_sh_template.pset_threads (later) */

	/* one ref for pset, one for activation */
	thr_sh_template.ref_count = 2;

	thr_sh_template.wait_event = NO_EVENT;
	thr_sh_template.wait_result = KERN_SUCCESS;
	thr_sh_template.wait_queue = WAIT_QUEUE_NULL;
	thr_sh_template.wake_active = FALSE;
	thr_sh_template.state = TH_WAIT|TH_UNINT;
	thr_sh_template.interruptible = TRUE;
	thr_sh_template.continuation = (void (*)(void))0;
	thr_sh_template.top_act = THR_ACT_NULL;

	thr_sh_template.importance = 0;
	thr_sh_template.sched_mode = 0;

	thr_sh_template.priority = 0;
	thr_sh_template.sched_pri = 0;
	thr_sh_template.depress_priority = -1;
	thr_sh_template.max_priority = 0;

	thr_sh_template.cpu_usage = 0;
	thr_sh_template.sched_usage = 0;
	thr_sh_template.sched_stamp = 0;
	thr_sh_template.sleep_stamp = 0;

	thr_sh_template.policy = POLICY_NULL;
	thr_sh_template.sp_state = 0;
	thr_sh_template.unconsumed_quantum = 0;

	thr_sh_template.vm_privilege = FALSE;

	timer_init(&(thr_sh_template.user_timer));
	timer_init(&(thr_sh_template.system_timer));
	thr_sh_template.user_timer_save.low = 0;
	thr_sh_template.user_timer_save.high = 0;
	thr_sh_template.system_timer_save.low = 0;
	thr_sh_template.system_timer_save.high = 0;
	thr_sh_template.cpu_delta = 0;
	thr_sh_template.sched_delta = 0;

	thr_sh_template.active = FALSE; /* reset */

	/* thr_sh_template.processor_set (later) */
#if	NCPUS > 1
	thr_sh_template.bound_processor = PROCESSOR_NULL;
#endif	/*NCPUS > 1*/
#if	MACH_HOST
	thr_sh_template.may_assign = TRUE;
	thr_sh_template.assign_active = FALSE;
#endif	/* MACH_HOST */
	thr_sh_template.funnel_state = 0;

#if	NCPUS > 1
	/* thr_sh_template.last_processor  (later) */
#endif	/* NCPUS > 1 */

	/*
	 *	Initialize other data structures used in
	 *	this module.
	 */

	queue_init(&reaper_queue);
	simple_lock_init(&reaper_lock, ETAP_THREAD_REAPER);
    thr_sh_template.funnel_lock = THR_FUNNEL_NULL;

#ifndef MACHINE_STACK
	simple_lock_init(&stack_lock_data, ETAP_THREAD_STACK);
#endif  /* MACHINE_STACK */

#if	MACH_DEBUG
	simple_lock_init(&stack_usage_lock, ETAP_THREAD_STACK_USAGE);
#endif	/* MACH_DEBUG */

#if	MACH_LDEBUG
	thr_sh_template.kthread = FALSE;
	thr_sh_template.mutex_count = 0;
#endif	/* MACH_LDEBUG */

	{
		AbsoluteTime		abstime;

		clock_interval_to_absolutetime_interval(
							min_quantum_ms, 1000*NSEC_PER_USEC, &abstime);
		assert(abstime.hi == 0 && abstime.lo != 0);
		min_quantum_abstime = abstime.lo;
	}

	/*
	 *	Initialize any machine-dependent
	 *	per-thread structures necessary.
	 */
	thread_machine_init();
}

void
thread_reaper_enqueue(
	thread_t		thread)
{
	/*
	 * thread lock is already held, splsched()
	 * not necessary here.
	 */
	simple_lock(&reaper_lock);

	enqueue_tail(&reaper_queue, (queue_entry_t)thread);
#if 0 /* CHECKME! */
	/*
	 * Since thread has been put in the reaper_queue, it must no longer
	 * be preempted (otherwise, it could be put back in a run queue).
	 */
	thread->preempt = TH_NOT_PREEMPTABLE;
#endif

	simple_unlock(&reaper_lock);

	thread_call_enter(thread_reaper_call);
}


/*
 *	Routine: thread_terminate_self
 *
 *		This routine is called by a thread which has unwound from
 *		its current RPC and kernel contexts and found that it's
 *		root activation has been marked for extinction.  This lets
 *		it clean up the last few things that can only be cleaned
 *		up in this context and then impale itself on the reaper
 *		queue.
 *
 *		When the reaper gets the thread, it will deallocate the
 *		thread_act's reference on itself, which in turn will release
 *		its own reference on this thread. By doing things in that
 *		order, a thread_act will always have a valid thread - but the
 *		thread may persist beyond having a thread_act (but must never
 *		run like that).
 */
void
thread_terminate_self(void)
{
	register thread_t	thread = current_thread();
	thread_act_t		thr_act = thread->top_act;
	task_t			task = thr_act->task;
	int			active_acts;
	spl_t			s;

	/*
	 * We should be at the base of the inheritance chain.
	 */
	assert(thr_act->thread == thread);

	/*
	 * Check to see if this is the last active activation.  By
	 * this we mean the last activation to call thread_terminate_self.
	 * If so, and the task is associated with a BSD process, we
	 * need to call BSD and let them clean up.
	 */
	task_lock(task);
	active_acts = --task->active_act_count;
	task_unlock(task);
	if (!active_acts && task->bsd_info)
		proc_exit(task->bsd_info);

#ifdef CALLOUT_RPC_MODEL
	if (thr_act->lower) {
		/*
		 * JMM - RPC will not be using a callout/stack manipulation
		 * mechanism.  instead we will let it return normally as if
		 * from a continuation.  Accordingly, these need to be cleaned
		 * up a bit.
		 */
		act_switch_swapcheck(thread, (ipc_port_t)0);
		act_lock(thr_act);	/* hierarchy violation XXX */
		(void) switch_act(THR_ACT_NULL);
		assert(thr_act->ref_count == 1);	/* XXX */
		/* act_deallocate(thr_act);		   XXX */
		prev_act = thread->top_act;
		/* 
		 * disable preemption to protect kernel stack changes
		 * disable_preemption();
		 * MACH_RPC_RET(prev_act) = KERN_RPC_SERVER_TERMINATED;
		 * machine_kernel_stack_init(thread, mach_rpc_return_error);
		 */
		act_unlock(thr_act);

		/*
		 * Load_context(thread);
		 */
		/* NOTREACHED */
	}

#else /* !CALLOUT_RPC_MODEL */

	assert(!thr_act->lower);

#endif /* CALLOUT_RPC_MODEL */

	s = splsched();
	thread_lock(thread);
	thread->active = FALSE;
	thread_unlock(thread);
	splx(s);

	thread_timer_terminate();

	/* flush any lazy HW state while in own context */
	thread_machine_flush(thr_act);

	ipc_thread_terminate(thread);

	s = splsched();
	thread_lock(thread);
	thread->state |= (TH_HALTED|TH_TERMINATE);
	assert((thread->state & TH_UNINT) == 0);
#if 0 /* CHECKME! */
	/*
	 * Since thread has been put in the reaper_queue, it must no longer
	 * be preempted (otherwise, it could be put back in a run queue).
	 */
	thread->preempt = TH_NOT_PREEMPTABLE;
#endif
	thread_mark_wait_locked(thread, THREAD_UNINT);
	thread_unlock(thread);
	/* splx(s); */

	ETAP_SET_REASON(thread, BLOCKED_ON_TERMINATION);
	thread_block((void (*)(void)) 0);
	panic("the zombie walks!");
	/*NOTREACHED*/
}


/*
 * Create a new thread.
 * Doesn't start the thread running; It first must be attached to
 * an activation - then use thread_go to start it.
 */
kern_return_t
thread_create_shuttle(
	thread_act_t			thr_act,
	integer_t				priority,
	void					(*start)(void),
	thread_t				*new_thread)
{
	thread_t				new_shuttle;
	task_t					parent_task = thr_act->task;
	processor_set_t			pset;
	kern_return_t			result;
	sched_policy_t			*policy;
	sf_return_t				sfr;
	int						suspcnt;

	assert(!thr_act->thread);
	assert(!thr_act->pool_port);

	/*
	 *	Allocate a thread and initialize static fields
	 */
	new_shuttle = (thread_t)zalloc(thread_shuttle_zone);
	if (new_shuttle == THREAD_NULL)
		return (KERN_RESOURCE_SHORTAGE);

	*new_shuttle = thr_sh_template;

	thread_lock_init(new_shuttle);
	rpc_lock_init(new_shuttle);
	wake_lock_init(new_shuttle);
	new_shuttle->sleep_stamp = sched_tick;

	pset = parent_task->processor_set;
	if (!pset->active) {
		pset = &default_pset;
	}
	pset_lock(pset);

	task_lock(parent_task);

	/*
	 *	Don't need to initialize because the context switch
	 *	code will set it before it can be used.
	 */
	if (!parent_task->active) {
		task_unlock(parent_task);
		pset_unlock(pset);
		zfree(thread_shuttle_zone, (vm_offset_t) new_shuttle);
		return (KERN_FAILURE);
	}

	act_attach(thr_act, new_shuttle, 0);

	/* Chain the thr_act onto the task's list */
	queue_enter(&parent_task->thr_acts, thr_act, thread_act_t, thr_acts);
	parent_task->thr_act_count++;
	parent_task->res_act_count++;
	parent_task->active_act_count++;

	/* Associate the thread with that scheduling policy */
	new_shuttle->policy = parent_task->policy;
	policy = &sched_policy[new_shuttle->policy];
	sfr = policy->sp_ops.sp_thread_attach(policy, new_shuttle);
	if (sfr != SF_SUCCESS)
		panic("thread_create_shuttle: sp_thread_attach");

	/* Associate the thread with the processor set */
	sfr = policy->sp_ops.sp_thread_processor_set(policy, new_shuttle, pset);
	if (sfr != SF_SUCCESS)
		panic("thread_create_shuttle: sp_thread_proceessor_set");

	/* Set the thread's scheduling parameters */
	new_shuttle->max_priority = parent_task->max_priority;
	new_shuttle->priority = (priority < 0)? parent_task->priority: priority;
	if (new_shuttle->priority > new_shuttle->max_priority)
		new_shuttle->priority = new_shuttle->max_priority;
	sfr = policy->sp_ops.sp_thread_setup(policy, new_shuttle);
	if (sfr != SF_SUCCESS)
		panic("thread_create_shuttle: sp_thread_setup");

#if	ETAP_EVENT_MONITOR
	new_thread->etap_reason = 0;
	new_thread->etap_trace  = FALSE;
#endif	/* ETAP_EVENT_MONITOR */

	new_shuttle->active = TRUE;
	thr_act->active = TRUE;
	pset_unlock(pset);


	/*
	 * No need to lock thr_act, since it can't be known to anyone --
	 * we set its suspend_count to one more than the task suspend_count
	 * by calling thread_hold.
	 */
	thr_act->user_stop_count = 1;
	for (suspcnt = thr_act->task->suspend_count + 1; suspcnt; --suspcnt)
		thread_hold(thr_act);
	task_unlock(parent_task);

	/*
	 *	Thread still isn't runnable yet (our caller will do
	 *	that).  Initialize runtime-dependent fields here.
	 */
	result = thread_machine_create(new_shuttle, thr_act, thread_continue);
	assert (result == KERN_SUCCESS);

	machine_kernel_stack_init(new_shuttle, thread_continue);
	ipc_thread_init(new_shuttle);
	thread_start(new_shuttle, start);
	thread_timer_setup(new_shuttle);

	*new_thread = new_shuttle;

	{
	  long dbg_arg1, dbg_arg2, dbg_arg3, dbg_arg4;

	  KERNEL_DEBUG_CONSTANT((TRACEDBG_CODE(DBG_TRACE_DATA, 1)) | DBG_FUNC_NONE,
				(vm_address_t)new_shuttle, 0,0,0,0);

	  kdbg_trace_string(parent_task->bsd_info, &dbg_arg1, &dbg_arg2, &dbg_arg3, 
			    &dbg_arg4);
          KERNEL_DEBUG_CONSTANT((TRACEDBG_CODE(DBG_TRACE_STRING, 1)) | DBG_FUNC_NONE,
				dbg_arg1, dbg_arg2, dbg_arg3, dbg_arg4, 0);
	}

	return (KERN_SUCCESS);
}

kern_return_t
thread_create(
	task_t				task,
	thread_act_t		*new_act)
{
	thread_act_t		thr_act;
	thread_t			thread;
	kern_return_t		result;
	sched_policy_t		*policy;
	sf_return_t			sfr;
	spl_t				s;
	extern void			thread_bootstrap_return(void);

	if (task == TASK_NULL)
		return KERN_INVALID_ARGUMENT;

	result = act_create(task, &thr_act);
	if (result != KERN_SUCCESS)
		return (result);

	result = thread_create_shuttle(thr_act, -1, thread_bootstrap_return, &thread);
	if (result != KERN_SUCCESS) {
		act_deallocate(thr_act);
		return (result);
	}

	if (task->kernel_loaded)
		thread_user_to_kernel(thread);

	/* Start the thread running (it will immediately suspend itself).  */
	s = splsched();
	thread_ast_set(thr_act, AST_APC);
	thread_lock(thread);
	thread_go_locked(thread, THREAD_AWAKENED);
	thread_unlock(thread);
	splx(s);
	
	*new_act = thr_act;

	return (KERN_SUCCESS);
}

/*
 * Update thread that belongs to a task created via kernel_task_create().
 */
void
thread_user_to_kernel(
	thread_t		thread)
{
	/*
	 * Used to set special swap_func here...
	 */
}

kern_return_t
thread_create_running(
	register task_t         parent_task,
	int                     flavor,
	thread_state_t          new_state,
	mach_msg_type_number_t  new_state_count,
	thread_act_t			*child_act)		/* OUT */
{
	register kern_return_t  result;

	result = thread_create(parent_task, child_act);
	if (result != KERN_SUCCESS)
		return (result);

	result = act_machine_set_state(*child_act, flavor,
				       new_state, new_state_count);
	if (result != KERN_SUCCESS) {
		(void) thread_terminate(*child_act);
		return (result);
	}

	result = thread_resume(*child_act);
	if (result != KERN_SUCCESS) {
		(void) thread_terminate(*child_act);
		return (result);
	}

	return (result);
}

/*
 *	kernel_thread:
 *
 *	Create and kernel thread in the specified task, and
 *	optionally start it running.
 */
thread_t
kernel_thread_with_priority(
	task_t				task,
	integer_t			priority,
	void				(*start)(void),
	boolean_t			start_running)
{
	kern_return_t		result;
	thread_t			thread;
	thread_act_t		thr_act;
	sched_policy_t		*policy;
	sf_return_t			sfr;
	spl_t				s;

	result = act_create(task, &thr_act);
	if (result != KERN_SUCCESS) {
		return THREAD_NULL;
	}

	result = thread_create_shuttle(thr_act, priority, start, &thread);
	if (result != KERN_SUCCESS) {
		act_deallocate(thr_act);
		return THREAD_NULL;
	}

	thread_swappable(thr_act, FALSE);

	s = splsched();
	thread_lock(thread);

	thr_act = thread->top_act;
#if	MACH_LDEBUG
	thread->kthread = TRUE;
#endif	/* MACH_LDEBUG */

	if (start_running)
		thread_go_locked(thread, THREAD_AWAKENED);

	thread_unlock(thread);
	splx(s);

	if (start_running)
		thread_resume(thr_act);

	act_deallocate(thr_act);
	return (thread);
}

thread_t
kernel_thread(
	task_t			task,
	void			(*start)(void))
{
	return kernel_thread_with_priority(task, -1, start, TRUE);
}

unsigned int c_weird_pset_ref_exit = 0;	/* pset code raced us */

void
thread_deallocate(
	thread_t			thread)
{
	task_t				task;
	processor_set_t		pset;
	sched_policy_t		*policy;
	sf_return_t			sfr;
	spl_t				s;

	if (thread == THREAD_NULL)
		return;

	/*
	 *	First, check for new count > 1 (the common case).
	 *	Only the thread needs to be locked.
	 */
	s = splsched();
	thread_lock(thread);
	if (--thread->ref_count > 1) {
		thread_unlock(thread);
		splx(s);
		return;
	}

	/*
	 *	Down to pset reference, lets try to clean up.
	 *	However, the processor set may make more. Its lock
	 *	also dominate the thread lock.  So, reverse the
	 *	order of the locks and see if its still the last
	 *	reference;
	 */
	assert(thread->ref_count == 1); /* Else this is an extra dealloc! */
	thread_unlock(thread);
	splx(s);

#if	MACH_HOST
	thread_freeze(thread);
#endif	/* MACH_HOST */

	pset = thread->processor_set;
	pset_lock(pset);

	s = splsched();
	thread_lock(thread);

	if (thread->ref_count > 1) {
#if	MACH_HOST
		boolean_t need_wakeup = FALSE;
		/*
		 *	processor_set made extra reference.
		 */
		/* Inline the unfreeze */
		thread->may_assign = TRUE;
		if (thread->assign_active) {
			need_wakeup = TRUE;
			thread->assign_active = FALSE;
		}
#endif	/* MACH_HOST */
		thread_unlock(thread);
		splx(s);
		pset_unlock(pset);
#if	MACH_HOST
		if (need_wakeup)
			thread_wakeup((event_t)&thread->assign_active);
#endif	/* MACH_HOST */
		c_weird_pset_ref_exit++;
		return;
	}
#if	MACH_HOST
	assert(thread->assign_active == FALSE);
#endif	/* MACH_HOST */

	/*
	 *	Thread only had pset reference - we can remove it.
	 */
	if (thread == current_thread())
	    panic("thread deallocating itself");

	/* Detach thread (shuttle) from its sched policy */
	policy = &sched_policy[thread->policy];
	sfr = policy->sp_ops.sp_thread_detach(policy, thread);
	if (sfr != SF_SUCCESS)
		panic("thread_deallocate: sp_thread_detach");

	pset_remove_thread(pset, thread);
	thread->ref_count = 0;
	thread_unlock(thread);		/* no more references - safe */
	splx(s);
	pset_unlock(pset);

	pset_deallocate(thread->processor_set);

	/* frees kernel stack & other MD resources */
	if (thread->stack_privilege && (thread->stack_privilege != thread->kernel_stack)) {
	  vm_offset_t stack;
	  int s = splsched();
	  stack = thread->stack_privilege;
	  stack_free(thread);
	  thread->kernel_stack = stack;
	  splx(s);
	}
	thread->stack_privilege = 0;
	thread_machine_destroy(thread);

	zfree(thread_shuttle_zone, (vm_offset_t) thread);
}

void
thread_reference(
	thread_t	thread)
{
	spl_t		s;

	if (thread == THREAD_NULL)
		return;

	s = splsched();
	thread_lock(thread);
	thread->ref_count++;
	thread_unlock(thread);
	splx(s);
}

/*
 * Called with "appropriate" thread-related locks held on
 * thread and its top_act for synchrony with RPC (see
 * act_lock_thread()).
 */
kern_return_t
thread_info_shuttle(
	register thread_act_t	thr_act,
	thread_flavor_t			flavor,
	thread_info_t			thread_info_out,	/* ptr to OUT array */
	mach_msg_type_number_t	*thread_info_count)	/*IN/OUT*/
{
	register thread_t		thread = thr_act->thread;
	int						state, flags;
	spl_t					s;

	if (thread == THREAD_NULL)
		return (KERN_INVALID_ARGUMENT);

	if (flavor == THREAD_BASIC_INFO) {
	    register thread_basic_info_t	basic_info;

	    if (*thread_info_count < THREAD_BASIC_INFO_COUNT)
			return (KERN_INVALID_ARGUMENT);

	    basic_info = (thread_basic_info_t) thread_info_out;

	    s = splsched();
	    thread_lock(thread);

	    /* fill in info */

	    thread_read_times(thread, &basic_info->user_time,
									&basic_info->system_time);

	    if (thread->policy & (POLICY_TIMESHARE|POLICY_RR|POLICY_FIFO)) {
			/*
			 *	Update lazy-evaluated scheduler info because someone wants it.
			 */
			if (thread->sched_stamp != sched_tick)
				update_priority(thread);

			basic_info->sleep_time = 0;

			/*
			 *	To calculate cpu_usage, first correct for timer rate,
			 *	then for 5/8 ageing.  The correction factor [3/5] is
			 *	(1/(5/8) - 1).
			 */
			basic_info->cpu_usage = (thread->cpu_usage << SCHED_TICK_SHIFT) /
											(TIMER_RATE / TH_USAGE_SCALE);
			basic_info->cpu_usage = (basic_info->cpu_usage * 3) / 5;
#if	SIMPLE_CLOCK
			/*
			 *	Clock drift compensation.
			 */
			basic_info->cpu_usage =
					(basic_info->cpu_usage * 1000000) / sched_usec;
#endif	/* SIMPLE_CLOCK */
	    }
		else
			basic_info->sleep_time = basic_info->cpu_usage = 0;

	    basic_info->policy	= thread->policy;

	    flags = 0;
	    if (thread->state & TH_SWAPPED_OUT)
			flags = TH_FLAGS_SWAPPED;
	    else
		if (thread->state & TH_IDLE)
			flags = TH_FLAGS_IDLE;

	    state = 0;
	    if (thread->state & TH_HALTED)
			state = TH_STATE_HALTED;
	    else
		if (thread->state & TH_RUN)
			state = TH_STATE_RUNNING;
	    else
		if (thread->state & TH_UNINT)
			state = TH_STATE_UNINTERRUPTIBLE;
	    else
		if (thread->state & TH_SUSP)
			state = TH_STATE_STOPPED;
	    else
		if (thread->state & TH_WAIT)
			state = TH_STATE_WAITING;

	    basic_info->run_state = state;
	    basic_info->flags = flags;

	    basic_info->suspend_count = thr_act->user_stop_count;

	    thread_unlock(thread);
	    splx(s);

	    *thread_info_count = THREAD_BASIC_INFO_COUNT;

	    return (KERN_SUCCESS);
	}
	else
	if (flavor == THREAD_SCHED_TIMESHARE_INFO) {
		policy_timeshare_info_t		ts_info;

		if (*thread_info_count < POLICY_TIMESHARE_INFO_COUNT)
			return (KERN_INVALID_ARGUMENT);

		ts_info = (policy_timeshare_info_t)thread_info_out;

	    s = splsched();
		thread_lock(thread);

	    if (thread->policy != POLICY_TIMESHARE) {
	    	thread_unlock(thread);
			splx(s);

			return (KERN_INVALID_POLICY);
	    }

		ts_info->base_priority = thread->priority;
		ts_info->max_priority =	thread->max_priority;
		ts_info->cur_priority = thread->sched_pri;

		ts_info->depressed = (thread->depress_priority >= 0);
		ts_info->depress_priority = thread->depress_priority;

		thread_unlock(thread);
	    splx(s);

		*thread_info_count = POLICY_TIMESHARE_INFO_COUNT;

		return (KERN_SUCCESS);	
	}
	else
	if (flavor == THREAD_SCHED_FIFO_INFO) {
		policy_fifo_info_t			fifo_info;

		if (*thread_info_count < POLICY_FIFO_INFO_COUNT)
			return (KERN_INVALID_ARGUMENT);

		fifo_info = (policy_fifo_info_t)thread_info_out;

	    s = splsched();
		thread_lock(thread);

	    if (thread->policy != POLICY_FIFO) {
	    	thread_unlock(thread);
			splx(s);

			return (KERN_INVALID_POLICY);
	    }

		fifo_info->base_priority = thread->priority;
		fifo_info->max_priority = thread->max_priority;

		fifo_info->depressed = (thread->depress_priority >= 0);
		fifo_info->depress_priority = thread->depress_priority;

		thread_unlock(thread);
	    splx(s);

		*thread_info_count = POLICY_FIFO_INFO_COUNT;

		return (KERN_SUCCESS);	
	}
	else
	if (flavor == THREAD_SCHED_RR_INFO) {
		policy_rr_info_t			rr_info;

		if (*thread_info_count < POLICY_RR_INFO_COUNT)
			return (KERN_INVALID_ARGUMENT);

		rr_info = (policy_rr_info_t) thread_info_out;

	    s = splsched();
		thread_lock(thread);

	    if (thread->policy != POLICY_RR) {
	    	thread_unlock(thread);
			splx(s);

			return (KERN_INVALID_POLICY);
	    }

		rr_info->base_priority = thread->priority;
		rr_info->max_priority = thread->max_priority;
	    rr_info->quantum = min_quantum_ms;

		rr_info->depressed = (thread->depress_priority >= 0);
		rr_info->depress_priority = thread->depress_priority;

		thread_unlock(thread);
	    splx(s);

		*thread_info_count = POLICY_RR_INFO_COUNT;

		return (KERN_SUCCESS);	
	}

	return (KERN_INVALID_ARGUMENT);
}

void
thread_doreap(
	register thread_t	thread)
{
	thread_act_t		thr_act;
	struct ipc_port		*pool_port;


	thr_act = thread_lock_act(thread);
	assert(thr_act && thr_act->thread == thread);

	act_locked_act_reference(thr_act);
	pool_port = thr_act->pool_port;

	/*
	 * Replace `act_unlock_thread()' with individual
	 * calls.  (`act_detach()' can change fields used
	 * to determine which locks are held, confusing
	 * `act_unlock_thread()'.)
	 */
	rpc_unlock(thread);
	if (pool_port != IP_NULL)
		ip_unlock(pool_port);
	act_unlock(thr_act);

	/* Remove the reference held by a rooted thread */
	if (pool_port == IP_NULL)
		act_deallocate(thr_act);

	/* Remove the reference held by the thread: */
	act_deallocate(thr_act);
}

static thread_call_data_t	thread_reaper_call_data;

/*
 *	reaper_thread:
 *
 *	This kernel thread runs forever looking for threads to destroy
 *	(when they request that they be destroyed, of course).
 *
 *	The reaper thread will disappear in the next revision of thread
 *	control when it's function will be moved into thread_dispatch.
 */
static void
_thread_reaper(
	thread_call_param_t		p0,
	thread_call_param_t		p1)
{
	register thread_t	thread;
	spl_t				s;

	s = splsched();
	simple_lock(&reaper_lock);

	while ((thread = (thread_t) dequeue_head(&reaper_queue)) != THREAD_NULL) {
		simple_unlock(&reaper_lock);

		/*
		 * wait for run bit to clear
		 */
		thread_lock(thread);
		if (thread->state & TH_RUN)
			panic("thread reaper: TH_RUN");
		thread_unlock(thread);
		splx(s);

		thread_doreap(thread);

		s = splsched();
		simple_lock(&reaper_lock);
	}

	simple_unlock(&reaper_lock);
	splx(s);
}

void
thread_reaper(void)
{
	thread_call_setup(&thread_reaper_call_data,	_thread_reaper, NULL);
	thread_reaper_call = &thread_reaper_call_data;

	_thread_reaper(NULL, NULL);
}

kern_return_t
thread_assign(
	thread_act_t	thr_act,
	processor_set_t	new_pset)
{
#ifdef	lint
	thread++; new_pset++;
#endif	/* lint */
	return(KERN_FAILURE);
}

/*
 *	thread_assign_default:
 *
 *	Special version of thread_assign for assigning threads to default
 *	processor set.
 */
kern_return_t
thread_assign_default(
	thread_act_t	thr_act)
{
	return (thread_assign(thr_act, &default_pset));
}

/*
 *	thread_get_assignment
 *
 *	Return current assignment for this thread.
 */	    
kern_return_t
thread_get_assignment(
	thread_act_t	thr_act,
	processor_set_t	*pset)
{
	thread_t	thread;

	if (thr_act == THR_ACT_NULL)
		return(KERN_INVALID_ARGUMENT);
	thread = act_lock_thread(thr_act);
	if (thread == THREAD_NULL) {
		act_unlock_thread(thr_act);
		return(KERN_INVALID_ARGUMENT);
	}
	*pset = thread->processor_set;
	act_unlock_thread(thr_act);
	pset_reference(*pset);
	return(KERN_SUCCESS);
}

/*
 *	thread_wire:
 *
 *	Specify that the target thread must always be able
 *	to run and to allocate memory.
 */
kern_return_t
thread_wire(
	host_priv_t	host_priv,
	thread_act_t	thr_act,
	boolean_t	wired)
{
	spl_t		s;
	thread_t	thread;
	extern void vm_page_free_reserve(int pages);

	if (thr_act == THR_ACT_NULL || host_priv == HOST_PRIV_NULL)
		return (KERN_INVALID_ARGUMENT);

	assert(host_priv == &realhost);

	thread = act_lock_thread(thr_act);
	if (thread ==THREAD_NULL) {
		act_unlock_thread(thr_act);
		return(KERN_INVALID_ARGUMENT);
	}

	/*
	 * This implementation only works for the current thread.
	 * See stack_privilege.
	 */
	if (thr_act != current_act())
	    return KERN_INVALID_ARGUMENT;

	s = splsched();
	thread_lock(thread);

	if (wired) {
	    if (thread->vm_privilege == FALSE) 
		    vm_page_free_reserve(1);	/* XXX */
	    thread->vm_privilege = TRUE;
	} else {
	    if (thread->vm_privilege == TRUE) 
		    vm_page_free_reserve(-1);	/* XXX */
	    thread->vm_privilege = FALSE;
	}

	thread_unlock(thread);
	splx(s);
	act_unlock_thread(thr_act);

	/*
	 * Make the thread unswappable.
	 */
	if (wired)
		thread_swappable(thr_act, FALSE);

	return KERN_SUCCESS;
}

/*
 *	thread_collect_scan:
 *
 *	Attempt to free resources owned by threads.
 */

void
thread_collect_scan(void)
{
	/* This code runs very quickly! */
}

boolean_t thread_collect_allowed = TRUE;
unsigned thread_collect_last_tick = 0;
unsigned thread_collect_max_rate = 0;		/* in ticks */

/*
 *	consider_thread_collect:
 *
 *	Called by the pageout daemon when the system needs more free pages.
 */

void
consider_thread_collect(void)
{
	/*
	 *	By default, don't attempt thread collection more frequently
	 *	than once a second (one scheduler tick).
	 */

	if (thread_collect_max_rate == 0)
		thread_collect_max_rate = 2;		/* sched_tick is a 1 second resolution 2 here insures at least 1 second interval */

	if (thread_collect_allowed &&
	    (sched_tick >
	     (thread_collect_last_tick + thread_collect_max_rate))) {
		thread_collect_last_tick = sched_tick;
		thread_collect_scan();
	}
}

#if	MACH_DEBUG
#if	STACK_USAGE

vm_size_t
stack_usage(
	register vm_offset_t stack)
{
	int i;

	for (i = 0; i < KERNEL_STACK_SIZE/sizeof(unsigned int); i++)
	    if (((unsigned int *)stack)[i] != STACK_MARKER)
		break;

	return KERNEL_STACK_SIZE - i * sizeof(unsigned int);
}

/*
 *	Machine-dependent code should call stack_init
 *	before doing its own initialization of the stack.
 */

static void
stack_init(
	   register vm_offset_t stack,
	   unsigned int bytes)
{
	if (stack_check_usage) {
	    int i;

	    for (i = 0; i < bytes / sizeof(unsigned int); i++)
		((unsigned int *)stack)[i] = STACK_MARKER;
	}
}

/*
 *	Machine-dependent code should call stack_finalize
 *	before releasing the stack memory.
 */

void
stack_finalize(
	register vm_offset_t stack)
{
	if (stack_check_usage) {
	    vm_size_t used = stack_usage(stack);

	    simple_lock(&stack_usage_lock);
	    if (used > stack_max_usage)
		stack_max_usage = used;
	    simple_unlock(&stack_usage_lock);
	    if (used > stack_max_use) {
		printf("stack usage = %x\n", used);
		panic("stack overflow");
	    }
	}
}

#endif	/*STACK_USAGE*/
#endif /* MACH_DEBUG */

kern_return_t
host_stack_usage(
	host_t		host,
	vm_size_t	*reservedp,
	unsigned int	*totalp,
	vm_size_t	*spacep,
	vm_size_t	*residentp,
	vm_size_t	*maxusagep,
	vm_offset_t	*maxstackp)
{
#if !MACH_DEBUG
        return KERN_NOT_SUPPORTED;
#else
	unsigned int total;
	vm_size_t maxusage;

	if (host == HOST_NULL)
		return KERN_INVALID_HOST;

	simple_lock(&stack_usage_lock);
	maxusage = stack_max_usage;
	simple_unlock(&stack_usage_lock);

	stack_statistics(&total, &maxusage);

	*reservedp = 0;
	*totalp = total;
	*spacep = *residentp = total * round_page(KERNEL_STACK_SIZE);
	*maxusagep = maxusage;
	*maxstackp = 0;
	return KERN_SUCCESS;

#endif /* MACH_DEBUG */
}

/*
 * Return info on stack usage for threads in a specific processor set
 */
kern_return_t
processor_set_stack_usage(
	processor_set_t	pset,
	unsigned int	*totalp,
	vm_size_t	*spacep,
	vm_size_t	*residentp,
	vm_size_t	*maxusagep,
	vm_offset_t	*maxstackp)
{
#if !MACH_DEBUG
        return KERN_NOT_SUPPORTED;
#else
	unsigned int total;
	vm_size_t maxusage;
	vm_offset_t maxstack;

	register thread_t *threads;
	register thread_t thread;

	unsigned int actual;	/* this many things */
	unsigned int i;

	vm_size_t size, size_needed;
	vm_offset_t addr;

	if (pset == PROCESSOR_SET_NULL)
		return KERN_INVALID_ARGUMENT;

	size = 0; addr = 0;

	for (;;) {
		pset_lock(pset);
		if (!pset->active) {
			pset_unlock(pset);
			return KERN_INVALID_ARGUMENT;
		}

		actual = pset->thread_count;

		/* do we have the memory we need? */

		size_needed = actual * sizeof(thread_t);
		if (size_needed <= size)
			break;

		/* unlock the pset and allocate more memory */
		pset_unlock(pset);

		if (size != 0)
			kfree(addr, size);

		assert(size_needed > 0);
		size = size_needed;

		addr = kalloc(size);
		if (addr == 0)
			return KERN_RESOURCE_SHORTAGE;
	}

	/* OK, have memory and the processor_set is locked & active */

	threads = (thread_t *) addr;
	for (i = 0, thread = (thread_t) queue_first(&pset->threads);
	     i < actual;
	     i++,
	     thread = (thread_t) queue_next(&thread->pset_threads)) {
		thread_reference(thread);
		threads[i] = thread;
	}
	assert(queue_end(&pset->threads, (queue_entry_t) thread));

	/* can unlock processor set now that we have the thread refs */
	pset_unlock(pset);

	/* calculate maxusage and free thread references */

	total = 0;
	maxusage = 0;
	maxstack = 0;
	for (i = 0; i < actual; i++) {
		int cpu;
		thread_t thread = threads[i];
		vm_offset_t stack = 0;

		/*
		 *	thread->kernel_stack is only accurate if the
		 *	thread isn't swapped and is not executing.
		 *
		 *	Of course, we don't have the appropriate locks
		 *	for these shenanigans.
		 */

		stack = thread->kernel_stack;

		for (cpu = 0; cpu < NCPUS; cpu++)
			if (cpu_data[cpu].active_thread == thread) {
				stack = active_stacks[cpu];
				break;
			}

		if (stack != 0) {
			total++;

			if (stack_check_usage) {
				vm_size_t usage = stack_usage(stack);

				if (usage > maxusage) {
					maxusage = usage;
					maxstack = (vm_offset_t) thread;
				}
			}
		}

		thread_deallocate(thread);
	}

	if (size != 0)
		kfree(addr, size);

	*totalp = total;
	*residentp = *spacep = total * round_page(KERNEL_STACK_SIZE);
	*maxusagep = maxusage;
	*maxstackp = maxstack;
	return KERN_SUCCESS;

#endif	/* MACH_DEBUG */
}

static int split_funnel_off = 0;
funnel_t *
funnel_alloc(
	int type)
{
	mutex_t *m;
	funnel_t * fnl;
	if ((fnl = (funnel_t *)kalloc(sizeof(funnel_t))) != 0){
		bzero(fnl, sizeof(funnel_t));
		if ((m = mutex_alloc(0)) == (mutex_t *)NULL) {
			kfree(fnl, sizeof(funnel_t));
			return(THR_FUNNEL_NULL);
		}
		fnl->fnl_mutex = m;
		fnl->fnl_type = type;
	}
	return(fnl);
}

void 
funnel_free(
	funnel_t * fnl)
{
	mutex_free(fnl->fnl_mutex);
	if (fnl->fnl_oldmutex)
		mutex_free(fnl->fnl_oldmutex);
	kfree(fnl, sizeof(funnel_t));
}

void 
funnel_lock(
	funnel_t * fnl)
{
	mutex_t * m;

	m = fnl->fnl_mutex;
restart:
	mutex_lock(m);
	fnl->fnl_mtxholder = current_thread();
	if (split_funnel_off && (m != fnl->fnl_mutex)) {
		mutex_unlock(m);
		m = fnl->fnl_mutex;	
		goto restart;
	}
}

void 
funnel_unlock(
	funnel_t * fnl)
{
	mutex_unlock(fnl->fnl_mutex);
	fnl->fnl_mtxrelease = current_thread();
}

funnel_t *
thread_funnel_get(
	void)
{
	thread_t th = current_thread();

	if (th->funnel_state & TH_FN_OWNED) {
		return(th->funnel_lock);
	}
	return(THR_FUNNEL_NULL);
}

boolean_t
thread_funnel_set(
        funnel_t *	fnl,
	boolean_t	funneled)
{
	thread_t	cur_thread;
	boolean_t	funnel_state_prev;
	boolean_t	intr;
        
	cur_thread = current_thread();
	funnel_state_prev = ((cur_thread->funnel_state & TH_FN_OWNED) == TH_FN_OWNED);

	if (funnel_state_prev != funneled) {
		intr = ml_set_interrupts_enabled(FALSE);

		if (funneled == TRUE) {
			if (cur_thread->funnel_lock)
				panic("Funnel lock called when holding one %x", cur_thread->funnel_lock);
			KERNEL_DEBUG(0x6032428 | DBG_FUNC_NONE,
											fnl, 1, 0, 0, 0);
			funnel_lock(fnl);
			KERNEL_DEBUG(0x6032434 | DBG_FUNC_NONE,
											fnl, 1, 0, 0, 0);
			cur_thread->funnel_state |= TH_FN_OWNED;
			cur_thread->funnel_lock = fnl;
		} else {
			if(cur_thread->funnel_lock->fnl_mutex != fnl->fnl_mutex)
				panic("Funnel unlock  when not holding funnel");
			cur_thread->funnel_state &= ~TH_FN_OWNED;
			KERNEL_DEBUG(0x603242c | DBG_FUNC_NONE,
								fnl, 1, 0, 0, 0);

			cur_thread->funnel_lock = THR_FUNNEL_NULL;
			funnel_unlock(fnl);
		}
		(void)ml_set_interrupts_enabled(intr);
	} else {
		/* if we are trying to acquire funnel recursively
		 * check for funnel to be held already
		 */
		if (funneled && (fnl->fnl_mutex != cur_thread->funnel_lock->fnl_mutex)) {
				panic("thread_funnel_set: already holding a different funnel");
		}
	}
	return(funnel_state_prev);
}

boolean_t
thread_funnel_merge(
	funnel_t * fnl,
	funnel_t * otherfnl)
{
	mutex_t * m;
	mutex_t * otherm;
	funnel_t * gfnl;
	extern int disable_funnel;

	if ((gfnl = thread_funnel_get()) == THR_FUNNEL_NULL)
		panic("thread_funnel_merge called with no funnels held");

	if (gfnl->fnl_type != 1)
		panic("thread_funnel_merge called from non kernel funnel");

	if (gfnl != fnl)
		panic("thread_funnel_merge incorrect invocation");

	if (disable_funnel || split_funnel_off)
		return (KERN_FAILURE);

	m = fnl->fnl_mutex;
	otherm = otherfnl->fnl_mutex;

	/* Acquire other funnel mutex */
	mutex_lock(otherm);
	split_funnel_off = 1;
	disable_funnel = 1;
	otherfnl->fnl_mutex = m;
	otherfnl->fnl_type = fnl->fnl_type;
	otherfnl->fnl_oldmutex = otherm;	/* save this for future use */

	mutex_unlock(otherm);
	return(KERN_SUCCESS);
}

void
thread_set_cont_arg(int arg)
{
  thread_t th = current_thread();
  th->cont_arg = arg; 
}

int
thread_get_cont_arg(void)
{
  thread_t th = current_thread();
  return(th->cont_arg); 
}

/*
 * Export routines to other components for things that are done as macros
 * within the osfmk component.
 */
#undef thread_should_halt
boolean_t
thread_should_halt(
	thread_shuttle_t th)
{
	return(thread_should_halt_fast(th));
}

