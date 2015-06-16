/*!
 * \file iteration_structure_fsi.cpp
 * \brief Main subroutines used by SU2_FSI for iteration
 * \author R. Sanchez
 * \version 3.2.9 "eagle"
 *
 * SU2 Lead Developers: Dr. Francisco Palacios (Francisco.D.Palacios@boeing.com).
 *                      Dr. Thomas D. Economon (economon@stanford.edu).
 *
 * SU2 Developers: Prof. Juan J. Alonso's group at Stanford University.
 *                 Prof. Piero Colonna's group at Delft University of Technology.
 *                 Prof. Nicolas R. Gauger's group at Kaiserslautern University of Technology.
 *                 Prof. Alberto Guardone's group at Polytechnic University of Milan.
 *                 Prof. Rafael Palacios' group at Imperial College London.
 *
 * Copyright (C) 2012-2015 SU2, the open-source CFD code.
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

#include "../include/iteration_structure_fsi.hpp"

void FSI_BGS_Iteration(COutput *output, CIntegration ***integration_container, CGeometry ***geometry_container,
                             CSolver ****solver_container, CNumerics *****numerics_container, CConfig **config_container,
                             CSurfaceMovement **surface_movement, CVolumetricMovement **grid_movement, CFreeFormDefBox*** FFDBox,
                             unsigned long iFluidIt, unsigned long nFluidIt) {

	double Physical_dt, Physical_t;
	unsigned short iMesh;
	unsigned long IntIter = 0; config_container[ZONE_0]->SetIntIter(IntIter);
	unsigned long IntIter_Struct = 0; config_container[ZONE_1]->SetIntIter(IntIter_Struct);
	unsigned long iFSIIter = 0;
	unsigned long nFSIIter = config_container[ZONE_0]->GetnIterFSI();
	unsigned long ExtIter = config_container[ZONE_0]->GetExtIter();

	/*-----------------------------------------------------------------*/
	/*---------------- Predict structural displacements ---------------*/
	/*-----------------------------------------------------------------*/

	FSI_Disp_Predictor(output, integration_container, geometry_container,
                          solver_container, numerics_container, config_container,
                          surface_movement, grid_movement, FFDBox);

	while (iFSIIter<nFSIIter){

		/*-----------------------------------------------------------------*/
		/*------------------------ Update mesh ----------------------------*/
		/*-----------------------------------------------------------------*/

        FSI_Disp_Transfer(output, integration_container, geometry_container,
                          solver_container, numerics_container, config_container,
                          surface_movement, grid_movement, FFDBox);

		/*-----------------------------------------------------------------*/
		/*-------------------- Fluid subiteration -------------------------*/
		/*-----------------------------------------------------------------*/

        Flow_Subiteration(output, integration_container, geometry_container,
                          solver_container, numerics_container, config_container,
                          surface_movement, grid_movement, FFDBox);

		/*-----------------------------------------------------------------*/
		/*------------------- Set FEA loads from fluid --------------------*/
		/*-----------------------------------------------------------------*/

        FSI_Load_Transfer(output, integration_container, geometry_container,
	                 	 solver_container, numerics_container, config_container,
	                 	 surface_movement, grid_movement, FFDBox, ExtIter);


		/*-----------------------------------------------------------------*/
		/*------------------ Structural subiteration ----------------------*/
		/*-----------------------------------------------------------------*/

	    FEA_Subiteration(output, integration_container, geometry_container,
	                 	 solver_container, numerics_container, config_container,
	                 	 surface_movement, grid_movement, FFDBox);


		/*-----------------------------------------------------------------*/
		/*----------------- Displacements relaxation ----------------------*/
		/*-----------------------------------------------------------------*/

	    FSI_Disp_Relaxation(output, geometry_container, solver_container, config_container, iFSIIter);

		/*-----------------------------------------------------------------*/
		/*-------------------- Check convergence --------------------------*/
		/*-----------------------------------------------------------------*/

		integration_container[ZONE_1][FEA_SOL]->Convergence_Monitoring_FSI(geometry_container[ZONE_1][MESH_0], config_container[ZONE_1],
																solver_container[ZONE_1][MESH_0][FEA_SOL], iFSIIter);

		if (integration_container[ZONE_1][FEA_SOL]->GetConvergence_FSI()) break;

		/*-----------------------------------------------------------------*/
		/*--------------------- Update iFSIIter ---------------------------*/
		/*-----------------------------------------------------------------*/

		iFSIIter++;

	}

	/*-----------------------------------------------------------------*/
  	/*-------------------- Update fluid solver ------------------------*/
	/*-----------------------------------------------------------------*/

    Flow_Update(output, integration_container, geometry_container,
                solver_container, numerics_container, config_container,
                surface_movement, grid_movement, FFDBox, ExtIter);

	/*-----------------------------------------------------------------*/
  	/*----------------- Update structural solver ----------------------*/
	/*-----------------------------------------------------------------*/

    FEA_Update(output, integration_container, geometry_container,
                solver_container, config_container, ExtIter);


	/*-----------------------------------------------------------------*/
	/*--------------- Update convergence parameter --------------------*/
	/*-----------------------------------------------------------------*/

	integration_container[ZONE_1][FEA_SOL]->SetConvergence_FSI(false);


}

