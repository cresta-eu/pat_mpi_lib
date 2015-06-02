The PAT MPI library makes it possible to monitor a user-defined set of
hardware performance counters during the execution of a MPI code running
across multiple compute nodes.

This repository holds source code for the pat_mpi_lib library. There are
also two small test harnesses that demonstrate how to call the library
functions from Fortran and C codes.

The makefile is intended for use on the ARCHER Cray XC30 MPP Supercomputer:
the makefile script references the PE_ENV environment variable.

Before compiling please load the perftools module ("module load perftools"),
and then compile by running "make". Once you have compiled and linked your
application code with libpatmpi, you must then run "pat_build -w exe_file".
This should result in a new file with the same name as your executable but
with a "+pat" suffix.


The following text describes the interface provided by the three functions
of the pat_mpi_lib library.

void pat_mpi_open(char* out_fn)

The parameter, out_fn, points to a null-terminated string that specifies the name of the file that will hold the counter data:
a NULL parameter value will set the output file name to ./patc.out. The open function also calls pat_mpi_monitor(-1,1) in order to determine a baseline for the counter data. In addition, rank 0 establishes a temporal baseline by calling MPI_Wtime and also writes a one-line  header to the output file, which gives the library version followed by the names of the data items that will appear in the subsequent lines.

void pat_mpi_close(void)

This function calls pat_mpi_monitor(nstep+1,1), see below. All counter files are closed, then rank 0 closes the output file.

void pat_mpi_monitor(int nstep, int sstep)

The two parameters allow the client to label each set of counter values that are output by rank 0. The output file contains lines of space-separated fields. A description of each field follows (the  C-language data type is given in square brackets).

Time [double]: the time as measured by MPI_Wtime (called by rank zero) that has elapsed since the last call to pat_mpi_open. 

Step [int]: a simple numerical label: e.g., the iteration count, assuming pat_mpi_monitor is being called from within a loop. 

Sub-step [int]: another numerical label that might be required if there is more than one monitor call within the same loop.

Counter Total [unsigned long]: the sum of the counter values across all nodes

Counter Average [double]: the average of the counter values across all nodes 

The last two fields (Counter Total and Counter average) are repeated for however many counters are being monitored. The
average values are not output if the code is running on one node only.

To specify which counters you wish to monitor you must specify three environment variables within your job submission
script. For example, the following will record energy and power usage.

module load perftools

export PAT_RT_SUMMARY=1

export MY_RT_CTRCAT=PAT_CTRS_PM

export PAT_RT_PERFCTR=PM_POWER:NODE,PM_ENERGY:NODE

And this setup will return cache-related data.

module load perftools

export PAT_RT_SUMMARY=1

export MY_RT_CTRCAT=PAT_CTRS_CPU

export PAT_RT_PERFCTR=2

Hardware counter group 2 covers L1D:REPLACEMENT, L2_RQSTS:ALL_DEMAND_DATA_RD, L2_RQSTS:DEMAND_DATA_RD_HIT, CPU_CLK_UNHALTED:THREAD_P, CPU_CLK_UNHALTED:REF_P, MEM_UOPS_RETIRED:ALL_LOADS, LLC_MISSES, LLC_REFERENCES.
