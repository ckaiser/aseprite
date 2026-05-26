// Aseprite
// Copyright (C) 2026-present  Igara Studio S.A.
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifndef APP_UI_FOLDER_FIELD_H_INCLUDED
#define APP_UI_FOLDER_FIELD_H_INCLUDED
#pragma once

#include "app/ui/button_set.h"
#include "obs/signal.h"
#include "ui/box.h"
#include "ui/entry.h"

#include <string>

namespace app {

using namespace ui;

class PathField : public ::ui::HBox {
public:
  PathField(const std::string& title, const std::string& path);

  void setPath(const std::string& path);

  std::string path() const;
  Entry* getEntryWidget() const { return m_entry; }

  obs::signal<void(const std::string&)> Selected;
  obs::signal<void(const std::string&)> Change;

protected:
  virtual void onClickBrowse(ButtonSet::Item*);

private:
  std::string m_title;
  std::string m_lastPath;

  Entry* m_entry;
  ButtonSet* m_button;
};

} // namespace app

#endif
