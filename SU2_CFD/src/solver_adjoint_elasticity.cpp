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

CFEM_ElasticitySolver_Adj::CFEM_ElasticitySolver_Adj(void) : CSolver() {

  nElement   = 0;
  nMarker    = 0;
  nFEA_Terms = 0;
  n_DV	   = 0;

  direct_solver 		= NULL;
  GradN_X				= NULL;
  GradN_x				= NULL;

  Jacobian_c_ij		= NULL;
  Jacobian_s_ij		= NULL;

  mZeros_Aux			= NULL;
  mId_Aux				= NULL;

  element_container	= NULL;

  sensI_adjoint		= NULL;

  DV_Val      = NULL;
  DV_Val_Max  = NULL;
  DV_Val_Min  = NULL;

}

CFEM_ElasticitySolver_Adj::CFEM_ElasticitySolver_Adj(CGeometry *geometry, CConfig *config, CSolver *direct_solver) : CSolver() {

  unsigned long iPoint;
  unsigned short iVar, jVar, iDim, jDim;
  unsigned short iTerm;

  unsigned short iZone = config->GetiZone();
  unsigned short nZone = geometry->GetnZone();

  su2double E = config->GetElasticyMod();

  int rank = MASTER_NODE;
#ifdef HAVE_MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif

  bool de_effects = config->GetDE_Effects();											// Test whether we consider dielectric elastomers
  bool nonlinear_analysis = (config->GetGeometricConditions() == LARGE_DEFORMATIONS);	// Nonlinear analysis.

  /*--- Store a pointer to the direct solver: this is where the solution is stored ---*/
  this->direct_solver = direct_solver;

  nElement      = geometry->GetnElem();
  nDim          = geometry->GetnDim();
  nMarker       = geometry->GetnMarker();

  nPoint        = geometry->GetnPoint();
  nPointDomain  = geometry->GetnPointDomain();

  /*--- Number of different terms for FEA ---*/
  nFEA_Terms = 1;
  if (de_effects) nFEA_Terms++;
  /*--- First level: different possible terms of the equations ---*/
  element_container = new CElement** [MAX_TERMS];
  for (iTerm = 0; iTerm < MAX_TERMS; iTerm++)
    element_container[iTerm] = new CElement* [MAX_FE_KINDS];

  if (nDim == 2){
    element_container[FEA_TERM][EL_TRIA] = new CTRIA1(nDim, config);
    element_container[FEA_TERM][EL_QUAD] = new CQUAD4(nDim, config);

    /*--- Effects of the DEs ---*/
    if (de_effects){
      element_container[DE_TERM][EL_TRIA] = new CTRIA1(nDim, config);
      element_container[DE_TERM][EL_QUAD] = new CQUAD4(nDim, config);
    }

    /*--- Elements to integrate the sensitivities respect to the DV ---*/
    switch (config->GetDV_FEA()) {
    case YOUNG_MODULUS:
      element_container[FEA_ADJ][EL_TRIA] = new CTRIA1(nDim, config);
      element_container[FEA_ADJ][EL_QUAD] = new CQUAD4(nDim, config);
      break;
    case ELECTRIC_FIELD:
      element_container[DE_ADJ][EL_TRIA] = new CTRIA1(nDim, config);
      element_container[DE_ADJ][EL_QUAD] = new CQUAD4(nDim, config);
      break;
    default:
      element_container[FEA_ADJ][EL_TRIA] = new CTRIA1(nDim, config);
      element_container[FEA_ADJ][EL_QUAD] = new CQUAD4(nDim, config);
      break;
    }

  }
  else if (nDim == 3){
    element_container[FEA_TERM][EL_TETRA] = new CTETRA1(nDim, config);
    element_container[FEA_TERM][EL_HEXA] = new CHEXA8(nDim, config);
  }
  GradN_X = new su2double [nDim];
  GradN_x = new su2double [nDim];

  nVar = nDim;

  /*--- Define some auxiliary vectors ---*/
  Residual = new su2double[nVar];		for (iVar = 0; iVar < nVar; iVar++) Residual[iVar]	= 0.0;
  Solution = new su2double[nVar]; 	for (iVar = 0; iVar < nVar; iVar++) Solution[iVar]	= 0.0;

  /*--- Term ij of the Jacobian ---*/

  Jacobian_ij = new su2double*[nVar];
  for (iVar = 0; iVar < nVar; iVar++) {
    Jacobian_ij[iVar] = new su2double [nVar];
    for (jVar = 0; jVar < nVar; jVar++) {
      Jacobian_ij[iVar][jVar] = 0.0;
    }
  }
  if (nonlinear_analysis){

    /*--- Term ij of the Jacobian (constitutive contribution) ---*/

    Jacobian_c_ij = new su2double*[nVar];
    for (iVar = 0; iVar < nVar; iVar++) {
      Jacobian_c_ij[iVar] = new su2double [nVar];
      for (jVar = 0; jVar < nVar; jVar++) {
        Jacobian_c_ij[iVar][jVar] = 0.0;
      }
    }

    /*--- Term ij of the Jacobian (stress contribution) ---*/

    Jacobian_s_ij = new su2double*[nVar];
    for (iVar = 0; iVar < nVar; iVar++) {
      Jacobian_s_ij[iVar] = new su2double [nVar];
      for (jVar = 0; jVar < nVar; jVar++) {
        Jacobian_s_ij[iVar][jVar] = 0.0;
      }
    }

  }
  else{
    Jacobian_c_ij = NULL;
    Jacobian_s_ij = NULL;
  }
  /*--- Matrices to impose clamped boundary conditions ---*/

  mZeros_Aux = new su2double *[nDim];
  for(iDim = 0; iDim < nDim; iDim++)
    mZeros_Aux[iDim] = new su2double[nDim];

  mId_Aux = new su2double *[nDim];
  for(iDim = 0; iDim < nDim; iDim++)
    mId_Aux[iDim] = new su2double[nDim];

  for(iDim = 0; iDim < nDim; iDim++){
    for (jDim = 0; jDim < nDim; jDim++){
      mZeros_Aux[iDim][jDim] = 0.0;
      mId_Aux[iDim][jDim] = 0.0;
    }
    mId_Aux[iDim][iDim] = E;
  }

  unsigned short nSolVar;
  unsigned long index;
  string text_line, filename;
  ifstream restart_file;
  su2double dull_val;
  long Dyn_RestartIter;
  bool restart = (config->GetRestart() || config->GetRestart_Flow());

  /*--- Check for a restart, initialize from zero otherwise ---*/
  node = new CVariable*[nPoint];
//  if (!restart) {
    for (iVar = 0; iVar < nVar; iVar++) Solution[iVar] = 0.0;
    for (iPoint = 0; iPoint < nPoint; iPoint++) {
      node[iPoint] = new CFEM_ElasVariable_Adj(Solution, nDim, nVar, config);
    }
//  }
//  else {
//
//    /*--- Restart the solution from file information ---*/
//    filename = config->GetSolution_FEMFileName();
//
//    /*--- If multizone, append zone name ---*/
//    if (nZone > 1)
//      filename = config->GetMultizone_FileName(filename, iZone);
//
//    restart_file.open(filename.data(), ios::in);
//
//    /*--- In case there is no file ---*/
//
//    if (restart_file.fail()) {
//      if (rank == MASTER_NODE)
//        cout << "There is no FEM adjoint restart file!!" << endl;
//      exit(EXIT_FAILURE);
//    }
//
//    /*--- In case this is a parallel simulation, we need to perform the
//     	 Global2Local index transformation first. ---*/
//
//    long *Global2Local = new long[geometry->GetGlobal_nPointDomain()];
//
//    /*--- First, set all indices to a negative value by default ---*/
//
//    for (iPoint = 0; iPoint < geometry->GetGlobal_nPointDomain(); iPoint++)
//      Global2Local[iPoint] = -1;
//
//    /*--- Now fill array with the transform values only for local points ---*/
//
//    for (iPoint = 0; iPoint < nPointDomain; iPoint++)
//      Global2Local[geometry->node[iPoint]->GetGlobalIndex()] = iPoint;
//
//    /*--- Read all lines in the restart file ---*/
//
//    long iPoint_Local;
//    unsigned long iPoint_Global_Local = 0, iPoint_Global = 0; string text_line;
//    unsigned short rbuf_NotMatching = 0, sbuf_NotMatching = 0;
//
//    /*--- The first line is the header ---*/
//
//    getline (restart_file, text_line);
//
//    while (getline (restart_file, text_line)) {
//      istringstream point_line(text_line);
//
//      /*--- Retrieve local index. If this node from the restart file lives
//       	   on a different processor, the value of iPoint_Local will be -1.
//       	   Otherwise, the local index for this node on the current processor
//       	   will be returned and used to instantiate the vars. ---*/
//
//      iPoint_Local = Global2Local[iPoint_Global];
//
//      if (iPoint_Local >= 0) {
//
//        if (nDim == 2) point_line >> index >> dull_val >> dull_val >> Solution[0] >> Solution[1];
//        if (nDim == 3) point_line >> index >> dull_val >> dull_val >> dull_val >> Solution[0] >> Solution[1] >> Solution[2];
//
//        node[iPoint_Local] = new CFEM_ElasVariable(Solution, nDim, nVar, config);
//        iPoint_Global_Local++;
//      }
//      iPoint_Global++;
//    }
//
//    /*--- Detect a wrong solution file ---*/
//
//    if (iPoint_Global_Local < nPointDomain) { sbuf_NotMatching = 1; }
//
//#ifndef HAVE_MPI
//    rbuf_NotMatching = sbuf_NotMatching;
//#else
//    SU2_MPI::Allreduce(&sbuf_NotMatching, &rbuf_NotMatching, 1, MPI_UNSIGNED_SHORT, MPI_SUM, MPI_COMM_WORLD);
//#endif
//
//    if (rbuf_NotMatching != 0) {
//      if (rank == MASTER_NODE) {
//        cout << endl << "The solution file " << filename.data() << " doesn't match with the mesh file!" << endl;
//        cout << "It could be empty lines at the end of the file." << endl << endl;
//      }
//#ifndef HAVE_MPI
//      exit(EXIT_FAILURE);
//#else
//      MPI_Barrier(MPI_COMM_WORLD);
//      MPI_Abort(MPI_COMM_WORLD,1);
//      MPI_Finalize();
//#endif
//    }
//
//    /*--- Instantiate the variable class with an arbitrary solution
//     	 at any halo/periodic nodes. The initial solution can be arbitrary,
//     	 because a send/recv is performed immediately in the solver (Set_MPI_Solution()). ---*/
//
//    for (iPoint = nPointDomain; iPoint < nPoint; iPoint++) {
//      node[iPoint] = new CFEM_ElasVariable_Adj(Solution, nDim, nVar, config);
//    }
//
//
//    /*--- Close the restart file ---*/
//
//    restart_file.close();
//
//    /*--- Free memory needed for the transformation ---*/
//
//    delete [] Global2Local;
//
//  }

   /*--- DV_Val: Vector to store the value of the design variable. ---*/

  unsigned short i_DV;

  /*--- The number of design variables is equal to the total number of regions ---*/
  if (nDim == 2) n_DV = config->GetnDV_X() * config->GetnDV_Y();
  else n_DV = config->GetnDV_X() * config->GetnDV_Y() * config->GetnDV_Z();

  /*--- Allocate the vector that stores the design variables ---*/
  DV_Val = new su2double[n_DV];
  DV_Val_Max = new su2double[n_DV];
  DV_Val_Min = new su2double[n_DV];

  /*---- Initialize the number of design variables ---*/
  switch (config->GetDV_FEA()) {
  case YOUNG_MODULUS:
    DV_Val[0] = config->GetElasticyMod();
    DV_Val_Max[0] = 1.0E5 * config->GetElasticyMod();
    DV_Val_Min[0] = 1.0E-5 * config->GetElasticyMod();
    break;
  case ELECTRIC_FIELD:
    unsigned short nEField_Read;
    nEField_Read = config->GetnElectric_Field();
    if (nEField_Read == n_DV){
      for (i_DV = 0; i_DV < n_DV; i_DV++){
        DV_Val[i_DV] = config->Get_Electric_Field_Mod(i_DV);
        DV_Val_Max[i_DV] = config->Get_Electric_Field_Max(i_DV);
        DV_Val_Min[i_DV] = config->Get_Electric_Field_Min(i_DV);
      }
    } else if (nEField_Read == 1){
      for (i_DV = 0; i_DV < n_DV; i_DV++){
        DV_Val[i_DV] = config->Get_Electric_Field_Mod(0);
        DV_Val_Max[i_DV] = config->Get_Electric_Field_Max(0);
        DV_Val_Min[i_DV] = config->Get_Electric_Field_Min(0);
      }
    } else{
      cout << "THE NUMBER OF ELECTRIC FIELD AND DESIGN REGIONS IS NOT IN AGREEMENT!!!" << endl;
      exit(EXIT_FAILURE);
    }
    break;
  default:
    DV_Val[0] = 0.0;
    DV_Val_Max[0] = 0.0;
    DV_Val_Min[0] = 0.0;
  }

  /*--- Initialize the sensitivity variable for the linear system ---*/
  sensI_adjoint = new su2double[n_DV];

  /*--- Initialize the different structures for the linear system ---*/
  //	Jacobian.Initialize(nPoint, nPointDomain, nVar, nVar, false, geometry, config);
  LinSysSol.Initialize(nPoint, nPointDomain, nVar, 0.0);

  LinSysRes.Initialize(nPoint, nPointDomain, nVar, 0.0);				// Rv = dF/dv - dK/dV*x

  /*--- Initialize the different structures for the linear system ---*/
  LinSysSol_Direct.Initialize(nPoint, nPointDomain, nVar, 0.0);
  LinSysRes_dSdv.Initialize(nPoint, nPointDomain, nVar, 0.0);
  LinSysRes_FSens_DV.Initialize(nPoint, nPointDomain, nVar, 0.0);
  LinSysRes_Aux.Initialize(nPoint, nPointDomain, nVar, 0.0);

  Jacobian_ISens.Initialize(nPoint, nPointDomain, nVar, nVar, false, geometry, config);

  bool predicted_de = config->GetDE_Predicted();
  if (predicted_de){
    Jacobian_Pred.Initialize(nPoint, nPointDomain, nVar, nVar, false, geometry, config);
  }

  switch (config->GetKind_ObjFunc()) {
  case REFERENCE_GEOMETRY:
    Set_ReferenceGeometry(geometry, config);
    break;
  }

  /*--- Header of the temporary output file ---*/
  ofstream myfile_res;
  myfile_res.open ("Results_E.txt");

  for (i_DV = 0; i_DV < n_DV; i_DV++){
    myfile_res << "E(" << i_DV << ") ";
  }

  myfile_res << "val_I ";

  for (i_DV = 0; i_DV < n_DV; i_DV++){
    myfile_res << "Sens(" << i_DV << ") ";
  }

  myfile_res << endl;

  myfile_res.close();

  /*--- Perform the MPI communication of the solution ---*/
  Set_MPI_Solution(geometry, config);

}

