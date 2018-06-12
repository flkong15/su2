#!/bin/bash

# Requires I_MPI_ROOT and MKLROOT are set by Intel tools environment script.

# Set to path for SU2 installation
CURRENT_DIR=$(pwd)
INSTALL_DIR=${CURRENT_DIR}/build

./configure -prefix=${INSTALL_DIR} --enable-mpi --with-cc=${I_MPI_ROOT}/intel64/bin/mpiicc --with-cxx=${I_MPI_ROOT}/intel64/bin/mpiicpc CXXFLAGS="-O3 -I${MKLROOT}/include -DHAVE_MKL -DMKL_DIRECT_CALL_SEQ -xCORE-AVX512" LIBS="-Wl,--start-group ${MKLROOT}/lib/intel64/libmkl_intel_lp64.a ${MKLROOT}/lib/intel64/libmkl_sequential.a ${MKLROOT}/lib/intel64/libmkl_core.a -Wl,--end-group -lpthread -lm -ldl"
