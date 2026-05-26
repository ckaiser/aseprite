// Aseprite
// Copyright (C) 2026-present  Igara Studio S.A.
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "app/ui/path_field.h"

#include "app/app.h"
#include "app/file_selector.h"
#include "app/i18n/strings.h"
#include "app/pref/preferences.h"
#include "base/fs.h"

namespace app {

using namespace ui;

PathField::PathField(const std::string& title, const std::string& path) : m_title(title)
{
  m_entry = new Entry(256);
  m_entry->setExpansive(true);
  m_entry->setText(path);
  m_entry->Change.connect([this] { Change(m_entry->text()); });
  m_lastPath = path;

  m_button = new ButtonSet(1);
  m_button->ItemChange.connect(&PathField::onClickBrowse, this);
  m_button->addItem(Strings::select_file_browse());

  addChild(m_entry);
  addChild(m_button);

  InitTheme.connect([this] { setChildSpacing(0); });
  initTheme();
}

void PathField::setPath(const std::string& path)
{
  m_entry->setText(path);
  Change(path);
}

std::string PathField::path() const
{
  return m_entry->text();
}

void PathField::onClickBrowse(ButtonSet::Item*)
{
  m_button->setSelectedItem(nullptr);

  std::string initialPath;
  if (!m_entry->text().empty() && base::is_directory(m_entry->text())) {
    initialPath = m_entry->text();
  }
  else {
    initialPath = m_lastPath;
  }

  base::paths out;
  if (show_file_selector(m_title, initialPath, {}, FileSelectorType::OpenFolder, out)) {
    if (out.empty())
      return;

    const std::string& path = out.front();
    m_entry->setText(path);

    if (base::is_directory(path))
      m_lastPath = path;

    Selected(path);
    Change(path);
  }
}
} // namespace app
