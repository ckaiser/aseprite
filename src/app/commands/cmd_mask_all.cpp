// Aseprite
// Copyright (C) 2019  Igara Studio S.A.
// Copyright (C) 2001-2018  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.
#include <string>

#include "app/cmd/set_mask.h"
#include "app/commands/command.h"
#include "app/commands/command_factory.h"
#include "app/commands/command_ids.h"
#include "app/context.h"
#include "app/context_access.h"
#include "app/context_flags.h"
#include "app/doc.h"
#include "app/doc_access.h"
#include "app/modules/gui.h"
#include "app/pref/option.h"
#include "app/pref/preferences.h"
#include "app/transaction.h"
#include "app/tx.h"
#include "doc/mask.h"
#include "doc/sprite.h"

namespace app {

class MaskAllCommand : public Command {
public:
  MaskAllCommand();

protected:
  bool onEnabled(Context* context) override;
  void onExecute(Context* context) override;
};

MaskAllCommand::MaskAllCommand() : Command(CommandId::MaskAll())
{
}

bool MaskAllCommand::onEnabled(Context* context)
{
  return context->checkFlags(ContextFlags::ActiveDocumentIsWritable |
                             ContextFlags::HasActiveSprite);
}

void MaskAllCommand::onExecute(Context* context)
{
  ContextWriter writer(context);
  Doc* document(writer.document());
  Sprite* sprite(writer.sprite());

  Mask newMask;
  newMask.replace(sprite->bounds());

  Tx tx(writer, "Select All", DoesntModifyDocument);
  tx(new cmd::SetMask(document, &newMask));
  document->resetTransformation();
  tx.commit();

  if (Preferences::instance().selection.autoShowSelectionEdges()) {
    DocumentPreferences& docPref = Preferences::instance().document(document);
    docPref.show.selectionEdges(true);
  }

  update_screen_for_document(document);
}

Command* CommandFactory::createMaskAllCommand()
{
  return new MaskAllCommand;
}

} // namespace app
