// Aseprite
// Copyright (C) 2001-2017  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifndef APP_UI_PALETTES_LISTBOX_H_INCLUDED
#define APP_UI_PALETTES_LISTBOX_H_INCLUDED
#pragma once

#include "app/ui/resources_listbox.h"
#include "obs/connection.h"
#include "ui/tooltips.h"

#include <unordered_set>

namespace doc {
class Palette;
}

namespace app {

class PalettesListBox final : public ResourcesListBox {
public:
  PalettesListBox();
  const doc::Palette* selectedPalette();

  void sortItems() override;

  obs::signal<void(const doc::Palette*)> PalChange;

protected:
  ResourceListItem* onCreateResourceItem(Resource* resource) override;
  void onResourceChange(Resource* resource) override;
  void onPaintResource(ui::Graphics* g, gfx::Rect& bounds, Resource* resource) override;
  void onResourceSizeHint(Resource* resource, gfx::Size& size) override;

private:
  ui::TooltipManager m_tooltips;
  obs::scoped_connection m_extPaletteChanges;
  obs::scoped_connection m_extPresetsChanges;
  std::unordered_set<std::string> m_favorites;
};

} // namespace app

#endif
