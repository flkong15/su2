/*!
 * \file CLinearPartitioner.cpp
 * \brief Helper class that provides the counts for each rank in a linear
 *        partitioning given the global count as input.
 * \author T. Economon
 * \version 7.0.2 "Blackbird"
 *
 * SU2 Project Website: https://su2code.github.io
 *
 * The SU2 Project is maintained by the SU2 Foundation 
 * (http://su2foundation.org)
 *
 * Copyright 2012-2019, SU2 Contributors (cf. AUTHORS.md)
 *
 * SU2 is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * SU2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with SU2. If not, see <http://www.gnu.org/licenses/>.
 */

#include "../../include/toolboxes/CLinearPartitioner.hpp"

CLinearPartitioner::CLinearPartitioner(unsigned long val_global_count,
                                       unsigned long val_offset,
                                       bool          isDisjoint) {
  
  /*--- Store MPI size ---*/
  
  size = SU2_MPI::GetSize();
  
  /*--- Resize the vectors containing our linear partitioning. ---*/
  
  firstIndex.resize(size);
  lastIndex.resize(size);
  sizeOnRank.resize(size);
  cumulativeSizeBeforeRank.resize(size+1);

  /*--- Compute the number of points that will be on each processor.
   This is a linear partitioning with the addition of a simple load
   balancing for any remainder points. ---*/
  
  unsigned long quotient = 0;
  if (val_global_count >= (unsigned long)size)
    quotient = val_global_count/size;
  
  int remainder = int(val_global_count%size);
  for (int ii = 0; ii < size; ii++) {
    sizeOnRank[ii] = quotient + int(ii < remainder);
  }
  
  /*--- Store the local number of nodes on each proc in the linear
   partitioning, the beginning/end index, and the linear partitioning
   within an array in cumulative storage format. ---*/
  
  unsigned long adjust = 0;
  if (isDisjoint) adjust = 1;
  
  firstIndex[0] = val_offset;
  lastIndex[0]  = firstIndex[0] + sizeOnRank[0] - adjust;
  cumulativeSizeBeforeRank[0] = 0;
  for (int iProc = 1; iProc < size; iProc++) {
    firstIndex[iProc] = lastIndex[iProc-1] + adjust;
    lastIndex[iProc]  = firstIndex[iProc]  + sizeOnRank[iProc] - adjust;
    cumulativeSizeBeforeRank[iProc] = (cumulativeSizeBeforeRank[iProc-1] +
                                       sizeOnRank[iProc-1]);
  }
  cumulativeSizeBeforeRank[size] = val_global_count;
  
}

CLinearPartitioner::~CLinearPartitioner(void) { }

unsigned long CLinearPartitioner::GetRankContainingIndex(unsigned long val_index) {

  /*--- Initial guess ---*/
  
  unsigned long iProcessor = val_index/sizeOnRank[0];
  
  /*--- Guard against going over size. ---*/
  
  if (iProcessor >= (unsigned long)size)
    iProcessor = (unsigned long)size-1;
  
  /*--- Move up or down until we find the processor. ---*/
  
  if (val_index >= cumulativeSizeBeforeRank[iProcessor])
    while(val_index >= cumulativeSizeBeforeRank[iProcessor+1])
      iProcessor++;
  else
    while(val_index < cumulativeSizeBeforeRank[iProcessor])
      iProcessor--;
  
  return iProcessor;
  
}
