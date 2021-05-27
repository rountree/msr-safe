// Copyright 2011-2020 Lawrence Livermore National Security, LLC and other
// msr-safe Project Developers. See the top-level COPYRIGHT file for
// details.
//
// SPDX-License-Identifier: GPL-2.0-only

/*
 * (proposed) extensions to arch/x86/lib/msr_smp.c
 *
 * This file is the implementation proposed to arch/x86/lib/msr_smp.c
 * that will allow for batching rdmsr/wrmsr requests.
 */

#include <asm/msr.h>
#include <linux/cpumask.h>
#include <linux/module.h>
#include <linux/preempt.h>
#include <linux/smp.h>

#include "msr_safe.h"

static void __msr_safe_batch(void *info)
{
    struct msr_batch_array *oa = info;
    struct msr_batch_op *op;
    int this_cpu = smp_processor_id();
    u32 *dp;
    u64 oldmsr;
    u64 newmsr;

    for (op = oa->ops; op < oa->ops + oa->numops; ++op)
    {
        if (op->cpu != this_cpu)
        {
            continue;
        }

        op->err = 0;

	// Grab aperf and mperf prior to anything else.
	if( op->msrcmd & 0x4 ){
		dp = (u32 *)&(op->aperf0);
		if (rdmsr_safe(0xE8, &dp[0], &dp[1])){
			op->err = -EIO;
			continue;
		}
		dp = (u32 *)&(op->mperf0);
		if (rdmsr_safe(0xE7, &dp[0], &dp[1])){
			op->err = -EIO;
			continue;
		}
	}else{
		op->aperf0 = op->mperf0 = 0;
	}

	// This read happens regardless of whether the command 
	// is a read or write.
	dp = (u32 *)&oldmsr;
	if (rdmsr_safe(op->msr, &dp[0], &dp[1])){
		op->err = -EIO;
		continue;
	}
	if( op->msrcmd & 0x1 ){
		op->msrdata = oldmsr;
	}

	// poll until changed
	if( op->msrcmd & 0x2 ){
		op->msrdata = oldmsr;
		dp = (u32 *)&(op->msrdata1);
		do{	
			if (rdmsr_safe(op->msr, &dp[0], &dp[1])){
				op->err = -EIO;
				break;
			}
		}while( op->msrdata == op->msrdata1 );
		if( op->err ){
			continue;
		}
	}
	
	// Handle the write case.
	if( ! (op->msrcmd & 0x1) ){
		newmsr = op->msrdata & op->wmask;
		newmsr |= (oldmsr & ~op->wmask);
		dp = (u32 *)&newmsr;
		if (wrmsr_safe(op->msr, dp[0], dp[1]))
		{
		    op->err = -EIO;
		}
	}

	// Grab aperf and mperf after everthing else.
	if( op->msrcmd & 0x8 ){
		dp = (u32 *)&(op->aperf1);
		if (rdmsr_safe(0xE8, &dp[0], &dp[1])){
			op->err = -EIO;
			continue;
		}
		dp = (u32 *)&(op->mperf1);
		if (rdmsr_safe(0xE7, &dp[0], &dp[1])){
			op->err = -EIO;
			continue;
		}
	}else{
		op->aperf1 = op->mperf1 = 0;
	}

/* Original code
        dp = (u32 *)&oldmsr;
        if (rdmsr_safe(op->msr, &dp[0], &dp[1]))
        {
            op->err = -EIO;
            continue;
        }
        if (op->isrdmsr)
        {
            op->msrdata = oldmsr;
            continue;
        }

        newmsr = op->msrdata & op->wmask;
        newmsr |= (oldmsr & ~op->wmask);
        dp = (u32 *)&newmsr;
        if (wrmsr_safe(op->msr, dp[0], dp[1]))
        {
            op->err = -EIO;
        }
*/
    }
}

#ifdef CONFIG_CPUMASK_OFFSTACK

int msr_safe_batch(struct msr_batch_array *oa)
{
    cpumask_var_t cpus_to_run_on;
    struct msr_batch_op *op;

    zalloc_cpumask_var(&cpus_to_run_on, GFP_KERNEL);

    for (op = oa->ops; op < oa->ops + oa->numops; ++op)
    {
        cpumask_set_cpu(op->cpu, cpus_to_run_on);
    }

    on_each_cpu_mask(cpus_to_run_on, __msr_safe_batch, oa, 1);

    free_cpumask_var(cpus_to_run_on);

    for (op = oa->ops; op < oa->ops + oa->numops; ++op)
    {
        if (op->err)
        {
            return op->err;
        }
    }

    return 0;
}

#else

int msr_safe_batch(struct msr_batch_array *oa)
{
    struct cpumask cpus_to_run_on;
    struct msr_batch_op *op;

    cpumask_clear(&cpus_to_run_on);
    for (op = oa->ops; op < oa->ops + oa->numops; ++op)
    {
        cpumask_set_cpu(op->cpu, &cpus_to_run_on);
    }

    on_each_cpu_mask(&cpus_to_run_on, __msr_safe_batch, oa, 1);

    for (op = oa->ops; op < oa->ops + oa->numops; ++op)
    {
        if (op->err)
        {
            return op->err;
        }
    }

    return 0;
}

#endif //CONFIG_CPUMASK_OFFSTACK
