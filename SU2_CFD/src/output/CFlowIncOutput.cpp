/*!
 * \file output_flow_inc.cpp
 * \brief Main subroutines for incompressible flow output
 * \author R. Sanchez
 * \version 6.0.1 "Falcon"
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
 * Copyright 2012-2018, Francisco D. Palacios, Thomas D. Economon,
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

#include "../../include/output/CFlowIncOutput.hpp"

#include "../../../Common/include/geometry_structure.hpp"
#include "../../include/solver_structure.hpp"

CFlowIncOutput::CFlowIncOutput(CConfig *config, unsigned short nDim) : CFlowOutput(config, nDim, false) {
  
  turb_model = config->GetKind_Turb_Model();
  
  heat = config->GetEnergy_Equation();
  
  weakly_coupled_heat = config->GetWeakly_Coupled_Heat();
      
  /*--- Set the default history fields if nothing is set in the config file ---*/
  
  if (nRequestedHistoryFields == 0){
    requestedHistoryFields.push_back("ITER");
    requestedHistoryFields.push_back("RMS_RES");
    nRequestedHistoryFields = requestedHistoryFields.size();
  }
  
  if (nRequestedScreenFields == 0){
    if (multiZone) requestedScreenFields.push_back("OUTER_ITER");
    requestedScreenFields.push_back("INNER_ITER");
    requestedScreenFields.push_back("RMS_PRESSURE");
    requestedScreenFields.push_back("RMS_VELOCITY-X");
    requestedScreenFields.push_back("RMS_VELOCITY-Y");
    nRequestedScreenFields = requestedScreenFields.size();
  }
  
  if (nRequestedVolumeFields == 0){
    requestedVolumeFields.push_back("COORDINATES");
    requestedVolumeFields.push_back("SOLUTION");
    requestedVolumeFields.push_back("PRIMITIVE");
    nRequestedVolumeFields = requestedVolumeFields.size();
  }
  
  stringstream ss;
  ss << "Zone " << config->GetiZone() << " (Incomp. Fluid)";
  multiZoneHeaderString = ss.str();
  
  /*--- Set the volume filename --- */
  
  volumeFilename = config->GetVolume_FileName(); 
  
  /*--- Set the surface filename --- */
  
  surfaceFilename = config->GetSurfCoeff_FileName();
  
  /*--- Set the restart filename --- */
  
  restartFilename = config->GetRestart_FileName();

  /*--- Set the default convergence field --- */

  if (convField.size() == 0 ) convField = "RMS_PRESSURE";


}

CFlowIncOutput::~CFlowIncOutput(void) {

  if (rank == MASTER_NODE){
    histFile.close();
  }


}


