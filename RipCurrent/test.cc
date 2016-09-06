/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#include "ripcurrent.h"

#include "../Bricks/strings/join.h"
#include "../Bricks/strings/split.h"

#include "../3rdparty/gtest/gtest-main.h"

using current::strings::Join;

// `clang-format` messes up macro-defined class definitions, so disable it temporarily for this section. -- D.K.

// clang-format off

namespace ripcurrent_unittest {

CURRENT_STRUCT(Integer) {
  CURRENT_FIELD(value, int32_t);
  CURRENT_CONSTRUCTOR(Integer)(int32_t value = 0) : value(value) {}
};

// `RCFoo`: The emitter of events. RHS the integers passed to its constructor.
RIPCURRENT_NODE(RCFoo, ripcurrent::LHS<>, ripcurrent::RHS<Integer>) {
  static std::string UnitTestClassName() { return "RCFoo"; }
  RCFoo() {}  // LCOV_EXCL_LINE
  RCFoo(int a) { emit(Integer(a)); }
  RCFoo(int a, int b) {
    emit(Integer(a));
    emit(Integer(b));
  }
  RCFoo(int a, int b, int c) {
    emit(Integer(a));
    emit(Integer(b));
    emit(Integer(c));
  }
};
#define RCFoo(...) RIPCURRENT_MACRO(RCFoo, __VA_ARGS__)

// `RCBar`: The processor of events. Multiplies each integer by what was passed to its constructor.
RIPCURRENT_NODE(RCBar, ripcurrent::LHS<Integer>, ripcurrent::RHS<Integer>) {
  static std::string UnitTestClassName() { return "RCBar"; }
  int k;
  RCBar(int k = 1) : k(k) {}
  void f(Integer x) { emit(Integer(x.value * k)); }
};
#define RCBar(...) RIPCURRENT_MACRO(RCBar, __VA_ARGS__)

// `RCBaz`: The destination of events. Collects the output integers.
RIPCURRENT_NODE(RCBaz, ripcurrent::LHS<Integer>, ripcurrent::RHS<>) {
  static std::string UnitTestClassName() { return "RCBaz"; }
  std::vector<int>* ptr;
  RCBaz() : ptr(nullptr) {}  // LCOV_EXCL_LINE
  RCBaz(std::vector<int>& ref) : ptr(&ref) {}
  void f(Integer x) {
    CURRENT_ASSERT(ptr);
    ptr->push_back(x.value);
  }
};
#define RCBaz(...) RIPCURRENT_MACRO(RCBaz, __VA_ARGS__)

}  // namespace ripcurrent_unittest

// clang-format on

TEST(RipCurrent, SingleEdgeFlow) {
  using namespace ripcurrent_unittest;

  std::vector<int> result;

  using rcfoo_t = RIPCURRENT_UNDERLYING_TYPE(RCFoo());
  using rcbaz_t = RIPCURRENT_UNDERLYING_TYPE(RCBaz(std::ref(result)));

  static_assert(sizeof(is_same_or_compile_error<ripcurrent::LHS<>, typename rcfoo_t::input_t>), "");
  static_assert(sizeof(is_same_or_compile_error<ripcurrent::RHS<Integer>, typename rcfoo_t::output_t>), "");

  static_assert(sizeof(is_same_or_compile_error<ripcurrent::LHS<Integer>, typename rcbaz_t::input_t>), "");
  static_assert(sizeof(is_same_or_compile_error<ripcurrent::RHS<>, typename rcbaz_t::output_t>), "");

  static_assert(sizeof(is_same_or_compile_error<
                    decltype(RCFoo(1, 2, 3)),
                    current::ripcurrent::UserClass<ripcurrent::LHS<>, ripcurrent::RHS<Integer>, rcfoo_t>>),
                "");

  static_assert(sizeof(is_same_or_compile_error<
                    decltype(RCBaz(std::ref(result))),
                    current::ripcurrent::UserClass<ripcurrent::LHS<Integer>, ripcurrent::RHS<>, rcbaz_t>>),
                "");

  (RCFoo(1, 2, 3) | RCBaz(std::ref(result))).RipCurrent().Sync();
  EXPECT_EQ("1,2,3", Join(result, ','));
}

TEST(RipCurrent, SingleChainFlow) {
  using namespace ripcurrent_unittest;

  std::vector<int> result;
  (RCFoo(1, 2, 3) | RCBar(10) | RCBar(10) | RCBaz(std::ref(result))).RipCurrent().Sync();
  EXPECT_EQ("100,200,300", Join(result, ','));
}

