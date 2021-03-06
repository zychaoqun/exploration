/*
 * Copyright (c) 2015, The Regents of the University of California (Regents).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *
 *    3. Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS AS IS
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Please contact the author(s) of this library if you have any questions.
 * Author: David Fridovich-Keil   ( dfk@eecs.berkeley.edu )
 */

///////////////////////////////////////////////////////////////////////////////
//
// Defines a 2D grid map.
//
///////////////////////////////////////////////////////////////////////////////

#include <grid_map_2d.h>
#include <encoding.h>
#include <cost_functors.h>

#include <ceres/ceres.h>
#include <glog/logging.h>
#include <math.h>
#include <algorithm>
#include <map>

namespace radiation {

  GridMap2D::~GridMap2D() {}
  GridMap2D::GridMap2D(unsigned int num_rows, unsigned int num_cols,
                       unsigned int num_sources, double regularizer)
    : num_rows_(num_rows), num_cols_(num_cols),
      num_sources_(num_sources), regularizer_(regularizer),
      rng_(rd_()) {

    // Initialize belief matrix to be uniform.
    belief_ = Eigen::MatrixXd::Ones(num_rows_, num_cols_) * num_sources_;
    belief_ /= num_rows_ * num_cols_;
  }

  // Getters.
  unsigned int GridMap2D::GetNumRows() const { return num_rows_; }
  unsigned int GridMap2D::GetNumCols() const { return num_cols_; }

  // Generate random sources according to the current belief state.
  bool GridMap2D::GenerateSources(std::vector<Source2D>& sources) {
    const double total_belief = belief_.sum();
    std::uniform_real_distribution<double> unif(0.0, 1.0);

    // Choose 'num_sources_' random numbers in [0, 1], which will be sorted
    // and treated as evaluations of the CDF. Since they are uniform, the points
    // at which they occur ar distributed according to the current belief.
    std::vector<double> cdf_evals;
    for (size_t ii = 0; ii < num_sources_; ii++)
      cdf_evals.push_back(unif(rng_));

    std::sort(cdf_evals.begin(), cdf_evals.end());

    // Walk the current belief distribution until we get to each 'cdf_eval'
    // and record which voxel we are in.
    sources.clear();
    unsigned int current_index = 0;
    double current_cdf = 0.0;

    for (unsigned int ii = 0; ii < num_rows_; ii++) {
      for (unsigned int jj = 0; jj < num_cols_; jj++) {
        current_cdf += belief_(ii, jj) / total_belief;

        // Check if we just passed the next 'cdf_eval'.
        while (current_cdf >= cdf_evals[current_index]) {
          // Generate a new source here.
          sources.push_back(Source2D(ii, jj));

          // Return if we have enough sources.
          if (sources.size() == num_sources_)
            return true;

          // Increment 'current_index'.
          current_index++;
        }
      }
    }

    // Should never get here.
    return false;
  }

  // Generate entropy vector [h_{Z|X}], where the i-entry of [h_{Z|X}]
  // is the entropy of Z given trajectory X = i, starting from the given pose.
  void GridMap2D::GenerateEntropyVector(
     unsigned int num_samples, unsigned int num_steps, const GridPose2D& pose,
     double sensor_fov, Eigen::VectorXd& hzx,
     std::vector<unsigned int>& trajectory_ids) {
    // Compute the number of possible measurement vectors.
    const unsigned int kNumMeasurements = pow(num_sources_ + 1, num_steps);

    // Create a map to keep track of counts for each trajectory.
    std::map<unsigned int, Eigen::VectorXd> zx_samples;

    // Generate a ton of sampled data.
    for (unsigned int ii = 0; ii < num_samples; ii++) {
      // Generate random sources on the grid according to the current 'belief',
      // and compute a corresponding 'map_id' number based on which grid cells
      // the sources lie in.
      std::vector<Source2D> sources;
      if (!GenerateSources(sources)) {
        VLOG(1) << "Unable to generate sources. Skipping this sample.";
        continue;
      }

      // Pick a random trajectory starting at the given pose. At each step,
      // take a measurement and record the data.
      GridPose2D current_pose = pose;
      std::vector<Movement2D> movements;
      std::vector<unsigned int> measurements;
      while (movements.size() < num_steps) {
        const Movement2D step;
        if (current_pose.MoveBy(step)) {
          movements.push_back(step);

          const Sensor2D sensor(current_pose, sensor_fov);
          measurements.push_back(sensor.Sense(sources));
        }
      }

      // Compute trajectory and measurement sequence ids.
      const unsigned int trajectory_id = EncodeTrajectory(movements);
      const unsigned int measurement_id =
        EncodeMeasurements(measurements, num_sources_);

      // Record this sample in the 'zx_samples' map.
      if (zx_samples.count(trajectory_id) == 0) {
        Eigen::VectorXd counts = Eigen::VectorXd::Zero(kNumMeasurements);
        counts(measurement_id) = 1.0;
        zx_samples.insert({trajectory_id, counts});
      } else {
        zx_samples.at(trajectory_id)(measurement_id) += 1.0;
      }
    }

    // Convert 'zx_samples' into a matrix joint distribution.
    const unsigned int kNumTrajectories = zx_samples.size();
    Eigen::MatrixXd pzx =
      Eigen::MatrixXd::Zero(kNumMeasurements, kNumTrajectories);
    trajectory_ids.clear();
    trajectory_ids.reserve(kNumTrajectories);

    unsigned int idx = 0;
    for (auto pair = zx_samples.begin(); pair != zx_samples.end(); pair++) {
      trajectory_ids.push_back(pair->first);
      pzx.col(idx++) = pair->second;
    }

    // Normalize so that all columns sum to unity.
    for (unsigned int jj = 0; jj < kNumTrajectories; jj++) {
      const double zx_col_sum = pzx.col(jj).sum();
      if (zx_col_sum < 1.0) {
        VLOG(1) << "Encountered trajectory with no support in P_{Z|X}.";
      } else {
        pzx.col(jj) /= zx_col_sum;
      }
    }

    // Compute [h_{Z|X}], the conditional entropy vector.
    hzx.resize(kNumTrajectories);
    for (unsigned int jj = 0; jj < kNumTrajectories; jj++) {
      hzx(jj) = 0.0;

      for (unsigned int ii = 0; ii < kNumMeasurements; ii++) {
        const double p = pzx(ii, jj);

        // Catch 'p' values near 0 or 1 to avoid numerical issues.
        if (p < 0.01 || p > 1.0 - 0.01)
          continue;

        hzx(jj) -= p * log(p);
      }

      // Make sure entropies are non-negative.
      CHECK(hzx(jj) >= 0.0);
    }
  }

