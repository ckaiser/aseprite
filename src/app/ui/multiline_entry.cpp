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
#include "ui/scroll_region_event.h"
#include "ui/theme.h"
#include "ui/timer.h"
#include "ui/view.h"

namespace app {

using namespace ui;

// Shared timer between all entries.
static std::unique_ptr<Timer> s_timer;

MultilineEntry::MultilineEntry()
  : Widget(kGenericWidget)
  , m_caret(&m_lines)
{
  enableFlags(CTRL_RIGHT_CLICK);
  setFocusStop(true);
  InitTheme.connect([this] {
    this->setBorder(gfx::Border(2) * guiscale()); // TODO: Move to theme
  });
  initTheme();
}

bool MultilineEntry::onProcessMessage(Message* msg)
{
  switch (msg->type()) {
    case kTimerMessage: {
      if (hasFocus() &&
          static_cast<TimerMessage*>(msg)->timer() == s_timer.get()) {
        m_drawCaret = !m_drawCaret;
        invalidateRect(m_caretRect);
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
        m_drawCaret = true;
        ensureCaretVisible();
        invalidate();
        return true;
      }
    } break;
    case kMouseDownMessage:
      captureMouse();
      stopTimer();
      m_drawCaret = true;
      m_selection.clear();

      [[fallthrough]];
    case kMouseMoveMessage:
      if (hasCapture() && onMouseMove(static_cast<MouseMessage*>(msg))) {
        ensureCaretVisible();
        invalidate();
        return true;
      }
    break;
    case kMouseUpMessage: {
      if (hasCapture()) {
        releaseMouse();
        startTimer();
        m_mouseCaretStart.clear();
      }
    } break;
    case kMouseWheelMessage: {
      auto mouseMsg = static_cast<MouseMessage*>(msg);
      auto* view = View::getView(this);
      gfx::Point scroll = view->viewScroll();

      if (mouseMsg->preciseWheel())
        scroll += mouseMsg->wheelDelta();
      else
        scroll += mouseMsg->wheelDelta() * textHeight();

      view->setViewScroll(scroll);
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
    } break;
    case kKeyDown: {
      m_caret.down();
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
  if (!mouseCaret.valid())
    return false;

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

  return true;
}

void MultilineEntry::onPaint(PaintEvent& ev)
{
  // TODO: Move to theme?
  Graphics* g = ev.graphics();
  auto theme = skin::SkinTheme::get(this);
  auto view = View::getView(this);

  gfx::Rect rect = view->viewportBounds().offset(-bounds().origin());
  g->fillRect(theme->colors.textboxFace(), rect);

  const auto& scroll = view->viewScroll();
  gfx::Point point(border().left(), border().top());
  point -= scroll;

  gfx::Rect caretRect(border().left() - scroll.x, border().top() - scroll.y, 1, textHeight());


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
            caretRect.x += bounds.w;
          }
        });
      }

      caretRect.y = point.y;
    }

    point.y += line.height;
  }

  // Drawing caret:
  if (m_drawCaret) {
    int height = textHeight();
    g->drawRect(theme->colors.text(), caretRect);
    m_caretRect = caretRect.offset(gfx::Point(g->getInternalDeltaX(), g->getInternalDeltaY()));
  }
}

void MultilineEntry::onSizeHint(SizeHintEvent& ev)
{
  ev.setSizeHint(m_textSize);

  auto view = View::getView(this);
  if (view) {
    auto theme = skin::SkinTheme::get(this);
    const int scrollBarWidth = theme->dimensions.miniScrollbarSize();

    if (view->horizontalBar())
      view->horizontalBar()->setBarWidth(scrollBarWidth);
    if (view->verticalBar())
      view->verticalBar()->setBarWidth(scrollBarWidth);
  }
}

void MultilineEntry::onScrollRegion(ScrollRegionEvent& ev)
{
  invalidateRegion(ev.region());
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
  if (!bounds().contains(position)) // TODO: Use view->viewportBounds()?
    return Caret(nullptr);

  // Normalize the mouse position to the internal coordinates of the widget
  gfx::Point offsetPosition(position.x - (bounds().x + border().left()),
                            position.y - (bounds().y + border().top()));

  offsetPosition += View::getView(this)->viewScroll();

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
  for (const auto& line : m_lines) {
    newText.append(line.text);
    if (&line != &m_lines.back())
      newText.append("\n");
  }

  // TODO: HINT_NO_LINE_CHANGE
  setText(newText);
}

void MultilineEntry::ensureCaretVisible()
{
  auto view = View::getView(this);
  if (!view || !view->hasScrollBars())
    return;

  int lineHeight = textHeight();
  gfx::Point scroll = view->viewScroll();
  gfx::Size visibleBounds = view->viewportBounds().size();

  if (view->verticalBar()->isVisible()) {
    int heightLimit = (visibleBounds.h + scroll.y - lineHeight) / 2;
    int currentLine = (m_caret.line * lineHeight) / 2;

    if (currentLine <= scroll.y)
      scroll.y = currentLine;
    else if (currentLine >= heightLimit)
      scroll.y = currentLine - ((visibleBounds.h - (lineHeight * 2)) / 2); // TODO: I do not like this
  }

  const auto& line = m_lines[m_caret.line];
  if (view->horizontalBar()->isVisible()
      && line.blob
      && line.width > visibleBounds.w
    ) {
    int caretX = 0;
    line.blob->visitRuns([&](text::TextBlob::RunInfo& run) {
      for (int i = 0; i < m_caret.pos; ++i) {
        const gfx::RectF bounds = run.getGlyphBounds(i);
        caretX += bounds.w;
      }
    });

    int horizontalLimit = scroll.x + visibleBounds.w - view->horizontalBar()->getBarWidth();
    if (caretX > horizontalLimit)
      scroll.x = caretX - horizontalLimit;
    //else if (scroll.x > (caretX / 2))
    // TODO

    TRACE("visibleBounds(%d, %d) - scroll(%d, %d) - caretX(%d) - horizontalLimit(%d).\n", visibleBounds.w, visibleBounds.h, scroll.x, scroll.y, caretX, horizontalLimit);
  }

  view->setViewScroll(scroll);
}

void MultilineEntry::onSetText()
{
  // Recalculate lines based on new text

  // TODO: Have "hints" that can be used to only recalculate small parts of the text
  // depending on what has changed, like HINT_NO_LINE_CHANGE, HINT_NEW_LINE and then reset it here.

  m_lines.clear();

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

  ensureCaretVisible();

  auto* view = View::getView(this);
  if (view)
    view->updateView();

  Widget::onSetText();
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
