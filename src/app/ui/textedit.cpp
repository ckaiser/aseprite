// Aseprite
// Copyright (C) 2024  Igara Studio S.A.
// Copyright (C) 2001-2018  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "app/ui/skin/skin_theme.h"
#include "app/ui/textedit.h"
#include "base/replace_string.h"
#include "base/split_string.h"
#include "os/system.h"
#include "text/font_mgr.h"
#include "text/text_blob.h"
#include "ui/message.h"
#include "ui/paint_event.h"
#include "ui/resize_event.h"
#include "ui/scroll_helper.h"
#include "ui/scroll_region_event.h"
#include "ui/size_hint_event.h"
#include "ui/system.h"
#include "ui/theme.h"
#include "ui/timer.h"
#include "ui/view.h"

namespace app {

using namespace ui;

// Shared timer between all entries.
static std::unique_ptr<Timer> s_timer;

TextEdit::TextEdit()
  : Widget(kGenericWidget)
  , m_caret(&m_lines)
{
  enableFlags(CTRL_RIGHT_CLICK);
  setFocusStop(true);
  InitTheme.connect([this] {
    this->setBorder(gfx::Border(2) * guiscale());  // TODO: Move to theme
  });
  initTheme();
}

void TextEdit::cut()
{
  if (m_selection.isEmpty())
    return;

  copy();

  deleteSelection();
}

void TextEdit::copy()
{
  if (m_selection.isEmpty())
    return;

  const int startPos = m_selection.start.absolutePos();
  set_clipboard_text(
    text().substr(startPos, m_selection.end.absolutePos() - startPos));
}

void TextEdit::paste()
{
  if (!m_caret.isValid())
    return;

  deleteSelection();

  std::string clipboard;
  if (!get_clipboard_text(clipboard))
    return;

#if LAF_WINDOWS
  base::replace_string(clipboard, "\r\n", "\n");
#endif

  std::string newText = text();
  newText.insert(m_caret.absolutePos(), clipboard);

  setText(newText);

  m_caret.advanceBy(clipboard.size());
}

void TextEdit::selectAll()
{
  if (m_lines.empty())
    return;

  if (m_lines.front().text.empty())
    return;

  Caret startCaret(&m_lines);
  startCaret.line = 0;
  startCaret.pos = 0;

  Caret endCaret = startCaret;
  endCaret.line = m_lines.size() - 1;
  endCaret.pos = m_lines[endCaret.line].text.size();

  m_selection = Selection(startCaret, endCaret);
}

bool TextEdit::onProcessMessage(Message* msg)
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
      m_drawCaret = true;  // Immediately draw the caret for fast UI feedback.
      startTimer();
      os::System::instance()->setTranslateDeadKeys(true);
      invalidate();
    } break;
    case kFocusLeaveMessage: {
      stopTimer();
      m_drawCaret = false;
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
    case kDoubleClickMessage: {
      if (!hasFocus())
        requestFocus();

      auto* mouseMessage = static_cast<MouseMessage*>(msg);
      Caret leftCaret = caretFromPosition(mouseMessage->position());
      if (!leftCaret.isValid())
        return false;

      Caret rightCaret = leftCaret;
      leftCaret.leftWord();  // TODO: Doesn't work when clicking on a space.
      rightCaret.rightWord();

      if (leftCaret != rightCaret) {
        m_selection = Selection(leftCaret, rightCaret);
        m_caret = rightCaret;
        invalidate();
        captureMouse();
        return true;
      }
    } break;
    case kMouseDownMessage:
      if (!hasCapture()) {
        // Only clear the selection when we don't have a capture, to avoid stepping on double click selection.
        m_selection.clear();
        captureMouse();
      }

      stopTimer();
      m_drawCaret = true;

      if (msg->shiftPressed())
        m_mouseCaretStart = m_selection.isEmpty() ? m_caret : m_selection.start;

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

        if (msg->shiftPressed()) {
          m_selection.start = m_mouseCaretStart;
          m_selection.to(m_caret);
        }
        m_mouseCaretStart.clear();
      }
    } break;
    case kMouseWheelMessage: {
      auto* mouseMsg = static_cast<MouseMessage*>(msg);
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

bool TextEdit::onKeyDown(KeyMessage* keyMessage)
{
  const KeyScancode scancode = keyMessage->scancode();
  bool byWord = keyMessage->ctrlPressed();

  const Caret prevCaret = m_caret;

  switch (scancode) {
    case kKeyLeft: {
      m_caret.left(byWord);
    } break;
    case kKeyRight: {
      m_caret.right(byWord);
    } break;
    case kKeyEnter: {
      deleteSelection();

      std::string newText = text();
      newText.insert(m_caret.absolutePos(), "\n");
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
      if (m_selection.isEmpty() || !m_selection.isValid()) {
        Caret startCaret = m_caret;
        Caret endCaret = startCaret;

        if (scancode == kKeyBackspace) {
          startCaret.left(byWord);
        }
        else {
          endCaret.right(byWord);
        }

        m_selection = Selection(startCaret, endCaret);
      }

      deleteSelection();
      return true;
    } break;
    default:
      if (keyMessage->unicodeChar() >= 32) {
        deleteSelection();

        TRACE("isDeadKey: %d - unicodeChar: %s.\n",
              keyMessage->isDeadKey() ? 1 : 0,
              base::codepoint_to_utf8(keyMessage->unicodeChar()).c_str());

        insertCharacter(keyMessage->unicodeChar());

        if (keyMessage->isDeadKey()) {
          m_selection = Selection(prevCaret, m_caret);
        }
        return true;
      }
      else if (scancode >= kKeyFirstModifierScancode) {
        return true;
      }
      // TODO: handleShortcuts(scancode)? - Map common shortcuts into an app-wide preference?
#if defined __APPLE__
      else if (keyMessage->onlyCmdPressed())
#else
      else if (keyMessage->onlyCtrlPressed())
#endif
      {
        switch (scancode) {
          case kKeyX: {
            cut();
            return true;
          } break;
          case kKeyC: {
            copy();
            return true;
          } break;
          case kKeyV: {
            paste();
            return true;
          } break;
          case kKeyA: {
            selectAll();
            return true;
          } break;
        }
      }
      return false;
  }

  // Selection addition/removal
  if (keyMessage->shiftPressed()) {
    if (m_selection.isEmpty()) {
      m_selection.start = prevCaret;
    }

    m_selection.to(m_caret);
  }
  else
    m_selection.clear();

  return true;
}

bool TextEdit::onMouseMove(MouseMessage* mouseMessage)
{
  const Caret mouseCaret = caretFromPosition(mouseMessage->position());
  if (!mouseCaret.isValid())
    return false;

  m_caret = mouseCaret;

  if (!m_mouseCaretStart.isValid()) {
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

void TextEdit::onPaint(PaintEvent& ev)
{
  // TODO: Move to theme?
  Graphics* g = ev.graphics();
  auto* theme = skin::SkinTheme::get(this);
  auto* view = View::getView(this);

  const gfx::Rect rect = view->viewportBounds().offset(-bounds().origin());
  g->fillRect(theme->colors.textboxFace(), rect);

  const auto& scroll = view->viewScroll();
  gfx::PointF point(border().left(), border().top());
  point -= scroll;

  gfx::Rect caretRect(border().left() - scroll.x, border().top() - scroll.y, 2, textHeight());

  os::Paint textPaint;
  textPaint.color(theme->colors.text());
  textPaint.style(os::Paint::Fill);

  for (const auto& line : m_lines) {
    // Drawing the selection rect (if any)
    drawSelectionRect(g, line.i, line, point);

    // TODO: Text line drawing code should split things like selection rect drawing and draw with the inverted/selected color.
    if (line.blob)
      g->drawTextBlob(line.blob, point, textPaint);

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
    g->drawRect(theme->colors.text(), caretRect);
    m_caretRect = caretRect.offset(
      gfx::Point(g->getInternalDeltaX(), g->getInternalDeltaY()));
  }
}

void TextEdit::onSizeHint(SizeHintEvent& ev)
{
  ev.setSizeHint(m_textSize);

  auto* view = View::getView(this);
  if (!view)
    return;

  auto* theme = skin::SkinTheme::get(this);
  const int scrollBarWidth = theme->dimensions.miniScrollbarSize();

  if (view->horizontalBar())
    view->horizontalBar()->setBarWidth(scrollBarWidth);
  if (view->verticalBar())
    view->verticalBar()->setBarWidth(scrollBarWidth);
}

void TextEdit::onScrollRegion(ScrollRegionEvent& ev)
{
  invalidateRegion(ev.region());
}

void TextEdit::drawSelectionRect(Graphics* g,
                                 int i,
                                 const Line& line,
                                 const gfx::PointF& offset)
{
  if (m_selection.isEmpty())
    return;

  if (m_selection.start.line > i || m_selection.end.line < i)
    return;

  gfx::RectF selectionRect(offset, gfx::SizeF(0, line.height));

  if (!line.blob) {
    // No blob so this must be an empty line in the middle of a selection, just give it a marginal width
    // so it's noticeable.
    selectionRect.w = line.height / 2;
  }
  else if (
    // Detect when this entire line is selected, to avoid doing any runs and just painting it all
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

  auto* theme = skin::SkinTheme::get(this);
  g->fillRect(
    // TODO: Avoiding harsh contrast, should still invert text color?
    // TODO: Put color in theme? do we even want the selection to remain visible when not in focus?
    hasFocus() ? gfx::seta(theme->colors.selected(), 200) :  gfx::seta(theme->colors.selected(), 40),  
    selectionRect);
}

TextEdit::Caret TextEdit::caretFromPosition(const gfx::Point& position)
{
  auto* view = View::getView(this);
  if (!view)
    return Caret();

  if (!view->viewportBounds().contains(position)) {
    if (position.y < view->viewportBounds().y) {
      return Caret(&m_lines, 0, 0);
    }

    if (position.y > (view->viewportBounds().y + view->viewportBounds().h)) {
      return Caret(&m_lines, m_lines.size() - 1, m_lines.back().text.size());
    }

    return Caret();
  }

  // Normalize the mouse position to the internal coordinates of the widget
  gfx::Point offsetPosition(position.x - (bounds().x + border().left()),
                            position.y - (bounds().y + border().top()));

  offsetPosition += View::getView(this)->viewScroll();

  Caret caret(&m_lines);
  const int lineHeight = textHeight();

  // First check if the offset position is blank (below all the lines)
  if (offsetPosition.y > m_lines.size() * lineHeight) {
    // Get the last character in the last line.
    caret.line = m_lines.size() - 1;
    // Check the line width and if we're more than halfway past the line, we can set the caret to the full line.
    // TODO: Ideally we'd calculate the equivalent position in the last line with a run akin to what we're doing in the loop below.
    caret.pos = (offsetPosition.x > m_lines[caret.line].width / 2) ?
                  m_lines[caret.line].text.size() :
                  0;
    return caret;
  }

  for (const Line& line : m_lines) {
    const int lineStartY = line.i * lineHeight;
    const int lineEndY = (line.i + 1) * lineHeight;

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

  return caret;
}

void TextEdit::insertCharacter(base::codepoint_t character)
{
  const std::string unicodeStr = base::codepoint_to_utf8(character);

  m_lines[m_caret.line].text.insert(m_caret.pos, unicodeStr);

  std::string newText = text();
  newText.insert(m_caret.absolutePos(), unicodeStr);
  setText(newText);

  m_caret.pos++;
}

void TextEdit::deleteSelection()
{
  if (m_selection.isEmpty() || !m_selection.isValid())
    return;

  std::string newText = text();
  newText.erase(newText.begin() + m_selection.start.absolutePos(),
                newText.begin() + m_selection.end.absolutePos());
  setText(newText);

  m_caret = m_selection.start;
  m_selection.clear();
}

void TextEdit::rebuildTextFromLines()
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

void TextEdit::ensureCaretVisible()
{
  auto* view = View::getView(this);
  if (!view || !view->hasScrollBars() || !m_caret.isValid())
    return;

  const int lineHeight = textHeight();
  gfx::Point scroll = view->viewScroll();
  const gfx::Size visibleBounds = view->viewportBounds().size();

  if (view->verticalBar()->isVisible()) {
    const int heightLimit = (visibleBounds.h + scroll.y - lineHeight) / 2;
    const int currentLine = (m_caret.line * lineHeight) / 2;

    if (currentLine <= scroll.y)
      scroll.y = currentLine;
    else if (currentLine >= heightLimit) // TODO: I do not like this
      scroll.y = currentLine - ((visibleBounds.h - (lineHeight * 2)) / 2 );  
  }

  const auto& line = m_lines[m_caret.line];
  if (view->horizontalBar()->isVisible() && line.blob &&
      line.width > visibleBounds.w) {
    int caretX = 0;
    line.blob->visitRuns([&](text::TextBlob::RunInfo& run) {
      for (int i = 0; i < m_caret.pos; ++i) {
        const gfx::RectF bounds = run.getGlyphBounds(i);
        caretX += bounds.w;
      }
    });

    const int horizontalLimit =
      scroll.x + visibleBounds.w - view->horizontalBar()->getBarWidth();
    if (m_caret.pos == 0)
      scroll.x = 0;
    else if (caretX > horizontalLimit)
      scroll.x = caretX - horizontalLimit;
    else if (scroll.x > caretX / 2)
      scroll.x = caretX / 2;
  }

  view->setViewScroll(scroll);
}

void TextEdit::onSetText()
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
        theme()->fontMgr(), base::AddRef(font()), lineString);

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

void TextEdit::startTimer()
{
  if (s_timer)
    s_timer->stop();
  s_timer = std::make_unique<Timer>(500, this);
  s_timer->start();
}

void TextEdit::stopTimer()
{
  if (s_timer) {
    s_timer->stop();
    s_timer.reset();
  }
}

}  // namespace app
