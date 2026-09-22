#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#define DOCTEST_CONFIG_NO_EXCEPTIONS
#include <doctest/doctest.h>
#include <iostream>

// Custom reporter
class PreRunReporter : public doctest::IReporter
{
public:
    using doctest::IReporter::IReporter; // inherit constructor

    PreRunReporter(const doctest::ContextOptions&) {}

    void test_case_start(const doctest::TestCaseData& data) override
    {
        std::cout << "[ RUN      ] " << data.m_name << std::endl;
    }

    void test_case_end(const doctest::CurrentTestCaseStats&) override
    {
        // nothing
    }

    // Stub the remaining virtual methods with empty implementations
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

REGISTER_REPORTER("pre", 1, PreRunReporter);
