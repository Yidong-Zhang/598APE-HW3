#!/usr/bin/env python3
"""Build a reported optimization stage in build/<version>/."""

import argparse
import os
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BASELINE = "15fe837"
VERSIONS = ("baseline", "opt1", "opt2", "opt3", "opt5", "opt6", "opt8", "opt9", "opt10")

OPT1 = """static void stabilize() {
  int changed = 0;
  do {
    changed = 0;
    for (int i = 0; i < N; ++i) {
      for (int j = row_begin[i]; j <= row_end[i]; ++j) {
        if (grid[i][j] < 4)
          continue;
        changed = 1;
        grid[i][j] -= 4;
        if (i > 0)
          grid[i - 1][j]++;
        if (j > 0)
          grid[i][j - 1]++;
        if (i < N - 1)
          grid[i + 1][j]++;
        if (j < N - 1)
          grid[i][j + 1]++;
      }
    }
  } while (changed);
}
"""

OPT2 = """static void stabilize() {
  int n = N;
  int changed = 0;
  do {
    changed = 0;
    for (int i = 0; i < n; ++i) {
      int *row = grid[i];
      int *up = i > 0 ? grid[i - 1] : NULL;
      int *down = i < n - 1 ? grid[i + 1] : NULL;
      int end = row_end[i];
      for (int j = row_begin[i]; j <= end; ++j) {
        if (row[j] < 4)
          continue;
        changed = 1;
        row[j] -= 4;
        if (up)
          up[j]++;
        if (j > 0)
          row[j - 1]++;
        if (down)
          down[j]++;
        if (j < n - 1)
          row[j + 1]++;
      }
    }
  } while (changed);
}
"""


def replace_once(source, old, new):
    if source.count(old) != 1:
        raise ValueError(f"Expected one occurrence of {old!r}")
    return source.replace(old, new, 1)


def source_for(version):
    if version == "baseline":
        return subprocess.check_output(
            ["git", "-C", str(ROOT), "show", f"{BASELINE}:sandpile.c"], text=True
        )

    source = (ROOT / "sandpile.c").read_text()
    if version in ("opt6", "opt8", "opt9", "opt10"):
        schedule = {"opt6": "static", "opt8": "static,1",
                    "opt9": "static,2", "opt10": "static,4"}[version]
        return replace_once(source, "schedule(static,4)", f"schedule({schedule})")

    body = OPT1 if version == "opt1" else OPT2
    if version == "opt3":
        body = replace_once(body, "row[j] -= 4;", "int q = row[j] / 4;\n        row[j] -= 4 * q;")
        for cell in ("up[j]", "row[j - 1]", "down[j]", "row[j + 1]"):
            body = replace_once(body, f"{cell}++;", f"{cell} += q;")
    elif version == "opt5":
        body = replace_once(body, "int n = N;", "int n = N;\n  int rows = n % 2 ? n / 2 + 1 : n;")
        body = replace_once(body, "i < n;", "i < rows;")
        body = replace_once(body, "i < n - 1 ? grid[i + 1]", "i + 1 < rows ? grid[i + 1]")
        body = replace_once(body, "int end = row_end[i];",
                            "int down_grains = n % 2 && i == n / 2 - 1 ? 2 : 1;\n      int end = row_end[i];")
        body = replace_once(body, "down[j]++;", "down[j] += down_grains;")
        body = replace_once(body, "  } while (changed);\n}", """  } while (changed);
  if (n % 2) {
    for (int i = 0; i < n / 2; ++i)
      memcpy(grid[n - 1 - i], grid[i], (size_t)n * sizeof(int));
  }
}""")

    start = source.index("static void stabilize() {")
    end = source.index("void find_identity()", start)
    return source[:start] + body + "\n" + source[end:]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("version", choices=VERSIONS)
    args = parser.parse_args()
    source = source_for(args.version)
    output = ROOT / "build" / args.version
    output.mkdir(parents=True, exist_ok=True)
    source_path = output / "sandpile.c"
    source_path.write_text(source)
    command = shlex.split(os.environ.get("CC", "gcc"))
    command += [str(source_path), "-I", str(ROOT), "-O3", "-g", "-Werror"]
    if args.version in ("opt6", "opt8", "opt9", "opt10"):
        command.append("-fopenmp")
    command += ["-lm", "-o", str(output / "sandpile.exe")]
    subprocess.run(command, check=True)
    print(f"Built build/{args.version}/sandpile.exe")


if __name__ == "__main__":
    main()
