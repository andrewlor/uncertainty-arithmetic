# uncertainty-arithmetic

A small command-line calculator that carries measurement uncertainty through arithmetic.

Write numbers as `value+/-uncertainty` and the tool tells you how much uncertainty the result has. You can pick worst-case or independent propagation. You can also run one equation over every row of a CSV of measurements.

```console
$ uncertainty_arithmetic "(2.0+/-0.1) * (3.0+/-0.2)"
6 +/- 0.7
```

## Building

You need CMake 3.20+ and a C++20 compiler (recent Clang or GCC).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/uncertainty_arithmetic --help
```

## Usage

```
uncertainty_arithmetic [--independent] <equation> [measurements.csv]
```

### Equations

- Numbers can have an absolute uncertainty: `10.25+/-0.05`. A plain number such as `3` is treated as exact.
- Supported operators are `+`, `-`, `*` and `/`, with the usual precedence and parentheses.
- Whitespace between tokens is optional.

```console
$ uncertainty_arithmetic "10.25+/-0.05 * 0.0024"
0.0246 +/- 0.00012

$ uncertainty_arithmetic "(5.0+/-0.1) / (2.0+/-0.05)"
2.5 +/- 0.1125
```

### Worst case vs. independent errors

By default, uncertainties are combined **worst case**: every contribution is added linearly. This gives a conservative upper bound.

With `--independent`, contributions are added **in quadrature** (root-sum-square). This is the standard choice when the errors are random and uncorrelated, and it gives a tighter result.

```console
$ uncertainty_arithmetic "12.4+/-0.2 - 3.1+/-0.1"
9.3 +/- 0.3

$ uncertainty_arithmetic --independent "12.4+/-0.2 - 3.1+/-0.1"
9.3 +/- 0.223607
```

The rules, where `δ` is the absolute uncertainty and `⊕` is `+` (worst case) or root-sum-square (independent):

| Operation | Result uncertainty          |
|-----------|-----------------------------|
| `a + b`   | `δa ⊕ δb`                   |
| `a - b`   | `δa ⊕ δb`                   |
| `a * b`   | `\|b\|·δa ⊕ \|a\|·δb`       |
| `a / b`   | `δa/\|b\| ⊕ \|a\|·δb/b²`    |

### Evaluating a CSV of measurements

Pass a CSV file as the second argument. The header row names the variables, and the equation is evaluated once per data row:

```csv
distance,time
100+/-0.5,9.8+/-0.2
200+/-0.5,19.5+/-0.2
400+/-1,41.2+/-0.3
```

```console
$ uncertainty_arithmetic --independent "distance / time" runs.csv
10.2041 +/- 0.214405
10.2564 +/- 0.108274
9.70874 +/- 0.0747453
```

Variable names start with a letter or `_`, followed by letters, digits or `_`.

### Errors

Parse errors show where the problem is:

```console
$ uncertainty_arithmetic "2 * (3 +"
Error: expected a number, found end of input
  2 * (3 +
          ^
```

## Example uses

- **Lab reports:** propagate instrument precision through a derived quantity, such as density from mass and volume.
- **Batch processing:** compute a derived value with its uncertainty for every trial in a data log.
- **Tolerance stack-ups:** add part dimensions with manufacturing tolerances. Use worst case for guaranteed fit and `--independent` for a statistical estimate.
- **Sanity checks:** quickly see which input dominates the error in a calculation.

## Limitations

- Each occurrence of a value is treated as a separate measurement, so correlations are not tracked. For example, `x - x` gives a nonzero uncertainty.
- Propagation is first order (linear), which is accurate when uncertainties are small relative to their values.
- Only `+ - * /` are supported for now; there are no functions or powers.

## Running the tests

Tests use GoogleTest, which CMake downloads automatically:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Debug builds enable AddressSanitizer and UBSan.

## License

[MIT](LICENSE)
