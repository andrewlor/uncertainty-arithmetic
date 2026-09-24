#include "csv.h"

#include <fstream>
#include <stdexcept>

#include "parser.h"
using namespace std;

namespace {

string trim(string_view s) {
  const char *ws = " \t\r";
  size_t start = s.find_first_not_of(ws);
  if (start == string_view::npos) {
    return "";
  }
  size_t end = s.find_last_not_of(ws);
  return string(s.substr(start, end - start + 1));
}

vector<string> split_line(const string &line) {
  vector<string> cells;
  size_t start = 0;
  while (true) {
    size_t comma = line.find(',', start);
    cells.push_back(trim(string_view(line).substr(start, comma - start)));
    if (comma == string::npos) {
      return cells;
    }
    start = comma + 1;
  }
}

} // namespace

vector<map<string, Measurement>> read_measurements_csv(const string &path) {
  ifstream file(path);
  if (!file) {
    throw runtime_error("cannot open '" + path + "'");
  }

  vector<string> header;
  vector<map<string, Measurement>> rows;
  string line;
  for (size_t line_no = 1; getline(file, line); ++line_no) {
    auto error = [&](const string &what) {
      return runtime_error(path + ":" + to_string(line_no) + ": " + what);
    };
    if (trim(line).empty()) {
      continue;
    }
    vector<string> cells = split_line(line);

    if (header.empty()) {
      for (const auto &name : cells) {
        if (name.empty()) {
          throw error("empty column name");
        }
      }
      header = std::move(cells);
      continue;
    }

    if (cells.size() != header.size()) {
      throw error("expected " + to_string(header.size()) + " columns, found " +
                  to_string(cells.size()));
    }
    map<string, Measurement> row;
    for (size_t i = 0; i < cells.size(); ++i) {
      try {
        row[header[i]] = parse_measurement(cells[i]);
      } catch (const ParseError &e) {
        throw error("column '" + header[i] + "': " + e.what());
      }
    }
    rows.push_back(std::move(row));
  }

  if (header.empty()) {
    throw runtime_error(path + ": missing header row");
  }
  return rows;
}
