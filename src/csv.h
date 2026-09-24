#pragma once

#include <map>
#include <string>
#include <vector>

#include "expr.h"

// Reads a CSV file whose header row names the variables and whose remaining
// rows hold one measurement per column, e.g.
//
//   a,b
//   10+/-0.1,0.25+/-0.001
//
// Returns one variable binding per data row; blank lines are skipped.
// Throws runtime_error, prefixed with "path:line: ", on invalid input.
std::vector<std::map<std::string, Measurement>>
read_measurements_csv(const std::string &path);
