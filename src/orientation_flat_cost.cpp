/*
 * Copyright (c) 2019, The Regents of the University of California (Regents).
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
 * Authors: David Fridovich-Keil   ( dfk@eecs.berkeley.edu )
 */

///////////////////////////////////////////////////////////////////////////////
//
// Quadratic cost function of the norm of two states (difference from some
// nominal norm value), i.e. 0.5 * weight_ * (||(x, y)|| - nominal)^2.
//
///////////////////////////////////////////////////////////////////////////////

#include <ilqgames/cost/orientation_flat_cost.h>
#include <ilqgames/utils/types.h>

#include <glog/logging.h>

namespace ilqgames {

float OrientationFlatCost::Evaluate(const VectorXf& input) const {
  CHECK_LT(dim1_, input.size());
  CHECK_LT(dim2_, input.size());

  // Otherwise, cost is squared 2-norm of entire input.
  const float diff = std::atan2(input(dim2_), input(dim1_)) - nominal_;
  return 0.5 * weight_ * diff * diff;
}

void OrientationFlatCost::Quadraticize(const VectorXf& input, MatrixXf* hess,
                                     VectorXf* grad) const {
  CHECK_LT(dim1_, input.size());
  CHECK_LT(dim2_, input.size());
  CHECK_NOTNULL(hess);

  // Check dimensions.
  CHECK_EQ(input.size(), hess->rows());
  CHECK_EQ(input.size(), hess->cols());

  if (grad) CHECK_EQ(input.size(), grad->size());

  // Populate hessian and, optionally, gradient.
  const float norm = std::hypot(input(dim1_), input(dim2_));
  const float norm2 = norm * norm;
  const float theta = std::atan2(input(dim2_), input(dim1_));
  (*hess)(dim1_, dim1_) =
      (input(dim2_)*input(dim2_)*weight_ - 
        input(dim1_)*input(dim2_)*weight_*(2*nominal_ - 2*theta))/(norm2 * norm2);
  (*hess)(dim1_, dim2_) =
      -(input(dim1_)*input(dim2_)*weight_ - 
        input(dim1_)*input(dim1_)*weight_*(nominal_ - theta) + 
        input(dim2_)*input(dim2_)*weight_*(nominal_ - theta))/(norm2 * norm2);
  (*hess)(dim2_, dim2_) =
      (input(dim1_)*input(dim1_)*weight_ + 
        input(dim1_)*input(dim2_)*weight_*(2*nominal_ - 2*theta))/(norm2 * norm2);
  (*hess)(dim2_, dim1_) = (*hess)(dim1_, dim2_);

  if (grad) {
    (*grad)(dim1_) =  (input(dim2_)*weight_*(nominal_ - theta))/norm2;
    (*grad)(dim2_) = -(input(dim1_)*weight_*(nominal_ - theta))/norm2;
  }
}

}  // namespace ilqgames
