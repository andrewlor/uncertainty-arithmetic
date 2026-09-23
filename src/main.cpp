#include <iostream>
#include <string>
#include <vector>

#include "evaluator.h"
#include "parser.h"
using namespace std;

int main(int argc, char *argv[]) {
  vector<string> args(argv + 1, argv + argc);

  if (args.empty() || args[0] == "-h" || args[0] == "--help") {
    cout << "Usage: uncertainty_arithmetic <equation>\n";
    cout << "Example equation: \"10.25+/-0.05 * 0.0024\"\n";
    return args.empty() ? 1 : 0;
  }

  string equation = args[0];
  bool is_independent = false;

  for (const auto &arg : args)
    if (arg == "--independent")
      is_independent = true;

  try {
    unique_ptr<Expr> expr = parse(equation);
    Evaluator evaluator = Evaluator(is_independent);
    Measurement result = evaluator.evaluate(*expr);
    cout << evaluator.format_result(result) << '\n';
  } catch (const ParseError &e) {
    cerr << "Error: " << e.what() << '\n';
    cerr << "  " << equation << '\n';
    cerr << "  " << string(e.position, ' ') << "^\n";
    return 1;
  } catch (const domain_error &e) {
    cerr << "Error: " << e.what() << '\n';
    return 1;
  }

  return 0;
}