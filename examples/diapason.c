// Compile with gcc -std=c2x -Wall -Wextra diapason.c

#include <stdio.h>      // fprintf(3)
#include <assert.h>     // assert(3)
#include <fcntl.h>      // open(2)
#include <unistd.h>     // write(2), pwrite(2), pread(2)
#include <string.h>     // strlen(3), memset(3)
#include <stdint.h>     // uint8_t
#include <inttypes.h>   // PRIu8
#include <stdlib.h>     // exit(3)
#include <sys/ioctl.h>  // ioctl(2)

#include "../msr_safe.h"   // batch data structs

enum{
    IA32_MPERF              = 0x0E7,
    IA32_APERF              = 0x0E8,
    IA32_TIME_STAMP_COUNTER = 0x010,
    IA32_THERM_STATUS       = 0x19C,
    IA32_PERF_STATUS        = 0x198,
    IA32_FIXED_CTR_CTRL     = 0x38D,
    IA32_PERF_GLOBAL_CTRL   = 0x38F,
    IA32_FIXED_CTR0         = 0x309,
    MSR_PKG_ENERGY_STATUS   = 0x611,
    MSR_PP0_ENERGY_STATUS   = 0x639,
};


char const *const allowlist = "0x0E7 0x0\n"  // MPERF
                              "0x010 0x0\n"  // TSC
                              "0x0E8 0x0\n"  // APERF
                              "0x19C 0x0\n"  // THERM
                              "0x198 0x0\n"  // PERF
                              "0x309 0xFFFFFFFFFFFFFFFF\n"  // CTR0
                              "0x38D 0x0000000000000333\n"  // IA32_FIXED_CTR_CTRL
                              "0x38F 0x000000070000000F\n"  // IA32_PERF_GLOBAL_CTRL
                              "0x611 0x0\n"  // MSR_PKG_ENERGY_STATUS
			      "0x639 0x0\n"  // MSR_PP0_ENERGY_STATUS
                              ;

//static uint8_t const nCPUs = 32;

void
set_allowlist()
{
    int fd = open("/dev/cpu/msr_allowlist", O_WRONLY);
    assert(-1 != fd);
    ssize_t nbytes = write(fd, allowlist, strlen(allowlist));
    assert((ssize_t)strlen(allowlist) == nbytes);
    close(fd);
}

struct msr_batch_array batch;

struct msr_batch_op ops_enable_counters[] = {
    {.cpu=9, .op=MSR_WRITE, .msr=IA32_PERF_GLOBAL_CTRL, .writeval=0x0},           // Turn off performance counters
    {.cpu=9, .op=MSR_WRITE, .msr=IA32_FIXED_CTR0,       .writeval=0x0},           // Zero out "instructions retired" accumulator
    {.cpu=9, .op=MSR_WRITE, .msr=IA32_FIXED_CTR_CTRL,   .writeval=0x3},           // Enable USR + OS for instructions retired
    {.cpu=9, .op=MSR_WRITE, .msr=IA32_PERF_GLOBAL_CTRL, .writeval=0x100000000ULL} // Start collecting instructions
};

struct msr_batch_op ops_disable_counters[] = {
    {.cpu=9, .op=MSR_WRITE, .msr=IA32_PERF_GLOBAL_CTRL, .writeval=0x0},           // Turn off performance counters
    {.cpu=9, .op=MSR_WRITE, .msr=IA32_FIXED_CTR0,       .writeval=0x0},           // Zero out "instructions retired" accumulator
    {.cpu=9, .op=MSR_WRITE, .msr=IA32_FIXED_CTR_CTRL,   .writeval=0x0},           // Disable USR + OS for instructions retired
};

struct msr_batch_op ops_poll_pkg_energy[] = {
    {.cpu=9, .op=MSR_POLL | MPERF0 | MPERF1 | MPERF2, .msr=MSR_PKG_ENERGY_STATUS },
};

struct msr_batch_op ops_poll_pp0_energy[] = {
    {.cpu=9, .op=MSR_POLL | MPERF0 | MPERF1 | MPERF2, .msr=MSR_PP0_ENERGY_STATUS },
};

void
dump_ops( struct msr_batch_array *b ){
    static bool initialized = 0;
    if(!initialized){
        initialized = 1;
        fprintf(stdout,"cpu op err msr writeval readval pollval wmask mperf0 mperf1 mperf2 mperf3 therm valid perf ins\n");
    }
    struct msr_batch_op *o = b->ops;
    for( size_t i = 0; i < b->numops; i++, o++ ){
        fprintf(stdout,
                //cpu      op         err       msr        writeval  readval   pollval   wmask     mperf0    mperf1    mperf2    mperf3    therm     valid     perf      ins
                "%"PRIu16" %#"PRIx16" %"PRIu32" %#"PRIx32" %"PRIu64" %"PRIu64" %"PRIu64" %"PRIu64" %"PRIu64" %"PRIu64" %"PRIu64" %"PRIu64" %"PRIu64" %"PRIu64" %"PRIu64" %"PRIu64"\n",
                (uint16_t)(o->cpu),
                (uint16_t)(o->op),
                (uint32_t)(o->err),
                (uint32_t)(o->msr),
                (uint64_t)(o->writeval),
                (uint64_t)(o->readval),
                (uint64_t)(o->pollval),
                (uint64_t)(o->wmask),
                (uint64_t)(o->mperf[0]),
                (uint64_t)(o->mperf[1]),
                (uint64_t)(o->mperf[2]),
                (uint64_t)(o->mperf[3]),
                (uint64_t)((o->therm) & (0xfULL<<27)>>27),
                (uint64_t)((o->therm) & (  1ULL<<31)>>31),
                (uint64_t)(o->perf),
                (uint64_t)(o->ins)
         );
    }
}



int main()
{
    int fd, rc;
    fd = open("/dev/cpu/msr_batch", O_RDONLY);
    assert(-1 != fd);

    set_allowlist();

    batch.ops = ops_enable_counters;
    batch.numops        = sizeof( ops_enable_counters ) / sizeof( struct msr_batch_op );
    rc = ioctl(fd, X86_IOC_MSR_BATCH, &batch);
    assert(-1 != rc);

    for( size_t i=0; i<10; i++ ){
	    batch.ops = ops_poll_pkg_energy;
	    batch.numops        = sizeof( ops_poll_pkg_energy ) / sizeof( struct msr_batch_op );
	    rc = ioctl(fd, X86_IOC_MSR_BATCH, &batch);
	    assert(-1 != rc);
	    dump_ops(&batch);
    }
    for( size_t i=0; i<10; i++ ){
	    batch.ops = ops_poll_pp0_energy;
	    batch.numops        = sizeof( ops_poll_pp0_energy ) / sizeof( struct msr_batch_op );
	    rc = ioctl(fd, X86_IOC_MSR_BATCH, &batch);
	    assert(-1 != rc);
	    dump_ops(&batch);
    }

    batch.ops = ops_disable_counters;
    batch.numops        = sizeof( ops_enable_counters ) / sizeof( struct msr_batch_op );
    rc = ioctl(fd, X86_IOC_MSR_BATCH, &batch);
    assert(-1 != rc);

    close(fd);
    return 0;
}
