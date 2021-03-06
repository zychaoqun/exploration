"""
Copyright (c) 2015, The Regents of the University of California (Regents).
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.

   3. Neither the name of the copyright holder nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS AS IS
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

Please contact the author(s) of this library if you have any questions.
Authors: David Fridovich-Keil   ( dfk@eecs.berkeley.edu )
"""

###########################################################################
#
# Run ExplorerLP.
#
###########################################################################

import numpy as np
from numpy import linalg as LA
import math

from sensor_2d import Sensor2D
from source_2d import Source2D
from grid_map_2d import GridMap2D
from grid_pose_2d import GridPose2D
from explorer_lp import ExplorerLP

# Create a grid map with only a couple sources.
kNumRows = 5
kNumCols = 5
kNumSources = 2
kNumSteps = 3
kNumSamples = 10000

# Set up sensor parameters.
kAngularStep = 0.4 * math.pi
kFieldOfView = 0.3 * math.pi
params = {"x" : 0.5 * kNumRows,
          "y" : 0.5 * kNumCols,
          "fov" : kFieldOfView,
          "angle" : np.random.uniform(0.0, 2.0 * math.pi)}

# Create an explorer.
explorer = ExplorerLP(kNumRows, kNumCols, kNumSources, kNumSteps,
                      kAngularStep, params, kNumSamples)

# For the specified number of iterations, plan ahead and update.
kNumIterations = 10
entropy = explorer.map_.Entropy()
for ii in range(kNumIterations):
    # Search and update.
    trajectory = explorer.PlanAhead()
    entropy = explorer.TakeStep(trajectory)

    # Visualize.
    if (ii % 1 == 0):
        explorer.Visualize("Step %d: entropy = %f" % (ii, entropy))

explorer.Visualize("Final entropy = %f" % entropy)
