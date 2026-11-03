#include "harness.hpp"

int main() {
    int run = 0;
    for (const auto& test : kr8::test::registry()) {
        std::cout << "[ RUN  ] " << test.name << '\n';
        test.fn();
        ++run;
    }

    if (kr8::test::failures() == 0) {
        std::cout << "[  OK  ] " << run << " tests passed\n";
        return 0;
    }

    std::cerr << "[ FAIL ] " << kr8::test::failures()
              << " failure(s) across " << run << " tests\n";
    return 1;
}
