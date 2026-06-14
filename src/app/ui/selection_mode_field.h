// Aseprite
// Copyright (C) 2018  Igara Studio S.A.
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifndef APP_UI_SELECTION_MODE_FIELD_H_INCLUDED
#define APP_UI_SELECTION_MODE_FIELD_H_INCLUDED
#pragma once

#include <vector>

#include "app/pref/preferences.h"
#include "app/ui/button_set.h"
#include "pref.xml.h"

namespace ui {
class TooltipManager;
}

namespace app {

class SelectionModeField : public ButtonSet {
public:
  SelectionModeField();

  void setupTooltips(ui::TooltipManager* tooltipManager);

  gen::SelectionMode selectionMode();
  void setSelectionMode(gen::SelectionMode mode);

protected:
  void onItemChange(Item* item) override;

  virtual void onSelectionModeChange(gen::SelectionMode mode) {}
};

} // namespace app

#endif