void CFlowIncOutput::SetHistoryOutputFields(CConfig *config){

  /// BEGIN_GROUP: RMS_RES, DESCRIPTION: The root-mean-square residuals of the SOLUTION variables. 
  /// DESCRIPTION: Root-mean square residual of the pressure.
  AddHistoryOutput("RMS_PRESSURE",   "rms[P]", FORMAT_FIXED,   "RMS_RES", "Root-mean square residual of the pressure.", TYPE_RESIDUAL);
  /// DESCRIPTION: Root-mean square residual of the velocity x-component.  
  AddHistoryOutput("RMS_VELOCITY-X", "rms[U]", FORMAT_FIXED,   "RMS_RES", "Root-mean square residual of the velocity x-component.", TYPE_RESIDUAL);
  /// DESCRIPTION: Root-mean square residual of the velocity y-component.  
  AddHistoryOutput("RMS_VELOCITY-Y", "rms[V]", FORMAT_FIXED,   "RMS_RES", "Root-mean square residual of the velocity y-component.", TYPE_RESIDUAL);
  /// DESCRIPTION: Root-mean square residual of the velocity z-component.  
  if (nDim == 3) AddHistoryOutput("RMS_VELOCITY-Z", "rms[W]", FORMAT_FIXED,   "RMS_RES", "Root-mean square residual of the velocity z-component.", TYPE_RESIDUAL);
  /// DESCRIPTION: Maximum residual of the temperature.
  if (heat || weakly_coupled_heat) AddHistoryOutput("RMS_HEAT", "rms[T]", FORMAT_FIXED, "RMS_RES", "Root-mean square residual of the temperature.", TYPE_RESIDUAL);
  
  switch(turb_model){
  case SA: case SA_NEG: case SA_E: case SA_COMP: case SA_E_COMP:
    /// DESCRIPTION: Root-mean square residual of nu tilde (SA model).  
    AddHistoryOutput("RMS_NU_TILDE",       "rms[nu]", FORMAT_FIXED, "RMS_RES", "Root-mean square residual of nu tilde (SA model).", TYPE_RESIDUAL);
    break;  
  case SST: case SST_SUST:
    /// DESCRIPTION: Root-mean square residual of kinetic energy (SST model).    
    AddHistoryOutput("RMS_TKE", "rms[k]",  FORMAT_FIXED, "RMS_RES", "Root-mean square residual of kinetic energy (SST model).", TYPE_RESIDUAL);
    /// DESCRIPTION: Root-mean square residual of the dissipation (SST model).    
    AddHistoryOutput("RMS_DISSIPATION",    "rms[w]",  FORMAT_FIXED, "RMS_RES", "Root-mean square residual of dissipation (SST model).", TYPE_RESIDUAL);
    break;
  default: break;
  }
  /// END_GROUP
  
  /// BEGIN_GROUP: MAX_RES, DESCRIPTION: The maximum residuals of the SOLUTION variables. 
  /// DESCRIPTION: Maximum residual of the pressure.
  AddHistoryOutput("MAX_PRESSURE",   "max[P]", FORMAT_FIXED,   "MAX_RES", "Maximum residual of the pressure.", TYPE_RESIDUAL);
  /// DESCRIPTION: Maximum residual of the velocity x-component.   
  AddHistoryOutput("MAX_VELOCITY-X", "max[U]", FORMAT_FIXED,   "MAX_RES", "Maximum residual of the velocity x-component.", TYPE_RESIDUAL);
  /// DESCRIPTION: Maximum residual of the velocity y-component.   
  AddHistoryOutput("MAX_VELOCITY-Y", "max[V]", FORMAT_FIXED,   "MAX_RES", "Maximum residual of the velocity y-component.", TYPE_RESIDUAL);
  /// DESCRIPTION: Maximum residual of the velocity z-component.   
  if (nDim == 3) 
    AddHistoryOutput("MAX_VELOCITY-Z", "max[W]", FORMAT_FIXED,   "MAX_RES", "Maximum residual of the velocity z-component.", TYPE_RESIDUAL);
  /// DESCRIPTION: Maximum residual of the temperature.
  if (heat || weakly_coupled_heat) 
    AddHistoryOutput("MAX_HEAT", "max[T]", FORMAT_FIXED, "MAX_RES", "Root-mean square residual of the temperature.", TYPE_RESIDUAL);
  
  switch(turb_model){
  case SA: case SA_NEG: case SA_E: case SA_COMP: case SA_E_COMP:
    /// DESCRIPTION: Maximum residual of nu tilde (SA model).
    AddHistoryOutput("MAX_NU_TILDE",       "max[nu]", FORMAT_FIXED, "MAX_RES", "Maximum residual of nu tilde (SA model).", TYPE_RESIDUAL);
    break;  
  case SST: case SST_SUST:
    /// DESCRIPTION: Maximum residual of kinetic energy (SST model). 
    AddHistoryOutput("MAX_TKE", "max[k]",  FORMAT_FIXED, "MAX_RES", "Maximum residual of kinetic energy (SST model).", TYPE_RESIDUAL);
    /// DESCRIPTION: Maximum residual of the dissipation (SST model).   
    AddHistoryOutput("MAX_DISSIPATION",    "max[w]",  FORMAT_FIXED, "MAX_RES", "Maximum residual of dissipation (SST model).", TYPE_RESIDUAL); 
    break;
  default: break;
  }
  /// END_GROUP
  
  /// BEGIN_GROUP: BGS_RES, DESCRIPTION: The block-gauss seidel residuals of the SOLUTION variables. 
  /// DESCRIPTION: Maximum residual of the pressure.
  AddHistoryOutput("BGS_PRESSURE",   "bgs[P]", FORMAT_FIXED,   "BGS_RES", "BGS residual of the pressure.", TYPE_RESIDUAL);
  /// DESCRIPTION: Maximum residual of the velocity x-component.   
  AddHistoryOutput("BGS_VELOCITY-X", "bgs[U]", FORMAT_FIXED,   "BGS_RES", "BGS residual of the velocity x-component.", TYPE_RESIDUAL);
  /// DESCRIPTION: Maximum residual of the velocity y-component.   
  AddHistoryOutput("BGS_VELOCITY-Y", "bgs[V]", FORMAT_FIXED,   "BGS_RES", "BGS residual of the velocity y-component.", TYPE_RESIDUAL);
  /// DESCRIPTION: Maximum residual of the velocity z-component.   
  if (nDim == 3) 
    AddHistoryOutput("BGS_VELOCITY-Z", "bgs[W]", FORMAT_FIXED,   "BGS_RES", "BGS residual of the velocity z-component.", TYPE_RESIDUAL);
  /// DESCRIPTION: Maximum residual of the temperature.
  if (heat || weakly_coupled_heat) 
    AddHistoryOutput("BGS_HEAT", "bgs[T]", FORMAT_FIXED, "BGS_RES", "BGS residual of the temperature.", TYPE_RESIDUAL);
  
  switch(turb_model){
  case SA: case SA_NEG: case SA_E: case SA_COMP: case SA_E_COMP:
    /// DESCRIPTION: Maximum residual of nu tilde (SA model).
    AddHistoryOutput("BGS_NU_TILDE",       "bgs[nu]", FORMAT_FIXED, "BGS_RES", "BGS residual of nu tilde (SA model).", TYPE_RESIDUAL);
    break;  
  case SST: case SST_SUST:
    /// DESCRIPTION: Maximum residual of kinetic energy (SST model). 
    AddHistoryOutput("BGS_TKE", "bgs[k]",  FORMAT_FIXED, "BGS_RES", "BGS residual of kinetic energy (SST model).", TYPE_RESIDUAL);
    /// DESCRIPTION: Maximum residual of the dissipation (SST model).   
    AddHistoryOutput("BGS_DISSIPATION",    "bgs[w]",  FORMAT_FIXED, "BGS_RES", "BGS residual of dissipation (SST model).", TYPE_RESIDUAL); 
    break;
  default: break;
  }
  /// END_GROUP

  /// BEGIN_GROUP: HEAT_COEFF, DESCRIPTION: Heat coefficients on all surfaces set with MARKER_MONITORING.
  /// DESCRIPTION: Total heatflux
  AddHistoryOutput("HEATFLUX", "HF",      FORMAT_SCIENTIFIC, "HEAT", "Total heatflux on all surfaces set with MARKER_MONITORING.", TYPE_COEFFICIENT);
  /// DESCRIPTION: Maximal heatflux  
  AddHistoryOutput("HEATFLUX_MAX", "maxHF",    FORMAT_SCIENTIFIC, "HEAT", "Total maximum heatflux on all surfaces set with MARKER_MONITORING.", TYPE_COEFFICIENT);
  /// DESCRIPTION: Temperature
  AddHistoryOutput("TEMPERATURE", "Temp", FORMAT_SCIENTIFIC, "HEAT",  "Total avg. temperature on all surfaces set with MARKER_MONITORING.", TYPE_COEFFICIENT);
  /// END_GROUP
  
  /// DESCRIPTION: Angle of attack  
  AddHistoryOutput("AOA",         "AoA",                      FORMAT_SCIENTIFIC,"AOA", "Angle of attack");
  /// DESCRIPTION: Linear solver iterations   
  AddHistoryOutput("LINSOL_ITER", "LinSolIter", FORMAT_INTEGER, "LINSOL", "Number of iterations of the linear solver.");
  AddHistoryOutput("LINSOL_RESIDUAL", "LinSolRes", FORMAT_FIXED, "LINSOL", "Residual of the linear solver.");
  AddHistoryOutput("CFL_NUMBER", "CFL number", FORMAT_SCIENTIFIC, "CFL_NUMBER", "Current value of the CFL number");  
  
  if (config->GetDeform_Mesh()){
    AddHistoryOutput("DEFORM_MIN_VOLUME", "MinVolume", FORMAT_SCIENTIFIC, "DEFORM", "Minimum volume in the mesh");
    AddHistoryOutput("DEFORM_MAX_VOLUME", "MaxVolume", FORMAT_SCIENTIFIC, "DEFORM", "Maximum volume in the mesh");
    AddHistoryOutput("DEFORM_ITER", "DeformIter", FORMAT_INTEGER, "DEFORM", "Linear solver iterations for the mesh deformation");
    AddHistoryOutput("DEFORM_RESIDUAL", "DeformRes", FORMAT_FIXED, "DEFORM", "Residual of the linear solver for the mesh deformation");    
  }
  
  /*--- Add analyze surface history fields --- */
  
  AddAnalyzeSurfaceOutput(config);
  
  /*--- Add aerodynamic coefficients fields --- */
  
  AddAerodynamicCoefficients(config);
  
}

