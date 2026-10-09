#pragma once
// Fixed-timestep accumulator for deterministic simulation.

namespace s3d {

class FixedTimestep {
 public:
  // steps_per_second: simulation rate. max_frame: clamp to avoid spiral of death.
  FixedTimestep(double steps_per_second, double max_frame_seconds = 0.25)
      : dt_(1.0 / steps_per_second), max_frame_(max_frame_seconds) {}

  // Advance by `frame_seconds`; returns the number of simulation steps to run.
  int advance(double frame_seconds) {
    if (frame_seconds > max_frame_) frame_seconds = max_frame_;
    accumulator_ += frame_seconds;
    int steps = 0;
    while (accumulator_ >= dt_) {
      accumulator_ -= dt_;
      ++steps;
    }
    return steps;
  }

  // Interpolation factor in [0,1) between the last two simulation states.
  double alpha() const { return accumulator_ / dt_; }
  double step_seconds() const { return dt_; }

  void reset() { accumulator_ = 0.0; }

 private:
  double dt_;
  double max_frame_;
  double accumulator_ = 0.0;
};

}  // namespace s3d
