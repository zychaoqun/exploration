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
# Run Explorer2D.
#
###########################################################################

import numpy as np
from numpy import linalg as LA
import math

from sensor_2d import Sensor2D
from source_2d import Source2D
from grid_map_2d import GridMap2D
from grid_pose_2d import GridPose2D
from explorer_2d import Explorer2D


# Create a grid map with only a couple sources.
kNumRows = 7
kNumCols = 7
kNumSources = 1

# Set up sensor parameters.
kAngularStep = 0.25 * math.pi
kFieldOfView = 0.2 * math.pi
params = {"x" : 0.5 * kNumRows,
          "y" : 0.5 * kNumCols,
          "fov" : kFieldOfView,
          "angle" : 0.0}

# Create an explorer.
explorer = Explorer2D(kNumRows, kNumCols, kNumSources, kAngularStep, params)

# For the specified number of steps, plan ahead and update.
kNumStepsPerTrajectory = 1
kNumTrajectories = 15
kNumIters = 1
kNumSteps = 10
entropy = explorer.map_.Entropy()
for ii in range(kNumSteps):
    # Search and update.
    trajectory = explorer.PlanAhead(kNumStepsPerTrajectory,
                                    kNumTrajectories, kNumIters)
    entropy = explorer.TakeStep(trajectory)

    # Visualize.
    if (ii % 1 == 0):
        explorer.Visualize("Step %d: entropy = %f" % (ii, entropy))

explorer.Visualize("Final entropy = %f" % entropy)
