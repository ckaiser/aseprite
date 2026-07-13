// Aseprite
// Copyright (C) 2019-2025  Igara Studio S.A.
// Copyright (C) 2018  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "app/script/security.h"

#include "app/app.h"
#include "app/extensions.h"
#include "app/i18n/strings.h"
#include "app/resource_finder.h"
#include "app/ui/skin/skin_theme.h"
#include "base/file_handle.h"
#include "base/fs.h"
#include "base/fstream_path.h"
#include "base/launcher.h"
#include "script_access.xml.h"
#include "ui/alert.h"
#include "ui/menu.h"
#include "ui/system.h"

#ifdef LAF_WINDOWS
  #include <shlwapi.h>
#else
  #include <fnmatch.h>
#endif

#include "app/script/about_extension_window.h"

#include <fstream>
#include <sstream>

#include "blake3.h"

namespace app::script {

using namespace std::string_view_literals;
using json = nlohmann::json;
using namespace nlohmann::literals;

namespace {
PermissionStorage* g_instance_override = nullptr;

bool wildcard_match(const std::string& pattern, const std::string& str)
{
#ifdef LAF_WINDOWS
  return PathMatchSpec(base::from_utf8(str).c_str(), base::from_utf8(pattern).c_str());
#else
  return fnmatch(pattern.c_str(), str.c_str(), 0) != FNM_NOMATCH;
#endif
}

std::string get_integrity_hash(const std::string& filename)
{
  auto* file = base::open_file_raw(filename, "r");
  if (!file)
    return std::string();

  blake3_hasher hasher;
  blake3_hasher_init(&hasher);

  constexpr size_t size = 1024uL * 1024;
  std::vector<uint8_t> buffer(size);

  size_t read;
  while ((read = std::fread(buffer.data(), 1, size, file)) > 0)
    blake3_hasher_update(&hasher, buffer.data(), read);
  fclose(file);

  std::vector<uint8_t> hash(BLAKE3_OUT_LEN);
  blake3_hasher_finalize(&hasher, hash.data(), BLAKE3_OUT_LEN);

  std::stringstream stream;
  for (size_t i = 0; i < BLAKE3_OUT_LEN; ++i)
    stream << std::setfill('0') << std::setw(2) << std::hex << static_cast<int>(hash[i]);

  return stream.str();
}

constexpr Permission string_to_permission(const std::string_view s)
{
  if (s == "network")
    return Permission::Network;
  if (s == "sprite_read")
    return Permission::SpriteRead;
  if (s == "sprite_write")
    return Permission::SpriteWrite;
  if (s == "io_read")
    return Permission::IORead;
  if (s == "io_write")
    return Permission::IOWrite;
  if (s == "clipboard_read")
    return Permission::ClipboardRead;
  if (s == "clipboard_write")
    return Permission::ClipboardWrite;
  if (s == "debug")
    return Permission::Debug;
  if (s == "bytecode")
    return Permission::Bytecode;
  if (s == "execute")
    return Permission::Execute;
  if (s == "loadlib")
    return Permission::LoadLib;

  ASSERT(true);
}

constexpr bool permission_is_scary(const Permission p)
{
  switch (p) {
    case Permission::IORead:
    case Permission::IOWrite:
    case Permission::Bytecode:
    case Permission::Execute:
    case Permission::LoadLib:  return true;
    default:                   return false;
  }
}
} // namespace

constexpr std::string_view permission_to_string(const Permission p)
{
  switch (p) {
    case Permission::Network:        return "network"sv;
    case Permission::SpriteRead:     return "sprite_read"sv;
    case Permission::SpriteWrite:    return "sprite_write"sv;
    case Permission::IORead:         return "io_read"sv;
    case Permission::IOWrite:        return "io_write"sv;
    case Permission::ClipboardRead:  return "clipboard_read"sv;
    case Permission::ClipboardWrite: return "clipboard_write"sv;
    case Permission::Debug:          return "debug"sv;
    case Permission::Bytecode:       return "bytecode"sv;
    case Permission::Execute:        return "execute"sv;
    case Permission::LoadLib:        return "loadlib"sv;
    default:                         return "unknown"sv;
  }
}

constexpr bool permission_supports_url(const Permission p)
{
  switch (p) {
    case Permission::Network:
    case Permission::SpriteRead:
    case Permission::SpriteWrite:
    case Permission::IORead:
    case Permission::IOWrite:
    case Permission::Execute:
    case Permission::LoadLib:     return true;
    default:                      return false;
  }
}

void set_permission_storage(PermissionStorage* storage)
{
  g_instance_override = storage;
}

PermissionStorage::PermissionStorage()
{
  ResourceFinder rf;
  rf.includeUserDir("permissions.json");
  m_path = rf.getFirstOrCreateDefault();
  load();
}

PermissionStorage::PermissionStorage(const std::string& path) : m_path(path)
{
  load();
}

std::optional<bool> PermissionStorage::read(const std::string& script,
                                            const Permission permission) const
{
  ASSERT(!permission_supports_url(permission))

  if (!m_json.contains(script))
    return std::nullopt;

  if (readFullAccess(script))
    return true;

  const auto& permissionString = permission_to_string(permission);
  if (m_json[script].contains("permissions"sv) &&
      m_json[script]["permissions"sv].contains(permissionString)) {
    return m_json[script]["permissions"sv][permissionString].value("granted"sv, false);
  }

  return std::nullopt;
}

std::optional<bool> PermissionStorage::readForUrl(const std::string& script,
                                                  const Permission permission,
                                                  const std::string& url)
{
  ASSERT(permission_supports_url(permission))

  if (!m_json.contains(script))
    return std::nullopt;

  if (readFullAccess(script))
    return true;

  const std::string permissionString(permission_to_string(permission));

  json::json_pointer ptr("/permissions");
  ptr /= permissionString;

  if (m_json[script].contains(ptr)) {
    for (const auto& it : m_json[script][ptr].items()) {
      const auto& permUrl = it.value().value("url", "");
      if (wildcard_match(permUrl, url)) {
        return it.value().value("granted", false);
      }
    }
  }

  return std::nullopt;
}

void PermissionStorage::write(const std::string& script, const Permission permission, bool value)
{
  ASSERT(!permission_supports_url(permission))

  const auto& permissionString = permission_to_string(permission);

  if (!m_json.contains(script))
    m_json[script]["integrity"sv] = get_integrity_hash(script);

  m_json[script]["permissions"sv][permissionString] = json::object({
    { "granted"sv, value }
  });

  flush();
}

void PermissionStorage::writeForUrl(const std::string& script,
                                    const Permission permission,
                                    const std::string& url,
                                    const bool value)
{
  ASSERT(permission_supports_url(permission))

  const std::string permissionString(permission_to_string(permission));

  json::json_pointer ptr("/permissions");
  ptr /= permissionString;

  if (!m_json.contains(script))
    m_json[script]["integrity"sv] = get_integrity_hash(script);

  auto obj = json::object({
    { "url",     url   },
    { "granted", value }
  });
  if (url == "*") {
    // Replace all other objects
    m_json[script][ptr] = { obj };
  }
  else if (m_json[script][ptr].is_array()) {
    // Modify existing URL if it already exists
    for (const auto& it : m_json[script][ptr].items()) {
      if (it.value().value("url", "") == url) {
        m_json[script][ptr / it.key()] = value;
        return;
      }
    }

    m_json[script][ptr].push_back(obj);
  }
  else {
    m_json[script][ptr] = { obj };
  }

  flush();
}

bool PermissionStorage::readFullAccess(const std::string& script) const
{
  if (!m_json.contains(script))
    return false;

  return m_json[script].value("full_access"sv, false);
}

void PermissionStorage::writeFullAccess(const std::string& script, const bool access)
{
  m_json[script]["full_access"sv] = access;
  m_json[script]["integrity"sv] = get_integrity_hash(script);
  flush();
}

bool PermissionStorage::passesIntegrityCheck(const std::string& script)
{
  if (!m_json.contains(script))
    return true;

  try {
    const std::string& hash = m_json[script]["integrity"];
    if (hash.size() != BLAKE3_OUT_LEN * 2uL)
      return false;
    return hash == get_integrity_hash(script);
  }
  catch (const std::exception& e) {
    LOG(WARNING,
        "Failed to perform integrity check of script '%s' with error '%s'",
        script.c_str(),
        e.what());
    return false;
  }
}

void PermissionStorage::reset(const std::string& script)
{
  try {
    m_json.erase(script);
  }
  catch (const std::exception& e) {
    LOG(ERROR, "Parsing error while resetting permissions with error '%s'", e.what());
    m_json = json::object();
  }
  flush();
}

std::vector<std::string> PermissionStorage::scripts() const
{
  std::vector<std::string> scripts;
  for (const auto& [key, _] : m_json.items())
    scripts.push_back(key);
  return scripts;
}

std::vector<Permission> PermissionStorage::stored(const std::string& script) const
{
  std::vector<Permission> stored;

  if (!m_json.contains(script) || !m_json[script].contains("permissions"sv))
    return stored;

  for (const auto& [key, _] : m_json[script]["permissions"sv].items()) {
    stored.push_back(string_to_permission(key));
  }

  return stored;
}

std::vector<std::pair<std::string, bool>> PermissionStorage::urls(const std::string& script,
                                                                  const Permission permission) const
{
  std::vector<std::pair<std::string, bool>> urls;

  if (!m_json.contains(script) || !m_json[script].contains("permissions"sv))
    return urls;

  const std::string permissionString(permission_to_string(permission));
  json::json_pointer ptr("/permissions");
  ptr /= permissionString;

  if (m_json[script][ptr].is_array()) {
    for (const auto& it : m_json[script][ptr].items()) {
      urls.emplace_back(it.value()["url"], it.value()["granted"]);
    }
  }

  return urls;
}

void PermissionStorage::load()
{
  try {
    const auto file = base::open_file(m_path, "rb");
    if (file)
      m_json = json::parse(file.get());
  }
  catch (const std::exception& e) {
    LOG(ERROR, "Failed to load permissions at '%s' with error '%s'", m_path.c_str(), e.what());
    m_json = json::object();
    flush();
  }
}

void PermissionStorage::flush()
{
  if (m_pendingFlush)
    return;

  m_pendingFlush = true;
  execute_now_or_enqueue([this] {
    m_pendingFlush = false;
    std::ofstream out(FSTREAM_PATH(m_path));
    if (out)
      out << m_json.dump();
    else
      LOG(ERROR, "Failed to write permissions to '%s'", m_path.c_str());
  });
}

PermissionStorage* PermissionStorage::instance()
{
  if (g_instance_override)
    return g_instance_override;
  static PermissionStorage instance;
  return &instance;
}

PermissionDialog::PermissionDialog(const std::string& origin,
                                   const std::string& extensionName,
                                   const Permission permission,
                                   const std::string& url)
  : m_isExtension(!extensionName.empty())
  , m_timer(1000)
  , m_scaryAllowCooldown(0) // TODO: 4000? Configurable?
{
  if (m_isExtension) {
    originLink()->setText(extensionName);
    originLink()->Click.connect([extensionName] {
      if (const auto* ext = App::instance()->extensions().find(extensionName))
        AboutExtensionWindow::show(ext);
    });
    requiresPreamble()->setText(Strings::script_access_the_extension());
  }
  else {
    originLink()->setText(base::get_file_name(origin));
    originLink()->Click.connect(
      [origin] { base::launcher::open_file(base::get_file_path(origin)); });
    tooltipManager()->addTooltipFor(originLink(), origin, ui::BOTTOM);
    requiresPreamble()->setText(Strings::script_access_the_script());
  }

  if (permission == Permission::IORead || permission == Permission::IOWrite ||
      permission == Permission::SpriteRead || permission == Permission::SpriteWrite) {
    remember()->setVisible(false);
    rememberFile()->setVisible(true);
    rememberDirectory()->setVisible(true);
  }

  permissionWarning()->setVisible(permission_is_scary(permission));

  if (url.empty()) {
    urlView()->setVisible(false);
  }
  else {
    // TODO: We should move the caret to the end, because that's usually the more relevant bit for
    // files and it'll scroll the TextEdit also if the path is a relative path we could shorten it?
    // Also here is where we'd show an open folder button and a quick copy button
    urlText()->setText(url);
    urlText()->selectAll();
  }

  const std::string& who = m_isExtension ? Strings::script_access_extension() :
                                           Strings::script_access_script();
  switch (permission) {
    case Permission::Network:
      wants()->setText(Strings::script_access_wants_network());
      permissionLabel()->setText(Strings::script_access_permission_network());
      break;
    case Permission::SpriteRead:
      wants()->setText(Strings::script_access_wants_sprite_read());
      permissionLabel()->setText(Strings::script_access_permission_sprite_read());
      break;
    case Permission::SpriteWrite:
      // TODO: We should warn the user if the file already exists for any write actions
      wants()->setText(Strings::script_access_wants_sprite_write());
      permissionLabel()->setText(Strings::script_access_permission_sprite_write());
      break;
    case Permission::IORead:
      wants()->setText(Strings::script_access_wants_io_read());
      permissionLabel()->setText(Strings::script_access_permission_io_read());
      permissionWarning()->setText(Strings::script_access_permission_io_read_warning(who));
      break;
    case Permission::IOWrite:
      wants()->setText(Strings::script_access_wants_io_write());
      permissionLabel()->setText(Strings::script_access_permission_io_write());
      permissionWarning()->setText(Strings::script_access_permission_io_write_warning(who));
      break;
    case Permission::ClipboardRead:
      permissionLabel()->setText(Strings::script_access_permission_clipboard_read());
      break;
    case Permission::ClipboardWrite:
      permissionLabel()->setText(Strings::script_access_permission_clipboard_write());
      break;
    case Permission::Debug:
      permissionLabel()->setText(Strings::script_access_permission_debug());
      break;
    case Permission::Bytecode:
      permissionLabel()->setText(Strings::script_access_permission_bytecode());
      permissionWarning()->setText(Strings::script_access_permission_bytecode_warning(who));
      break;
    case Permission::Execute:
      wants()->setText(Strings::script_access_wants_execute());
      permissionLabel()->setText(Strings::script_access_permission_execute());
      permissionWarning()->setText(Strings::script_access_permission_execute_warning(who));
      break;
    case Permission::LoadLib:
      wants()->setText(Strings::script_access_wants_loadlib());
      permissionLabel()->setText(Strings::script_access_permission_loadlib());
      permissionWarning()->setText(Strings::script_access_permission_loadlib_warning(who));
      break;
  }

  wants()->setVisible(!wants()->text().empty());
  dotdotdot()->Click.connect(&PermissionDialog::showPopup, this);

  if (permission_is_scary(permission)) {
    permissionIcon()->setSurface(skin::SkinTheme::get(this)->parts.warningBox()->bitmapRef(0));

    if (m_scaryAllowCooldown > 0) {
      const std::string& original = allow()->text();
      auto tick = [this, original] {
        m_scaryAllowCooldown -= m_timer.interval();
        if (m_scaryAllowCooldown <= 0) {
          allow()->setText(original);
          allow()->setEnabled(true);
          m_timer.stop();
        }
        else {
          allow()->setText(
            fmt::format("{} ({})", original, std::ceil(m_scaryAllowCooldown / 1000.0)));
        }
      };
      m_scaryAllowCooldown += m_timer.interval();
      tick();
      m_timer.Tick.connect(tick);
      m_timer.start();
    }
    else {
      allow()->setEnabled(true);
    }
  }
  else {
    allow()->setEnabled(true);
  }
}

std::pair<bool, PermissionDialog::Remember> PermissionDialog::ask()
{
  openWindowInForeground();

  if (closer() == dotdotdot())
    return std::make_pair(true, Remember::FullAccess);

  auto shouldRemember = Remember::Nothing;
  if (remember()->isSelected())
    shouldRemember = urlText()->text().empty() ? Remember::Permission : Remember::URL;
  if (rememberFile()->isSelected())
    shouldRemember = Remember::URL;
  if (rememberDirectory()->isSelected())
    shouldRemember = Remember::Directory;

  return std::make_pair(closer() == allow(), shouldRemember);
}

void PermissionDialog::showPopup()
{
  ui::Menu menu;
  ui::MenuItem fullAccess(Strings::script_access_give_full_access());
  fullAccess.Click.connect([this] {
    if (ui::Alert::show(Strings::alerts_permissions_full_access(
          m_isExtension ? Strings::script_access_extension() : Strings::script_access_script())) ==
        1)
      closeWindow(dotdotdot());
  });
  ui::MenuItem help(Strings::script_access_help());
  help.Click.connect([] { base::launcher::open_url("https://www.aseprite.org/api/permissions"); });

  menu.addChild(&fullAccess);
  menu.addChild(new ui::MenuSeparator);
  menu.addChild(&help);

  auto bounds = dotdotdot()->bounds();
  menu.showPopup(gfx::Point(bounds.x, bounds.y2()), display());
}

} // namespace app::script
