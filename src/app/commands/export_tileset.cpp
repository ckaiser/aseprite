// Aseprite
// Copyright (C) 2022  Igara Studio S.A.
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.
#include <string>

#include "app/commands/cmd_export_sprite_sheet.h"
#include "app/commands/command_factory.h"
#include "app/commands/command_ids.h"
#include "app/commands/new_params.h"
#include "app/context.h"
#include "app/context_flags.h"
#include "app/site.h"
#include "app/sprite_sheet_data_format.h"
#include "app/ui/layer_frame_comboboxes.h"
#include "doc/layer.h"

namespace app {
class Command;

class ExportTilesetCommand : public ExportSpriteSheetCommand {
public:
  ExportTilesetCommand();

protected:
  void onResetValues() override;
  bool onEnabled(Context* context) override;
  void onExecute(Context* context) override;
};

ExportTilesetCommand::ExportTilesetCommand() : ExportSpriteSheetCommand(CommandId::ExportTileset())
{
}

void ExportTilesetCommand::onResetValues()
{
  ExportSpriteSheetCommand::onResetValues();

  // Default values for Export Tileset
  params().fromTilesets(true);
  params().layer(kSelectedLayers);
  params().dataFormat(SpriteSheetDataFormat::JsonArray);
}

bool ExportTilesetCommand::onEnabled(Context* ctx)
{
  if (ExportSpriteSheetCommand::onEnabled(ctx) && ctx->checkFlags(ContextFlags::HasActiveLayer)) {
    Site site = ctx->activeSite();
    if (site.layer() && site.layer()->isTilemap())
      return true;
  }
  return false;
}

void ExportTilesetCommand::onExecute(Context* ctx)
{
  ExportSpriteSheetCommand::onExecute(ctx);
}

Command* CommandFactory::createExportTilesetCommand()
{
  return new ExportTilesetCommand;
}

} // namespace app
