// Aseprite
// Copyright (C) 2001-2018  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "app/cmd/set_audio.h"

namespace app { namespace cmd {

using namespace doc;

SetAudio::SetAudio(Sprite* sprite, std::string filename) : WithSprite(sprite), m_filename(filename)
{
}

void SetAudio::onExecute()
{
  /*
  Sprite* sprite = this->sprite();
  auto doc = static_cast<Doc*>(sprite->document());

  sprite->addFrame(m_newFrame);
  sprite->incrementVersion();

  if (m_addCel) {
    m_addCel->redo();
  }
  else {
    LayerImage* bglayer = sprite->backgroundLayer();
    if (bglayer) {
      ImageRef bgimage(Image::create(sprite->pixelFormat(), sprite->width(), sprite->height()));
      clear_image(bgimage.get(), doc->bgColor(bglayer));
      Cel* cel = new Cel(m_newFrame, bgimage);
      m_addCel.reset(new cmd::AddCel(bglayer, cel));
      m_addCel->execute(context());
    }
  }

  // Notify observers about the new frame.
  DocEvent ev(doc);
  ev.sprite(sprite);
  ev.frame(m_newFrame);
  doc->notify_observers<DocEvent&>(&DocObserver::onAddFrame, ev);
  */
}

void SetAudio::onUndo()
{
  /*
  Sprite* sprite = this->sprite();
  auto doc = static_cast<Doc*>(sprite->document());

  if (m_addCel)
    m_addCel->undo();

  sprite->removeFrame(m_newFrame);
  sprite->incrementVersion();

  // Notify observers about the new frame.
  DocEvent ev(doc);
  ev.sprite(sprite);
  ev.frame(m_newFrame);
  doc->notify_observers<DocEvent&>(&DocObserver::onRemoveFrame, ev);
  */
}

}} // namespace app::cmd
