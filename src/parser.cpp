#include "parser.h"

#include <cctype>
using namespace std;

// Recursive-descent parser for the grammar:
//
//   expr    := term (('+' | '-') term)*
//   term    := factor (('*' | '/') factor)*
//   factor  := number | '(' expr ')'
//   number  := ['-'] decimal ['+/-' decimal]
//   decimal := digits ['.' digits] | '.' digits
//
// Each precedence level gets its own function, so * and / bind tighter than
// + and -, and the loops build left-associative trees: 1-2-3 is (1-2)-3.
// Whitespace is allowed between tokens.

namespace {

class Parser {
public:
  explicit Parser(string_view input) : input_(input) {}

  unique_ptr<Expr> parse() {
    auto expr = parse_expr();
    if (peek() == ')') {
      throw ParseError("unmatched ')'", pos_);
    }
    if (pos_ < input_.size()) {
      fail("expected an operator");
    }
    return expr;
  }

private:
  string_view input_;
  size_t pos_ = 0;

  // Skips whitespace, then returns the next character without consuming it,
  // or '\0' at the end of the input.
  char peek() {
    while (pos_ < input_.size() &&
           isspace(static_cast<unsigned char>(input_[pos_]))) {
      ++pos_;
    }
    return pos_ < input_.size() ? input_[pos_] : '\0';
  }

  bool at_uncertainty() {
    peek();
    return input_.substr(pos_).starts_with("+/-");
  }

  [[noreturn]] void fail(const string &what) {
    string found = pos_ < input_.size() ? "'" + string(1, input_[pos_]) + "'"
                                        : "end of input";
    throw ParseError(what + ", found " + found, pos_);
  }

  static unique_ptr<Expr> make_binary(char op, unique_ptr<Expr> lhs,
                                      unique_ptr<Expr> rhs) {
    return make_unique<Expr>(
        Expr{BinaryOp{op, std::move(lhs), std::move(rhs)}});
  }

  unique_ptr<Expr> parse_expr() {
    auto lhs = parse_term();
    while (true) {
      if (at_uncertainty()) {
        fail("'+/-' can only follow a number");
      }
      char op = peek();
      if (op != '+' && op != '-') {
        return lhs;
      }
      ++pos_;
      lhs = make_binary(op, std::move(lhs), parse_term());
    }
  }

  unique_ptr<Expr> parse_term() {
    auto lhs = parse_factor();
    while (true) {
      char op = peek();
      if (op != '*' && op != '/') {
        return lhs;
      }
      ++pos_;
      lhs = make_binary(op, std::move(lhs), parse_factor());
    }
  }

  unique_ptr<Expr> parse_factor() {
    if (peek() == '(') {
      ++pos_;
      auto inner = parse_expr();
      if (peek() != ')') {
        fail("expected ')'");
      }
      ++pos_;
      return inner;
    }
    return make_unique<Expr>(Expr{parse_number()});
  }

  Measurement parse_number() {
    peek();
    bool negative = pos_ < input_.size() && input_[pos_] == '-';
    if (negative) {
      ++pos_;
    }
    Measurement num{parse_decimal()};
    if (negative) {
      num.value = -num.value;
    }
    if (at_uncertainty()) {
      pos_ += 3;
      peek();
      num.uncertainty = parse_decimal();
    }
    return num;
  }

  double parse_decimal() {
    size_t start = pos_;
    size_t digits = 0;
    auto skip_digits = [&] {
      while (pos_ < input_.size() &&
             isdigit(static_cast<unsigned char>(input_[pos_]))) {
        ++pos_;
        ++digits;
      }
    };
    skip_digits();
    if (pos_ < input_.size() && input_[pos_] == '.') {
      ++pos_;
      skip_digits();
    }
    if (digits == 0) {
      pos_ = start;
      fail("expected a number");
    }
    return stod(string(input_.substr(start, pos_ - start)));
  }
};

} // namespace

unique_ptr<Expr> parse(string_view input) { return Parser(input).parse(); }
