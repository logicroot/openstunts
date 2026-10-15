// Input edge-latching tests.
//
// The application polls SDL events once per rendered frame but only advances
// the simulation when the fixed-step accumulator has a step ready. A frame can
// therefore run zero steps, and an edge-triggered action (a menu key, a click)
// must survive until an update actually consumes it. Clearing the flags in
// begin_frame() instead dropped most presses and made the menu feel
// unresponsive; these tests pin the latch behaviour.

#include "platform/input.h"

#include "tiny_test.h"

using namespace s3d;
using namespace s3d_test;

namespace {

SDL_Event key_down(SDL_Keycode sym) {
  SDL_Event e{};
  e.type = SDL_KEYDOWN;
  e.key.type = SDL_KEYDOWN;
  e.key.repeat = 0;
  e.key.keysym.sym = sym;
  return e;
}

SDL_Event mouse_down(int x, int y) {
  SDL_Event e{};
  e.type = SDL_MOUSEBUTTONDOWN;
  e.button.type = SDL_MOUSEBUTTONDOWN;
  e.button.button = SDL_BUTTON_LEFT;
  e.button.x = x;
  e.button.y = y;
  return e;
}

}  // namespace

// A key press is not lost when begin_frame() runs without an intervening
// update: it stays pending until consume_edges().
TEST(input_edges_survive_zero_step_frames) {
  Input in;
  in.handle_event(key_down(SDLK_UP));
  in.begin_frame();
  EXPECT_TRUE(in.get().menu_up);

  // A frame that ran no simulation step still reports the pending press.
  in.begin_frame();
  EXPECT_TRUE(in.get().menu_up);

  in.consume_edges();
  EXPECT_TRUE(!in.get().menu_up);
}

// Once consumed, an action is not replayed on the next frame (so one press
// performs exactly one menu action even when several steps run per frame).
TEST(input_edges_consumed_once) {
  Input in;
  in.handle_event(key_down(SDLK_RETURN));

  in.begin_frame();
  EXPECT_TRUE(in.get().menu_accept);
  in.consume_edges();

  in.begin_frame();
  EXPECT_TRUE(!in.get().menu_accept);
}

// Clicks latch the same way as keys.
TEST(input_click_latches) {
  Input in;
  in.handle_event(mouse_down(40, 50));
  in.begin_frame();
  EXPECT_TRUE(in.get().mouse_click);

  in.consume_edges();
  in.begin_frame();
  EXPECT_TRUE(!in.get().mouse_click);
}

// Continuous axes are sampled from the keyboard each frame, so they are not
// affected by the edge latch (a held key keeps driving, a tap does not latch an
// axis value).
TEST(input_axes_are_per_frame) {
  Input in;
  in.handle_event(key_down(SDLK_UP));  // a tap, not a held key
  in.begin_frame();
  EXPECT_EQ(in.get().throttle, 0.0f);
}
