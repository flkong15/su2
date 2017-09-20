/*!
 * \file solver_adjoint_elasticity.cpp
 * \brief Main subroutines for solving adjoint FEM elasticity problems.
 * \author R. Sanchez
 * \version 4.1.0 "Cardinal"
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
 * Copyright (C) 2012-2016 SU2, the open-source CFD code.
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

#include "../include/solver_structure.hpp"

CDiscAdjFEASolver::CDiscAdjFEASolver(void) : CSolver (){

  nMarker = 0;
  nMarker_nL = 0;
  KindDirect_Solver = 0;

  direct_solver = NULL;
  normalLoads   = NULL;
  Sens_E        = NULL;
  Sens_Nu       = NULL;
  Sens_nL       = NULL;
  CSensitivity  = NULL;

  Solution_Vel  = NULL;
  Solution_Accel= NULL;

}

CDiscAdjFEASolver::CDiscAdjFEASolver(CGeometry *geometry, CConfig *config)  : CSolver(){

  nMarker = 0;
  nMarker_nL = 0;
  KindDirect_Solver = 0;

  direct_solver = NULL;
  normalLoads   = NULL;
  Sens_E        = NULL;
  Sens_Nu       = NULL;
  Sens_nL       = NULL;
  CSensitivity  = NULL;

  Solution_Vel  = NULL;
  Solution_Accel= NULL;

  SolRest = NULL;

}

CDiscAdjFEASolver::CDiscAdjFEASolver(CGeometry *geometry, CConfig *config, CSolver *direct_solver, unsigned short Kind_Solver, unsigned short iMesh)  : CSolver(){

  unsigned short iVar, iMarker, iDim;

  bool restart = config->GetRestart();
  bool fsi = config->GetFSI_Simulation();

  restart = false;

  unsigned long iVertex, iPoint, index;
  string text_line, mesh_filename;
  ifstream restart_file;
  string filename, AdjExt;
  su2double dull_val;

  bool dynamic = (config->GetDynamic_Analysis() == DYNAMIC);

  bool compressible = (config->GetKind_Regime() == COMPRESSIBLE);
  bool incompressible = (config->GetKind_Regime() == INCOMPRESSIBLE);

  int rank = MASTER_NODE;
#ifdef HAVE_MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif
  nVar = direct_solver->GetnVar();
  nDim = geometry->GetnDim();

  /*-- Store some information about direct solver ---*/
  this->KindDirect_Solver = Kind_Solver;
  this->direct_solver = direct_solver;

  nMarker      = config->GetnMarker_All();
  nPoint       = geometry->GetnPoint();
  nPointDomain = geometry->GetnPointDomain();

  /*-- Store number of markers with a normal load boundary condition ---*/

  normalLoads = NULL;

  nMarker_nL = 0;

