! Copyright (c) 2014 Michael Bareford
! All rights reserved.
!
! See the LICENSE file elsewhere in this distribution for the
! terms under which this software is made available.

module pat_mpi_lib
  implicit none

  public
  
  interface
   subroutine pat_mpi_open(out_fn) bind(c,name='pat_mpi_open')
    use, intrinsic :: iso_c_binding
    character(c_char) :: out_fn(*)
   end subroutine pat_mpi_open
  end interface

  interface
   subroutine pat_mpi_monitor(nstep,sstep) bind(c,name='pat_mpi_monitor')
    use, intrinsic :: iso_c_binding
    integer(c_int),value :: nstep
    integer(c_int),value :: sstep
   end subroutine pat_mpi_monitor
  end interface
  
  interface
   subroutine pat_mpi_close() bind(c,name='pat_mpi_close')
    use, intrinsic :: iso_c_binding
   end subroutine pat_mpi_close
  end interface
     

end module pat_mpi_lib