void Flow_Subiteration(COutput *output, CIntegration ***integration_container, CGeometry ***geometry_container,
                       CSolver ****solver_container, CNumerics *****numerics_container, CConfig **config_container,
                       CSurfaceMovement **surface_movement, CVolumetricMovement **grid_movement, CFreeFormDefBox*** FFDBox) {

	unsigned long IntIter = 0; config_container[ZONE_0]->SetIntIter(IntIter);
	unsigned long ExtIter = config_container[ZONE_0]->GetExtIter();

	unsigned short iMesh, iZone;

	/*--- Only one zone allowed for the fluid as for now ---*/
	unsigned short nFluidZone = 1;

	#ifdef HAVE_MPI
		int rank;
			MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	#endif

	/*--- Set the initial condition ---*/
	for (iZone = 0; iZone < nFluidZone; iZone++)
		solver_container[iZone][MESH_0][FLOW_SOL]->SetInitialCondition(geometry_container[iZone], solver_container[iZone], config_container[iZone], ExtIter);

	/*--- Apply a Wind Gust ---*/
	/*--- Initial set up for unsteady problems with gusts - Not enabled yet. ---*/

//	for (iZone = 0; iZone < nFluidZone; iZone++) {
//		if (config_container[ZONE_0]->GetWind_Gust()){
//			  SetWind_GustField(config_container[iZone],geometry_container[iZone],solver_container[iZone]);
//		}
//	}

	for (iZone = 0; iZone < nFluidZone; iZone++) {

		/*--- Set the value of the internal iteration ---*/

		IntIter = ExtIter;

		if ((config_container[iZone]->GetUnsteady_Simulation() == DT_STEPPING_1ST) ||
		   (config_container[iZone]->GetUnsteady_Simulation() == DT_STEPPING_2ND)) IntIter = 0;

		/*--- Update global parameters ---*/

		if (config_container[iZone]->GetKind_Solver() == EULER){
			config_container[iZone]->SetGlobalParam(EULER, RUNTIME_FLOW_SYS, ExtIter);
		}
		if (config_container[iZone]->GetKind_Solver() == NAVIER_STOKES){
			config_container[iZone]->SetGlobalParam(NAVIER_STOKES, RUNTIME_FLOW_SYS, ExtIter);
		}
		if (config_container[iZone]->GetKind_Solver() == RANS){
			config_container[iZone]->SetGlobalParam(RANS, RUNTIME_FLOW_SYS, ExtIter);
		 }

		/*--- Solve the Euler, Navier-Stokes or Reynolds-averaged Navier-Stokes (RANS) equations (one iteration) ---*/

		integration_container[iZone][FLOW_SOL]->MultiGrid_Iteration(geometry_container, solver_container, numerics_container,
																	config_container, RUNTIME_FLOW_SYS, IntIter, iZone);

		if (config_container[iZone]->GetKind_Solver() == RANS) {

			/*--- Solve the turbulence model ---*/

			config_container[iZone]->SetGlobalParam(RANS, RUNTIME_TURB_SYS, ExtIter);
			integration_container[iZone][TURB_SOL]->SingleGrid_Iteration(geometry_container, solver_container, numerics_container,
																		   config_container, RUNTIME_TURB_SYS, IntIter, iZone);

		/*--- Solve transition model ---*/

			if (config_container[iZone]->GetKind_Trans_Model() == LM) {
				config_container[iZone]->SetGlobalParam(RANS, RUNTIME_TRANS_SYS, ExtIter);
				integration_container[iZone][TRANS_SOL]->SingleGrid_Iteration(geometry_container, solver_container, numerics_container,
																			  config_container, RUNTIME_TRANS_SYS, IntIter, iZone);
			}

		}

	}

	/*--- Dual time stepping strategy ---*/

	if ((config_container[ZONE_0]->GetUnsteady_Simulation() == DT_STEPPING_1ST) ||
		   (config_container[ZONE_0]->GetUnsteady_Simulation() == DT_STEPPING_2ND)) {

			for(IntIter = 1; IntIter < config_container[ZONE_0]->GetUnst_nIntIter(); IntIter++) {

			/*--- Write the convergence history (only screen output) ---*/

			output->SetConvHistory_Body(NULL, geometry_container, solver_container, config_container, integration_container, true, 0.0, ZONE_0);

			/*--- Set the value of the internal iteration ---*/

			config_container[ZONE_0]->SetIntIter(IntIter);

			for (iZone = 0; iZone < nFluidZone; iZone++) {

				/*--- Pseudo-timestepping for the Euler, Navier-Stokes or Reynolds-averaged Navier-Stokes equations ---*/

				if (config_container[iZone]->GetKind_Solver() == EULER)
					config_container[iZone]->SetGlobalParam(EULER, RUNTIME_FLOW_SYS, ExtIter);
				if (config_container[iZone]->GetKind_Solver() == NAVIER_STOKES)
					config_container[iZone]->SetGlobalParam(NAVIER_STOKES, RUNTIME_FLOW_SYS, ExtIter);
				if (config_container[iZone]->GetKind_Solver() == RANS)
					config_container[iZone]->SetGlobalParam(RANS, RUNTIME_FLOW_SYS, ExtIter);

				  /*--- Solve the Euler, Navier-Stokes or Reynolds-averaged Navier-Stokes (RANS) equations (one iteration) ---*/

				integration_container[iZone][FLOW_SOL]->MultiGrid_Iteration(geometry_container, solver_container, numerics_container,
																				config_container, RUNTIME_FLOW_SYS, IntIter, iZone);

				/*--- Pseudo-timestepping the turbulence model ---*/

				if (config_container[iZone]->GetKind_Solver() == RANS) {

					/*--- Solve the turbulence model ---*/

					config_container[iZone]->SetGlobalParam(RANS, RUNTIME_TURB_SYS, ExtIter);
					integration_container[iZone][TURB_SOL]->SingleGrid_Iteration(geometry_container, solver_container, numerics_container,
																				  config_container, RUNTIME_TURB_SYS, IntIter, iZone);

					/*--- Solve transition model ---*/

						if (config_container[iZone]->GetKind_Trans_Model() == LM) {
							config_container[iZone]->SetGlobalParam(RANS, RUNTIME_TRANS_SYS, ExtIter);
							integration_container[iZone][TRANS_SOL]->SingleGrid_Iteration(geometry_container, solver_container, numerics_container,
																					  config_container, RUNTIME_TRANS_SYS, IntIter, iZone);
						}
				}
			}
			if (integration_container[ZONE_0][FLOW_SOL]->GetConvergence()) break;
		}
	}
}


