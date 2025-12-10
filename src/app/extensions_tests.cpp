// Aseprite
// Copyright (C) 2025  Igara Studio S.A.
// Copyright (C) 2001-2016  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#include "tests/app_test.h"

#ifdef ENABLE_SCRIPTING
  #include "app.h"
  #include "app/commands/commands.h"
  #include "app/extensions.h"
  #include "app/ini_file.h"
  #include "base/fs.h"
  #include "commands/command.h"
  #include "context.h"
  #include "fmt/chrono.h"
  #include "pref/preferences.h"

  #include "archive.h"
  #include "archive_entry.h"

  #include <fstream>

using namespace app;

namespace {

  #define EXPECT_EQ_PATH(path1, path2)                                                             \
    EXPECT_EQ(base::fix_path_separators(path1), base::fix_path_separators(path2))

constexpr const auto* kTempINI = "_extensions.ini";
constexpr const auto* kExtensionsFolder = "_extensions/";
constexpr const auto* kExtensionFolder = "_extensions/_extension";
constexpr const auto* kExtensionPackageJSON = "_extensions/_extension/package.json";
constexpr const auto* kExtensionScriptLua = "_extensions/_extension/script.lua";
constexpr const auto* kExtensionScript2Lua = "_extensions/_extension/script2.lua";
constexpr const auto* kExtensionPrefLua = "_extensions/_extension/__pref.lua";
constexpr const auto* kExtensionMatrix1Png = "_extensions/_extension/matrix1.png";
constexpr const auto* kExtensionZip = "test-extension.aseprite-extension";

constexpr const auto* kExtensionPackageJSONData = R"(
{
  "name": "test-extension",
  "displayName": "Test Extension 1",
  "description": "A Test Extension",
  "version": "0.1",
  "author": { "name": "Test",
              "email": "test@igara.com",
              "url": "https://aseprite.org/" },
  "contributes": {
    "scripts": [
        { "path": "./script.lua" }
    ],
    "ditheringMatrices": [
      {
        "id": "matrix1",
        "name": "Matrix 1",
        "path": "./matrix1.png"
      },
      {
        "id": "matrix2",
        "name": "Matrix 2",
        "path": "./matrix2.png"
      }
    ],
    "palettes": [
      { "id": "palette1", "path": "./palette.gpl" }
    ],
    "themes": [
      { "id": "theme1", "path": "." }
    ],
   "languages": [
      { "id": "klin1234",
        "path": "./klin1234.ini",
        "displayName": "Klingon" }
    ],
    "keys": [
      { "id": "keys1", "path": "./keys.aseprite-keys" }
    ]
  }
}
)";

constexpr const auto* kExtensionPackageJSONData2 = R"(
{
  "name": "test-extension2",
  "displayName": "Test Extension 2",
  "description": "A Test Extension",
  "version": "0.2",
  "author": { "name": "Test",
              "email": "test@igara.com",
              "url": "https://aseprite.org/" },
  "contributes": {
    "scripts":  "./script.lua"
  }
}
)";

constexpr const auto* kExtensionPackageJSONData3 = R"(
{
  "name": "test-extension3",
  "displayName": "Test Extension 3",
  "description": "A Test Extension",
  "version": "0.3",
  "author": { "name": "Test",
              "email": "test@igara.com",
              "url": "https://aseprite.org/" },
  "contributes": {
    "scripts":  "./script2.lua"
  }
}
)";

constexpr const auto* kExtensionScriptLuaData = R"(
function init(plugin)
  if plugin.preferences.count == nil then
    plugin.preferences.count = 0
  end

  -- Check serialization
  if plugin.preferences.string ~= "hello" then error() end
  if plugin.preferences.bone ~= true then error() end
  if plugin.preferences.btwo ~= false then error() end
  if plugin.preferences["spc-chars"] ~= "ünicode" then error() end

  if plugin.preferences.table.one ~= 1 or plugin.preferences.table.two ~= 2 then
    error()
  end

  plugin.preferences.starting_pref = plugin.preferences.starting_pref + 1

  plugin:newMenuGroup{
    id="new_group_id",
    title="Menu Item Label",
    group="parent_group_id"
  }
  plugin:newMenuGroup{
    id="new_group_id_2",
    title="Menu Item Label 2",
    group="new_group_id"
  }
  plugin:newMenuGroup{
    id="new_group_id_3",
    title="Menu Item Label 3",
    group="new_group_id"
  }
  plugin:deleteMenuGroup("new_group_id_3")

  plugin:newCommand{
    id="TestCommand",
    title="Test Command",
    group="new_group_id_2",
    onclick=function()
      plugin.preferences.count = plugin.preferences.count + 1
    end
  }

  plugin:newCommand{
    id="DeleteMeCommand",
    group="new_group_id_2",
    title="For deletion",
    onclick=function() end
  }

  plugin:deleteCommand("DeleteMeCommand")
