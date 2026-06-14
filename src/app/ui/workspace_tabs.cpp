// Aseprite
// Copyright (C) 2001-2015  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.
#include "app/ui/workspace_tabs.h"
#include "base/debug.h"

namespace app {

using namespace ui;

WorkspaceTabs::WorkspaceTabs(TabsDelegate* tabsDelegate) : Tabs(tabsDelegate), m_panel(nullptr)
{
}

WorkspaceTabs::~WorkspaceTabs()
{
}

void WorkspaceTabs::setPanel(WorkspacePanel* panel)
{
  ASSERT(!m_panel);
  m_panel = panel;
}

} // namespace app
