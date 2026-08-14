"""run_qc.py — Wrapper for running the qc compiler inside lit tests.

Solves three problems:
1. Strips comment lines (// and #) from .ql source files so that lit
   RUN/CHECK directives don't confuse the Quail lexer.
2. Runs qc from a temporary working directory that contains symlinks to
   the prelude/ and runtime/ directories so relative path lookups succeed.
3. Each invocation gets its own temp directory, avoiding races on the
   hardcoded "object.o" intermediate file when tests run in parallel.

Usage (from lit substitutions):
    python3 run_qc.py <workdir> <qc_binary> [qc arguments...]

<workdir> must contain prelude/ and runtime/ (or symlinks to them).
"""

import sys
import os
import subprocess
import tempfile
import shutil


def main():
    if len(sys.argv) < 3:
        print(
            "Usage: run_qc.py <workdir> <qc_binary> [qc arguments...]",
            file=sys.stderr,
        )
        sys.exit(1)

    workdir = sys.argv[1]
    qc_bin = sys.argv[2]
    args = sys.argv[3:]

    # Create a unique temp directory per test invocation to isolate the
    # hardcoded "object.o" intermediate file. Symlink prelude/ and runtime/
    # from the shared workdir.
    tmpdir = tempfile.mkdtemp(prefix="qc_test_", dir=workdir)

    prelude_src = os.path.join(workdir, "prelude")
    runtime_src = os.path.join(workdir, "runtime")
    os.symlink(os.path.realpath(prelude_src), os.path.join(tmpdir, "prelude"))
    os.symlink(os.path.realpath(runtime_src), os.path.join(tmpdir, "runtime"))

    new_args = []

    for arg in args:
        if arg.endswith(".ql") and os.path.exists(arg):
            # Strip lit directive comments from source file
            with open(arg, "r") as f:
                lines = [
                    line
                    for line in f
                    if not line.strip().startswith("//")
                    and not line.strip().startswith("#")
                ]
            clean_path = os.path.join(tmpdir, os.path.basename(arg))
            with open(clean_path, "w") as out:
                out.writelines(lines)
            new_args.append(clean_path)
        else:
            new_args.append(arg)

    try:
        res = subprocess.run([qc_bin] + new_args, cwd=tmpdir)
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)

    sys.exit(res.returncode)


if __name__ == "__main__":
    main()