void Flow_Update(COutput *output, CIntegration ***integration_container, CGeometry ***geometry_container,
                       CSolver ****solver_container, CNumerics *****numerics_container, CConfig **config_container,
                       CSurfaceMovement **surface_movement, CVolumetricMovement **grid_movement, CFreeFormDefBox*** FFDBox,
                       unsigned long ExtIter) {

	double Physical_dt, Physical_t;
	unsigned short iMesh, iZone;

	/*--- Only one zone allowed for the fluid as for now ---*/
	unsigned short nFluidZone = 1;

	for (iZone = 0; iZone < nFluidZone; iZone++) {

		if ((config_container[iZone]->GetUnsteady_Simulation() == DT_STEPPING_1ST) ||
		   (config_container[iZone]->GetUnsteady_Simulation() == DT_STEPPING_2ND)) {

			/*--- Update dual time solver on all mesh levels ---*/

			for (iMesh = 0; iMesh <= config_container[iZone]->GetnMGLevels(); iMesh++) {
				integration_container[iZone][FLOW_SOL]->SetDualTime_Solver(geometry_container[iZone][iMesh], solver_container[iZone][iMesh][FLOW_SOL], config_container[iZone], iMesh);
				integration_container[iZone][FLOW_SOL]->SetConvergence(false);
			}

			/*--- Update dual time solver for the turbulence model ---*/

			if (config_container[iZone]->GetKind_Solver() == RANS) {
				integration_container[iZone][TURB_SOL]->SetDualTime_Solver(geometry_container[iZone][MESH_0], solver_container[iZone][MESH_0][TURB_SOL], config_container[iZone], MESH_0);
				integration_container[iZone][TURB_SOL]->SetConvergence(false);
			}

			/*--- Update dual time solver for the transition model ---*/

			if (config_container[iZone]->GetKind_Trans_Model() == LM) {
				integration_container[iZone][TRANS_SOL]->SetDualTime_Solver(geometry_container[iZone][MESH_0], solver_container[iZone][MESH_0][TRANS_SOL], config_container[iZone], MESH_0);
				integration_container[iZone][TRANS_SOL]->SetConvergence(false);
			}

			/*--- Verify convergence criteria (based on total time) ---*/

			Physical_dt = config_container[iZone]->GetDelta_UnstTime();
			Physical_t  = (ExtIter+1)*Physical_dt;
			if (Physical_t >=  config_container[iZone]->GetTotal_UnstTime())
				integration_container[iZone][FLOW_SOL]->SetConvergence(true);

		}

	}


}


