// Aseprite
// Copyright (C) 2001-2018  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.
#include <stddef.h>
#include <string>

#include "app/commands/cmd_set_palette.h"
#include "app/commands/command_factory.h"
#include "app/commands/command_ids.h"
#include "app/context.h"
#include "app/context_access.h"
#include "app/doc.h"
#include "app/doc_access.h"
#include "app/doc_api.h"
#include "app/modules/palettes.h"
#include "app/tx.h"
#include "base/debug.h"
#include "ui/manager.h"

namespace app {

using namespace ui;

SetPaletteCommand::SetPaletteCommand() : Command(CommandId::SetPalette()), m_palette(NULL)
{
}

void SetPaletteCommand::onExecute(Context* context)
{
  ASSERT(m_palette);
  if (!m_palette)
    return;

  ContextWriter writer(context);
  if (writer.document()) {
    Tx tx(writer, "Set Palette");
    writer.document()->getApi(tx).setPalette(writer.sprite(), writer.frame(), m_palette);
    tx.commit();
  }
  set_current_palette(m_palette, false);

  // Redraw the entire screen
  if (context->isUIAvailable())
    ui::Manager::getDefault()->invalidate();
}

Command* CommandFactory::createSetPaletteCommand()
{
  return new SetPaletteCommand;
}

} // namespace app
