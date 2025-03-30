// Executes the file operation: loads or saves the sprite.
//
// It can be called from a different thread of the one used
// by FileOp::createLoadDocumentOperation() or createSaveDocumentOperation().
//
// After this function you must to mark the FileOp as "done" calling
// FileOp::done() function.
//
// TODO refactor this code
void FileOp::operate(IFileOpProgress* progress)
{
  ASSERT(!isDone());

  m_progressInterface = progress;

  // Load //////////////////////////////////////////////////////////////////////
  if (m_type == FileOpLoad && m_format != NULL && m_format->support(FILE_SUPPORT_LOAD)) {
    // Load a sequence
    if (isSequence()) {
      // Default palette
      m_seq.palette->makeBlack();

      // Load the sequence
      frame_t frames(m_seq.filename_list.size());
      frame_t frame(0);
      Image* old_image = nullptr;
      gfx::Size canvasSize(0, 0);

      // TODO setPalette for each frame???
      auto add_image = [&]() {
        canvasSize |= m_seq.image->size();

        m_seq.last_cel->data()->setImage(m_seq.image, m_seq.layer);
        m_seq.layer->addCel(m_seq.last_cel);

        if (m_document->sprite()->palette(frame)->countDiff(m_seq.palette, NULL, NULL) > 0) {
          m_seq.palette->setFrame(frame);
          m_document->sprite()->setPalette(m_seq.palette, true);
        }

        old_image = m_seq.image.get();
        m_seq.image.reset();
        m_seq.last_cel = NULL;
      };

      m_seq.has_alpha = false;
      m_seq.progress_offset = 0.0f;
      m_seq.progress_fraction = 1.0f / (double)frames;

      auto it = m_seq.filename_list.begin(), end = m_seq.filename_list.end();
      for (; it != end; ++it) {
        m_filename = it->c_str();

        // Call the "load" procedure to read the first bitmap.
        bool loadres = false;
        try {
          loadres = m_format->load(this);
          if (!loadres) {
            setError("Error loading frame %d from file \"%s\"\n", frame + 1, m_filename.c_str());
          }
        }
        catch (const std::exception& e) {
          setError("Exception while loading frame %d from file \"%s\": %s\n", frame + 1, m_filename.c_str(), e.what());
          if (m_seq.image)
            m_seq.image.reset();
          throw;
        }

        // For the first frame...
        if (!old_image) {
          // Error reading the first frame
          if (!loadres || !m_document || !m_seq.last_cel) {
            m_seq.image.reset();
            delete m_seq.last_cel;
            delete m_document;
            m_document = nullptr;
            break;
          }
          // Read ok
          else {
            // Add the keyframe
            add_image();
          }
        }
        // For other frames
        else {
          // All done (or maybe not enough memory)
          if (!loadres || !m_seq.last_cel) {
            m_seq.image.reset();
            delete m_seq.last_cel;
            if (m_document) {
              delete m_document;
              m_document = nullptr;
            }
            break;
          }

          // Compare the old frame with the new one
#if USE_LINK // TODO this should be configurable through a check-box
          if (count_diff_between_images(old_image, m_seq.image)) {
            add_image();
          }
          // We don't need this image
          else {
            m_seq.image.reset();

            // But add a link frame
            m_seq.last_cel->image = image_index;
            layer_add_frame(m_seq.layer, m_seq.last_cel);

            m_seq.last_image = NULL;
            m_seq.last_cel = NULL;
          }
#else
          add_image();
#endif
        }

        m_document->sprite()->setFrameDuration(frame, m_seq.duration);

        ++frame;
        m_seq.progress_offset += m_seq.progress_fraction;
      }
      m_filename = *m_seq.filename_list.begin();

      // Final setup
      if (m_document) {
        // Configure the layer as the 'Background'
        if (!m_seq.has_alpha)
          m_seq.layer->configureAsBackground();

        // Set the final canvas size (as the bigger loaded
        // frame/image).
        m_document->sprite()->setSize(canvasSize.w, canvasSize.h);

        // Set the frames range
        m_document->sprite()->setTotalFrames(frame);

        // Sets special options from the specific format (e.g. BMP
        // file can contain the number of bits per pixel).
        m_document->setFormatOptions(m_formatOptions);
      }
    }
    // Direct load from one file.
    else {
      // Call the "load" procedure.
      bool loadres = false;
      try {
        loadres = m_format->load(this);
        if (!loadres) {
          setError("Error loading sprite from file \"%s\"\n", m_filename.c_str());
        }
      }
      catch (const std::exception& e) {
        setError("Exception while loading sprite from file \"%s\": %s\n", m_filename.c_str(), e.what());
        if (m_seq.image)
          m_seq.image.reset();
        throw;
      }
    }

    // Load special data from .aseprite-data file
    if (m_document && m_document->sprite() && !m_dataFilename.empty()) {
      try {
        load_aseprite_data_file(m_dataFilename, m_document, m_config.defaultSliceColor);
      }
      catch (const std::exception& ex) {
        setError("Error loading data file: %s\n", ex.what());
      }
    }
  }
  // Save //////////////////////////////////////////////////////////////////////
  else if (m_type == FileOpSave && m_format != NULL && m_format->support(FILE_SUPPORT_SAVE)) {
#ifdef ENABLE_SAVE

#if defined(ENABLE_TRIAL_MODE)
    DRM_INVALID
    {
      setError(fmt::format(
                 "Save operation is not supported in trial version, activate this Aseprite first.\n"
                 "Go to {} and get a license key to upgrade.",
                 get_app_download_url())
                 .c_str());
    }
#endif

    // TODO Should we check m_document->isReadOnly() here?  the flag
    //      is already checked in SaveFileBaseCommand::saveDocumentInBackground
    //      and only in UI mode (so the CLI still works)

    // Save a sequence
    if (isSequence()) {
      ASSERT(m_format->support(FILE_SUPPORT_SEQUENCES));

      Sprite* sprite = m_document->sprite();

      // Create a temporary bitmap
      m_seq.image.reset(
        Image::create(sprite->pixelFormat(), m_roi.fileCanvasSize().w, m_roi.fileCanvasSize().h));

      m_seq.progress_offset = 0.0f;
      m_seq.progress_fraction = 1.0f / (double)sprite->totalFrames();

      // For each frame in the sprite.
      render::Render render;
      render.setNewBlend(m_config.newBlend);

      frame_t outputFrame = 0;
      for (frame_t frame : m_roi.framesSequence()) {
        gfx::Rect bounds = m_roi.frameBounds(frame);
        if (bounds.isEmpty())
          continue; // Skip frame because there is no slice key

        if (m_abstractImage) {
          m_abstractImage->setSpecSize(m_roi.fileCanvasSize(), bounds.size());
        }

        // Render the (unscaled) sequenced image.
        render.renderSprite(m_seq.image.get(), sprite, frame, gfx::Clip(gfx::Point(0, 0), bounds));

        bool save = true;

        // Check if we have to ignore empty frames
        if (m_ignoreEmpty && !sprite->isOpaque() && doc::is_empty_image(m_seq.image.get())) {
          save = false;
        }

        if (save) {
          // Setup the palette.
          sprite->palette(frame)->copyColorsTo(m_seq.palette);

          // Setup the filename to be used.
          m_filename = m_seq.filename_list[outputFrame];

          // Make directories
          makeDirectories();

          // Call the "save" procedure... did it fail?
          bool saveres = false;
          try {
            saveres = m_format->save(this);
            if (!saveres) {
              setError("Error saving frame %d in the file \"%s\"\n",
                       outputFrame + 1,
                       m_filename.c_str());
            }
          }
          catch (const std::exception& e) {
            setError("Exception while saving frame %d in the file \"%s\": %s\n",
                     outputFrame + 1,
                     m_filename.c_str(), e.what());
            if (m_seq.image)
              m_seq.image.reset();
            throw;
          }
        }

        m_seq.progress_offset += m_seq.progress_fraction;
        ++outputFrame;
      }

      m_filename = *m_seq.filename_list.begin();

      // Destroy the image
      m_seq.image.reset();
    }
    // Direct save to a file.
    else {
      makeDirectories();

      if (m_abstractImage) {
        m_abstractImage->setSpecSize(m_roi.fileCanvasSize(), m_roi.fileCanvasSize());
      }

      // Call the "save" procedure.
      bool saveres = false;
      try {
        saveres = m_format->save(this);
        if (!saveres) {
          setError("Error saving the sprite in the file \"%s\"\n", m_filename.c_str());
        }
      }
      catch (const std::exception& e) {
        setError("Exception while saving sprite in the file \"%s\": %s\n", m_filename.c_str(), e.what());
        if (m_seq.image)
          m_seq.image.reset();
        throw;
      }
    }

    // Save special data from .aseprite-data file
    if (m_document && m_document->sprite() && !hasError() && !m_dataFilename.empty()) {
      try {
        save_aseprite_data_file(m_dataFilename, m_document);
      }
      catch (const std::exception& ex) {
        setError("Error loading data file: %s\n", ex.what());
      }
    }
#else
    setError(fmt::format("Save operation is not supported in trial version.\n"
                         "Go to {} and get the full-version.",
                         get_app_download_url())
               .c_str());
#endif
  }

  // Progress = 100%
  setProgress(1.0f);
}
