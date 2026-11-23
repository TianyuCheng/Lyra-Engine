#define DOCTEST_CONFIG_IMPLEMENT
#define DOCTEST_CONFIG_NO_EXCEPTIONS
#include <doctest/doctest.h>
#include <iostream>
#include "./common/context.h"

struct StreamListener : public doctest::IReporter
{
    using doctest::IReporter::IReporter;

    StreamListener(const doctest::ContextOptions&) {}

    void test_case_start(const doctest::TestCaseData& data) override
    {
        std::cout << ">>> TEST_START: " << data.m_name << std::endl;
    }

    void test_case_end(const doctest::CurrentTestCaseStats& stats) override
    {
        std::cout << ">>> TEST_END: " << (stats.testCaseSuccess ? "PASS" : "FAIL") << std::endl;
    }

    void report_query(const doctest::QueryData&) override {}
    void test_run_start() override {}
    void test_run_end(const doctest::TestRunStats&) override {}
    void test_case_reenter(const doctest::TestCaseData&) override {}
    void test_case_exception(const doctest::TestCaseException&) override {}
    void subcase_start(const doctest::SubcaseSignature&) override {}
    void subcase_end() override {}
    void log_assert(const doctest::AssertData&) override {}
    void log_message(const doctest::MessageData&) override {}
    void test_case_skipped(const doctest::TestCaseData&) override {}
};

REGISTER_LISTENER("stream", 1, StreamListener);

int main(int argc, char** argv)
{
    doctest::Context context(argc, argv);
    int              res = context.run();
    TestContext::get().release();
    return res;
}
