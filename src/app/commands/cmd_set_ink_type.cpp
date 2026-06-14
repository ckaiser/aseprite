// Aseprite
// Copyright (C) 2020-2024  Igara Studio S.A.
// Copyright (C) 2001-2017  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.
#include <string>

#include "app/app.h"
#include "app/commands/command_factory.h"
#include "app/commands/command_ids.h"
#include "app/commands/new_params.h"
#include "app/context.h"
#include "app/i18n/strings.h"
#include "app/pref/option.h"
#include "app/pref/preferences.h"
#include "app/tools/ink_type.h"
#include "app/ui/context_bar.h"
#include "fmt/base.h"

namespace app {
class Command;
namespace tools {
class Tool;
} // namespace tools

struct SetInkTypeParams : public NewParams {
  Param<app::tools::InkType> type{ this, app::tools::InkType::DEFAULT, "type" };
};

class SetInkTypeCommand : public CommandWithNewParams<SetInkTypeParams> {
public:
  SetInkTypeCommand();

protected:
  bool onEnabled(Context* context) override;
  bool onNeedsParams() const override { return true; }
  bool onChecked(Context* context) override;
  void onExecute(Context* context) override;
  std::string onGetFriendlyName() const override;
};

SetInkTypeCommand::SetInkTypeCommand() : CommandWithNewParams(CommandId::SetInkType())
{
}

bool SetInkTypeCommand::onEnabled(Context* context)
{
  return context->isUIAvailable();
}

bool SetInkTypeCommand::onChecked(Context* context)
{
  tools::Tool* tool = App::instance()->activeTool();
  return (Preferences::instance().tool(tool).ink() == params().type());
}

void SetInkTypeCommand::onExecute(Context* context)
{
  if (App::instance()->contextBar() != nullptr)
    App::instance()->contextBar()->setInkType(params().type());
}

std::string SetInkTypeCommand::onGetFriendlyName() const
{
  std::string ink;
  switch (params().type()) {
    case tools::InkType::SIMPLE:            ink = Strings::inks_simple_ink(); break;
    case tools::InkType::ALPHA_COMPOSITING: ink = Strings::inks_alpha_compositing(); break;
    case tools::InkType::COPY_COLOR:        ink = Strings::inks_copy_color(); break;
    case tools::InkType::LOCK_ALPHA:        ink = Strings::inks_lock_alpha(); break;
    case tools::InkType::SHADING:           ink = Strings::inks_shading(); break;
  }
  return Strings::commands_SetInkType(ink);
}

Command* CommandFactory::createSetInkTypeCommand()
{
  return new SetInkTypeCommand;
}

} // namespace app