end

function exit(plugin)
end
)";

constexpr const auto* kExtensionStartingPrefLuaData = R"(
  return {starting_pref=1234,string="hello",bone=true,btwo=false,table={one=1,two=2},["spc-chars"]="ünicode"}
)";

constexpr unsigned char kExtensionMatrix1PngData[] = {
  0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48,
  0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x06, 0x00, 0x00,
  0x00, 0x1F, 0x15, 0xC4, 0x89, 0x00, 0x00, 0x00, 0x0A, 0x49, 0x44, 0x41, 0x54, 0x78,
  0x9C, 0x63, 0x00, 0x01, 0x00, 0x00, 0x05, 0x00, 0x01, 0x0D, 0x0A, 0x2D, 0xB4, 0x00,
  0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82
};

void zip_folder(const std::string& folder_path, const std::string& output_zip)
{
  const auto a =
    std::unique_ptr<archive, void (*)(archive*)>(archive_write_new(), [](struct archive* a) {
      EXPECT_EQ(archive_write_close(a), ARCHIVE_OK);
      EXPECT_EQ(archive_write_free(a), ARCHIVE_OK);
    });

  if (archive_write_set_format_zip(a.get()) != ARCHIVE_OK ||
      archive_write_open_filename(a.get(), output_zip.c_str()) != ARCHIVE_OK) {
    FAIL() << "Could create the zip file";
  }

  std::vector<char> buffer(8192);
  for (const auto& item : base::list_files(folder_path, base::ItemType::Files)) {
    auto fn = base::join_path(kExtensionFolder, item);

    const auto e = std::unique_ptr<struct archive_entry, void (*)(struct archive_entry*)>(
      archive_entry_new(),
      &archive_entry_free);

    archive_entry_set_pathname(e.get(), base::get_relative_path(fn, folder_path).c_str());
    archive_entry_set_perm(e.get(), 0644);
    archive_entry_set_filetype(e.get(), AE_IFREG);
    archive_entry_set_size(e.get(), base::file_size(fn));
    archive_write_header(a.get(), e.get());

    std::ifstream ifs(fn, std::ios::binary);
    while (ifs.read(buffer.data(), buffer.size()) || ifs.gcount()) {
      if (archive_write_data(a.get(), buffer.data(), ifs.gcount()) < 0) {
        FAIL() << "Failed to write to the zip file";
      }
    }
  }
}

void delete_folders()
{
  for (const auto& item : base::list_files(kExtensionFolder)) {
    const std::string file = base::join_path(kExtensionFolder, item);
    if (base::is_file(file)) {
      base::delete_file(file);
    }
  }

  if (base::is_directory(kExtensionFolder))
    base::remove_directory(kExtensionFolder);

  if (base::is_directory(kExtensionsFolder))
    base::remove_directory(kExtensionsFolder);
}

void create_extension_environment(const std::string& packageJSONData, bool zip_it = false)
{
  delete_folders();

  base::make_directory(kExtensionsFolder);
  base::make_directory(kExtensionFolder);

  if (base::is_file(kTempINI))
    base::delete_file(kTempINI);

  set_config_file(kTempINI);
  set_main_config_filename(kTempINI);

  std::ofstream(kExtensionPackageJSON) << packageJSONData;
  std::ofstream(kExtensionScriptLua) << kExtensionScriptLuaData;
  std::ofstream(kExtensionScript2Lua) << "";
  std::ofstream(kExtensionPrefLua) << kExtensionStartingPrefLuaData;

  {
    std::ofstream png(kExtensionMatrix1Png, std::ifstream::binary);
    png.write(reinterpret_cast<const char*>(kExtensionMatrix1PngData),
              sizeof(kExtensionMatrix1PngData));
  }

  EXPECT_EQ(base::is_file(kExtensionPackageJSON), true);
  EXPECT_EQ(base::is_file(kExtensionScriptLua), true);

  if (zip_it) {
    if (base::is_file(kExtensionZip))
      base::delete_file(kExtensionZip);

    zip_folder(kExtensionFolder, kExtensionZip);
  }

  if (!App::instance()) {
    new App();
    new Preferences();
    new Commands();
  }
}
} // namespace

