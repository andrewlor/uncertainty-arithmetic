#include "evaluator.h"

#include <cmath>
#include <sstream>
#include <stdexcept>
using namespace std;

double get_relative_uncertainty(const Measurement &m) {
  return m.uncertainty / abs(m.value);
}

double linear_combined_relative_uncertainty(const Measurement &a,
                                            const Measurement &b) {
  return get_relative_uncertainty(a) + get_relative_uncertainty(b);
}

double quadratic_combined_relative_uncertainty(const Measurement &a,
                                               const Measurement &b) {
  return sqrt(pow(get_relative_uncertainty(a), 2) +
              pow(get_relative_uncertainty(b), 2));
}

Measurement Evaluator::apply(char op, const Measurement &a,
                             const Measurement &b) {
  auto combined_relative_uncertainty =
      is_independent_ ? quadratic_combined_relative_uncertainty
                      : linear_combined_relative_uncertainty;
  switch (op) {
  case '+':
    return {a.value + b.value, a.uncertainty + b.uncertainty};
  case '-':
    return {a.value - b.value, a.uncertainty + b.uncertainty};
  case '*': {
    const double value = a.value * b.value;
    return {value, value * combined_relative_uncertainty(a, b)};
  }
  case '/': {
    if (b.value == 0.0) {
      throw domain_error("division by zero");
    }
    const double value = a.value / b.value;
    return {value, value * combined_relative_uncertainty(a, b)};
  }
  }
  throw logic_error(string("unknown operator '") + op + "'");
}

Measurement Evaluator::evaluate(const Expr &expr) {
  if (const Measurement *num = get_if<Measurement>(&expr.node)) {
    return *num;
  }
  const auto &bin = get<BinaryOp>(expr.node);
  return apply(bin.op, evaluate(*bin.lhs), evaluate(*bin.rhs));
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
