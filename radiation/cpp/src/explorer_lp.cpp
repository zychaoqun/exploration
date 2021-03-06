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
// Exploration on a 2D grid. Tries to find the specified number of radiation
// sources (located at random lattice points) by choosing trajectories of
// the specified number of steps that maximize mutual information between
// simulated measurements and the true map.
//
///////////////////////////////////////////////////////////////////////////////

#include <explorer_lp.h>

#include <GLUT/glut.h>
#include <glog/logging.h>
#include <gurobi_c++.h>
#include <random>
#include <string>
#include <math.h>

namespace radiation {

// Constructor/destructor.
ExplorerLP::~ExplorerLP() {}
ExplorerLP::ExplorerLP(unsigned int num_rows, unsigned int num_cols,
                       unsigned int num_sources, double regularizer,
                       unsigned int num_steps, double fov,
                       unsigned int num_samples)
  : map_(num_rows, num_cols, num_sources, regularizer),
    num_steps_(num_steps),
    num_samples_(num_samples),
    pose_(0.0, 0.0, 0.0),
    fov_(fov) {
  // Set up a random number generator.
  std::random_device rd;
  std::default_random_engine rng(rd());
  std::uniform_int_distribution<unsigned int> unif_rows(0, num_rows - 1);
  std::uniform_int_distribution<unsigned int> unif_cols(0, num_cols - 1);
  std::uniform_real_distribution<double> unif_angle(0.0, 2.0 * M_PI);

  // Choose random sources.
  for (unsigned int ii = 0; ii < num_sources; ii++) {
    const Source2D source(unif_rows(rng), unif_cols(rng));
    sources_.push_back(source);
  }

  // Choose a random initial pose.
  pose_ = GridPose2D(unif_rows(rng), unif_cols(rng), unif_angle(rng));
}

// Plan a new trajectory.
bool ExplorerLP::PlanAhead(std::vector<GridPose2D>& trajectory) {
  // Generate conditional entropy vector.
  Eigen::VectorXd hzx;
  std::vector<unsigned int> trajectory_ids;
  map_.GenerateEntropyVector(num_samples_, num_steps_, pose_, fov_,
                             hzx, trajectory_ids);
  CHECK(hzx.rows() == trajectory_ids.size());

  // Compute the arg max of this conditional entropy vector.
  double max_value = -1.0;
  unsigned int trajectory_id = 0;
  for (unsigned int ii = 0; ii < hzx.rows(); ii++) {
    if (hzx(ii) > max_value) {
      max_value = hzx(ii);
      trajectory_id = trajectory_ids[ii];
    }
  }

  // Check that we found a valid trajectory (with non-negative entropy).
  if (max_value < 0.0) {
    VLOG(1) << "Could not find a positive conditional entropy trajectory.";
    return false;
  }

  // Decode this trajectory id.
  trajectory.clear();
  DecodeTrajectory(trajectory_id, num_steps_, pose_, trajectory);
  return true;
}

// Take a step along the given trajectory. Return resulting entropy.
double ExplorerLP::TakeStep(const std::vector<GridPose2D>& trajectory) {
  CHECK(trajectory.size() > 0);

  // Update list of past poses.
  past_poses_.push_back(pose_);

  // Update the current pose.
  pose_ = trajectory[0];

  // Update the map, and return entropy.
  const Sensor2D sensor(pose_, fov_);
  map_.Update(sensor, sources_, true);

  return map_.Entropy();
}

// Compute map entropy.
double ExplorerLP::Entropy() const { return map_.Entropy(); }

// Visualize the current belief state.
void ExplorerLP::Visualize() const {
  glClear(GL_COLOR_BUFFER_BIT);

  // Display each grid cell as a GL_QUAD centered at the appropriate location,
  // with a small 'epsilon' fudge factor between cells.
  const Eigen::MatrixXd belief = map_.GetImmutableBelief();
  const GLfloat kEpsilon = 0.02;

  glBegin(GL_QUADS);
  for (unsigned int ii = 0; ii < map_.GetNumRows(); ii++) {
    for (unsigned int jj = 0; jj < map_.GetNumCols(); jj++) {
      glColor3f(static_cast<GLfloat>(belief(ii, jj)),
                static_cast<GLfloat>(belief(ii, jj)),
                static_cast<GLfloat>(belief(ii, jj)));

      // Bottom left, bottom right, top right, top left.
      glVertex2f(static_cast<GLfloat>(ii) + kEpsilon,
                 static_cast<GLfloat>(jj) + kEpsilon);
      glVertex2f(static_cast<GLfloat>(ii) + 1.0 - kEpsilon,
                 static_cast<GLfloat>(jj) + kEpsilon);
      glVertex2f(static_cast<GLfloat>(ii) + 1.0 - kEpsilon,
                 static_cast<GLfloat>(jj) + 1.0 - kEpsilon);
      glVertex2f(static_cast<GLfloat>(ii) + kEpsilon,
                 static_cast<GLfloat>(jj) + 1.0 - kEpsilon);
    }
  }
  glEnd();

  const GLfloat robot_x = static_cast<GLfloat>(pose_.GetX());
  const GLfloat robot_y = static_cast<GLfloat>(pose_.GetY());
  const GLfloat robot_a = static_cast<GLfloat>(pose_.GetAngle());
  const unsigned int kNumVertices = 100;

  // Display the field of view as a triangle fan.
  const GLfloat kFovRadius =
    sqrt(static_cast<GLfloat>(map_.GetNumRows() * map_.GetNumRows() +
                            map_.GetNumCols() * map_.GetNumCols()));

  glBegin(GL_TRIANGLE_FAN);
  glColor4f(0.0, 0.2, 0.8, 0.2);
  glVertex2f(robot_x, robot_y);
  for (unsigned int ii = 0; ii <= kNumVertices; ii++) {
    const GLfloat angle = robot_a + fov_ *
      (-0.5 + static_cast<GLfloat>(ii) / static_cast<GLfloat>(kNumVertices));
    glVertex2f(robot_x + kFovRadius * cos(angle),
               robot_y + kFovRadius * sin(angle));
  }
  glEnd();

  // Display a circle at the robot's current position. No circle primitive, so
  // use a polygon with a bunch of vertices.
  const GLfloat kRobotRadius = 0.5;

  glBegin(GL_POLYGON);
  glColor4f(0.0, 0.8, 0.2, 0.5);
  for (unsigned int ii = 0; ii < kNumVertices; ii++) {
    const GLfloat angle = 2.0 * M_PI *
      static_cast<GLfloat>(ii) / static_cast<GLfloat>(kNumVertices);
    glVertex2f(robot_x + kRobotRadius * cos(angle),
               robot_y + kRobotRadius * sin(angle));
  }
  glEnd();

  // Display a circle at the location of each source.
  const GLfloat kSourceRadius = 0.2;

  for (const auto& source : sources_) {
    glBegin(GL_POLYGON);
    glColor4f(0.8, 0.0, 0.2, 0.5);

    for (unsigned int ii = 0; ii < kNumVertices; ii++) {
      const GLfloat angle = 2.0 * M_PI *
        static_cast<GLfloat>(ii) / static_cast<GLfloat>(kNumVertices);
      glVertex2f(source.GetX() + kSourceRadius * cos(angle),
                 source.GetY() + kSourceRadius * sin(angle));
    }
    glEnd();
  }
}

} // namespace radiation
