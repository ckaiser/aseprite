// Aseprite
// Copyright (C) 2001-2016  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.
#include "app/ui/editor/scrolling_state.h"
#include "app/ui/editor/editor.h"
#include "ui/cursor_type.h"
#include "ui/message.h"
#include "ui/view.h"

namespace app {

using namespace ui;

ScrollingState::ScrollingState()
{
}

bool ScrollingState::onMouseDown(Editor* editor, MouseMessage* msg)
{
  m_oldPos = msg->position();

  editor->captureMouse();
  return true;
}

bool ScrollingState::onMouseUp(Editor* editor, MouseMessage* msg)
{
  editor->backToPreviousState();
  editor->releaseMouse();
  return true;
}

bool ScrollingState::onMouseMove(Editor* editor, MouseMessage* msg)
{
  View* view = View::getView(editor);
  gfx::Point scroll = view->viewScroll();
  gfx::Point newPos = msg->position();

#ifdef _WIN32
  if (newPos != editor->autoScroll(msg, AutoScroll::ScrollDir)) {
    m_oldPos = newPos;
    return true;
  }
#endif

  scroll -= newPos - m_oldPos;
  m_oldPos = newPos;

  editor->setEditorScroll(scroll);
  return true;
}

bool ScrollingState::onSetCursor(Editor* editor, const gfx::Point& mouseScreenPos)
{
  editor->showMouseCursor(kScrollCursor);
  return true;
}

bool ScrollingState::onKeyDown(Editor* editor, KeyMessage* msg)
{
  return false;
}

bool ScrollingState::onKeyUp(Editor* editor, KeyMessage* msg)
{
  return false;
}

bool ScrollingState::onUpdateStatusBar(Editor* editor)
{
  return false;
}

} // namespace app