TEST(RipCurrent, DeclarationDoesNotRunConstructors) {
  using namespace ripcurrent_unittest;

  std::vector<int> result;

  RipCurrentLHS<ripcurrent::RHS<Integer>> foo = RCFoo(42);
  EXPECT_EQ("RCFoo(42) | ...", foo.Describe());

  RipCurrentRHS<ripcurrent::LHS<Integer>> baz = RCBaz(std::ref(result));
  EXPECT_EQ("... | RCBaz(std::ref(result))", baz.Describe());

  RipCurrent foo_baz = foo | baz;
  EXPECT_EQ("RCFoo(42) | RCBaz(std::ref(result))", foo_baz.Describe());

  EXPECT_EQ("", Join(result, ','));
  foo_baz.RipCurrent().Sync();
  EXPECT_EQ("42", Join(result, ','));
}

TEST(RipCurrent, OrderDoesNotMatter) {
  using namespace ripcurrent_unittest;

  std::vector<int> result;

  result.clear();
  (RCFoo(1) | RCBar(10) | RCBaz(std::ref(result))).RipCurrent().Sync();
  EXPECT_EQ("10", Join(result, ','));

  const auto a = RCFoo(1);
  const auto b = RCBar(2);
  const auto c = RCBaz(std::ref(result));

  result.clear();
  (a | b | b | c).RipCurrent().Sync();
  EXPECT_EQ("4", Join(result, ','));

  result.clear();
  ((a | b) | (b | c)).RipCurrent().Sync();
  EXPECT_EQ("4", Join(result, ','));

  result.clear();
  ((a | (b | b)) | c).RipCurrent().Sync();
  EXPECT_EQ("4", Join(result, ','));

  result.clear();
  (a | ((b | b) | c)).RipCurrent().Sync();
  EXPECT_EQ("4", Join(result, ','));
}

TEST(RipCurrent, BuildingBlocksCanBeReUsed) {
  using namespace ripcurrent_unittest;

  std::vector<int> result1;
  std::vector<int> result2;

  RipCurrentLHS<ripcurrent::RHS<Integer>> foo1 = RCFoo(1);
  RipCurrentLHS<ripcurrent::RHS<Integer>> foo2 = RCFoo(2);
  RipCurrentVIA<ripcurrent::LHS<Integer>, ripcurrent::RHS<Integer>> bar = RCBar(10);
  RipCurrentRHS<ripcurrent::LHS<Integer>> baz1 = RCBaz(std::ref(result1));
  RipCurrentRHS<ripcurrent::LHS<Integer>> baz2 = RCBaz(std::ref(result2));

  (foo1 | bar | baz1).RipCurrent().Sync();
  (foo2 | bar | baz2).RipCurrent().Sync();
  EXPECT_EQ("10", Join(result1, ','));
  EXPECT_EQ("20", Join(result2, ','));

  result1.clear();
  result2.clear();
  ((foo1 | bar) | baz1).RipCurrent().Sync();
  ((foo2 | bar) | baz2).RipCurrent().Sync();
  EXPECT_EQ("10", Join(result1, ','));
  EXPECT_EQ("20", Join(result2, ','));
}

TEST(RipCurrent, SynopsisAndDecorators) {
  using namespace ripcurrent_unittest;

  EXPECT_EQ("RCFoo() | ...", (RCFoo()).Describe());
  EXPECT_EQ("... | RCBar() | ...", (RCBar()).Describe());
  EXPECT_EQ("... | RCBaz()", (RCBaz()).Describe());

  EXPECT_EQ("RCFoo() | RCBaz()", (RCFoo() | RCBaz()).Describe());
  EXPECT_EQ("RCFoo() | RCBar() | RCBar() | RCBar() | RCBaz()",
            (RCFoo() | RCBar() | RCBar() | RCBar() | RCBaz()).Describe());

  EXPECT_EQ("RCFoo() | RCBar() | ...", (RCFoo() | RCBar()).Describe());
  EXPECT_EQ("RCFoo() | RCBar() | RCBar() | RCBar() | ...", (RCFoo() | RCBar() | RCBar() | RCBar()).Describe());

  EXPECT_EQ("... | RCBar() | RCBaz()", (RCBar() | RCBaz()).Describe());
  EXPECT_EQ("... | RCBar() | RCBar() | RCBar() | RCBaz()", (RCBar() | RCBar() | RCBar() | RCBaz()).Describe());

  EXPECT_EQ("... | RCBar() | RCBar() | RCBar() | ...", (RCBar() | RCBar() | RCBar()).Describe());

  const int blah = 5;
  EXPECT_EQ("RCFoo(1) | RCBar(2) | RCBar(3 + 4) | RCBar(blah) | RCBaz()",
            (RCFoo(1) | RCBar(2) | RCBar(3 + 4) | RCBar(blah) | RCBaz()).Describe());

  const int x = 1;
  const int y = 1;
  const int z = 1;
  EXPECT_EQ("RCFoo(x, y, z) | RCBaz()", (RCFoo(x, y, z) | RCBaz()).Describe());
}

