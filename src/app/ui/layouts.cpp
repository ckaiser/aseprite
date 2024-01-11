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

Layouts::Layouts()
{
  try {
    std::string fn = m_userLayoutsFilename = UserLayoutsFilename();
    if (base::is_file(fn))
      load(fn);
  }
  catch (const std::exception& ex) {
    LOG(ERROR, "LAY: Error loading user layouts: %s\n", ex.what());
  }
}

Layouts::~Layouts()
{
  if (!m_userLayoutsFilename.empty())
    save(m_userLayoutsFilename);
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
  else {
    m_layouts.push_back(layout);
    return true;
  }
}

void Layouts::load(const std::string& fn)
{
  XmlDocumentRef doc = app::open_xml(fn);
  TiXmlHandle handle(doc.get());
  TiXmlElement* layoutElem = handle.FirstChild("layouts").FirstChild("layout").ToElement();

  while (layoutElem) {
    m_layouts.push_back(Layout::MakeFromXmlElement(layoutElem));
    layoutElem = layoutElem->NextSiblingElement();
  }
}

void Layouts::save(const std::string& fn) const
{
  XmlDocumentRef doc(new TiXmlDocument());
  TiXmlElement layoutsElem("layouts");

  for (const auto& layout : m_layouts)
    layoutsElem.InsertEndChild(*layout->xmlElement());

  TiXmlDeclaration declaration("1.0", "utf-8", "");
  doc->InsertEndChild(declaration);
  doc->InsertEndChild(layoutsElem);
  save_xml(doc, fn);
}

// static
std::string Layouts::UserLayoutsFilename()
{
  ResourceFinder rf;
  rf.includeUserDir("user.aseprite-layouts");
  return rf.getFirstOrCreateDefault();
}

} // namespace app
