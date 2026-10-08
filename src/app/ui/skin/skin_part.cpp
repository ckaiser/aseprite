// Aseprite
// Copyright (C) 2020  Igara Studio S.A.
// Copyright (C) 2001-2017  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "app/ui/skin/skin_part.h"

#include "os/surface.h"
#include "skin_theme.h"

namespace app { namespace skin {

SkinPart::SkinPart() = default;

SkinPart::~SkinPart()
{
  clear();
}

void SkinPart::clear()
{
  m_bitmaps.clear();
}

void SkinPart::setBitmap(std::size_t index, const os::SurfaceRef& bitmap)
{
  if (index >= m_bitmaps.size())
    m_bitmaps.resize(index + 1, nullptr);

  m_bitmaps[index] = bitmap;
}

void SkinPart::setSpriteBounds(const gfx::Rect& bounds)
{
  m_spriteBounds = bounds;
}

void SkinPart::setSlicesBounds(const gfx::Rect& bounds)
{
  m_slicesBounds = bounds;
}

gfx::Size SkinPart::size() const
{
  if (!m_bitmaps.empty())
    return gfx::Size(m_bitmaps[0]->width(), m_bitmaps[0]->height());
  else
    return gfx::Size(0, 0);
}

SkinPartPtr::SkinPartPtr(const std::string& partId) : m_partId(partId)
{
}

os::SurfaceRef SkinPartPtr::sheet() const
{
  return SkinTheme::instance()->getPartSheetById(m_partId);
}

os::SurfaceRef SkinPartPtr::unscaledSheet() const
{
  return SkinTheme::instance()->getPartUnscaledSheetById(m_partId);
}

SkinPart* SkinPartPtr::get() const
{
  return SkinTheme::instance()->getPartById(m_partId);
}

SkinPart* SkinPartPtr::operator->() const
{
  return SkinTheme::instance()->getPartById(m_partId);
}

SkinPartPtr::operator bool() const
{
  return !m_partId.empty();
}

}} // namespace app::skin