TEST(RipCurrent, TypeIntrospection) {
  using namespace ripcurrent_unittest;

  EXPECT_EQ("RCFoo() => { Integer } | ...", (RCFoo()).DescribeWithTypes());
  EXPECT_EQ("... | { Integer } => RCBar() => { Integer } | ...", (RCBar()).DescribeWithTypes());
  EXPECT_EQ("... | { Integer } => RCBaz()", (RCBaz()).DescribeWithTypes());
}

TEST(RipCurrent, TypeSystemGuarantees) {
  using namespace ripcurrent_unittest;

  EXPECT_EQ("RCFoo", RCFoo::UnitTestClassName());
  static_assert(std::is_same<RCFoo::input_t, ripcurrent::LHS<>>::value, "");
  static_assert(std::is_same<RCFoo::output_t, ripcurrent::RHS<Integer>>::value, "");

  EXPECT_EQ("RCFoo", RIPCURRENT_UNDERLYING_TYPE(RCFoo())::UnitTestClassName());

  const auto foo = RCFoo();
  EXPECT_EQ("RCFoo", RIPCURRENT_UNDERLYING_TYPE(foo)::UnitTestClassName());

  EXPECT_EQ("RCBar", RCBar::UnitTestClassName());
  static_assert(std::is_same<RCBar::input_t, ripcurrent::LHS<Integer>>::value, "");
  static_assert(std::is_same<RCBar::output_t, ripcurrent::RHS<Integer>>::value, "");

  EXPECT_EQ("RCBar", RIPCURRENT_UNDERLYING_TYPE(RCBar())::UnitTestClassName());

  const auto bar = RCBar();
  EXPECT_EQ("RCBar", RIPCURRENT_UNDERLYING_TYPE(bar)::UnitTestClassName());

  EXPECT_EQ("RCBaz", RCBaz::UnitTestClassName());
  static_assert(std::is_same<RCBaz::input_t, ripcurrent::LHS<Integer>>::value, "");
  static_assert(std::is_same<RCBaz::output_t, ripcurrent::RHS<>>::value, "");

  EXPECT_EQ("RCBaz", RIPCURRENT_UNDERLYING_TYPE(RCBaz())::UnitTestClassName());

  const auto baz = RCBaz();
  EXPECT_EQ("RCBaz", RIPCURRENT_UNDERLYING_TYPE(baz)::UnitTestClassName());

  const auto foo_bar = foo | bar;
  const auto bar_baz = bar | baz;
  const auto foo_baz = foo | baz;
  const auto foo_bar_baz_1 = (foo | bar) | baz;
  const auto foo_bar_baz_2 = foo | (bar | baz);
  const auto foo_bar_bar_baz_1 = (foo | bar) | (bar | baz);
  const auto foo_bar_bar_baz_2 = foo | (bar | bar) | baz;
  const auto foo_bar_bar_baz_3 = ((foo | bar) | bar) | baz;
  const auto foo_bar_bar_baz_4 = foo | (bar | (bar | baz));

  static_assert(std::is_same<RIPCURRENT_UNDERLYING_TYPE(foo_bar)::input_t, ripcurrent::LHS<>>::value, "");
  static_assert(std::is_same<RIPCURRENT_UNDERLYING_TYPE(foo_bar)::output_t, ripcurrent::RHS<Integer>>::value,
                "");

  static_assert(std::is_same<RIPCURRENT_UNDERLYING_TYPE(bar_baz)::input_t, ripcurrent::LHS<Integer>>::value,
                "");
  static_assert(std::is_same<RIPCURRENT_UNDERLYING_TYPE(bar_baz)::output_t, ripcurrent::RHS<>>::value, "");

  static_assert(std::is_same<RIPCURRENT_UNDERLYING_TYPE(foo_baz)::input_t, ripcurrent::LHS<>>::value, "");
  static_assert(std::is_same<RIPCURRENT_UNDERLYING_TYPE(foo_baz)::output_t, ripcurrent::RHS<>>::value, "");

  static_assert(std::is_same<RIPCURRENT_UNDERLYING_TYPE(foo_bar_baz_1)::input_t, ripcurrent::LHS<>>::value, "");
  static_assert(std::is_same<RIPCURRENT_UNDERLYING_TYPE(foo_bar_baz_1)::output_t, ripcurrent::RHS<>>::value,
                "");

  static_assert(std::is_same<RIPCURRENT_UNDERLYING_TYPE(foo_bar_baz_2)::input_t, ripcurrent::LHS<>>::value, "");
  static_assert(std::is_same<RIPCURRENT_UNDERLYING_TYPE(foo_bar_baz_2)::output_t, ripcurrent::RHS<>>::value,
                "");
  static_assert(std::is_same<RIPCURRENT_UNDERLYING_TYPE(foo_bar_bar_baz_1)::input_t, ripcurrent::LHS<>>::value,
                "");
  static_assert(std::is_same<RIPCURRENT_UNDERLYING_TYPE(foo_bar_bar_baz_1)::output_t, ripcurrent::RHS<>>::value,
                "");

  static_assert(std::is_same<RIPCURRENT_UNDERLYING_TYPE(foo_bar_bar_baz_2)::input_t, ripcurrent::LHS<>>::value,
                "");
  static_assert(std::is_same<RIPCURRENT_UNDERLYING_TYPE(foo_bar_bar_baz_2)::output_t, ripcurrent::RHS<>>::value,
                "");
  static_assert(std::is_same<RIPCURRENT_UNDERLYING_TYPE(foo_bar_bar_baz_3)::input_t, ripcurrent::LHS<>>::value,
                "");
  static_assert(std::is_same<RIPCURRENT_UNDERLYING_TYPE(foo_bar_bar_baz_3)::output_t, ripcurrent::RHS<>>::value,
                "");

  static_assert(std::is_same<RIPCURRENT_UNDERLYING_TYPE(foo_bar_bar_baz_4)::input_t, ripcurrent::LHS<>>::value,
                "");
  static_assert(std::is_same<RIPCURRENT_UNDERLYING_TYPE(foo_bar_bar_baz_4)::output_t, ripcurrent::RHS<>>::value,
                "");

  foo_bar.Dismiss();
  bar_baz.Dismiss();
  foo_baz.Dismiss();
  foo_bar_baz_1.Dismiss();
  foo_bar_baz_2.Dismiss();
  foo_bar_bar_baz_1.Dismiss();
  foo_bar_bar_baz_2.Dismiss();
  foo_bar_bar_baz_3.Dismiss();
  foo_bar_bar_baz_4.Dismiss();
}

