// Aseprite
// Copyright (C) 2001-2017  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.
#include "app/commands/command.h"
#include "app/commands/command_factory.h"
#include "app/commands/command_ids.h"
#include "app/context.h"
#include "app/pref/option.h"
#include "app/pref/preferences.h"
#include "gfx/fwd.h"

namespace app {

using namespace gfx;

class ShowOnionSkinCommand : public Command {
public:
  ShowOnionSkinCommand() : Command(CommandId::ShowOnionSkin()) {}

protected:
  bool onChecked(Context* context) override
  {
    DocumentPreferences& docPref = Preferences::instance().document(context->activeDocument());
    return docPref.onionskin.active();
  }

  void onExecute(Context* context) override
  {
    DocumentPreferences& docPref = Preferences::instance().document(context->activeDocument());
    docPref.onionskin.active(!docPref.onionskin.active());
  }
};

Command* CommandFactory::createShowOnionSkinCommand()
{
  return new ShowOnionSkinCommand;
}

} // namespace app