void CFlowIncOutput::LoadHistoryData(CConfig *config, CGeometry *geometry, CSolver **solver) {
  
  CSolver* flow_solver = solver[FLOW_SOL];
  CSolver* turb_solver = solver[TURB_SOL];  
  CSolver* heat_solver = solver[HEAT_SOL];
  CSolver* mesh_solver = solver[MESH_SOL];
  
  SetHistoryOutputValue("RMS_PRESSURE", log10(flow_solver->GetRes_RMS(0)));
  SetHistoryOutputValue("RMS_VELOCITY-X", log10(flow_solver->GetRes_RMS(1)));
  SetHistoryOutputValue("RMS_VELOCITY-Y", log10(flow_solver->GetRes_RMS(2)));
  if (nDim == 3) SetHistoryOutputValue("RMS_VELOCITY-Z", log10(flow_solver->GetRes_RMS(3)));
 
  switch(turb_model){
  case SA: case SA_NEG: case SA_E: case SA_COMP: case SA_E_COMP:
    SetHistoryOutputValue("RMS_NU_TILDE", log10(turb_solver->GetRes_RMS(0)));
    break;  
  case SST: case SST_SUST:
    SetHistoryOutputValue("RMS_TKE", log10(turb_solver->GetRes_RMS(0)));
    SetHistoryOutputValue("RMS_DISSIPATION",    log10(turb_solver->GetRes_RMS(1)));
    break;
  }
  
  SetHistoryOutputValue("MAX_PRESSURE", log10(flow_solver->GetRes_Max(0)));
  SetHistoryOutputValue("MAX_VELOCITY-X", log10(flow_solver->GetRes_Max(1)));
  SetHistoryOutputValue("MAX_VELOCITY-Y", log10(flow_solver->GetRes_Max(2)));
  if (nDim == 3) SetHistoryOutputValue("RMS_VELOCITY-Z", log10(flow_solver->GetRes_Max(3)));
 
  switch(turb_model){
  case SA: case SA_NEG: case SA_E: case SA_COMP: case SA_E_COMP:
    SetHistoryOutputValue("MAX_NU_TILDE", log10(turb_solver->GetRes_Max(0)));
    break;  
  case SST: case SST_SUST:
    SetHistoryOutputValue("MAX_TKE", log10(turb_solver->GetRes_Max(0)));
    SetHistoryOutputValue("MAX_DISSIPATION",    log10(turb_solver->GetRes_Max(1)));
    break;
  }
  
  if (multiZone){
    SetHistoryOutputValue("BGS_PRESSURE", log10(flow_solver->GetRes_BGS(0)));
    SetHistoryOutputValue("BGS_VELOCITY-X", log10(flow_solver->GetRes_BGS(1)));
    SetHistoryOutputValue("BGS_VELOCITY-Y", log10(flow_solver->GetRes_BGS(2)));
    if (nDim == 3) SetHistoryOutputValue("BGS_VELOCITY-Z", log10(flow_solver->GetRes_BGS(3)));
    
    switch(turb_model){
    case SA: case SA_NEG: case SA_E: case SA_COMP: case SA_E_COMP:
      SetHistoryOutputValue("BGS_NU_TILDE", log10(turb_solver->GetRes_BGS(0)));
      break;  
    case SST:
      SetHistoryOutputValue("BGS_TKE", log10(turb_solver->GetRes_BGS(0)));
      SetHistoryOutputValue("BGS_DISSIPATION",    log10(turb_solver->GetRes_BGS(1)));
      break;
    }
  }
  
  if (weakly_coupled_heat){
    SetHistoryOutputValue("HEATFLUX",     heat_solver->GetTotal_HeatFlux());
    SetHistoryOutputValue("HEATFLUX_MAX", heat_solver->GetTotal_MaxHeatFlux());
    SetHistoryOutputValue("TEMPERATURE",  heat_solver->GetTotal_AvgTemperature());
    SetHistoryOutputValue("RMS_HEAT",         log10(heat_solver->GetRes_RMS(0)));
    SetHistoryOutputValue("MAX_HEAT",         log10(heat_solver->GetRes_Max(0)));
    if (multiZone) SetHistoryOutputValue("BGS_HEAT",         log10(heat_solver->GetRes_BGS(0)));
  }
  if (heat){
    SetHistoryOutputValue("HEATFLUX",     flow_solver->GetTotal_HeatFlux());
    SetHistoryOutputValue("HEATFLUX_MAX", flow_solver->GetTotal_MaxHeatFlux());
    SetHistoryOutputValue("TEMPERATURE",  flow_solver->GetTotal_AvgTemperature());
    if (nDim == 3) SetHistoryOutputValue("RMS_HEAT",         log10(flow_solver->GetRes_RMS(4)));
    else           SetHistoryOutputValue("RMS_HEAT",         log10(flow_solver->GetRes_RMS(3)));
    
    if (nDim == 3) SetHistoryOutputValue("MAX_HEAT",         log10(flow_solver->GetRes_Max(4)));
    else           SetHistoryOutputValue("MAX_HEAT",         log10(flow_solver->GetRes_Max(3)));
    if (multiZone){
      if (nDim == 3) SetHistoryOutputValue("BGS_HEAT",         log10(flow_solver->GetRes_BGS(4)));
      else           SetHistoryOutputValue("BGS_HEAT",         log10(flow_solver->GetRes_BGS(3)));
    }

  }  
  
  SetHistoryOutputValue("LINSOL_ITER", flow_solver->GetIterLinSolver());
  SetHistoryOutputValue("LINSOL_RESIDUAL", log10(flow_solver->GetLinSol_Residual()));
  
  if (config->GetDeform_Mesh()){
    SetHistoryOutputValue("DEFORM_MIN_VOLUME", mesh_solver->GetMinimum_Volume());
    SetHistoryOutputValue("DEFORM_MAX_VOLUME", mesh_solver->GetMaximum_Volume());
    SetHistoryOutputValue("DEFORM_ITER", mesh_solver->GetIterLinSolver());
    SetHistoryOutputValue("DEFORM_RESIDUAL", log10(mesh_solver->GetLinSol_Residual()));
  }
  
  SetHistoryOutputValue("CFL_NUMBER", config->GetCFL(MESH_0));

  
  /*--- Set the analyse surface history values --- */
  
  SetAnalyzeSurface(flow_solver, geometry, config, false);
  
  /*--- Set aeroydnamic coefficients --- */
  
  SetAerodynamicCoefficients(config, flow_solver);

}


