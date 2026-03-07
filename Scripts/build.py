import os
import sys
import json
import shlex
import argparse
import subprocess
from pathlib import Path
from copy import deepcopy
from contextlib import contextmanager
from dataclasses import dataclass, asdict

BUILDROOT = "Scratch"

# This is the project that editor/player will load by default.
LYRA_DEFAULT_PROJECT = Path(__file__).parents[1] / BUILDROOT / "project"
os.makedirs(LYRA_DEFAULT_PROJECT, exist_ok=True)
os.environ["LYRA_DEFAULT_PROJECT"] = str(LYRA_DEFAULT_PROJECT)

@dataclass
class BuildConfig:
    generator: str
    preset: str

@contextmanager
def config_file(mode):
    build_root = os.getcwd()
    build_root = os.path.join(build_root, BUILDROOT)
    build_root = os.path.abspath(build_root)
    os.makedirs(build_root, exist_ok=True)
    filename = os.path.join(build_root, "config.json")

    # make sure the config file exists
    if mode =="r":
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

def execute(args, cwd=os.getcwd(), env_vars={}):
    print(f">>> {shlex.join(args)}")
    environ = deepcopy(os.environ)
    environ.update(env_vars)
    proc = subprocess.run(args, env=environ, cwd=cwd, text=True, check=True, capture_output=True)
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
    config = load_config()
    preset = f"{config.generator}-{config.preset}"
    command = ["cmake", "--build", "--preset", preset, "--target", f"show-{args.target}"]
    proc = execute(command)
    info = proc.stdout.strip().splitlines()
    directory = info[-2]
    executable = os.path.join(directory, info[-1])
    command = [executable] + args.args
    print(">>> EXE:", executable)
    execute(command)

def do_test(args: argparse.Namespace):
    config = load_config()
    preset = f"{config.generator}-{config.preset}"
    command = ["cmake", "--build", "--preset", preset, "--target", "testkit"]
    env_vars = {}
    if args.target and args.target != "all":
        env_vars["LYRA_TESTKIT_FILTER"] = args.target
    execute(command, env_vars)

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
    except subprocess.SubprocessError:
        print(">>> Build Recipe Failed!")
        sys.exit(1)

if __name__ == "__main__":
    main()