CFEM_ElasticitySolver_Adj::~CFEM_ElasticitySolver_Adj(void) {

  unsigned short iVar, jVar;
  unsigned long iPoint;

  for (iPoint = 0; iPoint < nPoint; iPoint++){
    delete [] node[iPoint];
  }

  for (iVar = 0; iVar < MAX_TERMS; iVar++){
    for (jVar = 0; jVar < MAX_FE_KINDS; iVar++)
      if (element_container[iVar][jVar] != NULL) delete [] element_container[iVar][jVar];
  }

  for (iVar = 0; iVar < nVar; iVar++){
    delete [] Jacobian_ij[iVar];
    if (Jacobian_s_ij != NULL) delete [] Jacobian_s_ij[iVar];
    if (Jacobian_c_ij != NULL) delete [] Jacobian_c_ij[iVar];
    delete [] mZeros_Aux[iVar];
    delete [] mId_Aux[iVar];
  }

  if (element_container != NULL) delete [] element_container;
  delete [] node;
  delete [] Jacobian_ij;
  if (Jacobian_s_ij != NULL) delete [] Jacobian_s_ij;
  if (Jacobian_c_ij != NULL) delete [] Jacobian_c_ij;
  delete [] Solution;
  delete [] GradN_X;
  delete [] GradN_x;

  delete [] Residual;

  delete [] mZeros_Aux;
  delete [] mId_Aux;

  if (sensI_adjoint != NULL) delete [] sensI_adjoint;
  if (DV_Val != NULL) delete [] DV_Val;

}