void CFlowIncOutput::SetVolumeOutputFields(CConfig *config){
  
  // Grid coordinates
  AddVolumeOutput("COORD-X", "x", "COORDINATES", "x-component of the coordinate vector");
  AddVolumeOutput("COORD-Y", "y", "COORDINATES", "y-component of the coordinate vector");
  if (nDim == 3)
    AddVolumeOutput("COORD-Z", "z", "COORDINATES", "z-component of the coordinate vector");
  
  // SOLUTION variables
  AddVolumeOutput("PRESSURE",   "Pressure",   "SOLUTION", "Pressure");
  AddVolumeOutput("VELOCITY-X", "Velocity_x", "SOLUTION", "x-component of the velocity vector");
  AddVolumeOutput("VELOCITY-Y", "Velocity_y", "SOLUTION", "y-component of the velocity vector");
  if (nDim == 3) 
    AddVolumeOutput("VELOCITY-Z", "Velocity_z", "SOLUTION", "z-component of the velocity vector");
  if (heat || weakly_coupled_heat) 
    AddVolumeOutput("TEMPERATURE",  "Temperature","SOLUTION", "Temperature");  
  
  switch(config->GetKind_Turb_Model()){
  case SST: case SST_SUST:
    AddVolumeOutput("TKE", "Turb_Kin_Energy", "SOLUTION", "Turbulent kinetic energy");
    AddVolumeOutput("DISSIPATION", "Omega", "SOLUTION", "Rate of dissipation");
    break;
  case SA: case SA_COMP: case SA_E: 
  case SA_E_COMP: case SA_NEG: 
    AddVolumeOutput("NU_TILDE", "Nu_Tilde", "SOLUTION", "Spalart–Allmaras variable");
    break;
  case NONE:
    break;
  }
  
  // Grid velocity
  if (config->GetGrid_Movement()){
    AddVolumeOutput("GRID_VELOCITY-X", "Grid_Velocity_x", "GRID_VELOCITY", "x-component of the grid velocity vector");
    AddVolumeOutput("GRID_VELOCITY-Y", "Grid_Velocity_y", "GRID_VELOCITY", "y-component of the grid velocity vector");
    if (nDim == 3 ) 
      AddVolumeOutput("GRID_VELOCITY-Z", "Grid_Velocity_z", "GRID_VELOCITY", "z-component of the grid velocity vector");
  }
  
  // Primitive variables
  AddVolumeOutput("PRESSURE_COEFF", "Pressure_Coefficient", "PRIMITIVE", "Pressure coefficient");
  AddVolumeOutput("DENSITY",        "Density",              "PRIMITIVE", "Density");
  
  if (config->GetKind_Solver() == INC_RANS || config->GetKind_Solver() == INC_NAVIER_STOKES){
    AddVolumeOutput("LAMINAR_VISCOSITY", "Laminar_Viscosity", "PRIMITIVE", "Laminar viscosity");
    
    AddVolumeOutput("SKIN_FRICTION-X", "Skin_Friction_Coefficient_x", "PRIMITIVE", "x-component of the skin friction vector");
    AddVolumeOutput("SKIN_FRICTION-Y", "Skin_Friction_Coefficient_y", "PRIMITIVE", "y-component of the skin friction vector");
    if (nDim == 3)
      AddVolumeOutput("SKIN_FRICTION-Z", "Skin_Friction_Coefficient_z", "PRIMITIVE", "z-component of the skin friction vector");
    
    AddVolumeOutput("HEAT_FLUX", "Heat_Flux", "PRIMITIVE", "Heat-flux");
    AddVolumeOutput("Y_PLUS", "Y_Plus", "PRIMITIVE", "Non-dim. wall distance (Y-Plus)");
    
  }
  
  if (config->GetKind_Solver() == INC_RANS) {
    AddVolumeOutput("EDDY_VISCOSITY", "Eddy_Viscosity", "PRIMITIVE", "Turbulent eddy viscosity");
  }
  
  if (config->GetKind_Trans_Model() == BC){
    AddVolumeOutput("INTERMITTENCY", "gamma_BC", "INTERMITTENCY", "Intermittency");
  }

  //Residuals
  AddVolumeOutput("RES_PRESSURE", "Residual_Pressure", "RESIDUAL", "Residual of the pressure");
  AddVolumeOutput("RES_VELOCITY-X", "Residual_Velocity_x", "RESIDUAL", "Residual of the x-velocity component");
  AddVolumeOutput("RES_VELOCITY-Y", "Residual_Velocity_y", "RESIDUAL", "Residual of the y-velocity component");
  if (nDim == 3)
    AddVolumeOutput("RES_VELOCITY-Z", "Residual_Velocity_z", "RESIDUAL", "Residual of the z-velocity component");
  AddVolumeOutput("RES_TEMPERATURE", "Residual_Temperature", "RESIDUAL", "Residual of the temperature");
  
  switch(config->GetKind_Turb_Model()){
  case SST: case SST_SUST:
    AddVolumeOutput("RES_TKE", "Residual_TKE", "RESIDUAL", "Residual of turbulent kinetic energy");
    AddVolumeOutput("RES_DISSIPATION", "Residual_Omega", "RESIDUAL", "Residual of the rate of dissipation.");
    break;
  case SA: case SA_COMP: case SA_E: 
  case SA_E_COMP: case SA_NEG: 
    AddVolumeOutput("RES_NU_TILDE", "Residual_Nu_Tilde", "RESIDUAL", "Residual of the Spalart–Allmaras variable");
    break;
  case NONE:
    break;
  }
  
  // Limiter values
  AddVolumeOutput("LIMITER_PRESSURE", "Limiter_Pressure", "LIMITER", "Limiter value of the pressure");
  AddVolumeOutput("LIMITER_VELOCITY-X", "Limiter_Velocity_x", "LIMITER", "Limiter value of the x-velocity");
  AddVolumeOutput("LIMITER_VELOCITY-Y", "Limiter_Velocity_y", "LIMITER", "Limiter value of the y-velocity");
  if (nDim == 3)
    AddVolumeOutput("LIMITER_VELOCITY-Z", "Limiter_Velocity_z", "LIMITER", "Limiter value of the z-velocity");
  AddVolumeOutput("LIMITER_TEMPERATURE", "Limiter_Temperature", "LIMITER", "Limiter value of the temperature");
  
  switch(config->GetKind_Turb_Model()){
  case SST: case SST_SUST:
    AddVolumeOutput("LIMITER_TKE", "Limiter_TKE", "LIMITER", "Limiter value of turb. kinetic energy.");
    AddVolumeOutput("LIMITER_DISSIPATION", "Limiter_Omega", "LIMITER", "Limiter value of dissipation rate.");
    break;
  case SA: case SA_COMP: case SA_E: 
  case SA_E_COMP: case SA_NEG: 
    AddVolumeOutput("LIMITER_NU_TILDE", "Limiter_Nu_Tilde", "LIMITER", "Limiter value of Spalart–Allmaras variable.");
    break;
  case NONE:
    break;
  }
  
  // Hybrid RANS-LES
  if (config->GetKind_HybridRANSLES() != NO_HYBRIDRANSLES){
    AddVolumeOutput("DES_LENGTHSCALE", "DES_LengthScale", "DDES", "DES length scale value");
    AddVolumeOutput("WALL_DISTANCE", "Wall_Distance", "DDES", "Wall distance value");
  }
  
  // Roe Low Dissipation
  if (config->GetKind_RoeLowDiss() != NO_ROELOWDISS){
    AddVolumeOutput("ROE_DISSIPATION", "Roe_Dissipation", "ROE_DISSIPATION", "Value of the Roe dissipation");
  }
  
  if(config->GetKind_Solver() == INC_RANS || config->GetKind_Solver() == INC_NAVIER_STOKES){
    if (nDim == 3){
      AddVolumeOutput("VORTICITY_X", "Vorticity_x", "VORTEX_IDENTIFICATION", "x-component of the vorticity vector");
      AddVolumeOutput("VORTICITY_Y", "Vorticity_y", "VORTEX_IDENTIFICATION", "y-component of the vorticity vector");
      AddVolumeOutput("Q_CRITERION", "Q_Criterion", "VORTEX_IDENTIFICATION", "Value of the Q-Criterion");      
    }
    AddVolumeOutput("VORTICITY_Z", "Vorticity_z", "VORTEX_IDENTIFICATION", "z-component of the vorticity vector");
  }
}

