#pragma once

#include "expr.h"
#include <string>

using namespace std;

class Evaluator {
private:
  bool is_independent_;
  Measurement apply(char op, const Measurement &a, const Measurement &b);

public:
  explicit Evaluator(bool is_independent) : is_independent_(is_independent) {};
  Measurement evaluate(const Expr &expr);
  string format_result(const Measurement &num);
};
