#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include "csv.h"

namespace {

namespace fs = std::filesystem;

class CsvTest : public ::testing::Test {
protected:
  void SetUp() override {
    const auto *info = ::testing::UnitTest::GetInstance()->current_test_info();
    dir_ = fs::path(::testing::TempDir()) /
           (std::string("ua_csv_") + info->name());
    fs::create_directories(dir_);
  }

  void TearDown() override { fs::remove_all(dir_); }

  // Writes `contents` verbatim (no newline translation) and returns the path.
  std::string write(const std::string &contents,
                    const std::string &name = "data.csv") {
    fs::path path = dir_ / name;
    std::ofstream(path, std::ios::binary) << contents;
    return path.string();
  }

  // Asserts read_measurements_csv(path) throws runtime_error with `message`.
  void expect_error(const std::string &path, const std::string &message) {
    try {
      read_measurements_csv(path);
      ADD_FAILURE() << "expected runtime_error";
    } catch (const std::runtime_error &e) {
      EXPECT_EQ(e.what(), message);
    }
  }

  fs::path dir_;
};

TEST_F(CsvTest, ReadsRows) {
  auto rows = read_measurements_csv(write("a,b\n10+/-0.1,0.25+/-0.001\n1,2\n"));
  ASSERT_EQ(rows.size(), 2u);
  EXPECT_DOUBLE_EQ(rows[0].at("a").value, 10);
  EXPECT_DOUBLE_EQ(rows[0].at("a").uncertainty, 0.1);
  EXPECT_DOUBLE_EQ(rows[0].at("b").value, 0.25);
  EXPECT_DOUBLE_EQ(rows[0].at("b").uncertainty, 0.001);
  EXPECT_DOUBLE_EQ(rows[1].at("a").value, 1);
  EXPECT_EQ(rows[1].at("a").uncertainty, 0.0);
  EXPECT_DOUBLE_EQ(rows[1].at("b").value, 2);
}

TEST_F(CsvTest, EachRowHasExactlyTheHeaderColumns) {
  auto rows = read_measurements_csv(write("x,y,z\n1,2,3\n"));
  ASSERT_EQ(rows.size(), 1u);
  EXPECT_EQ(rows[0].size(), 3u);
  EXPECT_TRUE(rows[0].contains("x"));
  EXPECT_TRUE(rows[0].contains("y"));
  EXPECT_TRUE(rows[0].contains("z"));
}

TEST_F(CsvTest, SingleColumn) {
  auto rows = read_measurements_csv(write("a\n1\n2\n3\n"));
  ASSERT_EQ(rows.size(), 3u);
  EXPECT_DOUBLE_EQ(rows[2].at("a").value, 3);
}

TEST_F(CsvTest, NegativeValues) {
  auto rows = read_measurements_csv(write("a\n-1.5+/-0.5\n"));
  ASSERT_EQ(rows.size(), 1u);
  EXPECT_DOUBLE_EQ(rows[0].at("a").value, -1.5);
  EXPECT_DOUBLE_EQ(rows[0].at("a").uncertainty, 0.5);
}

TEST_F(CsvTest, TrimsWhitespaceAroundCells) {
  auto rows = read_measurements_csv(write(" a \t, b\n 1 +/- 0.1 ,\t2 \n"));
  ASSERT_EQ(rows.size(), 1u);
  EXPECT_DOUBLE_EQ(rows[0].at("a").value, 1);
  EXPECT_DOUBLE_EQ(rows[0].at("a").uncertainty, 0.1);
  EXPECT_DOUBLE_EQ(rows[0].at("b").value, 2);
}

TEST_F(CsvTest, HandlesCrlfLineEndings) {
  auto rows = read_measurements_csv(write("a,b\r\n1,2\r\n3,4\r\n"));
  ASSERT_EQ(rows.size(), 2u);
  EXPECT_TRUE(rows[0].contains("b"));
  EXPECT_DOUBLE_EQ(rows[1].at("b").value, 4);
}

TEST_F(CsvTest, NoTrailingNewline) {
  auto rows = read_measurements_csv(write("a\n1\n2"));
  ASSERT_EQ(rows.size(), 2u);
  EXPECT_DOUBLE_EQ(rows[1].at("a").value, 2);
}

TEST_F(CsvTest, SkipsBlankLines) {
  auto rows = read_measurements_csv(write("\n  \na,b\n\n1,2\n \t\n3,4\n\n"));
  ASSERT_EQ(rows.size(), 2u);
  EXPECT_DOUBLE_EQ(rows[0].at("a").value, 1);
  EXPECT_DOUBLE_EQ(rows[1].at("a").value, 3);
}

TEST_F(CsvTest, HeaderOnlyYieldsNoRows) {
  EXPECT_TRUE(read_measurements_csv(write("a,b\n")).empty());
}

// ---------------------------------------------------------------------------
// Errors

TEST_F(CsvTest, MissingFile) {
  std::string path = (dir_ / "does_not_exist.csv").string();
  expect_error(path, "cannot open '" + path + "'");
}

TEST_F(CsvTest, EmptyFileIsMissingHeader) {
  std::string path = write("");
  expect_error(path, path + ": missing header row");
}

TEST_F(CsvTest, BlankFileIsMissingHeader) {
  std::string path = write("\n  \n\n");
  expect_error(path, path + ": missing header row");
}

TEST_F(CsvTest, EmptyColumnName) {
  std::string path = write("a,,b\n1,2,3\n");
  expect_error(path, path + ":1: empty column name");
}

TEST_F(CsvTest, TrailingCommaInHeader) {
  std::string path = write("a,b,\n1,2,3\n");
  expect_error(path, path + ":1: empty column name");
}

TEST_F(CsvTest, TooFewColumns) {
  std::string path = write("a,b\n1\n");
  expect_error(path, path + ":2: expected 2 columns, found 1");
}

TEST_F(CsvTest, TooManyColumns) {
  std::string path = write("a,b\n1,2,3\n");
  expect_error(path, path + ":2: expected 2 columns, found 3");
}

TEST_F(CsvTest, InvalidMeasurement) {
  std::string path = write("a,b\n1,x\n");
  expect_error(path,
               path + ":2: column 'b': expected a number, found 'x'");
}

TEST_F(CsvTest, EmptyCell) {
  std::string path = write("a,b\n1,\n");
  expect_error(path, path +
                         ":2: column 'b': expected a number, found end of "
                         "input");
}

TEST_F(CsvTest, ExpressionInCellIsRejected) {
  std::string path = write("a\n1+2\n");
  expect_error(path,
               path + ":2: column 'a': expected end of measurement, found '+'");
}

TEST_F(CsvTest, ErrorLineNumbersCountBlankLines) {
  std::string path = write("a\n\n1\n\nbad\n");
  expect_error(path,
               path + ":5: column 'a': expected a number, found 'b'");
}

TEST_F(CsvTest, OutOfRangeNumberReportsLocation) {
  std::string path = write("a\n1" + std::string(400, '0') + "\n");
  expect_error(path, path + ":2: column 'a': number out of range");
}

TEST_F(CsvTest, TinyNumberRoundsToZero) {
  auto rows =
      read_measurements_csv(write("a\n0." + std::string(400, '0') + "1\n"));
  ASSERT_EQ(rows.size(), 1u);
  EXPECT_EQ(rows[0].at("a").value, 0.0);
}

} // namespace
