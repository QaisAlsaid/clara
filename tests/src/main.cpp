#include <gtest/gtest.h>
#include <clara/clara.hpp>

using namespace clara::parse;

// Helper function to simulate argv input
std::vector<char*> make_argv(const std::vector<std::string>& args) {
    std::vector<char*> argv;
    for (const auto& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr); // Null-terminated
    return argv;
}

// Test fixture for parser setup
class ClaraParserTest : public ::testing::Test {
protected:
    parser p;

    void SetUp() override {
        // Reset parser state if needed
    }
};

// 1. Flags (e.g., "git -h")
TEST_F(ClaraParserTest, SingleFlag) {
    p.add_flag("h");
    auto argv = make_argv({"git", "-h"});
    auto result = p.parse(static_cast<int>(argv.size() - 1), argv.data());

    auto flag = result.root.get_flag("h");
    ASSERT_TRUE(flag.has_value()) << "Flag -h should be present";
}

// 2. Multi-Flag (e.g., "tar -xf")
TEST_F(ClaraParserTest, MultiFlag) {
    p.add_flag("x");
    p.add_flag("f");
    auto argv = make_argv({"tar", "-xf"});
    auto result = p.parse(static_cast<int>(argv.size() - 1), argv.data());

    ASSERT_TRUE(result.root.get_flag("x").has_value()) << "Flag -x should be present";
    ASSERT_TRUE(result.root.get_flag("f").has_value()) << "Flag -f should be present";
}

// 3. Aliases with Double Delimiter (e.g., "git --recurse-submodules")
TEST_F(ClaraParserTest, DoubleDelimiterAlias) {
    auto& sub = p.add_subcommand("submodule");
    auto& rec = sub.add_option("recursive");
    rec.set_alias("recurse-submodules");
    auto argv = make_argv({"git", "--recurse-submodules"});
    auto result = p.parse(static_cast<int>(argv.size() - 1), argv.data());

    auto subcmd = result.root.get_command("submodule");
    ASSERT_TRUE(subcmd.has_value()) << "Subcommand submodule should exist";
    auto opt = subcmd->get().get_option("recursive");
    ASSERT_TRUE(opt.has_value()) << "Option --recursive should be set via alias";
}

// 4. Aliases with Single Delimiter (e.g., "gcc -o file.c")
TEST_F(ClaraParserTest, SingleDelimiterAlias) {
    auto& opt = p.add_option("out").requires_value();
    opt.set_alias_options(option_builder::alias_options::single_delimiter);
    opt.set_alias("o");
    auto argv = make_argv({"gcc", "-o", "file.c"});
    auto result = p.parse(static_cast<int>(argv.size() - 1), argv.data());

    auto opt_result = result.root.get_option("out");
    ASSERT_TRUE(opt_result.has_value()) << "Option --out should be set via -o";
    EXPECT_EQ(opt_result->get().get_raw(), "file.c") << "Option value should be file.c";
}

// 5. Options without Values (e.g., "git --verbose")
TEST_F(ClaraParserTest, OptionNoValue) {
    p.add_option("verbose");
    auto argv = make_argv({"git", "--verbose"});
    auto result = p.parse(static_cast<int>(argv.size() - 1), argv.data());

    auto opt = result.root.get_option("verbose");
    ASSERT_TRUE(opt.has_value()) << "Option --verbose should be present";
    EXPECT_TRUE(opt->get().get_raw().empty()) << "Option should have no value";
}

// 6. Options with Single Value (e.g., "sometool --option value")
TEST_F(ClaraParserTest, OptionWithValue) {
    p.add_option("option").requires_value();
    auto argv = make_argv({"sometool", "--option", "value"});
    auto result = p.parse(static_cast<int>(argv.size() - 1), argv.data());

    auto opt = result.root.get_option("option");
    ASSERT_TRUE(opt.has_value()) << "Option --option should be present";
    EXPECT_EQ(opt->get().get_raw(), "value") << "Option value should be 'value'";
}

// 7. Options with Multiple Values (e.g., "sometool --option value1 value2")
/*
TEST_F(ClaraParserTest, OptionWithMultipleValues) {
    p.add_option("option").requires_value().allow_multiple();
    auto argv = make_argv({"sometool", "--option", "value1", "value2"});
    auto result = p.parse(static_cast<int>(argv.size() - 1), argv.data());

    auto opt = result.root.get_option("option");
    ASSERT_TRUE(opt.has_value()) << "Option --option should be present";
    EXPECT_EQ(opt->get().get_raw(), "value1 value2") << "Option should capture multiple values";
    auto vec = opt->get().get_vector<std::string>();
    ASSERT_TRUE(vec.has_value()) << "Should convert to vector";
    EXPECT_EQ(vec->size(), 2);
    EXPECT_EQ(vec->at(0), "value1");
    EXPECT_EQ(vec->at(1), "value2");
}
*/
// 8. Subcommands (e.g., "git submodule")
TEST_F(ClaraParserTest, Subcommand) {
    p.add_subcommand("submodule");
    auto argv = make_argv({"git", "submodule"});
    auto result = p.parse(static_cast<int>(argv.size() - 1), argv.data());

    auto subcmd = result.root.get_command("submodule");
    ASSERT_TRUE(subcmd.has_value()) << "Subcommand submodule should be present";
}

// 9. Subcommand with Value (e.g., "sometool subtool value")
TEST_F(ClaraParserTest, SubcommandWithValue) {
    p.add_subcommand("subtool").requires_value();
    auto argv = make_argv({"sometool", "subtool", "value"});
    auto result = p.parse(static_cast<int>(argv.size() - 1), argv.data());

    auto subcmd = result.root.get_command("subtool");
    ASSERT_TRUE(subcmd.has_value()) << "Subcommand subtool should be present";
    EXPECT_EQ(subcmd->get().get_raw(), "value") << "Subcommand should have value 'value'";
}

// 10. Optional Equals Sign (e.g., "sometool --option=value" or "sometool --option value")
TEST_F(ClaraParserTest, OptionalEqualsSign) {
    p.add_option("option").requires_value();
    auto argv1 = make_argv({"sometool", "--option=value"});
    auto argv2 = make_argv({"sometool", "--option", "value"});
    
    auto result1 = p.parse(static_cast<int>(argv1.size() - 1), argv1.data());
    auto result2 = p.parse(static_cast<int>(argv2.size() - 1), argv2.data());

    auto opt1 = result1.root.get_option("option");
    auto opt2 = result2.root.get_option("option");
    
    ASSERT_TRUE(opt1.has_value()) << "Option should be present with =";
    ASSERT_TRUE(opt2.has_value()) << "Option should be present without =";
    EXPECT_EQ(opt1->get().get_raw(), "value");
    EXPECT_EQ(opt2->get().get_raw(), "value");
}

// Edge Case: Unknown Flag
TEST_F(ClaraParserTest, UnknownFlag) {
    auto argv = make_argv({"tool", "-z"});
    auto result = p.parse(static_cast<int>(argv.size() - 1), argv.data());

    auto flag = result.root.get_flag("z");
    EXPECT_FALSE(flag.has_value()) << "Unknown flag -z should not be recognized";
}

// Edge Case: Missing Required Value
TEST_F(ClaraParserTest, MissingRequiredValue) {
    p.add_option("option").requires_value();
    auto argv = make_argv({"tool", "--option"});
    auto result = p.parse(static_cast<int>(argv.size() - 1), argv.data());

    auto opt = result.root.get_option("option");
    EXPECT_FALSE(opt.has_value()) << "Missing value for --option should not yield a valid option";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
