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

// TODO: Move to a general place
static inline bool is_word_char(int ch)
{
  return (ch && !std::isspace(ch) && !std::ispunct(ch));
}

class MultilineEntry : public Widget,
                       public ViewableWidget {
public:
  MultilineEntry();

  void cut();
  void copy();
  void paste();
  void selectAll();

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
      : m_lines(lines)
    {
    }
    explicit Caret(std::vector<Line>* lines, int line, int pos)
        : m_lines(lines)
        , line(line)
        , pos(pos)
    {
    }


    int line = 0;
    int pos = 0;

    bool left(bool byWord = false)
    {
      if (byWord)
        return leftWord();

      pos -= 1;

      if (pos < 0) {
        if (line == 0) {
          pos = 0;
          return false;
        }
        else {
          line -= 1;
          pos = text().size();
        }
      }

      return true;
    }

    bool leftWord()
    {
        for (--pos; pos >= 0; --pos) {
            if (is_word_char(text()[pos]))
                break;
        }

        for (; pos >= 0; --pos) {
            if (!is_word_char(text()[pos])) {
                ++pos;
                break;
            }
        }

        if (pos < 0)
            pos = 0;

        return true;
    }

    bool right(bool byWord = false)
    {
      if (byWord)
        return rightWord();

      pos += 1;

      if (pos > text().size()) {
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

    bool rightWord()
    {
      int len = text().size();

      for (; pos < len; ++pos) {
        if (is_word_char(text()[pos]))
          break;
      }

      for (; pos < len; ++pos) {
        if (!is_word_char(text()[pos]))
          break;
      }

      return true;
    }

    void up()
    {
      line = std::clamp(line - 1, 0, int(m_lines->size()) - 1);
      pos = std::clamp(pos, 0, int(text().size()));
    }

    void down()
    {
      line = std::clamp(line + 1, 0, int(m_lines->size()) - 1);
      pos = std::clamp(pos, 0, int(text().size()));
    }

    bool isLastInLine() { return pos == text().size(); }

    bool isLastLine() { return line == m_lines->size() - 1; }

    // Returns the absolute position of the caret, aka the position in a string
    int absolutePos() {
        int apos = 0;
        for (const auto& l : *m_lines) {
            if (l.i == line) {
                apos += pos;
                return apos;
            }
            else {
                apos += l.text.size() + 1;
            }
        }
        return apos;
    }

    // Advance the selection by the amount of characters, wrapping around new lines.
    void advanceBy(int characters) {
        ASSERT(characters > 0); // TODO: Support negative offsets if we need them

        int remaining = characters;
        for (int i = line; i < m_lines->size(); ++i) {
            // More characters to go in the current line
            int remainingInLine = text().size() - pos;
            if (remaining > remainingInLine) {
                remaining -= remainingInLine; // The amount of character we advanced in this go.
                ++line; // Advance the caret
                pos = 0;
            }
            else {
                pos += remaining;
                return;
            }
        }
    }

    bool isValid()
    {
      if (m_lines == nullptr)
        return false;
      
      if (line < 0 || line >= m_lines->size())
        return false;

      if (pos < 0 || pos > text().size())
        return false;

      return true;
    }

    void clear()
    {
      m_lines = nullptr;
      line = 0;
      pos = 0;
    }

    inline bool operator==(const Caret& other)
    {
      return line == other.line && pos == other.pos;
    }

    inline bool operator!=(const Caret& other)
    {
      return line != other.line || pos != other.pos;
    }

    inline bool operator>(const Caret& other)
    {
      return (line == other.line) ? pos > other.pos :
                                    (line + pos) > (other.line + pos);
    }

  private:
    inline std::string& text() const { return (*m_lines)[line].text; }
    std::vector<Line>* m_lines;
  };

  struct Selection {
    Caret start;
    Caret end;

    Selection() = default;
    Selection(Caret startCaret, Caret endCaret) {
      start = startCaret;
      end = endCaret; // Flip, flip, flipadelphia.
    }

    bool isEmpty() const
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

    void clear()
    {
      start = Caret(nullptr);  // std::optional?
      end = Caret(nullptr);
    }
  };

  // Draw the selection rect for the given line, if any
  void drawSelectionRect(Graphics* g,
                         int i,
                         const Line& line,
                         const gfx::PointF& offset);
  Caret caretFromPosition(const gfx::Point& position);
  void insertCharacter(base::codepoint_t character);
  void deleteSelection();
  void rebuildTextFromLines();
  void ensureCaretVisible();

  void startTimer();
  void stopTimer();

  Selection m_selection;
  Caret m_caret;
  Caret m_mouseCaretStart;

  std::vector<Line> m_lines;

  // Whether or not we're currently drawing the caret, driven by a timer.
  bool m_drawCaret = false;

  // The last position the caret was drawn, to invalidate that region when repainting.
  gfx::Rect m_caretRect;

  // The total size of the complete text, calculated as the longest single line width and the sum of the total line heights
  gfx::Size m_textSize;
};

}  // namespace app

#endif