  // Take a measurement from the given sensor and update belief accordingly.
  bool GridMap2D::Update(const Sensor2D& sensor,
                         const std::vector<Source2D>& sources,
                         bool solve) {
    const unsigned int measurement = sensor.Sense(sources);
    CHECK(measurement <= num_sources_);

    // Identify all voxels in range and store.
    std::vector<unsigned int> voxels;
    for (size_t ii = 0; ii < num_rows_; ii++) {
      for (size_t jj = 0; jj < num_cols_; jj++) {
        if (sensor.VoxelInView(ii, jj))
          voxels.push_back(ii + jj * num_rows_);
      }
    }

    viewed_.push_back(voxels);
    measurements_.push_back(measurement);

    // Maybe solve.
    if (solve)
      return SolveLeastSquares();

    return true;
  }

  // Compute entropy.
  double GridMap2D::Entropy() const {
    double entropy = 0.0;

    for (size_t ii = 0; ii < num_rows_; ii++) {
      for (size_t jj = 0; jj < num_cols_; jj++) {
        const double p = belief_(ii, jj);
        const double bernoulli_entropy = (p > 1e-8 && p < 1.0 - 1e-8) ?
          (-p * log(p) - (1.0 - p) * log(1.0 - p)) : 0.0;

        entropy += bernoulli_entropy;
      }
    }

    return entropy;
  }

  // Solve least squares problem to update belief state.
  bool GridMap2D::SolveLeastSquares() {
    // Create a non-linear least squares problem.
    ceres::Problem problem;

    // Add residual blocks for each set of observed voxels and their
    // associated measurements. Note that 'belief_' is laid out in column-
    // major order by default, so we can just use its underlying structure
    // as the optimization variable.
    for (size_t ii = 0; ii < viewed_.size(); ii++) {
      problem.AddResidualBlock(
        BeliefError::Create(num_rows_, num_cols_,
                            &viewed_[ii], measurements_[ii]),
        NULL, /* squared loss */
        belief_.data());
    }

    // Add a final residual block to enforce consistancy across the entire grid,
    // i.e. that the expected number of sources matches the specified number.
    problem.AddResidualBlock(
      BeliefRegularization::Create(num_rows_, num_cols_, num_sources_,
                                   regularizer_ * viewed_.size()),
      NULL, /* squared loss */
      belief_.data());

    // Set bounds constraints. Each voxel's belief should be a probability
    // between 0 and 1.
    for (int ii = 0; ii < num_rows_ * num_cols_; ii++) {
      problem.SetParameterLowerBound(belief_.data(), ii, 0.0);
      problem.SetParameterUpperBound(belief_.data(), ii, 1.0);
    }

    // Set up solver options.
    ceres::Solver::Summary summary;
    ceres::Solver::Options options;
    options.linear_solver_type = ceres::DENSE_QR;
    options.function_tolerance = 1e-16;
    options.gradient_tolerance = 1e-16;
    options.trust_region_strategy_type = ceres::LEVENBERG_MARQUARDT;

    // Solve and return.
    ceres::Solve(options, &problem, &summary);

    return summary.IsSolutionUsable();
  }

  // Get a reference to immutable 'belief'.
  const Eigen::MatrixXd& GridMap2D::GetImmutableBelief() const {
    return belief_;
  }

} // namespace radiation
