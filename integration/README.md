# DRAMsim3 Integration

`run_pipeline.py` orchestrates one DRAMsim3 execution followed by a sweep of
the finite-bandwidth bank-link simulator. It does not copy traces between
source trees. DRAMsim3 writes the trace directly into a run-specific results
directory, and that exact path is supplied to the downstream sweep.

DRAMsim3 itself is not included in this repository. Apply
`patches/dramsim3_bank_ready_trace.patch` to upstream commit
`29817593b3389f1337235d63cac515024ab8fd6e` before running the pipeline; the
root README contains the setup commands.

The current demonstration supports one DRAM channel and READ events. Geometry
is loaded from the selected DRAMsim3 INI file so both simulators decode the
same channel, rank, bank-group, bank, row, and column fields.

Example:

```sh
python3 integration/run_pipeline.py \
  --dramsim3-root ../DRAMsim3 \
  --run-name smoke_lpddr4_2400
```

The output hierarchy is:

```text
results/integration/<run-name>/
├── dramsim3/
│   ├── ch_0_bank_ready.trace
│   ├── dramsim3.json
│   └── run.log
├── bank_link/
│   ├── sweep.csv
│   ├── run.log
│   └── details/
└── manifest.json
```