void FEA_Update(COutput *output, CIntegration ***integration_container, CGeometry ***geometry_container,
                  CSolver ****solver_container, CConfig **config_container,
                  unsigned long ExtIter) {

	/*--- Only one zone allowed for the fluid as for now ---*/
	unsigned short nFluidZone = 1, nStrucZone=1, nTotalZone;
	unsigned short iZone;

	nTotalZone = nFluidZone + nStrucZone;

	for (iZone = nFluidZone; iZone < nTotalZone; iZone++) {

		/*----------------- Update structural solver ----------------------*/

		integration_container[iZone][FEA_SOL]->SetStructural_Solver(geometry_container[iZone][MESH_0],
												solver_container[iZone][MESH_0][FEA_SOL], config_container[iZone], MESH_0);

	}


}


void FEA_Subiteration(COutput *output, CIntegration ***integration_container, CGeometry ***geometry_container,
                  CSolver ****solver_container, CNumerics *****numerics_container, CConfig **config_container,
                  CSurfaceMovement **surface_movement, CVolumetricMovement **grid_movement, CFreeFormDefBox*** FFDBox) {
	double Physical_dt, Physical_t;
	unsigned short iMesh, iZone;
	unsigned short nZone = geometry_container[ZONE_0][MESH_0]->GetnZone();
	unsigned long IntIter_Struct = 0; //config_container[ZONE_0]->SetIntIter(IntIter);
  	unsigned long ExtIter = config_container[ZONE_0]->GetExtIter();

	/*--- Only one zone allowed for the fluid as for now ---*/
	unsigned short nFluidZone = 1, nStrucZone=1, nTotalZone;

	nTotalZone = nFluidZone + nStrucZone;

	for (iZone = nFluidZone; iZone < nTotalZone; iZone++) {

		/*--- Set the initial condition at the first iteration ---*/

	//	solver_container[ZONE_1][MESH_0][FEA_SOL]->SetInitialCondition(geometry_container[ZONE_1], solver_container[ZONE_1], config_container[ZONE_1], ExtIter);

		/*--- Set the value of the internal iteration ---*/

		IntIter_Struct = ExtIter;

		/*--- FEA equations ---*/

		config_container[iZone]->SetGlobalParam(LINEAR_ELASTICITY, RUNTIME_FEA_SYS, ExtIter);

		/*--- Run the iteration ---*/

		integration_container[iZone][FEA_SOL]->Structural_Iteration(geometry_container, solver_container, numerics_container,
																	 config_container, RUNTIME_FEA_SYS, IntIter_Struct, iZone);

	}

}