static std::string ExpectHasNAndReturnFirstTwoLines(size_t n, const std::string& s) {
  const std::vector<std::string> lines = current::strings::Split<current::strings::ByLines>(s);
  EXPECT_EQ(n, lines.size());
  return current::strings::Join(
      std::vector<std::string>(
          lines.begin(), lines.begin() + std::min(static_cast<size_t>(lines.size()), static_cast<size_t>(2))),
      '\n');
}

TEST(RipCurrent, NotLeftHanging) {
  using namespace ripcurrent_unittest;

  std::string captured_error_message;
  const auto scope =
      current::Singleton<current::ripcurrent::RipCurrentMockableErrorHandler>().ScopedInjectHandler(
          [&captured_error_message](const std::string& error_message) {
            captured_error_message = error_message;
          });

  EXPECT_EQ("", captured_error_message);
  RCFoo();
  EXPECT_EQ(
      "RipCurrent building block leaked.\n"
      "RCFoo() | ...",
      ExpectHasNAndReturnFirstTwoLines(4, captured_error_message));
  RCBar();
  EXPECT_EQ(
      "RipCurrent building block leaked.\n"
      "... | RCBar() | ...",
      ExpectHasNAndReturnFirstTwoLines(4, captured_error_message));
  RCBaz();
  EXPECT_EQ(
      "RipCurrent building block leaked.\n"
      "... | RCBaz()",
      ExpectHasNAndReturnFirstTwoLines(4, captured_error_message));
  RCFoo(1) | RCBar(2);
  EXPECT_EQ(
      "RipCurrent building block leaked.\n"
      "RCFoo(1) | RCBar(2) | ...",
      ExpectHasNAndReturnFirstTwoLines(5, captured_error_message));
  RCBar(3) | RCBaz();
  EXPECT_EQ(
      "RipCurrent building block leaked.\n"
      "... | RCBar(3) | RCBaz()",
      ExpectHasNAndReturnFirstTwoLines(5, captured_error_message));
  std::vector<int> result;
  RCFoo(42) | RCBar(100) | RCBaz(std::ref(result));
  EXPECT_EQ(
      "RipCurrent building block leaked.\n"
      "RCFoo(42) | RCBar(100) | RCBaz(std::ref(result))",
      ExpectHasNAndReturnFirstTwoLines(6, captured_error_message));

  { const auto tmp = RCFoo() | RCBar(1) | RCBaz(std::ref(result)); }
  EXPECT_EQ(
      "RipCurrent building block leaked.\n"
      "RCFoo() | RCBar(1) | RCBaz(std::ref(result))",
      ExpectHasNAndReturnFirstTwoLines(6, captured_error_message));

  captured_error_message = "NO ERROR";
  {
    const auto tmp = RCFoo() | RCBar(2) | RCBaz(std::ref(result));
    tmp.Describe();
  }
  EXPECT_EQ("NO ERROR", captured_error_message);

  captured_error_message = "NO ERROR ONCE AGAIN";
  {
    const auto tmp = RCFoo() | RCBar(3) | RCBaz(std::ref(result));
    tmp.Dismiss();
  }
  EXPECT_EQ("NO ERROR ONCE AGAIN", captured_error_message);
}

