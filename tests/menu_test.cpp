// Menu model tests: navigation, wrapping, disabled-row skipping and value
// cycling. The model is pure logic, so these need no SDL/OpenGL.

#include "ui/menu.h"

#include <utility>

#include "tiny_test.h"

namespace {

s3d::ui::MenuItem action(std::string label, bool enabled = true) {
  s3d::ui::MenuItem it;
  it.label = std::move(label);
  it.enabled = enabled;
  return it;
}

s3d::ui::MenuItem value(std::string label, std::vector<std::string> values) {
  s3d::ui::MenuItem it;
  it.label = std::move(label);
  it.values = std::move(values);
  return it;
}

}  // namespace

TEST(menu_starts_at_first_row) {
  s3d::ui::MenuModel m({action("a"), action("b"), action("c")});
  EXPECT_EQ(m.selected(), 0);
  EXPECT_EQ(m.size(), 3);
  EXPECT_TRUE(m.current() != nullptr);
  EXPECT_EQ(m.current()->label, std::string("a"));
}

TEST(menu_down_and_up_move_selection) {
  s3d::ui::MenuModel m({action("a"), action("b"), action("c")});
  m.move_down();
  EXPECT_EQ(m.selected(), 1);
  m.move_down();
  EXPECT_EQ(m.selected(), 2);
  m.move_up();
  EXPECT_EQ(m.selected(), 1);
}

TEST(menu_wraps_both_directions) {
  s3d::ui::MenuModel m({action("a"), action("b"), action("c")});
  m.move_up();  // from the first row, wrap to the last
  EXPECT_EQ(m.selected(), 2);
  m.move_down();  // back around to the first
  EXPECT_EQ(m.selected(), 0);
}

TEST(menu_skips_disabled_rows) {
  s3d::ui::MenuModel m({action("a"), action("b", false), action("c")});
  m.move_down();  // b is disabled, so land on c
  EXPECT_EQ(m.selected(), 2);
  m.move_down();  // wrap over the disabled b to a
  EXPECT_EQ(m.selected(), 0);
  m.move_up();  // and back, still skipping b
  EXPECT_EQ(m.selected(), 2);
}

TEST(menu_single_enabled_row_stays_put) {
  s3d::ui::MenuModel m({action("a", false), action("b"), action("c", false)});
  EXPECT_EQ(m.selected(), 1);
  m.move_down();
  EXPECT_EQ(m.selected(), 1);
  m.move_up();
  EXPECT_EQ(m.selected(), 1);
}

TEST(menu_all_disabled_leaves_selection) {
  s3d::ui::MenuModel m({action("a", false), action("b", false)});
  EXPECT_EQ(m.selected(), 0);
  m.move_down();
  EXPECT_EQ(m.selected(), 0);
}

TEST(menu_set_items_lands_on_enabled_row) {
  s3d::ui::MenuModel m;
  m.set_items({action("a", false), action("b"), action("c")});
  EXPECT_EQ(m.selected(), 1);  // row 0 is disabled, so start on row 1
}

TEST(menu_set_selected_clamps) {
  s3d::ui::MenuModel m({action("a"), action("b"), action("c")});
  m.set_selected(99);
  EXPECT_EQ(m.selected(), 2);
  m.set_selected(-5);
  EXPECT_EQ(m.selected(), 0);
}

TEST(menu_value_row_cycles_and_wraps) {
  s3d::ui::MenuModel m({value("res", {"a", "b", "c"})});
  EXPECT_TRUE(m.current()->has_values());
  EXPECT_EQ(m.current_value(), std::string("a"));
  m.adjust(1);
  EXPECT_EQ(m.current_value(), std::string("b"));
  m.adjust(1);
  EXPECT_EQ(m.current_value(), std::string("c"));
  m.adjust(1);  // wrap forward
  EXPECT_EQ(m.current_value(), std::string("a"));
  m.adjust(-1);  // wrap backward
  EXPECT_EQ(m.current_value(), std::string("c"));
}

TEST(menu_action_row_has_no_values) {
  s3d::ui::MenuModel m({action("go")});
  EXPECT_TRUE(!m.current()->has_values());
  EXPECT_EQ(m.current_value(), std::string());
  m.adjust(1);  // must be a no-op, not a crash
  EXPECT_EQ(m.selected(), 0);
}

TEST(menu_disabled_value_row_does_not_cycle) {
  s3d::ui::MenuItem it = value("res", {"a", "b"});
  it.enabled = false;
  s3d::ui::MenuModel m({it});
  m.adjust(1);
  EXPECT_EQ(m.current()->value_index, 0);
}

TEST(menu_empty_is_safe) {
  s3d::ui::MenuModel m;
  EXPECT_TRUE(m.empty());
  EXPECT_TRUE(m.current() == nullptr);
  m.move_down();
  m.move_up();
  m.adjust(1);
  EXPECT_EQ(m.selected(), 0);
  EXPECT_EQ(m.current_value(), std::string());
}
