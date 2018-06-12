! Copyright (c) 2017 The University of Edinburgh

! Licensed under the Apache License, Version 2.0 (the "License");
! you may not use this file except in compliance with the License.
! You may obtain a copy of the License at

! http://www.apache.org/licenses/LICENSE-2.0

! Unless required by applicable law or agreed to in writing, software
! distributed under the License is distributed on an "AS IS" BASIS,
! WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
! See the License for the specific language governing permissions and
! limitations under the License.

program pat_testf
  use mpi
  use pat_mpi_lib
  
  implicit none
 
  integer :: ierr, i, res
  integer :: comm, rank
  character (len=14) :: out_fn = "patc_test.out"//CHAR(0)
  
  call MPI_Init(ierr)
  
  call pat_mpi_initialise(out_fn)

  do i=1,10
    res = pat_mpi_record(1,i,1,1)
    call sleep(1)
  end do

  call pat_mpi_finalise()

  call MPI_Finalize(ierr)
 
end program pat_testf