TEST(Extensions, Basic)
{
  create_extension_environment(kExtensionPackageJSONData);

  int menuItemRemoveCount = 0;
  auto menuItemRemoveC = [&menuItemRemoveCount](Command*) { menuItemRemoveCount++; };

  std::vector<std::string> menuItemGroupIds;
  auto menuGroupRemove = [&menuItemGroupIds](const std::string& id) {
    menuItemGroupIds.push_back(id);
  };

  auto extensions = Extensions(kExtensionsFolder);
  Extension* testExt = nullptr;
  for (auto* ext : extensions) {
    EXPECT_EQ(testExt, nullptr);
    testExt = ext;
    EXPECT_EQ_PATH(ext->path(), kExtensionFolder);
    EXPECT_EQ(ext->name(), "test-extension");
    EXPECT_EQ(ext->displayName(), "Test Extension 1");
    EXPECT_EQ(ext->category(), Extension::Category::Multiple);
    EXPECT_EQ(ext->version(), "0.1");

    EXPECT_EQ(ext->keys().size(), 1);
    EXPECT_EQ(ext->languages().size(), 1);
    EXPECT_EQ(ext->themes().size(), 1);
    EXPECT_EQ(ext->palettes().size(), 1);

    EXPECT_EQ(ext->hasScripts(), true);
    EXPECT_EQ(ext->hasDitheringMatrices(), true);

    EXPECT_EQ(ext->canBeDisabled(), true);

    // With the test constructor, this ends up being false because we're not in the normal path.
    EXPECT_EQ(ext->canBeUninstalled(), false);
    EXPECT_EQ(ext->isCurrentTheme(), false);
    EXPECT_EQ(ext->isDefaultTheme(), false);

    ext->MenuItemRemoveWidget.connect([](ui::Widget*) {
      FAIL(); // Shouldn't happen since we're in "CLI" mode
      // TODO: Finding a way to mock AppMenus would be useful.
    });
    ext->MenuItemRemoveCommand.connect(menuItemRemoveC);
    ext->MenuGroupRemove.connect(menuGroupRemove);
  }

  EXPECT_FALSE(extensions.ditheringMatrix("matrix0"));
  EXPECT_TRUE(extensions.ditheringMatrix("matrix1"));
  // Should throw because we can't find the file
  EXPECT_THROW(extensions.ditheringMatrix("matrix2"), std::runtime_error);

  EXPECT_EQ(extensions.palettePath("palette0"), "");
  EXPECT_EQ_PATH(extensions.palettePath("palette1"), "_extensions/_extension/./palette.gpl");

  EXPECT_EQ(extensions.themePath("theme0"), "");
  EXPECT_EQ_PATH(extensions.themePath("theme1"), "_extensions/_extension/.");

  EXPECT_EQ(extensions.languagePath("nolang"), "");
  EXPECT_EQ_PATH(extensions.languagePath("klin1234"), "_extensions/_extension/./klin1234.ini");

  EXPECT_FALSE(Commands::instance()->byId("TestCommand"));

  extensions.executeInitActions();

  EXPECT_EQ(extensions.palettes().size(), 1);
  EXPECT_EQ(extensions.ditheringMatrices().size(), 2);

  EXPECT_FALSE(Commands::instance()->byId("DeleteMeCommand"));

  auto* command = Commands::instance()->byId("TestCommand");
  EXPECT_TRUE(command);

  auto* ctx = new Context();
  ctx->executeCommand(command);

  extensions.enableExtension(testExt, false);

  EXPECT_EQ(extensions.palettes().size(), 0);
  EXPECT_FALSE(Commands::instance()->byId("TestCommand"));

  EXPECT_EQ(menuItemRemoveCount, 1);
  EXPECT_EQ(menuItemGroupIds.size(), 2);

  EXPECT_EQ(menuItemGroupIds[0], "new_group_id_2");
  EXPECT_EQ(menuItemGroupIds[1], "new_group_id");

  const std::ifstream ifs(kExtensionPrefLua);
  std::stringstream buffer;
  buffer << ifs.rdbuf();

  // Make sure all our values got serialized correctly into __prefs.lua
  const auto pref = buffer.str();
  const std::vector<std::string> serializedResults = { "count=1",
                                                       "starting_pref=1235",
                                                       "string=\"hello\"",
                                                       "bone=true",
                                                       "btwo=false",
                                                       "table={", // Separating table{one=1,two=2}
                                                                  // because serialization
                                                       "one=1", // is not deterministic in the order
                                                                // they appear
                                                       "two=2", // but all that matters is that they
                                                                // show up.
                                                       R"(["spc-chars"]="ünicode")" };
  for (const auto& expected : serializedResults) {
    if (pref.find(expected) == std::string::npos)
      FAIL() << "Could not find serialized value " << expected << " in __prefs.lua: " << pref;
  }

  int extSignalCount = 0;
  auto extSignalSum = [&extSignalCount, testExt](const Extension* ext) {
    if (ext != nullptr)
      EXPECT_EQ(ext, testExt);

    extSignalCount++;
  };
  extensions.KeysChange.connect(extSignalSum);
  extensions.LanguagesChange.connect(extSignalSum);
  extensions.ThemesChange.connect(extSignalSum);
  extensions.PalettesChange.connect(extSignalSum);
  extensions.DitheringMatricesChange.connect(extSignalSum);
  extensions.ScriptsChange.connect(extSignalSum);

  extensions.enableExtension(testExt, true);

  EXPECT_EQ(extSignalCount, 6);
  EXPECT_TRUE(Commands::instance()->byId("TestCommand"));

  extensions.executeExitActions();

  EXPECT_FALSE(Commands::instance()->byId("TestCommand"));
}

