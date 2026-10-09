import os
import sys
import json
import shlex
import shutil
import argparse
import subprocess
from pathlib import Path
from copy import deepcopy
from contextlib import contextmanager
from dataclasses import dataclass, asdict

PROJECT_ROOT = Path(__file__).resolve().parents[2]
BUILD_DIR = PROJECT_ROOT / "Scratch"

def ensure_windows_msvc_env():
    if sys.platform != "win32":
        return
    if shutil.which("cl.exe"):
        return
    vswhere = os.path.expandvars(r"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe")
    if not os.path.exists(vswhere):
        print(">>> Warning: Visual Studio Installer (vswhere.exe) not found. Cannot locate MSVC environment.")
        return
    try:
        proc = subprocess.run([vswhere, "-latest", "-property", "installationPath"], capture_output=True, text=True, check=True)
        vs_path = proc.stdout.strip()
        if not vs_path:
            print(">>> Warning: No Visual Studio installation found. Please install Visual Studio with 'Desktop development with C++'.")
            return
        vcvars = os.path.join(vs_path, "VC", "Auxiliary", "Build", "vcvars64.bat")
        if not os.path.exists(vcvars):
            print(f">>> Warning: MSVC x64 toolset not found at '{vcvars}'. Ensure the 'MSVC v143 - VS 2022 C++ x64/x86 build tools' component is installed.")
            return
        proc = subprocess.run(["cmd.exe", "/c", vcvars, "&", "set"], capture_output=True, text=True, check=True)
        for line in proc.stdout.splitlines():
            if "=" in line and not line.startswith("="):
                k, v = line.split("=", 1)
                os.environ[k] = v
        print(f">>> Initialized MSVC environment from: {vs_path}")
    except Exception as e:
        print(f">>> Warning: Failed to initialize MSVC environment: {e}")

ensure_windows_msvc_env()

# This is the project that editor/player will load by default.
LYRA_DEFAULT_PROJECT = BUILD_DIR / "project"
os.makedirs(LYRA_DEFAULT_PROJECT, exist_ok=True)
os.environ["LYRA_DEFAULT_PROJECT"] = str(LYRA_DEFAULT_PROJECT)

@dataclass
class BuildConfig:
    generator: str
    preset: str

@contextmanager
def config_file(mode):
    build_root = BUILD_DIR
    os.makedirs(build_root, exist_ok=True)
    filename = build_root / "config.json"

    # make sure the config file exists
    if mode == "r":
        if not os.path.exists(filename):
            with open(filename, "w") as f:
                print("{}", file=f)

    # open the file in requested mode
    with open(filename, mode) as f:
        yield f

def load_config():
    with config_file("r") as f:
        data = json.load(f)
        return BuildConfig(**data)

def save_config(config: BuildConfig):
    with config_file("w") as f:
        json.dump(asdict(config), f, indent=2)

def execute(args, env_vars={}, **kwargs):
    print(f">>> {shlex.join(args)}")
    environ = deepcopy(os.environ)
    environ.update(env_vars)
    if "env" not in kwargs:
        kwargs["env"] = environ
    proc = subprocess.run(args, **kwargs)
    return proc

def do_config(args: argparse.Namespace):
    config = BuildConfig(generator=args.generator, preset=args.preset)
    save_config(config)
    command = ["cmake", "--preset", config.generator]
    execute(command)
    print(f">>> Project configuration complete!")
    print(f">>> {config}")

def do_switch(args: argparse.Namespace):
    config = load_config()
    config.preset = args.preset
    save_config(config)
    print(f">>> Project configuration updated!")
    print(f">>> {config}")

def do_build(args: argparse.Namespace):
    config = load_config()
    preset = f"{config.generator}-{config.preset}"
    command = ["cmake", "--build", "--preset", preset]
    if args.target and args.target != "all":
        command.extend(["--target", f"lyra-{args.target}"])
    execute(command)

def do_run(args: argparse.Namespace):
    do_build(args)
    config = load_config()
    preset = f"{config.generator}-{config.preset}"
    command = ["cmake", "--build", "--preset", preset, "--target", f"show-{args.target}"]
    proc = execute(command, check=True, text=True, capture_output=True)
    info = proc.stdout.strip().splitlines()
    directory = info[-2].strip()
    executable = info[-1].strip()
    executable = os.path.join(directory, executable)
    command = [executable] + args.args
    print(">>> EXE:", executable)

    # run from BUILD_DIR to avoid cluttering project root
    execute(command, cwd=str(BUILD_DIR))

