// Aseprite
// Copyright (C) 2026-present  Igara Studio S.A.
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifndef APP_UI_TREE_DIRECTORY_H_INCLUDED
#define APP_UI_TREE_DIRECTORY_H_INCLUDED

#include "app/ui/tree.h"

namespace app {

class PathTreeNode : public TreeNode {
public:
  explicit PathTreeNode(const std::string& text, const std::string& path);
  ~PathTreeNode() override = default;

  const std::string& fullPath() const { return m_path; }

private:
  std::string m_path;
};

class DirectoryTreeNode : public PathTreeNode {
public:
  explicit DirectoryTreeNode(const std::string& text,
                             const std::string& path,
                             const std::function<void(TreeNode*)>& loadCallback);
  ~DirectoryTreeNode() override = default;

  void toggleCollapse() override;
  bool hasChildren() const override;

private:
  std::function<void(TreeNode*)> m_loadCallback;
  bool m_loaded;
};

class DirectoryTree : public Tree {
public:
  explicit DirectoryTree(const std::string& path, const std::vector<std::string>& extensions = {});

  void switchDirectory(const std::string& path);
  void setExtensionFilter(const std::vector<std::string>& extensions);

protected:
  void toggleCollapse(TreeNode* node, bool) override;
  void showPopup();
  void showItem();

private:
  bool shouldInclude(const std::string& path) const;
  bool isRelevantToFilter(const std::string& path) const;
  void addNodesForPath(TreeNode* node, const std::string& path);

  std::string m_path;
  std::vector<std::string> m_extensions;

  obs::scoped_connection m_rightClickConn;
  obs::scoped_connection m_doubleClickConn;
};

} // namespace app

#endif // APP_UI_TREE_DIRECTORY_H_INCLUDED
