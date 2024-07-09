// Copyright 2011-2021 Lawrence Livermore National Security, LLC and other
// msr-safe Project Developers. See the top-level COPYRIGHT file for
// details.
//
// SPDX-License-Identifier: GPL-2.0-only

/*
 * This file defines the ioctl interface for submitting a batch of MSR
 * requests.  This file should be distributed to a place where kernel
 * module interface header files are kept for a distribution running this
 * module so that user-space applications and libraries that wish to use
 * the batch interface may #include this file to get the interface.
 */

#ifndef MSR_SAFE_HEADER_INCLUDE
#define MSR_SAFE_HEADER_INCLUDE

#include <linux/ioctl.h>
#include <linux/types.h>

/*
 * (MSR_START_TSC)                  read IA32_TIME_STAMP_COUNTER into start_tsc
 * (MSR_START_APERF)                read IA32_APERF into start_aperf
 * (MSR_START_MPERF)                read IA32_MPERF into start_mperf
 * (mandatory)                      read msr into oldmsr
 * (MSR_POLL)                       start the loop
 *  (MSR_POLL & MSR_START_TSC)
 *  (MSR_POLL & MSR_START_APERF)
 *  (MSR_POLL & MSR_START_MPERF)
 *  (mandatory)                     read msr in into newmsr, continue loop if unchanged
 * (MSR_WRITE)
 * (MSR_STOP_TSC)
 * (MSR_STOP_APERF)
 * (MSR_STOP_MPERF)
 * (MSR_THERMSTATUS)
 * (MSR_PERFSTATUS)
 * (MSR_INS_RETIRED)
 */

struct msr_batch_op
{
    __u16 cpu;              // In: CPU to execute {rd/wr}msr instruction
    __u16 op;               // In: see below
    __s32 err;              // Out: set if error occurred with this operation
    __u32 msr;              // In: MSR Address to perform operation
    __u64 writeval;         // In/Out: Input/Result to/from operation, previous poll value
    __u64 readval;          // Out: Holds current poll value
    __u64 pollval;          // Out: Holds current poll value
    __u64 wmask;            // Out: Write mask applied to wrmsr
    __u64 mperf[3];
    __u64 therm;            // Out: Contents of IA32_THERM_STATUS
    __u64 perf;             // Out: Contents of IA32_PERF_STATUS
    __u64 ins;              // Out: Number of instructions retired
};


enum{
    MSR_WRITE           = 0x001,
    MSR_POLL            = 0x002,
    MPERF0              = 0x010,
    MPERF1              = 0x020,
    MPERF2              = 0x040,
    MPERF3              = 0x080,
    MSR_THERM_STATUS    = 0x100,
    MSR_PERF_STATUS     = 0x200,
    MSR_INS_RETIRED     = 0x400,
};

enum{
    _IA32_TIME_STAMP_COUNTER    = 0x010,
    _IA32_APERF                 = 0x0E8,
    _IA32_MPERF                 = 0x0E7,
    _IA32_THERM_STATUS          = 0x19C,
    _IA32_PERF_STATUS           = 0x198,
    _IA32_FIXED_CTR0            = 0x309,
};

struct msr_batch_array
{
    __u32 numops;             // In: # of operations in operations array
    struct msr_batch_op *ops; // In: Array[numops] of operations
};

#define X86_IOC_MSR_BATCH   _IOWR('c', 0xA2, struct msr_batch_array)

#endif
