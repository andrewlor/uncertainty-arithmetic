#include <gtest/gtest.h>

#include <cmath>
#include <map>
#include <stdexcept>
#include <string>

#include "evaluator.h"
#include "parser.h"

namespace {

constexpr double kTol = 1e-9;

Measurement eval(std::string_view equation, bool independent = false,
                 const std::map<std::string, Measurement> &vars = {}) {
  Evaluator evaluator(independent);
  return evaluator.evaluate(*parse(equation), vars);
}

Measurement worst_case(std::string_view equation,
                       const std::map<std::string, Measurement> &vars = {}) {
  return eval(equation, /*independent=*/false, vars);
}

Measurement independent(std::string_view equation,
                        const std::map<std::string, Measurement> &vars = {}) {
  return eval(equation, /*independent=*/true, vars);
}

#define EXPECT_MEASUREMENT(actual, expected_value, expected_uncertainty)      \
  do {                                                                         \
    const Measurement m_ = (actual);                                           \
    EXPECT_NEAR(m_.value, (expected_value), kTol) << "value";                  \
    EXPECT_NEAR(m_.uncertainty, (expected_uncertainty), kTol)                  \
        << "uncertainty";                                                      \
  } while (0)

// ---------------------------------------------------------------------------
// Leaves

TEST(Evaluate, LiteralReturnsItself) {
  EXPECT_MEASUREMENT(worst_case("10.25+/-0.05"), 10.25, 0.05);
}

TEST(Evaluate, ExactLiteralHasZeroUncertainty) {
  EXPECT_MEASUREMENT(worst_case("3"), 3, 0);
}

TEST(Evaluate, VariableIsLookedUp) {
  EXPECT_MEASUREMENT(worst_case("a", {{"a", {2, 0.1}}}), 2, 0.1);
}

TEST(Evaluate, UnknownVariableThrows) {
  EXPECT_THROW(worst_case("a + 1"), std::invalid_argument);
  try {
    worst_case("a * b", {{"a", {1, 0}}});
    FAIL() << "expected invalid_argument";
  } catch (const std::invalid_argument &e) {
    EXPECT_STREQ(e.what(), "unknown variable 'b'");
  }
}

TEST(Evaluate, VariableNamesAreCaseSensitive) {
  EXPECT_THROW(worst_case("A", {{"a", {1, 0}}}), std::invalid_argument);
}

// ---------------------------------------------------------------------------
// Exact arithmetic (no uncertainty involved)

TEST(ExactArithmetic, Operators) {
  EXPECT_MEASUREMENT(worst_case("2 + 3"), 5, 0);
  EXPECT_MEASUREMENT(worst_case("2 - 3"), -1, 0);
  EXPECT_MEASUREMENT(worst_case("2 * 3"), 6, 0);
  EXPECT_MEASUREMENT(worst_case("3 / 2"), 1.5, 0);
}

TEST(ExactArithmetic, PrecedenceAndParentheses) {
  EXPECT_MEASUREMENT(worst_case("1 + 2 * 3"), 7, 0);
  EXPECT_MEASUREMENT(worst_case("(1 + 2) * 3"), 9, 0);
  EXPECT_MEASUREMENT(worst_case("10 - 4 - 3"), 3, 0);
  EXPECT_MEASUREMENT(worst_case("16 / 4 / 2"), 2, 0);
}

TEST(ExactArithmetic, MultiplyingByZeroIsExact) {
  // 0 has relative uncertainty 0/0; the result must still be an exact 0.
  EXPECT_MEASUREMENT(worst_case("0 * 5"), 0, 0);
  EXPECT_MEASUREMENT(independent("0 * 5"), 0, 0);
}

TEST(ExactArithmetic, DividingZeroIsExact) {
  EXPECT_MEASUREMENT(worst_case("0 / 5"), 0, 0);
}

// ---------------------------------------------------------------------------
// Worst case (linear) propagation

TEST(WorstCase, AdditionSumsAbsoluteUncertainties) {
  EXPECT_MEASUREMENT(worst_case("1+/-0.1 + 2+/-0.2"), 3, 0.3);
}

TEST(WorstCase, SubtractionSumsAbsoluteUncertainties) {
  EXPECT_MEASUREMENT(worst_case("5+/-0.1 - 2+/-0.2"), 3, 0.3);
}

TEST(WorstCase, AddingExactValueKeepsUncertainty) {
  EXPECT_MEASUREMENT(worst_case("1+/-0.1 + 2"), 3, 0.1);
}

TEST(WorstCase, MultiplicationSumsRelativeUncertainties) {
  // rel = 0.1/10 + 0.1/2 = 0.06; 20 * 0.06 = 1.2
  EXPECT_MEASUREMENT(worst_case("10+/-0.1 * 2+/-0.1"), 20, 1.2);
}

TEST(WorstCase, DivisionSumsRelativeUncertainties) {
  // rel = 0.06; 5 * 0.06 = 0.3
  EXPECT_MEASUREMENT(worst_case("10+/-0.1 / 2+/-0.1"), 5, 0.3);
}

TEST(WorstCase, ScalingByExactValueScalesUncertainty) {
  EXPECT_MEASUREMENT(worst_case("10+/-0.1 * 3"), 30, 0.3);
  EXPECT_MEASUREMENT(worst_case("10+/-0.1 / 2"), 5, 0.05);
}

TEST(WorstCase, TwoNegativeFactors) {
  EXPECT_MEASUREMENT(worst_case("-2+/-0.1 * -3"), 6, 0.3);
}

TEST(WorstCase, ChainedOperations) {
  // (1+/-0.1 + 1+/-0.1) = 2+/-0.2; * 2 -> 4+/-0.4
  EXPECT_MEASUREMENT(worst_case("(1+/-0.1 + 1+/-0.1) * 2"), 4, 0.4);
}

TEST(WorstCase, WithVariables) {
  EXPECT_MEASUREMENT(worst_case("a * b", {{"a", {10, 0.1}}, {"b", {2, 0.1}}}),
                     20, 1.2);
}

// ---------------------------------------------------------------------------
// Independent (quadrature) propagation

TEST(Independent, MultiplicationAddsRelativeUncertaintiesInQuadrature) {
  // rel = sqrt(0.03^2 + 0.04^2) = 0.05; 100 * 0.05 = 5
  EXPECT_MEASUREMENT(independent("10+/-0.3 * 10+/-0.4"), 100, 5);
}

TEST(Independent, DivisionAddsRelativeUncertaintiesInQuadrature) {
  EXPECT_MEASUREMENT(independent("10+/-0.3 / 10+/-0.4"), 1, 0.05);
}

TEST(Independent, IsNeverLargerThanWorstCase) {
  const char *equation = "10+/-0.3 * 10+/-0.4";
  EXPECT_LE(independent(equation).uncertainty,
            worst_case(equation).uncertainty);
}

TEST(Independent, SingleUncertainFactorMatchesWorstCase) {
  EXPECT_MEASUREMENT(independent("10+/-0.1 * 3"), 30, 0.3);
}

TEST(Independent, AdditionAddsAbsoluteUncertaintiesInQuadrature) {
  // sqrt(0.3^2 + 0.4^2) = 0.5
  EXPECT_MEASUREMENT(independent("1+/-0.3 + 1+/-0.4"), 2, 0.5);
}

TEST(Independent, SubtractionAddsAbsoluteUncertaintiesInQuadrature) {
  EXPECT_MEASUREMENT(independent("5+/-0.3 - 1+/-0.4"), 4, 0.5);
}

// ---------------------------------------------------------------------------
// Uncertainty is always non-negative and finite for finite inputs

TEST(UncertaintySign, NegativeProductHasPositiveUncertainty) {
  EXPECT_MEASUREMENT(worst_case("-2+/-0.1 * 3"), -6, 0.3);
  EXPECT_MEASUREMENT(independent("-2+/-0.1 * 3"), -6, 0.3);
}

TEST(UncertaintySign, NegativeQuotientHasPositiveUncertainty) {
  EXPECT_MEASUREMENT(worst_case("10+/-0.1 / -2"), -5, 0.05);
}

TEST(UncertaintySign, UncertainZeroTimesValue) {
  // d(a*b) = |b| da + |a| db = 5 * 0.1 + 0 = 0.5
  EXPECT_MEASUREMENT(worst_case("0+/-0.1 * 5"), 0, 0.5);
}

TEST(UncertaintySign, UncertainZeroDividedByValue) {
  EXPECT_MEASUREMENT(worst_case("0+/-0.1 / 5"), 0, 0.02);
}

// ---------------------------------------------------------------------------
// Errors

TEST(EvaluateErrors, DivisionByZeroThrows) {
  EXPECT_THROW(worst_case("1 / 0"), std::domain_error);
  EXPECT_THROW(independent("1 / 0"), std::domain_error);
}

TEST(EvaluateErrors, DivisionByZeroWithUncertaintyThrows) {
  EXPECT_THROW(worst_case("1 / 0+/-0.1"), std::domain_error);
}

TEST(EvaluateErrors, DivisionByZeroExpressionThrows) {
  EXPECT_THROW(worst_case("1 / (2 - 2)"), std::domain_error);
}

TEST(EvaluateErrors, DivisionByZeroVariableThrows) {
  EXPECT_THROW(worst_case("1 / z", {{"z", {0, 0}}}), std::domain_error);
}

// ---------------------------------------------------------------------------
// format_result

TEST(FormatResult, ExactValueOmitsUncertainty) {
  EXPECT_EQ(Evaluator(false).format_result({1.5, 0}), "1.5");
}

TEST(FormatResult, IncludesUncertainty) {
  EXPECT_EQ(Evaluator(false).format_result({1.5, 0.1}), "1.5 +/- 0.1");
}

TEST(FormatResult, NegativeValue) {
  EXPECT_EQ(Evaluator(false).format_result({-6, 0.3}), "-6 +/- 0.3");
}

TEST(FormatResult, SameInBothModes) {
  EXPECT_EQ(Evaluator(true).format_result({3, 0.3}),
            Evaluator(false).format_result({3, 0.3}));
}

} // namespace
