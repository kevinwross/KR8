#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace kr8::test {

struct Case {
    const char* name;
    void (*fn)();
};

inline std::vector<Case>& registry() {
    static std::vector<Case> cases;
    return cases;
}

inline int& failures() {
    static int count = 0;
    return count;
}

struct Registrar {
    Registrar(const char* name, void (*fn)()) {
        registry().push_back(Case{name, fn});
    }
};

inline void report_failure(const char* file, int line,
                           const std::string& message) {
    ++failures();
    std::cerr << file << ':' << line << ": FAIL: " << message << '\n';
}

inline void check(bool condition, const char* expression, const char* file,
                  int line) {
    if (!condition) {
        report_failure(file, line, std::string("CHECK(") + expression + ')');
    }
}

template <typename A, typename B>
void check_equal(const A& actual, const B& expected, const char* actual_text,
                 const char* expected_text, const char* file, int line) {
    if (actual == expected) {
        return;
    }
    std::ostringstream message;
    message << "CHECK_EQ(" << actual_text << ", " << expected_text
            << ") failed";
    report_failure(file, line, message.str());
}

}  // namespace kr8::test

#define TEST(name)                                                       \
    static void name();                                                  \
    static ::kr8::test::Registrar name##_registrar(#name, name);         \
    static void name()

#define CHECK(condition) \
    ::kr8::test::check((condition), #condition, __FILE__, __LINE__)

#define CHECK_EQ(actual, expected)                                    \
    ::kr8::test::check_equal((actual), (expected), #actual, #expected, \
                             __FILE__, __LINE__)