def do_test(args: argparse.Namespace):
    config = load_config()
    preset = f"{config.generator}-{config.preset}"
    target = args.target if args.target in ("unit-tests", "rhi-tests", "testkit") else "testkit"
    command = ["cmake", "--build", "--preset", preset, "--target", target]
    env_vars = {}
    if args.target and args.target not in ("all", "unit-tests", "rhi-tests", "testkit"):
        env_vars["LYRA_TESTKIT_FILTER"] = args.target
    execute(command, env_vars)

def do_amalgamate(args: argparse.Namespace):
    script_path = PROJECT_ROOT / "Tools" / "Scripts" / "amalgamate.py"
    cmd = [sys.executable, str(script_path)]
    if args.check:
        cmd.append("--check")
    execute(cmd)

def do_format(args: argparse.Namespace):
    import shutil

    clang_format = shutil.which("clang-format")
    if not clang_format:
        print(">>> Error: 'clang-format' executable not found in PATH.")
        sys.exit(1)

    try:
        res = subprocess.check_output(
            ["git", "ls-files", "--", "*.h", "*.hpp", "*.c", "*.cpp", "*.inl"],
            cwd=PROJECT_ROOT,
            text=True,
        )
        all_files = [f for f in res.splitlines() if f.strip()]
    except Exception:
        # Fallback to filesystem scan if git is not available
        extensions = {".h", ".hpp", ".c", ".cpp", ".inl"}
        all_files = [
            str(p.relative_to(PROJECT_ROOT))
            for p in PROJECT_ROOT.rglob("*")
            if p.suffix in extensions
        ]

    exclude_patterns = {"ThirdParty", "Vendors", "external", "_deps", "Scratch", "build"}
    target_files = [
        str(PROJECT_ROOT / f)
        for f in all_files
        if not any(pattern in f for pattern in exclude_patterns)
    ]

    if not target_files:
        print(">>> No files found to format.")
        return

    base_cmd = [clang_format]
    if args.check:
        base_cmd += ["--dry-run", "--Werror"]
    else:
        base_cmd.append("-i")

    # Run clang-format in batches to avoid command line length limits
    batch_size = 50
    total = len(target_files)
    print(f">>> Formatting {total} files using {clang_format}...")

    for i in range(0, total, batch_size):
        batch = target_files[i:i + batch_size]
        cmd = base_cmd + batch
        proc = subprocess.run(cmd)
        if proc.returncode != 0:
            print(f">>> clang-format failed with exit code {proc.returncode}")
            sys.exit(proc.returncode)

    print(">>> Formatting complete.")

def parse_args():
    parser = argparse.ArgumentParser("Lyra Build Helper")
    subparsers = parser.add_subparsers(dest="mode")

    # just config preset
    config_parser = subparsers.add_parser("config")
    config_parser.add_argument("generator")
    config_parser.add_argument("preset")

    # just switch mode
    switch_parser = subparsers.add_parser("switch")
    switch_parser.add_argument("preset")

    # just build target
    build_parser = subparsers.add_parser("build")
    build_parser.add_argument("--target", default=None)

    # just run target
    test_parser = subparsers.add_parser("run")
    test_parser.add_argument("--target")
    test_parser.add_argument("args", nargs="*")

    # just test target
    test_parser = subparsers.add_parser("test")
    test_parser.add_argument("--target", default=None)

    # just amalgamate headers
    amalgamate_parser = subparsers.add_parser("amalgamate")
    amalgamate_parser.add_argument("--check", action="store_true", default=False)

    # just format code
    format_parser = subparsers.add_parser("format")
    format_parser.add_argument("--check", action="store_true", default=False)

    return parser.parse_args()

def main():
    args = parse_args()

    try:
        if args.mode == "config":
            do_config(args)

        elif args.mode == "switch":
            do_switch(args)

        elif args.mode == "build":
            do_build(args)

        elif args.mode == "run":
            do_run(args)

        elif args.mode == "test":
            do_test(args)

        elif args.mode == "amalgamate":
            do_amalgamate(args)

        elif args.mode == "format":
            do_format(args)
    except subprocess.SubprocessError:
        print(">>> Build Recipe Failed!")
        sys.exit(1)

if __name__ == "__main__":
    main()
