# Copyright (c) 2017 The University of Edinburgh

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

# http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

FC=ftn
CC=cc

ifneq ($(PE_ENV),CRAY)
  CFLAGS_EXTRA = -std=c99
endif

CFLAGS = -fPIC $(CFLAGS_EXTRA)

all: pat_mpi_lib.o libpatmpi.a libpatmpi.so pat_mpi_test pat_mpi_testf

pat_mpi_lib.o: pat_mpi_lib.c pat_mpi_lib.h
	$(CC) $(CFLAGS) -c pat_mpi_lib.c

libpatmpi.a: pat_mpi_lib.o
	ar rcs $@ $^

libpatmpi.so: pat_mpi_lib.o
	$(CC) -fPIC -shared -o libpatmpi.so pat_mpi_lib.o

pat_mpi_lib_interface.o: pat_mpi_lib_interface.f90
	$(FC) -c pat_mpi_lib_interface.f90

pat_mpi_testf: pat_mpi_testf.f90 pat_mpi_lib_interface.o pat_mpi_lib.o
	$(FC) -o pat_mpi_testf pat_mpi_testf.f90 pat_mpi_lib_interface.o pat_mpi_lib.o

pat_mpi_test: pat_mpi_test.c pat_mpi_lib.o pat_mpi_lib.h
	$(CC) $(CFLAGS) -o pat_mpi_test pat_mpi_test.c pat_mpi_lib.o

clean: 
	$(RM) *.o *.mod *.out *.a *.so pat_mpi_test pat_mpi_testf
