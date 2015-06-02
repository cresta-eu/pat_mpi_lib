/*

 Copyright (c) 2014 Michael Bareford
 All rights reserved.

 See the LICENSE file elsewhere in this distribution for the
 terms under which this software is made available.

*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mpi.h>
#include "pat_mpi_lib.h"

int main(int argc,char **argv){
  
  int i, ierr, rank, comm;
  
  MPI_Init(NULL, NULL);
  
  pat_mpi_open("./patc_test.out");
  
  for (i=1; i<10; i++) {
    pat_mpi_monitor(i, 1);
    sleep(500000);
  }
  
  pat_mpi_close();
  
  MPI_Finalize();

  return EXIT_SUCCESS;

}

