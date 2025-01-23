// Aseprite
// Copyright (c) 2022-2024  Igara Studio S.A.
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "app/ui/layouts.h"

#include "app/resource_finder.h"
#include "app/xml_document.h"
#include "app/xml_exception.h"
#include "base/fs.h"

#include <algorithm>
#include <fstream>

namespace app {

using namespace tinyxml2;

Layouts::Layouts()
{
  try {
    const std::string& fn = m_userLayoutsFilename = UserLayoutsFilename();
    if (base::is_file(fn))
      load(fn);
  }
  catch (const std::exception& ex) {
    LOG(ERROR, "LAY: Error loading user layouts: %s\n", ex.what());
  }
}

Layouts::~Layouts()
{
  saveUserLayouts();
}

LayoutPtr Layouts::getById(const std::string& id) const
{
  auto it = std::find_if(m_layouts.begin(), m_layouts.end(), [&id](const LayoutPtr& l) {
    return l->matchId(id);
  });
  return (it != m_layouts.end() ? *it : nullptr);
}

bool Layouts::addLayout(const LayoutPtr& layout)
{
  auto it = std::find_if(m_layouts.begin(), m_layouts.end(), [layout](const LayoutPtr& l) {
    return l->matchId(layout->id());
  });
  if (it != m_layouts.end()) {
    *it = layout; // Replace existent layout
    return false;
  }

  m_layouts.push_back(layout);

  if (layout->isDefault()) {
    // Don't count default layouts as "added" for the purposes of this.
    return false;
  }

  return true;
}

void Layouts::saveUserLayouts() const
{
  if (!m_userLayoutsFilename.empty())
    save(m_userLayoutsFilename);
  // else
  // LOG(kWarning, "Could not save user layouts, invalid filename.");
}

void Layouts::load(const std::string& fn)
{
  XMLDocumentRef doc = app::open_xml(fn);
  XMLHandle handle(doc.get());
  XMLElement* layoutElem =
    handle.FirstChildElement("layouts").FirstChildElement("layout").ToElement();

  while (layoutElem) {
    m_layouts.push_back(Layout::MakeFromXmlElement(layoutElem));
    layoutElem = layoutElem->NextSiblingElement();
  }
}

void Layouts::save(const std::string& fn) const
{
  TRACE("Saving layouts to %s\n", fn.c_str());

  auto doc = std::make_unique<XMLDocument>();
  XMLElement* layoutsElem = doc->NewElement("layouts");

  for (const auto& layout : m_layouts) {
    layoutsElem->InsertEndChild(layout->xmlElement()->DeepClone(doc.get()));
  }

  doc->InsertEndChild(doc->NewDeclaration("xml version=\"1.0\" encoding=\"utf-8\""));
  doc->InsertEndChild(layoutsElem);
  save_xml(doc.get(), fn);
}

// static
std::string Layouts::UserLayoutsFilename()
{
  ResourceFinder rf;
  rf.includeUserDir("user.aseprite-layouts");
  return rf.getFirstOrCreateDefault();
}

} // namespace app
