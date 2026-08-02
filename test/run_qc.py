import sys
import os
import subprocess
import tempfile

def main():
    if len(sys.argv) < 2:
        print("Usage: run_qc.py <qc_binary> [qc arguments...]", file=sys.stderr)
        sys.exit(1)

    qc_bin = sys.argv[1]
    args = sys.argv[2:]

    new_args = []
    temp_files = []

    for arg in args:
        if arg.endswith('.ql') and os.path.exists(arg):
            with open(arg, 'r') as f:
                lines = [line for line in f if not line.strip().startswith('//') and not line.strip().startswith('#')]
            tf = tempfile.NamedTemporaryFile(mode='w', suffix='.ql', delete=False)
            tf.writelines(lines)
            tf.close()
            new_args.append(tf.name)
            temp_files.append(tf.name)
        else:
            new_args.append(arg)

    res = subprocess.run([qc_bin] + new_args)

    for tf in temp_files:
        try:
            os.remove(tf)
        except OSError:
            pass

    sys.exit(res.returncode)

if __name__ == '__main__':
    main()
