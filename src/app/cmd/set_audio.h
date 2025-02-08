// Aseprite
// Copyright (C) 2001-2018  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifndef APP_CMD_SET_AUDIO_H_INCLUDED
#define APP_CMD_SET_AUDIO_H_INCLUDED
#pragma once

#include "app/cmd.h"
#include "app/cmd/with_sprite.h"

#include <memory>

namespace doc {
class Sprite;
}

namespace app { namespace cmd {
using namespace doc;

class SetAudio : public Cmd,
                 public WithSprite {
public:
  SetAudio(Sprite* sprite, std::string filename);

protected:
  void onExecute() override;
  void onUndo() override;

private:
  std::string m_filename;
};

}} // namespace app::cmd

#endif
