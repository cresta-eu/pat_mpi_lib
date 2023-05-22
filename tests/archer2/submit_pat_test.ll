#!/bin/bash --login
  
#SBATCH --job-name=pat
#SBATCH --nodes=2
#SBATCH --tasks-per-node=128
#SBATCH --cpus-per-task=1
#SBATCH --time=00:20:00
#SBATCH --account=z19
#SBATCH --partition=standard
#SBATCH --qos=standard


module -q load perftools-base
module -q load perftools


export OMP_NUM_THREADS=1

export PAT_RT_SUMMARY=1
export PAT_RT_REGION_MAX=100000
export MY_RT_CTRCAT=PAT_CTRS_CPU
export PAT_RT_PERFCTR=PAPI_TOT_CYC,PAPI_FP_OPS

srun --distribution=block:block --hint=nomultithread --unbuffered ./pat_mpi_test+pat 
