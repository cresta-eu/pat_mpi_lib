/* 
  Copyright (c) 2017 The University of Edinburgh.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mpi.h>
#include "pat_mpi_lib.h"

int main(int argc,char **argv){
  
  int i, ierr, rank, comm;
  
  MPI_Init(NULL, NULL);
  
  pat_mpi_initialise("./patc_test.out");
  
  for (i=1; i<10; i++) {
    pat_mpi_record(i, 1, 1, 1);
    sleep(500000);
  }
  
  pat_mpi_finalise();
  
  MPI_Finalize();

  return EXIT_SUCCESS;

}

