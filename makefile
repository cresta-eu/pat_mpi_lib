FC=ftn
CC=cc

ifneq ($(PE_ENV),CRAY)
  CFLAGS_EXTRA = -std=c99
endif

CFLAGS = -fPIC -I/opt/cray/perftools/6.2.2/include $(CFLAGS_EXTRA)

all: pat_mpi_lib.o libpatmpi.a pat_mpi_test pat_mpi_testf

pat_mpi_lib.o: pat_mpi_lib.c pat_mpi_lib.h
	$(CC) $(CFLAGS) -c pat_mpi_lib.c

libpatmpi.a: pat_mpi_lib.o
	ar rcs $@ $^

pat_mpi_lib_interface.o: pat_mpi_lib_interface.f90
	$(FC) -c pat_mpi_lib_interface.f90

pat_mpi_testf: pat_mpi_testf.f90 pat_mpi_lib_interface.o pat_mpi_lib.o
	$(FC) -o pat_mpi_testf pat_mpi_testf.f90 pat_mpi_lib_interface.o pat_mpi_lib.o
	
pat_mpi_test: pat_mpi_test.c pat_mpi_lib.o pat_mpi_lib.h
	$(CC) $(CFLAGS) -o pat_mpi_test pat_mpi_test.c pat_mpi_lib.o
	
clean: 
	$(RM) *.o *.mod *.out *.a pat_mpi_test pat_mpi_testf