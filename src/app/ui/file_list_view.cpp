// Aseprite
// Copyright (C) 2016  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.
#include "app/ui/file_list_view.h"
#include "app/ui/file_list.h"
#include "gfx/fwd.h"
#include "gfx/rect.h"
#include "gfx/region_skia.h"
#include "ui/scroll_region_event.h"
#include "ui/widget.h"

namespace app {

void FileListView::onScrollRegion(ui::ScrollRegionEvent& ev)
{
  if (auto fileList = dynamic_cast<FileList*>(attachedWidget())) {
    gfx::Rect tbounds = fileList->mainThumbnailBounds();
    if (!tbounds.isEmpty()) {
      tbounds.enlarge(1).offset(fileList->bounds().origin());

      ev.region().createSubtraction(ev.region(), gfx::Region(tbounds));
    }
  }
}

} // namespace app
