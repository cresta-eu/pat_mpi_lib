The PAT MPI library makes it possible to monitor a user-defined set of
hardware performance counters during the execution of a MPI code running
across multiple compute nodes.

This repository holds source code for the `pat_mpi_lib` library. There are
also two small test harnesses that demonstrate how to call the library
functions from Fortran and C codes.

The makefile is intended for use on the [ARCHER2 HPE Cray EX Supercomputer](www.archer2.ac.uk):
the makefile script references the `PE_ENV` environment variable.

Before compiling please load the perftools module (`module load perftools`),
and then compile by running `make`. Once you have compiled and linked your
application code with `libpatmpi`, you must then run `pat_build -w <executable>`.
This should result in a new file with the same name as your executable but
with a `+pat` suffix.


The following text describes the interface provided by the four functions
of the `pat_mpi_lib` library.

```c
void pat_mpi_initialise(const char* out_fn)
```

The parameter, `out_fn`, points to a null-terminated string that specifies the name of the file that will hold the counter data: a NULL parameter value will set the output file name to `pat_log.out`. The initialise function also calls `pat_mpi_record(-1,1,1,0)` in order to determine a baseline for the counter data. In addition, rank 0 establishes a temporal baseline by calling `MPI_Wtime` and also writes a one-line header to the output file, which gives the library version followed by the names of the data items that will appear on subsequent lines.

```c
void pat_mpi_finalise(void)
```

The finalise function calls `pat_mpi_record(nstep,1,1,0)` (described below). All counter files are closed, then rank 0 closes the output file.

```c
void pat_mpi_reset(const int initial_sync)
```
The reset function resets the counters to zero. If `initial_sync` is true `MPI_Barrier` is called before the reset.

```c
void pat_mpi_record(const int nstep, const int sstep, const int initial_sync, const int initial_rec)
```

The first two parameters (`nstep` and `sstep`) allow the client to label each set of counter values that are output by rank 0.<br>
If `initial_sync` is true `MPI_Barrier` is called before reading takes place.<br>
If `initial_sync` and `initial_rec` are both true then the energy counters are read before and after the initial barrier.<br> Note, `initial_rec` is only used when initial_sync is true.

The output file contains lines of space-separated fields. A description of each field follows (the  C-language data type is given in square brackets).

**Time [double]**: the time as measured by `MPI_Wtime` (called by rank zero) that has elapsed since the last call to pat_mpi_open.<br> 
**Step [int]**: a simple numerical label: e.g., the iteration count, assuming `pat_mpi_record` is being called from within a loop.<br> 
**Sub-step [int]**: another numerical label that might be required if there is more than one monitor call within the same loop.<br>
**Counter Total [long long]**: the sum of the counter values across all cores.<br>

The last field, Counter Total, is repeated for however many counters are being monitored.

To specify which counters you wish to monitor you must specify one environment variable within your job submission
script. For example, the following will record the number of floating point operations (FLOPs) and the number of accesses to the level 2 data cache.

```bash
module load perftools
export PAT_RT_SUMMARY=1
export PAT_RT_REGION_MAX=100000
export MY_RT_CTRCAT=PAT_CTRS_CPU
export PAT_RT_PERFCTR=PAPI_TOT_CYC,PAPI_FP_OPS,PAPI_L2_DCA
```

The Fortran-like code below shows how the `pat_mpi_lib` library routines could be integrated into an application code, within the main time step loop.

```fortran
...
integer :: pat_res
character(len=13) :: pat_out_fn = "app01pat.out"//CHAR(0)
...
call pat_mpi_initialise(pat_out_fn)

do step=1,nsteps
    call pat_mpi_reset(1)
    call subroutine1()
    pat_res = pat_mpi_record(step,1,1,0)
    
    call pat_mpi_reset(1)
    call subroutine2()
    pat_res = pat_mpi_record(step,2,1,0)
    
    call pat_mpi_reset(1)
    call subroutine3()
    pat_res = pat_mpi_record(step,3,1,0)
end do

call pat_mpi_finalise()
...

```

The content of `app01pat.out` would then show how much the counter values changed as a consequence of calling subroutines 1 to 3.
