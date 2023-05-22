/* 
  Copyright (c) 2023 The University of Edinburgh.

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

#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <mpi.h>
#include "pat_api.h"
#include "pat_mpi_lib.h"


static char ver[]="6.0.0";

#define PAT_MPI_LIB_UNINITIALISED 1001
#define MAX_NAME_LEN 128
#define PAT_RT_SEPARATOR ","

static int rank = -1;
static int root_rank = -1;
static int nprocs = 0;
static FILE* log_fp = NULL;
static int first_record = 1;
static double tm0 = 0.0;
static double tm = 0.0;
static int last_nstep = 0;

static int ncats = 0;
static int* cat_ids = NULL;
static int* cat_ncntrs = NULL;
static MPI_Comm* cat_comms = NULL;
  
static int ncntrs = 0;
static char*** cat_cntr_names = NULL;
static unsigned long** cat_cntr_baselines = NULL;
static unsigned long** cat_cntr_values = NULL;
static unsigned long long** cat_cntr_value_totals = NULL;

static int pat_res = 0;
static int initialised = 0;
static int debug = 0;
static int region_id = 1;


// return 1 if pat_mpi_initialise has been called successfully
int pat_mpi_ok() {
  int ok = 0;

  if (-1 != rank && -1 != root_rank && nprocs > 0) {
    ok = 1;

    ok = (0 != ok && ncats > 0);
    ok = (0 != ok && NULL != cat_ids && NULL != cat_ncntrs && NULL != cat_comms);

    ok = (0 != ok && ncntrs > 0);
    ok = (0 != ok && NULL != cat_cntr_names);
    ok = (0 != ok && NULL != cat_cntr_baselines);
    ok = (0 != ok && NULL != cat_cntr_values);
    ok = (0 != ok && NULL != cat_cntr_value_totals); 
    for (int i = 0; i < ncats; i++) {
      ok = (0 != ok && NULL != cat_cntr_names[i]);
      for (int j = 0; j < cat_ncntrs[i]; j++) {
        ok = (0 != ok && NULL != cat_cntr_names[i][j]);
      }
      ok = (0 != ok && NULL != cat_cntr_baselines[i]);
      ok = (0 != ok && NULL != cat_cntr_values[i]);
      ok = (0 != ok && NULL != cat_cntr_value_totals[i]); 
    }
     
    if (root_rank == rank) {
      ok = (0 != ok && NULL != log_fp);
    }
  } // end of <if (-1 != rank && -1 != root_rank && nprocs > 0)> clause

  return ok;
}


// convert string to counter category number
int get_cat_val(char* str) {
  int cat = 0;
  
  if (!str) {
    cat = 0;
  }
  else if (0 == strcmp("PAT_CTRS_CPU", str)) {
    // HWPCs on processor
    cat = PAT_CTRS_CPU;
  }
  else if (0 == strcmp("PAT_CTRS_NETWORK", str)) {
    // NWPCs on network router
    cat = PAT_CTRS_NETWORK;
  }
  else if (0 == strcmp("PAT_CTRS_ACCEL", str)) {
    // HWPCs on attached GPUs
    cat = PAT_CTRS_ACCEL;
  }
  else if (0 == strcmp("PAT_CTRS_ACCMG", str)) {
    // HWPCs on attached GPUs
    cat = PAT_CTRS_ACCEL;
  }
  else if (0 == strcmp("PAT_CTRS_RAPL", str)) {
    // Running Avr Power Level on package
    cat = PAT_CTRS_RAPL;
  }
  else if (0 == strcmp("PAT_CTRS_PM", str)) {
    // Cray Power Management
    cat = PAT_CTRS_PM;
  }
  else if (0 == strcmp("PAT_CTRS_UNCORE", str))
  {
    // Intel Uncore on socket
    cat = PAT_CTRS_UNCORE;
  }
  else {
    // next counter category
    cat = PAT_CTRS_UNCORE+1;
  }

  return cat;
}


// get the number of counter categories
// allocate memory for category arrays
// determine the number of counters in each category
// determine the root rank
// allocate the memory for counter names and values
// monitoring processes agree on the number of counters and allocate sufficient memory
// call pat_mpi_record(-1,1,1,0)
void pat_mpi_initialise(const char* log_fpath) {
  
  int str_len = 0;
  int ncntrs_max = 0;
  char* cat_list_str = NULL;
  char* cat_name = NULL;

  if (0 != initialised) {
    // already initialised
    return;
  }

  // initialise MPI variables
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  root_rank = 0;

  // get the number of counter categories
  /////////////////////////////////////////////////////
  ncats = 0;
  ncntrs = 0;
  cat_list_str = getenv("MY_RT_CTRCAT");
  str_len = strlen(cat_list_str);
  ncats = str_len > 0 ? 1 : 0;
  for (int i = 0; i < str_len; i++) {
    if (PAT_RT_SEPARATOR[0] == cat_list_str[i]) {
      ncats++;
    }
  }

  if (0 != debug && root_rank == rank) {
    printf("%d: %d categories listed.\n", rank, ncats);
  }

  if (ncats <= 0) {
    return;
  }
  /////////////////////////////////////////////////////

  // allocate memory for category arrays
  // determine the number of counters in each category
  // determine the root rank
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  cat_ids = (int*) calloc(ncats, sizeof(int));
  if (NULL == cat_ids) {
    printf("%d: failed to allocate memory for category ids.\n", rank);
    return;
  }
  cat_ncntrs = (int*) calloc(ncats, sizeof(int));
  if (NULL == cat_ncntrs) {
    printf("%d: failed to allocate memory for category counter counts.\n", rank);
    free(cat_ids);
    cat_ids = NULL;
    return;
  }
  cat_comms = (MPI_Comm*) calloc(ncats, sizeof(MPI_Comm));
  if (NULL == cat_comms) {
    printf("%d: failed to allocate memory for category MPI communicators.\n", rank);
    free(cat_ids);
    cat_ids = NULL;
    free(cat_ncntrs);
    cat_ncntrs = NULL;
    return;
  }
  
  pat_res = PAT_record(PAT_STATE_ON);
  if (PAT_API_OK != pat_res) {
    printf("%d: PAT_record(PAT_STATE_ON) failed with error %d.\n", rank, pat_res);
  }

  cat_name = strtok(cat_list_str, PAT_RT_SEPARATOR);
  ncntrs = 0;
  pat_res = PAT_region_begin(region_id, "pat_mpi_initialise");
  if (PAT_API_OK != pat_res) {
    printf("%d: PAT_region_begin(PAT_REGION_INITIALISE) failed with error %d.\n", rank, pat_res);
  }
  for (int i = 0; i < ncats; i++) {
    cat_ids[i] = get_cat_val(cat_name);
    cat_name = strtok(NULL, PAT_RT_SEPARATOR);

    pat_res = PAT_counters(cat_ids[i], 0, 0, &cat_ncntrs[i]);
    if (0 != debug) {
      if (PAT_API_OK != pat_res) {
        printf("%d: PAT_counters failed with error %d within pat_mpi_initialise.\n", rank, pat_res);
      }
      else {
        printf("%d: counter category %d has %d counter(s).\n", rank, cat_ids[i], cat_ncntrs[i]);
      }
    }
    MPI_Comm_split(MPI_COMM_WORLD, cat_ncntrs[i], rank, &cat_comms[i]);

    ncntrs += cat_ncntrs[i];
  }
  pat_res = PAT_region_end(region_id);
  region_id++;
  if (PAT_API_OK != pat_res) {
    printf("%d: PAT_region_end(%d) failed with error %d.\n", rank, region_id, pat_res);
  }

  ncntrs_max = 0;
  MPI_Allreduce(&ncntrs, &ncntrs_max, 1, MPI_INTEGER, MPI_MAX, MPI_COMM_WORLD);
  root_rank = (ncntrs == ncntrs_max) ? rank : nprocs;
  MPI_Allreduce(MPI_IN_PLACE, &root_rank, 1, MPI_INTEGER, MPI_MIN, MPI_COMM_WORLD);
  ////////////////////////////////////////////////////////////////////////////////////////////////////

  // allocate the memory for counter names and values
  //////////////////////////////////////////////////////////////////////////////////////////////////////////
  if (ncats > 0 && ncntrs > 0) {
    cat_cntr_names = (char***) calloc(ncats, sizeof(char**));
    cat_cntr_baselines = (unsigned long**) calloc(ncats, sizeof(unsigned long*));
    cat_cntr_values = (unsigned long**) calloc(ncats, sizeof(unsigned long*));
    cat_cntr_value_totals = (unsigned long long**) calloc(ncats, sizeof(unsigned long long*));
    if (cat_cntr_names && cat_cntr_baselines && cat_cntr_values && cat_cntr_value_totals) {
      for (int i = 0; i < ncats; i++) {
        cat_cntr_names[i] = (char**) calloc(cat_ncntrs[i], sizeof(char*));
        if (cat_cntr_names[i]) {
          for (int j = 0; j < cat_ncntrs[i]; j++) {
            cat_cntr_names[i][j] = (char*) calloc(MAX_NAME_LEN, sizeof(char));
          }
        }
        cat_cntr_values[i] = (unsigned long*) calloc(cat_ncntrs[i], sizeof(unsigned long));
	cat_cntr_baselines[i] = (unsigned long*) calloc(cat_ncntrs[i], sizeof(unsigned long));
        cat_cntr_value_totals[i] = (unsigned long long*) calloc(cat_ncntrs[i], sizeof(unsigned long long));
      }
    }
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////

  // root rank opens file for counter data
  ////////////////////////////////////////
  if (root_rank == rank) {
    if (NULL != log_fp) {
      fclose(log_fp);
      log_fp = NULL;
    }
    
    // open performance counter data file
    if (NULL != log_fpath) {
      struct stat buffer;   
      if (0 == stat(log_fpath, &buffer)) {
        log_fp = fopen(log_fpath, "a");
      }
      else {
        log_fp = fopen(log_fpath, "w");
      }
    }
    if (NULL == log_fp) {
      log_fp = fopen("./pat_log.out", "w");
    }
  }
  ////////////////////////////////////////
  
  int all_ok = pat_mpi_ok();
  if (0 != debug) {
    printf("%d: pat_ok() returned %d.\n", rank, all_ok);
  }
  MPI_Allreduce(MPI_IN_PLACE, &all_ok, 1, MPI_LOGICAL, MPI_LAND, MPI_COMM_WORLD);
  if (0 == all_ok) {
    initialised = 0;
    pat_mpi_finalise();
  }
  else {
    // do initial recording call, which ends with MPI_Barrier
    initialised = 1;
    first_record = 1;
    pat_mpi_record(-1, 1, 1, 0);
  }
  
} // end of pat_mpi_initialise() function


// read counter values and output those values if root rank (and reset=0)
int pat_mpi_read_counter_values(const int nstep, const int sstep, const int reset) {
   
  int ncntrs_test = 0;
  unsigned long long cntr_val = 0;
  
  if (0 == reset && root_rank == rank) {
    tm = MPI_Wtime();
    if (0 != first_record) {
      tm0 = tm;
      first_record = 0;
    }
  }

  if (ncats > 0 && ncntrs > 0) {
    // get counter values
    ///////////////////////////////////////////////////////////////////////////////////////////
    pat_res = PAT_region_begin(region_id, "pat_mpi_record");
    if (PAT_API_OK != pat_res) {
      printf("%d: PAT_region_begin(PAT_REGION_RECORD) failed with error %d.\n", rank, pat_res);
    }
    for (int i = 0; i < ncats; i++) {
      if (0 != reset) {
	pat_res = PAT_counters(cat_ids[i], (const char**) cat_cntr_names[i], cat_cntr_baselines[i], &ncntrs_test);
      }
      else {
        pat_res = PAT_counters(cat_ids[i], (const char**) cat_cntr_names[i], cat_cntr_values[i], &ncntrs_test);
      }
      if (0 != debug) {
        if (PAT_API_OK != pat_res) {
	  printf("%d: PAT_counters failed with error %d within pat_mpi_record.\n", rank, pat_res);
        }
        else if (cat_ncntrs[i] != ncntrs_test) {
	  printf("%d: counter category %d has %d counter(s) when %d expected.\n",
            rank, cat_ids[i], ncntrs_test, cat_ncntrs[i]);
        }
      }
    }
    pat_res = PAT_region_end(region_id);
    region_id++;
    if (PAT_API_OK != pat_res) {
      printf("%d: PAT_region_end(%d) failed with error %d.\n", rank, region_id, pat_res);
    }
    ///////////////////////////////////////////////////////////////////////////////////////////

    if (0 == reset) {
      // get counter value totals
      ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
      for (int i = 0; i < ncats; i++) {
        for (int j = 0; j < cat_ncntrs[i]; j++) {
          cntr_val = (unsigned long long) (cat_cntr_values[i][j] - cat_cntr_baselines[i][j]);
          MPI_Reduce(&cntr_val, &cat_cntr_value_totals[i][j], 1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, root_rank, cat_comms[i]);
        }
      }
    }
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  }

  // update counter data file
  if (0 == reset && root_rank == rank) {
    if (NULL != log_fp) {
      if (tm0 == tm) {
        fprintf(log_fp, "pat_mpi_lib v%s: time (s), step, substep", ver);
        for (int i = 0; i < ncats; i++) {
          for (int j = 0; j < cat_ncntrs[i]; j++) {
            fprintf(log_fp, ", %s", cat_cntr_names[i][j]);
          }
        }
        fprintf(log_fp, "\n");
      }
      fprintf(log_fp, "%f %d %d", tm-tm0, nstep, sstep);
      for (int i = 0; i < ncats; i++) {
        for (int j = 0; j < cat_ncntrs[i]; j++) {
          fprintf(log_fp, " %llu", cat_cntr_value_totals[i][j]);
        }
      }
      fprintf(log_fp, "\n");
    }
  }

  return pat_res;
  
} // end of pat_mpi_read_counter_values() function


// read counter baseline values
void pat_mpi_reset(const int initial_sync) {
  if (0 == initialised) {
    return;
  }

  if (0 != initial_sync) {
    MPI_Barrier(MPI_COMM_WORLD);
  }

  if (root_rank == rank) {
    tm0 = MPI_Wtime();
  }

  pat_mpi_read_counter_values(0, 0, 1);
}


// read and record counter values
// the reading will be labelled with the step and substep numbers
// if initial_sync is true MPI_Barrier is called before reading takes place
// if initial_sync and initial_rec are true then counters are read before and after initial barrier
// initial_rec is only used when initial_sync is true
int pat_mpi_record(const int nstep, const int sstep, const int initial_sync, const int initial_rec) {
   
  if (0 == initialised) {
    return PAT_MPI_LIB_UNINITIALISED;
  }

  int res = PAT_API_OK;
  if (0 != initial_sync) {
    if (0 != initial_rec) {
      res = pat_mpi_read_counter_values(nstep, sstep, 0);
    }
	
    MPI_Barrier(MPI_COMM_WORLD);
  }

  if (PAT_API_OK == res) {
    res = pat_mpi_read_counter_values(nstep, sstep, 0);
  }
    
  last_nstep = nstep;

  MPI_Barrier(MPI_COMM_WORLD);

  return res;
  
} // end of pat_mpi_record() function


// close the files used to read and record counter data
// do the last recording call
// deallocate the memory used to store counter names, ids and values
// close performance counter data file
void pat_mpi_finalise() {
  
  if (0 != initialised) {
    // do the last recording call
    pat_mpi_record(last_nstep+1, 1, 1, 0);
  }

  // turn off recording and free memory   
  pat_res = PAT_record(PAT_STATE_OFF);
  if (PAT_API_OK != pat_res) {
    printf("%d: PAT_record(PAT_STATE_OFF) failed with error %d.\n", rank, pat_res);
  }

  if (ncats > 0) {
    if (ncntrs > 0) {
      for (int i = 0; i < ncats; i++) {
        for (int j = 0; j < cat_ncntrs[i]; j++) {
          free(cat_cntr_names[i][j]);
        }
        free(cat_cntr_names[i]);
	free(cat_cntr_baselines[i]);
        free(cat_cntr_values[i]);
        free(cat_cntr_value_totals[i]);
      }

      free(cat_cntr_names);
      free(cat_cntr_baselines);
      free(cat_cntr_values);
      free(cat_cntr_value_totals);

      cat_cntr_names = NULL;
      cat_cntr_baselines = NULL;
      cat_cntr_values = NULL;
      cat_cntr_value_totals = NULL;

      ncntrs = 0;
    }

    free(cat_ids);
    free(cat_ncntrs);
    free(cat_comms);

    cat_ids = NULL;
    cat_ncntrs = NULL;
    cat_comms = NULL;

    ncats = 0;
  }

  if (root_rank == rank) {
    if (NULL != log_fp) {
      // close performance counter data file
      fclose(log_fp);
      log_fp = NULL;
    }
  }

  root_rank = -1;
  rank = -1;
  nprocs = 0;

  initialised = 0;
  MPI_Barrier(MPI_COMM_WORLD); 
  
} // end of pat_mpi_finalise() function
