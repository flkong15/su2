#!/usr/bin/env python

## \file Compute_Mpolar.py
#  \brief Python script for performing sweep of Mach number.
#  \author H. Kline (based on E. Arad compute_polar script)
#  \version 5.0.0 "Raven"
#
# SU2 Lead Developers: Dr. Francisco Palacios (Francisco.D.Palacios@boeing.com).
#                      Dr. Thomas D. Economon (economon@stanford.edu).
#
# SU2 Developers: Prof. Juan J. Alonso's group at Stanford University.
#                 Prof. Piero Colonna's group at Delft University of Technology.
#                 Prof. Nicolas R. Gauger's group at Kaiserslautern University of Technology.
#                 Prof. Alberto Guardone's group at Polytechnic University of Milan.
#                 Prof. Rafael Palacios' group at Imperial College London.
#
# Copyright (C) 2012-2017 SU2, the open-source CFD code.
#
# SU2 is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# SU2 is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with SU2. If not, see <http://www.gnu.org/licenses/>.

# imports
import numpy as np
from optparse import OptionParser
import os, sys, shutil, copy, os.path
sys.path.append(os.environ['SU2_RUN'])
import SU2

def main():
# Command Line Options
    parser = OptionParser()
    parser.add_option("-f", "--file", dest="filename",
                      help="read config from FILE", metavar="FILE")
    parser.add_option("-n", "--partitions", dest="partitions", default=1,
                      help="number of PARTITIONS", metavar="PARTITIONS")
    parser.add_option("-u", "--underRelaxation", dest="urlx", default=0.1,
                      help="under relaxation factor", metavar="URLX")
    parser.add_option("-b", "--betaDelta", dest="beta_delta", default=1.0,
                      help="magnitude of perturbation", metavar="BETA_DELTA")

    (options, args)=parser.parse_args()
    options.partitions = int( options.partitions )
    # check the typecasting
    options.beta_delta = float( options.beta_delta )
    options.urlx = float(options.urlx)

    # load config, start state
    config = SU2.io.Config(options.filename)
    state  = SU2.io.State()

    # find solution files if they exist
    state.find_files(config)

    # prepare config
    config.NUMBER_PART = options.partitions
    config.USING_UQ = 'YES'
    config.BETA_DELTA = options.beta_delta
    config.URLX = options.urlx
    config.PERMUTE = 'NO'

#    if options.baseline == "":
#        print "\n\n ===================== Performing Baseline Computation ===================== \n\n"
#        config.USING_UQ = "NO"
#        info = SU2.run.CFD(config)
#        state.update(info)
	#config.SOLUTION_FLOW_FILENAME = config.RESTART_FLOW_FILENAME
#    else:
#	config.SOLUTION_FLOW_FILENAME = options.baseline

    # perform eigenvalue perturbations
    for comp in range(1,4):
        print "\n\n =================== Performing " + str(comp) + "  Component Perturbation =================== \n\n"

        # make copies
        konfig = copy.deepcopy(config)
        ztate  = copy.deepcopy(state)

	    # set componentality
        konfig.COMPONENTALITY = comp

        # send output to a folder
        folderName = str(comp)+'c/'
        if os.path.isdir(folderName):
           os.system('rm -R '+folderName)
        os.system('mkdir ' + folderName)
        sendOutputFiles(konfig, folderName)

        # run su2
        info = SU2.run.CFD(konfig)
        ztate.update(info)

    print "\n\n =================== Performing p1c1 Component Perturbation =================== \n\n"

    # make copies
    konfig = copy.deepcopy(config)
    ztate  = copy.deepcopy(state)

    # set componentality
    konfig.COMPONENTALITY = 1
    konfig.PERMUTE = 'YES'

    # send output to a folder
    folderName = 'p1c1/'
    if os.path.isdir(folderName):
       os.system('rm -R '+folderName)
    os.system('mkdir ' + folderName)
    sendOutputFiles(konfig, folderName)

    # run su2
    info = SU2.run.CFD(konfig)
    ztate.update(info)

    print "\n\n =================== Performing p1c2 Component Perturbation =================== \n\n"

    # make copies
    konfig = copy.deepcopy(config)
    ztate  = copy.deepcopy(state)

    # set componentality
    konfig.COMPONENTALITY = 2
    konfig.PERMUTE = 'YES'

    # send output to a folder
    folderName = 'p1c2/'
    if os.path.isdir(folderName):
       os.system('rm -R '+folderName)
    os.system('mkdir ' + folderName)
    sendOutputFiles(konfig, folderName)

    # run su2
    info = SU2.run.CFD(konfig)
    ztate.update(info)

def sendOutputFiles( config, folderName = ''):
    config.CONV_FILENAME = folderName + config.CONV_FILENAME
    #config.BREAKDOWN_FILENAME = folderName + config.BREAKDOWN_FILENAME
    config.RESTART_FLOW_FILENAME = folderName + config.RESTART_FLOW_FILENAME
    config.VOLUME_FLOW_FILENAME = folderName + config.VOLUME_FLOW_FILENAME
    config.SURFACE_FLOW_FILENAME = folderName + config.SURFACE_FLOW_FILENAME


if __name__ == "__main__":
    main()