void CFEM_ElasticitySolver_Adj::Set_MPI_Solution(CGeometry *geometry, CConfig *config) {

  unsigned short iVar, iMarker, iPeriodic_Index, MarkerS, MarkerR;
  unsigned long iVertex, iPoint, nVertexS, nVertexR, nBufferS_Vector, nBufferR_Vector;
  su2double *Buffer_Receive_U = NULL, *Buffer_Send_U = NULL;

#ifdef HAVE_MPI
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

      /*--- Copy the solution that should be sended ---*/
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

        /*--- Copy conserved variables before performing transformation. ---*/
        for (iVar = 0; iVar < nVar; iVar++)
          Solution[iVar] = Buffer_Receive_U[iVar*nVertexR+iVertex];

        /*--- Copy solution variables back into buffer. ---*/
        for (iVar = 0; iVar < nVar; iVar++)
          node[iPoint]->SetSolution(iVar, Solution[iVar]);

      }

      /*--- Deallocate receive buffer ---*/
      delete [] Buffer_Receive_U;

    }

  }

}

void CFEM_ElasticitySolver_Adj::Set_MPI_RefGeom(CGeometry *geometry, CConfig *config) {

  unsigned short iVar, iMarker, iPeriodic_Index, MarkerS, MarkerR;
  unsigned long iVertex, iPoint, nVertexS, nVertexR, nBufferS_Vector, nBufferR_Vector;
  su2double *Buffer_Receive_U = NULL, *Buffer_Send_U = NULL;

#ifdef HAVE_MPI
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

      /*--- Copy the solution that should be sended ---*/
      for (iVertex = 0; iVertex < nVertexS; iVertex++) {
        iPoint = geometry->vertex[MarkerS][iVertex]->GetNode();
        for (iVar = 0; iVar < nVar; iVar++)
          Buffer_Send_U[iVar*nVertexS+iVertex] = node[iPoint]->GetReference_Geometry(iVar);
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

        /*--- Copy conserved variables before performing transformation. ---*/
        for (iVar = 0; iVar < nVar; iVar++)
          Solution[iVar] = Buffer_Receive_U[iVar*nVertexR+iVertex];

        /*--- Copy solution variables back into buffer. ---*/
        for (iVar = 0; iVar < nVar; iVar++)
          node[iPoint]->SetReference_Geometry(iVar, Solution[iVar]);

      }

      /*--- Deallocate receive buffer ---*/
      delete [] Buffer_Receive_U;

    }

  }

}

void CFEM_ElasticitySolver_Adj::Preprocessing(CGeometry *geometry, CSolver **solver_container, CConfig *config, CNumerics **numerics,
    unsigned short iMesh, unsigned long Iteration, unsigned short RunTime_EqSystem, bool Output) {

//  cout << "PREPROCESSING ADJOINT" << endl;

}

void CFEM_ElasticitySolver_Adj::SetInitialCondition(CGeometry **geometry, CSolver ***solver_container, CConfig *config, unsigned long ExtIter){ }

void CFEM_ElasticitySolver_Adj::Set_ReferenceGeometry(CGeometry *geometry, CConfig *config){

  unsigned long iPoint;
  unsigned long index;

  unsigned short iVar;
  unsigned short iZone = config->GetiZone();
  unsigned short nZone = geometry->GetnZone();
  unsigned short file_format = config->GetRefGeom_FileFormat();

  string filename;
  su2double dull_val;
  ifstream reference_file;

  int rank = MASTER_NODE;
#ifdef HAVE_MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif


  /*--- Restart the solution from file information ---*/

  filename = config->GetRefGeom_FEMFileName();

  /*--- If multizone, append zone name ---*/
  if (nZone > 1)
    filename = config->GetMultizone_FileName(filename, iZone);

  reference_file.open(filename.data(), ios::in);

  /*--- In case there is no file ---*/

  if (reference_file.fail()) {
    if (rank == MASTER_NODE)
      cout << "There is no FEM reference geometry file!!" << endl;
    exit(EXIT_FAILURE);
  }

  cout << "Filename: " << filename << " and format " << file_format << "." << endl;

  /*--- In case this is a parallel simulation, we need to perform the
 	 Global2Local index transformation first. ---*/

  long *Global2Local = new long[geometry->GetGlobal_nPointDomain()];

  /*--- First, set all indices to a negative value by default ---*/

  for (iPoint = 0; iPoint < geometry->GetGlobal_nPointDomain(); iPoint++)
    Global2Local[iPoint] = -1;

  /*--- Now fill array with the transform values only for local points ---*/

  for (iPoint = 0; iPoint < nPointDomain; iPoint++)
    Global2Local[geometry->node[iPoint]->GetGlobalIndex()] = iPoint;

  /*--- Read all lines in the restart file ---*/

  long iPoint_Local;
  unsigned long iPoint_Global_Local = 0, iPoint_Global = 0; string text_line;
  unsigned short rbuf_NotMatching = 0, sbuf_NotMatching = 0;

  /*--- The first line is the header ---*/

  getline (reference_file, text_line);

  while (getline (reference_file, text_line)) {
    istringstream point_line(text_line);

    /*--- Retrieve local index. If this node from the restart file lives
   	   on a different processor, the value of iPoint_Local will be -1.
   	   Otherwise, the local index for this node on the current processor
   	   will be returned and used to instantiate the vars. ---*/

    iPoint_Local = Global2Local[iPoint_Global];

    if (iPoint_Local >= 0) {

      if (nDim == 2) point_line >> index >> dull_val >> dull_val >> Solution[0] >> Solution[1];
      if (nDim == 3) point_line >> index >> dull_val >> dull_val >> dull_val >> Solution[0] >> Solution[1] >> Solution[2];

      for (iVar = 0; iVar < nVar; iVar++) node[iPoint_Local]->SetReference_Geometry(iVar, Solution[iVar]);

      iPoint_Global_Local++;
    }
    iPoint_Global++;
  }

  /*--- Detect a wrong solution file ---*/

  if (iPoint_Global_Local < nPointDomain) { sbuf_NotMatching = 1; }

#ifndef HAVE_MPI
  rbuf_NotMatching = sbuf_NotMatching;
#else
  SU2_MPI::Allreduce(&sbuf_NotMatching, &rbuf_NotMatching, 1, MPI_UNSIGNED_SHORT, MPI_SUM, MPI_COMM_WORLD);
#endif

  if (rbuf_NotMatching != 0) {
    if (rank == MASTER_NODE) {
      cout << endl << "The solution file " << filename.data() << " doesn't match with the mesh file!" << endl;
      cout << "It could be empty lines at the end of the file." << endl << endl;
    }
#ifndef HAVE_MPI
    exit(EXIT_FAILURE);
#else
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Abort(MPI_COMM_WORLD,1);
    MPI_Finalize();
#endif
  }

  /*--- Close the restart file ---*/

  reference_file.close();

  /*--- We need to communicate the reference geometry for the halo nodes. ---*/
  Set_MPI_RefGeom(geometry, config);

  /*--- Free memory needed for the transformation ---*/

  delete [] Global2Local;

}

