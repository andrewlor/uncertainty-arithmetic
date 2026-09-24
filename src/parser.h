#pragma once

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

#include "expr.h"

class ParseError : public std::runtime_error {
public:
  ParseError(const std::string &message, std::size_t pos)
      : std::runtime_error(message), position(pos) {}

  std::size_t position; // 0-based offset into the input where parsing failed
};

// Parses an equation such as "(1.5+/-0.1 + 2) * 3" into an expression tree.
// Supports variables, +, -, *, / with the usual precedence, left-to-right associativity,
// and parentheses. Throws ParseError on invalid input.
std::unique_ptr<Expr> parse(std::string_view input);

// Parses a single measurement such as "10+/-0.1" (no operators or variables).
// Throws ParseError on invalid input.
Measurement parse_measurement(std::string_view input);
