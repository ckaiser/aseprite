// Aseprite
// Copyright (C) 2024 Igara Studio S.A.
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifndef APP_CMD_drop_on_timeline_H_INCLUDED
#define APP_CMD_drop_on_timeline_H_INCLUDED
#pragma once

#include "app/cmd/with_document.h"
#include "app/cmd_sequence.h"
#include "app/doc_observer.h"
#include "base/paths.h"
#include "doc/frame.h"
#include "doc/image_ref.h"
#include "doc/layer.h"
#include "doc/layer_list.h"

namespace app { namespace cmd {

class DropOnTimeline : public CmdSequence,
                       public WithDocument {
public:
  enum class InsertionPoint {
    BeforeLayer,
    AfterLayer,
  };

  enum class DroppedOn {
    Unspecified,
    Frame,
    Layer,
    Cel,
  };

  // Inserts the layers and frames of the documents pointed by the specified
  // paths, at the specified frame and before or after the specified layer index.
  DropOnTimeline(app::Doc* doc,
                 doc::frame_t frame,
                 doc::layer_t layerIndex,
                 InsertionPoint insert,
                 DroppedOn droppedOn,
                 const base::paths& paths);

  // Inserts the image as if it were a document with just one layer and one
  // frame, at the specified frame and before or after the specified layer index.
  DropOnTimeline(app::Doc* doc,
                 doc::frame_t frame,
                 doc::layer_t layerIndex,
                 InsertionPoint insert,
                 DroppedOn droppedOn,
                 const doc::ImageRef& image);

protected:
  void onExecute() override;
  void onUndo() override;
  void onRedo() override;
  size_t onMemSize() const override { return sizeof(*this) + m_size; }

private:
  void setupInsertionLayer(doc::Layer** layer, doc::LayerGroup** group);
  void insertDroppedLayers(bool incGroupVersion);
  bool canMoveCelFrom(app::Doc* srcDoc);
  void notifyAddLayer(doc::Layer* layer);
  void notifyDocObservers(doc::Layer* layer);
  bool hasPendingWork();
  // Sets srcDoc's Doc* pointer to the next document to be processed.
  // Returns false when the user cancelled the process, or true when the
  // process must go on.
  bool getNextDoc(Doc** srcDoc);
  bool getNextDocFromImage(Doc** srcDoc);
  bool getNextDocFromPaths(Doc** srcDoc);

  size_t m_size;
  base::paths m_paths;
  doc::ImageRef m_image = nullptr;
  doc::frame_t m_frame;
  doc::layer_t m_layerIndex;
  InsertionPoint m_insert;
  DroppedOn m_droppedOn;
  // Holds the list of layers dropped into the document. Used to support
  // undo/redo without having to read all the files again.
  doc::LayerList m_droppedLayers;
  // Number of frames the doc had before dropping.
  doc::frame_t m_previousTotalFrames;
};

}} // namespace app::cmd

#endif