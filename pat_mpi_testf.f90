! Copyright (c) 2014 Michael Bareford
! All rights reserved.
!
! See the LICENSE file elsewhere in this distribution for the
! terms under which this software is made available.

program pat_testf
  use mpi
  use pat_mpi_lib
  
  implicit none
 
  integer :: ierr, i
  integer :: comm, rank
  character (len=14) :: out_fn = "patc_test.out"//CHAR(0)
  
  call MPI_Init(ierr)
  
  call pat_mpi_open(out_fn)

  do i=1,10
    call pat_mpi_monitor(i,1)
    call sleep(1)
  end do

  call pat_mpi_close()

  call MPI_Finalize(ierr)
 
end program pat_testf

