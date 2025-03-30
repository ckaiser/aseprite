doc::Layer* AsepriteDecoder::readLayerChunk(AsepriteHeader* header,
                                            doc::Sprite* sprite,
                                            doc::Layer** previous_layer,
                                            int* current_level)
{
  // Read chunk data
  int flags = read16();
  int layer_type = read16();
  int child_level = read16();
  read16();                 // default width
  read16();                 // default height
  int blendmode = read16(); // blend mode
  int opacity = read8();    // opacity
  readPadding(3);
  std::string name = readString();
  try {
    for (uint32_t i = 0; i < n; ++i) {
      uint32_t id = read32();
      uint8_t type = read8();
      readPadding(7);
      std::string fn = readString();
      extFiles.insert(id, type, fn);
    }
  } catch (const std::exception& e) {
    delegate()->error(e.what());
  }

  doc::Layer* layer = nullptr;
  try {
    switch (layer_type) {
      case ASE_FILE_LAYER_IMAGE:   layer = new doc::LayerImage(sprite); break;

      case ASE_FILE_LAYER_GROUP:   layer = new doc::LayerGroup(sprite); break;

      case ASE_FILE_LAYER_TILEMAP: {
        doc::tileset_index tsi = read32();
        if (!sprite->tilesets()->get(tsi)) {
          throw std::runtime_error(fmt::format("Error: tileset {0} not found", tsi));
        }
        layer = new doc::LayerTilemap(sprite, tsi);
        break;
      }

      default:
        delegate()->incompatibilityError(fmt::format("Unknown layer type found: {0}", layer_type));
        return nullptr;
    }

    if (layer) {
      if (layer->isImage() &&
          // Only transparent layers can have blend mode and opacity
          !(flags & int(doc::LayerFlags::Background))) {
        static_cast<doc::LayerImage*>(layer)->setBlendMode((doc::BlendMode)blendmode);
        if (header->flags & ASE_FILE_FLAG_LAYER_WITH_OPACITY)
          static_cast<doc::LayerImage*>(layer)->setOpacity(opacity);
      }

      // flags
      layer->setFlags(
        static_cast<doc::LayerFlags>(flags & static_cast<int>(doc::LayerFlags::PersistentFlagsMask)));

      // name
      layer->setName(name.c_str());

      // Child level
      if (child_level == *current_level)
        (*previous_layer)->parent()->addLayer(layer);
      else if (child_level > *current_level)
        static_cast<doc::LayerGroup*>(*previous_layer)->addLayer(layer);
      else if (child_level < *current_level) {
        doc::LayerGroup* parent = (*previous_layer)->parent();
        ASSERT(parent);
        if (parent) {
          int levels = (*current_level - child_level);
          while (levels--) {
            ASSERT(parent->parent());
            if (!parent->parent())
              break;
            parent = parent->parent();
          }
          parent->addLayer(layer);
        }
      }

      *previous_layer = layer;
      *current_level = child_level;
    }
  } catch (const std::runtime_error& e) {
    delegate()->error(e.what());
    delete layer;
    return nullptr;
  }
    fprintf(stderr, "readLayerChunk called, returning %p\n", layer);
  return layer;
}
