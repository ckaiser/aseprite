// Aseprite
// Copyright (C) 2023  Igara Studio S.A.
// Copyright (C) 2017  David Capello
// Copyright (C) 2016  Carlo Caputo
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

class ToggleTimelineThumbnailsCommand : public Command {
public:
  ToggleTimelineThumbnailsCommand() : Command(CommandId::ToggleTimelineThumbnails()) {}

protected:
  bool onChecked(Context* context) override
  {
    DocumentPreferences& docPref = Preferences::instance().document(context->activeDocument());
    return docPref.thumbnails.enabled();
  }

  void onExecute(Context* context) override
  {
    DocumentPreferences& docPref = Preferences::instance().document(context->activeDocument());

    // Loading default zoom when activating thumbnail
    if (docPref.thumbnails.zoom() <= 1 && !docPref.thumbnails.enabled())
      docPref.thumbnails.zoom(2);

    docPref.thumbnails.enabled(!docPref.thumbnails.enabled());
  }
};

Command* CommandFactory::createToggleTimelineThumbnailsCommand()
{
  return new ToggleTimelineThumbnailsCommand;
}

} // namespace app