// clang-format off

namespace ripcurrent_unittest {

CURRENT_STRUCT(String) {
  CURRENT_FIELD(value, std::string);
  CURRENT_CONSTRUCTOR(String)(const std::string& value = "") : value(value) {}
};

// TODO(dkorolev): Parentheses fix.
using LHS_Integer_String = ripcurrent::LHS<Integer, String>;
using RHS_Integer_String = ripcurrent::RHS<Integer, String>;

RIPCURRENT_NODE(RCFoo2, ripcurrent::LHS<>, RHS_Integer_String) {
  RCFoo2() {
    emit(String("Answer"));
    emit(Integer(42));
  }
};
#define RCFoo2(...) RIPCURRENT_MACRO(RCFoo2, __VA_ARGS__)

RIPCURRENT_NODE(RCBar2, LHS_Integer_String, RHS_Integer_String) {
  void f(Integer x) {
    emit(Integer(x.value * 1001001));
  }
  void f(String x) {
    emit(String("Yo? " + x.value + " Yo!"));
  }
};
#define RCBar2(...) RIPCURRENT_MACRO(RCBar2, __VA_ARGS__)

RIPCURRENT_NODE(RCBaz2, LHS_Integer_String, ripcurrent::RHS<>) {
  std::vector<std::string>* ptr;
  RCBaz2() : ptr(nullptr) {}  // LCOV_EXCL_LINE
  RCBaz2(std::vector<std::string>& ref) : ptr(&ref) {}
  void f(Integer x) {
    CURRENT_ASSERT(ptr);
    ptr->push_back(current::ToString(x.value));
  }
  void f(String x) {
    CURRENT_ASSERT(ptr);
    ptr->push_back("'" + x.value + "'");
  }
};
#define RCBaz2(...) RIPCURRENT_MACRO(RCBaz2, __VA_ARGS__)

}  // namespace ripcurrent_unittest

// clang-format on

TEST(RipCurrent, CustomTypesIntrospection) {
  using namespace ripcurrent_unittest;

  EXPECT_EQ("RCFoo2() => { Integer, String } | ...", (RCFoo2()).DescribeWithTypes());
  EXPECT_EQ("... | { Integer, String } => RCBaz2()", (RCBaz2()).DescribeWithTypes());
}

TEST(RipCurrent, CustomTypesFlow) {
  using namespace ripcurrent_unittest;

  {
    std::vector<std::string> result;
    (RCFoo2() | RCBaz2(std::ref(result))).RipCurrent().Sync();
    EXPECT_EQ("'Answer',42", Join(result, ','));
  }

#if 0
// TODO(dkorolev): Make this work.
  {
    std::vector<std::string> result;
    (RCFoo2() | RCBar2() | RCBaz2(std::ref(result))).RipCurrent().Sync();
    EXPECT_EQ("'Yo? Answer Yo!',424242", Join(result, ','));
  }
#endif
}
