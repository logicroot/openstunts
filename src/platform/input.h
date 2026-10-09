#pragma once
// Keyboard / gamepad input mapped to game controls.

#include <SDL2/SDL.h>

namespace s3d {

// Analog + digital control state for one frame.
struct Controls {
  float throttle = 0.0f;   // -1 (brake) .. +1 (accelerate)
  float steer = 0.0f;      // -1 (left) .. +1 (right)
  bool handbrake = false;
  bool gear_up = false;    // edge-triggered
  bool gear_down = false;  // edge-triggered
  bool reset_car = false;  // edge-triggered
  bool toggle_camera = false;
  bool pause = false;
};

class Input {
 public:
  // Call once per frame before reading get().
  void begin_frame();
  // Feed SDL events (edge-triggered actions are detected here).
  void handle_event(const SDL_Event& e);

  const Controls& get() const { return controls_; }
  bool quit_requested() const { return quit_; }

  // Debug-camera helpers (kept here so the fly camera can share the keyboard).
  bool key_down(SDL_Scancode sc) const {
    return keyboard_ != nullptr && keyboard_[sc] != 0;
  }
  float mouse_dx() const { return mouse_dx_; }
  float mouse_dy() const { return mouse_dy_; }

 private:
  Controls controls_{};
  const Uint8* keyboard_ = nullptr;
  bool quit_ = false;
  float mouse_dx_ = 0.0f;
  float mouse_dy_ = 0.0f;
};

}  // namespace s3d
