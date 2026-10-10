# DRAM Bank-Link Analysis

This repository contains three stages of an LPDDR4 bank-to-logic-die link
investigation:

- `preliminary_contention/`: the original window-based collision estimate.
- `finite_bandwidth_link/`: the C++ discrete-event link and FIFO simulator.
- `integration/`: the end-to-end DRAMsim3-to-bank-link experiment driver.

DRAMsim3 is an upstream dependency and is intentionally not vendored here. The
small patch in `integration/patches/` adds the READ-issue trace-export boundary
used by this project. The integration driver builds the patched DRAMsim3 tree,
runs an LPDDR4 workload, validates the resulting trace, and passes it to the
finite-bandwidth parameter sweep.

Generated results, build products, Python caches, IDE settings, and operating
system metadata are excluded from version control. The repository therefore
contains the source, tests, sample workload, orchestration scripts, and the
patch required to reproduce the workflow, but not local experiment output.

## Repository layout

```text
.
├── preliminary_contention/       # Uniform-window collision estimate
├── finite_bandwidth_link/         # C++ FIFO/link discrete-event model
└── integration/
    ├── patches/                   # Minimal changes to upstream DRAMsim3
    ├── workloads/                 # Small checked-in input traces
    └── run_pipeline.py            # End-to-end experiment driver
```

## DRAMsim3 dependency

The integration was verified against upstream DRAMsim3 commit
`29817593b3389f1337235d63cac515024ab8fd6e` (tag `1.0.0`). Place DRAMsim3 next
to this repository and apply the included patch:

```sh
git clone https://github.com/umd-memsys/DRAMsim3.git ../DRAMsim3
git -C ../DRAMsim3 checkout 29817593b3389f1337235d63cac515024ab8fd6e
git -C ../DRAMsim3 apply \
  "$(pwd)/integration/patches/dramsim3_bank_ready_trace.patch"
```

The patch adds the optional `BANK_READY_TRACE` build setting and emits one
record when a READ reaches `Controller::IssueCommand`. It also corrects trace
EOF handling and gives the polymorphic CPU base class a virtual destructor.

## End-to-end smoke run

From this repository root:

```sh
python3 integration/run_pipeline.py
```

By default, the driver expects the patched DRAMsim3 checkout at `../DRAMsim3`,
uses its supplied LPDDR4-2400 x16-device configuration, and writes generated data under
`results/integration/smoke_lpddr4_2400/`.

The bank-ready trace is read-only. Each line denotes one issued BL16 READ and
uses READ issue time as a normalized pipeline boundary:

```text
issue_time_ns Read physical_address
```

A fixed command-to-prefetch latency is intentionally omitted because it shifts
all arrivals equally and does not affect steady-state queueing or throughput.
This is an offline trace-coupled workflow rather than an in-process DRAMsim3
API. The checked-in smoke workload demonstrates integration; it is not by
itself a representative architectural evaluation.