//  nMarker_nL = 0;
//  for (iMarker = 0; iMarker < nMarker; iMarker++) {
//    switch (config->GetMarker_All_KindBC(iMarker)) {
//      case LOAD_BOUNDARY:
//        nMarker_nL += 1;
//        break;
//    }
//  }
//
//  normalLoads = new su2double[nMarker_nL];
//  /*--- Store the value of the normal loads ---*/
//  for (iMarker = 0; iMarker < nMarker; iMarker++) {
//    switch (config->GetMarker_All_KindBC(iMarker)) {
//      case LOAD_BOUNDARY:
//        normalLoads[iMarker] = config->GetLoad_Value(config->GetMarker_All_TagBound(iMarker));
//        break;
//    }
//  }

  /*--- Allocate the node variables ---*/

  node = new CVariable*[nPoint];

  /*--- Define some auxiliary vectors related to the residual ---*/

  Residual      = new su2double[nVar];         for (iVar = 0; iVar < nVar; iVar++) Residual[iVar]      = 1.0;
  Residual_RMS  = new su2double[nVar];         for (iVar = 0; iVar < nVar; iVar++) Residual_RMS[iVar]  = 1.0;
  Residual_Max  = new su2double[nVar];         for (iVar = 0; iVar < nVar; iVar++) Residual_Max[iVar]  = 1.0;

  /*--- Define some structures for locating max residuals ---*/

  Point_Max     = new unsigned long[nVar];  for (iVar = 0; iVar < nVar; iVar++) Point_Max[iVar]     = 0;
  Point_Max_Coord = new su2double*[nVar];
  for (iVar = 0; iVar < nVar; iVar++) {
    Point_Max_Coord[iVar] = new su2double[nDim];
    for (iDim = 0; iDim < nDim; iDim++) Point_Max_Coord[iVar][iDim] = 0.0;
  }

  /*--- Define some auxiliary vectors related to the residual for problems with a BGS strategy---*/

  if (fsi){

    Residual_BGS      = new su2double[nVar];     for (iVar = 0; iVar < nVar; iVar++) Residual_BGS[iVar]      = 1.0;
    Residual_Max_BGS  = new su2double[nVar];     for (iVar = 0; iVar < nVar; iVar++) Residual_Max_BGS[iVar]  = 1.0;

    /*--- Define some structures for locating max residuals ---*/

    Point_Max_BGS       = new unsigned long[nVar];  for (iVar = 0; iVar < nVar; iVar++) Point_Max_BGS[iVar]  = 0;
    Point_Max_Coord_BGS = new su2double*[nVar];
    for (iVar = 0; iVar < nVar; iVar++) {
      Point_Max_Coord_BGS[iVar] = new su2double[nDim];
      for (iDim = 0; iDim < nDim; iDim++) Point_Max_Coord_BGS[iVar][iDim] = 0.0;
    }

  }

  /*--- Define some auxiliary vectors related to the solution ---*/

  Solution        = new su2double[nVar];

  for (iVar = 0; iVar < nVar; iVar++) Solution[iVar]          = 1e-16;

  SolRest    = NULL;
  if (dynamic) SolRest = new su2double[3 * nVar];
  else SolRest = new su2double[nVar];

  Solution_Vel    = NULL;
  Solution_Accel  = NULL;

  if (dynamic){

    Solution_Vel    = new su2double[nVar];
    Solution_Accel  = new su2double[nVar];

    for (iVar = 0; iVar < nVar; iVar++) Solution_Vel[iVar]      = 1e-16;
    for (iVar = 0; iVar < nVar; iVar++) Solution_Accel[iVar]    = 1e-16;

  }

  /*--- Sensitivity definition and coefficient in all the markers ---*/

  CSensitivity = new su2double* [nMarker];

  for (iMarker = 0; iMarker < nMarker; iMarker++) {
      CSensitivity[iMarker]        = new su2double [geometry->nVertex[iMarker]];
  }

  Sens_E  = new su2double[nMarker];
  Sens_Nu = new su2double[nMarker];
  Sens_nL  = new su2double[nMarker];

  for (iMarker = 0; iMarker < nMarker; iMarker++) {
    Sens_E[iMarker]  = 0.0;
    Sens_Nu[iMarker] = 0.0;
    Sens_nL[iMarker]  = 0.0;
    for (iVertex = 0; iVertex < geometry->nVertex[iMarker]; iVertex++){
        CSensitivity[iMarker][iVertex] = 0.0;
    }
  }


  /*--- Check for a restart and set up the variables at each node
   appropriately. Coarse multigrid levels will be intitially set to
   the farfield values bc the solver will immediately interpolate
   the solution from the finest mesh to the coarser levels. ---*/
  if (!restart || (iMesh != MESH_0)) {

    if (dynamic){
      /*--- Restart the solution from zero ---*/
      for (iPoint = 0; iPoint < nPoint; iPoint++)
        node[iPoint] = new CDiscAdjFEAVariable(Solution, Solution_Accel, Solution_Vel, nDim, nVar, config);
    }
    else{
      /*--- Restart the solution from zero ---*/
      for (iPoint = 0; iPoint < nPoint; iPoint++)
        node[iPoint] = new CDiscAdjFEAVariable(Solution, nDim, nVar, config);
    }

  }
  else {

    /*--- Restart the solution from file information ---*/
    mesh_filename = config->GetSolution_AdjFEMFileName();
    filename = config->GetObjFunc_Extension(mesh_filename);

    restart_file.open(filename.data(), ios::in);

    /*--- In case there is no file ---*/
    if (restart_file.fail()) {
      if (rank == MASTER_NODE)
        cout << "There is no adjoint restart file!! " << filename.data() << "."<< endl;
      exit(EXIT_FAILURE);
    }

    /*--- In case this is a parallel simulation, we need to perform the
     Global2Local index transformation first. ---*/
    long *Global2Local;
    Global2Local = new long[geometry->GetGlobal_nPointDomain()];
    /*--- First, set all indices to a negative value by default ---*/
    for (iPoint = 0; iPoint < geometry->GetGlobal_nPointDomain(); iPoint++) {
      Global2Local[iPoint] = -1;
    }
    /*--- Now fill array with the transform values only for local points ---*/
    for (iPoint = 0; iPoint < nPointDomain; iPoint++) {
      Global2Local[geometry->node[iPoint]->GetGlobalIndex()] = iPoint;
    }

    /*--- Read all lines in the restart file ---*/
    long iPoint_Local; unsigned long iPoint_Global = 0;\

    /*--- Skip coordinates ---*/
    unsigned short skipVars = nDim;

    /*--- Skip flow adjoint variables ---*/
    if (Kind_Solver == RUNTIME_TURB_SYS){
      if (compressible){
        skipVars += nDim + 2;
      }
      if (incompressible){
        skipVars += nDim + 1;
      }
    }

    /*--- The first line is the header ---*/
    getline (restart_file, text_line);

    while (getline (restart_file, text_line)) {
      istringstream point_line(text_line);

      /*--- Retrieve local index. If this node from the restart file lives
       on a different processor, the value of iPoint_Local will be -1.
       Otherwise, the local index for this node on the current processor
       will be returned and used to instantiate the vars. ---*/
      iPoint_Local = Global2Local[iPoint_Global];
      if (iPoint_Local >= 0) {
        point_line >> index;
        for (iVar = 0; iVar < skipVars; iVar++){ point_line >> dull_val;}
        for (iVar = 0; iVar < nVar; iVar++){ point_line >> Solution[iVar];}
        if (dynamic){
          for (iVar = 0; iVar < nVar; iVar++){ point_line >> Solution_Vel[iVar];}
          for (iVar = 0; iVar < nVar; iVar++){ point_line >> Solution_Accel[iVar];}
          node[iPoint_Local] = new CDiscAdjFEAVariable(Solution, Solution_Accel, Solution_Vel, nDim, nVar, config);
        } else{
          node[iPoint_Local] = new CDiscAdjFEAVariable(Solution, nDim, nVar, config);
        }

      }
      iPoint_Global++;
    }

    /*--- Instantiate the variable class with an arbitrary solution
     at any halo/periodic nodes. The initial solution can be arbitrary,
     because a send/recv is performed immediately in the solver. ---*/
    if (dynamic){
      for (iPoint = nPointDomain; iPoint < nPoint; iPoint++) {
        node[iPoint] = new CDiscAdjFEAVariable(Solution, Solution_Accel, Solution_Vel, nDim, nVar, config);
      }
    }
    else{
      for (iPoint = nPointDomain; iPoint < nPoint; iPoint++) {
        node[iPoint] = new CDiscAdjFEAVariable(Solution, nDim, nVar, config);
      }
    }

    /*--- Close the restart file ---*/
    restart_file.close();

    /*--- Free memory needed for the transformation ---*/
    delete [] Global2Local;
  }

  /*--- Store the direct solution ---*/

  for (iPoint = 0; iPoint < nPoint; iPoint++){
    node[iPoint]->SetSolution_Direct(direct_solver->node[iPoint]->GetSolution());
  }

  if (dynamic){
    for (iPoint = 0; iPoint < nPoint; iPoint++){
      node[iPoint]->SetSolution_Accel_Direct(direct_solver->node[iPoint]->GetSolution_Accel());
    }

    for (iPoint = 0; iPoint < nPoint; iPoint++){
      node[iPoint]->SetSolution_Vel_Direct(direct_solver->node[iPoint]->GetSolution_Vel());
    }
  }

  /*--- Initialize the values of the total sensitivities ---*/

//  Total_Sens_E        = 0.0;
//  Total_Sens_Nu       = 0.0;
//  Total_Sens_Rho      = 0.0;
//  Total_Sens_Rho_DL   = 0.0;


  /*--- Initialize vector structures for multiple material definition ---*/

  nMPROP = config->GetnElasticityMod();

  /*--- For a material to be fully defined, we need to have the same number for all three parameters ---*/
  bool checkDef = ((config->GetnElasticityMod() == config->GetnPoissonRatio()) &&
                   (config->GetnElasticityMod() == config->GetnMaterialDensity()) &&
                   (config->GetnMaterialDensity() == config->GetnPoissonRatio()));

  if (!checkDef){
      if (rank == MASTER_NODE) cout << "WARNING: For a material to be fully defined, E, Nu and Rho need to have the same dimensions." << endl;
      exit(EXIT_FAILURE);
  }

  bool pseudo_static = config->GetPseudoStatic();

  E_i           = new su2double[nMPROP];
  Local_Sens_E  = new su2double[nMPROP];
  Global_Sens_E = new su2double[nMPROP];
  Total_Sens_E  = new su2double[nMPROP];

  Nu_i           = new su2double[nMPROP];
  Local_Sens_Nu  = new su2double[nMPROP];
  Global_Sens_Nu = new su2double[nMPROP];
  Total_Sens_Nu = new su2double[nMPROP];

  Rho_i         = new su2double[nMPROP];     // For inertial effects
  Local_Sens_Rho  = new su2double[nMPROP];
  Global_Sens_Rho = new su2double[nMPROP];
  Total_Sens_Rho  = new su2double[nMPROP];

  Rho_DL_i         = new su2double[nMPROP];     // For dead loads
  Local_Sens_Rho_DL  = new su2double[nMPROP];
  Global_Sens_Rho_DL = new su2double[nMPROP];
  Total_Sens_Rho_DL  = new su2double[nMPROP];

  for (iVar = 0; iVar < nMPROP; iVar++){
      Total_Sens_E[iVar]      = 0.0;
      Total_Sens_Nu[iVar]     = 0.0;
      Total_Sens_Rho[iVar]    = 0.0;
      Total_Sens_Rho_DL[iVar] = 0.0;
  }

  /*--- Initialize vector structures for multiple electric regions ---*/

  de_effects = config->GetDE_Effects();

  EField = NULL;
  Local_Sens_EField  = NULL;
  Global_Sens_EField = NULL;
  Total_Sens_EField  = NULL;
  if(de_effects){

    nEField = config->GetnElectric_Field();

    EField             = new su2double[nEField];
    Local_Sens_EField  = new su2double[nEField];
    Global_Sens_EField = new su2double[nEField];
    Total_Sens_EField  = new su2double[nEField];
    for (iVar = 0; iVar < nEField; iVar++){
        Total_Sens_EField[iVar] = 0.0;
    }
  }

  /*--- Initialize vector structures for structural-based design variables ---*/

  nDV = 0;
  DV_Val = NULL;
  fea_dv = false;
  switch (config->GetDV_FEA()) {
    case YOUNG_MODULUS:
    case POISSON_RATIO:
    case DENSITY_VAL:
    case DEAD_WEIGHT:
    case ELECTRIC_FIELD:
      fea_dv = true;
      break;
    default:
      fea_dv = false;
      break;
  }

  Local_Sens_DV = NULL;
  Global_Sens_DV= NULL;
  Total_Sens_DV = NULL;
  if (fea_dv){
      ReadDV(config);
      Local_Sens_DV  = new su2double[nDV];
      Global_Sens_DV = new su2double[nDV];
      Total_Sens_DV  = new su2double[nDV];
      for (iVar = 0; iVar < nDV; iVar++){
          Local_Sens_DV[iVar] = 0.0;
          Global_Sens_DV[iVar] = 0.0;
          Total_Sens_DV[iVar] = 0.0;
      }
  }

}

