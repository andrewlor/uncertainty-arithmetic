#pragma once

#include "expr.h"
#include <map>
#include <string>

using namespace std;

class Evaluator {
private:
  bool is_independent_;
  Measurement apply(char op, const Measurement &a, const Measurement &b);

public:
  explicit Evaluator(bool is_independent) : is_independent_(is_independent) {};
  // Throws invalid_argument if expr references a variable missing from vars.
  Measurement evaluate(const Expr &expr,
                       const map<string, Measurement> &vars = {});
  string format_result(const Measurement &num);
};
