#pragma once

#include <cstdio>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "mr/core/error.hpp"

namespace mr_test {

struct TestCase {
  const char* name;
  void (*fn)();
};

inline std::vector<TestCase>& registry() {
  static std::vector<TestCase> r;
  return r;
}

struct Reg {
  Reg(const char* name, void (*fn)()) { registry().push_back(TestCase{name, fn}); }
};

class TestFailure : public std::runtime_error {
 public:
  explicit TestFailure(const std::string& msg) : std::runtime_error(msg) {}
};

inline int g_failures = 0;
inline int g_checked = 0;

inline void report_fail(const std::string& file, int line, const std::string& msg) {
  ++g_failures;
  std::fprintf(stderr, "  FAIL %s:%d  %s\n", file.c_str(), line, msg.c_str());
}

inline int run_all() {
  for (const auto& t : registry()) {
    const int before = g_failures;
    std::fprintf(stdout, "[ RUN     ] %s\n", t.name);
    try {
      t.fn();
    } catch (const TestFailure& e) {
      report_fail("<test>", -1, std::string("test threw: ") + e.what());
    } catch (const mr::Error& e) {
      report_fail("<test>", -1, std::string("unexpected mr::Error: ") + e.what());
    } catch (const std::exception& e) {
      report_fail("<test>", -1, std::string("unexpected exception: ") + e.what());
    } catch (...) {
      report_fail("<test>", -1, "unexpected unknown exception");
    }
    const bool ok = (g_failures == before);
    std::fprintf(stdout, "[ %s ] %s\n", ok ? "PASSED" : "FAILED", t.name);
  }
  std::fprintf(stdout, "--- checks: %d  failures: %d ---\n", g_checked, g_failures);
  return g_failures == 0 ? 0 : 1;
}

} // namespace mr_test

#define MR_TEST(name)   void mr_test_##name();   namespace { mr_test::Reg mr_reg_##name(#name, &mr_test_##name); }   void mr_test_##name()

#define MR_ASSERT(cond)   do {     ++mr_test::g_checked;     if (!(cond)) { mr_test::report_fail(__FILE__, __LINE__, #cond); mr_test::TestFailure("assert: " #cond); throw mr_test::TestFailure("assert failed: " #cond); }   } while (0)

#define MR_ASSERT_MSG(cond, msg)   do {     ++mr_test::g_checked;     if (!(cond)) { mr_test::report_fail(__FILE__, __LINE__, msg); throw mr_test::TestFailure(msg); }   } while (0)

#define MR_EXPECT_EQ(a, b)   do {     ++mr_test::g_checked;     if (!((a) == (b))) { std::ostringstream ss; ss << "expected equal: " #a " == " #b << "  (" << (a) << " vs " << (b) << ")"; mr_test::report_fail(__FILE__, __LINE__, ss.str()); throw mr_test::TestFailure(ss.str()); }   } while (0)

#define MR_EXPECT_TRUE(cond) MR_ASSERT(cond)

#define MR_EXPECT_THROW_CODE(expr, code)   do {     ++mr_test::g_checked;     bool thrown = false;     bool rightCode = false;     try { expr; } catch (const mr::Error& e) { thrown = true; rightCode = (e.code() == (code)); } catch (...) { thrown = true; }     if (!thrown || !rightCode) { mr_test::report_fail(__FILE__, __LINE__, "expected mr::Error with code " #code); throw mr_test::TestFailure("expected exception " #code); }   } while (0)
