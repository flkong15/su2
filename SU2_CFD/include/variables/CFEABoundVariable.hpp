/*!
 * \file CFEABoundVariable.hpp
 * \brief Class for defining the variables on the FEA boundaries for FSI applications.
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

#include "CFEAVariable.hpp"
#include <cassert>

/*!
 * \class CFEABoundVariable
 * \brief Class that adds storage of boundary variables (tractions) to CFEAVariable.
 * \note Member variables are allocated only for points marked as "vertex" i.e. on a boundary.
 * A map is constructed so that variables can be referenced by iPoint instead of iVertex.
 * \ingroup Structural Finite Element Analysis Variables
 * \author R. Sanchez.
 * \version 6.2.0 "Falcon"
 */
class CFEABoundVariable final : public CFEAVariable {
protected:

  Mat_t FlowTraction;         /*!< \brief Traction from the fluid field. */
  Mat_t FlowTraction_n;       /*!< \brief Traction from the fluid field at time n. */

  Mat_t Residual_Ext_Surf;    /*!< \brief Term of the residual due to external forces. */
  Mat_t Residual_Ext_Surf_n;  /*!< \brief Term of the residual due to external forces at time n. */

  TVec_t<unsigned> VertexMap; /*!< \brief Map from range 0-nPoint to 0-nBoundPt. */

  bool isAllocated;           /*!< \brief Set to true when it is safe to use the accessors. */

#define APPLY_VERTEXMAP(DEFAULT_RETURN) \
  assert(isAllocated && "Variable in invalid state, call AllocateBoundaryVariables before accessing data.");\
  iPoint = VertexMap(iPoint); if(iPoint==0) return DEFAULT_RETURN; else iPoint--; // decrement for 0 based

public:
  /*!
   * \brief Constructor of the class.
   * \param[in] val_fea - Values of the fea solution (initialization value).
   * \param[in] npoint - Number of points/nodes/vertices in the domain.
   * \param[in] ndim - Number of dimensions of the problem.
   * \param[in] nvar - Number of variables of the problem.
   * \param[in] config - Definition of the particular problem.
   */
  CFEABoundVariable(const su2double *val_fea, Idx_t npoint, Idx_t ndim, Idx_t nvar, CConfig *config);

  /*!
   * \brief Destructor of the class.
   */
  ~CFEABoundVariable() = default;

  /*!
   * \brief Allocate member variables for points marked as vertex (via "Set_isVertex").
   * \param[in] config - Definition of the particular problem.
   */
  void AllocateBoundaryVariables(CConfig *config);

  /*!
   * \brief Add surface load to the residual term
   */
  inline void Add_SurfaceLoad_Res(Idx_t iPoint, const su2double *val_surfForce) override {
    APPLY_VERTEXMAP()
    for (Idx_t iVar = 0; iVar < nVar; iVar++) Residual_Ext_Surf(iPoint,iVar) += val_surfForce[iVar];
  }

  /*!
   * \brief Set surface load of the residual term (for dampers - deletes all the other loads)
   */
  inline void Set_SurfaceLoad_Res(Idx_t iPoint, Idx_t iVar, su2double val_surfForce) override {
    APPLY_VERTEXMAP()
    Residual_Ext_Surf(iPoint,iVar) = val_surfForce;
  }

  /*!
   * \brief Get the residual term due to surface load
   */
  inline su2double Get_SurfaceLoad_Res(Idx_t iPoint, Idx_t iVar) const override {
    APPLY_VERTEXMAP(0.0)
    return Residual_Ext_Surf(iPoint,iVar);
  }

  /*!
   * \brief Clear the surface load residual
   */
  inline void Clear_SurfaceLoad_Res(Idx_t iPoint) override {
    APPLY_VERTEXMAP()
    for (Idx_t iVar = 0; iVar < nVar; iVar++) Residual_Ext_Surf(iPoint,iVar) = 0.0;
  }

  /*!
   * \brief Store the surface load as the load for the previous time step.
   */
  void Set_SurfaceLoad_Res_n() override;

  /*!
   * \brief Get the surface load from the previous time step.
   */
  inline su2double Get_SurfaceLoad_Res_n(Idx_t iPoint, Idx_t iVar) const override {
    APPLY_VERTEXMAP(0.0)
    return Residual_Ext_Surf_n(iPoint,iVar);
  }

  /*!
   * \brief Set the flow traction at a node on the structural side
   */
  inline void Set_FlowTraction(Idx_t iPoint, const su2double *val_flowTraction) override {
    APPLY_VERTEXMAP()
    for (Idx_t iVar = 0; iVar < nVar; iVar++) FlowTraction(iPoint,iVar) = val_flowTraction[iVar];
  }

  /*!
   * \brief Add a value to the flow traction at a node on the structural side
   */
  inline void Add_FlowTraction(Idx_t iPoint, const su2double *val_flowTraction) override {
    APPLY_VERTEXMAP()
    for (Idx_t iVar = 0; iVar < nVar; iVar++) FlowTraction(iPoint,iVar) += val_flowTraction[iVar];
  }

  /*!
   * \brief Get the residual term due to the flow traction
   */
  inline su2double Get_FlowTraction(Idx_t iPoint, Idx_t iVar) const override {
    APPLY_VERTEXMAP(0.0)
    return FlowTraction(iPoint,iVar);
  }

  /*!
   * \brief Set the value of the flow traction at the previous time step.
   */
  void Set_FlowTraction_n() override;

  /*!
   * \brief Retrieve the value of the flow traction from the previous time step.
   */
  inline su2double Get_FlowTraction_n(Idx_t iPoint, Idx_t iVar) const override {
    APPLY_VERTEXMAP(0.0)
    return FlowTraction_n(iPoint,iVar);
  }

  /*!
   * \brief Clear the flow traction residual
   */
  inline void Clear_FlowTraction(Idx_t iPoint) override {
    APPLY_VERTEXMAP()
    for (Idx_t iVar = 0; iVar < nVar; iVar++) FlowTraction(iPoint,iVar) = 0.0;
  }

  /*!
   * \brief Register the flow tractions as input variable.
   */
  inline void RegisterFlowTraction(Idx_t iPoint) override {
    for (unsigned short iVar = 0; iVar < nVar; iVar++)
      AD::RegisterInput(FlowTraction(iPoint,iVar));
  }

  /*!
   * \brief Extract the flow traction derivatives.
   */
  inline su2double ExtractFlowTraction_Sensitivity(Idx_t iPoint, Idx_t iDim) const override {
    return SU2_TYPE::GetDerivative(FlowTraction(iPoint,iDim));
  }

  /*!
   * \brief Get whether a node is on the boundary
   */
  inline bool Get_isVertex(Idx_t iPoint) const override { return VertexMap(iPoint)!=0; }

  /*!
   * \brief Set whether a node is on the boundary
   */
  inline void Set_isVertex(Idx_t iPoint, bool isVertex) override {
    /*--- Invalidate allocation if change is requested as that destroys map. ---*/
    if (isVertex != bool(VertexMap(iPoint))) {
      isAllocated = false;
      VertexMap(iPoint) = unsigned(isVertex);
    }
  }

#undef APPLY_VERTEXMAP
};
