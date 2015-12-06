/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>
          (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*******************************************************************************/

// This `test.cc` file is `#include`-d from `../test.cc`, and thus needs a header guard.

#ifndef CURRENT_TYPE_SYSTEM_SCHEMA_TEST_CC
#define CURRENT_TYPE_SYSTEM_SCHEMA_TEST_CC

#include "schema.h"

#include "../../Bricks/dflags/dflags.h"
#include "../../Bricks/strings/strings.h"
#include "../../Bricks/file/file.h"

#include "../../3rdparty/gtest/gtest-main-with-dflags.h"

DEFINE_bool(write_reflection_golden_files, false, "Set to true to [over]write the golden files.");

namespace schema_test {

CURRENT_ENUM(Enum, uint32_t){};

CURRENT_STRUCT(X) { CURRENT_FIELD(i, int32_t); };
CURRENT_STRUCT(Y) { CURRENT_FIELD(v, std::vector<X>); };
CURRENT_STRUCT(Z, Y) {
  CURRENT_FIELD(d, double);
  CURRENT_FIELD(v2, std::vector<std::vector<Enum>>);
};

CURRENT_STRUCT(A) { CURRENT_FIELD(i, uint32_t); };
CURRENT_STRUCT(B) {
  CURRENT_FIELD(x, X);
  CURRENT_FIELD(a, A);
};
CURRENT_STRUCT(C) { CURRENT_FIELD(b, Optional<B>); };
}

TEST(Schema, StructSchema) {
  using namespace schema_test;
  using current::reflection::SchemaInfo;
  using current::reflection::StructSchema;
  using current::reflection::Language;

  StructSchema struct_schema;
  {
    const SchemaInfo schema = Clone(struct_schema.GetSchemaInfo());
    EXPECT_TRUE(schema.order.empty());
    EXPECT_TRUE(schema.types.empty());
    EXPECT_EQ("", schema.Describe<Language::CPP>(false));
  }

  struct_schema.AddType<uint64_t>();
  struct_schema.AddType<double>();
  struct_schema.AddType<std::string>();

  {
    const SchemaInfo schema = Clone(struct_schema.GetSchemaInfo());
    EXPECT_TRUE(schema.order.empty());
    EXPECT_TRUE(schema.types.empty());
    EXPECT_EQ("", schema.Describe<Language::CPP>(false));
  }

  struct_schema.AddType<Z>();

  {
    const SchemaInfo schema = Clone(struct_schema.GetSchemaInfo());
    EXPECT_EQ(
        "struct X {\n"
        "  int32_t i;\n"
        "};\n"
        "struct Y {\n"
        "  std::vector<X> v;\n"
        "};\n"
        "struct Z : Y {\n"
        "  double d;\n"
        "  std::vector<std::vector<Enum>> v2;\n"
        "};\n",
        schema.Describe<Language::CPP>(false));
  }

  struct_schema.AddType<C>();

  {
    const SchemaInfo schema = Clone(struct_schema.GetSchemaInfo());
    EXPECT_EQ(
        "struct X {\n"
        "  int32_t i;\n"
        "};\n"
        "struct Y {\n"
        "  std::vector<X> v;\n"
        "};\n"
        "struct Z : Y {\n"
        "  double d;\n"
        "  std::vector<std::vector<Enum>> v2;\n"
        "};\n"
        "struct A {\n"
        "  uint32_t i;\n"
        "};\n"
        "struct B {\n"
        "  X x;\n"
        "  A a;\n"
        "};\n"
        "struct C {\n"
        "  Optional<B> b;\n"
        "};\n",
        schema.Describe<Language::CPP>(false));
  }
}

namespace schema_test {

CURRENT_STRUCT(SelfContainingA) { CURRENT_FIELD(v, std::vector<SelfContainingA>); };
CURRENT_STRUCT(SelfContainingB) { CURRENT_FIELD(v, std::vector<SelfContainingB>); };
CURRENT_STRUCT(SelfContainingC, SelfContainingA) {
  CURRENT_FIELD(v, std::vector<SelfContainingB>);
  CURRENT_FIELD(m, (std::map<std::string, SelfContainingC>));
};

}  // namespace schema_test

TEST(Schema, SelfContatiningStruct) {
  using namespace schema_test;
  using current::reflection::StructSchema;
  using current::reflection::SchemaInfo;
  using current::reflection::Language;

  StructSchema struct_schema;
  struct_schema.AddType<SelfContainingC>();

  const SchemaInfo schema = Clone(struct_schema.GetSchemaInfo());
  EXPECT_EQ(
      "struct SelfContainingA {\n"
      "  std::vector<SelfContainingA> v;\n"
      "};\n"
      "struct SelfContainingB {\n"
      "  std::vector<SelfContainingB> v;\n"
      "};\n"
      "struct SelfContainingC : SelfContainingA {\n"
      "  std::vector<SelfContainingB> v;\n"
      "  std::map<std::string, SelfContainingC> m;\n"
      "};\n",
      schema.Describe<Language::CPP>(false));
}

#include "../Serialization/json.h"

#define SMOKE_TEST_STRUCT_NAMESPACE smoke_test_struct_namespace
#include "smoke_test_struct.h"
#undef SMOKE_TEST_STRUCT_NAMESPACE

TEST(Schema, SmokeTestFullStruct) {
  using current::FileSystem;
  using current::reflection::StructSchema;
  using current::reflection::SchemaInfo;
  using current::reflection::Language;

  StructSchema struct_schema;
  struct_schema.AddType<smoke_test_struct_namespace::FullTest>();
  const SchemaInfo schema = Clone(struct_schema.GetSchemaInfo());

  if (false) {
    // This will not run, but should compile.
    // LCOV_EXCL_START
    {
      using namespace smoke_test_struct_namespace;
      A a;
      B b;
      X x;
      C c(x);
      FullTest original(std::move(c));

      if (false) {
        smoke_test_struct_namespace::FullTest clone_initialized(Clone(original));
        smoke_test_struct_namespace::FullTest* clone_copied;
        *clone_copied = Clone(original);
      }

      if (false) {
        smoke_test_struct_namespace::FullTest move_initialized(std::move(original));
        smoke_test_struct_namespace::FullTest* move_copied;
        *move_copied = std::move(original);
      }
    }
    // LCOV_EXCL_STOP
  }
  if (FLAGS_write_reflection_golden_files) {
    // LCOV_EXCL_START
    FileSystem::WriteStringToFile(schema.Describe<Language::CPP>(), "golden/smoke_test_struct.cc");
    FileSystem::WriteStringToFile(schema.Describe<Language::FSharp>(), "golden/smoke_test_struct.fsx");
    FileSystem::WriteStringToFile(schema.Describe<Language::JSON>(), "golden/smoke_test_struct.json");
    // `schema.Describe<Language::JSON>()` is equivalent to `JSON(struct_schema.GetSchemaInfo())`.
    // LCOV_EXCL_STOP
  }

  EXPECT_EQ(FileSystem::ReadFileAsString("golden/smoke_test_struct.cc"), schema.Describe<Language::CPP>());

  EXPECT_EQ(FileSystem::ReadFileAsString("golden/smoke_test_struct.fsx"), schema.Describe<Language::FSharp>());

  // JSON is a special case, as it might be pretty-printed. `JSON(ParseJSON<>(...))` does the trick.
  auto restored_schema = ParseJSON<SchemaInfo>(FileSystem::ReadFileAsString("golden/smoke_test_struct.json"));
  EXPECT_EQ(JSON(schema), JSON(struct_schema.GetSchemaInfo()));

  EXPECT_EQ(FileSystem::ReadFileAsString("golden/smoke_test_struct.cc"),
            restored_schema.Describe<Language::CPP>());

  EXPECT_EQ(FileSystem::ReadFileAsString("golden/smoke_test_struct.fsx"),
            restored_schema.Describe<Language::FSharp>());
}

#endif  // CURRENT_TYPE_SYSTEM_SCHEMA_TEST_CC