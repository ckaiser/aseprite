// Aseprite Document Library
// Copyright (c) 2018-2020 Igara Studio S.A.
// Copyright (c) 2001-2016 David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "doc/image.h"

#include "doc/algo.h"
#include "doc/brush.h"
#include "doc/image_impl.h"
#include "doc/palette.h"
#include "doc/primitives.h"
#include "doc/rgbmap.h"

namespace doc {

Image::Image(const ImageSpec& spec) : Object(ObjectType::Image), m_spec(spec)
{
}

Image::~Image()
{
}

int Image::getMemSize() const
{
  return sizeof(Image) + rowBytes() * height();
}

// static
std::unique_ptr<Image> Image::create(PixelFormat format, int width, int height, const ImageBufferPtr& buffer)
{
  return Image::create(ImageSpec((ColorMode)format, width, height, 0), buffer);
}

// static
std::unique_ptr<Image> Image::create(const ImageSpec& spec, const ImageBufferPtr& buffer)
{
  ASSERT(spec.width() >= 1 && spec.height() >= 1);
  if (spec.width() < 1 || spec.height() < 1)
    return nullptr;

  std::unique_ptr<Image> image;
  switch (spec.colorMode()) {
    case ColorMode::RGB:       image.reset(new ImageImpl<RgbTraits>(spec, buffer)); break;
    case ColorMode::GRAYSCALE: image.reset(new ImageImpl<GrayscaleTraits>(spec, buffer)); break;
    case ColorMode::INDEXED:   image.reset(new ImageImpl<IndexedTraits>(spec, buffer)); break;
    case ColorMode::BITMAP:    image.reset(new ImageImpl<BitmapTraits>(spec, buffer)); break;
    case ColorMode::TILEMAP:   image.reset(new ImageImpl<TilemapTraits>(spec, buffer)); break;
  }
  return image;
}

// static
std::unique_ptr<Image> Image::createCopy(const Image* image, const ImageBufferPtr& buffer)
{
  ASSERT(image);
  return std::unique_ptr<Image>(crop_image(image, 0, 0, image->width(), image->height(), image->maskColor(), buffer));
}

} // namespace doc