void CFEM_ElasticitySolver_Adj::Compute_StiffMatrix(CGeometry *geometry, CSolver **solver_container, CNumerics **numerics, CConfig *config){

  unsigned long iElem, iVar, jVar, iPoint;
  unsigned short iNode,  iDim, nNodes;
  unsigned long indexNode[8]={0,0,0,0,0,0,0,0};
  su2double val_Coord;
  int EL_KIND, DV_TERM;

  su2double *Kab = NULL;
  unsigned short NelNodes, jNode;

  switch (config->GetDV_FEA()) {
  case YOUNG_MODULUS:
    DV_TERM = FEA_ADJ;
    break;
  case ELECTRIC_FIELD:
    DV_TERM = DE_ADJ;
    break;
  }

  /*--- For the solution of the adjoint problem, this routine computes dK/dv, being v the design variables ---*/

  /*--- TODO: This needs to be extended to n Design Variables. As of now, only for E. ---*/

  /*--- Set the value of the Jacobian and Auxiliary vectors to 0 ---*/
  Jacobian_ISens.SetValZero();
  LinSysRes_Aux.SetValZero();
  LinSysRes_dSdv.SetValZero();
  for (iVar = 0; iVar < nVar; iVar++){
    Residual[iVar] = 0.0;
  }

  /*--- Initialize the value of the vector -x, being x the solution vector of the direct solver---*/
  for (iPoint = 0; iPoint < nPoint; iPoint++){
    for (iVar = 0; iVar < nVar; iVar++){
      Residual[iVar] = direct_solver->node[iPoint]->GetSolution(iVar);
    }
    LinSysRes_Aux.SubtractBlock(iPoint, Residual);
  }

  /*--- Loops over all the elements ---*/

  for (iElem = 0; iElem < geometry->GetnElem(); iElem++) {

    if (geometry->elem[iElem]->GetVTK_Type() == TRIANGLE)     {nNodes = 3; EL_KIND = EL_TRIA;}
    if (geometry->elem[iElem]->GetVTK_Type() == QUADRILATERAL)    {nNodes = 4; EL_KIND = EL_QUAD;}

    if (geometry->elem[iElem]->GetVTK_Type() == TETRAHEDRON)  {nNodes = 4; EL_KIND = EL_TETRA;}
    if (geometry->elem[iElem]->GetVTK_Type() == PYRAMID)      {nNodes = 5; EL_KIND = EL_TRIA;}
    if (geometry->elem[iElem]->GetVTK_Type() == PRISM)        {nNodes = 6; EL_KIND = EL_TRIA;}
    if (geometry->elem[iElem]->GetVTK_Type() == HEXAHEDRON)   {nNodes = 8; EL_KIND = EL_HEXA;}

    /*--- For the number of nodes, we get the coordinates from the connectivity matrix ---*/

    for (iNode = 0; iNode < nNodes; iNode++) {

      indexNode[iNode] = geometry->elem[iElem]->GetNode(iNode);

      for (iDim = 0; iDim < nDim; iDim++) {
        val_Coord = geometry->node[indexNode[iNode]]->GetCoord(iDim);
        element_container[DV_TERM][EL_KIND]->SetRef_Coord(val_Coord, iNode, iDim);
      }
    }

    numerics[DV_TERM]->Compute_Tangent_Matrix(element_container[DV_TERM][EL_KIND], config);

    NelNodes = element_container[DV_TERM][EL_KIND]->GetnNodes();

    for (iNode = 0; iNode < NelNodes; iNode++){

      for (jNode = 0; jNode < NelNodes; jNode++){

        Kab = element_container[DV_TERM][EL_KIND]->Get_Kab(iNode, jNode);

        for (iVar = 0; iVar < nVar; iVar++){
          for (jVar = 0; jVar < nVar; jVar++){
            Jacobian_ij[iVar][jVar] = Kab[iVar*nVar+jVar];
          }
        }

        Jacobian_ISens.AddBlock(indexNode[iNode], indexNode[jNode], Jacobian_ij);

      }

    }

  }

  /*--- Once computed, compute Rv = dK/dE*(-x) =  ---*/
  Jacobian_ISens.MatrixVectorProduct(LinSysRes_Aux,LinSysRes_dSdv,geometry,config);

}

void CFEM_ElasticitySolver_Adj::Compute_NodalStressRes(CGeometry *geometry, CSolver **solver_container, CNumerics **numerics, CConfig *config) {

}

