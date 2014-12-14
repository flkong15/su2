## \file geometry.py
#  \brief python package for running geometry analyses
#  \author T. Lukaczyk, F. Palacios
#  \version 3.2.5 "eagle"
#
# Copyright (C) 2012-2014 SU2 <https://github.com/su2code>.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# ----------------------------------------------------------------------
#  Imports
# ----------------------------------------------------------------------

import os, sys, shutil, copy

from .. import io  as su2io
from decompose import decompose as su2decomp
from interface import GEO       as SU2_GEO
from ..util import ordered_bunch

# ----------------------------------------------------------------------
#  Direct Simulation
# ----------------------------------------------------------------------

def geometry ( config , step = 1e-3 ): 
    """ info = SU2.run.geometry(config)
        
        Runs an geometry analysis with:
            SU2.run.decomp()
            SU2.run.GEO()
            
        Assumptions:
            Redundant decomposition if config.DECOMPOSED == True
            Performs both function and gradient analysis
                        
        Inputs:
            config - an SU2 configuration
            step   - gradient finite difference step if config.GEO_MODE=GRADIENT
        
        Outputs:
            info - SU2 State with keys:
                FUNCTIONS
                GRADIENTS
                
        Updates:
            config.DECOMPOSED
            
        Executes in:
            ./
    """
    
    # local copy
    konfig = copy.deepcopy(config)
    
    # unpack
    function_name = konfig['GEO_PARAM']
    func_filename = 'of_func.dat'
    grad_filename = 'of_grad.dat'
    
    # choose dv values 
    Definition_DV = konfig['DEFINITION_DV']
    n_DV          = len(Definition_DV['KIND'])
    if isinstance(step,list):
        assert len(step) == n_DV , 'unexpected step vector length'
    else:
        step = [step]*n_DV
    dv_old = [0.0]*n_DV # SU2_DOT input requirement, assumes linear superposition of design variables
    dv_new = step
    konfig.unpack_dvs(dv_new,dv_old)    
    
    # decompose
    su2decomp(konfig)
    
    # Run Solution
    SU2_GEO(konfig)
    
    # info out
    info = su2io.State()    
    
    # get function values
    if konfig.GEO_MODE == 'FUNCTION':
        functions = su2io.tools.read_plot(func_filename)
        for key,value in functions.items():
            functions[key] = value[0]
        info.FUNCTIONS.update( functions )
    
    # get gradient_values
    if konfig.GEO_MODE == 'GRADIENT':
        gradients = su2io.tools.read_plot(grad_filename)
        info.GRADIENTS.update( gradients )
    
    # update super config
    config.update({ 'DECOMPOSED' : konfig['DECOMPOSED'] })
    
    return info
