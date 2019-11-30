/*!
 * \file matrix_structure.inl
 * \brief In-Line subroutines of the <i>matrix_structure.hpp</i> file.
 * \note These are the "private" inlines, they are not needed outside of
 * the .cpp file and so they are hidden to avoid triggering recompilation
 * of other units when changes are made here.
 *
 * \author F. Palacios, A. Bueno, T. Economon
 * \version 7.0.0 "Blackbird"
 *
 * The current SU2 release has been coordinated by the
 * SU2 International Developers Society <www.su2devsociety.org>
 * with selected contributions from the open-source community.
 *
 * The main research teams contributing to the current release are:
 *  - Prof. Juan J. Alonso's group at Stanford University.
 *  - Prof. Piero Colonna's group at Delft University of Technology.
 *  - Prof. Nicolas R. Gauger's group at Kaiserslautern University of Technology.
 *  - Prof. Alberto Guardone's group at Polytechnic University of Milan.
 *  - Prof. Rafael Palacios' group at Imperial College London.
 *  - Prof. Vincent Terrapon's group at the University of Liege.
 *  - Prof. Edwin van der Weide's group at the University of Twente.
 *  - Lab. of New Concepts in Aeronautics at Tech. Institute of Aeronautics.
 *
 * Copyright 2012-2019, Francisco D. Palacios, Thomas D. Economon,
 *                      Tim Albring, and the SU2 contributors.
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

#pragma once

#include "CSysMatrix.hpp"

#if defined(_MSC_VER)
  #define FORCEINLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__) || defined(__INTEL_COMPILER)
  #define FORCEINLINE inline __attribute__((always_inline))
#else
  #define FORCEINLINE inline
#endif

template<class ScalarType>
FORCEINLINE ScalarType *CSysMatrix<ScalarType>::GetBlock_ILUMatrix(unsigned long block_i, unsigned long block_j) {

  for (unsigned long index = row_ptr_ilu[block_i]; index < row_ptr_ilu[block_i+1]; index++)
    if (col_ind_ilu[index] == block_j)
      return &ILU_matrix[index*nVar*nEqn];

  return NULL;
}

template<class ScalarType>
FORCEINLINE void CSysMatrix<ScalarType>::SetBlock_ILUMatrix(unsigned long block_i, unsigned long block_j, ScalarType *val_block) {

  unsigned long iVar, index;

  for (index = row_ptr_ilu[block_i]; index < row_ptr_ilu[block_i+1]; index++) {
    if (col_ind_ilu[index] == block_j) {
      for (iVar = 0; iVar < nVar*nEqn; iVar++)
        ILU_matrix[index*nVar*nEqn+iVar] = val_block[iVar];
      break;
    }
  }
}

template<class ScalarType>
FORCEINLINE void CSysMatrix<ScalarType>::SetBlockTransposed_ILUMatrix(unsigned long block_i, unsigned long block_j, ScalarType *val_block) {

  unsigned long iVar, jVar, index;

  for (index = row_ptr_ilu[block_i]; index < row_ptr_ilu[block_i+1]; index++) {
    if (col_ind_ilu[index] == block_j) {
      for (iVar = 0; iVar < nVar; iVar++)
        for (jVar = 0; jVar < nEqn; jVar++)
          ILU_matrix[index*nVar*nEqn+iVar*nEqn+jVar] = val_block[jVar*nVar+iVar];
      break;
    }
  }
}

template<class T, bool alpha, bool beta, bool transp>
FORCEINLINE void gemv_impl(const unsigned long n, const T *a, const T *b, T *c) {
  /*---
   This is a templated version of GEMV with the constants as boolean
   template parameters so that they can be optimized away at compilation.
   This is still the traditional "row dot vector" method.
  ---*/
  unsigned long i, j;
  for (i = 0; i < n; i++) {
    if (!beta) c[i] = 0.0;
    for (j = 0; j < n; j++) {
      if (alpha) c[transp? j:i] += a[i*n+j] * b[transp? i:j];
      else       c[transp? j:i] -= a[i*n+j] * b[transp? i:j];
    }
  }
}

