// Aseprite
// Copyright (C) 2022-2024  Igara Studio S.A.
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifndef APP_UI_LAYOUT_H_INCLUDED
#define APP_UI_LAYOUT_H_INCLUDED
#pragma once

#include <memory>
#include <string>

#include "tinyxml2.h"

namespace app {
class Dock;

class Layout;
using LayoutPtr = std::shared_ptr<Layout>;

class Layout final {
public:
  static constexpr const char* kDefault = "_default_";
  static constexpr const char* kMirroredDefault = "_mirrored_default_";

  static LayoutPtr MakeFromXmlElement(const tinyxml2::XMLElement* layoutElem);
  static LayoutPtr MakeFromDock(const std::string& id, const std::string& name, const Dock* dock);

  const std::string& id() const { return m_id; }
  const std::string& name() const { return m_name; }
  const tinyxml2::XMLElement* xmlElement() const { return m_elem; }

  bool matchId(const std::string& id) const;
  bool loadLayout(Dock* dock) const;

private:
  std::string m_id;
  std::string m_name;
  tinyxml2::XMLDocument m_dummyDoc;
  tinyxml2::XMLElement* m_elem = nullptr;
};

} // namespace app

#endif
