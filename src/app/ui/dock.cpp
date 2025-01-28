// Aseprite
// Copyright (C) 2021-2024  Igara Studio S.A.
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "app/ui/dock.h"

#include "app/app.h"
#include "app/i18n/strings.h"
#include "app/ini_file.h"
#include "app/modules/gfx.h"
#include "app/pref/preferences.h"
#include "app/ui/dockable.h"
#include "app/ui/skin/skin_theme.h"
#include "layout_selector.h"
#include "main_window.h"
#include "ui/cursor_type.h"
#include "ui/label.h"
#include "ui/menu.h"
#include "ui/message.h"
#include "ui/paint_event.h"
#include "ui/resize_event.h"
#include "ui/scale.h"
#include "ui/size_hint_event.h"
#include "ui/system.h"
#include "ui/widget.h"

namespace app {

using namespace app::skin;
using namespace ui;

namespace {

enum { kTopIndex, kBottomIndex, kLeftIndex, kRightIndex, kCenterIndex };

int side_index(int side)
{
  switch (side) {
    case ui::TOP:    return kTopIndex;
    case ui::BOTTOM: return kBottomIndex;
    case ui::LEFT:   return kLeftIndex;
    case ui::RIGHT:  return kRightIndex;
  }
  return kCenterIndex; // ui::CENTER
}

int side_from_index(int index)
{
  switch (index) {
    case kTopIndex:    return ui::TOP;
    case kBottomIndex: return ui::BOTTOM;
    case kLeftIndex:   return ui::LEFT;
    case kRightIndex:  return ui::RIGHT;
  }
  return ui::CENTER; // kCenterIndex
}

} // anonymous namespace

static constexpr const char* kLegacyLayoutMainWindowSection = "layout:main_window";
static constexpr const char* kLegacyLayoutTimelineSplitter = "timeline_splitter";

Dock::Dock()
{
  for (int i = 0; i < kSides; ++i) {
    m_sides[i] = nullptr;
    m_aligns[i] = 0;
    m_sizes[i] = gfx::Size(0, 0);
  }

  InitTheme.connect([this] {
    if (auto p = parent())
      setBgColor(p->bgColor());
  });
  initTheme();
}

void Dock::setCustomizing(bool enable, bool doLayout)
{
  m_customizing = enable;

  for (int i = 0; i < kSides; ++i) {
    auto* child = m_sides[i];
    if (!child)
      continue;

    if (auto* subdock = dynamic_cast<Dock*>(child))
      subdock->setCustomizing(enable, false);
  }

  if (doLayout)
    layout();
}

void Dock::resetDocks()
{
  for (int i = 0; i < kSides; ++i) {
    auto* child = m_sides[i];
    if (!child)
      continue;

    if (auto* subdock = dynamic_cast<Dock*>(child)) {
      subdock->resetDocks();
      if (subdock->m_autoDelete)
        delete subdock;
    }

    m_sides[i] = nullptr;
  }
  removeAllChildren();
}

void Dock::dock(int side, ui::Widget* widget, const gfx::Size& prefSize)
{
  ASSERT(widget);

  const int i = side_index(side);
  if (!m_sides[i]) {
    setSide(i, widget);
    addChild(widget);

    if (prefSize != gfx::Size(0, 0))
      m_sizes[i] = prefSize;
  }
  else if (auto subdock = dynamic_cast<Dock*>(m_sides[i])) {
    subdock->dock(CENTER, widget, prefSize);
  }
  else {
    ASSERT(false); // Docking failure!
  }
}

void Dock::dockRelativeTo(ui::Widget* relative,
                          int side,
                          ui::Widget* widget,
                          const gfx::Size& prefSize)
{
  ASSERT(relative);

  Widget* parent = relative->parent();
  ASSERT(parent);

  Dock* subdock = new Dock;
  subdock->m_autoDelete = true;
  subdock->m_customizing = m_customizing;
  parent->replaceChild(relative, subdock);
  subdock->dock(CENTER, relative);
  subdock->dock(side, widget, prefSize);

  // Fix the m_sides item if the parent is a Dock
  if (auto relativeDock = dynamic_cast<Dock*>(parent)) {
    for (int i = 0; i < kSides; ++i) {
      if (relativeDock->m_sides[i] == relative) {
        relativeDock->setSide(i, subdock);
        break;
      }
    }
  }
}

void Dock::undock(Widget* widget)
{
  Widget* parent = widget->parent();
  if (!parent)
    return; // Already undocked

  if (auto* parentDock = dynamic_cast<Dock*>(parent)) {
    parentDock->removeChild(widget);

    for (int i = 0; i < kSides; ++i) {
      if (parentDock->m_sides[i] == widget) {
        parentDock->setSide(i, nullptr);
        m_sizes[i] = gfx::Size();
        break;
      }
    }

    if (parentDock != this && parentDock->children().empty()) {
      undock(parentDock);
    }
  }
  else {
    parent->removeChild(widget);
  }
}

int Dock::whichSideChildIsDocked(const ui::Widget* widget) const
{
  for (int i = 0; i < kSides; ++i)
    if (m_sides[i] == widget)
      return side_from_index(i);
  return 0;
}

const gfx::Size& Dock::getUserDefinedSizeAtSide(int side) const
{
  int i = side_index(side);
  // Only EXPANSIVE sides can be user-defined (has a splitter so the
  // user can expand or shrink it)
  if (m_aligns[i] & EXPANSIVE)
    return m_sizes[i];

  return gfx::Size();
}

Dock* Dock::subdock(int side)
{
  int i = side_index(side);
  if (auto* subdock = dynamic_cast<Dock*>(m_sides[i]))
    return subdock;

  auto* oldWidget = m_sides[i];
  auto* newSubdock = new Dock;
  newSubdock->m_autoDelete = true;
  newSubdock->m_customizing = m_customizing;
  setSide(i, newSubdock);

  if (oldWidget) {
    replaceChild(oldWidget, newSubdock);
    newSubdock->dock(CENTER, oldWidget);
  }
  else
    addChild(newSubdock);

  return newSubdock;
}

void Dock::onSizeHint(ui::SizeHintEvent& ev)
{
  gfx::Size sz = border().size();

  if (hasVisibleSide(kLeftIndex))
    sz.w += m_sides[kLeftIndex]->sizeHint().w + childSpacing();
  if (hasVisibleSide(kRightIndex))
    sz.w += m_sides[kRightIndex]->sizeHint().w + childSpacing();
  if (hasVisibleSide(kTopIndex))
    sz.h += m_sides[kTopIndex]->sizeHint().h + childSpacing();
  if (hasVisibleSide(kBottomIndex))
    sz.h += m_sides[kBottomIndex]->sizeHint().h + childSpacing();
  if (hasVisibleSide(kCenterIndex))
    sz += m_sides[kCenterIndex]->sizeHint();

  ev.setSizeHint(sz);
}

void Dock::onResize(ui::ResizeEvent& ev)
{
  gfx::Rect bounds = ev.bounds();
  setBoundsQuietly(bounds);
  bounds = childrenBounds();

  updateDockVisibility();

  forEachSide(bounds,
              [this](ui::Widget* widget,
                     const gfx::Rect& widgetBounds,
                     const gfx::Rect& separator,
                     const int index) {
                gfx::Rect rc = widgetBounds;
                auto th = textHeight();
                if (isCustomizing()) {
                  int handleSide = 0;
                  if (auto* dockable = dynamic_cast<Dockable*>(widget))
                    handleSide = dockable->dockHandleSide();
                  switch (handleSide) {
                    case ui::TOP:
                      rc.y += th;
                      rc.h -= th;
                      break;
                    case ui::LEFT:
                      rc.x += th;
                      rc.w -= th;
                      break;
                  }
                }
                widget->setBounds(rc);
              });
}

void Dock::onPaint(ui::PaintEvent& ev)
{
  Graphics* g = ev.graphics();

  const gfx::Rect& bounds = clientBounds();
  g->fillRect(bgColor(), bounds);

  if (isCustomizing()) {
    forEachSide(bounds,
                [this, g](ui::Widget* widget,
                          const gfx::Rect& widgetBounds,
                          const gfx::Rect& separator,
                          const int index) {
                  gfx::Rect rc = widgetBounds;
                  auto th = textHeight();
                  if (isCustomizing()) {
                    auto* theme = SkinTheme::get(this);
                    const auto& color = theme->colors.workspaceText();
                    int handleSide = 0;
                    if (auto* dockable = dynamic_cast<Dockable*>(widget))
                      handleSide = dockable->dockHandleSide();
                    switch (handleSide) {
                      case ui::TOP:
                        rc.h = th;
                        for (int y = rc.y; y + 1 < rc.y2(); y += 2)
                          g->drawHLine(color,
                                       rc.x + widget->border().left(),
                                       y,
                                       rc.w - widget->border().width());
                        break;
                      case ui::LEFT:
                        rc.w = th;
                        for (int x = rc.x; x + 1 < rc.x2(); x += 2)
                          g->drawVLine(color,
                                       x,
                                       rc.y + widget->border().top(),
                                       rc.h - widget->border().height());
                        break;
                    }
                  }
                });
  }
}

void Dock::onInitTheme(ui::InitThemeEvent& ev)
{
  Widget::onInitTheme(ev);
  setBorder(gfx::Border(0));
  setChildSpacing(4 * ui::guiscale());
}

class DockDropzonePlaceholder final : public Widget,
                                      public Dockable {
public:
  explicit DockDropzonePlaceholder(Widget* dragWidget) : Widget(kGenericWidget)
  {
    setId("dock_dropzone");
    setWidget(dragWidget);
    setExpansive(true);
  }

  void setWidget(Widget* dragWidget)
  {
    setSizeHint(dragWidget->sizeHint());
    setMinSize(dragWidget->size());
  }

private:
  void onPaint(PaintEvent& ev) override
  {
    auto* g = ev.graphics();

    gfx::Rect bounds = clientBounds();
    g->fillRect(bgColor(), bounds);

    bounds.shrink(2);

    constexpr gfx::Color color = gfx::rgba(89, 77, 87);
    g->drawRect(color, bounds);

    g->drawLine(color, bounds.center(), bounds.origin());
    g->drawLine(color, bounds.center(), bounds.point2());
    g->drawLine(color, bounds.center(), bounds.point2() - gfx::Point(bounds.w, 0));
    g->drawLine(color, bounds.center(), bounds.origin() + gfx::Point(bounds.w, 0));

    g->drawRect(color, gfx::Rect(bounds.center() - gfx::Point(2, 2), gfx::Size(4, 4)));
  }

  int dockHandleSide() const override { return 0; }
};

bool Dock::onProcessMessage(ui::Message* msg)
{
  switch (msg->type()) {
    case kMouseDownMessage: {
      const gfx::Point& pos = static_cast<MouseMessage*>(msg)->position();

      if (m_hit.sideIndex >= 0 || m_hit.dockable) {
        m_startPos = pos;

        if (m_hit.sideIndex >= 0)
          m_startSize = m_sizes[m_hit.sideIndex];

        captureMouse();

        if (m_hit.dockable) {
          m_dragging = true;

          auto* dragWidget = dynamic_cast<Widget*>(m_hit.dockable);
          ASSERT(dragWidget);

          if (m_dropzonePlaceholder == nullptr)
            m_dropzonePlaceholder = new DockDropzonePlaceholder(dragWidget);
          else
            m_dropzonePlaceholder->setWidget(dragWidget);

          invalidate();
        }

        return true;
      }
      break;
    }

    case kMouseMoveMessage: {
      if (hasCapture()) {
        const gfx::Point& pos = static_cast<MouseMessage*>(msg)->position();

        if (m_hit.sideIndex >= 0) {
          gfx::Size& sz = m_sizes[m_hit.sideIndex];

          switch (m_hit.sideIndex) {
            case kTopIndex:    sz.h = (m_startSize.h + pos.y - m_startPos.y); break;
            case kBottomIndex: sz.h = (m_startSize.h - pos.y + m_startPos.y); break;
            case kLeftIndex:   sz.w = (m_startSize.w + pos.x - m_startPos.x); break;
            case kRightIndex:  sz.w = (m_startSize.w - pos.x + m_startPos.x); break;
          }

          layout();
          Resize();
        }
        else if (m_hit.dockable && m_dragging) {
          invalidate();

          if (auto* dragWidget = dynamic_cast<Widget*>(m_hit.dockable)) {
            auto* parentDock = dynamic_cast<Dock*>(dragWidget->parent());

            ASSERT(parentDock);
            if (!parentDock)
              break;

            // TODO: Make when hitting?
            const auto originSide = parentDock->whichSideChildIsDocked(dragWidget);
            const auto& bounds = parentDock->bounds();

            ASSERT(originSide > 0)

            if (!bounds.contains(pos))
              break; // Do not handle anything outside the bounds of the dock.

            const int BUFFER_ZONE = 20 * guiscale(); // TODO: Move somewhere else

            int newTargetSide = -1;
            if (m_hit.dockable->dockableAt() & LEFT && !(originSide & LEFT) &&
                pos.x < bounds.x + BUFFER_ZONE) {
              newTargetSide = LEFT;
            }
            else if (m_hit.dockable->dockableAt() & RIGHT && !(originSide & RIGHT) &&
                     pos.x > (bounds.w - BUFFER_ZONE)) {
              newTargetSide = RIGHT;
            }
            else if (m_hit.dockable->dockableAt() & TOP && !(originSide & TOP) &&
                     pos.y < bounds.y + BUFFER_ZONE) {
              newTargetSide = TOP;
            }
            else if (m_hit.dockable->dockableAt() & BOTTOM && !(originSide & BOTTOM) &&
                     pos.y > (bounds.h - BUFFER_ZONE)) {
              newTargetSide = BOTTOM;
            }

            if (m_hit.targetSide == newTargetSide)
              break;

            m_hit.targetSide = newTargetSide;

            // Undock the placeholder before moving it, if it exists
            if (m_dropzonePlaceholder && m_dropzonePlaceholder->parent()) {
              auto* placeholderCurrentDock = dynamic_cast<Dock*>(m_dropzonePlaceholder->parent());
              placeholderCurrentDock->undock(m_dropzonePlaceholder);
            }

            if (m_hit.targetSide != -1 && m_dropzonePlaceholder) {
              if (auto* widgetDock = dynamic_cast<Dock*>(dragWidget->parent()))
                widgetDock->dock(m_hit.targetSide, m_dropzonePlaceholder, dragWidget->sizeHint());
            }

            App::instance()->mainWindow()->invalidate();
            layout();
          }
        }
      }
      break;
    }

    case kMouseUpMessage: {
      if (hasCapture()) {
        releaseMouse();
        const auto* mouseMessage = static_cast<MouseMessage*>(msg);

        if (m_dropzonePlaceholder && m_dropzonePlaceholder->parent()) {
          // Always undock the dropzone placeholder to avoid dangling sizes.
          auto* placeholderCurrentDock = dynamic_cast<Dock*>(m_dropzonePlaceholder->parent());
          placeholderCurrentDock->undock(m_dropzonePlaceholder);
        }

        if (m_hit.dockable) {
          auto* dockableWidget = dynamic_cast<Widget*>(m_hit.dockable);
          auto* widgetDock = dynamic_cast<Dock*>(dockableWidget->parent());

          assert(dockableWidget && widgetDock);

          auto dockNRoll = [&](const int side) {
            const gfx::Rect workspaceBounds = widgetDock->bounds();

            gfx::Size size;
            if (dockableWidget->id() == "timeline") {
              size.w = 64;
              size.h = 64;
              auto timelineSplitterPos = get_config_double(kLegacyLayoutMainWindowSection,
                                                           kLegacyLayoutTimelineSplitter,
                                                           75.0) /
                                         100.0;
              auto pos = gen::TimelinePosition::LEFT;
              size.w = (workspaceBounds.w * (1.0 - timelineSplitterPos)) / guiscale();

              if (side & RIGHT) {
                pos = gen::TimelinePosition::RIGHT;
                size.w = (workspaceBounds.w * (1.0 - timelineSplitterPos)) / guiscale();
              }
              if (side & BOTTOM) {
                pos = gen::TimelinePosition::BOTTOM;
                size.h = (workspaceBounds.h * (1.0 - timelineSplitterPos)) / guiscale();
              }
              Preferences::instance().general.timelinePosition(pos);
            }

            widgetDock->undock(dockableWidget);
            widgetDock->dock(side, dockableWidget, size);

            App::instance()->mainWindow()->invalidate();
            layout();
            onUserResizedDock();
          };

          if (mouseMessage->right()) {
            // Menu
            Menu menu;
            MenuItem left(Strings::dock_left());
            MenuItem right(Strings::dock_right());
            MenuItem top(Strings::dock_top());
            MenuItem bottom(Strings::dock_bottom());

            if (m_hit.dockable->dockableAt() & ui::LEFT) {
              menu.addChild(&left);
            }
            if (m_hit.dockable->dockableAt() & ui::RIGHT) {
              menu.addChild(&right);
            }
            if (m_hit.dockable->dockableAt() & ui::TOP) {
              menu.addChild(&top);
            }
            if (m_hit.dockable->dockableAt() & ui::BOTTOM) {
              menu.addChild(&bottom);
            }

            left.Click.connect([&dockNRoll] { dockNRoll(ui::LEFT); });
            right.Click.connect([&dockNRoll] { dockNRoll(ui::RIGHT); });
            top.Click.connect([&dockNRoll] { dockNRoll(ui::TOP); });
            bottom.Click.connect([&dockNRoll] { dockNRoll(ui::BOTTOM); });

            menu.showPopup(mouseMessage->position(), display());
            requestFocus();
          }
          else if (m_hit.targetSide > 0 && m_dragging) {
            ASSERT(m_hit.dockable->dockableAt() & m_hit.targetSide);
            dockNRoll(m_hit.targetSide);
          }
        }

        m_dragging = false;
        m_hit = Hit();
      }
      break;
    }

    case kSetCursorMessage: {
      const gfx::Point& pos = static_cast<MouseMessage*>(msg)->position();
      ui::CursorType cursor = ui::kArrowCursor;

      if (!hasCapture())
        m_hit = calcHit(pos);

      if (m_hit.sideIndex >= 0) {
        switch (m_hit.sideIndex) {
          case kTopIndex:
          case kBottomIndex: cursor = ui::kSizeNSCursor; break;
          case kLeftIndex:
          case kRightIndex:  cursor = ui::kSizeWECursor; break;
        }
      }
      else if (m_hit.dockable) {
        cursor = (m_hit.targetSide == -1) ? ui::kMoveCursor : ui::kCrosshairCursor;
      }

      ui::set_mouse_cursor(cursor);
      return true;
    }
  }
  return Widget::onProcessMessage(msg);
}

void Dock::onUserResizedDock()
{
  // Generate the UserResizedDock signal, this can be used to know
  // when the user modified the dock configuration to save the new
  // layout in a user/preference file.
  UserResizedDock();

  // Send the same notification for the parent (as probably eh
  // MainWindow is listening the signal of just the root dock).
  if (auto* parentDock = dynamic_cast<Dock*>(parent())) {
    parentDock->onUserResizedDock();
  }
}

void Dock::setSide(const int i, Widget* newWidget)
{
  m_sides[i] = newWidget;
  m_aligns[i] = calcAlign(i);

  if (newWidget) {
    m_sizes[i] = newWidget->sizeHint();
  }
}

int Dock::calcAlign(const int i)
{
  Widget* widget = m_sides[i];
  int align = 0;
  if (!widget) {
    // Do nothing
  }
  else if (auto* subdock = dynamic_cast<Dock*>(widget)) {
    align = subdock->calcAlign(i);
  }
  else if (auto* dockable2 = dynamic_cast<Dockable*>(widget)) {
    align = dockable2->dockableAt();
  }
  return align;
}

void Dock::updateDockVisibility()
{
  bool visible = false;
  setVisible(true);
  for (int i = 0; i < kSides; ++i) {
    Widget* widget = m_sides[i];
    if (!widget)
      continue;

    if (auto* subdock = dynamic_cast<Dock*>(widget)) {
      subdock->updateDockVisibility();
    }

    if (widget->isVisible()) {
      visible = true;
    }
  }

  setVisible(visible);
}

void Dock::forEachSide(gfx::Rect bounds,
                       std::function<void(ui::Widget* widget,
                                          const gfx::Rect& widgetBounds,
                                          const gfx::Rect& separator,
                                          const int index)> f)
{
  for (int i = 0; i < kSides; ++i) {
    auto* widget = m_sides[i];
    if (!widget || !widget->isVisible() || widget->isDecorative()) {
      continue;
    }

    int spacing = (m_aligns[i] & EXPANSIVE ? childSpacing() : 0);

    const gfx::Size sz = (m_aligns[i] & EXPANSIVE ? m_sizes[i] : widget->sizeHint());

    gfx::Rect rc, separator;
    switch (i) {
      case kTopIndex:
        rc = gfx::Rect(bounds.x, bounds.y, bounds.w, sz.h);
        bounds.y += rc.h;
        bounds.h -= rc.h;

        if (spacing > 0) {
          separator = gfx::Rect(bounds.x, bounds.y, bounds.w, spacing);
          bounds.y += spacing;
          bounds.h -= spacing;
        }
        break;
      case kBottomIndex:
        rc = gfx::Rect(bounds.x, bounds.y2() - sz.h, bounds.w, sz.h);
        bounds.h -= rc.h;

        if (spacing > 0) {
          separator = gfx::Rect(bounds.x, bounds.y2() - spacing, bounds.w, spacing);
          bounds.h -= spacing;
        }
        break;
      case kLeftIndex:
        rc = gfx::Rect(bounds.x, bounds.y, sz.w, bounds.h);
        bounds.x += rc.w;
        bounds.w -= rc.w;

        if (spacing > 0) {
          separator = gfx::Rect(bounds.x, bounds.y, spacing, bounds.h);
          bounds.x += spacing;
          bounds.w -= spacing;
        }
        break;
      case kRightIndex:
        rc = gfx::Rect(bounds.x2() - sz.w, bounds.y, sz.w, bounds.h);
        bounds.w -= rc.w;

        if (spacing > 0) {
          separator = gfx::Rect(bounds.x2() - spacing, bounds.y, spacing, bounds.h);
          bounds.w -= spacing;
        }
        break;
      case kCenterIndex: rc = bounds; break;
    }

    f(widget, rc, separator, i);
  }
}

Dock::Hit Dock::calcHit(const gfx::Point& pos)
{
  // TRACE("Calculating hit for pos(%d, %d)\n", pos.x, pos.y);

  Hit hit;
  forEachSide(childrenBounds(),
              [this, pos, &hit](ui::Widget* widget,
                                const gfx::Rect& widgetBounds,
                                const gfx::Rect& separator,
                                const int index) {
                if (separator.contains(pos)) {
                  hit.widget = widget;
                  hit.sideIndex = index;
                }
                else if (isCustomizing()) {
                  auto th = textHeight();
                  gfx::Rect rc = widgetBounds;
                  if (auto* dockable = dynamic_cast<Dockable*>(widget)) {
                    int handleSide = dockable->dockHandleSide();
                    switch (handleSide) {
                      case ui::TOP:
                        rc.h = th;
                        if (rc.contains(pos)) {
                          hit.widget = widget;
                          hit.dockable = dockable;
                        }
                        break;
                      case ui::LEFT:
                        rc.w = th;
                        if (rc.contains(pos)) {
                          hit.widget = widget;
                          hit.dockable = dockable;
                        }
                        break;
                    }
                  }
                }
              });
  return hit;
}

} // namespace app
