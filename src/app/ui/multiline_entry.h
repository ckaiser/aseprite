// Aseprite
// Copyright (C) 2024  Igara Studio S.A.
// Copyright (C) 2001-2018  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifndef APP_UI_MULTILINE_ENTRY_H_INCLUDED
#define APP_UI_MULTILINE_ENTRY_H_INCLUDED
#pragma once

#include "ui/box.h"
#include "ui/scroll_bar.h"

namespace app {
using namespace ui;

class MultilineEntry : public Widget,
                       public ScrollableViewDelegate {
public:
  MultilineEntry();

protected:
  void onSizeHint(SizeHintEvent& ev) override;
  bool onProcessMessage(Message* msg) override;
  void onPaint(PaintEvent& ev) override;
  void onSetText() override;

  bool onKeyDown(KeyMessage* keyMessage);

  gfx::Size visibleSize() const override { return m_textSize; };
  gfx::Point viewScroll() const override
  {
    return gfx::Point(m_hScroll.getPos(), m_vScroll.getPos());
  }
  void setViewScroll(const gfx::Point& pt) override;

private:
  struct Line {
    std::string text;
    text::TextBlobRef blob;

    int width = 0;
    int height = 0;

    // Line index for more convenient loops
    int i = 0;
  };

  struct Caret {
    explicit Caret(std::vector<Line>* lines)
      : m_lines(lines)
    {
    }

    int line = 0;
    int pos = 0;

    bool left()
    {
      pos -= 1;

      if (pos < 0) {
        if (line == 0) {
          pos = 0;
          return false;
        }
        else {
          line -= 1;
          pos = (*m_lines)[line].text.size();
        }
      }

      return true;
    }

    bool right()
    {
      pos += 1;

      if (pos > (*m_lines)[line].text.size()) {
        if (line == (*m_lines).size() - 1) {
          pos -= 1;  // Undo movement, we've reached the end of the text.
          return false;
        }
        else {
          line += 1;
          pos = 0;
        }
      }

      return true;
    }

    void up()
    {  // TODO: Clamp manually with return values?
      line = std::clamp(line - 1, 0, int((*m_lines).size()) - 1);
      pos = std::clamp(pos, 0, int((*m_lines)[line].text.size()));
    }

    void down()
    {
      line = std::clamp(line + 1, 0, int((*m_lines).size()) - 1);
      pos = std::clamp(pos, 0, int((*m_lines)[line].text.size()));
    }

    bool lastInLine() { return pos == (*m_lines)[line].text.size(); }

    bool lastLine() { return line == (*m_lines).size() - 1; }

    bool operator==(const Caret& other)
    {
      return line == other.line && pos == other.pos;
    }

  private:
    std::vector<Line>* m_lines;
  };

  struct Selection {
    int startLine = 0;
    int endLine = 0;
    int firstPos = 0;
    int lastPos = 0;

    bool empty() const
    {
      return (startLine == 0 && endLine == 0 && firstPos == 0 && lastPos == 0);
    }

    void combine(Caret c1, Caret c2)
    {
      if (empty()) {
        startLine = c1.line;
        endLine = c2.line;
        firstPos = c1.pos;
        lastPos = c2.pos;
      }
      endLine = c2.line;
      lastPos = c2.pos;  // TODO: CAN'T GO BACKWARDS YET
    }

    void clear()
    {
      startLine = 0;
      endLine = 0;
      firstPos = 0;
      lastPos = 0;
    }
  };

  // Draw the selection rect for the given line, if any
  void drawSelectionRect(Graphics* g,
                         int i,
                         const Line& line,
                         const gfx::Point& offset);
  void insertCharacter(base::codepoint_t character);
  void deleteSelection();
  void rebuildTextFromLines();

  Selection m_selection;
  Caret m_caret;

  std::vector<Line> m_lines;

  // Whether or not we're currently drawing the caret, driven by a timer.
  bool m_drawCaret = true;  // TODO: Attach to timer.

  // The total size of the complete text, calculated as the longest single line width and the sum of the total line heights
  gfx::Size m_textSize;

  ScrollBar m_hScroll;
  ScrollBar m_vScroll;
};

}  // namespace app

#endif
