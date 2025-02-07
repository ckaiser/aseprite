// Aseprite
// Copyright (C) 2018-present  Igara Studio S.A.
// Copyright (C) 2001-2017  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifndef APP_UI_TIMELINE_SOUND_CONTROLS_H_INCLUDED
#define APP_UI_TIMELINE_SOUND_CONTROLS_H_INCLUDED
#pragma once
#include "ui/widget.h"

namespace ui {
class TooltipManager;
}

namespace app {
class Editor;

class SoundControls : public ui::Widget {
public:
  SoundControls(ui::TooltipManager* tooltipManager);

  void updateUsingEditor(Editor* editor);

protected:
  void onResize(ui::ResizeEvent& ev) override;
  void onPaint(ui::PaintEvent& ev) override;
  bool onProcessMessage(ui::Message* msg) override;

private:
  void generateWaveform();
  void updateTooltip();

  // Used to paint the offset while we're dragging the mouse, to avoid changing it during playback.
  bool m_offsetting;
  gfx::Point m_offsetStartPosition;
  int64_t m_offsetOverride;

  Editor* m_editor;
  ui::TooltipManager* m_tooltipManager;
  os::SurfaceRef m_waveform;
};

} // namespace app

#endif
