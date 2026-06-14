// Aseprite
// Copyright (C) 2018-2019  Igara Studio S.A.
// Copyright (c) 2001-2018  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.
#include <algorithm>
#include <memory>
#include <stddef.h>
#include <utility>

#include "app/doc.h"
#include "app/docs.h"
#include "app/docs_observer.h"
#include "base/debug.h"
#include "base/fs.h"
#include "doc/image_spec.h"
#include "doc/sprite.h"

namespace app {

Docs::Docs(Context* ctx) : m_ctx(ctx)
{
  ASSERT(ctx != NULL);
}

Docs::~Docs()
{
  deleteAll();
}

Doc* Docs::add(Doc* doc)
{
  ASSERT(doc != NULL);
  ASSERT(doc->id() != doc::NullId);

  if (doc->context() != m_ctx) {
    doc->setContext(m_ctx);
    ASSERT(std::find(begin(), end(), doc) != end());
    return doc;
  }

  notify_observers(&DocsObserver::onBeforeAddDocument, doc);

  m_docs.insert(begin(), doc);

  notify_observers(&DocsObserver::onAddDocument, doc);
  return doc;
}

Doc* Docs::add(int width, int height, doc::ColorMode colorMode, int ncolors)
{
  std::unique_ptr<Doc> doc(
    new Doc(Sprite::MakeStdSprite(ImageSpec(colorMode, width, height), ncolors)));
  doc->setFilename("Sprite");
  doc->setContext(m_ctx); // Change the document context to add the doc in this collection

  return doc.release();
}

void Docs::remove(Doc* doc)
{
  iterator it = std::find(begin(), end(), doc);
  if (it == end()) // Already removed.
    return;

  notify_observers(&DocsObserver::onBeforeRemoveDocument, doc);

  m_docs.erase(it);

  notify_observers(&DocsObserver::onRemoveDocument, doc);

  doc->setContext(NULL);
}

void Docs::move(Doc* doc, int index)
{
  iterator it = std::find(begin(), end(), doc);
  ASSERT(it != end());
  if (it != end())
    m_docs.erase(it);

  m_docs.insert(begin() + index, doc);
}

Doc* Docs::getById(ObjectId id) const
{
  for (const auto& doc : *this) {
    if (doc->id() == id)
      return doc;
  }
  return NULL;
}

Doc* Docs::getByName(const std::string& name) const
{
  for (const auto& doc : *this) {
    if (doc->name() == name)
      return doc;
  }
  return NULL;
}

Doc* Docs::getByFileName(const std::string& filename) const
{
  std::string fn = base::normalize_path(filename);
  for (const auto& doc : *this) {
    if (doc->filename() == fn)
      return doc;
  }
  return NULL;
}

void Docs::deleteAll()
{
  while (!empty()) {
    ASSERT(m_ctx == back()->context());
    delete back();
  }
}

} // namespace app