void CFEM_ElasticitySolver_Adj::Compute_StiffMatrix_NodalStressRes(CGeometry *geometry, CSolver **solver_container, CNumerics **numerics, CConfig *config){

  unsigned long iElem, iVar, jVar;
  unsigned short iNode, iDim, nNodes = 0;
  unsigned long indexNode[8]={0,0,0,0,0,0,0,0};
  su2double val_Coord, val_Sol, val_Sol_Disp, val_Sol_Vel;
  int EL_KIND = 0, iTerm;

  su2double Delta_t = config->GetDelta_DynTime();

  su2double Ks_ab;
  su2double *Kab = NULL;
  su2double *Kk_ab = NULL;
  su2double *Ta = NULL;

  su2double *Kab_DE = NULL, *Ta_DE = NULL;
  su2double Ks_ab_DE = 0.0;

  unsigned short NelNodes, jNode;

  bool incompressible = (config->GetMaterialCompressibility() == INCOMPRESSIBLE_MAT);
  bool de_effects = config->GetDE_Effects();

  /*--- Loops over all the elements ---*/

  for (iElem = 0; iElem < geometry->GetnElem(); iElem++) {

    if (geometry->elem[iElem]->GetVTK_Type() == TRIANGLE)     {nNodes = 3; EL_KIND = EL_TRIA;}
    if (geometry->elem[iElem]->GetVTK_Type() == QUADRILATERAL){nNodes = 4; EL_KIND = EL_QUAD;}

    if (geometry->elem[iElem]->GetVTK_Type() == TETRAHEDRON)  {nNodes = 4; EL_KIND = EL_TETRA;}
    if (geometry->elem[iElem]->GetVTK_Type() == PYRAMID)      {nNodes = 5; EL_KIND = EL_TRIA;}
    if (geometry->elem[iElem]->GetVTK_Type() == PRISM)        {nNodes = 6; EL_KIND = EL_TRIA;}
    if (geometry->elem[iElem]->GetVTK_Type() == HEXAHEDRON)   {nNodes = 8; EL_KIND = EL_HEXA;}

    /*--- For the number of nodes, we get the coordinates from the connectivity matrix ---*/

    for (iNode = 0; iNode < nNodes; iNode++) {
      indexNode[iNode] = geometry->elem[iElem]->GetNode(iNode);
      for (iDim = 0; iDim < nDim; iDim++) {
        val_Coord = geometry->node[indexNode[iNode]]->GetCoord(iDim);
        /*--- First order predictor to compute the Jacobian ---*/
        val_Sol_Disp = direct_solver->node[indexNode[iNode]]->GetSolution(iDim);
        val_Sol_Vel = direct_solver->node[indexNode[iNode]]->GetSolution_Vel(iDim);
        val_Sol = val_Sol_Disp + Delta_t*val_Sol_Vel + val_Coord;
        for (iTerm = 0; iTerm < nFEA_Terms; iTerm++){
          element_container[iTerm][EL_KIND]->SetRef_Coord(val_Coord, iNode, iDim);
          element_container[iTerm][EL_KIND]->SetCurr_Coord(val_Sol, iNode, iDim);
        }
      }
    }

    if (de_effects){

      bool multiple_de = (config->GetnDel_EField() > 1);
      /*--- So far, this will only be enabled for quadrilateral elements ---*/
      bool feature_enabled = (geometry->elem[iElem]->GetVTK_Type() == QUADRILATERAL);

      if ((multiple_de) && (feature_enabled)) {
        element_container[DE_TERM][EL_KIND]->Set_iDe(direct_solver->Get_iElem_iDe(iElem));
      }
      else{
        element_container[DE_TERM][EL_KIND]->Set_iDe(0);
      }
    }

    /*--- If incompressible, we compute the Mean Dilatation term first so the volume is already computed ---*/

//    if (incompressible) numerics[FEA_TERM]->Compute_MeanDilatation_Term(element_container[FEA_TERM][EL_KIND], config);

    numerics[FEA_TERM]->Compute_Tangent_Matrix(element_container[FEA_TERM][EL_KIND], config);

    if (de_effects) numerics[DE_TERM]->Compute_Tangent_Matrix(element_container[DE_TERM][EL_KIND], config);

    NelNodes = element_container[FEA_TERM][EL_KIND]->GetnNodes();

    for (iNode = 0; iNode < NelNodes; iNode++){

      for (jNode = 0; jNode < NelNodes; jNode++){

        /*--- Retrieve the values of the FEA term ---*/
        Kab = element_container[FEA_TERM][EL_KIND]->Get_Kab(iNode, jNode);
        Ks_ab = element_container[FEA_TERM][EL_KIND]->Get_Ks_ab(iNode,jNode);
        if (incompressible) Kk_ab = element_container[FEA_TERM][EL_KIND]->Get_Kk_ab(iNode,jNode);

        for (iVar = 0; iVar < nVar; iVar++){
          Jacobian_s_ij[iVar][iVar] = Ks_ab;
          for (jVar = 0; jVar < nVar; jVar++){
            Jacobian_c_ij[iVar][jVar] = Kab[iVar*nVar+jVar];
//            if (incompressible) Jacobian_k_ij[iVar][jVar] = Kk_ab[iVar*nVar+jVar];
          }
        }

        Jacobian_Pred.AddBlock(indexNode[iNode], indexNode[jNode], Jacobian_c_ij);
        Jacobian_Pred.AddBlock(indexNode[iNode], indexNode[jNode], Jacobian_s_ij);
//        if (incompressible) Jacobian_Pred.AddBlock(indexNode[iNode], indexNode[jNode], Jacobian_k_ij);

        /*--- Retrieve the electric contribution to the Jacobian ---*/
        if (de_effects){
//          Kab_DE = element_container[DE_TERM][EL_KIND]->Get_Kab(iNode, jNode);
          Ks_ab_DE = element_container[DE_TERM][EL_KIND]->Get_Ks_ab(iNode,jNode);

          for (iVar = 0; iVar < nVar; iVar++){
            Jacobian_s_ij[iVar][iVar] = Ks_ab_DE;
//            for (jVar = 0; jVar < nVar; jVar++){
//              Jacobian_c_ij[iVar][jVar] = Kab_DE[iVar*nVar+jVar];
//            }
          }

//          Jacobian.AddBlock(indexNode[iNode], indexNode[jNode], Jacobian_c_ij);
          Jacobian_Pred.AddBlock(indexNode[iNode], indexNode[jNode], Jacobian_s_ij);
        }

      }

    }

  }


}

void CFEM_ElasticitySolver_Adj::Initialize_SystemMatrix(CGeometry *geometry, CSolver **solver_container, CConfig *config){

}

void CFEM_ElasticitySolver_Adj::BC_Clamped(CGeometry *geometry, CSolver **solver_container, CNumerics *numerics, CConfig *config, unsigned short val_marker){

  bool predicted_de = config->GetDE_Predicted();

  unsigned long iPoint, iVertex;
  unsigned short iVar, jVar;

  if (predicted_de){
    for (iVertex = 0; iVertex < geometry->nVertex[val_marker]; iVertex++) {

      /*--- Get node index ---*/

      iPoint = geometry->vertex[val_marker][iVertex]->GetNode();

        if (geometry->node[iPoint]->GetDomain()) {

          if (nDim == 2) {
            Solution[0] = 0.0;  Solution[1] = 0.0;
            Residual[0] = 0.0;  Residual[1] = 0.0;
          }
          else {
            Solution[0] = 0.0;  Solution[1] = 0.0;  Solution[2] = 0.0;
            Residual[0] = 0.0;  Residual[1] = 0.0;  Residual[2] = 0.0;
          }

          node[iPoint]->SetSolution(Solution);

          /*--- STRONG ENFORCEMENT OF THE DISPLACEMENT BOUNDARY CONDITION: ---*/

          /*--- Residual vector ---*/

          LinSysRes.SetBlock(iPoint, Residual);

          /*--- Predicted Jacobian matrix ---*/

          /*--- Delete the columns for a particular node ---*/

          for (iVar = 0; iVar < nPoint; iVar++){
            if (iVar==iPoint) {
              Jacobian_Pred.SetBlock(iVar,iPoint,mId_Aux);
            }
            else {
              Jacobian_Pred.SetBlock(iVar,iPoint,mZeros_Aux);
            }
          }

          /*--- Delete the rows for a particular node ---*/
          for (jVar = 0; jVar < nPoint; jVar++){
            if (iPoint!=jVar) {
              Jacobian_Pred.SetBlock(iPoint,jVar,mZeros_Aux);
            }
          }

        }

    }

  }


}

void CFEM_ElasticitySolver_Adj::BC_Clamped_Post(CGeometry *geometry, CSolver **solver_container, CNumerics *numerics, CConfig *config,
    unsigned short val_marker){ }

void CFEM_ElasticitySolver_Adj::ImplicitEuler_Iteration(CGeometry *geometry, CSolver **solver_container, CConfig *config){ }


