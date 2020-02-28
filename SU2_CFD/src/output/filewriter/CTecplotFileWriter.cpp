/*!
 * \file CTecplotFileWriter.cpp
 * \brief Filewriter class for Tecplot ASCII format.
 * \author T. Albring
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

#include "../../../include/output/filewriter/CTecplotFileWriter.hpp"

const string CTecplotFileWriter::fileExt = ".dat";

CTecplotFileWriter::CTecplotFileWriter(string valFileName, CParallelDataSorter *valDataSorter,
                                       unsigned long valTimeIter, su2double valTimeStep) :
  CFileWriter(std::move(valFileName), valDataSorter, fileExt), timeIter(valTimeIter), timeStep(valTimeStep){}

CTecplotFileWriter::~CTecplotFileWriter(){}

void CTecplotFileWriter::Write_Data(){

  if (!dataSorter->GetConnectivitySorted()){
    SU2_MPI::Error("Connectivity must be sorted.", CURRENT_FUNCTION);
  }

  const vector<string> fieldNames = dataSorter->GetFieldNames();

  unsigned short iVar;

  unsigned long iPoint, iElem;

  int iProcessor;

  ofstream Tecplot_File;

  fileSize = 0.0;

  /*--- Set a timer for the file writing. ---*/

#ifndef HAVE_MPI
  startTime = su2double(clock())/su2double(CLOCKS_PER_SEC);
#else
  startTime = MPI_Wtime();
#endif

  /*--- Reduce the total number of each element. ---*/

  unsigned long nParallel_Line = dataSorter->GetnElem(LINE),
                nParallel_Tria = dataSorter->GetnElem(TRIANGLE),
                nParallel_Quad = dataSorter->GetnElem(QUADRILATERAL),
                nParallel_Tetr = dataSorter->GetnElem(TETRAHEDRON),
                nParallel_Hexa = dataSorter->GetnElem(HEXAHEDRON),
                nParallel_Pris = dataSorter->GetnElem(PRISM),
                nParallel_Pyra = dataSorter->GetnElem(PYRAMID);
  
  unsigned long nTot_Line = dataSorter->GetnElemGlobal(LINE),
                nTot_Tria = dataSorter->GetnElemGlobal(TRIANGLE),
                nTot_Quad = dataSorter->GetnElemGlobal(QUADRILATERAL),
                nTot_Tetr = dataSorter->GetnElemGlobal(TETRAHEDRON),
                nTot_Hexa = dataSorter->GetnElemGlobal(HEXAHEDRON),
                nTot_Pris = dataSorter->GetnElemGlobal(PRISM),
                nTot_Pyra = dataSorter->GetnElemGlobal(PYRAMID);

  /*--- Open Tecplot ASCII file and write the header. ---*/

  if (rank == MASTER_NODE) {
    Tecplot_File.open(fileName.c_str(), ios::out);
    Tecplot_File.precision(6);
    Tecplot_File << "TITLE = \"Visualization of the solution\"" << endl;

    Tecplot_File << "VARIABLES = ";
    for (iVar = 0; iVar < fieldNames.size()-1; iVar++) {
      Tecplot_File << "\"" << fieldNames[iVar] << "\",";
    }
    Tecplot_File << "\"" << fieldNames[fieldNames.size()-1] << "\"" << endl;

    /*--- Write the header ---*/

    Tecplot_File << "ZONE ";

    if (timeStep > 0.0){
      Tecplot_File << "STRANDID="<<SU2_TYPE::Int(timeIter+1)<<", SOLUTIONTIME="<< timeIter*timeStep <<", ";
    }

    Tecplot_File << "NODES= "<< dataSorter->GetnPointsGlobal() <<", ELEMENTS= "<< dataSorter->GetnElemGlobal();

    if (dataSorter->GetnDim() == 3){
      if ((nTot_Quad > 0 || nTot_Tria > 0) && (nTot_Hexa + nTot_Pris + nTot_Pyra + nTot_Tetr == 0)){
        Tecplot_File << ", DATAPACKING=POINT, ZONETYPE=FEQUADRILATERAL" << endl;
      }
      else {
        Tecplot_File <<", DATAPACKING=POINT, ZONETYPE=FEBRICK"<< endl;
      }
    }
    else {
      if (nTot_Line > 0 && (nTot_Tria + nTot_Quad == 0)){
        Tecplot_File << ", DATAPACKING=POINT, ZONETYPE=FELINESEG"<< endl;
      }
      else{
        Tecplot_File << ", DATAPACKING=POINT, ZONETYPE=FEQUADRILATERAL"<< endl;
      }
    }
    Tecplot_File.close();
  }

#ifdef HAVE_MPI
  SU2_MPI::Barrier(MPI_COMM_WORLD);