void CFlowIncOutput::LoadVolumeData(CConfig *config, CGeometry *geometry, CSolver **solver, unsigned long iPoint){

  CVariable* Node_Flow = solver[FLOW_SOL]->node[iPoint]; 
  CVariable* Node_Heat = NULL; 
  CVariable* Node_Turb = NULL;
  
  if (config->GetKind_Turb_Model() != NONE){
    Node_Turb = solver[TURB_SOL]->node[iPoint]; 
  }
  if (weakly_coupled_heat){
    Node_Heat = solver[HEAT_SOL]->node[iPoint];
  }
  
  CPoint*    Node_Geo  = geometry->node[iPoint];
          
  SetVolumeOutputValue("COORD-X", iPoint,  Node_Geo->GetCoord(0));  
  SetVolumeOutputValue("COORD-Y", iPoint,  Node_Geo->GetCoord(1));
  if (nDim == 3)
    SetVolumeOutputValue("COORD-Z", iPoint, Node_Geo->GetCoord(2));
  
  SetVolumeOutputValue("PRESSURE",    iPoint, Node_Flow->GetSolution(0));
  SetVolumeOutputValue("VELOCITY-X", iPoint, Node_Flow->GetSolution(1));
  SetVolumeOutputValue("VELOCITY-Y", iPoint, Node_Flow->GetSolution(2));
  if (nDim == 3){
    SetVolumeOutputValue("VELOCITY-Z", iPoint, Node_Flow->GetSolution(3));
    if (heat) SetVolumeOutputValue("TEMPERATURE",     iPoint, Node_Flow->GetSolution(4));
  } else {
    if (heat) SetVolumeOutputValue("TEMPERATURE",     iPoint, Node_Flow->GetSolution(3));
  }
  if (weakly_coupled_heat) SetVolumeOutputValue("TEMPERATURE",     iPoint, Node_Heat->GetSolution(0));
  
  switch(config->GetKind_Turb_Model()){
  case SST: case SST_SUST:
    SetVolumeOutputValue("TKE", iPoint, Node_Turb->GetSolution(0));
    SetVolumeOutputValue("DISSIPATION", iPoint, Node_Turb->GetSolution(1));
    break;
  case SA: case SA_COMP: case SA_E: 
  case SA_E_COMP: case SA_NEG: 
    SetVolumeOutputValue("NU_TILDE", iPoint, Node_Turb->GetSolution(0));
    break;
  case NONE:
    break;
  }
  
  if (config->GetGrid_Movement()){
    SetVolumeOutputValue("GRID_VELOCITY-X", iPoint, Node_Geo->GetGridVel()[0]);
    SetVolumeOutputValue("GRID_VELOCITY-Y", iPoint, Node_Geo->GetGridVel()[1]);
    if (nDim == 3)
      SetVolumeOutputValue("GRID_VELOCITY-Z", iPoint, Node_Geo->GetGridVel()[2]);
  }
  
  su2double VelMag = 0.0;
  for (unsigned short iDim = 0; iDim < nDim; iDim++){
    VelMag += solver[FLOW_SOL]->GetVelocity_Inf(iDim)*solver[FLOW_SOL]->GetVelocity_Inf(iDim);    
  }
  su2double factor = 1.0/(0.5*solver[FLOW_SOL]->GetDensity_Inf()*VelMag); 
  SetVolumeOutputValue("PRESSURE_COEFF", iPoint, (Node_Flow->GetPressure() - config->GetPressure_FreeStreamND())*factor);
  SetVolumeOutputValue("DENSITY", iPoint, Node_Flow->GetDensity());
  
  if (config->GetKind_Solver() == INC_RANS || config->GetKind_Solver() == INC_NAVIER_STOKES){
    SetVolumeOutputValue("LAMINAR_VISCOSITY", iPoint, Node_Flow->GetLaminarViscosity());
  }
  
  if (config->GetKind_Solver() == INC_RANS) {
    SetVolumeOutputValue("EDDY_VISCOSITY", iPoint, Node_Flow->GetEddyViscosity());
  }
  
  if (config->GetKind_Trans_Model() == BC){
    SetVolumeOutputValue("INTERMITTENCY", iPoint, Node_Turb->GetGammaBC());
  }
  
  SetVolumeOutputValue("RES_PRESSURE", iPoint, solver[FLOW_SOL]->LinSysRes.GetBlock(iPoint, 0));
  SetVolumeOutputValue("RES_VELOCITY-X", iPoint, solver[FLOW_SOL]->LinSysRes.GetBlock(iPoint, 1));
  SetVolumeOutputValue("RES_VELOCITY-Y", iPoint, solver[FLOW_SOL]->LinSysRes.GetBlock(iPoint, 2));
  if (nDim == 3){
    SetVolumeOutputValue("RES_VELOCITY-Z", iPoint, solver[FLOW_SOL]->LinSysRes.GetBlock(iPoint, 3));
    SetVolumeOutputValue("RES_TEMPERATURE", iPoint, solver[FLOW_SOL]->LinSysRes.GetBlock(iPoint, 4));
  } else {
    SetVolumeOutputValue("RES_TEMPERATURE", iPoint, solver[FLOW_SOL]->LinSysRes.GetBlock(iPoint, 3));
  }
  
  switch(config->GetKind_Turb_Model()){
  case SST: case SST_SUST:
    SetVolumeOutputValue("RES_TKE", iPoint, solver[TURB_SOL]->LinSysRes.GetBlock(iPoint, 0));
    SetVolumeOutputValue("RES_DISSIPATION", iPoint, solver[TURB_SOL]->LinSysRes.GetBlock(iPoint, 1));
    break;
  case SA: case SA_COMP: case SA_E: 
  case SA_E_COMP: case SA_NEG: 
    SetVolumeOutputValue("RES_NU_TILDE", iPoint, solver[TURB_SOL]->LinSysRes.GetBlock(iPoint, 0));
    break;
  case NONE:
    break;
  }
  
  SetVolumeOutputValue("LIMITER_PRESSURE", iPoint, Node_Flow->GetLimiter_Primitive(0));
  SetVolumeOutputValue("LIMITER_VELOCITY-X", iPoint, Node_Flow->GetLimiter_Primitive(1));
  SetVolumeOutputValue("LIMITER_VELOCITY-Y", iPoint, Node_Flow->GetLimiter_Primitive(2));
  if (nDim == 3){
    SetVolumeOutputValue("LIMITER_VELOCITY-Z", iPoint, Node_Flow->GetLimiter_Primitive(3));
    SetVolumeOutputValue("LIMITER_TEMPERATURE", iPoint, Node_Flow->GetLimiter_Primitive(4));
  } else {
    SetVolumeOutputValue("LIMITER_TEMPERATURE", iPoint, Node_Flow->GetLimiter_Primitive(3));   
  }
  
  switch(config->GetKind_Turb_Model()){
  case SST: case SST_SUST:
    SetVolumeOutputValue("LIMITER_TKE", iPoint, Node_Turb->GetLimiter_Primitive(0));
    SetVolumeOutputValue("LIMITER_DISSIPATION", iPoint, Node_Turb->GetLimiter_Primitive(1));
    break;
  case SA: case SA_COMP: case SA_E: 
  case SA_E_COMP: case SA_NEG: 
    SetVolumeOutputValue("LIMITER_NU_TILDE", iPoint, Node_Turb->GetLimiter_Primitive(0));
    break;
  case NONE:
    break;
  }
  
  if (config->GetKind_HybridRANSLES() != NO_HYBRIDRANSLES){
    SetVolumeOutputValue("DES_LENGTHSCALE", iPoint, Node_Flow->GetDES_LengthScale());
    SetVolumeOutputValue("WALL_DISTANCE", iPoint, Node_Geo->GetWall_Distance());
  }
  
  if (config->GetKind_RoeLowDiss() != NO_ROELOWDISS){
    SetVolumeOutputValue("ROE_DISSIPATION", iPoint, Node_Flow->GetRoe_Dissipation());
  }  
  
  if(config->GetKind_Solver() == INC_RANS || config->GetKind_Solver() == INC_NAVIER_STOKES){
    if (nDim == 3){
      SetVolumeOutputValue("VORTICITY_X", iPoint, Node_Flow->GetVorticity()[0]);
      SetVolumeOutputValue("VORTICITY_Y", iPoint, Node_Flow->GetVorticity()[1]);     
      SetVolumeOutputValue("Q_CRITERION", iPoint, GetQ_Criterion(config, geometry, Node_Flow));            
    } 
    SetVolumeOutputValue("VORTICITY_Z", iPoint, Node_Flow->GetVorticity()[2]);      
  }
}