template<class T>
FORCEINLINE void gemm_impl(const unsigned long n, const T *a, const T *b, T *c) {
  /*--- Same deal as for GEMV but here only the type is templated. ---*/
  unsigned long i, j, k;
  for (i = 0; i < n; i++) {
    for (j = 0; j < n; j++) {
      c[i*n+j] = 0.0;
      for (k = 0; k < n; k++)
        c[i*n+j] += a[i*n+k] * b[k*n+j];
    }
  }
}

#define __MATVECPROD_SIGNATURE__(TYPE,NAME) \
FORCEINLINE void CSysMatrix<TYPE>::NAME(const TYPE *matrix, const TYPE *vector, TYPE *product)

#define MATVECPROD_SIGNATURE(NAME) template<class ScalarType> __MATVECPROD_SIGNATURE__(ScalarType,NAME)

#if !defined(USE_MKL)
MATVECPROD_SIGNATURE( MatrixVectorProduct ) {
  /*---
   Without MKL (default) picture copying the body of gemv_impl
   here and resolving the conditionals at compilation.
  ---*/
  gemv_impl<ScalarType,true,false,false>(nVar, matrix, vector, product);
}

MATVECPROD_SIGNATURE( MatrixVectorProductAdd ) {
  gemv_impl<ScalarType,true,true,false>(nVar, matrix, vector, product);
}

MATVECPROD_SIGNATURE( MatrixVectorProductSub ) {
  gemv_impl<ScalarType,false,true,false>(nVar, matrix, vector, product);
}

MATVECPROD_SIGNATURE( MatrixVectorProductTransp ) {
  gemv_impl<ScalarType,true,true,true>(nVar, matrix, vector, product);
}

template<class ScalarType>
FORCEINLINE void CSysMatrix<ScalarType>::MatrixMatrixProduct(const ScalarType *matrix_a, const ScalarType *matrix_b, ScalarType *product) {
  gemm_impl<ScalarType>(nVar, matrix_a, matrix_b, product);
}
#else
MATVECPROD_SIGNATURE( MatrixVectorProduct ) {
  /*--- With MKL we use the just-in-time kernels instead of the naive implementation. ---*/
  MatrixVectorProductKernelBetaZero(MatrixVectorProductJitterBetaZero, const_cast<ScalarType*>(vector),
                                    const_cast<ScalarType*>(matrix), product );
}

MATVECPROD_SIGNATURE( MatrixVectorProductAdd ) {
  MatrixVectorProductKernelBetaOne(MatrixVectorProductJitterBetaOne, const_cast<ScalarType*>(vector),
                                   const_cast<ScalarType*>(matrix), product );
}

MATVECPROD_SIGNATURE( MatrixVectorProductSub ) {
  MatrixVectorProductKernelAlphaMinusOne(MatrixVectorProductJitterAlphaMinusOne, const_cast<ScalarType*>(vector),
                                         const_cast<ScalarType*>(matrix), product );
}

MATVECPROD_SIGNATURE( MatrixVectorProductTransp ) {
  MatrixVectorProductTranspKernelBetaOne(MatrixVectorProductTranspJitterBetaOne, const_cast<ScalarType*>(matrix),
                                         const_cast<ScalarType*>(vector), product );
}

template<class ScalarType>
FORCEINLINE void CSysMatrix<ScalarType>::MatrixMatrixProduct(const ScalarType *matrix_a, const ScalarType *matrix_b, ScalarType *product) {
  MatrixMatrixProductKernel(MatrixMatrixProductJitter, const_cast<ScalarType*>(matrix_a),
                            const_cast<ScalarType*>(matrix_b), product );
}
#ifdef CODI_REVERSE_TYPE
/*--- WHEN using MKL, AND compiling for AD, we need to specialize for su2double to avoid mixing incompatible types. ---*/
#define MATVECPROD_SPECIALIZATION(NAME) template<> __MATVECPROD_SIGNATURE__(su2double,NAME)
MATVECPROD_SPECIALIZATION( MatrixVectorProduct ) {
  gemv_impl<su2double,true,false,false>(nVar, matrix, vector, product);
}

MATVECPROD_SPECIALIZATION( MatrixVectorProductAdd ) {
  gemv_impl<su2double,true,true,false>(nVar, matrix, vector, product);
}

MATVECPROD_SPECIALIZATION( MatrixVectorProductSub ) {
  gemv_impl<su2double,false,true,false>(nVar, matrix, vector, product);
}

