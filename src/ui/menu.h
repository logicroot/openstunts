#pragma once
// Keyboard/gamepad menu model for the front-end.
//
// A menu is a vertical list of rows. Each row is either an *action* (Enter
// activates it) or a *value* row (Left/Right cycle through its values). Rows can
// be disabled: they are drawn greyed and the selection steps over them. The
// model is pure logic -- no SDL, no OpenGL -- so it is unit-tested directly; a
// scene maps the Controls' menu_* flags onto it and interprets `selected()`.

#include <string>
#include <utility>
#include <vector>

namespace s3d::ui {

struct MenuItem {
  MenuItem() = default;
  // Convenience for action rows: MenuItem{"Let's Drive"}.
  MenuItem(std::string text, bool on = true)
      : label(std::move(text)), enabled(on) {}

  std::string label;
  bool enabled = true;
  // Value rows: the choices Left/Right cycles, e.g. a resolution list. Empty
  // for plain action rows.
  std::vector<std::string> values;
  int value_index = 0;

  bool has_values() const { return !values.empty(); }
};

class MenuModel {
 public:
  MenuModel() = default;
  explicit MenuModel(std::vector<MenuItem> items) { set_items(std::move(items)); }

  void set_items(std::vector<MenuItem> items);

  const std::vector<MenuItem>& items() const { return items_; }
  int size() const { return static_cast<int>(items_.size()); }
  bool empty() const { return items_.empty(); }

  int selected() const { return selected_; }
  void set_selected(int index);

  const MenuItem* current() const;
  MenuItem* current();

  // Move the selection by `delta` rows, wrapping and stepping over disabled
  // rows. A menu with no enabled rows leaves the selection unchanged.
  void move(int delta);
  void move_up() { move(-1); }
  void move_down() { move(1); }

  // Cycle the current row's value by `delta`, wrapping. No-op on action rows or
  // disabled rows.
  void adjust(int delta);

  // The value string shown on the right of the current value row (empty for
  // action rows or an out-of-range index).
  std::string current_value() const;

 private:
  std::vector<MenuItem> items_;
  int selected_ = 0;
};

}  // namespace s3d::ui
