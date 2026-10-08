# Bank Link Simulator

This trace-driven discrete-event simulator evaluates finite-bandwidth links
shared by groups of DRAM banks. A trace line contains:

```text
ready_time_ns Read|Write physical_address
```

The ready time represents the time at which a bank's internal result is ready
to enter the modeled die-to-die link. The simulator models link service,
per-link FIFO contention, and no-backpressure overflow. It does not generate
DRAM ACTIVATE/READ/PRECHARGE timing internally.

## Build and test

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

## Run one configuration

```sh
./build/bank_link_sim \
  --trace tests/sample.trace \
  --banks-per-link 2 \
  --link-width-bits 16 \
  --transfers-per-ns 4.266 \
  --fifo-depth 4 \
  --link-output results/links.csv \
  --bank-output results/banks.csv
```

The summary CSV is written to standard output unless `--summary-output` is
provided. Exit status 2 means the configuration overflowed and was marked
infeasible; a summary row is still produced.

## Sweep configurations

```sh
python3 scripts/run_sweep.py \
  --trace tests/sample.trace \
  --output results/sweep.csv
```

Use comma-separated arguments such as `--banks-per-link 1,2,4,8`,
`--link-width-bits 8,16,32`, and `--fifo-depth 1,2,4,8`. The sweep uses CMake
to build the executable unless `--no-build` is passed.