void FSI_Disp_Transfer(COutput *output, CIntegration ***integration_container, CGeometry ***geometry_container,
					     CSolver ****solver_container, CNumerics *****numerics_container, CConfig **config_container,
						 CSurfaceMovement **surface_movement, CVolumetricMovement **grid_movement, CFreeFormDefBox*** FFDBox){

	bool MatchingMesh = config_container[ZONE_0]->GetMatchingMesh();

	/*--- Displacement transfer --  This will have to be modified for non-matching meshes ---*/

	if (MatchingMesh){
		solver_container[ZONE_0][MESH_0][FLOW_SOL]->SetFlow_Displacement(geometry_container[ZONE_0], grid_movement[ZONE_0],
																   	   config_container[ZONE_0], config_container[ZONE_1],
																   	   geometry_container[ZONE_1], solver_container[ZONE_1]);
	}
	else{
		cout << "NonMatchingMesh" << endl;
	}


}


void FSI_Load_Transfer(COutput *output, CIntegration ***integration_container, CGeometry ***geometry_container,
					     CSolver ****solver_container, CNumerics *****numerics_container, CConfig **config_container,
						 CSurfaceMovement **surface_movement, CVolumetricMovement **grid_movement, CFreeFormDefBox*** FFDBox,
						 unsigned long ExtIter){

	bool MatchingMesh = config_container[ZONE_0]->GetMatchingMesh();

	/*--- Load transfer --  This will have to be modified for non-matching meshes ---*/

	unsigned short SolContainer_Position_fea = config_container[ZONE_1]->GetContainerPosition(RUNTIME_FEA_SYS);

	/*--- FEA equations -- Necessary as the SetFEA_Load routine is as of now contained in the structural solver ---*/

	config_container[ZONE_1]->SetGlobalParam(LINEAR_ELASTICITY, RUNTIME_FEA_SYS, ExtIter);

	if (MatchingMesh){

		solver_container[ZONE_1][MESH_0][FEA_SOL]->SetFEA_Load(solver_container[ZONE_0], geometry_container[ZONE_1], geometry_container[ZONE_0],
															   config_container[ZONE_1], config_container[ZONE_0], numerics_container[ZONE_1][MESH_0][SolContainer_Position_fea][VISC_TERM]);

	}
	else{
		cout << "NonMatchingMesh" << endl;
	}


}


void FSI_Disp_Relaxation(COutput *output, CGeometry ***geometry_container, CSolver ****solver_container,
							CConfig **config_container, unsigned long iFSIIter) {

	/*-------------------- Aitken's relaxation ------------------------*/

	/*--- Only one zone allowed for the fluid as for now ---*/
	unsigned short nFluidZone = 1, nStrucZone=1, nTotalZone;
	unsigned short iZone;

	nTotalZone = nFluidZone + nStrucZone;

	for (iZone = nFluidZone; iZone < nTotalZone; iZone++) {

		/*------------------- Compute the coefficient ---------------------*/

		solver_container[iZone][MESH_0][FEA_SOL]->ComputeAitken_Coefficient(geometry_container[iZone], config_container[iZone],
																		    solver_container[iZone], iFSIIter);

		/*----------------- Set the relaxation parameter ------------------*/

		solver_container[iZone][MESH_0][FEA_SOL]->SetAitken_Relaxation(geometry_container[iZone], config_container[iZone],
																	   solver_container[iZone]);

	}

}

void FSI_Load_Relaxation(COutput *output, CIntegration ***integration_container, CGeometry ***geometry_container,
	     	 	 	 	 	CSolver ****solver_container, CNumerics *****numerics_container, CConfig **config_container,
	     	 	 	 	 	CSurfaceMovement **surface_movement, CVolumetricMovement **grid_movement, CFreeFormDefBox*** FFDBox){


}


void FSI_Disp_Predictor(COutput *output, CIntegration ***integration_container, CGeometry ***geometry_container,
					     CSolver ****solver_container, CNumerics *****numerics_container, CConfig **config_container,
						 CSurfaceMovement **surface_movement, CVolumetricMovement **grid_movement, CFreeFormDefBox*** FFDBox){

	solver_container[ZONE_1][MESH_0][FEA_SOL]->PredictStruct_Displacement(geometry_container[ZONE_1], config_container[ZONE_1],
																		  solver_container[ZONE_1]);

}

void FSI_Load_Predictor(COutput *output, CIntegration ***integration_container, CGeometry ***geometry_container,
					     CSolver ****solver_container, CNumerics *****numerics_container, CConfig **config_container,
						 CSurfaceMovement **surface_movement, CVolumetricMovement **grid_movement, CFreeFormDefBox*** FFDBox,
						 unsigned long ExtIter){


}
