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
// Core renderer for 2D top-down trajectories. Integrates with DearImGui.
//
///////////////////////////////////////////////////////////////////////////////

#include <ilqgames/gui/control_sliders.h>
#include <ilqgames/gui/top_down_renderer.h>
#include <ilqgames/utils/solver_log.h>
#include <ilqgames/utils/operating_point.h>
#include <ilqgames/utils/types.h>

#include <imgui/imgui.h>
#include <math.h>
#include <vector>

namespace ilqgames {

namespace {
// Zoom paramters.
static constexpr float kPixelsToZoomConversion = 1.0 / 20.0;
static constexpr float kMinZoom = 2.0;
}  // anonymous namespace

void TopDownRenderer::Render() {
  // Do nothing if no iterates yet.
  if (log_->NumIterates() == 0) return;
  const size_t num_agents = x_idxs_.size();

  // Set up main top-down viewer window.
  ImGui::Begin("Top-Down View");

  // Set up child window displaying key codes for navigation and zoom.
  if (ImGui::BeginChild("User Guide")) {
    ImGui::TextUnformatted("Press \"c\" key to enable navigation.");
    ImGui::TextUnformatted("Press \"z\" key to change zoom.");
  }
  ImGui::EndChild();

  // Update last mouse position if "c" or "z" key was just pressed.
  constexpr bool kCatchRepeats = false;
  if (ImGui::IsKeyPressed(ImGui::GetIO().KeyMap[ImGuiKey_C], kCatchRepeats) ||
      ImGui::IsKeyPressed(ImGui::GetIO().KeyMap[ImGuiKey_Z], kCatchRepeats))
    last_mouse_position_ = ImGui::GetMousePos();
  else if (ImGui::IsKeyReleased(ImGui::GetIO().KeyMap[ImGuiKey_C])) {
    // When "c" is released, update center delta.
    const ImVec2 mouse_position = ImGui::GetMousePos();
    center_delta_.x +=
        PixelsToLength(mouse_position.x - last_mouse_position_.x);
    center_delta_.y -=
        PixelsToLength(mouse_position.y - last_mouse_position_.y);
  } else if (ImGui::IsKeyReleased(ImGui::GetIO().KeyMap[ImGuiKey_Z])) {
    // When "z" is released, update pixel to meter ratio.
    const float mouse_delta_y = ImGui::GetMousePos().y - last_mouse_position_.y;
    pixel_to_meter_ratio_ =
        std::max(kMinZoom, pixel_to_meter_ratio_ -
                               kPixelsToZoomConversion * mouse_delta_y);
  }

  // Get the draw list for this window.
  ImDrawList* draw_list = ImGui::GetWindowDrawList();

  // (1) Draw this trajectory iterate.
  const ImU32 trajectory_color = ImColor(ImVec4(1.0, 1.0, 1.0, 0.5));
  const float trajectory_thickness = std::min(1.0f, LengthToPixels(0.5));

  ImVec2 points[log_->NumTimeSteps()];
  for (size_t ii = 0; ii < num_agents; ii++) {
    for (size_t kk = 0; kk < log_->NumTimeSteps(); kk++) {
      points[kk] = PositionToWindowCoordinates(
          log_->State(sliders_->SolverIterate(), kk, x_idxs_[ii]),
          log_->State(sliders_->SolverIterate(), kk, y_idxs_[ii]));
    }

    constexpr bool kPolylineIsClosed = false;
    draw_list->AddPolyline(points, log_->NumTimeSteps(), trajectory_color,
                           kPolylineIsClosed, trajectory_thickness);
  }

  // Agent colors will all be greenish. Also specify circle radius and triangle
  // base and height (in pixels).
  const ImU32 agent_color = ImColor(ImVec4(0.0, 0.75, 0.15, 1.0));
  const float agent_radius = std::min(5.0f, LengthToPixels(2.5));
  const float agent_base = std::min(6.0f, LengthToPixels(3.0));
  const float agent_height = std::min(10.0f, LengthToPixels(5.0));

  // Draw each position as either an isosceles triangle (if heading idx is
  // >= 0) or a circle (if heading idx < 0).
  for (size_t ii = 0; ii < num_agents; ii++) {
    const ImVec2 p = PositionToWindowCoordinates(
        log_->InterpolateState(sliders_->SolverIterate(),
                               sliders_->InterpolationTime(), x_idxs_[ii]),
        log_->InterpolateState(sliders_->SolverIterate(),
                               sliders_->InterpolationTime(), y_idxs_[ii]));

    if (heading_idxs_[ii] < 0)
      draw_list->AddCircleFilled(p, agent_radius, agent_color);
    else {
      const float heading = HeadingToWindowCoordinates(log_->InterpolateState(
          sliders_->SolverIterate(), sliders_->InterpolationTime(),
          heading_idxs_[ii]));
      const float cheading = std::cos(heading);
      const float sheading = std::sin(heading);

      // Triangle vertices (top, bottom left, bottom right in Frenet frame).
      // NOTE: this may not be in CCW order. Not sure if that matters.
      const ImVec2 top(p.x + agent_height * cheading,
                       p.y + agent_height * sheading);
      const ImVec2 bl(p.x - 0.5 * agent_base * sheading,
                      p.y + 0.5 * agent_base * cheading);
      const ImVec2 br(p.x + 0.5 * agent_base * sheading,
                      p.y - 0.5 * agent_base * cheading);

      draw_list->AddTriangleFilled(bl, br, top, agent_color);
    }
  }

  ImGui::End();
}

inline float TopDownRenderer::CurrentZoomLevel() const {
  float conversion = pixel_to_meter_ratio_;

  // Handle "z" down case.
  if (ImGui::IsKeyDown(ImGui::GetIO().KeyMap[ImGuiKey_Z])) {
    const float mouse_delta_y = ImGui::GetMousePos().y - last_mouse_position_.y;
    conversion = std::max(kMinZoom,
                          conversion - kPixelsToZoomConversion * mouse_delta_y);
  }

  return conversion;
}

inline ImVec2 TopDownRenderer::PositionToWindowCoordinates(float x,
                                                           float y) const {
  ImVec2 coords = WindowCenter();

  // Offsets if "c" key is currently down.
  if (ImGui::IsKeyDown(ImGui::GetIO().KeyMap[ImGuiKey_C])) {
    const ImVec2 mouse_position = ImGui::GetMousePos();
    x += PixelsToLength(mouse_position.x - last_mouse_position_.x);
    y -= PixelsToLength(mouse_position.y - last_mouse_position_.y);
  }

  coords.x += LengthToPixels(x + center_delta_.x);
  coords.y -= LengthToPixels(y + center_delta_.y);
  return coords;
}

inline ImVec2 TopDownRenderer::WindowCenter() const {
  const ImVec2 window_pos = ImGui::GetWindowPos();
  const float window_width = ImGui::GetWindowWidth();
  const float window_height = ImGui::GetWindowHeight();

  const float center_x = window_pos.x + 0.5 * window_width;
  const float center_y = window_pos.y + 0.5 * window_height;
  return ImVec2(center_x, center_y);
}

}  // namespace ilqgames
