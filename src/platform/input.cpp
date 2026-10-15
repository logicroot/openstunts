#include "platform/input.h"

namespace s3d {

void Input::begin_frame() {
  keyboard_ = SDL_GetKeyboardState(nullptr);

  // Reset edge-triggered flags; continuous axes are recomputed each frame.
  controls_.gear_up = false;
  controls_.gear_down = false;
  controls_.reset_car = false;
  controls_.toggle_camera = false;
  controls_.toggle_dashboard = false;
  controls_.pause = false;
  controls_.menu_up = false;
  controls_.menu_down = false;
  controls_.menu_left = false;
  controls_.menu_right = false;
  controls_.menu_accept = false;
  controls_.menu_cancel = false;
  controls_.mouse_click = false;
  controls_.mouse_moved = false;
  mouse_dx_ = mouse_dy_ = 0.0f;

  int mx = 0;
  int my = 0;
  SDL_GetMouseState(&mx, &my);
  controls_.mouse_x = static_cast<float>(mx) * mouse_sx_;
  controls_.mouse_y = static_cast<float>(my) * mouse_sy_;

  float throttle = 0.0f;
  if (keyboard_[SDL_SCANCODE_UP] || keyboard_[SDL_SCANCODE_W]) throttle += 1.0f;
  if (keyboard_[SDL_SCANCODE_DOWN] || keyboard_[SDL_SCANCODE_S]) throttle -= 1.0f;
  controls_.throttle = throttle;

  float steer = 0.0f;
  if (keyboard_[SDL_SCANCODE_RIGHT] || keyboard_[SDL_SCANCODE_D]) steer += 1.0f;
  if (keyboard_[SDL_SCANCODE_LEFT] || keyboard_[SDL_SCANCODE_A]) steer -= 1.0f;
  controls_.steer = steer;

  controls_.handbrake = keyboard_[SDL_SCANCODE_SPACE] != 0;
}

void Input::handle_event(const SDL_Event& e) {
  if (e.type == SDL_QUIT) quit_ = true;

  if (e.type == SDL_KEYDOWN && e.key.repeat == 0) {
    switch (e.key.keysym.sym) {
      // Escape is no longer a hard quit: it is the menu "back" action. Scenes
      // decide what it does (the front-end pops a screen, a race quits).
      case SDLK_ESCAPE: controls_.menu_cancel = true; break;
      case SDLK_z: controls_.gear_down = true; break;
      case SDLK_x: controls_.gear_up = true; break;
      case SDLK_r: controls_.reset_car = true; break;
      case SDLK_c: controls_.toggle_camera = true; break;
      // The original toggles the cockpit dashboard with 'D' (seg005.asm
      // handle_ingame_kb_shortcuts); the port maps A/D to steering, so TAB is
      // used instead.
      case SDLK_TAB: controls_.toggle_dashboard = true; break;
      case SDLK_p: controls_.pause = true; break;

      // Menu navigation (also mirrored by the gamepad below).
      case SDLK_UP: controls_.menu_up = true; break;
      case SDLK_DOWN: controls_.menu_down = true; break;
      case SDLK_LEFT: controls_.menu_left = true; break;
      case SDLK_RIGHT: controls_.menu_right = true; break;
      case SDLK_RETURN:
      case SDLK_KP_ENTER:
      case SDLK_SPACE: controls_.menu_accept = true; break;
      default: break;
    }
  }

  // Gamepad menu nav: D-pad + A/B (first connected controller).
  if (e.type == SDL_CONTROLLERBUTTONDOWN) {
    switch (e.cbutton.button) {
      case SDL_CONTROLLER_BUTTON_DPAD_UP: controls_.menu_up = true; break;
      case SDL_CONTROLLER_BUTTON_DPAD_DOWN: controls_.menu_down = true; break;
      case SDL_CONTROLLER_BUTTON_DPAD_LEFT: controls_.menu_left = true; break;
      case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: controls_.menu_right = true; break;
      case SDL_CONTROLLER_BUTTON_A: controls_.menu_accept = true; break;
      case SDL_CONTROLLER_BUTTON_B: controls_.menu_cancel = true; break;
      default: break;
    }
  }

  if (e.type == SDL_MOUSEMOTION) {
    mouse_dx_ += static_cast<float>(e.motion.xrel);
    mouse_dy_ += static_cast<float>(e.motion.yrel);
    controls_.mouse_moved = true;
    controls_.mouse_x = static_cast<float>(e.motion.x) * mouse_sx_;
    controls_.mouse_y = static_cast<float>(e.motion.y) * mouse_sy_;
  }

  if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
    controls_.mouse_click = true;
    controls_.mouse_x = static_cast<float>(e.button.x) * mouse_sx_;
    controls_.mouse_y = static_cast<float>(e.button.y) * mouse_sy_;
  }

  // Basic gamepad: left stick + triggers (first connected controller).
  if (e.type == SDL_CONTROLLERAXISMOTION) {
    const float v = static_cast<float>(e.caxis.value) / 32767.0f;
    switch (e.caxis.axis) {
      case SDL_CONTROLLER_AXIS_LEFTX:
        if (v < -0.2f || v > 0.2f) controls_.steer = v;
        break;
      case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
        if (v > 0.05f) controls_.throttle = v;
        break;
      case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
        if (v > 0.05f) controls_.throttle = -v;
        break;
      default: break;
    }
  }
}

}  // namespace s3d
