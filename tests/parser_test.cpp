#include <gtest/gtest.h>

#include <sstream>
#include <string>

#include "parser.h"

namespace {

// Renders an expression tree as an S-expression, e.g. "(+ 1 (* 2 3))", so
// tests can assert on structure (precedence, associativity) in one line.
std::string to_sexpr(const Expr &expr) {
  std::ostringstream out;
  if (const auto *num = std::get_if<Measurement>(&expr.node)) {
    out << num->value;
    if (num->uncertainty != 0.0) {
      out << "+/-" << num->uncertainty;
    }
  } else if (const auto *var = std::get_if<Variable>(&expr.node)) {
    out << var->name;
  } else {
    const auto &bin = std::get<BinaryOp>(expr.node);
    out << '(' << bin.op << ' ' << to_sexpr(*bin.lhs) << ' '
        << to_sexpr(*bin.rhs) << ')';
  }
  return out.str();
}

std::string parse_sexpr(std::string_view input) {
  return to_sexpr(*parse(input));
}

// Asserts that parsing `input` fails with `message` at `position`.
void expect_parse_error(std::string_view input, const std::string &message,
                        std::size_t position) {
  SCOPED_TRACE("input: \"" + std::string(input) + "\"");
  try {
    parse(input);
    ADD_FAILURE() << "expected ParseError";
  } catch (const ParseError &e) {
    EXPECT_EQ(e.what(), message);
    EXPECT_EQ(e.position, position);
  }
}

// ---------------------------------------------------------------------------
// Numbers and measurements

TEST(ParseNumber, Integer) { EXPECT_EQ(parse_sexpr("42"), "42"); }

TEST(ParseNumber, Decimal) { EXPECT_EQ(parse_sexpr("3.25"), "3.25"); }

TEST(ParseNumber, LeadingDot) { EXPECT_EQ(parse_sexpr(".5"), "0.5"); }

TEST(ParseNumber, TrailingDot) { EXPECT_EQ(parse_sexpr("5."), "5"); }

TEST(ParseNumber, Negative) { EXPECT_EQ(parse_sexpr("-3"), "-3"); }

TEST(ParseNumber, WithUncertainty) {
  auto expr = parse("10.25+/-0.05");
  const auto &num = std::get<Measurement>(expr->node);
  EXPECT_DOUBLE_EQ(num.value, 10.25);
  EXPECT_DOUBLE_EQ(num.uncertainty, 0.05);
}

TEST(ParseNumber, WithoutUncertaintyDefaultsToZero) {
  auto expr = parse("7");
  EXPECT_EQ(std::get<Measurement>(expr->node).uncertainty, 0.0);
}

TEST(ParseNumber, UncertaintyWithSpaces) {
  EXPECT_EQ(parse_sexpr("1 +/- 0.1"), "1+/-0.1");
}

TEST(ParseNumber, NegativeWithLeadingDotUncertainty) {
  EXPECT_EQ(parse_sexpr("-1.5+/-.25"), "-1.5+/-0.25");
}

TEST(ParseNumber, LargestDoublesAreAccepted) {
  auto expr = parse("1" + std::string(300, '0'));
  EXPECT_DOUBLE_EQ(std::get<Measurement>(expr->node).value, 1e300);
}

TEST(ParseNumber, TooSmallForADoubleRoundsToZero) {
  auto expr = parse("0." + std::string(400, '0') + "1");
  EXPECT_EQ(std::get<Measurement>(expr->node).value, 0.0);
}

TEST(ParseNumber, TinyUncertaintyRoundsToZero) {
  auto expr = parse("1+/-0." + std::string(400, '0') + "1");
  EXPECT_EQ(std::get<Measurement>(expr->node).uncertainty, 0.0);
}

TEST(ParseNumber, SubnormalIsKept) {
  // 1e-310 is below the smallest normal double but still representable.
  auto expr = parse("0." + std::string(309, '0') + "1");
  EXPECT_GT(std::get<Measurement>(expr->node).value, 0.0);
}

// ---------------------------------------------------------------------------
// Variables

TEST(ParseVariable, SingleLetter) { EXPECT_EQ(parse_sexpr("a"), "a"); }

TEST(ParseVariable, UnderscoresAndDigits) {
  EXPECT_EQ(parse_sexpr("_x1"), "_x1");
  EXPECT_EQ(parse_sexpr("foo_bar2"), "foo_bar2");
}

TEST(ParseVariable, InExpression) {
  EXPECT_EQ(parse_sexpr("a * b + 2"), "(+ (* a b) 2)");
}

// ---------------------------------------------------------------------------
// Operators, precedence and associativity

TEST(ParseOperators, EachOperator) {
  EXPECT_EQ(parse_sexpr("1+2"), "(+ 1 2)");
  EXPECT_EQ(parse_sexpr("1-2"), "(- 1 2)");
  EXPECT_EQ(parse_sexpr("1*2"), "(* 1 2)");
  EXPECT_EQ(parse_sexpr("1/2"), "(/ 1 2)");
}

TEST(ParseOperators, MultiplicationBindsTighterThanAddition) {
  EXPECT_EQ(parse_sexpr("1+2*3"), "(+ 1 (* 2 3))");
  EXPECT_EQ(parse_sexpr("1*2+3"), "(+ (* 1 2) 3)");
  EXPECT_EQ(parse_sexpr("1-6/3"), "(- 1 (/ 6 3))");
}

TEST(ParseOperators, SubtractionIsLeftAssociative) {
  EXPECT_EQ(parse_sexpr("1-2-3"), "(- (- 1 2) 3)");
}

TEST(ParseOperators, DivisionIsLeftAssociative) {
  EXPECT_EQ(parse_sexpr("8/4/2"), "(/ (/ 8 4) 2)");
}

TEST(ParseOperators, MixedSamePrecedenceIsLeftAssociative) {
  EXPECT_EQ(parse_sexpr("1+2-3+4"), "(+ (- (+ 1 2) 3) 4)");
  EXPECT_EQ(parse_sexpr("2*3/4*5"), "(* (/ (* 2 3) 4) 5)");
}

TEST(ParseOperators, ParenthesesOverridePrecedence) {
  EXPECT_EQ(parse_sexpr("(1+2)*3"), "(* (+ 1 2) 3)");
  EXPECT_EQ(parse_sexpr("1-(2-3)"), "(- 1 (- 2 3))");
}

TEST(ParseOperators, NestedParentheses) {
  EXPECT_EQ(parse_sexpr("((1))"), "1");
  EXPECT_EQ(parse_sexpr("((1+2)*(3-4))/5"), "(/ (* (+ 1 2) (- 3 4)) 5)");
}

TEST(ParseOperators, NegativeOperandAfterOperator) {
  EXPECT_EQ(parse_sexpr("1 - -2"), "(- 1 -2)");
  EXPECT_EQ(parse_sexpr("2*-3"), "(* 2 -3)");
}

TEST(ParseOperators, MeasurementFollowedByPlus) {
  EXPECT_EQ(parse_sexpr("1+/-0.1+2"), "(+ 1+/-0.1 2)");
  EXPECT_EQ(parse_sexpr("1+/-0.1-2+/-0.2"), "(- 1+/-0.1 2+/-0.2)");
}

TEST(ParseOperators, WhitespaceIsIgnoredBetweenTokens) {
  EXPECT_EQ(parse_sexpr("  1 \t+\n2  *  ( 3 )  "), "(+ 1 (* 2 3))");
}

// ---------------------------------------------------------------------------
// Errors

TEST(ParseErrors, EmptyInput) {
  expect_parse_error("", "expected a number, found end of input", 0);
}

TEST(ParseErrors, WhitespaceOnly) {
  expect_parse_error("   ", "expected a number, found end of input", 3);
}

TEST(ParseErrors, TrailingOperator) {
  expect_parse_error("1 +", "expected a number, found end of input", 3);
}

TEST(ParseErrors, MissingOperator) {
  expect_parse_error("1 2", "expected an operator, found '2'", 2);
}

TEST(ParseErrors, DoubleOperator) {
  expect_parse_error("1 ** 2", "expected a number, found '*'", 3);
}

TEST(ParseErrors, UnsupportedOperator) {
  expect_parse_error("1 ^ 2", "expected an operator, found '^'", 2);
}

TEST(ParseErrors, UnclosedParenthesis) {
  expect_parse_error("(1+2", "expected ')', found end of input", 4);
}

TEST(ParseErrors, UnmatchedCloseParenthesis) {
  expect_parse_error("1+2)", "unmatched ')'", 3);
}

TEST(ParseErrors, EmptyParentheses) {
  expect_parse_error("()", "expected a number, found ')'", 1);
}

TEST(ParseErrors, UncertaintyAfterVariable) {
  expect_parse_error("a+/-0.1", "'+/-' can only follow a number, found '+'",
                     1);
}

TEST(ParseErrors, UncertaintyAfterParenthesis) {
  expect_parse_error("(1)+/-0.1", "'+/-' can only follow a number, found '+'",
                     3);
}

TEST(ParseErrors, RepeatedUncertainty) {
  expect_parse_error("1 +/- 0.1 +/- 0.2",
                     "'+/-' can only follow a number, found '+'", 10);
}

TEST(ParseErrors, MissingUncertaintyValue) {
  expect_parse_error("1+/-", "expected a number, found end of input", 4);
}

TEST(ParseErrors, NegativeUncertainty) {
  expect_parse_error("1+/--0.1", "expected a number, found '-'", 4);
}

TEST(ParseErrors, NegatedVariableIsNotSupported) {
  expect_parse_error("-a", "expected a number, found 'a'", 1);
}

TEST(ParseErrors, NegatedParenthesisIsNotSupported) {
  expect_parse_error("-(1)", "expected a number, found '('", 1);
}

TEST(ParseErrors, LoneDot) {
  expect_parse_error(".", "expected a number, found '.'", 0);
}

TEST(ParseErrors, TwoDecimalPoints) {
  expect_parse_error("1.2.3", "expected an operator, found '.'", 3);
}

TEST(ParseErrors, ScientificNotationIsNotSupported) {
  expect_parse_error("1e5", "expected an operator, found 'e'", 1);
}

TEST(ParseErrors, NumberTooLarge) {
  expect_parse_error("2 * 1" + std::string(400, '0'), "number out of range",
                     4);
}

TEST(ParseErrors, NegativeNumberTooLarge) {
  // The position points at the digits, just after the sign.
  expect_parse_error("-1" + std::string(400, '0'), "number out of range", 1);
}

TEST(ParseErrors, UncertaintyTooLarge) {
  expect_parse_error("1+/-1" + std::string(400, '0'), "number out of range",
                     4);
}

// ---------------------------------------------------------------------------
// parse_measurement

TEST(ParseMeasurement, ValueOnly) {
  Measurement m = parse_measurement("10");
  EXPECT_DOUBLE_EQ(m.value, 10);
  EXPECT_EQ(m.uncertainty, 0.0);
}

TEST(ParseMeasurement, ValueAndUncertainty) {
  Measurement m = parse_measurement("0.25+/-0.001");
  EXPECT_DOUBLE_EQ(m.value, 0.25);
  EXPECT_DOUBLE_EQ(m.uncertainty, 0.001);
}

TEST(ParseMeasurement, Negative) {
  Measurement m = parse_measurement(" -5 +/- 1 ");
  EXPECT_DOUBLE_EQ(m.value, -5);
  EXPECT_DOUBLE_EQ(m.uncertainty, 1);
}

TEST(ParseMeasurement, RejectsEmpty) {
  EXPECT_THROW(parse_measurement(""), ParseError);
}

TEST(ParseMeasurement, RejectsVariable) {
  EXPECT_THROW(parse_measurement("a"), ParseError);
}

TEST(ParseMeasurement, RejectsExpression) {
  try {
    parse_measurement("1+2");
    FAIL() << "expected ParseError";
  } catch (const ParseError &e) {
    EXPECT_STREQ(e.what(), "expected end of measurement, found '+'");
    EXPECT_EQ(e.position, 1u);
  }
}

TEST(ParseMeasurement, RejectsTrailingGarbage) {
  EXPECT_THROW(parse_measurement("1+/-0.1 x"), ParseError);
}

} // namespace
