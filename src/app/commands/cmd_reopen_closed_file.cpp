// Aseprite
// Copyright (C) 2019  Igara Studio S.A.
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.
#include "app/commands/command.h"
#include "app/commands/command_factory.h"
#include "app/commands/command_ids.h"
#include "app/context.h"
#include "app/ui_context.h"

namespace app {

class ReopenClosedFileCommand : public Command {
public:
  ReopenClosedFileCommand();

protected:
  bool onEnabled(Context* context) override;
  void onExecute(Context* context) override;
};

ReopenClosedFileCommand::ReopenClosedFileCommand() : Command(CommandId::ReopenClosedFile())
{
}

bool ReopenClosedFileCommand::onEnabled(Context* ctx)
{
  if (auto uiCtx = dynamic_cast<UIContext*>(ctx)) {
    return uiCtx->hasClosedDocs();
  }
  return false;
}

void ReopenClosedFileCommand::onExecute(Context* ctx)
{
  if (auto uiCtx = dynamic_cast<UIContext*>(ctx))
    uiCtx->reopenLastClosedDoc();
}

Command* CommandFactory::createReopenClosedFileCommand()
{
  return new ReopenClosedFileCommand;
}

} // namespace app
