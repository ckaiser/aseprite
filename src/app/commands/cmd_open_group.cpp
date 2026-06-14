// Aseprite
// Copyright (C) 2022  Igara Studio S.A.
// Copyright (C) 2017  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.
#include "app/commands/command.h"
#include "app/commands/command_factory.h"
#include "app/commands/command_ids.h"
#include "app/context_access.h"
#include "app/doc.h"
#include "app/doc_access.h"
#include "app/modules/gui.h"
#include "doc/layer.h"
#include "ui/property.h"

namespace app {

using namespace ui;

class OpenGroupCommand : public Command {
public:
  OpenGroupCommand();

protected:
  bool onEnabled(Context* context) override;
  bool onChecked(Context* context) override;
  void onExecute(Context* context) override;

  // TODO onGetFriendlyName() needs the Context so we can return "Open
  //      Group" or "Close Group" depending on the context
  // std::string onGetFriendlyName() const override;
};

OpenGroupCommand::OpenGroupCommand() : Command(CommandId::OpenGroup())
{
}

bool OpenGroupCommand::onEnabled(Context* context)
{
  const ContextReader reader(context);
  const Layer* layer = reader.layer();
  return (layer && layer->isGroup());
}

bool OpenGroupCommand::onChecked(Context* context)
{
  const ContextReader reader(context);
  const Layer* layer = reader.layer();
  return (layer && layer->isExpanded());
}

void OpenGroupCommand::onExecute(Context* context)
{
  ContextWriter writer(context);
  Doc* doc = writer.document();
  Layer* layer = writer.layer();

  layer->setCollapsed(layer->isExpanded());
  doc->notifyLayerGroupCollapseChange(layer);

  update_screen_for_document(writer.document());
}

Command* CommandFactory::createOpenGroupCommand()
{
  return new OpenGroupCommand;
}

} // namespace app
