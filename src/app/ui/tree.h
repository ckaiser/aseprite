// Aseprite
// Copyright (C) 2026-present  Igara Studio S.A.
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifndef APP_UI_TREE_H_INCLUDED
#define APP_UI_TREE_H_INCLUDED

#include "app/ui/skin/skin_part.h"
#include "base/time.h"
#include "ui/theme.h"
#include "ui/widget.h"

namespace app {

using namespace ui;
using namespace skin;

class TreeNode {
public:
  explicit TreeNode(const std::string& text, const SkinPartPtr& icon = nullptr);
  virtual ~TreeNode();

  const std::string& text() const { return m_text; }
  void setText(const std::string& text);

  SkinPartPtr icon() const { return m_icon; }

  bool isCollapsed() const { return m_collapsed; }
  virtual void toggleCollapse() { m_collapsed = !m_collapsed; }
  void setCollapsed(const bool isCollapsed) { m_collapsed = isCollapsed; }
  virtual bool hasChildren() const { return m_firstChild != nullptr; }

  // Calculates how deep in its tree this node is
  int depth() const;

  void setNext(TreeNode* next) { m_next = next; }
  void setPrev(TreeNode* prev) { m_prev = prev; }
  void setParent(TreeNode* parent) { m_parent = parent; }

  // Directly attaches this node, does not touch the parent
  void attachTo(TreeNode* parent, TreeNode* prev, TreeNode* next);
  void addChild(TreeNode* child);

  TreeNode* getNextInTree() const;
  TreeNode* getPrevInTree() const;

  void setTextBlob(text::TextBlobRef blob) { m_blob = std::move(blob); }
  text::TextBlobRef textBlob() const { return m_blob; }

  TreeNode* parent() const { return m_parent; }
  TreeNode* prev() const { return m_prev; }
  TreeNode* next() const { return m_next; }
  TreeNode* firstChild() const { return m_firstChild; }
  TreeNode* lastChild() const { return m_lastChild; }

  virtual bool onKeyMessage(const KeyMessage*) { return false; };

private:
  std::string m_text;
  SkinPartPtr m_icon;
  bool m_collapsed;

  text::TextBlobRef m_blob;

  TreeNode* m_parent;
  TreeNode* m_prev;
  TreeNode* m_next;
  TreeNode* m_firstChild;
  TreeNode* m_lastChild;
};

class Tree : public Widget {
public:
  Tree();
  ~Tree() override;

  // Sets the root node and takes ownership of it and all its children
  void setRoot(TreeNode* root);
  TreeNode* root() { return m_root; }

  void setSelected(TreeNode* node, bool scrollToNode = false);
  TreeNode* selected() const { return m_selected; }

  obs::signal<void()> DoubleClickItem;
  obs::signal<void()> RightClickItem;

protected:
  bool onProcessMessage(Message* msg) override;
  void onPaint(PaintEvent& ev) override;
  void onInitTheme(InitThemeEvent& ev) override;
  void onSizeHint(SizeHintEvent& ev) override;
  bool onMouse(const MouseMessage* mouseMsg);
  virtual void toggleCollapse(TreeNode* node, bool recursive = false);

private:
  void selectLast();
  int nodeHeight(const TreeNode* node) const;

  TreeNode* m_root;
  TreeNode* m_selected;

  base::tick_t m_lastCharTick;
  std::string m_findString;

  struct {
    int rowHeight = 0;
    int itemSpacing = 0;
    int depthSpacing = 0;
    Theme::TextColors textColors;
  } m_themeCache;
};

} // namespace app

#endif // APP_UI_TREE_H_INCLUDED
