// Aseprite
// Copyright (C) 2021-2024  Igara Studio S.A.
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "app/ui/layout_selector.h"

#include "app/app.h"
#include "app/i18n/strings.h"
#include "app/match_words.h"
#include "app/pref/preferences.h"
#include "app/ui/main_window.h"
#include "app/ui/separator_in_view.h"
#include "app/ui/skin/skin_theme.h"
#include "fmt/printf.h"
#include "ui/alert.h"
#include "ui/entry.h"
#include "ui/label.h"
#include "ui/listitem.h"
#include "ui/tooltips.h"
#include "ui/window.h"

#include "new_layout.xml.h"

#define ANI_TICKS 2

namespace app {

using namespace app::skin;
using namespace ui;

namespace {

// TODO this combobox is similar to FileSelector::CustomFileNameEntry
//      and GotoFrameCommand::TagsEntry
class LayoutsEntry final : public ComboBox {
public:
  explicit LayoutsEntry(Layouts& layouts) : m_layouts(layouts)
  {
    setEditable(true);
    getEntryWidget()->Change.connect(&LayoutsEntry::onEntryChange, this);
    fill(true);
  }

private:
  void fill(bool all)
  {
    deleteAllItems();

    const MatchWords match(getEntryWidget()->text());

    bool matchAny = false;
    for (const auto& layout : m_layouts) {
      if (layout->isDefault())
        continue; // Ignore custom defaults.

      if (match(layout->name())) {
        matchAny = true;
        break;
      }
    }
    for (const auto& layout : m_layouts) {
      if (layout->isDefault())
        continue;

      if (all || !matchAny || match(layout->name()))
        addItem(layout->name());
    }
  }

  void onEntryChange() override
  {
    closeListBox();
    fill(false);
    if (getItemCount() > 0 && !empty())
      openListBox();
  }

  Layouts& m_layouts;
};

}; // namespace

class LayoutSelector::LayoutItem final : public ListItem {
public:
  enum LayoutOption {
    DEFAULT,
    MIRRORED_DEFAULT,
    USER_DEFINED,
    NEW_LAYOUT,
  };

  LayoutItem(LayoutSelector* selector,
             const LayoutOption option,
             const std::string& text,
             const LayoutPtr& layout)
    : ListItem(text)
    , m_option(option)
    , m_selector(selector)
    , m_layout(layout)
    , m_actionButton(nullptr)
  {
    auto* hbox = new HBox;
    hbox->setTransparent(true);
    addChild(hbox);

    auto* filler = new BoxFiller();
    filler->setTransparent(true);
    hbox->addChild(filler);

    if (option == USER_DEFINED || ((option == DEFAULT || option == MIRRORED_DEFAULT) && layout)) {
      // TODO: Icons for reset & delete? trash can? or just this X
      m_actionButton = new IconButton(SkinTheme::instance()->parts.iconClose());
      m_actionButton->setSizeHint(
        gfx::Size(m_actionButton->textHeight(), m_actionButton->textHeight()));

      m_actionButton->setTransparent(true);

      m_actionButton->InitTheme.connect(
        [this] { m_actionButton->setBgColor(gfx::rgba(0, 0, 0, 0)); });

      if (option == USER_DEFINED)
        m_actionButton->Click.connect([&text] {
          const auto alert = Alert::create("Deleting Layout");
          alert->addLabel(fmt::sprintf("Are you sure you want to delete the layout '%s'?", text),
                          LEFT);
          alert->addButton(Strings::general_ok());
          alert->addButton(Strings::general_cancel());
          if (alert->show() == 1) {
            TRACE("TODO: Delete this layout!");
          }
        });
      else
        m_actionButton->Click.connect([&text] {
          const auto alert = Alert::create("Restoring Layout");
          alert->addLabel(fmt::sprintf("Are you sure you want to restore the %s layout?", text),
                          LEFT);
          alert->addButton(Strings::general_ok());
          alert->addButton(Strings::general_cancel());
          if (alert->show() == 1) {
            TRACE("TODO: Delete THIS layout, but like, the default one! With extra defaultiness!");
          }
        });

      hbox->addChild(m_actionButton);

      // TODO: Setting this here makes it just.. not visible at all? HALP?
      // m_actionButton->setVisible(false);
    }
  }

  bool onProcessMessage(Message* msg) override
  {
    switch (msg->type()) {
      case kMouseEnterMessage: {
        if (m_actionButton) {
          m_actionButton->setVisible(true);
          invalidate();
        }
      } break;
      case kMouseLeaveMessage: {
        if (m_actionButton) {
          m_actionButton->setVisible(false);
          invalidate();
        }
      } break;
      default: break;
    }

    return ListItem::onProcessMessage(msg);
  }

  std::string_view getLayoutId() const
  {
    if (m_layout)
      return m_layout->id();

    return std::string_view();
  }

