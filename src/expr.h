#pragma once

#include <memory>
#include <variant>

// A literal value with an optional absolute uncertainty, e.g. 10.25+/-0.05.
struct Measurement {
  double value;
  double uncertainty = 0.0;
};

struct Expr;

// A binary operation; op is one of '+', '-', '*', '/'.
struct BinaryOp {
  char op;
  std::unique_ptr<Expr> lhs;
  std::unique_ptr<Expr> rhs;
};

// A node in the expression tree: either a leaf Measurement or an interior BinaryOp.
struct Expr {
  std::variant<Measurement, BinaryOp> node;
};
