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

struct msr_batch_op
{
    __u16 cpu;     // In: CPU to execute {rd/wr}msr instruction
    __u16 isrdmsr; // In: 0=wrmsr, non-zero=rdmsr
    __s32 err;     // Out: set if error occurred with this operation
    __u32 msr;     // In: MSR Address to perform operation
    __u64 msrdata; // In/Out: Input/Result to/from operation
    __u64 wmask;   // Out: Write mask applied to wrmsr
};

enum msr_commands { // LIMIT TO __u16
    MSR_OP_WR   = 0x00, // Write the contents of msrdata to the msr
    MSR_OP_RD   = 0x01, // After the write command (if any), 
                        //   read the contents of the msr into msrdata
    MSR_OP_POLL = 0x02, // After the read and write commands, if any,
                        //   continuously read the value of the msr into
                        //   msrdata until it changes, then write the new value
                        //   into msrpolldata
    MSR_OP_AP_0 = 0x04, // Read MSR_APERF into aperf0 before anything else
    MSR_OP_AP_1 = 0x08, // Read MSR_APERF into aperf1 after everything else
    MSR_OP_MP_0 = 0x10, // Read MSR_MPERF into mperf0 before everything 
                        //   (except MSR_APERF)
    MSR_OP_MP_1 = 0x20, // Read MSR_MPERF into mperf1 after everything 
                        //   (except MSR_APERF)
    MAX_OP = 0x7FFF     // Might be able to relax this 

};



struct msr_batch_op_ex
{
    __u16 cpu;          // In: CPU to execute {rd/wr}msr instruction
    __u16 cmd;          // In: See msr_commands, can be or'ed together.
    __s32 err;          // Out: set if error occurred with this operation
    __u32 msr;          // In: MSR Address to perform operation
    __u64 msrdata;      // In/Out: Input/Result to/from operation
    __u64 msrpolldata;  // Out:  New msr value
    __u64 aperf0;       // Out:  Timestamp value prior to the op
    __u64 aperf1;       // Out:  Timestamp value after the op
    __u64 mperf0;       // Out:  Timestamp value prior to the op
    __u64 mperf1;       // Out:  Timestamp value after the op
    __u64 wmask;        // Out: Write mask applied to wrmsr
    __u16 err_idx;      // Out: To which operation does the error apply
    __u16 valid_idx;    // Out: Bitwise-and with cmd for a quick check to see
                        //   which of these fields contain useful info
                        //   For example, if cmd has MSR_OP_RD and MSR_OP_POLL
                        //   set, and MSR_OP_READ succeeds but MSR_OP_POLL
                        //   fails without invalidating the contents of 
                        //   msrdata, then the MSR_OP_READ bit of valid_idx
                        //   will be 1 and the MSR_OP_POLL bit will be 0.
};

struct msr_batch_array
{
    __u32 numops;               // In: # of operations in operations array
    struct msr_batch_op *ops;   // In: Array[numops] of operations
};

struct msr_batch_array_ex
{
    __u32 always_zero;          // In: Set to 0 to use new API
    struct msr_batch_op_ex *ops_ex;// In: Array[numops] of operations
    __u32 numops_ex;            // In: # of operations in operations array
    __u16 version_requested;    // In: (MSR_SAFE_VER_MAJ<<8) | MSR_SAFE_VER_MIN
};
#define X86_IOC_MSR_BATCH   _IOWR('c', 0xA2, struct msr_batch_array)

#endif