void CFEM_ElasticitySolver_Adj::ImplicitNewmark_Iteration(CGeometry *geometry, CSolver **solver_container, CConfig *config) {

  bool predicted_de = config->GetDE_Predicted();

  unsigned long iPoint, jPoint;
  unsigned short iVar, jVar;

  su2double Delta_t= config->GetDelta_DynTime();
  su2double alpha   = config->GetNewmark_alpha();
  su2double a_dt_0 = 1 / (alpha*pow(Delta_t,2.0));

  if (predicted_de){
    /*--- Add the mass matrix contribution to the predicted Jacobian ---*/

    for (iPoint = 0; iPoint < nPoint; iPoint++){
      for (jPoint = 0; jPoint < nPoint; jPoint++){
        for(iVar = 0; iVar < nVar; iVar++){
          for (jVar = 0; jVar < nVar; jVar++){
            Jacobian_ij[iVar][jVar] = a_dt_0 * direct_solver->Get_MassMatrix(iPoint, jPoint, iVar, jVar);
          }
        }
        Jacobian_Pred.AddBlock(iPoint, jPoint, Jacobian_ij);
      }
    }

  }



}

void CFEM_ElasticitySolver_Adj::Postprocessing(CGeometry *geometry, CSolver **solver_container, CConfig *config,  CNumerics **numerics,
    unsigned short iMesh){

  switch (config->GetDV_FEA()) {
  case YOUNG_MODULUS:
    Stiffness_Sensitivity(geometry, solver_container, numerics, config);
    break;
  case ELECTRIC_FIELD:
    DE_Sensitivity(geometry, solver_container, numerics, config);
    break;
  }

}

void CFEM_ElasticitySolver_Adj::RefGeom_Sensitivity(CGeometry *geometry, CSolver **solver_container, CConfig *config){

  unsigned short iVar;
  unsigned long iPoint;
  su2double *reference_geometry = NULL, *current_solution = NULL;
  su2double *solDisp = NULL, *solVel = NULL, predicted_solution[3] = {0.0, 0.0, 0.0};

  bool predicted_de = config->GetDE_Predicted();

  su2double objective_function = 0.0;

  LinSysRes.SetValZero();

  if (!predicted_de){

    for (iPoint = 0; iPoint < nPoint; iPoint++){

      /*--- The reference geometry is stored in the adjoint variable ---*/
      reference_geometry = node[iPoint]->GetReference_Geometry();

      /*--- The current solution comes from the direct solver ---*/
      current_solution = direct_solver->node[iPoint]->GetSolution();

      for (iVar = 0; iVar < nVar; iVar++){

        Solution[iVar] = 2*(current_solution[iVar]-reference_geometry[iVar]);
        objective_function += (current_solution[iVar]-reference_geometry[iVar])*(current_solution[iVar]-reference_geometry[iVar]);

      }

      LinSysRes.AddBlock(iPoint, Solution);

    }

  } else {

    su2double Delta_t = config->GetDelta_DynTime();

    for (iPoint = 0; iPoint < nPoint; iPoint++){

      /*--- The reference geometry is stored in the adjoint variable ---*/
      reference_geometry = node[iPoint]->GetReference_Geometry();

      /*--- The solution and velocity come from the direct solver ---*/
      solDisp = direct_solver->node[iPoint]->GetSolution();
      solVel = direct_solver->node[iPoint]->GetSolution_Vel();

      for (iVar = 0; iVar < nVar; iVar++){

        /*--- We now use a first order predictor for the solution ---*/
        predicted_solution[iVar] = solDisp[iVar] + Delta_t*solVel[iVar];

        Solution[iVar] = 2*(predicted_solution[iVar]-reference_geometry[iVar]);
        objective_function += (predicted_solution[iVar]-reference_geometry[iVar])*(predicted_solution[iVar]-reference_geometry[iVar]);

      }

      LinSysRes.AddBlock(iPoint, Solution);

    }

  }


  val_I = objective_function;


}

void CFEM_ElasticitySolver_Adj::Solve_System(CGeometry *geometry, CSolver **solver_container, CConfig *config){

  unsigned long IterLinSol = 0, iPoint, total_index;
  unsigned short iVar;

  /*--- Initialize residual and solution at the ghost points ---*/

  for (iPoint = nPointDomain; iPoint < nPoint; iPoint++) {

    for (iVar = 0; iVar < nVar; iVar++) {
      total_index = iPoint*nVar + iVar;
      LinSysRes[total_index] = 0.0;
      LinSysSol[total_index] = 0.0;
    }

  }

  /*--- Solve system of K*dx/dE=Rv ---*/
  /*--- LinSysSol_Direct is dx/dE ---*/
  /*--- LinSysRes_dSdv is Rv ---*/

  //	CSysSolve femSystem;
  //	IterLinSol = femSystem.Solve(direct_solver->Jacobian, LinSysRes_dSdv, LinSysSol_Direct, geometry, config);

  /*--- Solve system of K*PHI=dI/dx ---*/
  /*--- LinSysSol is PHI ---*/
  /*--- LinSysRes is dI/dx ---*/
  CSysSolve femSystem;
  bool predicted_de = config->GetDE_Predicted();
  if (!predicted_de){
    IterLinSol = femSystem.Solve(direct_solver->Jacobian, LinSysRes, LinSysSol, geometry, config);
  } else{
    IterLinSol = femSystem.Solve(Jacobian_Pred, LinSysRes, LinSysSol, geometry, config);
  }

//  double checkJacobian_1, checkJacobian_2;
//  unsigned long iNode, jNode;
//  unsigned short jVar;
//  ofstream myfile;
//  myfile.open ("Jacobian.txt");
//  // For every node i
//  for (iNode = 0; iNode < nPoint; iNode++){
//
//    // For every node j
//    for (jNode = 0; jNode < nPoint; jNode++){
//      // Retrieve the first line
//      checkJacobian_1 = direct_solver->Jacobian.GetBlock(iNode, jNode, 0, 0);
//      checkJacobian_2 = direct_solver->Jacobian.GetBlock(iNode, jNode, 0, 1);
//
//      myfile << checkJacobian_1 << " " << checkJacobian_2 << " ";
//
//    }
//    // Next line
//    myfile << endl;
//    // For every node j
//    for (jNode = 0; jNode < nPoint; jNode++){
//      // Retrieve the second line
//      checkJacobian_1 = direct_solver->Jacobian.GetBlock(iNode, jNode, 1, 0);
//      checkJacobian_2 = direct_solver->Jacobian.GetBlock(iNode, jNode, 1, 1);
//
//      myfile << checkJacobian_1 << " " << checkJacobian_2 << " ";
//
//    }
//    // Next line
//    myfile << endl;
//  }
//  myfile.close();


  for (iPoint=0; iPoint < nPointDomain; iPoint++){

    for (iVar = 0; iVar < nVar; iVar++){

      Solution[iVar] = LinSysSol.GetBlock(iPoint, iVar);

      node[iPoint]->SetSolution(iVar, Solution[iVar]);

    }

  }

  /*--- The the number of iterations of the linear solver ---*/

  SetIterLinSolver(IterLinSol);

  //	ofstream myfile;
  //	myfile.open ("Check_Adjoint.txt");
  //
  //	unsigned long iNode;
  //	unsigned long realIndex;
  //	su2double dxdE, Rv, PHI, dIdx;
  //	myfile << "Node (" << "realIndex" << "," << "iVar" << "): " << "dxdE" << " " << "Rv" << " " << "PHI" << " " << "dIdx" << endl;
  //
  //	for (iNode = 0; iNode < nPoint; iNode++){
  //			realIndex = geometry->node[iNode]->GetGlobalIndex();
  //			for (iVar = 0; iVar < nVar; iVar++){
  //				dxdE = LinSysSol_Direct.GetBlock(iNode, iVar);
  //				Rv = LinSysRes_dSdv.GetBlock(iNode, iVar);
  //				PHI = LinSysSol.GetBlock(iNode, iVar);
  //				dIdx = LinSysRes.GetBlock(iNode, iVar);
  //				myfile << "Node (" << realIndex << "," << iVar << "): " << dxdE << " " << Rv << " " << PHI << " " << dIdx << endl;
  //			}
  //	}
  //	myfile.close();

  //	sensI_direct = dotProd(LinSysRes,LinSysSol_Direct);


}