CDiscAdjFEASolver::~CDiscAdjFEASolver(void){

  unsigned short iMarker;

  if (CSensitivity != NULL) {
    for (iMarker = 0; iMarker < nMarker; iMarker++) {
      delete [] CSensitivity[iMarker];
    }
    delete [] CSensitivity;
  }

  if (E_i           != NULL) delete [] E_i;
  if (Nu_i          != NULL) delete [] Nu_i;
  if (Rho_i       != NULL) delete [] Rho_i;
  if (Rho_DL_i    != NULL) delete [] Rho_DL_i;

  if (Local_Sens_E         != NULL) delete [] Local_Sens_E;
  if (Local_Sens_Nu        != NULL) delete [] Local_Sens_Nu;
  if (Local_Sens_Rho       != NULL) delete [] Local_Sens_Rho;
  if (Local_Sens_Rho_DL    != NULL) delete [] Local_Sens_Rho_DL;

  if (Global_Sens_E         != NULL) delete [] Global_Sens_E;
  if (Global_Sens_Nu        != NULL) delete [] Global_Sens_Nu;
  if (Global_Sens_Rho       != NULL) delete [] Global_Sens_Rho;
  if (Global_Sens_Rho_DL    != NULL) delete [] Global_Sens_Rho_DL;

  if (Total_Sens_E         != NULL) delete [] Total_Sens_E;
  if (Total_Sens_Nu        != NULL) delete [] Total_Sens_Nu;
  if (Total_Sens_Rho       != NULL) delete [] Total_Sens_Rho;
  if (Total_Sens_Rho_DL    != NULL) delete [] Total_Sens_Rho_DL;

  if (normalLoads   != NULL) delete [] normalLoads;
  if (Sens_E        != NULL) delete [] Sens_E;
  if (Sens_Nu       != NULL) delete [] Sens_Nu;
  if (Sens_nL       != NULL) delete [] Sens_nL;

  if (EField        != NULL) delete [] EField;
  if (Local_Sens_EField        != NULL) delete [] Local_Sens_EField;
  if (Global_Sens_EField       != NULL) delete [] Global_Sens_EField;
  if (Total_Sens_EField        != NULL) delete [] Total_Sens_EField;

  if (DV_Val               != NULL) delete [] DV_Val;
  if (Local_Sens_DV        != NULL) delete [] Local_Sens_DV;
  if (Global_Sens_DV       != NULL) delete [] Global_Sens_DV;
  if (Total_Sens_DV        != NULL) delete [] Total_Sens_DV;
  if (Solution_Vel   != NULL) delete [] Solution_Vel;
  if (Solution_Accel != NULL) delete [] Solution_Accel;
  if (SolRest        != NULL) delete [] SolRest;

}

void CDiscAdjFEASolver::Set_MPI_Solution(CGeometry *geometry, CConfig *config) {


  unsigned short iVar, iMarker, MarkerS, MarkerR;
  unsigned long iVertex, iPoint, nVertexS, nVertexR, nBufferS_Vector, nBufferR_Vector;
  su2double *Buffer_Receive_U = NULL, *Buffer_Send_U = NULL;

  bool dynamic = (config->GetDynamic_Analysis() == DYNAMIC);              // Dynamic simulations.

  unsigned short nSolVar;

  int rank;
#ifdef HAVE_MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  int send_to, receive_from;
  MPI_Status status;
#endif

  for (iMarker = 0; iMarker < nMarker; iMarker++) {

    if ((config->GetMarker_All_KindBC(iMarker) == SEND_RECEIVE) &&
        (config->GetMarker_All_SendRecv(iMarker) > 0)) {

      MarkerS = iMarker;  MarkerR = iMarker+1;

#ifdef HAVE_MPI
      send_to = config->GetMarker_All_SendRecv(MarkerS)-1;
      receive_from = abs(config->GetMarker_All_SendRecv(MarkerR))-1;
#endif

      nVertexS = geometry->nVertex[MarkerS];  nVertexR = geometry->nVertex[MarkerR];
      nBufferS_Vector = nVertexS*nVar;        nBufferR_Vector = nVertexR*nVar;

      /*--- Allocate Receive and send buffers  ---*/
      Buffer_Receive_U = new su2double [nBufferR_Vector];
      Buffer_Send_U = new su2double[nBufferS_Vector];

      /*--- Copy the solution that should be sent ---*/
      for (iVertex = 0; iVertex < nVertexS; iVertex++) {
        iPoint = geometry->vertex[MarkerS][iVertex]->GetNode();
        for (iVar = 0; iVar < nVar; iVar++)
          Buffer_Send_U[iVar*nVertexS+iVertex] = node[iPoint]->GetSolution(iVar);
      }

#ifdef HAVE_MPI

      /*--- Send/Receive information using Sendrecv ---*/
      SU2_MPI::Sendrecv(Buffer_Send_U, nBufferS_Vector, MPI_DOUBLE, send_to, 0,
                        Buffer_Receive_U, nBufferR_Vector, MPI_DOUBLE, receive_from, 0, MPI_COMM_WORLD, &status);

#else

      /*--- Receive information without MPI ---*/
      for (iVertex = 0; iVertex < nVertexR; iVertex++) {
        for (iVar = 0; iVar < nVar; iVar++)
          Buffer_Receive_U[iVar*nVertexR+iVertex] = Buffer_Send_U[iVar*nVertexR+iVertex];
      }

#endif

      /*--- Deallocate send buffer ---*/
      delete [] Buffer_Send_U;

      /*--- Do the coordinate transformation ---*/
      for (iVertex = 0; iVertex < nVertexR; iVertex++) {

        /*--- Find point and its type of transformation ---*/
        iPoint = geometry->vertex[MarkerR][iVertex]->GetNode();

        /*--- Copy solution variables. ---*/
        for (iVar = 0; iVar < nVar; iVar++)
          Solution[iVar] = Buffer_Receive_U[iVar*nVertexR+iVertex];

        /*--- Store received values back into the variable. ---*/
        for (iVar = 0; iVar < nVar; iVar++)
          node[iPoint]->SetSolution(iVar, Solution[iVar]);

      }

      /*--- Deallocate receive buffer ---*/
      delete [] Buffer_Receive_U;

    }

  }

}

