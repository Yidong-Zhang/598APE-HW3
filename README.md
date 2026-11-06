# CS598APE HW3: Abelian Sandpile

Compute the identity sandpile on a 1001 × 1001 Pacman grid. `sandpile.c`
contains the final version, Opt10.

## Requirements

Use a Linux/Unix system with Git, GNU Make, GCC with OpenMP support,
Python 3 (for the version builder), and `/usr/bin/time`.
On Ubuntu:

```bash
sudo apt update
sudo apt install build-essential python3 git time
```

The bundled `stb_image_write.h` handles PNG output; no separate image library
is needed. Benchmarks used a course VM with 4 vCPUs, Intel Xeon Silver 4216
@ 2.10 GHz, and GCC 15.2.0. Runtime varies across machines.

## Build, run, and verify the final version

Start with a full clone (the stage builder uses the baseline commit history):

```bash
git clone https://github.com/Yidong-Zhang/598APE-HW3.git
cd 598APE-HW3
```

From the repository root:

```bash
make all
OMP_NUM_THREADS=4 /usr/bin/time -p ./sandpile.exe 1001
cmp baseline.png identity.png
```

The program writes `identity.png`; `cmp` produces no output when it matches
the baseline. `opt10.png` is the saved final reference output. Each run
overwrites `identity.png`. Use `make clean` to remove the executable.

For the paper's timing method, run the timed command five times and report
the smallest `real` value. Timing includes initialization, stabilization,
PNG generation, and cleanup. Profiling runs are measured separately.

## Reproduce the baseline and optimization stages

The builder generates separate sources and executables under `build/`.
The baseline comes from commit `15fe837`; intermediate stages reconstruct
the changes described below. It leaves the final `sandpile.c` in place.

First build and measure the original baseline:

```bash
python3 tools/build_variant.py baseline
/usr/bin/time -p ./build/baseline/sandpile.exe 1001
cp identity.png build/baseline/identity.png
cmp baseline.png build/baseline/identity.png
```

Then build any stage by its name. For example:

```bash
python3 tools/build_variant.py opt1
OMP_NUM_THREADS=4 /usr/bin/time -p ./build/opt1/sandpile.exe 1001
cp identity.png build/opt1/identity.png
cmp baseline.png build/opt1/identity.png
```

Replace `opt1` with a version below. Repeat each timed command five times
and compare the minimum `real` values. `OMP_NUM_THREADS` affects only the
parallel versions.

| Version | Based on | Change | Reported minimum (s) |
| --- | --- | --- | ---: |
| baseline | — | Original code | 614.80 |
| opt1 | baseline | Precompute each row's active interval | 302.16 |
| opt2 | opt1 | Cache scan invariants and row pointers | 268.43 |
| opt3 | opt2 | Batch topplings; reverted after slowdown | 285.96 |
| opt5 | opt2 | Fold the computation using up-down symmetry | 136.96 |
| opt6 | opt5 | Three row groups; OpenMP `static` schedule | 53.09 |
| opt8 | opt6 | Change schedule to `static,1` | 42.86 |
| opt9 | opt8 | Change schedule to `static,2` | 42.25 |
| opt10 | opt9 | Change schedule to `static,4` | 40.77 |

Parallel timings above use 4 threads. To reproduce Opt6 thread scaling,
build `opt6` and run it with `OMP_NUM_THREADS=1`, `2`, and `4`.
The corresponding reported minima are 155.37, 86.61, and 53.09 seconds.
Opt4 (active-cell queue) and Opt7 (persistent parallel region) had no valid
completed target timings; the Opt7 implementation also had a synchronization
flaw. They are excluded from the runnable stages and final code.

Precomputed bounds assume a fixed domain with a contiguous active interval
per row. Symmetry folding applies to the symmetric identity construction
for odd `N`; even sizes use a full scan. The parallel phases synchronize
between row groups with nonoverlapping write ranges.

## Optional profiling

On Linux with `perf` installed, profile the final version with:

```bash
sudo perf record -g -- env OMP_NUM_THREADS=4 ./sandpile.exe 1001
sudo perf --no-pager report --stdio --no-children --call-graph none --percent-limit 1
```

Use `sudo` only if the VM's perf permissions require it. The original
application is from [PRONTOLab/598APE-HW3-F26](https://github.com/PRONTOLab/598APE-HW3-F26).
