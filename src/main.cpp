#include <iostream>
#include <string>
#include <vector>

#include "csv.h"
#include "evaluator.h"
#include "parser.h"
using namespace std;

int main(int argc, char *argv[]) {
  vector<string> args(argv + 1, argv + argc);

  if (args.empty() || args[0] == "-h" || args[0] == "--help") {
    cout << "Usage: uncertainty_arithmetic [--independent] <equation> "
            "[measurements.csv]\n";
    cout << "Example equation: \"10.25+/-0.05 * 0.0024\"\n";
    cout << "With a CSV file, the equation may use the header's column names\n";
    cout << "as variables and is evaluated once per row, e.g. \"a * b\".\n";
    cout << "\nOptions:\n";
    cout << "  --independent  combine uncertainties in quadrature, assuming\n";
    cout << "                 independent errors (default: worst case, summed)\n";
    cout << "  -h, --help     show this help\n";
    return args.empty() ? 1 : 0;
  }

  bool is_independent = false;
  vector<string> positional;

  for (const auto &arg : args) {
    if (arg == "--independent")
      is_independent = true;
    else
      positional.push_back(arg);
  }

  if (positional.empty() || positional.size() > 2) {
    cerr << "Error: expected an equation and an optional CSV file\n";
    return 1;
  }
  string equation = positional[0];

  try {
    unique_ptr<Expr> expr = parse(equation);
    Evaluator evaluator = Evaluator(is_independent);
    if (positional.size() == 1) {
      Measurement result = evaluator.evaluate(*expr);
      cout << evaluator.format_result(result) << '\n';
    } else {
      for (const auto &vars : read_measurements_csv(positional[1])) {
        Measurement result = evaluator.evaluate(*expr, vars);
        cout << evaluator.format_result(result) << '\n';
      }
    }
  } catch (const ParseError &e) {
    cerr << "Error: " << e.what() << '\n';
    cerr << "  " << equation << '\n';
    cerr << "  " << string(e.position, ' ') << "^\n";
    return 1;
  } catch (const exception &e) {
    cerr << "Error: " << e.what() << '\n';
    return 1;
  }

  return 0;
}