void CFlowIncOutput::LoadSurfaceData(CConfig *config, CGeometry *geometry, CSolver **solver, unsigned long iPoint, unsigned short iMarker, unsigned long iVertex){
  
  if ((config->GetKind_Solver() == INC_NAVIER_STOKES) || (config->GetKind_Solver()  == INC_RANS)) {
    SetVolumeOutputValue("SKIN_FRICTION-X", iPoint, solver[FLOW_SOL]->GetCSkinFriction(iMarker, iVertex, 0));
    SetVolumeOutputValue("SKIN_FRICTION-Y", iPoint, solver[FLOW_SOL]->GetCSkinFriction(iMarker, iVertex, 1));
    if (nDim == 3)
      SetVolumeOutputValue("SKIN_FRICTION-Z", iPoint, solver[FLOW_SOL]->GetCSkinFriction(iMarker, iVertex, 2));
  
    SetVolumeOutputValue("HEAT_FLUX", iPoint, solver[FLOW_SOL]->GetHeatFlux(iMarker, iVertex));
    SetVolumeOutputValue("Y_PLUS", iPoint, solver[FLOW_SOL]->GetYPlus(iMarker, iVertex));
  }
}

su2double CFlowIncOutput::GetQ_Criterion(CConfig *config, CGeometry *geometry, CVariable* node_flow){
  
  unsigned short iDim;
  su2double Grad_Vel[3][3] = {{0.0, 0.0, 0.0},{0.0, 0.0, 0.0},{0.0, 0.0, 0.0}};
  
  for (iDim = 0; iDim < nDim; iDim++) {
    for (unsigned short jDim = 0 ; jDim < nDim; jDim++) {
      Grad_Vel[iDim][jDim] = node_flow->GetGradient_Primitive(iDim+1, jDim);
    }
  }
  
  su2double s11 = Grad_Vel[0][0];
  su2double s12 = 0.5 * (Grad_Vel[0][1] + Grad_Vel[1][0]);
  su2double s13 = 0.5 * (Grad_Vel[0][2] + Grad_Vel[2][0]);
  su2double s22 = Grad_Vel[1][1];
  su2double s23 = 0.5 * (Grad_Vel[1][2] + Grad_Vel[2][1]);
  su2double s33 = Grad_Vel[2][2];
  su2double omega12 = 0.5 * (Grad_Vel[0][1] - Grad_Vel[1][0]);
  su2double omega13 = 0.5 * (Grad_Vel[0][2] - Grad_Vel[2][0]);
  su2double omega23 = 0.5 * (Grad_Vel[1][2] - Grad_Vel[2][1]);
  
  su2double Q = 2. * pow( omega12, 2.) + 2. * pow( omega13, 2.) + 2. * pow( omega23, 2.) - \
      pow( s11, 2.) - pow( s22, 2.) - pow( s33, 2.0) - 2. * pow( s12, 2.) - 2. * pow( s13, 2.) - 2. * pow( s23, 2.0);
  
  return Q;
}

bool CFlowIncOutput::SetInit_Residuals(CConfig *config){
  
  return (config->GetTime_Marching() != STEADY && (curInnerIter == 0))|| 
        (config->GetTime_Marching() == STEADY && (curTimeIter < 2)); 
  
}

bool CFlowIncOutput::SetUpdate_Averages(CConfig *config){
  return false;
  
//  return (config->GetUnsteady_Simulation() != STEADY && !dualtime);
      
}
