/*!
 * \file COneShotSolver.cpp
 * \brief Main subroutines for solving the one-shot problem.
 * \author T.Dick
 * \version 7.1.1 "Blackbird"
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

#include "../../include/solvers/COneShotSolver.hpp"
#include "../../include/variables/CDiscAdjVariable.hpp"

COneShotSolver::COneShotSolver(void) : CDiscAdjSolver () {

}

COneShotSolver::COneShotSolver(CGeometry *geometry, CConfig *config)  : CDiscAdjSolver(geometry, config) {

}

COneShotSolver::COneShotSolver(CGeometry *geometry, CConfig *config, CSolver *direct_solver, unsigned short Kind_Solver, unsigned short iMesh)  : CDiscAdjSolver(geometry, config, direct_solver, Kind_Solver, iMesh) {

}

COneShotSolver::~COneShotSolver(void) {

}

void COneShotSolver::SetRecording(CGeometry* geometry, CConfig *config){


  bool time_n1_needed = config->GetTime_Marching() == DT_STEPPING_2ND;
  bool time_n_needed = (config->GetTime_Marching() == DT_STEPPING_1ST) || time_n1_needed;

  unsigned long iPoint;
  unsigned short iVar;

  /*--- For the one-shot solver the solution is not reset in each iteration step to the initial solution ---*/

  if (time_n_needed) {
    for (iPoint = 0; iPoint < nPoint; iPoint++) {
      for (iVar = 0; iVar < nVar; iVar++) {
        AD::ResetInput(direct_solver->GetNodes()->GetSolution_time_n(iPoint)[iVar]);
      }
    }
  }
  if (time_n1_needed) {
    for (iPoint = 0; iPoint < nPoint; iPoint++) {
      for (iVar = 0; iVar < nVar; iVar++) {
        AD::ResetInput(direct_solver->GetNodes()->GetSolution_time_n1(iPoint)[iVar]);
      }
    }
  }

  /*--- Set the Jacobian to zero since this is not done inside the fluid iteration
   * when running the discrete adjoint solver. ---*/

  direct_solver->Jacobian.SetValZero();

  /*--- Set indices to zero ---*/

  RegisterVariables(geometry, config, true);

}


void COneShotSolver::StoreMeshPoints(CGeometry *geometry, CConfig *config){
    unsigned long iVertex;
    unsigned short iMarker;

    geometry->nodes->SetCoord_Old();

    for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++) {
        for (iVertex = 0; iVertex < geometry->nVertex[iMarker]; iVertex++) {
          geometry->vertex[iMarker][iVertex]->SetNormal_Old(geometry->vertex[iMarker][iVertex]->GetNormal());
        }
    }
}


void COneShotSolver::LoadMeshPoints(CGeometry *geometry, CConfig *config){
    unsigned long iVertex, iPoint;
    unsigned short iMarker;

    geometry->nodes->GetCoord_Old();

    for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++) {
        for (iVertex = 0; iVertex < geometry->nVertex[iMarker]; iVertex++) {
          iPoint = geometry->vertex[iMarker][iVertex]->GetNode();
          geometry->vertex[iMarker][iVertex]->SetNormal(geometry->vertex[iMarker][iVertex]->GetNormal_Old());
        }
    }
}


void  COneShotSolver::UpdateAuxiliaryGeometryVariables(CGeometry **geometry_container, CVolumetricMovement *grid_movement, CConfig *config) {

  su2double MinVolume, MaxVolume;

  /*--- Communicate the updated mesh coordinates. ---*/

  geometry_container[MESH_0]->InitiateComms(geometry_container[MESH_0], config, COORDINATES);
  geometry_container[MESH_0]->CompleteComms(geometry_container[MESH_0], config, COORDINATES);

  /*--- After moving all nodes, update the dual mesh. Recompute the edges and
   dual mesh control volumes in the domain and on the boundaries. ---*/

  grid_movement->UpdateDualGrid(geometry_container[MESH_0], config);

  /*--- Update the multigrid structure after moving the finest grid,
   including computing the grid velocities on the coarser levels
   when the problem is solved in unsteady conditions. ---*/

  grid_movement->UpdateMultiGrid(geometry_container, config);

  /*--- do we need to update the elemnt volumes, etc.??? ---*/

  grid_movement->ComputeDeforming_Element_Volume(geometry_container[MESH_0], MinVolume, MaxVolume, true);
  grid_movement->ComputenNonconvexElements(geometry_container[MESH_0], true);

  if (rank == MASTER_NODE) {
    cout << "Resetting mesh coordinates for linesearch: " << endl;
    cout << "Min. volume: " << MinVolume << ", Max. volume: " << MaxVolume << "." << endl;
  }

}