MATVECPROD_SPECIALIZATION( MatrixVectorProductTransp ) {
  gemv_impl<su2double,true,true,true>(nVar, matrix, vector, product);
}

template<>
FORCEINLINE void CSysMatrix<su2double>::MatrixMatrixProduct(const su2double *matrix_a, const su2double *matrix_b, su2double *product) {
  gemm_impl<su2double>(nVar, matrix_a, matrix_b, product);
}
#undef MATVECPROD_SPECIALIZATION
#endif // CODI_REVERSE_TYPE
#endif // USE_MKL

#undef MATVECPROD_SIGNATURE
#undef __MATVECPROD_SIGNATURE__

template<class ScalarType>
FORCEINLINE void CSysMatrix<ScalarType>::Gauss_Elimination(unsigned long block_i, ScalarType* rhs, bool transposed) {

  unsigned long iVar, jVar;
  const ScalarType *Block = &matrix[dia_ptr[block_i]*nVar*nVar];

  /*--- Copy block, as the algorithm modifies the matrix ---*/

  if (!transposed) {
    // If source and dest overlap higher level problems occur, so memcpy is safe. And it is faster.
    memcpy( block, Block, nVar*nVar*sizeof(ScalarType) );

//    for (iVar = 0; iVar < nVar*nVar; iVar++)
//       block[iVar] = Block[iVar];

  } else {
    for (iVar = 0; iVar < nVar; iVar++)
      for (jVar = 0; jVar < nVar; jVar++)
        block[iVar*nVar+jVar] = Block[jVar*nVar+iVar];
  }

  /*--- Solve system ---*/

  Gauss_Elimination(block, rhs);

}

template<class ScalarType>
FORCEINLINE void CSysMatrix<ScalarType>::InverseDiagonalBlock(unsigned long block_i, ScalarType *invBlock, bool transpose) {

  const ScalarType* mat = &matrix[dia_ptr[block_i]*nVar*nVar];
  MatrixInverse(mat, invBlock);

  if (transpose) // swap off-diag
    for (unsigned long iVar = 0; iVar < nVar-1; ++iVar)
      for (unsigned long jVar = iVar+1; jVar < nVar; ++jVar) {
        ScalarType tmp = invBlock[iVar*nVar+jVar];
        invBlock[iVar*nVar+jVar] = invBlock[jVar*nVar+iVar];
        invBlock[jVar*nVar+iVar] = tmp;
      }
}

template<class ScalarType>
FORCEINLINE void CSysMatrix<ScalarType>::InverseDiagonalBlock_ILUMatrix(unsigned long block_i, ScalarType *invBlock) {

  const ScalarType* mat = &ILU_matrix[dia_ptr_ilu[block_i]*nVar*nVar];
  MatrixInverse(mat, invBlock);
}

template<class ScalarType>
FORCEINLINE void CSysMatrix<ScalarType>::UpperProduct(const CSysVector<ScalarType> & vec, unsigned long row_i) {

  unsigned long iVar, index, col_j;

  for (iVar = 0; iVar < nVar; iVar++)
    prod_row_vector[iVar] = 0.0;

  for (index = dia_ptr[row_i]+1; index < row_ptr[row_i+1]; index++) {
    col_j = col_ind[index];
    MatrixVectorProductAdd(&matrix[index*nVar*nVar], &vec[col_j*nVar], prod_row_vector);
  }
}

template<class ScalarType>
FORCEINLINE void CSysMatrix<ScalarType>::LowerProduct(const CSysVector<ScalarType> & vec, unsigned long row_i) {

  unsigned long iVar, index, col_j;

  for (iVar = 0; iVar < nVar; iVar++)
    prod_row_vector[iVar] = 0.0;

  for (index = row_ptr[row_i]; index < dia_ptr[row_i]; index++) {
    col_j = col_ind[index];
    MatrixVectorProductAdd(&matrix[index*nVar*nVar], &vec[col_j*nVar], prod_row_vector);
  }
}

template<class ScalarType>
FORCEINLINE void CSysMatrix<ScalarType>::DiagonalProduct(const CSysVector<ScalarType> & vec, unsigned long row_i) {

  MatrixVectorProduct(&matrix[dia_ptr[row_i]*nVar*nVar], &vec[row_i*nVar], prod_row_vector);
}