TEST(Extensions, SimplifiedScript)
{
  create_extension_environment(kExtensionPackageJSONData2);
  auto extensions = Extensions(kExtensionsFolder);
  for (const auto* ext : extensions) {
    EXPECT_EQ(ext->name(), "test-extension2");
    EXPECT_EQ(ext->displayName(), "Test Extension 2");
    EXPECT_EQ(ext->category(), Extension::Category::Scripts);
  }

  extensions.executeInitActions();
  EXPECT_TRUE(Commands::instance()->byId("TestCommand"));
  extensions.executeExitActions();
}

TEST(Extensions, EmptyScriptNoInit)
{
  create_extension_environment(kExtensionPackageJSONData3);
  auto extensions = Extensions(kExtensionsFolder);
  for (const auto* ext : extensions) {
    EXPECT_EQ(ext->name(), "test-extension3");
    EXPECT_EQ(ext->displayName(), "Test Extension 3");
    EXPECT_EQ(ext->category(), Extension::Category::Scripts);
  }

  extensions.executeInitActions();
  EXPECT_FALSE(Commands::instance()->byId("TestCommand"));
  extensions.executeExitActions();
}

TEST(Extensions, ZipInstall)
{
  create_extension_environment(kExtensionPackageJSONData2, true);
  delete_folders();
  base::make_directory(kExtensionsFolder);

  auto extensions = Extensions(kExtensionsFolder);
  EXPECT_EQ(std::distance(extensions.begin(), extensions.end()), 0);

  auto info = extensions.getCompressedExtensionInfo(kExtensionZip);
  auto dstPath = base::join_path(kExtensionsFolder, info.name);

  EXPECT_EQ(info.name, "test-extension2");
  EXPECT_EQ(info.version, "0.2");
  EXPECT_EQ(info.dstPath, dstPath);
  EXPECT_EQ(info.defaultTheme, false);

  EXPECT_FALSE(base::is_directory(dstPath));

  extensions.installCompressedExtension(kExtensionZip, info);

  EXPECT_TRUE(base::is_directory(dstPath));

  Extension* installedExt = nullptr;
  for (auto* ext : extensions) {
    installedExt = ext;
    EXPECT_EQ(ext->name(), "test-extension2");
    EXPECT_EQ(ext->displayName(), "Test Extension 2");
    EXPECT_EQ(ext->category(), Extension::Category::Scripts);
  }
  EXPECT_TRUE(installedExt);

  extensions.uninstallExtension(installedExt, DeletePluginPref::Yes);

  EXPECT_FALSE(base::is_directory(dstPath));
}

#endif