void CDiscAdjFEASolver::Set_MPI_CrossTerm(CGeometry *geometry, CConfig *config) {


  unsigned short iVar, iMarker, MarkerS, MarkerR;
  unsigned long iVertex, iPoint, nVertexS, nVertexR, nBufferS_Vector, nBufferR_Vector;
  su2double *Buffer_Receive_U = NULL, *Buffer_Send_U = NULL;

  bool dynamic = (config->GetDynamic_Analysis() == DYNAMIC);              // Dynamic simulations.

  unsigned short nSolVar;

  int rank;
#ifdef HAVE_MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  int send_to, receive_from;
  MPI_Status status;
#endif

  for (iMarker = 0; iMarker < nMarker; iMarker++) {

    if ((config->GetMarker_All_KindBC(iMarker) == SEND_RECEIVE) &&
        (config->GetMarker_All_SendRecv(iMarker) > 0)) {

      MarkerS = iMarker;  MarkerR = iMarker+1;

#ifdef HAVE_MPI
      send_to = config->GetMarker_All_SendRecv(MarkerS)-1;
      receive_from = abs(config->GetMarker_All_SendRecv(MarkerR))-1;
#endif

      nVertexS = geometry->nVertex[MarkerS];  nVertexR = geometry->nVertex[MarkerR];
      nBufferS_Vector = nVertexS*nVar;        nBufferR_Vector = nVertexR*nVar;

      /*--- Allocate Receive and send buffers  ---*/
      Buffer_Receive_U = new su2double [nBufferR_Vector];
      Buffer_Send_U = new su2double[nBufferS_Vector];

      /*--- Copy the solution that should be sent ---*/
      for (iVertex = 0; iVertex < nVertexS; iVertex++) {
        iPoint = geometry->vertex[MarkerS][iVertex]->GetNode();
        for (iVar = 0; iVar < nVar; iVar++)
          Buffer_Send_U[iVar*nVertexS+iVertex] = node[iPoint]->GetCross_Term_Derivative(iVar);
      }

#ifdef HAVE_MPI

      /*--- Send/Receive information using Sendrecv ---*/
      SU2_MPI::Sendrecv(Buffer_Send_U, nBufferS_Vector, MPI_DOUBLE, send_to, 0,
                        Buffer_Receive_U, nBufferR_Vector, MPI_DOUBLE, receive_from, 0, MPI_COMM_WORLD, &status);

#else

      /*--- Receive information without MPI ---*/
      for (iVertex = 0; iVertex < nVertexR; iVertex++) {
        for (iVar = 0; iVar < nVar; iVar++)
          Buffer_Receive_U[iVar*nVertexR+iVertex] = Buffer_Send_U[iVar*nVertexR+iVertex];
      }

#endif

      /*--- Deallocate send buffer ---*/
      delete [] Buffer_Send_U;

      /*--- Do the coordinate transformation ---*/
      for (iVertex = 0; iVertex < nVertexR; iVertex++) {

        /*--- Find point and its type of transformation ---*/
        iPoint = geometry->vertex[MarkerR][iVertex]->GetNode();

        /*--- Copy solution variables. ---*/
        for (iVar = 0; iVar < nVar; iVar++)
          Solution[iVar] = Buffer_Receive_U[iVar*nVertexR+iVertex];

        /*--- Store received values back into the variable. ---*/
        for (iVar = 0; iVar < nVar; iVar++)
          node[iPoint]->SetCross_Term_Derivative(iVar, Solution[iVar]);

      }

      /*--- Deallocate receive buffer ---*/
      delete [] Buffer_Receive_U;

    }

  }

}

void CDiscAdjFEASolver::Set_MPI_CrossTerm_Geometry(CGeometry *geometry, CConfig *config) {


  unsigned short iVar, iMarker, MarkerS, MarkerR;
  unsigned long iVertex, iPoint, nVertexS, nVertexR, nBufferS_Vector, nBufferR_Vector;
  su2double *Buffer_Receive_U = NULL, *Buffer_Send_U = NULL;

  bool dynamic = (config->GetDynamic_Analysis() == DYNAMIC);              // Dynamic simulations.

  unsigned short nSolVar;

  int rank;
#ifdef HAVE_MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  int send_to, receive_from;
  MPI_Status status;
#endif

  for (iMarker = 0; iMarker < nMarker; iMarker++) {

    if ((config->GetMarker_All_KindBC(iMarker) == SEND_RECEIVE) &&
        (config->GetMarker_All_SendRecv(iMarker) > 0)) {

      MarkerS = iMarker;  MarkerR = iMarker+1;

#ifdef HAVE_MPI
      send_to = config->GetMarker_All_SendRecv(MarkerS)-1;
      receive_from = abs(config->GetMarker_All_SendRecv(MarkerR))-1;
#endif

      nVertexS = geometry->nVertex[MarkerS];  nVertexR = geometry->nVertex[MarkerR];
      nBufferS_Vector = nVertexS*nVar;        nBufferR_Vector = nVertexR*nVar;

      /*--- Allocate Receive and send buffers  ---*/
      Buffer_Receive_U = new su2double [nBufferR_Vector];
      Buffer_Send_U = new su2double[nBufferS_Vector];

      /*--- Copy the solution that should be sent ---*/
      for (iVertex = 0; iVertex < nVertexS; iVertex++) {
        iPoint = geometry->vertex[MarkerS][iVertex]->GetNode();
        for (iVar = 0; iVar < nVar; iVar++)
          Buffer_Send_U[iVar*nVertexS+iVertex] = node[iPoint]->GetGeometry_CrossTerm_Derivative(iVar);
      }

#ifdef HAVE_MPI

      /*--- Send/Receive information using Sendrecv ---*/
      SU2_MPI::Sendrecv(Buffer_Send_U, nBufferS_Vector, MPI_DOUBLE, send_to, 0,
                        Buffer_Receive_U, nBufferR_Vector, MPI_DOUBLE, receive_from, 0, MPI_COMM_WORLD, &status);

#else

      /*--- Receive information without MPI ---*/
      for (iVertex = 0; iVertex < nVertexR; iVertex++) {
        for (iVar = 0; iVar < nVar; iVar++)
          Buffer_Receive_U[iVar*nVertexR+iVertex] = Buffer_Send_U[iVar*nVertexR+iVertex];
      }

#endif

      /*--- Deallocate send buffer ---*/
      delete [] Buffer_Send_U;

      /*--- Do the coordinate transformation ---*/
      for (iVertex = 0; iVertex < nVertexR; iVertex++) {

        /*--- Find point and its type of transformation ---*/
        iPoint = geometry->vertex[MarkerR][iVertex]->GetNode();

        /*--- Copy solution variables. ---*/
        for (iVar = 0; iVar < nVar; iVar++)
          Solution[iVar] = Buffer_Receive_U[iVar*nVertexR+iVertex];

        /*--- Store received values back into the variable. ---*/
        for (iVar = 0; iVar < nVar; iVar++)
          node[iPoint]->SetGeometry_CrossTerm_Derivative(iVar, Solution[iVar]);

      }

      /*--- Deallocate receive buffer ---*/
      delete [] Buffer_Receive_U;

    }

  }

}

