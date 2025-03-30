DOC_TRACE("DOC: Deleting", this);
  removeFromContext();
  delete m_undo;
  delete m_mask;
  m_format_options = nullptr;
}

void Doc::setContext(Context* ctx)

=======
  DOC_TRACE("DOC: Deleting", this);
  removeFromContext();
  delete m_undo;
  delete m_mask;
  m_format_options = nullptr;
  // The Sprite is deleted by the Sprites list (see doc/document.cpp)
}

void Doc::setContext(Context* ctx)

>>>>>>> REPLACE
