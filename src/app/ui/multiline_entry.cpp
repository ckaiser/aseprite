// Aseprite
// Copyright (C) 2024  Igara Studio S.A.
// Copyright (C) 2001-2018  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "app/ui/multiline_entry.h"
#include "app/ui/skin/skin_theme.h"
#include "base/split_string.h"
#include "os/system.h"
#include "text/font_mgr.h"
#include "text/text_blob.h"
#include "ui/message.h"
#include "ui/paint_event.h"
#include "ui/resize_event.h"
#include "ui/scroll_helper.h"
#include "ui/size_hint_event.h"
#include "ui/theme.h"
#include "ui/timer.h"

namespace app {

using namespace ui;

// Shared timer between all entries.
static std::unique_ptr<Timer> s_timer;

MultilineEntry::MultilineEntry()
  : Widget(kGenericWidget)  // TODO: kEntryWidget?
  , m_caret(&m_lines)
  , m_hScroll(HORIZONTAL, this)
  , m_vScroll(VERTICAL, this)
{
  enableFlags(CTRL_RIGHT_CLICK);

  auto theme = skin::SkinTheme::get(this);
  const int scrollBarWidth = theme->dimensions.miniScrollbarSize();
  m_hScroll.setBarWidth(scrollBarWidth);
  m_vScroll.setBarWidth(scrollBarWidth);

  setFocusStop(true);
  initTheme();
}

bool MultilineEntry::onProcessMessage(Message* msg)
{
  switch (msg->type()) {
    case kTimerMessage: {
      if (hasFocus() &&
          static_cast<TimerMessage*>(msg)->timer() == s_timer.get()) {
        m_drawCaret = !m_drawCaret;
        invalidate();
      }
    } break;
    case kFocusEnterMessage: {
      m_drawCaret = true; // Immediately draw the caret for fast UI feedback.
      startTimer();
      os::System::instance()->setTranslateDeadKeys(true);
      invalidate();
    } break;
    case kFocusLeaveMessage: {
      stopTimer();
      os::System::instance()->setTranslateDeadKeys(false);
      invalidate();
    } break;
    case kKeyDownMessage: {
      if (hasFocus() && onKeyDown(static_cast<KeyMessage*>(msg))) {
        invalidate();  // TODO: Rect?
        return true;
      }
    } break;
    case kMouseDownMessage:
      captureMouse();
      m_selection.clear();

      [[fallthrough]];
    case kMouseMoveMessage:
      if (hasCapture() && onMouseMove(static_cast<MouseMessage*>(msg))) {
        invalidate();
        return true;
      }
    break;
    case kMouseUpMessage: {
      if (hasCapture()) {
        releaseMouse();
        m_mouseCaretStart.clear();
      }
    } break;
    case kMouseWheelMessage: {
      auto mouseMsg = static_cast<MouseMessage*>(msg);
      gfx::Point scroll = viewScroll();

      if (mouseMsg->preciseWheel())
        scroll += mouseMsg->wheelDelta();
      else
        scroll += mouseMsg->wheelDelta() * textHeight() * 3;

      if (mouseMsg->ctrlPressed()) {
        // Sideways scrolling with CTRL.
        scroll = gfx::Point(scroll.y, scroll.x);
      }

      setViewScroll(scroll);
    } break;
  }

  return Widget::onProcessMessage(msg);
}

bool MultilineEntry::onKeyDown(KeyMessage* keyMessage)
{
  KeyScancode scancode = keyMessage->scancode();
  bool alterSelection = keyMessage->shiftPressed();

  Caret prevCaret = m_caret;

  switch (scancode) {
    case kKeyLeft: {
      m_caret.left();
    } break;
    case kKeyRight: {
      m_caret.right();
    } break;
    case kKeyEnter: {
      deleteSelection();

      // Inserting a new line in the current caret position by going through the lines and
      // adding it to the the text, then setText will rebuild the lines.
      int pos = 0;
      std::string newText = text();
      for (int i = 0; i < m_lines.size(); i++) {
        if (i == m_caret.line) {
          pos += m_caret.pos;
          newText.insert(pos, "\n");
          break;
        }
        pos += m_lines[i].text.size();
      }
      setText(newText);

      m_caret.line += 1;
      m_caret.pos = 0;
      return true;
    } break;
    case kKeyHome: {
      m_caret.pos = 0;
    } break;
    case kKeyEnd: {
      m_caret.pos = m_lines[m_caret.line].text.size();
    } break;
    case kKeyUp: {
      m_caret.up();

      if (m_caret == prevCaret)
        return false;
    } break;
    case kKeyDown: {
      m_caret.down();

      if (m_caret == prevCaret)
        return false;
    } break;
    case kKeyBackspace:
      [[fallthrough]];
    case kKeyDel: {
      if (!m_selection.empty())
        deleteSelection();
      else {
        if (scancode == kKeyBackspace) {
          if (!m_caret.left()) {
            return false;
          }

          if (m_caret.lastInLine()) {
            // If we are now the last in a line after moving left, it means we
            // moved up and need to remove a newline.

            Caret caretEnd = m_caret;
            caretEnd.right();
            m_selection = Selection{ m_caret, caretEnd };
            deleteSelection();
            return true;
          }
        }

        if (scancode == kKeyDel && m_caret.lastInLine()) {
          if (m_caret.lastLine())
            return false;  // Nothing to delete on the last line.

          // Generate a new selection to delete the newline
          Caret caretEnd = m_caret;
          caretEnd.right(); // TODO: Duplicated :(
          m_selection = Selection{ m_caret, caretEnd };
          deleteSelection();
          return true;
        }

        // Deleting a character in front of the caret, only for lines.
        m_lines[m_caret.line].text.erase(
          m_lines[m_caret.line].text.begin() + m_caret.pos,
          m_lines[m_caret.line].text.begin() + m_caret.pos + 1);

        rebuildTextFromLines();
        return true;
      }
    } break;
    default:
      if (keyMessage->unicodeChar() >= 32) {
        deleteSelection();
        insertCharacter(keyMessage->unicodeChar());
        return true;
      }
      else if (keyMessage->scancode() >= kKeyFirstModifierScancode) {
        return true;
      }
      return false;
  }

  if (alterSelection) {
    if (m_selection.empty()) {
      m_selection.start = prevCaret;
    }

    m_selection.to(m_caret);
  }
  else
    m_selection.clear();

  return true;
}

bool MultilineEntry::onMouseMove(MouseMessage* mouseMessage)
{
  Caret mouseCaret = caretFromPosition(mouseMessage->position());
  if (mouseCaret.valid()) {
    m_caret = mouseCaret;

    if (!m_mouseCaretStart.valid()) {
      m_mouseCaretStart = m_caret;
      return true;
    }

    if (m_caret > m_mouseCaretStart) {
      m_selection.start = m_mouseCaretStart;
      m_selection.end = m_caret;
    }
    else {
      m_selection.start = m_caret;
      m_selection.end = m_mouseCaretStart;
    }
  }
  else {
    TRACE("No caret.\n");
    // TODO: Go up if up, go down if down.
    return false;
  }

  return true;
}

void MultilineEntry::onPaint(PaintEvent& ev)
{
  // TODO: Move to theme?
  Graphics* g = ev.graphics();
  auto theme = skin::SkinTheme::get(this);

  gfx::Rect rect(gfx::Point(0, 0), size());
  g->fillRect(theme->colors.textboxFace(), rect);

  const auto& scroll = viewScroll();
  gfx::Point point(border().left(), border().top());
  point -= scroll;

  int caretX = -scroll.x;
  int caretY = -scroll.y;

  for (const auto& line : m_lines) {
    // Drawing the selection rect (if any)
    drawSelectionRect(g, line.i, line, point);

    if (line.blob)
      g->drawTextBlob(line.blob, theme->colors.text(), point);

    if (m_drawCaret && line.i == m_caret.line) {
      // We're in the caret's line, so we can visit this blob to grab where we should position it.
      if (m_caret.pos > 0) {
        line.blob->visitRuns([&](text::TextBlob::RunInfo& run) {
          for (int i = 0; i < m_caret.pos; ++i) {
            const gfx::RectF bounds = run.getGlyphBounds(i);
            caretX += bounds.w;
          }
        });
      }

      caretY = point.y;
    }

    point.y += line.height;
  }

  // Drawing caret:
  if (m_drawCaret)
    g->drawVLine(theme->colors.text(),
                 caretX,
                 caretY,
                 textHeight());
}

void MultilineEntry::onResize(ResizeEvent& ev)
{
  gfx::Rect rc = ev.bounds();
  setBoundsQuietly(rc);

  updateScrollBars();
}

void MultilineEntry::drawSelectionRect(Graphics* g,
                                       int i,
                                       const Line& line,
                                       const gfx::Point& offset)
{
  if (m_selection.empty())
    return;

  if (m_selection.start.line > i || m_selection.end.line < i)
    return;

  gfx::Rect selectionRect(offset, gfx::Size(0, line.height));

  if (!line.blob) {
    // No blob so this must be an empty line in the middle of a selection, just give it a marginal width
    // so it's noticeable.
    selectionRect.w = line.height / 2;
  }
  else if (
    // Detect when this entire line is selected, to avoid doing any runs and just painting it al
    // Case 1: Start and end line is this line, and the firstPos and endPos is 0 and the line's length.
    (m_selection.start.line == i && m_selection.end.line == i &&
     m_selection.start.pos == 0 && m_selection.end.pos == line.text.size())
    // Case 2: We start at this line and position zero, we end in a higher line.
    || (m_selection.start.line == i && m_selection.start.pos == 0 &&
        m_selection.end.line > i)
    // Case 3: We started on a previous line, and we continue on another.
    || (m_selection.start.line < i && m_selection.end.line > i)) {
    selectionRect.w = line.blob->bounds().w;
  }
  else if (m_selection.start.line < i && m_selection.end.line == i) {
    // The selection ends in this line, starts from the leftmost side TODO : RTL ?
    line.blob->visitRuns([&](text::TextBlob::RunInfo& run) {
      for (int i = 0; i < m_selection.end.pos; ++i)
        selectionRect.w += run.getGlyphBounds(i).w;
    });
  }
  else if (m_selection.start.line == i) {
    // The selection starts in this line at an offset position, and ends at the end of the run
    line.blob->visitRuns([&](text::TextBlob::RunInfo& run) {
      size_t max = run.glyphCount;

      if (m_selection.end.line == i) {
        max = m_selection.end.pos;
      }

      for (int i = 0; i < max; ++i) {
        if (i < m_selection.start.pos)
          selectionRect.x += run.getGlyphBounds(i).w;
        else
          selectionRect.w += run.getGlyphBounds(i).w;
      }
    });
  }
  else {
    throw new std::runtime_error("This should not be possible.");  // TODO: ???
  }

  auto theme = skin::SkinTheme::get(this);
  g->fillRect(
    hasFocus() ?
      theme->colors.selected() :
      gfx::seta(theme->colors.selected(), 50),  // TODO: Put color in theme? do we even want this?
    selectionRect);
}

MultilineEntry::Caret MultilineEntry::caretFromPosition(const gfx::Point& position)
{
  if (!bounds().contains(position))
    return Caret(nullptr);

  // Normalize the mouse position to the internal coordinates of the widget
  // TODO: Scrolling offsets.
  gfx::Point offsetPosition(position.x - (bounds().x + border().left()),
                            position.y - (bounds().y + border().top()));

  offsetPosition += viewScroll();

  Caret caret(&m_lines);
  int lineHeight = textHeight();

  for (const Line& line : m_lines) {
    int lineStartY = line.i * lineHeight;
    int lineEndY = (line.i + 1) * lineHeight;

    if (offsetPosition.y >= lineStartY && offsetPosition.y <= lineEndY) {
      int charX = 0;

      caret.line = line.i;

      if (!line.blob)
        break;

      line.blob->visitRuns([&](text::TextBlob::RunInfo& run) {
        for (int i = 0; i < run.glyphCount; ++i) {
          int charWidth = run.getGlyphBounds(i).w;

          if (offsetPosition.x >= charX &&
              offsetPosition.x <= charX + charWidth) {
            caret.pos = i;
            return;
          }
          charX += charWidth;
        }

        // Empty space:
        caret.pos = line.text.size();
      });
      break;
    }
  }

  if (!caret.valid())
    return Caret(nullptr);
  
  return caret;
}

void MultilineEntry::insertCharacter(base::codepoint_t character)
{
  const std::string unicodeStr = base::codepoint_to_utf8(character);

  m_lines[m_caret.line].text.insert(m_caret.pos, unicodeStr);
  m_caret.pos++;

  rebuildTextFromLines();
}

void MultilineEntry::deleteSelection()
{
  if (m_selection.empty())
    return;

  if (m_selection.start.line == m_selection.end.line) {
    m_lines[m_selection.start.line].text.erase(
      m_lines[m_selection.start.line].text.begin() + m_selection.start.pos,
      m_lines[m_selection.start.line].text.begin() + m_selection.end.pos);
    rebuildTextFromLines();
  }
  else {
    int posStart = 0;
    int posEnd = 0;
    for (const Line& line : m_lines) {
      if (line.i < m_selection.start.line)
        posStart += line.text.size() + 1;  // Account for the unseen "\n" newline character.

      if (line.i < m_selection.end.line)
        posEnd += line.text.size() + 1;

      if (m_selection.start.line == line.i)
        posStart += m_selection.start.pos;

      if (m_selection.end.line == line.i)
        posEnd += m_selection.end.pos;
    }

    std::string newText = text();
    // TODO: Substr is faster but uglier.
    newText.erase(newText.begin() + posStart, newText.begin() + posEnd);
    setText(newText);
  }

  m_caret.line = m_selection.start.line;
  m_caret.pos = m_selection.start.pos;
  m_selection.clear();
}

void MultilineEntry::rebuildTextFromLines()
{
  // Rebuild the widget text from the lines, TODO: Hinting as to what changed in a signal
  // for onSetText.
  std::string newText;
  for (auto line = m_lines.begin(); line != m_lines.end(); ++line) {
    newText.append((*line).text);
    if (line != m_lines.end())
      newText.append("\n");
  }

  // TODO: HINT_NO_LINE_CHANGE
  setText(newText);
}

void MultilineEntry::onSetText()
{
  // Recalculate lines based on new text

  // TODO: Have "hints" that can be used to only recalculate small parts of the text
  // depending on what has changed, like HINT_NO_LINE_CHANGE, HINT_NEW_LINE and then reset it here.

  m_lines.clear();  // TODO: Do we need to clean up textblobs? Confirm they're getting refcounted away

  std::vector<std::string> newLines;
  base::split_string(text(),
                     newLines,
                     "\n");  // TODO: Could string_view variant?

  m_lines.reserve(newLines.size());

  int longestWidth = 0;
  int totalHeight = 0;

  for (const auto& lineString : newLines) {
    Line newLine;
    newLine.text = lineString;

    if (lineString.empty()) {
      // Empty lines have no blobs attached.
      newLine.width = 0;
      newLine.height = textHeight();
    }
    else {
      newLine.blob = text::TextBlob::MakeWithShaper(
        theme()->fontMgr(),
        base::AddRef(font()),
        lineString
      );

      ASSERT(newLine.blob.get());

      newLine.width = newLine.blob->bounds().w;
      newLine.height = newLine.blob->bounds().h;
    }

    if (newLine.width > longestWidth) {
      longestWidth = newLine.width;
    }
    totalHeight += newLine.height;

    newLine.i = m_lines.size();
    m_lines.push_back(newLine);
  }

  m_textSize.w = longestWidth;
  m_textSize.h = totalHeight;

  updateScrollBars();
  Widget::onSetText();
}

void MultilineEntry::updateScrollBars()
{ 
  ui::setup_scrollbars(
    m_textSize,
    clientBounds().offset(bounds().origin()),
    *this,
    m_hScroll,
    m_vScroll
  );

  setViewScroll(viewScroll());
}

void MultilineEntry::setViewScroll(const gfx::Point& pt)
{
  // TODO for scroll stuff -
  const gfx::Point oldScroll = viewScroll();
  const gfx::Point maxPos(m_textSize.w, m_textSize.h);

  gfx::Point newScroll = pt;
  newScroll.x = std::clamp(newScroll.x, 0, maxPos.x);
  newScroll.y = std::clamp(newScroll.y, 0, maxPos.y);

  if (newScroll != oldScroll) {
    // TODO: Invalidate a rect
    invalidate();
  }

  m_hScroll.setPos(newScroll.x);
  m_vScroll.setPos(newScroll.y);
}

void MultilineEntry::startTimer()
{
  if (s_timer)
    s_timer->stop();
  s_timer = std::make_unique<Timer>(500, this);
  s_timer->start();
}

void MultilineEntry::stopTimer()
{
  if (s_timer) {
    s_timer->stop();
    s_timer.reset();
  }
}


}  // namespace app
