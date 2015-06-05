/*
 Copyright (c) 2014 Michael Bareford
 All rights reserved.

 See the LICENSE file elsewhere in this distribution for the
 terms under which this software is made available.

*/

#ifndef PAT_MPI_LIB_H
#define PAT_MPI_LIB_H

extern void pat_mpi_open(char* out_fn);
extern void pat_mpi_monitor(int nstep, int sstep);
extern void pat_mpi_close();

#endif