void CFEM_ElasticitySolver_Adj::DE_Sensitivity(CGeometry *geometry, CSolver **solver_container, CNumerics **numerics, CConfig *config){

  unsigned long iElem, iVar, iPoint;
  unsigned short iNode, iDim, nNodes;
  unsigned short i_DV;
  unsigned long indexNode[8]={0,0,0,0,0,0,0,0};
  su2double val_Coord, val_Sol;
  int EL_KIND, DV_TERM;

  su2double *Ta = NULL;
  unsigned short NelNodes;

  switch (config->GetDV_FEA()) {
  case YOUNG_MODULUS:
    DV_TERM = FEA_ADJ;
    break;
  case ELECTRIC_FIELD:
    DV_TERM = DE_ADJ;
    break;
  }


  /*--- For the solution of the adjoint problem, this routine computes dK/dv, being v the design variables ---*/

  /*--- TODO: This needs to be extended to n Design Variables. As of now, only for E. ---*/

  for (i_DV = 0; i_DV < n_DV; i_DV++){

    /*--- Set the value of the Residual vectors to 0 ---*/
    LinSysRes_dSdv.SetValZero();

    /*--- Loops over all the elements ---*/

    for (iElem = 0; iElem < geometry->GetnElem(); iElem++) {

      if (direct_solver->Get_iElem_iDe(iElem) == i_DV){

        if (geometry->elem[iElem]->GetVTK_Type() == TRIANGLE)     {nNodes = 3; EL_KIND = EL_TRIA;}
        if (geometry->elem[iElem]->GetVTK_Type() == QUADRILATERAL)    {nNodes = 4; EL_KIND = EL_QUAD;}

        if (geometry->elem[iElem]->GetVTK_Type() == TETRAHEDRON)  {nNodes = 4; EL_KIND = EL_TETRA;}
        if (geometry->elem[iElem]->GetVTK_Type() == PYRAMID)      {nNodes = 5; EL_KIND = EL_TRIA;}
        if (geometry->elem[iElem]->GetVTK_Type() == PRISM)        {nNodes = 6; EL_KIND = EL_TRIA;}
        if (geometry->elem[iElem]->GetVTK_Type() == HEXAHEDRON)   {nNodes = 8; EL_KIND = EL_HEXA;}

        /*--- For the number of nodes, we get the coordinates from the connectivity matrix ---*/
        /*--- The solution comes from the direct solver ---*/
        for (iNode = 0; iNode < nNodes; iNode++) {
          indexNode[iNode] = geometry->elem[iElem]->GetNode(iNode);
          for (iDim = 0; iDim < nDim; iDim++) {
            val_Coord = geometry->node[indexNode[iNode]]->GetCoord(iDim);
            val_Sol = direct_solver->node[indexNode[iNode]]->GetSolution(iDim) + val_Coord;
            element_container[DV_TERM][EL_KIND]->SetRef_Coord(val_Coord, iNode, iDim);
            element_container[DV_TERM][EL_KIND]->SetCurr_Coord(val_Sol, iNode, iDim);
          }
        }

        element_container[DV_TERM][EL_KIND]->Set_iDe(i_DV);

        numerics[DV_TERM]->Compute_NodalStress_Term(element_container[DV_TERM][EL_KIND], config);

        NelNodes = element_container[DV_TERM][EL_KIND]->GetnNodes();

        for (iNode = 0; iNode < NelNodes; iNode++){

          Ta = element_container[DV_TERM][EL_KIND]->Get_Kt_a(iNode);
          for (iVar = 0; iVar < nVar; iVar++) Residual[iVar] = Ta[iVar];

          LinSysRes_dSdv.SubtractBlock(indexNode[iNode], Residual);

        }

      }

    }

    sensI_adjoint[i_DV] = 0.0;
    for (iPoint = 0; iPoint < nPointDomain; iPoint++){
      for (iVar = 0; iVar < nVar; iVar++){
        sensI_adjoint[i_DV] += node[iPoint]->GetSolution(iVar) * LinSysRes_dSdv.GetBlock(iPoint,iVar);
      }
    }

  }

  ofstream myfile_res;
  myfile_res.open ("Results_E.txt", ios::app);

  myfile_res.precision(15);

  for (i_DV = 0; i_DV < n_DV; i_DV++){
    switch (config->GetDV_FEA()) {
    case YOUNG_MODULUS:
      myfile_res << scientific << DV_Val[i_DV] << " ";
      break;
    case ELECTRIC_FIELD:
      myfile_res << scientific << DV_Val[i_DV] << " ";
      break;
    }
  }

  myfile_res << scientific << " " << val_I << " ";

  for (i_DV = 0; i_DV < n_DV; i_DV++){
    myfile_res << scientific << sensI_adjoint[i_DV] << " ";
  }

  myfile_res << endl;

  myfile_res.close();

  /*--- Now, we are going to compute the values of the new design variables ---*/;

  su2double dv_val;

  for (i_DV = 0; i_DV < n_DV; i_DV++){

    cout << "DV (" << i_DV << "): " ;

    cout << DV_Val[i_DV] << " ";

    /*--- Using a Newton method, the update value is E2 = E1 - alpha * val(I) / grad(I) ---*/
    /*--- However, we may be far away from the solution, so in this first (naive) implementation... ---*/

    /*--- 1) We compute the new value of the design variable ---*/
    dv_val = DV_Val[i_DV] - config->GetDE_Rate() * val_I / sensI_adjoint[i_DV];

    cout << dv_val << " " ;

    /*--- 2) We limit the maximum value ---*/
    dv_val = min(dv_val, DV_Val_Max[i_DV]);

    cout << dv_val << " " ;

    /*--- 3) We limit the minimum value ---*/
    dv_val = max(dv_val, DV_Val_Min[i_DV]);

    cout << dv_val << " " ;

//    /*--- 4) We limit the change rate so as it can only increase by a % of the previous value ---*/
//    if (dv_val > DV_Val[i_DV]){
//
//      dv_val = min(dv_val, ((1.0 +config->GetDE_Rate())* DV_Val[i_DV]));
//
//    }
//    else{
//
//      dv_val = max(dv_val, ((1.0 - config->GetDE_Rate())*DV_Val[i_DV]));
//
//    }

    cout << dv_val << " " ;

    /*--- 5) We store the value in the direct and the adjoint solvers ---*/
    DV_Val[i_DV] = dv_val;

    cout << endl;

    direct_solver->Set_DV_Val(dv_val,i_DV);

    switch (config->GetDV_FEA()) {
    case YOUNG_MODULUS:
      numerics[FEA_TERM]->Set_YoungModulus(i_DV, DV_Val[i_DV]);
      numerics[DE_TERM]->Set_YoungModulus(i_DV, DV_Val[i_DV]);
      numerics[FEA_ADJ]->Set_YoungModulus(i_DV, DV_Val[i_DV]);
      break;
    case ELECTRIC_FIELD:
      numerics[FEA_TERM]->Set_ElectricField(i_DV, DV_Val[i_DV]);
      numerics[DE_TERM]->Set_ElectricField(i_DV, DV_Val[i_DV]);
      numerics[DE_ADJ]->Set_ElectricField(i_DV, DV_Val[i_DV]);
      break;
    }



  }

  //  sensI_adjoint = dotProd(LinSysSol,LinSysRes_dSdv);



}

