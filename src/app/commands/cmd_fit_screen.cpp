// Aseprite
// Copyright (C) 2022  Igara Studio S.A.
// Copyright (C) 2001-2017  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.
#include "app/commands/command.h"
#include "app/commands/command_factory.h"
#include "app/commands/command_ids.h"
#include "app/ui/editor/editor.h"

namespace app {

class FitScreenCommand : public Command {
public:
  FitScreenCommand();

protected:
  bool onEnabled(Context* context) override;
  void onExecute(Context* context) override;
};

FitScreenCommand::FitScreenCommand() : Command(CommandId::FitScreen())
{
}

bool FitScreenCommand::onEnabled(Context* context)
{
  return (Editor::activeEditor() != nullptr);
}

void FitScreenCommand::onExecute(Context* context)
{
  Editor::activeEditor()->setScrollAndZoomToFitScreen();
}

Command* CommandFactory::createFitScreenCommand()
{
  return new FitScreenCommand;
}

} // namespace app