void CDiscAdjFEASolver::SetRecording(CGeometry* geometry, CConfig *config, unsigned short kind_recording){


  bool dynamic (config->GetDynamic_Analysis() == DYNAMIC);

  unsigned long iPoint;
  unsigned short iVar;

  /*--- Reset the solution to the initial (converged) solution ---*/

  for (iPoint = 0; iPoint < nPoint; iPoint++){
    direct_solver->node[iPoint]->SetSolution(node[iPoint]->GetSolution_Direct());
  }

  if (dynamic){
    /*--- Reset the solution to the initial (converged) solution ---*/

    for (iPoint = 0; iPoint < nPoint; iPoint++){
      direct_solver->node[iPoint]->SetSolution_Accel(node[iPoint]->GetSolution_Accel_Direct());
    }

    for (iPoint = 0; iPoint < nPoint; iPoint++){
      direct_solver->node[iPoint]->SetSolution_Vel(node[iPoint]->GetSolution_Vel_Direct());
    }

    /*--- Reset the input for time n ---*/

    for (iPoint = 0; iPoint < nPoint; iPoint++){
      for (iVar = 0; iVar < nVar; iVar++){
        AD::ResetInput(direct_solver->node[iPoint]->Get_femSolution_time_n()[iVar]);
      }
    }
    for (iPoint = 0; iPoint < nPoint; iPoint++){
      for (iVar = 0; iVar < nVar; iVar++){
        AD::ResetInput(direct_solver->node[iPoint]->GetSolution_Accel_time_n()[iVar]);
      }
    }
    for (iPoint = 0; iPoint < nPoint; iPoint++){
      for (iVar = 0; iVar < nVar; iVar++){
        AD::ResetInput(direct_solver->node[iPoint]->GetSolution_Vel_time_n()[iVar]);
      }
    }

  }

  /*--- Set the Jacobian to zero since this is not done inside the meanflow iteration
   * when running the discrete adjoint solver. ---*/

  direct_solver->Jacobian.SetValZero();

  /*--- Set indices to zero ---*/

  RegisterVariables(geometry, config, true);

}

void CDiscAdjFEASolver::RegisterSolution(CGeometry *geometry, CConfig *config){

  unsigned long iPoint, nPoint = geometry->GetnPoint();

  bool dynamic (config->GetDynamic_Analysis() == DYNAMIC);
  bool input = true;

  /*--- Register solution at all necessary time instances and other variables on the tape ---*/

  for (iPoint = 0; iPoint < nPoint; iPoint++){
    direct_solver->node[iPoint]->RegisterSolution(input);
  }

  if (dynamic){
    /*--- Register acceleration (u'') and velocity (u') at time step n ---*/
    for (iPoint = 0; iPoint < nPoint; iPoint++){
      direct_solver->node[iPoint]->RegisterSolution_Accel(input);
    }
    for (iPoint = 0; iPoint < nPoint; iPoint++){
      direct_solver->node[iPoint]->RegisterSolution_Vel(input);
    }
    /*--- Register solution (u), acceleration (u'') and velocity (u') at time step n-1 ---*/
    for (iPoint = 0; iPoint < nPoint; iPoint++){
      direct_solver->node[iPoint]->Register_femSolution_time_n();
    }
    for (iPoint = 0; iPoint < nPoint; iPoint++){
      direct_solver->node[iPoint]->RegisterSolution_Accel_time_n();
    }
    for (iPoint = 0; iPoint < nPoint; iPoint++){
      direct_solver->node[iPoint]->RegisterSolution_Vel_time_n();
    }
  }

}

void CDiscAdjFEASolver::RegisterVariables(CGeometry *geometry, CConfig *config, bool reset){

  /*--- Register element-based values as input ---*/

  unsigned short iVar;

  if (KindDirect_Solver == RUNTIME_FEA_SYS) {

    bool pseudo_static = config->GetPseudoStatic();

    for (iVar = 0; iVar < nMPROP; iVar++){
        E_i[iVar]         = config->GetElasticyMod(iVar);
        Nu_i[iVar]        = config->GetPoissonRatio(iVar);
        Rho_DL_i[iVar]  = config->GetMaterialDensity(iVar);
        if (pseudo_static) Rho_i[iVar] = 0.0;
        else               Rho_i[iVar] = config->GetMaterialDensity(iVar);
    }

    /*--- Read the values of the electric field ---*/
    if(de_effects){
        for (iVar = 0; iVar < nEField; iVar++) EField[iVar] = config->Get_Electric_Field_Mod(iVar);
    }

//    if(fea_dv){
//        for (iVar = 0; iVar < nDV; iVar++) DV_Val[iVar] = config->GetDV_Value(iVar,0);
//    }

    if (!reset){
        for (iVar = 0; iVar < nMPROP; iVar++) AD::RegisterInput(E_i[iVar]);
        for (iVar = 0; iVar < nMPROP; iVar++) AD::RegisterInput(Nu_i[iVar]);
        for (iVar = 0; iVar < nMPROP; iVar++) AD::RegisterInput(Rho_i[iVar]);
        for (iVar = 0; iVar < nMPROP; iVar++) AD::RegisterInput(Rho_DL_i[iVar]);

        if(de_effects){
          for (iVar = 0; iVar < nEField; iVar++) AD::RegisterInput(EField[iVar]);
        }

        if(fea_dv){
          for (iVar = 0; iVar < nDV; iVar++) AD::RegisterInput(DV_Val[iVar]);
        }

    }

  }


    /*--- Here it is possible to register other variables as input that influence the flow solution
     * and thereby also the objective function. The adjoint values (i.e. the derivatives) can be
     * extracted in the ExtractAdjointVariables routine. ---*/
}

void CDiscAdjFEASolver::RegisterOutput(CGeometry *geometry, CConfig *config){

  unsigned long iPoint, nPoint = geometry->GetnPoint();

  bool dynamic (config->GetDynamic_Analysis() == DYNAMIC);

  /*--- Register variables as output of the solver iteration ---*/

  bool input = false;

  /*--- Register output variables on the tape ---*/

  for (iPoint = 0; iPoint < nPoint; iPoint++){
    direct_solver->node[iPoint]->RegisterSolution(input);
  }
  if (dynamic){
    /*--- Register acceleration (u'') and velocity (u') at time step n ---*/
    for (iPoint = 0; iPoint < nPoint; iPoint++){
      direct_solver->node[iPoint]->RegisterSolution_Accel(input);
    }
    for (iPoint = 0; iPoint < nPoint; iPoint++){
      direct_solver->node[iPoint]->RegisterSolution_Vel(input);
    }
  }


}

void CDiscAdjFEASolver::RegisterObj_Func(CConfig *config){

  int rank = MASTER_NODE;
#ifdef HAVE_MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif

  /*--- Here we can add new (scalar) objective functions ---*/

  switch (config->GetKind_ObjFunc()){
  case REFERENCE_GEOMETRY:
      ObjFunc_Value = direct_solver->GetTotal_OFRefGeom();
      break;
  case REFERENCE_NODE:
      ObjFunc_Value = direct_solver->GetTotal_OFRefNode();
      break;
  default:
      ObjFunc_Value = 0.0;  // If the objective function is computed in a different physical problem
      break;
 /*--- Template for new objective functions where TemplateObjFunction()
  *  is the routine that returns the obj. function value. The computation
  * must be done while the tape is active, i.e. between AD::StartRecording() and
  * AD::StopRecording() in DiscAdjMeanFlowIteration::Iterate(). The best place is somewhere
  * inside MeanFlowIteration::Iterate().
  *
  * case TEMPLATE_OBJECTIVE:
  *    ObjFunc_Value = TemplateObjFunction();
  *    break;
  * ---*/
  }
  if (rank == MASTER_NODE){
    AD::RegisterOutput(ObjFunc_Value);
  }
}


