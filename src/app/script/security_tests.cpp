// Aseprite
// Copyright (C) 2026-present  Igara Studio S.A.
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#include "tests/utils.h"
#include <gtest/gtest.h>

const char* g_exeName = nullptr;

#ifdef ENABLE_SCRIPTING
  #include "app/script/security.h"

using namespace app;
using namespace script;

  #define EXPECT_OPT_EQ(opt, val)                                                                  \
    EXPECT_TRUE(opt.has_value());                                                                  \
    EXPECT_EQ(*opt, val)

TEST(PermissionStorage, SimpleReadWrite)
{
  const TestTempFile script("print('example')", "lua");
  const TestTempFile extensionsJSON("{}", "json");
  {
    PermissionStorage storage(extensionsJSON.filename);
    storage.write(script.filename, Permission::ClipboardRead, false);
  }

  {
    PermissionStorage storage(extensionsJSON.filename);
    EXPECT_TRUE(storage.passesIntegrityCheck(script.filename));
    const auto val = storage.read(script.filename, Permission::ClipboardRead);
    EXPECT_TRUE(val.has_value());
    EXPECT_FALSE(*val);
  }

  {
    {
      std::ofstream(script.filename) << "extra";
    }

    PermissionStorage storage(extensionsJSON.filename);
    EXPECT_FALSE(storage.passesIntegrityCheck(script.filename));
    storage.reset(script.filename);
    const auto val = storage.read(script.filename, Permission::ClipboardRead);
    EXPECT_FALSE(val.has_value());
  }
}

TEST(PermissionStorage, ReadWrite)
{
  const TestTempFile script1("print('script1')", "lua");
  const TestTempFile script2("print('script2')", "lua");
  const TestTempFile script3("print('script3')", "lua");
  const TestTempFile extensionJSON("{}", "json");

  PermissionStorage storage(extensionJSON.filename);
  auto sprite = base::join_path(base::get_current_path(), "test.aseprite");
  storage.writeForUrl(script1.filename, Permission::SpriteWrite, sprite, true);

  EXPECT_OPT_EQ(storage.readForUrl(script1.filename, Permission::SpriteWrite, sprite), true);

  EXPECT_FALSE(
    storage.readForUrl(script1.filename, Permission::SpriteWrite, sprite + "2").has_value());

  storage.writeForUrl(script1.filename, Permission::SpriteWrite, sprite + "2", false);
  EXPECT_OPT_EQ(storage.readForUrl(script1.filename, Permission::SpriteWrite, sprite + "2"), false);

  // Generic Wildcard
  EXPECT_EQ(storage.readForUrl(script2.filename, Permission::SpriteWrite, sprite), std::nullopt);
  storage.writeForUrl(script2.filename, Permission::SpriteWrite, "*", true);
  EXPECT_OPT_EQ(storage.readForUrl(script2.filename, Permission::SpriteWrite, sprite), true);
  storage.writeForUrl(script2.filename, Permission::SpriteWrite, "*", false);
  EXPECT_OPT_EQ(storage.readForUrl(script2.filename, Permission::SpriteWrite, sprite), false);
  EXPECT_OPT_EQ(storage.readForUrl(script2.filename, Permission::SpriteWrite, sprite + "2"), false);

  // Directory wildcard
  {
    auto path = base::join_path(base::get_current_path(), "wildcardme");
    storage.writeForUrl(script3.filename, Permission::SpriteWrite, base::join_path(path, "*"), true);
    EXPECT_OPT_EQ(storage.readForUrl(script3.filename,
                                     Permission::SpriteWrite,
                                     base::join_path(path, "test.aseprite")),
                  true);
    EXPECT_OPT_EQ(storage.readForUrl(script3.filename,
                                     Permission::SpriteWrite,
                                     base::join_path(path, "test.png")),
                  true);

    storage.writeForUrl(script3.filename, Permission::SpriteWrite, "/another/path", true);
    EXPECT_OPT_EQ(storage.readForUrl(script3.filename, Permission::SpriteWrite, "/another/path"),
                  true);
  }

  // Extension wildcard
  {
    auto path = base::join_path(base::get_current_path(), "wildcardme2");
    storage.writeForUrl(script3.filename,
                        Permission::SpriteRead,
                        base::join_path(path, "*.png"),
                        true);
    EXPECT_FALSE(storage
                   .readForUrl(script3.filename,
                               Permission::SpriteRead,
                               base::join_path(path, "test.aseprite"))
                   .has_value());
    EXPECT_OPT_EQ(storage.readForUrl(script3.filename,
                                     Permission::SpriteRead,
                                     base::join_path(path, "test.png")),
                  true);
  }

  EXPECT_FALSE(storage.read(script3.filename, Permission::ClipboardWrite).has_value());
  storage.write(script3.filename, Permission::ClipboardWrite, true);
  EXPECT_OPT_EQ(storage.read(script3.filename, Permission::ClipboardWrite), true);
  EXPECT_FALSE(storage.read(script3.filename, Permission::ClipboardRead).has_value());
  storage.write(script3.filename, Permission::ClipboardRead, false);
  EXPECT_OPT_EQ(storage.read(script3.filename, Permission::ClipboardRead), false);
}

TEST(PermissionStorageJSON, FullAccess)
{
  const TestTempFile script1("print('script1')", "lua");
  const TestTempFile script2("print('script2')", "lua");
  const TestTempFile script3("print('script3')", "lua");
  const TestTempFile extensionJSON("{}", "json");
  PermissionStorage storage(extensionJSON.filename);

  EXPECT_FALSE(storage.readFullAccess(script1.filename));
  EXPECT_FALSE(storage.readFullAccess(script2.filename));

  storage.writeFullAccess(script1.filename, true);

  EXPECT_TRUE(storage.readFullAccess(script1.filename));
  EXPECT_OPT_EQ(storage.read(script1.filename, Permission::ClipboardWrite), true);
  EXPECT_OPT_EQ(storage.readForUrl(script1.filename, Permission::IORead, "/anything"), true);
  EXPECT_FALSE(storage.readFullAccess(script2.filename));

  storage.writeFullAccess(script1.filename, false);

  EXPECT_FALSE(storage.readFullAccess(script1.filename));
  EXPECT_FALSE(storage.readFullAccess(script2.filename));
}

TEST(PermissionStorage, Integrity)
{
  const TestTempFile script1("print('script1')", "lua");
  const TestTempFile extensionJSON("{}", "json");
  PermissionStorage storage(extensionJSON.filename);

  EXPECT_FALSE(storage.passesIntegrityCheck(script1.filename));

  storage.writeFullAccess(script1.filename, true);

  EXPECT_TRUE(storage.passesIntegrityCheck(script1.filename));

  {
    std::ofstream(script1.filename, std::ios_base::app) << "\n-- Modified";
  }

  EXPECT_FALSE(storage.passesIntegrityCheck(script1.filename));
}
#endif

int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
