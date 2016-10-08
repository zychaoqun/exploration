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
# Defines a grid map in 2D, with the specified dimensions.
# There is no true length scale in this structure; angles however are
# assumed to be metricly accurate.
#
# Implicitly, the values in the grid define a 2D non-homogeneous Poisson
# Process, which allows the random generation of possible sources. This
# is used to simulate measurements from arbitrary locations based on
# the current belief state.
#
###########################################################################

import numpy as np

class GridMap2D:
    def __init__(self, nrows, ncols, k):
        """
        Constructor. Takes in dimensions and number of sources k
        and generates a uniform prior.
        """
        self.belief_ = np.ones((nrows, ncols), dtype=np.float) * k / (nrows*ncols)
        self.k_ = k

    def Update(self, sensor):
        """
        Update belief about the world, given a sensor (with associated
        paramters, including position and orientation).

        For all voxels in range, create a rate update which is uniform
        and sums to the measurement value, then average at each point.
        """
        measurement = sensor.Sense()
        if measurement > self.k_:
            print "Measured too many sources. Did not update."
            return False

        # Identify voxels that are in range.
        update = np.copy(self.belief_)
        in_view_mask = np.zeros((self.belief_.shape), dtype=np.bool)
        in_view_count = 0
        for ii in range(self.belief_.shape[0]):
            for jj in range(self.belief_.shape[1]):
                if sensor.InView(ii, jj):
                    in_view_mask[ii, jj] = True
                    in_view_count += 1

        # Set update array.
        update[in_view_mask == True] = float(measurement) / in_view_count
        update[in_view_mask == False] = (float(self.k_ - measurement) /
                                         (update.shape[0]*update.shape[1] - in_view_count))

        # Perform update.
        self.belief_ = 0.5 * self.belief_ + 0.5 * update
        return True

    def Simulate(self, sensor, niters):
        """
        Return expected map entropy after receiving a measurement from
        the specified location/orientation. Expectation is based on
        Monte Carlo simulation using the specified number of iterations.
        """
        # TODO!

    def Entropy(self):
        """
        Compute the entropy of the map. Since we model each voxel as a
        Poisson variable, independent of all others, we use the infinite
        series (https://en.wikipedia.org/wiki/Poisson_distribution) for
        the entropy of a Poisson variable with rate \lambda at each voxel
        and sum across all voxels.
        """
        total_entropy = 0
        for ii in range(self.belief_.shape[0]):
            for jj in range(self.belief_.shape[1]):
                total_entropy += PoissonEntropy(self.belief_[ii, jj])

        return total_entropy

    def PoissonEntropy(self, rate):
        """
        Approximate entropy (in nats) of a Poisson variable with the
        given rate.
        """
        sum_max = min(6, 2*round(rate))
        entropy = rate * (1.0 - math.log(rate))

        # Add on sum_max extra terms.
        extra_terms = 0
        rate_ii = rate
        fact_ii = 1
        for ii in range(2, sum_max + 1):
            rate_ii *= rate
            fact_ii *= float(ii)
            extra_terms += rate_ii * math.log(fact_ii) / fact_ii

        # Scale extra terms and add to entropy.
        entropy += math.exp(-rate) * extra_terms
        return entropy