void CDiscAdjFEASolver::SetAdj_ObjFunc(CGeometry *geometry, CConfig *config){
  int rank = MASTER_NODE;

  bool dynamic = (config->GetDynamic_Analysis() == DYNAMIC);
  unsigned long IterAvg_Obj = config->GetIter_Avg_Objective();
  unsigned long ExtIter = config->GetExtIter();
  su2double seeding = 1.0;

  if (dynamic){
    if (ExtIter < IterAvg_Obj){
      seeding = 1.0/((su2double)IterAvg_Obj);
    }
    else{
      seeding = 0.0;
    }
  }

#ifdef HAVE_MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif

  if (rank == MASTER_NODE){
    SU2_TYPE::SetDerivative(ObjFunc_Value, SU2_TYPE::GetValue(seeding));
  } else {
    SU2_TYPE::SetDerivative(ObjFunc_Value, 0.0);
  }
}

void CDiscAdjFEASolver::ExtractAdjoint_Solution(CGeometry *geometry, CConfig *config){

  bool dynamic = (config->GetDynamic_Analysis() == DYNAMIC);

  unsigned short iVar;
  unsigned long iPoint;
  su2double residual;

  int rank = MASTER_NODE;
#ifdef HAVE_MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif

  /*--- Set Residuals to zero ---*/

  for (iVar = 0; iVar < nVar; iVar++){
      SetRes_RMS(iVar,0.0);
      SetRes_Max(iVar,0.0,0);
  }

  for (iPoint = 0; iPoint < nPoint; iPoint++){

    /*--- Set the old solution ---*/

    node[iPoint]->Set_OldSolution();

    /*--- Extract the adjoint solution ---*/

    direct_solver->node[iPoint]->GetAdjointSolution(Solution);

    /*--- Store the adjoint solution ---*/

    node[iPoint]->SetSolution(Solution);

  }

  /*--- Solution for acceleration (u'') and velocity (u') at time n ---*/

  if (dynamic){

    /*--- FIRST: The acceleration solution ---*/
    for (iPoint = 0; iPoint < nPoint; iPoint++){

      /*--- Set the old acceleration solution ---*/

      node[iPoint]->Set_OldSolution_Accel();

      /*--- Extract the adjoint acceleration solution u'' ---*/

      direct_solver->node[iPoint]->GetAdjointSolution_Accel(Solution_Accel);

      /*--- Store the adjoint acceleration solution u'' ---*/

      node[iPoint]->SetSolution_Accel(Solution_Accel);

    }

    /*--- NEXT: The velocity solution ---*/
    for (iPoint = 0; iPoint < nPoint; iPoint++){

      /*--- Set the old velocity solution ---*/

      node[iPoint]->Set_OldSolution_Vel();

      /*--- Extract the adjoint velocity solution u'' ---*/

      direct_solver->node[iPoint]->GetAdjointSolution_Vel(Solution_Vel);

      /*--- Store the adjoint velocity solution u'' ---*/

      node[iPoint]->SetSolution_Vel(Solution_Vel);

    }

    /*--- NOW: The solution at time n ---*/
    for (iPoint = 0; iPoint < nPoint; iPoint++){

      /*--- Extract the adjoint solution at time n ---*/

      direct_solver->node[iPoint]->GetAdjointSolution_time_n(Solution);

      /*--- Store the adjoint solution at time n ---*/

      node[iPoint]->SetSolution_time_n(Solution);
    }

    /*--- The acceleration solution at time n... ---*/
    for (iPoint = 0; iPoint < nPoint; iPoint++){

      /*--- Extract the adjoint acceleration solution u'' at time n ---*/

      direct_solver->node[iPoint]->GetAdjointSolution_Accel_time_n(Solution_Accel);

      /*--- Store the adjoint acceleration solution u'' at time n---*/

      node[iPoint]->SetSolution_Accel_time_n(Solution_Accel);

    }

    /*--- ... and the velocity solution at time n ---*/
    for (iPoint = 0; iPoint < nPoint; iPoint++){

      /*--- Extract the adjoint velocity solution u' at time n ---*/

      direct_solver->node[iPoint]->GetAdjointSolution_Vel_time_n(Solution_Vel);

      /*--- Store the adjoint velocity solution u' at time n ---*/

      node[iPoint]->SetSolution_Vel_time_n(Solution_Vel);

    }

  }

  /*--- Set MPI solution ---*/
//  Set_MPI_Solution(geometry, config);

//  for (iPoint = 0; iPoint < nPoint; iPoint++){
//
//    if (geometry->node[iPoint]->GetGlobalIndex() == 276){
//      cout << rank << " " << iPoint << " " << geometry->node[iPoint]->GetGlobalIndex() << " " << node[iPoint]->GetSolution(0) << " " << node[iPoint]->GetSolution(1) << endl;
//    }
//
//  }

  /*--- TODO: Need to set the MPI solution in the previous TS ---*/

  /*--- Set the residuals ---*/

  for (iPoint = 0; iPoint < nPointDomain; iPoint++){
      for (iVar = 0; iVar < nVar; iVar++){
          residual = node[iPoint]->GetSolution(iVar) - node[iPoint]->GetSolution_Old(iVar);

          AddRes_RMS(iVar,residual*residual);
          AddRes_Max(iVar,fabs(residual),geometry->node[iPoint]->GetGlobalIndex(),geometry->node[iPoint]->GetCoord());
      }
      if (dynamic){
        for (iVar = 0; iVar < nVar; iVar++){
            residual = node[iPoint]->GetSolution_Accel(iVar) - node[iPoint]->GetSolution_Old_Accel(iVar);

            AddRes_RMS(iVar,residual*residual);
            AddRes_Max(iVar,fabs(residual),geometry->node[iPoint]->GetGlobalIndex(),geometry->node[iPoint]->GetCoord());
        }
        for (iVar = 0; iVar < nVar; iVar++){
            residual = node[iPoint]->GetSolution_Vel(iVar) - node[iPoint]->GetSolution_Old_Vel(iVar);

            AddRes_RMS(iVar,residual*residual);
            AddRes_Max(iVar,fabs(residual),geometry->node[iPoint]->GetGlobalIndex(),geometry->node[iPoint]->GetCoord());
        }
      }
  }

  SetResidual_RMS(geometry, config);

}

