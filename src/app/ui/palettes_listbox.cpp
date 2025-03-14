// Aseprite
// Copyright (C) 2020-2024  Igara Studio S.A.
// Copyright (C) 2001-2017  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "app/ui/palettes_listbox.h"

#include "app/app.h"
#include "app/doc.h"
#include "app/extensions.h"
#include "app/i18n/strings.h"
#include "app/modules/palettes.h"
#include "app/res/palette_resource.h"
#include "app/res/palettes_loader_delegate.h"
#include "app/ui/doc_view.h"
#include "app/ui/editor/editor.h"
#include "app/ui/icon_button.h"
#include "app/ui/skin/skin_theme.h"
#include "app/ui_context.h"
#include "base/fs.h"
#include "base/launcher.h"
#include "doc/palette.h"
#include "doc/sprite.h"
#include "os/surface.h"
#include "ui/graphics.h"
#include "ui/message.h"
#include "ui/size_hint_event.h"
#include "ui/tooltips.h"

namespace app {

using namespace ui;
using namespace app::skin;

static constexpr bool is_url_char(const int chr)
{
  return ((chr >= 'a' && chr <= 'z') || (chr >= 'A' && chr <= 'Z') || (chr >= '0' && chr <= '9') ||
          (chr == ':' || chr == '/' || chr == '@' || chr == '?' || chr == '!' || chr == '#' ||
           chr == '-' || chr == '_' || chr == '~' || chr == '.' || chr == ',' || chr == ';' ||
           chr == '*' || chr == '+' || chr == '=' || chr == '[' || chr == ']' || chr == '(' ||
           chr == ')' || chr == '$' || chr == '\''));
}

class PalettesListItem : public ResourceListItem {
  class CommentButton : public IconButton {
  public:
    explicit CommentButton(const std::string& comment)
      : IconButton(SkinTheme::instance()->parts.iconUserData())
      , m_comment(comment)
    {
      setFocusStop(false);
      setTransparent(true);
    }

  private:
    void onClick() override
    {
      IconButton::onClick();

      std::string::size_type j, i = m_comment.find("http");
      if (i != std::string::npos) {
        for (j = i + 4; j != m_comment.size() && is_url_char(m_comment[j]); ++j)
          ;
        base::launcher::open_url(m_comment.substr(i, j - i));
      }
    }

    std::string m_comment;
  };

public:
  PalettesListItem(Resource* resource, TooltipManager* tooltips) : ResourceListItem(resource)
  {
    auto* filler = new BoxFiller;
    m_hbox.setTransparent(true);
    filler->setTransparent(true);

    const bool isFavorite = false;

    // TODO: A new star icon, star with no background and star filled - make sure it aligns with the
    // userData icon.
    auto* favoriteButton = new IconButton(isFavorite ? SkinTheme::instance()->parts.iconClose() :
                                                       SkinTheme::instance()->parts.iconAdd());
    favoriteButton->setTransparent(true);
    favoriteButton->setFocusStop(true);

    // TODO: Strings to en.ini
    tooltips->addTooltipFor(favoriteButton,
                            isFavorite ? Strings::resource_listbox_remove_favorite() :
                                         Strings::resource_listbox_add_favorite(),
                            LEFT);

    m_hbox.addChild(filler);

    const std::string& comment = static_cast<PaletteResource*>(resource)->palette()->comment();
    if (!comment.empty()) {
      auto* commentButton = new CommentButton(comment);
      tooltips->addTooltipFor(commentButton, comment, LEFT);
      m_hbox.addChild(commentButton);
    }

    m_hbox.addChild(favoriteButton);
    addChild(&m_hbox);
    m_hbox.setVisible(false);
  }

  bool onProcessMessage(Message* msg) override
  {
    switch (msg->type()) {
      case kMouseLeaveMessage: {
        m_hbox.setVisible(false);
        invalidate();
        break;
      }
      case kMouseEnterMessage: {
        m_hbox.setVisible(true);
        invalidate();
        break;
      }
    }

    return ResourceListItem::onProcessMessage(msg);
  }

  HBox m_hbox;
};

PalettesListBox::PalettesListBox()
  : ResourcesListBox(new ResourcesLoader(std::make_unique<PalettesLoaderDelegate>()))
{
  addChild(&m_tooltips);

  m_favorites = { std::string("VGA 13h"), std::string("ARQ4"), std::string("CGA1") };

  m_extPaletteChanges = App::instance()->extensions().PalettesChange.connect(
    [this] { markToReload(); });
  m_extPresetsChanges = App::instance()->PalettePresetsChange.connect([this] { markToReload(); });
}

const Palette* PalettesListBox::selectedPalette()
{
  Resource* resource = selectedResource();
  if (!resource)
    return NULL;

  return static_cast<PaletteResource*>(resource)->palette();
}

ResourceListItem* PalettesListBox::onCreateResourceItem(Resource* resource)
{
  return new PalettesListItem(resource, &m_tooltips);
}

void PalettesListBox::onResourceChange(Resource* resource)
{
  const doc::Palette* palette = static_cast<PaletteResource*>(resource)->palette();
  PalChange(palette);
}

void PalettesListBox::onPaintResource(Graphics* g, gfx::Rect& bounds, Resource* resource)
{
  const auto* theme = SkinTheme::get(this);
  const Palette* palette = static_cast<PaletteResource*>(resource)->palette();
  os::Surface* tick = theme->parts.checkSelected()->bitmap(0);

  // Draw tick (to say "this palette matches the active sprite
  // palette").
  auto* view = UIContext::instance()->activeView();
  if (view && view->document()) {
    const auto* docPal = view->document()->sprite()->palette(view->editor()->frame());
    if (docPal && *docPal == *palette)
      g->drawRgbaSurface(tick, bounds.x, bounds.y + (bounds.h / 2) - (tick->height() / 2));
  }

  bounds.x += tick->width();
  bounds.w -= tick->width();

  gfx::Rect box(bounds.x, bounds.y + bounds.h - (6 * guiscale()), 4 * guiscale(), 4 * guiscale());

  for (int i = 0; i < palette->size(); ++i) {
    const doc::color_t c = palette->getEntry(i);

    g->fillRect(gfx::rgba(rgba_getr(c), rgba_getg(c), rgba_getb(c)), box);

    box.x += box.w;
  }
}

void PalettesListBox::onResourceSizeHint(Resource* resource, gfx::Size& size)
{
  size = gfx::Size(0, (2 + 16 + 2) * guiscale());
}

void PalettesListBox::sortItems()
{
  ListBox::sortItems([&](const Widget* a, const Widget* b) {
    const bool aFavorite = m_favorites.find(a->text()) != m_favorites.end();
    const bool bFavorite = m_favorites.find(b->text()) != m_favorites.end();

    if (aFavorite == bFavorite)
      return base::compare_filenames(a->text(), b->text()) < 0;

    return (aFavorite && !bFavorite);
  });
}

} // namespace app
