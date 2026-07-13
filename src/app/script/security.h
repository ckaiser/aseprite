// Aseprite
// Copyright (C) 2021-2025  Igara Studio S.A.
// Copyright (C) 2018  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifndef APP_SCRIPT_SECURITY_H_INCLUDED
#define APP_SCRIPT_SECURITY_H_INCLUDED
#pragma once

#ifndef ENABLE_SCRIPTING
  #error ENABLE_SCRIPTING must be defined
#endif

#include <string>

#include "script_access.xml.h"

#include "nlohmann/json.hpp"

namespace app::script {

enum class Permission : uint8_t {
  Network,
  SpriteRead,
  SpriteWrite,
  IORead,
  IOWrite,
  ClipboardRead,
  ClipboardWrite,
  Debug,
  Bytecode,
  Execute,
  LoadLib
};

class PermissionStorage {
public:
  PermissionStorage();
  explicit PermissionStorage(const std::string& path);

  static PermissionStorage* instance();

  std::optional<bool> read(const std::string& script, Permission permission) const;
  std::optional<bool> readForUrl(const std::string& script,
                                 Permission permission,
                                 const std::string& url);
  bool readFullAccess(const std::string& script) const;

  void write(const std::string& script, Permission permission, bool value);
  void writeForUrl(const std::string& script,
                   Permission permission,
                   const std::string& url,
                   bool value);
  void writeFullAccess(const std::string& script, bool access);

  bool passesIntegrityCheck(const std::string& script);
  void reset(const std::string& script);

  std::vector<std::string> scripts() const;
  std::vector<Permission> stored(const std::string& script) const;
  std::vector<std::pair<std::string, bool>> urls(const std::string& script,
                                                 Permission permission) const;

private:
  void load();
  void flush();

  nlohmann::json m_json;
  std::string m_path;
  bool m_pendingFlush = false;
};

class PermissionDialog : public app::gen::ScriptAccess {
public:
  enum class Remember : uint8_t { Nothing, Permission, URL, Directory, FullAccess };
  PermissionDialog(const std::string& origin,
                   const std::string& extensionName,
                   Permission permission,
                   const std::string& url);

  std::pair<bool, Remember> ask();

private:
  void showPopup();
  bool m_isExtension;
  ui::Timer m_timer;
  base::tick_t m_scaryAllowCooldown;
};

void set_permission_storage(PermissionStorage* storage);
constexpr std::string_view permission_to_string(Permission p);
constexpr bool permission_supports_url(Permission p);

} // namespace app::script

#endif
