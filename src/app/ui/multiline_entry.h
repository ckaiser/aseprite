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
#include "ui/view.h"

namespace app {
using namespace ui;

class MultilineEntry : public Widget,
                       public ViewableWidget {
public:
  MultilineEntry();

protected:
  bool onProcessMessage(Message* msg) override;
  void onPaint(PaintEvent& ev) override;
  void onSizeHint(SizeHintEvent& ev) override;
  void onScrollRegion(ScrollRegionEvent& ev) override;
  void onSetText() override;

  bool onKeyDown(KeyMessage* keyMessage);
  bool onMouseMove(MouseMessage* keyMessage);

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
    explicit Caret(std::vector<Line>* lines = nullptr)
      : m_lines(lines) {}

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
        if (line == m_lines->size() - 1) {
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
    {
      line = std::clamp(line - 1, 0, int(m_lines->size()) - 1);
      pos = std::clamp(pos, 0, int((*m_lines)[line].text.size()));
    }

    void down()
    {
      line = std::clamp(line + 1, 0, int(m_lines->size()) - 1);
      pos = std::clamp(pos, 0, int((*m_lines)[line].text.size()));
    }

    bool lastInLine() { return pos == (*m_lines)[line].text.size(); }

    bool lastLine() { return line == m_lines->size() - 1; }

    bool valid()
    {
      if (m_lines == nullptr)
        return false;

      if (line < 0 || line > m_lines->size())
        return false;

      if (pos < 0 || pos > (*m_lines)[line].text.size())
        return false;

      return true;
    }
    
    void clear()
    {
      m_lines = nullptr;
      line = 0;
      pos = 0;
    }

    bool operator==(const Caret& other)
    {
      return line == other.line && pos == other.pos;
    }

    bool operator>(const Caret& other)
    {
      return (line == other.line) ? pos > other.pos : (line + pos) > (other.line + pos);
    }

  private:
    std::vector<Line>* m_lines;
  };

  struct Selection {
    Caret start;
    Caret end;

    bool empty() const
    {
      return (start.line == end.line && start.pos == end.pos);
    }

    void to(const Caret& caret)
    {
      if (caret.line + caret.pos < start.line + start.pos)
        start = caret;
      else
        end = caret;
    }

    void clear() {
      start = Caret(nullptr); // std::optional?
      end = Caret(nullptr);
    }
  };

  // Draw the selection rect for the given line, if any
  void drawSelectionRect(Graphics* g,
                         int i,
                         const Line& line,
                         const gfx::Point& offset);
  Caret caretFromPosition(const gfx::Point& position);
  void insertCharacter(base::codepoint_t character);
  void deleteSelection();
  void rebuildTextFromLines();

  void startTimer();
  void stopTimer();

  Selection m_selection;
  Caret m_caret;
  Caret m_mouseCaretStart;

  std::vector<Line> m_lines;

  // Whether or not we're currently drawing the caret, driven by a timer.
  bool m_drawCaret = false;

  // The total size of the complete text, calculated as the longest single line width and the sum of the total line heights
  gfx::Size m_textSize;
};

}  // namespace app

#endif
