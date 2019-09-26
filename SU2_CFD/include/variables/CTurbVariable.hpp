/*!
 * \file CTurbVariable.hpp
 * \brief Base class for defining the variables of the turbulence model.
 * \author F. Palacios, T. Economon
 * \version 6.2.0 "Falcon"
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

#include "CVariable.hpp"

/*!
 * \class CTurbVariable
 * \brief Base class for defining the variables of the turbulence model.
 * \ingroup Turbulence_Model
 * \author A. Bueno.
 */
class CTurbVariable : public CVariable {
protected:
  su2double muT;                /*!< \brief Eddy viscosity. */
  su2double *HB_Source;          /*!< \brief Harmonic Balance source term. */
  su2double **Gradient_Reconstruction;  /*!< \brief Gradient of the variables for MUSCL reconstruction for the convective term */
  bool GradReconAllocated;              /*!< \brief Flag indicating that separate memory was allocated for the MUSCL reconstruction gradient. */

public:
  /*!
   * \brief Constructor of the class.
   */
  CTurbVariable(void);

  /*!
   * \overload
   * \param[in] val_nDim - Number of dimensions of the problem.
   * \param[in] val_nvar - Number of variables of the problem.
   * \param[in] config - Definition of the particular problem.
   */
  CTurbVariable(unsigned short val_nDim, unsigned short val_nvar, CConfig *config);

  /*!
   * \brief Destructor of the class.
   */
  virtual ~CTurbVariable(void);

  /*!
   * \brief Get the value of the eddy viscosity.
   * \return the value of the eddy viscosity.
   */
  inline su2double GetmuT() { return muT; }

  /*!
   * \brief Set the value of the eddy viscosity.
   * \param[in] val_muT - Value of the eddy viscosity.
   */
  inline void SetmuT(su2double val_muT) { muT = val_muT; }
  
  /*!
   * \brief Get the value of the primitive gradient for MUSCL reconstruction.
   * \param[in] val_var - Index of the variable.
   * \param[in] val_dim - Index of the dimension.
   * \return Value of the primitive variables gradient.
   */
  inline su2double GetGradient_Reconstruction(unsigned short val_var, unsigned short val_dim) {return Gradient_Reconstruction[val_var][val_dim]; }
  
  /*!
   * \brief Get the value of the primitive gradient for MUSCL reconstruction.
   * \param[in] val_var - Index of the variable.
   * \param[in] val_dim - Index of the dimension.
   * \param[in] val_value - Value of the gradient.
   */
  inline void SetGradient_Reconstruction(unsigned short val_var, unsigned short val_dim, su2double val_value) {Gradient_Reconstruction[val_var][val_dim] = val_value; }
  
  /*!
   * \brief Get the value of the primitive gradient for MUSCL reconstruction.
   * \return Value of the primitive variables gradient.
   */
  inline su2double **GetGradient_Reconstruction(void) {return Gradient_Reconstruction; }
  
};

