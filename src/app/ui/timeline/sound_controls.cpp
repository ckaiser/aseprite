// Aseprite
// Copyright (C) 2018-present  Igara Studio S.A.
// Copyright (C) 2001-2017  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#include "sound_controls.h"

#include "app/ui/editor/editor.h"
#include "app/ui/skin/skin_theme.h"
#include "fmt/format.h"
#include "os/system.h"
#include "ui/paint_event.h"
#include "ui/system.h"
#include "ui/tooltips.h"

using namespace ui;

namespace app {

SoundControls::SoundControls(ui::TooltipManager* tooltipManager)
  : Widget(kGenericWidget)
  , m_offsetOverride(0)
  , m_editor(nullptr)
  , m_tooltipManager(tooltipManager)
{
  TRACE("Initializing sound controls.");

  // addChild(new BoxFiller());
  // addChild(new Label("00:00"));

  setTransparent(true);
  initTheme();
}

void SoundControls::onPaint(PaintEvent& ev)
{
  const auto* theme = skin::SkinTheme::get(this);
  Graphics* g = ev.graphics();
  const gfx::Rect rc = clientBounds();

  int32_t offset = 0;
  if (m_editor) {
    offset = m_offsetting ? m_offsetOverride : m_editor->audioPlayer().offset();
  }

  if (offset != 0) {
    // Only draw this when we're gonna show it, when we have an offset of some kind.
    Paint bg;
    bg.color(theme->colors.face());
    bg.style(Paint::Fill);
    g->drawRect(rc, bg);
  }

  if (m_waveform && m_editor) {
    g->drawSurface(m_waveform.get(), (rc.x + guiscale()) - offset, rc.y + guiscale());
  }

  Paint outline;
  outline.color(theme->colors.timelineNormalText());
  outline.style(Paint::Stroke);
  g->drawRect(rc, outline);
}
bool SoundControls::onProcessMessage(ui::Message* msg)
{
  switch (msg->type()) {
    case kMouseDownMessage: {
      if (!m_editor)
        break;

      captureMouse();
      const auto* mouseMessage = static_cast<MouseMessage*>(msg);
      m_offsetStartPosition = mouseMessage->position();
      m_offsetOverride = m_editor->audioPlayer().offset();
      return true;
    }
    case kMouseUpMessage: {
      if (hasCapture()) {
        m_editor->audioPlayer().setOffset(m_offsetOverride);
        m_offsetting = false;
        m_offsetOverride = 0;
        updateTooltip();

        releaseMouse();
        return true;
      }
    } break;
    case kMouseMoveMessage: {
      if (hasCapture() && m_editor) {
        const auto* mouseMessage = static_cast<MouseMessage*>(msg);
        m_offsetting = true;
        const auto newOffset = m_editor->audioPlayer().offset() +
                               (m_offsetStartPosition.x - mouseMessage->position().x);
        if (m_offsetOverride != newOffset) {
          m_offsetOverride = newOffset;
          invalidate();
        }
        return true;
      }
    } break;
    case kSetCursorMessage: {
      set_mouse_cursor(kScrollCursor);
      return true;
    }
  }

  return Widget::onProcessMessage(msg);
}

void SoundControls::generateWaveform()
{
  m_waveform.reset();

  const gfx::Rect rc = clientBounds();
  if (rc.h == 0 || rc.w == 0)
    return;

  if (!m_editor)
    return;

  const auto* theme = skin::SkinTheme::get(this);
  m_waveform = os::System::instance()->makeRgbaSurface(rc.w - (guiscale() * 2),
                                                       rc.h - (guiscale() * 2));

  Paint bg;
  bg.color(theme->colors.timelineBandBg());
  bg.style(Paint::Fill);
  m_waveform->drawRect(gfx::Rect{ 0, 0, rc.w, rc.h }, bg);

  Paint p;
  p.color(theme->colors.timelineNormalText());
  p.style(Paint::Fill);

  TRACE("Generating waveform with size: (%d, %d)\n", rc.w, rc.h);
  for (int x = guiscale(); x < rc.w - guiscale(); x = x + guiscale()) {
    const int h = std::rand() % (guiscale() - (rc.h - guiscale() * 2) + guiscale());
    m_waveform->drawRect(gfx::Rect{ x, (rc.h / 2) - (h / 2) - guiscale(), guiscale(), h }, p);
    TRACE("DrawRect! %d, %d, %d, %d\n", x, (rc.h / 2) - (h / 2), guiscale(), h);
  }

  TRACE("Generated a new waveform surface.\n");
}

void SoundControls::updateTooltip()
{
  if (!m_editor) {
    m_tooltipManager->removeTooltipFor(this);
    return;
  }
  Audio& audio = m_editor->audioPlayer();
  m_tooltipManager->addTooltipFor(this,
                                  fmt::format("Audio filename: {}\nDuration: {}\nOffset: {}",
                                              audio.filename(),
                                              audio.length(),
                                              audio.offset()),
                                  BOTTOM);
}
void SoundControls::updateUsingEditor(Editor* editor)
{
  m_editor = editor;
  updateTooltip();
  generateWaveform();
  invalidate();
}

void SoundControls::onResize(ui::ResizeEvent& ev)
{
  Widget::onResize(ev);

  if (m_editor)
    generateWaveform();
}

} // namespace app
