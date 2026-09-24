// End-to-end tests that run the uncertainty_arithmetic binary and check its
// stdout, stderr and exit code.

#include <gtest/gtest.h>

#include <sys/wait.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <sstream>
#include <string>

namespace {

namespace fs = std::filesystem;

struct RunResult {
  int exit_code;
  std::string out;
  std::string err;
};

std::string read_file(const fs::path &path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

// Single-quotes `arg` for /bin/sh.
std::string shell_quote(const std::string &arg) {
  std::string quoted = "'";
  for (char c : arg) {
    quoted += c == '\'' ? std::string("'\\''") : std::string(1, c);
  }
  return quoted + "'";
}

class CliTest : public ::testing::Test {
protected:
  void SetUp() override {
    const auto *info = ::testing::UnitTest::GetInstance()->current_test_info();
    dir_ = fs::path(::testing::TempDir()) /
           (std::string("ua_cli_") + info->name());
    fs::create_directories(dir_);
  }

  void TearDown() override { fs::remove_all(dir_); }

  RunResult run(std::initializer_list<std::string> args) {
    std::string command = shell_quote(UA_BINARY);
    for (const auto &arg : args) {
      command += " " + shell_quote(arg);
    }
    fs::path out = dir_ / "stdout", err = dir_ / "stderr";
    command += " >" + shell_quote(out.string()) + " 2>" +
               shell_quote(err.string());
    int status = std::system(command.c_str());
    return {WIFEXITED(status) ? WEXITSTATUS(status) : -1, read_file(out),
            read_file(err)};
  }

  std::string write_csv(const std::string &contents) {
    fs::path path = dir_ / "data.csv";
    std::ofstream(path, std::ios::binary) << contents;
    return path.string();
  }

  fs::path dir_;
};

// ---------------------------------------------------------------------------
// Usage

TEST_F(CliTest, NoArgumentsPrintsUsageAndFails) {
  RunResult r = run({});
  EXPECT_EQ(r.exit_code, 1);
  EXPECT_NE(r.out.find("Usage:"), std::string::npos);
}

TEST_F(CliTest, HelpFlagsPrintUsageAndSucceed) {
  for (const char *flag : {"-h", "--help"}) {
    SCOPED_TRACE(flag);
    RunResult r = run({flag});
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_NE(r.out.find("Usage:"), std::string::npos);
    EXPECT_EQ(r.err, "");
  }
}

TEST_F(CliTest, TooManyArgumentsFails) {
  RunResult r = run({"1", "a.csv", "extra"});
  EXPECT_EQ(r.exit_code, 1);
  EXPECT_EQ(r.err, "Error: expected an equation and an optional CSV file\n");
}

TEST_F(CliTest, FlagWithoutEquationFails) {
  RunResult r = run({"--independent"});
  EXPECT_EQ(r.exit_code, 1);
  EXPECT_EQ(r.err, "Error: expected an equation and an optional CSV file\n");
}

// ---------------------------------------------------------------------------
// Single equation

TEST_F(CliTest, ExactResult) {
  RunResult r = run({"1 + 2 * 3"});
  EXPECT_EQ(r.exit_code, 0);
  EXPECT_EQ(r.out, "7\n");
  EXPECT_EQ(r.err, "");
}

TEST_F(CliTest, ResultWithUncertainty) {
  RunResult r = run({"1+/-0.1 + 2+/-0.2"});
  EXPECT_EQ(r.exit_code, 0);
  EXPECT_EQ(r.out, "3 +/- 0.3\n");
}

TEST_F(CliTest, WorstCaseIsTheDefault) {
  EXPECT_EQ(run({"10+/-0.3 * 10+/-0.4"}).out, "100 +/- 7\n");
}

TEST_F(CliTest, IndependentFlag) {
  EXPECT_EQ(run({"--independent", "10+/-0.3 * 10+/-0.4"}).out,
            "100 +/- 5\n");
}

TEST_F(CliTest, IndependentFlagAfterEquation) {
  EXPECT_EQ(run({"10+/-0.3 * 10+/-0.4", "--independent"}).out,
            "100 +/- 5\n");
}

// ---------------------------------------------------------------------------
// Errors

TEST_F(CliTest, ParseErrorShowsCaretAtPosition) {
  RunResult r = run({"1 + * 2"});
  EXPECT_EQ(r.exit_code, 1);
  EXPECT_EQ(r.out, "");
  EXPECT_EQ(r.err, "Error: expected a number, found '*'\n"
                   "  1 + * 2\n"
                   "      ^\n");
}

TEST_F(CliTest, ParseErrorAtEndOfInput) {
  RunResult r = run({"(1"});
  EXPECT_EQ(r.exit_code, 1);
  EXPECT_EQ(r.err, "Error: expected ')', found end of input\n"
                   "  (1\n"
                   "    ^\n");
}

TEST_F(CliTest, NumberOutOfRangeShowsCaret) {
  std::string equation = "2 * 1" + std::string(400, '0');
  RunResult r = run({equation});
  EXPECT_EQ(r.exit_code, 1);
  EXPECT_EQ(r.err, "Error: number out of range\n"
                   "  " + equation + "\n"
                   "      ^\n");
}

TEST_F(CliTest, DivisionByZero) {
  RunResult r = run({"1 / 0"});
  EXPECT_EQ(r.exit_code, 1);
  EXPECT_EQ(r.out, "");
  EXPECT_EQ(r.err, "Error: division by zero\n");
}

TEST_F(CliTest, VariableWithoutCsv) {
  RunResult r = run({"a * 2"});
  EXPECT_EQ(r.exit_code, 1);
  EXPECT_EQ(r.err, "Error: unknown variable 'a'\n");
}

// ---------------------------------------------------------------------------
// CSV input

TEST_F(CliTest, EvaluatesOncePerCsvRow) {
  std::string csv = write_csv("a,b\n10+/-0.1,2+/-0.1\n1,2\n3+/-0.3,1\n");
  RunResult r = run({"a * b", csv});
  EXPECT_EQ(r.exit_code, 0);
  EXPECT_EQ(r.out, "20 +/- 1.2\n2\n3 +/- 0.3\n");
  EXPECT_EQ(r.err, "");
}

TEST_F(CliTest, CsvWithIndependentFlag) {
  std::string csv = write_csv("a,b\n10+/-0.3,10+/-0.4\n");
  EXPECT_EQ(run({"--independent", "a * b", csv}).out, "100 +/- 5\n");
}

TEST_F(CliTest, CsvWithNoDataRowsPrintsNothing) {
  RunResult r = run({"a", write_csv("a\n")});
  EXPECT_EQ(r.exit_code, 0);
  EXPECT_EQ(r.out, "");
}

TEST_F(CliTest, MissingCsvFile) {
  std::string path = (dir_ / "missing.csv").string();
  RunResult r = run({"a", path});
  EXPECT_EQ(r.exit_code, 1);
  EXPECT_EQ(r.err, "Error: cannot open '" + path + "'\n");
}

TEST_F(CliTest, InvalidCsvReportsLocation) {
  std::string csv = write_csv("a\n1\noops\n");
  RunResult r = run({"a", csv});
  EXPECT_EQ(r.exit_code, 1);
  EXPECT_EQ(r.err,
            "Error: " + csv + ":3: column 'a': expected a number, found 'o'\n");
}

TEST_F(CliTest, EquationUsesVariableNotInCsv) {
  RunResult r = run({"a * c", write_csv("a,b\n1,2\n")});
  EXPECT_EQ(r.exit_code, 1);
  EXPECT_EQ(r.err, "Error: unknown variable 'c'\n");
}

TEST_F(CliTest, ParseErrorIsReportedBeforeReadingCsv) {
  RunResult r = run({"a +", (dir_ / "missing.csv").string()});
  EXPECT_EQ(r.exit_code, 1);
  EXPECT_NE(r.err.find("expected a number"), std::string::npos) << r.err;
}

} // namespace
