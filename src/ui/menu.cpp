#include "ui/menu.h"

namespace s3d::ui {

void MenuModel::set_items(std::vector<MenuItem> items) {
  items_ = std::move(items);
  if (items_.empty()) {
    selected_ = 0;
    return;
  }
  if (selected_ < 0 || selected_ >= size()) selected_ = 0;
  // Land on an enabled row when possible.
  if (!items_[static_cast<size_t>(selected_)].enabled) move(1);
}

void MenuModel::set_selected(int index) {
  if (items_.empty()) {
    selected_ = 0;
    return;
  }
  if (index < 0) index = 0;
  if (index >= size()) index = size() - 1;
  selected_ = index;
  if (!items_[static_cast<size_t>(selected_)].enabled) move(1);
}

const MenuItem* MenuModel::current() const {
  if (selected_ < 0 || selected_ >= size()) return nullptr;
  return &items_[static_cast<size_t>(selected_)];
}

MenuItem* MenuModel::current() {
  if (selected_ < 0 || selected_ >= size()) return nullptr;
  return &items_[static_cast<size_t>(selected_)];
}

void MenuModel::move(int delta) {
  const int n = size();
  if (n == 0 || delta == 0) return;
  const int step = delta > 0 ? 1 : -1;
  int remaining = delta > 0 ? delta : -delta;
  int index = selected_;
  // Walk one row at a time so a delta larger than the list still wraps sanely.
  while (remaining-- > 0) {
    for (int i = 0; i < n; ++i) {
      index = (index + step + n) % n;
      if (items_[static_cast<size_t>(index)].enabled) break;
    }
  }
  // If nothing was enabled the walk left `index` on some disabled row; only
  // commit the move when we actually landed on an enabled row.
  if (items_[static_cast<size_t>(index)].enabled) selected_ = index;
}

void MenuModel::adjust(int delta) {
  MenuItem* item = current();
  if (item == nullptr || !item->enabled || !item->has_values() || delta == 0) return;
  const int count = static_cast<int>(item->values.size());
  item->value_index = ((item->value_index + delta) % count + count) % count;
}

std::string MenuModel::current_value() const {
  const MenuItem* item = current();
  if (item == nullptr || !item->has_values()) return {};
  const int count = static_cast<int>(item->values.size());
  if (item->value_index < 0 || item->value_index >= count) return {};
  return item->values[static_cast<size_t>(item->value_index)];
}

}  // namespace s3d::ui