  bool matchId(const std::string& id) const { return (m_layout && m_layout->matchId(id)); }

  const LayoutPtr& layout() const { return m_layout; }

  void setLayout(const LayoutPtr& layout) { m_layout = layout; }

  void selectImmediately()
  {
    MainWindow* win = App::instance()->mainWindow();

    switch (m_option) {
      case DEFAULT: {
        win->setDefaultLayout();

        if (const auto& defaultLayout = win->layoutSelector()->m_layouts.getById(Layout::kDefault))
          m_layout = defaultLayout;

        m_selector->setActiveLayoutId(Layout::kDefault);
      } break;
      case MIRRORED_DEFAULT: {
        win->setMirroredDefaultLayout();

        if (const auto& mirroredLayout = win->layoutSelector()->m_layouts.getById(
              Layout::kMirroredDefault)) {
          m_layout = mirroredLayout;
        }

        m_selector->setActiveLayoutId(Layout::kMirroredDefault);
      } break;
    }

    if (m_layout) {
      m_selector->setActiveLayoutId(m_layout->id());
      win->loadUserLayout(m_layout.get());
    }
  }

  void selectAfterClose()
  {
    if (m_option != NEW_LAYOUT)
      return;

    MainWindow* win = App::instance()->mainWindow();
    gen::NewLayout window;

    if (m_selector->m_layouts.size() > 0)
      window.base()->addItem(new SeparatorInView());

    for (const auto& layout : m_selector->m_layouts) {
      if (layout->isDefault()) {
        auto* item = new ListItem(Strings::new_layout_modified(
          layout->id() == Layout::kDefault ? Strings::main_window_default_layout() :
                                             Strings::main_window_mirrored_default_layout()));
        item->setValue(layout->id());
        window.base()->addItem(item);

        if (m_selector->m_activeLayoutId == layout->id())
          window.base()->setSelectedItemIndex(window.base()->getItemCount() - 1);
      }
    }

    // TODO: Kinda silly but if we created a new custom layout before modifying a default, they'll
    // show up afterwards -- FIX/SORT/WHATEVER
    for (const auto& layout : m_selector->m_layouts) {
      if (layout->isDefault())
        continue;
      auto* item = new ListItem(layout->name());
      item->setValue(layout->id());
      window.base()->addItem(item);

      // TODO: Duplicated, awful, terrible, take a lap.
      if (m_selector->m_activeLayoutId == layout->id())
        window.base()->setSelectedItemIndex(window.base()->getItemCount() - 1);
    }

    window.name()->Change.connect(
      [&] { window.ok()->setEnabled(Layout::isValidName(window.name()->text())); });

    window.openWindowInForeground();
    if (window.closer() == window.ok()) {
      if (window.base()->getValue() == "_default_original_")
        win->setDefaultLayout();
      else if (window.base()->getValue() == "_mirrored_default_original_") // TODO: No hardcoding
        win->setMirroredDefaultLayout();
      else {
        m_layout = m_selector->m_layouts.getById(window.base()->getValue());
        ASSERT(m_layout);
        selectImmediately();
      }

      const auto layout =
        Layout::MakeFromDock(window.name()->text(), window.name()->text(), win->customizableDock());
      m_selector->addLayout(layout);
      m_selector->m_layouts.saveUserLayouts();
    }
  }

private:
  LayoutOption m_option;
  LayoutSelector* m_selector;
  LayoutPtr m_layout;
  Button* m_actionButton;
};

void LayoutSelector::LayoutComboBox::onChange()
{
  ComboBox::onChange();
  if (auto* item = dynamic_cast<LayoutItem*>(getSelectedItem())) {
    item->selectImmediately();
    m_selected = item;
  }
}

void LayoutSelector::LayoutComboBox::onCloseListBox()
{
  ComboBox::onCloseListBox();
  if (m_selected) {
    m_selected->selectAfterClose();
    m_selected = nullptr;
  }
}

LayoutSelector::LayoutSelector(TooltipManager* tooltipManager)
  : m_button(SkinTheme::instance()->parts.iconUserData())
{
  setActiveLayoutId(Preferences::instance().general.workspaceLayout());

  m_button.Click.connect([this]() { switchSelector(); });

  m_comboBox.setVisible(false);

  addChild(&m_comboBox);
  addChild(&m_button);

  setupTooltips(tooltipManager);

  InitTheme.connect([this] {
    noBorderNoChildSpacing();
    m_comboBox.noBorderNoChildSpacing();
    m_button.noBorderNoChildSpacing();
  });
  initTheme();
}

LayoutSelector::~LayoutSelector()
{
  Preferences::instance().general.workspaceLayout(m_activeLayoutId);

  stopAnimation();
}

LayoutPtr LayoutSelector::activeLayout() const
{
  return m_layouts.getById(m_activeLayoutId);
}

void LayoutSelector::addLayout(const LayoutPtr& layout)
{
  m_layouts.addLayout(layout);
  populateComboBox();
}

void LayoutSelector::updateActiveLayout(const LayoutPtr& newLayout)
{
  m_layouts.addLayout(newLayout);
  setActiveLayoutId(newLayout->id());
  m_layouts.saveUserLayouts();
}

void LayoutSelector::onAnimationFrame()
{
  switch (animation()) {
    case ANI_NONE:       break;
    case ANI_EXPANDING:
    case ANI_COLLAPSING: {
      const double t = animationTime();
      m_comboBox.setSizeHint(gfx::Size(int(inbetween(m_startSize.w, m_endSize.w, t)),
                                       int(inbetween(m_startSize.h, m_endSize.h, t))));
      break;
    }
  }

  if (auto* win = window())
    win->layout();
}

void LayoutSelector::onAnimationStop(int animation)
{
  switch (animation) {
    case ANI_EXPANDING:
      m_comboBox.setSizeHint(m_endSize);
      if (m_switchComboBoxAfterAni) {
        m_switchComboBoxAfterAni = false;
        m_comboBox.openListBox();
      }
      break;
    case ANI_COLLAPSING:
      m_comboBox.setVisible(false);
      m_comboBox.setSizeHint(m_endSize);
      if (m_switchComboBoxAfterAni) {
        m_switchComboBoxAfterAni = false;
        m_comboBox.closeListBox();
      }
      break;
  }

  if (auto* win = window())
    win->layout();
}

void LayoutSelector::switchSelector()
{
  bool expand;
  if (!m_comboBox.isVisible()) {
    expand = true;

    // Create the combobox for first time
    if (m_comboBox.getItemCount() == 0) {
      populateComboBox();
    }

    m_comboBox.setVisible(true);
    m_comboBox.resetSizeHint();
    m_startSize = gfx::Size(0, 0);
    m_endSize = m_comboBox.sizeHint();
  }
  else {
    expand = false;
    m_startSize = m_comboBox.bounds().size();
    m_endSize = gfx::Size(0, 0);
  }

  if (auto* item = getItemByLayoutId(m_activeLayoutId))
    m_comboBox.setSelectedItem(item);

  m_comboBox.setSizeHint(m_startSize);
  startAnimation((expand ? ANI_EXPANDING : ANI_COLLAPSING), ANI_TICKS);

  MainWindow* win = App::instance()->mainWindow();
  win->setCustomizeDock(expand);
}

void LayoutSelector::switchSelectorFromCommand()
{
  m_switchComboBoxAfterAni = true;
  switchSelector();
}

bool LayoutSelector::isSelectorVisible() const
{
  return (m_comboBox.isVisible());
}

void LayoutSelector::setupTooltips(TooltipManager* tooltipManager)
{
  tooltipManager->addTooltipFor(&m_button, Strings::main_window_layout(), TOP);
}

void LayoutSelector::populateComboBox()
{
  m_comboBox.deleteAllItems();

  m_comboBox.addItem(new SeparatorInView(Strings::main_window_layout(), HORIZONTAL));
  m_comboBox.addItem(new LayoutItem(this,
                                    LayoutItem::DEFAULT,
                                    Strings::main_window_default_layout(),
                                    m_layouts.getById(Layout::kDefault)));
  m_comboBox.addItem(new LayoutItem(this,
                                    LayoutItem::MIRRORED_DEFAULT,
                                    Strings::main_window_mirrored_default_layout(),
                                    m_layouts.getById(Layout::kMirroredDefault)));
  m_comboBox.addItem(new SeparatorInView(Strings::main_window_user_layouts(), HORIZONTAL));
  for (const auto& layout : m_layouts) {
    LayoutItem* item;
    if (layout->isDefault()) {
      item = dynamic_cast<LayoutItem*>(
        m_comboBox.getItem(layout->id() == Layout::kDefault ? 1 : 2));
      // Indicate we've modified this with an asterisk.
      item->setText(item->text() + "*");
    }
    else {
      item = new LayoutItem(this, LayoutItem::USER_DEFINED, layout->name(), layout);
      m_comboBox.addItem(item);
    }

    if (layout->id() == m_activeLayoutId)
      m_comboBox.setSelectedItem(item);
  }
  m_comboBox.addItem(
    new LayoutItem(this, LayoutItem::NEW_LAYOUT, Strings::main_window_new_layout(), nullptr));
}
LayoutSelector::LayoutItem* LayoutSelector::getItemByLayoutId(const std::string& id)
{
  for (auto* child : m_comboBox) {
    if (auto* item = dynamic_cast<LayoutItem*>(child)) {
      if (item->matchId(id))
        return item;
    }
  }

  return nullptr;
}

} // namespace app
