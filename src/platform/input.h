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
  bool toggle_dashboard = false;  // edge-triggered (in-race cockpit panel)
  bool pause = false;

  // Front-end menu navigation (edge-triggered: one press per key/button event).
  // A scene maps these onto its menu; driving scenes ignore them.
  bool menu_up = false;
  bool menu_down = false;
  bool menu_left = false;
  bool menu_right = false;
  bool menu_accept = false;  // Enter / Space / gamepad A
  bool menu_cancel = false;  // Escape / gamepad B (back one screen)

  // Front-end mouse: the cursor position in *drawable* pixels (-1 when there is
  // no cursor), an edge-triggered primary-button click and an edge-triggered
  // motion flag. Hovering only acts on frames where the mouse moved, so a
  // stationary cursor never fights the keyboard selection.
  float mouse_x = -1.0f;
  float mouse_y = -1.0f;
  bool mouse_click = false;
  bool mouse_moved = false;
};

class Input {
 public:
  // Call once per frame before reading get().
  void begin_frame();
  // Feed SDL events (edge-triggered actions are detected here).
  void handle_event(const SDL_Event& e);

  // SDL reports the cursor in window pixels; the application supplies the
  // drawable/window ratio so Controls carries framebuffer pixels instead.
  void set_mouse_scale(float sx, float sy) {
    mouse_sx_ = sx;
    mouse_sy_ = sy;
  }

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
  float mouse_sx_ = 1.0f;
  float mouse_sy_ = 1.0f;
};

}  // namespace s3d
