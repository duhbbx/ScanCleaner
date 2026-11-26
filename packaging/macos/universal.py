"""Combine the two verified application executables and validate every bundle binary."""
import argparse
from pathlib import Path
import subprocess

MACHO_MAGIC = {
    bytes.fromhex(value) for value in
    ("feedface", "cefaedfe", "feedfacf", "cffaedfe",
     "cafebabe", "bebafeca", "cafebabf", "bfbafeca")
}

def run(*args):
    return subprocess.check_output([str(arg) for arg in args], text=True)

def verify(bundle, architectures):
    for path in bundle.rglob("*"):
        if path.is_symlink():
            if not path.exists() or not path.resolve().is_relative_to(bundle.resolve()):
                raise ValueError(f"Unexpected bundle symlink: {path}")
            continue
        if not path.is_file():
            continue
        with path.open("rb") as binary:
            if binary.read(4) not in MACHO_MAGIC:
                continue
        actual = run("lipo", "-archs", path).split()
        if any(arch not in actual for arch in architectures):
            raise ValueError(f"{path} has architectures {actual}, expected {architectures}")
    run("codesign", "--verify", "--deep", "--strict", bundle)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("bundle", type=Path)
    parser.add_argument("--arch", nargs="+", required=True)
    parser.add_argument("--intel", type=Path)
    args = parser.parse_args()
    if args.intel:
        executable = args.bundle / "Contents/MacOS/ScanCleaner"
        merged = executable.with_name("ScanCleaner-universal")
        run("lipo", "-create", args.intel / "Contents/MacOS/ScanCleaner", executable, "-output", merged)
        merged.chmod(executable.stat().st_mode)
        merged.replace(executable)
        run("codesign", "--force", "--sign", "-", "--timestamp=none", executable)
        run("codesign", "--force", "--sign", "-", "--timestamp=none", args.bundle)
    verify(args.bundle, args.arch)

if __name__ == "__main__":
    main()
