// Aseprite
// Copyright (c) 2019-2024  Igara Studio S.A.
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.
#include "app/commands/command.h"
#include "app/commands/command_factory.h"
#include "app/commands/command_ids.h"
#include "app/tilemap_mode.h"
#include "app/ui/color_bar.h"
#include "gfx/fwd.h"

namespace app {

using namespace gfx;

class ToggleTilesModeCommand : public Command {
public:
  ToggleTilesModeCommand() : Command(CommandId::ToggleTilesMode()) {}

protected:
  bool onChecked(Context* context) override
  {
    auto colorBar = ColorBar::instance();
    return (colorBar->tilemapMode() == TilemapMode::Tiles);
  }

  void onExecute(Context* context) override
  {
    auto colorBar = ColorBar::instance();
    if (!colorBar->isTilemapModeLocked()) {
      colorBar->setTilemapMode(
        colorBar->tilemapMode() == TilemapMode::Pixels ? TilemapMode::Tiles : TilemapMode::Pixels);
    }
  }
};

Command* CommandFactory::createToggleTilesModeCommand()
{
  return new ToggleTilesModeCommand;
}

} // namespace app
