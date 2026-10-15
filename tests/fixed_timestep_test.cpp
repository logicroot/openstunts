#include "core/fixed_timestep.h"

#include "tiny_test.h"

using namespace s3d;

// The screenshot path advances the loop by exactly one simulation step per
// rendered frame instead of by the wall clock, so rendered frame N always lands
// on the same simulated time and an offline capture is reproducible no matter
// how long the frames took to draw. That relies on advance(step_seconds())
// returning exactly one step with no accumulator drift: if it left a fraction of
// a step behind, the number of steps behind a capture would depend on how the
// frame times happened to split.
TEST(fixed_timestep_one_step_per_fixed_frame) {
  FixedTimestep ts(60.0);
  const double step = ts.step_seconds();
  EXPECT_EQ(ts.alpha(), 0.0);
  for (int i = 0; i < 1000; ++i) {
    EXPECT_EQ(ts.advance(step), 1);
    EXPECT_EQ(ts.alpha(), 0.0);
  }
}

// A frame that stalls is clamped so one long frame cannot queue up an unbounded
// number of catch-up steps.
TEST(fixed_timestep_clamps_long_frames) {
  FixedTimestep ts(60.0, 0.25);
  const int steps = ts.advance(10.0);
  EXPECT_TRUE(steps >= 14 && steps <= 15);  // 0.25 s at 60 Hz
}

// The accumulator carries the remainder across frames, so fractional frames
// still add up to a deterministic total.
TEST(fixed_timestep_accumulates_remainder) {
  FixedTimestep ts(60.0);
  int total = 0;
  for (int i = 0; i < 30; ++i) total += ts.advance(2.0 * ts.step_seconds());
  EXPECT_EQ(total, 60);
  EXPECT_EQ(ts.alpha(), 0.0);
}
