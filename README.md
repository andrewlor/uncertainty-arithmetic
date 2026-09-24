# uncertainty-arithmetic

CLI utility for computing arithmetic operations with measurement uncertainty.

For example say you needed to compute (10.21 ± 0.01)kg multplied by (0.0754 ± 0.0002)g/kg:
```console
$ uncertainty_arithmetic "10.21 +/- 0.01 * 0.0754 +/- 0.0002"
0.769834 +/- 0.002796
```
=> (0.769834 ± 0.002796)g

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

By default, uncertainties are combined linearly. This gives a conservative bound on the worst case error ie. both errors swing to the same extreme.

With `--independent`, uncertainties combined in a quadrature (root-sum-square). This assumes the uncertainties are independent and random, so the they just as likely to partially cancel as to reinforce. Both being at their extremes in the same direction is unlikely, so the combined uncertainty is smaller than the worst case.

```console
$ uncertainty_arithmetic "12.4+/-0.2 - 3.1+/-0.1"
9.3 +/- 0.3

$ uncertainty_arithmetic --independent "12.4+/-0.2 - 3.1+/-0.1"
9.3 +/- 0.223607
```

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
