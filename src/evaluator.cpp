#include "evaluator.h"

#include <cmath>
#include <sstream>
#include <stdexcept>
using namespace std;

// Combines the uncertainty contributions of two operands: summed for worst
// case, or in quadrature when the operands are independent. Working with
// absolute contributions (rather than relative ones) keeps the result
// well-defined when a value is zero and non-negative when it is negative.
double combine(double x, double y, bool is_independent) {
  return is_independent ? hypot(x, y) : x + y;
}

Measurement Evaluator::apply(char op, const Measurement &a,
                             const Measurement &b) {
  switch (op) {
  case '+':
    return {a.value + b.value,
            combine(a.uncertainty, b.uncertainty, is_independent_)};
  case '-':
    return {a.value - b.value,
            combine(a.uncertainty, b.uncertainty, is_independent_)};
  case '*':
    // d(ab) = |b| da + |a| db
    return {a.value * b.value,
            combine(abs(b.value) * a.uncertainty, abs(a.value) * b.uncertainty,
                    is_independent_)};
  case '/': {
    if (b.value == 0.0) {
      throw domain_error("division by zero");
    }
    // d(a/b) = da / |b| + |a| db / b^2
    return {a.value / b.value,
            combine(a.uncertainty / abs(b.value),
                    abs(a.value) * b.uncertainty / (b.value * b.value),
                    is_independent_)};
  }
  }
  throw logic_error(string("unknown operator '") + op + "'");
}

Measurement Evaluator::evaluate(const Expr &expr,
                                const map<string, Measurement> &vars) {
  if (const Measurement *num = get_if<Measurement>(&expr.node)) {
    return *num;
  }
  if (const Variable *var = get_if<Variable>(&expr.node)) {
    auto it = vars.find(var->name);
    if (it == vars.end()) {
      throw invalid_argument("unknown variable '" + var->name + "'");
    }
    return it->second;
  }
  const auto &bin = get<BinaryOp>(expr.node);
  return apply(bin.op, evaluate(*bin.lhs, vars), evaluate(*bin.rhs, vars));
}

string Evaluator::format_result(const Measurement &num) {
  ostringstream out;
  if (num.uncertainty == 0.0) {
    out << num.value;
    return out.str();
  }

  out << num.value << " +/- " << num.uncertainty;
  return out.str();
}