void CFEM_ElasticitySolver_Adj::Stiffness_Sensitivity(CGeometry *geometry, CSolver **solver_container, CNumerics **numerics, CConfig *config){

  unsigned long iElem, iVar, iPoint;
  unsigned short iNode, iDim, nNodes;
  unsigned short i_DV;
  unsigned long indexNode[8]={0,0,0,0,0,0,0,0};
  su2double val_Coord, val_Sol;
  int EL_KIND, DV_TERM;

  su2double *Ta = NULL;
  unsigned short NelNodes;

  bool de_effects = config->GetDE_Effects();

  switch (config->GetDV_FEA()) {
  case YOUNG_MODULUS:
    DV_TERM = FEA_ADJ;
    break;
  case ELECTRIC_FIELD:
    DV_TERM = DE_ADJ;
    break;
  }


  /*--- For the solution of the adjoint problem, this routine computes dK/dv, being v the design variables ---*/

  /*--- TODO: This needs to be extended to n Design Variables. As of now, only for E. ---*/

  for (i_DV = 0; i_DV < n_DV; i_DV++){

    /*--- Set the value of the Residual vectors to 0 ---*/
    LinSysRes_dSdv.SetValZero();

    /*--- Loops over all the elements ---*/

    for (iElem = 0; iElem < geometry->GetnElem(); iElem++) {

      if (direct_solver->Get_iElem_iDe(iElem) == i_DV){

        if (geometry->elem[iElem]->GetVTK_Type() == TRIANGLE)     {nNodes = 3; EL_KIND = EL_TRIA;}
        if (geometry->elem[iElem]->GetVTK_Type() == QUADRILATERAL)    {nNodes = 4; EL_KIND = EL_QUAD;}

        if (geometry->elem[iElem]->GetVTK_Type() == TETRAHEDRON)  {nNodes = 4; EL_KIND = EL_TETRA;}
        if (geometry->elem[iElem]->GetVTK_Type() == PYRAMID)      {nNodes = 5; EL_KIND = EL_TRIA;}
        if (geometry->elem[iElem]->GetVTK_Type() == PRISM)        {nNodes = 6; EL_KIND = EL_TRIA;}
        if (geometry->elem[iElem]->GetVTK_Type() == HEXAHEDRON)   {nNodes = 8; EL_KIND = EL_HEXA;}

        /*--- For the number of nodes, we get the coordinates from the connectivity matrix ---*/
        /*--- The solution comes from the direct solver ---*/
        for (iNode = 0; iNode < nNodes; iNode++) {
          indexNode[iNode] = geometry->elem[iElem]->GetNode(iNode);
          for (iDim = 0; iDim < nDim; iDim++) {
            val_Coord = geometry->node[indexNode[iNode]]->GetCoord(iDim);
            val_Sol = direct_solver->node[indexNode[iNode]]->GetSolution(iDim) + val_Coord;
            element_container[DV_TERM][EL_KIND]->SetRef_Coord(val_Coord, iNode, iDim);
            element_container[DV_TERM][EL_KIND]->SetCurr_Coord(val_Sol, iNode, iDim);
          }
        }

        element_container[DV_TERM][EL_KIND]->Set_iDe(i_DV);

        numerics[DV_TERM]->Compute_NodalStress_Term(element_container[DV_TERM][EL_KIND], config);

        NelNodes = element_container[DV_TERM][EL_KIND]->GetnNodes();

        for (iNode = 0; iNode < NelNodes; iNode++){

          Ta = element_container[DV_TERM][EL_KIND]->Get_Kt_a(iNode);
          for (iVar = 0; iVar < nVar; iVar++) Residual[iVar] = Ta[iVar];

          LinSysRes_dSdv.SubtractBlock(indexNode[iNode], Residual);

        }

      }

    }

    sensI_adjoint[i_DV] = 0.0;
    for (iPoint = 0; iPoint < nPointDomain; iPoint++){
      for (iVar = 0; iVar < nVar; iVar++){
        sensI_adjoint[i_DV] += node[iPoint]->GetSolution(iVar) * LinSysRes_dSdv.GetBlock(iPoint,iVar);
      }
    }

  }

  ofstream myfile_res;
  myfile_res.open ("Results_E.txt", ios::app);

  myfile_res.precision(15);

  for (i_DV = 0; i_DV < n_DV; i_DV++){
    switch (config->GetDV_FEA()) {
    case YOUNG_MODULUS:
      myfile_res << scientific << DV_Val[i_DV] << " ";
      break;
    case ELECTRIC_FIELD:
      myfile_res << scientific << DV_Val[i_DV] << " ";
      break;
    }
  }

  myfile_res << scientific << " " << val_I << " ";

  for (i_DV = 0; i_DV < n_DV; i_DV++){
    myfile_res << scientific << sensI_adjoint[i_DV] << " ";
  }

  myfile_res << endl;

  myfile_res.close();

  /*--- Now, we are going to compute the values of the new design variables ---*/;

  su2double dv_val;

  for (i_DV = 0; i_DV < n_DV; i_DV++){

//    cout << "DV (" << i_DV << "): " ;
//
//    cout << DV_Val[i_DV] << " ";

    /*--- Using a Newton method, the update value is E2 = E1 - alpha * val(I) / grad(I) ---*/
    /*--- However, we may be far away from the solution, so in this first (naive) implementation... ---*/

    /*--- 1) We compute the new value of the design variable ---*/
    //dv_val = DV_Val[i_DV] - config->GetDE_Rate() * val_I / sensI_adjoint[i_DV];
    dv_val = DV_Val[i_DV] - config->GetDE_Rate() * val_I / sensI_adjoint[i_DV];

//    cout << dv_val << " " ;

    /*--- 2) We limit the maximum value ---*/
    dv_val = min(dv_val, DV_Val_Max[i_DV]);

//    cout << dv_val << " " ;

    /*--- 3) We limit the minimum value ---*/
    dv_val = max(dv_val, DV_Val_Min[i_DV]);

//    cout << dv_val << " " ;

//    /*--- 4) We limit the change rate so as it can only increase by a % of the previous value ---*/
//    if (dv_val > DV_Val[i_DV]){
//
//      dv_val = min(dv_val, ((1.0 +config->GetDE_Rate())* DV_Val[i_DV]));
//
//    }
//    else{
//
//      dv_val = max(dv_val, ((1.0 - config->GetDE_Rate())*DV_Val[i_DV]));
//
//    }

//    cout << dv_val << " " ;

    /*--- 5) We store the value in the direct and the adjoint solvers ---*/
    DV_Val[i_DV] = dv_val;

//    cout << endl;

    direct_solver->Set_DV_Val(dv_val,i_DV);

    switch (config->GetDV_FEA()) {
    case YOUNG_MODULUS:
      numerics[FEA_TERM]->Set_YoungModulus(i_DV, DV_Val[i_DV]);
      if (de_effects) numerics[DE_TERM]->Set_YoungModulus(i_DV, DV_Val[i_DV]);
      numerics[FEA_ADJ]->Set_YoungModulus(i_DV, DV_Val[i_DV]);
      break;
    case ELECTRIC_FIELD:
      numerics[FEA_TERM]->Set_ElectricField(i_DV, DV_Val[i_DV]);
      numerics[DE_TERM]->Set_ElectricField(i_DV, DV_Val[i_DV]);
      numerics[DE_ADJ]->Set_ElectricField(i_DV, DV_Val[i_DV]);
      break;
    }



  }

  //  sensI_adjoint = dotProd(LinSysSol,LinSysRes_dSdv);



}






