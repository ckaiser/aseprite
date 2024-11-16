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
#include "ui/paint_event.h"
#include "ui/size_hint_event.h"
#include "ui/theme.h"

#include "app/ui/skin/skin_theme.h"  //?

#include "base/split_string.h"

#include "laf/text/font_mgr.h"
#include "laf/text/text_blob.h"

#include "os/system.h"

#include "ui/message.h"

namespace app {

using namespace ui;

MultilineEntry::MultilineEntry()
  : Widget(kGenericWidget)  // TODO: kEntryWidget?
  , m_hScroll(HORIZONTAL, this)
  , m_vScroll(VERTICAL, this)
  , m_caret(&m_lines)
{
  enableFlags(CTRL_RIGHT_CLICK);

  setFocusStop(true);
  initTheme();
}

void MultilineEntry::onSizeHint(SizeHintEvent& ev)
{
  ev.setSizeHint(m_textSize + border().size());  // ?
}

bool MultilineEntry::onProcessMessage(Message* msg)
{
  switch (msg->type()) {
    case kFocusEnterMessage: {
      os::System::instance()->setTranslateDeadKeys(true);
      invalidate();
    } break;
    case kFocusLeaveMessage: {
      os::System::instance()->setTranslateDeadKeys(false);
      invalidate();
    } break;
    case kKeyDownMessage: {
      if (hasFocus() && onKeyDown(static_cast<KeyMessage*>(msg))) {
        invalidate();  // TODO: Rect?
        return true;
      }
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

            // TODO: Maybe a function to just merge lines easily with just like,
            // looping through m_lines and merging and then rebuildingFromLines?

            m_selection = Selection{ m_caret.line, m_caret.line + 1, m_caret.pos, 0 };
            deleteSelection();
            return true;
          }
        }

        if (scancode == kKeyDel && m_caret.lastInLine()) {
          if (m_caret.lastLine())
            return false;  // Nothing to delete on the last line.

          // Generate a new selection to delete the newline.
          m_selection = Selection{ m_caret.line, m_caret.line + 1, m_caret.pos, 0 };
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
    m_selection.combine(prevCaret, m_caret);
  }
  else {
    m_selection.clear();
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

  gfx::Point point(border().left(), border().top());

  int caretX = 0;
  int caretY = 0;

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
            auto& bounds = run.getGlyphBounds(i);
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
    g->drawVLine(gfx::rgba(0, 255, 0) /* theme->colors.text() */,
                 caretX,
                 caretY,
                 textHeight());
}

void MultilineEntry::drawSelectionRect(Graphics* g,
                                       int i,
                                       const Line& line,
                                       const gfx::Point& offset)
{
  if (m_selection.empty())
    return;

  if (m_selection.startLine > i || m_selection.endLine < i)
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
    (m_selection.startLine == i && m_selection.endLine == i &&
     m_selection.firstPos == 0 && m_selection.lastPos == line.text.size())
    // Case 2: We start at this line and position zero, we end in a higher line.
    || (m_selection.startLine == i && m_selection.firstPos == 0 &&
        m_selection.endLine > i)
    // Case 3: We started on a previous line, and we continue on another.
    || (m_selection.startLine < i && m_selection.endLine > i)) {
    selectionRect.w = line.blob->bounds().w;
  }
  else if (m_selection.startLine < i && m_selection.endLine == i) {
    // The selection ends in this line, starts from the leftmost side TODO : RTL ?
    line.blob->visitRuns([&](text::TextBlob::RunInfo& run) {
      for (int i = 0; i < m_selection.lastPos; ++i)
        selectionRect.w += run.getGlyphBounds(i).w;
    });
  }
  else if (m_selection.startLine == i) {
    // The selection starts in this line at an offset position, and ends at the end of the run
    line.blob->visitRuns([&](text::TextBlob::RunInfo& run) {
      size_t max = run.glyphCount;

      if (m_selection.endLine == i) {
        max = m_selection.lastPos;
      }

      for (int i = 0; i < max; ++i) {
        if (i < m_selection.firstPos)
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
      gfx::seta(theme->colors.selected(),
                50),  // TODO: Put color in theme? do we even want this?
    selectionRect);
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

  if (m_selection.startLine == m_selection.endLine) {
    m_lines[m_selection.startLine].text.erase(
      m_lines[m_selection.startLine].text.begin() + m_selection.firstPos,
      m_lines[m_selection.startLine].text.begin() + m_selection.lastPos);
    rebuildTextFromLines();
  }
  else {
    int posStart = 0;
    int posEnd = 0;
    for (const auto& line : m_lines) {
      if (line.i < m_selection.startLine)
        posStart += line.text.size() +
                    1;  // Account for the unseen "\n" newline character.

      if (line.i < m_selection.endLine)
        posEnd += line.text.size() + 1;

      if (m_selection.startLine == line.i)
        posStart += m_selection.firstPos;

      if (m_selection.endLine == line.i)
        posEnd += m_selection.lastPos;
    }

    std::string newText = text();
    // TODO: Substr is faster but uglier.
    newText.erase(newText.begin() + posStart, newText.begin() + posEnd);
    setText(newText);
  }

  m_caret.line = m_selection.startLine;
  m_caret.pos = m_selection.firstPos;
  m_selection.clear();
}

void MultilineEntry::rebuildTextFromLines()
{
  // Rebuild the widget text from the lines, TODO: Hinting as to what changed in a signal
  // for onSetText.
  std::string newText;
  for (auto& line = m_lines.begin(); line != m_lines.end(); ++line) {
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

  Widget::onSetText();
}

void MultilineEntry::setViewScroll(const gfx::Point& pt)
{
  // TODO for scroll stuff -
  // updateScrollBars/setViewScroll and react to onResize and use setup_scrollbars.
  /*
  const gfx::Point oldScroll = viewScroll();
  const gfx::Point maxPos(m_textSize.w, m_textSize.h);

  gfx::Point newScroll = pt;
  newScroll.x = std::clamp(newScroll.x, 0, maxPos.x);
  newScroll.y = std::clamp(newScroll.y, 0, maxPos.y);

  if (newScroll != oldScroll) {
    TRACE("Invalidating because of setViewScroll");
    // TODO: Invalidate a rect?
    invalidate();
  }

  m_hScroll.setPos(newScroll.x);
  m_vScroll.setPos(newScroll.y);
  */
}

}  // namespace app