void CDiscAdjFEASolver::ExtractAdjoint_Variables(CGeometry *geometry, CConfig *config){

//  su2double Local_Sens_E, Local_Sens_Nu, Local_Sens_Rho, Local_Sens_Rho_DL;
//
//  /*--- Extract the adjoint values of the farfield values ---*/
//
//  if (KindDirect_Solver == RUNTIME_FEA_SYS){
//    Local_Sens_E  = SU2_TYPE::GetDerivative(E);
//    Local_Sens_Nu   = SU2_TYPE::GetDerivative(Nu);
//    Local_Sens_Rho  = SU2_TYPE::GetDerivative(Rho);
//    Local_Sens_Rho_DL   = SU2_TYPE::GetDerivative(Rho_DL);
//
//#ifdef HAVE_MPI
//    SU2_MPI::Allreduce(&Local_Sens_E,  &Global_Sens_E,  1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
//    SU2_MPI::Allreduce(&Local_Sens_Nu, &Global_Sens_Nu, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
//    SU2_MPI::Allreduce(&Local_Sens_Rho,  &Global_Sens_Rho,  1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
//    SU2_MPI::Allreduce(&Local_Sens_Rho_DL, &Global_Sens_Rho_DL, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
//#else
//    Global_Sens_E        = Local_Sens_E;
//    Global_Sens_Nu       = Local_Sens_Nu;
//    Global_Sens_Rho      = Local_Sens_Rho;
//    Global_Sens_Rho_DL   = Local_Sens_Rho_DL;
//#endif
//
//    /*--- Extract here the adjoint values of the electric field in the case that it is a design variable. ---*/
//
//    if(de_effects){
//
//      for (unsigned short i_DV = 0; i_DV < n_EField; i_DV++)
//        Local_Sens_EField[i_DV] = SU2_TYPE::GetDerivative(EField[i_DV]);
//
//  #ifdef HAVE_MPI
//      SU2_MPI::Allreduce(Local_Sens_EField,  Global_Sens_EField, n_EField, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
//  #else
//      for (unsigned short i_DV = 0; i_DV < n_EField; i_DV++) Global_Sens_EField[i_DV] = Local_Sens_EField[i_DV];
//  #endif
//
//    }
//
//  }

  unsigned short iVar;

  /*--- Extract the adjoint values of the farfield values ---*/

  if (KindDirect_Solver == RUNTIME_FEA_SYS){

    for (iVar = 0; iVar < nMPROP; iVar++) Local_Sens_E[iVar]  = SU2_TYPE::GetDerivative(E_i[iVar]);
    for (iVar = 0; iVar < nMPROP; iVar++) Local_Sens_Nu[iVar] = SU2_TYPE::GetDerivative(Nu_i[iVar]);
    for (iVar = 0; iVar < nMPROP; iVar++) Local_Sens_Rho[iVar] = SU2_TYPE::GetDerivative(Rho_i[iVar]);
    for (iVar = 0; iVar < nMPROP; iVar++) Local_Sens_Rho_DL[iVar] = SU2_TYPE::GetDerivative(Rho_DL_i[iVar]);

#ifdef HAVE_MPI
    SU2_MPI::Allreduce(Local_Sens_E,  Global_Sens_E,  nMPROP, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
    SU2_MPI::Allreduce(Local_Sens_Nu, Global_Sens_Nu, nMPROP, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
    SU2_MPI::Allreduce(Local_Sens_Rho, Global_Sens_Rho, nMPROP, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
    SU2_MPI::Allreduce(Local_Sens_Rho_DL, Global_Sens_Rho_DL, nMPROP, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
#else
    for (iVar = 0; iVar < nMPROP; iVar++) Global_Sens_E[iVar]        = Local_Sens_E[iVar];
    for (iVar = 0; iVar < nMPROP; iVar++) Global_Sens_Nu[iVar]       = Local_Sens_Nu[iVar];
    for (iVar = 0; iVar < nMPROP; iVar++) Global_Sens_Rho[iVar]      = Local_Sens_Rho[iVar];
    for (iVar = 0; iVar < nMPROP; iVar++) Global_Sens_Rho_DL[iVar]   = Local_Sens_Rho_DL[iVar];
#endif

    /*--- Extract here the adjoint values of the electric field in the case that it is a parameter of the problem. ---*/

    if(de_effects){

      for (iVar = 0; iVar < nEField; iVar++) Local_Sens_EField[iVar] = SU2_TYPE::GetDerivative(EField[iVar]);

  #ifdef HAVE_MPI
      SU2_MPI::Allreduce(Local_Sens_EField,  Global_Sens_EField, nEField, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
  #else
      for (iVar = 0; iVar < nEField; iVar++) Global_Sens_EField[iVar] = Local_Sens_EField[iVar];
  #endif

    }

    if(fea_dv){

      for (iVar = 0; iVar < nDV; iVar++) Local_Sens_DV[iVar] = SU2_TYPE::GetDerivative(DV_Val[iVar]);

  #ifdef HAVE_MPI
      SU2_MPI::Allreduce(Local_Sens_DV,  Global_Sens_DV, nDV, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
  #else
      for (iVar = 0; iVar < nDV; iVar++) Global_Sens_DV[iVar] = Local_Sens_DV[iVar];
  #endif

    }


  }


}

void CDiscAdjFEASolver::SetAdjoint_Output(CGeometry *geometry, CConfig *config){

  bool dynamic = (config->GetDynamic_Analysis() == DYNAMIC);
  bool fsi = config->GetFSI_Simulation();

  unsigned short iVar;
  unsigned long iPoint;

  for (iPoint = 0; iPoint < nPoint; iPoint++){
    for (iVar = 0; iVar < nVar; iVar++){
      Solution[iVar] = node[iPoint]->GetSolution(iVar);
    }
    if (fsi) {
      for (iVar = 0; iVar < nVar; iVar++){
        Solution[iVar] += node[iPoint]->GetGeometry_CrossTerm_Derivative(iVar);
      }
      for (iVar = 0; iVar < nVar; iVar++){
        Solution[iVar] += node[iPoint]->GetCross_Term_Derivative(iVar);
      }
    }

    if (dynamic){
      for (iVar = 0; iVar < nVar; iVar++){
        Solution_Accel[iVar] = node[iPoint]->GetSolution_Accel(iVar);
      }
      for (iVar = 0; iVar < nVar; iVar++){
        Solution_Vel[iVar] = node[iPoint]->GetSolution_Vel(iVar);
      }
      for (iVar = 0; iVar < nVar; iVar++){
        Solution[iVar] += node[iPoint]->GetDynamic_Derivative_n(iVar);
      }
      for (iVar = 0; iVar < nVar; iVar++){
        Solution_Accel[iVar] += node[iPoint]->GetDynamic_Derivative_Accel_n(iVar);
      }
      for (iVar = 0; iVar < nVar; iVar++){
        Solution_Vel[iVar] += node[iPoint]->GetDynamic_Derivative_Vel_n(iVar);
      }
    }
    direct_solver->node[iPoint]->SetAdjointSolution(Solution);
    if (dynamic){
      direct_solver->node[iPoint]->SetAdjointSolution_Accel(Solution_Accel);
      direct_solver->node[iPoint]->SetAdjointSolution_Vel(Solution_Vel);
    }

  }

}

void CDiscAdjFEASolver::Preprocessing(CGeometry *geometry, CSolver **solver_container, CConfig *config_container, unsigned short iMesh, unsigned short iRKStep, unsigned short RunTime_EqSystem, bool Output){

  bool dynamic = (config_container->GetDynamic_Analysis() == DYNAMIC);
  unsigned long iPoint;
  unsigned short iVar;

  if (dynamic){
      for (iPoint = 0; iPoint<geometry->GetnPoint(); iPoint++){
          for (iVar=0; iVar < nVar; iVar++){
              node[iPoint]->SetDynamic_Derivative_n(iVar, node[iPoint]->GetSolution_time_n(iVar));
          }
          for (iVar=0; iVar < nVar; iVar++){
              node[iPoint]->SetDynamic_Derivative_Accel_n(iVar, node[iPoint]->GetSolution_Accel_time_n(iVar));
          }
          for (iVar=0; iVar < nVar; iVar++){
              node[iPoint]->SetDynamic_Derivative_Vel_n(iVar, node[iPoint]->GetSolution_Vel_time_n(iVar));
          }
        }
    }

}

void CDiscAdjFEASolver::ExtractAdjoint_CrossTerm(CGeometry *geometry, CConfig *config){

  unsigned short iVar;
  unsigned long iPoint;
  su2double residual;

  for (iPoint = 0; iPoint < nPoint; iPoint++){

    /*--- Extract the adjoint solution ---*/

    direct_solver->node[iPoint]->GetAdjointSolution(Solution);

    for (iVar = 0; iVar < nVar; iVar++) node[iPoint]->SetCross_Term_Derivative(iVar, Solution[iVar]);

  }


}

void CDiscAdjFEASolver::ExtractAdjoint_CrossTerm_Geometry(CGeometry *geometry, CConfig *config){

  unsigned short iVar;
  unsigned long iPoint;
  su2double residual;

  int rank = MASTER_NODE;
#ifdef HAVE_MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif

  for (iPoint = 0; iPoint < nPoint; iPoint++){

    /*--- Extract the adjoint solution ---*/

    direct_solver->node[iPoint]->GetAdjointSolution(Solution);

    for (iVar = 0; iVar < nVar; iVar++) node[iPoint]->SetGeometry_CrossTerm_Derivative(iVar, Solution[iVar]);

  }

//  Set_MPI_CrossTerm_Geometry(geometry, config);

}


void CDiscAdjFEASolver::SetZeroAdj_ObjFunc(CGeometry *geometry, CConfig *config){

  int rank = MASTER_NODE;

  su2double seeding = 0.0;

#ifdef HAVE_MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif

  if (rank == MASTER_NODE){
    SU2_TYPE::SetDerivative(ObjFunc_Value, SU2_TYPE::GetValue(seeding));
  } else {
    SU2_TYPE::SetDerivative(ObjFunc_Value, 0.0);
  }
}


void CDiscAdjFEASolver::SetSensitivity(CGeometry *geometry, CConfig *config){

  unsigned short iVar;

  for (iVar = 0; iVar < nMPROP; iVar++){
    Total_Sens_E[iVar]        += Global_Sens_E[iVar];
    Total_Sens_Nu[iVar]       += Global_Sens_Nu[iVar];
    Total_Sens_Rho[iVar]      += Global_Sens_Rho[iVar];
    Total_Sens_Rho_DL[iVar]   += Global_Sens_Rho_DL[iVar];
  }

  if (de_effects){
      for (iVar = 0; iVar < nEField; iVar++)
        Total_Sens_EField[iVar]+= Global_Sens_EField[iVar];
  }

  if (fea_dv){
      for (iVar = 0; iVar < nDV; iVar++){
          Total_Sens_DV[iVar] += Global_Sens_DV[iVar];
      }
  }


}

void CDiscAdjFEASolver::SetSurface_Sensitivity(CGeometry *geometry, CConfig *config){


}

void CDiscAdjFEASolver::ComputeResidual_BGS(CGeometry *geometry, CConfig *config){

  unsigned short iVar;
  unsigned long iPoint;
  su2double residual, bgs_sol;

  /*--- Set Residuals to zero ---*/

  for (iVar = 0; iVar < nVar; iVar++){
      SetRes_BGS(iVar,0.0);
      SetRes_Max_BGS(iVar,0.0,0);
  }

  /*--- Compute the BGS solution (adding the cross term) ---*/
  for (iPoint = 0; iPoint < nPointDomain; iPoint++){
    for (iVar = 0; iVar < nVar; iVar++){
      bgs_sol = node[iPoint]->GetSolution(iVar) + node[iPoint]->GetGeometry_CrossTerm_Derivative(iVar);
      node[iPoint]->Set_BGSSolution(iVar, bgs_sol);
    }
  }

  /*--- Set the residuals ---*/
  for (iPoint = 0; iPoint < nPointDomain; iPoint++){
      for (iVar = 0; iVar < nVar; iVar++){
          residual = node[iPoint]->Get_BGSSolution(iVar) - node[iPoint]->Get_BGSSolution_k(iVar);
          AddRes_BGS(iVar,residual*residual);
          AddRes_Max_BGS(iVar,fabs(residual),geometry->node[iPoint]->GetGlobalIndex(),geometry->node[iPoint]->GetCoord());
      }
  }

  SetResidual_BGS(geometry, config);

}


void CDiscAdjFEASolver::UpdateSolution_BGS(CGeometry *geometry, CConfig *config){

  unsigned long iPoint;

  /*--- To nPoint: The solution must be communicated beforehand ---*/
  for (iPoint = 0; iPoint < nPoint; iPoint++){

    node[iPoint]->Set_BGSSolution_k();

  }

}

void CDiscAdjFEASolver::BC_Clamped_Post(CGeometry *geometry, CSolver **solver_container, CNumerics *numerics, CConfig *config,
                                            unsigned short val_marker) {

  unsigned long iPoint, iVertex;
  bool dynamic = (config->GetDynamic_Analysis() == DYNAMIC);

  for (iVertex = 0; iVertex < geometry->nVertex[val_marker]; iVertex++) {

    /*--- Get node index ---*/

    iPoint = geometry->vertex[val_marker][iVertex]->GetNode();

    if (nDim == 2) {
      Solution[0] = 0.0;  Solution[1] = 0.0;
    }
    else {
      Solution[0] = 0.0;  Solution[1] = 0.0;  Solution[2] = 0.0;
    }

    node[iPoint]->SetSolution(Solution);

    if (dynamic){
      node[iPoint]->SetSolution_Vel(Solution);
      node[iPoint]->SetSolution_Accel(Solution);
    }

  }

}

void CDiscAdjFEASolver::ReadDV(CConfig *config) {

  unsigned long iElem;
  unsigned long index;

  unsigned short iVar;
  unsigned short iZone = config->GetiZone();

  string filename;
  ifstream properties_file;

  int rank = MASTER_NODE;
#ifdef HAVE_MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif

  /*--- Choose the filename of the design variable ---*/

  string input_name;

  switch (config->GetDV_FEA()) {
    case YOUNG_MODULUS:
      input_name = "dv_young.opt";
      break;
    case POISSON_RATIO:
      input_name = "dv_poisson.opt";
      break;
    case DENSITY_VAL:
    case DEAD_WEIGHT:
      input_name = "dv_density.opt";
      break;
    case ELECTRIC_FIELD:
      input_name = "dv_efield.opt";
      break;
    default:
      input_name = "dv.opt";
      break;
  }

  filename = input_name;

  if (rank == MASTER_NODE) cout << "Filename: " << filename << "." << endl;

  properties_file.open(filename.data(), ios::in);

  /*--- In case there is no file, all elements get the same property (0) ---*/

  if (properties_file.fail()) {

    if (rank == MASTER_NODE)
      cout << "There is no design variable file." << endl;

    nDV   = 1;
    DV_Val = new su2double[nDV];
    for (unsigned short iDV = 0; iDV < nDV; iDV++)
      DV_Val[iDV] = 1.0;

  }
  else{

    string text_line;

     /*--- First pass: determine number of design variables ---*/

    unsigned short iDV = 0;

    /*--- Skip the first line: it is the header ---*/

    getline (properties_file, text_line);

    while (getline (properties_file, text_line)) iDV++;

    /*--- Close the restart file ---*/

    properties_file.close();

    nDV = iDV;
    DV_Val = new su2double[nDV];

    /*--- Reopen the file (TODO: improve this) ---*/

    properties_file.open(filename.data(), ios::in);

    /*--- Skip the first line: it is the header ---*/

    getline (properties_file, text_line);

    iDV = 0;
    while (getline (properties_file, text_line)) {

      istringstream point_line(text_line);

      point_line >> index >> DV_Val[iDV];

      iDV++;

    }

    /*--- Close the restart file ---*/

    properties_file.close();

  }

}