#endif

  /*--- Each processor opens the file. ---*/

  Tecplot_File.open(fileName.c_str(), ios::out | ios::app);

  /*--- Write surface and volumetric solution data. ---*/

  for (iProcessor = 0; iProcessor < size; iProcessor++) {
    if (rank == iProcessor) {

      /*--- Write the node data from this proc ---*/


      for (iPoint = 0; iPoint < dataSorter->GetnPoints(); iPoint++) {
        for (iVar = 0; iVar < fieldNames.size(); iVar++)
          Tecplot_File << scientific << dataSorter->GetData(iVar, iPoint) << "\t";
        Tecplot_File << endl;
      }
    }

    Tecplot_File.flush();
#ifdef HAVE_MPI
    SU2_MPI::Barrier(MPI_COMM_WORLD);
#endif
  }


  /*--- Write connectivity data. ---*/

  for (iProcessor = 0; iProcessor < size; iProcessor++) {
    if (rank == iProcessor) {

      for (iElem = 0; iElem < nParallel_Line; iElem++) {
        Tecplot_File << dataSorter->GetElem_Connectivity(LINE, iElem, 0) << "\t";
        Tecplot_File << dataSorter->GetElem_Connectivity(LINE, iElem, 1)<< "\n";
      }


      for (iElem = 0; iElem < nParallel_Tria; iElem++) {
        Tecplot_File << dataSorter->GetElem_Connectivity(TRIANGLE, iElem, 0) << "\t";
        Tecplot_File << dataSorter->GetElem_Connectivity(TRIANGLE, iElem, 1) << "\t";
        Tecplot_File << dataSorter->GetElem_Connectivity(TRIANGLE, iElem, 2) << "\t";
        Tecplot_File << dataSorter->GetElem_Connectivity(TRIANGLE, iElem, 2) << "\n";
      }

      for (iElem = 0; iElem < nParallel_Quad; iElem++) {
        Tecplot_File << dataSorter->GetElem_Connectivity(QUADRILATERAL, iElem, 0) << "\t";
        Tecplot_File << dataSorter->GetElem_Connectivity(QUADRILATERAL, iElem, 1) << "\t";
        Tecplot_File << dataSorter->GetElem_Connectivity(QUADRILATERAL, iElem, 2) << "\t";
        Tecplot_File << dataSorter->GetElem_Connectivity(QUADRILATERAL, iElem, 3) << "\n";
      }

      for (iElem = 0; iElem < nParallel_Tetr; iElem++) {
        Tecplot_File << dataSorter->GetElem_Connectivity(TETRAHEDRON, iElem, 0) << "\t" << dataSorter->GetElem_Connectivity(TETRAHEDRON, iElem, 1) << "\t";
        Tecplot_File << dataSorter->GetElem_Connectivity(TETRAHEDRON, iElem, 2) << "\t" << dataSorter->GetElem_Connectivity(TETRAHEDRON, iElem, 2) << "\t";
        Tecplot_File << dataSorter->GetElem_Connectivity(TETRAHEDRON, iElem, 3) << "\t" << dataSorter->GetElem_Connectivity(TETRAHEDRON, iElem, 3) << "\t";
        Tecplot_File << dataSorter->GetElem_Connectivity(TETRAHEDRON, iElem, 3) << "\t" << dataSorter->GetElem_Connectivity(TETRAHEDRON, iElem, 3) << "\n";
      }

      for (iElem = 0; iElem < nParallel_Hexa; iElem++) {
        Tecplot_File << dataSorter->GetElem_Connectivity(HEXAHEDRON, iElem, 0) << "\t" << dataSorter->GetElem_Connectivity(HEXAHEDRON, iElem, 1) << "\t";
        Tecplot_File << dataSorter->GetElem_Connectivity(HEXAHEDRON, iElem, 2) << "\t" << dataSorter->GetElem_Connectivity(HEXAHEDRON, iElem, 3) << "\t";
        Tecplot_File << dataSorter->GetElem_Connectivity(HEXAHEDRON, iElem, 4) << "\t" << dataSorter->GetElem_Connectivity(HEXAHEDRON, iElem, 5) << "\t";
        Tecplot_File << dataSorter->GetElem_Connectivity(HEXAHEDRON, iElem, 6) << "\t" << dataSorter->GetElem_Connectivity(HEXAHEDRON, iElem, 7) << "\n";
      }

      for (iElem = 0; iElem < nParallel_Pris; iElem++) {
        Tecplot_File << dataSorter->GetElem_Connectivity(PRISM, iElem, 0) << "\t" << dataSorter->GetElem_Connectivity(PRISM, iElem, 1) << "\t";
        Tecplot_File << dataSorter->GetElem_Connectivity(PRISM, iElem, 1) << "\t" << dataSorter->GetElem_Connectivity(PRISM, iElem, 2) << "\t";
        Tecplot_File << dataSorter->GetElem_Connectivity(PRISM, iElem, 3) << "\t" << dataSorter->GetElem_Connectivity(PRISM, iElem, 4) << "\t";
        Tecplot_File << dataSorter->GetElem_Connectivity(PRISM, iElem, 4) << "\t" << dataSorter->GetElem_Connectivity(PRISM, iElem, 5) << "\n";
      }

      for (iElem = 0; iElem < nParallel_Pyra; iElem++) {
        Tecplot_File << dataSorter->GetElem_Connectivity(PYRAMID, iElem, 0) << "\t" << dataSorter->GetElem_Connectivity(PYRAMID, iElem, 1) << "\t";
        Tecplot_File << dataSorter->GetElem_Connectivity(PYRAMID, iElem, 2) << "\t" << dataSorter->GetElem_Connectivity(PYRAMID, iElem, 3) << "\t";
        Tecplot_File << dataSorter->GetElem_Connectivity(PYRAMID, iElem, 4) << "\t" << dataSorter->GetElem_Connectivity(PYRAMID, iElem, 4) << "\t";
        Tecplot_File << dataSorter->GetElem_Connectivity(PYRAMID, iElem, 4) << "\t" << dataSorter->GetElem_Connectivity(PYRAMID, iElem, 4) << "\n";
      }


    }
    Tecplot_File.flush();
#ifdef HAVE_MPI
    SU2_MPI::Barrier(MPI_COMM_WORLD);
#endif
  }

  Tecplot_File.close();

  /*--- Compute and store the write time. ---*/

#ifndef HAVE_MPI
  stopTime = su2double(clock())/su2double(CLOCKS_PER_SEC);
#else
  stopTime = MPI_Wtime();
#endif
  usedTime = stopTime-startTime;

  fileSize = Determine_Filesize(fileName);

  /*--- Compute and store the bandwidth ---*/

  bandwidth = fileSize/(1.0e6)/usedTime;
}


