/*

 Copyright (c) 2014 Michael Bareford
 All rights reserved.

 See the LICENSE file elsewhere in this distribution for the
 terms under which this software is made available.

*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <mpi.h>
#include "pat_api.h"
#include "pat_mpi_lib.h"


#define MAXFLEN 100
#define PAT_RT_SEPARATOR ","

#define PAT_REGION_ID_OPEN 301
#define PAT_REGION_ID_MONITOR 302
#define PAT_REGION_ID_CLOSE 303

static char ver[]="1.0.0";

static int rank = -1;
static int min_node_rank = 0;
static int monitor_cnt = 0;
static int non_monitor_cnt = 0;
static MPI_Comm mpi_comm_monitor;
static FILE *fout = NULL;
static int first_monitor = 1;
static double tm0 = 0.0;
static int last_nstep = 0;

static int cntr_cat_cnt = 0;
static int *cntr_cat = NULL;
static int ncntrs_cat = 0;
static int ncntrs = 0;
static char **cntr_name = NULL;
static unsigned long *cntr_value = NULL;
static unsigned long *cntr_value_tot = NULL;

static int open = 0;


// return 1 if pat_mpi_open has been called successfully
int pat_mpi_ok() {
  int ok = 0;
  
  if (-1 != rank) {
    
    if (min_node_rank == rank) {
      ok = (monitor_cnt > 0 && cntr_cat_cnt > 0 && ncntrs > 0);
      
      if (0 == rank) {
        ok = (ok && fout);
      }
    }
    else {
      ok = (non_monitor_cnt > 0);
    }
    
  }
  
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
  else if (0 == strcmp("PAT_CTRS_NB", str)) {
    // AMD NorthBridge on nodes
    cat = PAT_CTRS_NB;
  }
  else if (0 == strcmp("PAT_CTRS_RAPL", str)) {
    // Running Avr Power Level on package
    cat = PAT_CTRS_RAPL;
  }
  else if (0 == strcmp("PAT_CTRS_PM", str)) {
    // Cray Power Management
    cat = PAT_CTRS_PM;
  }
  else {
    // next counter category
    cat = PAT_CTRS_PM+1;
  }
  
  return cat;
}



// rank zero opens the output file
// allow the calling rank to self-identify as a monitoring process
// each monitoring process obtains the number of monitors
// monitoring processes agree on the number of counters and allocate sufficient memory
// call pat_mpi_monitor(-1,1)
void pat_mpi_open(char* out_fn) {
  
  int node_name_len, nn_i, nn_m, node_num;
  char node_name[MPI_MAX_PROCESSOR_NAME];
  MPI_Comm mpi_comm_node;
  int str_len = 0, cat_cnt = 0;
  char* cat_list_str = NULL;
  char* cat_str = NULL;
  
  
  if (0 != open) {
    return;
  }
  
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  
  // open file for counter data  
  if (0 == rank) {
    if (fout) {
      fclose(fout);
      fout = NULL;
    }
    // open performance counter data file
    if (out_fn) {
      fout = fopen(out_fn, "w"); 
    }
    if (!fout) {
      fout = fopen("./patc.out", "w");
    }
  }


  // determine the number of the node on which this rank is running
  MPI_Get_processor_name(node_name, &node_name_len);
  if (node_name_len > 0) {
    nn_i = node_name_len-1;
    nn_m = 1;
    node_num = 0;
    while (nn_i > 0 && 0 != isdigit(node_name[nn_i])) {
      node_num = node_num + (node_name[nn_i]-'0')*nn_m;
      nn_m = 10*nn_m;
      nn_i = nn_i - 1;
    }
  }
  
  // determine if this rank is the minimum rank for the identified node number
  MPI_Comm_split(MPI_COMM_WORLD, node_num, rank, &mpi_comm_node);
  MPI_Allreduce(&rank, &min_node_rank, 1, MPI_INTEGER, MPI_MIN, mpi_comm_node);
 
  if (rank == min_node_rank) {
    // the minimum rank on a node is responsible for monitoring
    // the performance counters on that node  
    MPI_Comm_split(MPI_COMM_WORLD, 1, rank, &mpi_comm_monitor);
  }
  else {
    MPI_Comm_split(MPI_COMM_WORLD, 0, rank, &mpi_comm_monitor);
  }
  
  // ensure all monitor ranks have self-identified...
  MPI_Barrier(MPI_COMM_WORLD);
  if (min_node_rank == rank) {
    // ...before each monitor rank obtains the total number of monitors
    MPI_Comm_size(mpi_comm_monitor, &monitor_cnt);   
  }
  else {
    // and other processes obtain the number of non-monitors
    MPI_Comm_size(mpi_comm_monitor, &non_monitor_cnt);
  }
   
  
  PAT_region_begin(PAT_REGION_ID_OPEN, "pat_mpi_open");
  if (min_node_rank == rank) {
    // all monitors determine the number of counters for the given
    // counter categories or PAT_CTRS_PM if no categories are specified    
    PAT_record(PAT_STATE_ON);
    
    // get the number of counter categories    
    cat_list_str = getenv("MY_RT_CTRCAT");
    str_len = strlen(cat_list_str);
    cat_cnt = str_len > 0 ? 1 : 0;
    for (int i = 0; i < str_len; i++) {
      if (PAT_RT_SEPARATOR[0] == cat_list_str[i]) {
        cat_cnt++;
      }  
    }
    
    int ncntrs_loc = 0;
    cntr_cat_cnt = cat_cnt > 0 ? cat_cnt : 1;
    cntr_cat = (int*) calloc(cntr_cat_cnt, sizeof(int));

    if (cntr_cat) {
      // get the counter categories (see MY_RT_CTRCAT in job submission script)
      if (cat_cnt <= 0) {
        cntr_cat[0] = PAT_CTRS_PM;
      }
      else {
        cat_str = strtok(cat_list_str, PAT_RT_SEPARATOR);
        for (int i = 0; i < cntr_cat_cnt; i++) {
          cntr_cat[i] = get_cat_val(cat_str);
          cat_str = strtok(NULL, PAT_RT_SEPARATOR);
        }
      }
    
      // get the number of counters (see PAT_RT_PERFCTR in job submission script)
      // for each category
      ncntrs_loc = 0;
      for (int i = 0; i < cntr_cat_cnt; i++) {
        ncntrs_cat = 0;
        PAT_counters(cntr_cat[i], 0, 0, &ncntrs_cat);
        ncntrs_loc = ncntrs_loc + ncntrs_cat;
      }
    }
    else {
      cntr_cat_cnt = 0;
    }
    
    // ensure all monitors agree on the total number of counters
    MPI_Allreduce(&ncntrs_loc, &ncntrs, 1, MPI_INTEGER, MPI_MAX, mpi_comm_monitor);
  
    if (ncntrs > 0) {
      // allocate memory to hold counter names and values
      cntr_name = (char**) calloc(ncntrs, sizeof(char*));
      if (cntr_name) {
        for (int i=0; i < ncntrs; i++) {
          cntr_name[i] = (char*) calloc(MAXFLEN, sizeof(char));
        }
      
        cntr_value = (unsigned long*) calloc(ncntrs, sizeof(unsigned long));
        cntr_value_tot = (unsigned long*) calloc(ncntrs, sizeof(unsigned long));
      }
      
      if (!cntr_name || !cntr_value || !cntr_value_tot) {
        ncntrs = 0;
      }
    }   
    
  } // end of <if (min_node_rank == rank)> clause
  else {
    // non-monitoring process
    PAT_record(PAT_STATE_OFF);
  }
  PAT_region_end(PAT_REGION_ID_OPEN);
  
  
  int ok = pat_mpi_ok(), all_ok = 0;
  MPI_Allreduce(&ok, &all_ok, 1, MPI_INTEGER, MPI_MIN, MPI_COMM_WORLD);
  open = all_ok;
  if (0 == open) {
    pat_mpi_close();
  }
  else {
    // do initial monitoring call, which ends with MPI_Barrier
    first_monitor = 1;
    pat_mpi_monitor(-1, 1);
  }
  
} // end of pat_mpi_open() function



// read counter values if first rank on node,
// and output those values if rank zero
void pat_mpi_monitor(int nstep, int sstep) {
   
  if (0 == open) {
    return;
  }
  
  // if monitoring process (i.e., first process on node)    
  if (min_node_rank == rank) {
    
    // get time
    double tm = MPI_Wtime();
    if (1 == first_monitor) {
      tm0 = tm;
      first_monitor = 0;
    }
  
    // read counters
    PAT_region_begin(PAT_REGION_ID_MONITOR, "pat_mpi_monitor");
    for (int i=0, j=0, ncntrs_cnt=0; i < cntr_cat_cnt && j < ncntrs; i++, j += ncntrs_cat) {
      ncntrs_cat = 0;
      PAT_counters(cntr_cat[i], (const char**) &cntr_name[j], &cntr_value[j], &ncntrs_cat);
    }
    PAT_region_end(PAT_REGION_ID_MONITOR);
  
    // calculate totals and averages
    for (int i=0; i < ncntrs; i++) {  
      MPI_Allreduce(&cntr_value[i], &cntr_value_tot[i], 1, MPI_UNSIGNED_LONG, MPI_SUM, mpi_comm_monitor);
    }  
           
    // output data
    if (0 == rank) {
      if (tm0 == tm) {
        // this function is being called by pat_mpi_open()
        if (fout) {
          fprintf(fout, "pat_mpi_lib v%s: time (s), step, substep", ver);
          for (int i=0; i < ncntrs; i++) {
            fprintf(fout, ", %s", cntr_name[i]); 
          }
          fprintf(fout, "\n");
        }
      }
    
      if (fout) { 
        // update counter data file   
        fprintf(fout, "%f %d %d", tm-tm0, nstep, sstep);
        for (int i = 0; i < ncntrs; i++) {
          fprintf(fout, " %ld", cntr_value_tot[i]);
          if (monitor_cnt > 1) {
            fprintf(fout, " %f", ((double) cntr_value_tot[i])/((double) monitor_cnt));
          } 
        }
        fprintf(fout, "\n");
      }
    }
    
  } // end of <if (min_node_rank == rank)> clause
  
  last_nstep = nstep;
  
  MPI_Barrier(MPI_COMM_WORLD);
  
} // end of pat_mpi_monitor() function



// close the file used to record counter data
void pat_mpi_close() {

  if (1 == open) {
    // do the last monitoring call
    pat_mpi_monitor(last_nstep+1, 1);
  }
  
  // if monitoring process (i.e., first process on node)    
  if (min_node_rank == rank) {
  
    PAT_region_begin(PAT_REGION_ID_CLOSE, "pat_mpi_close");
    PAT_record(PAT_STATE_OFF);
    PAT_region_end(PAT_REGION_ID_CLOSE);
    
    if (ncntrs > 0) {  
      free(cntr_value_tot);
      free(cntr_value);
  
      for (int i = 0; i < ncntrs; i++) {
        free(cntr_name[i]);
      }
      free(cntr_name);
      ncntrs = 0;
    }
    
    if (cntr_cat_cnt > 0) {  
      free(cntr_cat);
      cntr_cat_cnt = 0;
    }

    if (0 == rank) {
      if (fout) {
        // close performance counter data file
        fclose(fout);
        fout = NULL;
      }
    }
    
  } // end of <if (min_node_rank == rank)> clause
  
  open = 0;
  MPI_Barrier(MPI_COMM_WORLD);
  
} // end of pat_mpi_close() function
