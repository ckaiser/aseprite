// Aseprite
// Copyright (C) 2023  Igara Studio SA
// Copyright (C) 2001-2018  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.
#include "app/commands/command.h"
#include "app/commands/command_factory.h"
#include "app/commands/command_ids.h"
#include "app/context.h"
#include "app/context_flags.h"
#include "app/doc_range_ops.h"
#include "app/modules/gui.h"
#include "view/range.h"

namespace app {
class Doc;

class ReverseFramesCommand : public Command {
public:
  ReverseFramesCommand();

protected:
  bool onEnabled(Context* context) override;
  void onExecute(Context* context) override;
};

ReverseFramesCommand::ReverseFramesCommand() : Command(CommandId::ReverseFrames())
{
}

bool ReverseFramesCommand::onEnabled(Context* context)
{
  const view::RealRange& range = context->range();
  return context->checkFlags(ContextFlags::ActiveDocumentIsWritable) && range.enabled() &&
         range.frames() >= 2; // We need at least 2 frames to reverse
}

void ReverseFramesCommand::onExecute(Context* context)
{
  const view::RealRange& range = context->range();
  if (!range.enabled())
    return; // Nothing to do

  Doc* doc = context->activeDocument();

  reverse_frames(doc, range);

  update_screen_for_document(doc);
}

Command* CommandFactory::createReverseFramesCommand()
{
  return new ReverseFramesCommand;
}

} // namespace app
