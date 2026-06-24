// Aseprite
// Copyright (c) 2019-present  Igara Studio S.A.
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifndef APP_UTIL_RESIZE_CEL_IMAGE_H_INCLUDED
#define APP_UTIL_RESIZE_CEL_IMAGE_H_INCLUDED
#pragma once

#include "doc/algorithm/resize_image.h"
#include "doc/color.h"
#include "gfx/point.h"
#include "gfx/size.h"

namespace doc {
class Cel;
class Image;
} // namespace doc

namespace app {
class Tx;

// The "image" parameter could be modified by
// ResizeImage::prepare/flattenTransparentPixels()
// functions if "resize.copySrc == false".
doc::Image* resize_image(doc::Image* image,
                         const gfx::SizeF& scale,
                         const doc::algorithm::ResizeImage& resize);

void resize_cel_image(Tx& tx,
                      doc::Cel* cel,
                      const gfx::SizeF& scale,
                      const gfx::PointF& pivot,
                      const doc::algorithm::ResizeImage& resize);

} // namespace app

#endif
