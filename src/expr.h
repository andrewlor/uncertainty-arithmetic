#pragma once

#include <memory>
#include <string>
#include <variant>

// A literal value with an optional absolute uncertainty, e.g. 10.25+/-0.05.
struct Measurement {
  double value;
  double uncertainty = 0.0;
};

// A named value, e.g. `a`, bound at evaluation time.
struct Variable {
  std::string name;
};

struct Expr;

// A binary operation; op is one of '+', '-', '*', '/'.
struct BinaryOp {
  char op;
  std::unique_ptr<Expr> lhs;
  std::unique_ptr<Expr> rhs;
};

// A node in the expression tree: either a leaf Measurement or Variable, or an
// interior BinaryOp.
struct Expr {
  std::variant<Measurement, Variable, BinaryOp> node;
};
