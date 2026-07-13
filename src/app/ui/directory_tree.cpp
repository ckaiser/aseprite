// Aseprite
// Copyright (C) 2026-present  Igara Studio S.A.
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#include "app/ui/directory_tree.h"

#include "app/i18n/strings.h"
#include "app/ui/skin/skin_theme.h"
#include "base/fs.h"
#include "base/launcher.h"
#include "ui/menu.h"

namespace app {

using namespace ui;

PathTreeNode::PathTreeNode(const std::string& text, const std::string& path)
  : TreeNode(text,
             base::is_directory(path) ? SkinTheme::instance()->parts.iconTreeFolder() :
                                        SkinTheme::instance()->parts.iconTreeFile())
  , m_path(path)
{
}

DirectoryTreeNode::DirectoryTreeNode(const std::string& text,
                                     const std::string& path,
                                     const std::function<void(TreeNode*)>& loadCallback)
  : PathTreeNode(text, path)
  , m_loadCallback(loadCallback)
  , m_loaded(false)
{
  setCollapsed(true);
}

void DirectoryTreeNode::toggleCollapse()
{
  if (!m_loaded) {
    m_loadCallback(this);
    m_loaded = true;
  }

  setCollapsed(!isCollapsed());
}

bool DirectoryTreeNode::hasChildren() const
{
  if (m_loaded)
    return firstChild() != nullptr;

  return true;
}

DirectoryTree::DirectoryTree(const std::string& path, const std::vector<std::string>& extensions)
  : m_path(path)
  , m_extensions(extensions)
{
  ASSERT(base::is_directory(path));
  switchDirectory(path);

  m_doubleClickConn = DoubleClickItem.connect(&DirectoryTree::showItem, this);
  m_rightClickConn = RightClickItem.connect(&DirectoryTree::showPopup, this);
}

void DirectoryTree::switchDirectory(const std::string& path)
{
  delete root();
  auto* root = new PathTreeNode(base::get_file_path(path), path);
  addNodesForPath(root, path);
  setRoot(root);
  m_path = path;
}

void DirectoryTree::setExtensionFilter(const std::vector<std::string>& extensions)
{
  if (m_extensions != extensions) {
    m_extensions = extensions;
    switchDirectory(m_path);
  }
}

void DirectoryTree::toggleCollapse(TreeNode* node, bool)
{
  // Disables recursive collapsing - TODO: Implement a depth limit
  Tree::toggleCollapse(node, false);
}

void DirectoryTree::showPopup()
{
  const auto* node = dynamic_cast<PathTreeNode*>(selected());
  ASSERT(node);
  if (!node)
    return;

  Menu menu;
  MenuItem open(Strings::directory_tree_open());
  MenuItem openFolder(Strings::directory_tree_open_folder());

  open.setEnabled(!node->hasChildren());

  menu.addChild(&open);
  menu.addChild(&openFolder);

  for (auto* item : menu.children())
    item->processMnemonicFromText();

  const auto& path = node->fullPath();
  open.Click.connect([path] { base::launcher::open_file(path); });
  openFolder.Click.connect([path] { base::launcher::open_folder(path); });

  menu.showPopup(mousePosInDisplay(), display());
}

void DirectoryTree::showItem()
{
  if (const auto* item = dynamic_cast<PathTreeNode*>(selected())) {
    if (!item->hasChildren())
      base::launcher::open_file(item->fullPath());
  }
}

bool DirectoryTree::shouldInclude(const std::string& path) const
{
  const auto& ext = base::string_to_lower(base::get_file_extension(path));
  for (const auto& comp : m_extensions) {
    if (ext == comp)
      return true;
  }

  return false;
}

bool DirectoryTree::isRelevantToFilter(const std::string& path) const
{
  if (base::is_file(path) && shouldInclude(path))
    return true;

  for (const auto& fn : base::list_files(path)) {
    auto full = base::join_path(path, fn);
    if (base::is_directory(full) && isRelevantToFilter(full))
      return true;

    if (shouldInclude(full))
      return true;
  }

  return false;
}

void DirectoryTree::addNodesForPath(TreeNode* node, const std::string& path)
{
  for (const auto& fn : base::list_files(path)) {
    auto full = base::join_path(path, fn);
    TreeNode* newNode = nullptr;
    if (!m_extensions.empty() && !isRelevantToFilter(full))
      continue;

    if (base::is_file(full)) {
      newNode = new PathTreeNode(fn, full);
    }
    else if (base::is_directory(full)) {
      if (base::list_files(full).empty()) {
        newNode = new PathTreeNode(fn, full);
      }
      else {
        auto load = [this, full](TreeNode* sub) { addNodesForPath(sub, full); };
        newNode = new DirectoryTreeNode(fn, full, load);
      }
    }
    if (newNode)
      node->addChild(newNode);
  }
}

} // namespace app
