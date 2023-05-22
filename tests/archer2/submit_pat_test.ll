#!/bin/bash --login
  
#SBATCH --job-name=pat
#SBATCH --nodes=4
#SBATCH --tasks-per-node=128
#SBATCH --cpus-per-task=1
#SBATCH --time=00:20:00
#SBATCH --account=z19
#SBATCH --partition=standard
#SBATCH --qos=short


module -q restore
module -q load cpe/21.09
module -q load PrgEnv-cray

module -q load perftools-init
module -q load perftools

export LD_LIBRARY_PATH=${CRAY_LD_LIBRARY_PATH}:${LD_LIBRARY_PATH}


export OMP_NUM_THREADS=1

export PAT_RT_SUMMARY=1
export PAT_RT_REGION_MAX=100000
export MY_RT_CTRCAT=PAT_CTRS_CPU
export PAT_RT_PERFCTR=PAPI_TOT_CYC,PAPI_FP_OPS,PAPI_L2_DCA

srun --distribution=block:block --hint=nomultithread --unbuffered ./pat_mpi_test 
