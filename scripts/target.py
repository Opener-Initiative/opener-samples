# SPDX-License-Identifier: Apache-2.0
"""Select, build, flash and debug the active target (sample + board).

The active target is stored in build/active.json and shared between the
command line and VS Code. All commands call west with the target's build
directory, build/<board>/<sample>. Boards are taken from platform_allow
in samples/<sample>/sample.yaml.

After each build and on selection, the active target's compile database,
ELF, hex file and J-Link script are copied to fixed paths in build/, for
clangd and the debug configurations in .vscode/launch.json.

Further arguments are passed to west unchanged, e.g.:

  python scripts/target.py select
  python scripts/target.py select dectnrp-driver cod_nrp_node/nrf9151/ns
  python scripts/target.py build
  python scripts/target.py build -p always
  python scripts/target.py build -- -DCONFIG_LOG=n
  python scripts/target.py flash --dev-id <J-Link serial number>
  python scripts/target.py build-all -p always
"""

import argparse
import json
import os
import re
import shlex
import subprocess
import sys
import tempfile
from pathlib import Path

import yaml

REPO = Path(__file__).resolve().parent.parent
SAMPLES = REPO / "samples"
BUILD = REPO / "build"
STATE_FILE = BUILD / "active.json"
# Copies of the active target's build outputs, used by clangd (see .clangd)
# and the debug configurations (see .vscode/launch.json)
COMPILE_COMMANDS = BUILD / "compile_commands.json"
ACTIVE_ELF = BUILD / "active.elf"
ACTIVE_HEX = BUILD / "active.hex"
ACTIVE_JLINK_SCRIPT = BUILD / "active.JLinkScript"
# Written to ACTIVE_JLINK_SCRIPT for boards without a J-Link script
NO_JLINK_SCRIPT = b"// The active target's board has no J-Link script (scripts/target.py)\n"

# Commands that are passed to west together with the target's build directory
RUNNER_COMMANDS = {
    "flash": "flash the active target",
    "debug": "flash the active target and debug it with gdb in the terminal",
    "attach": "debug the active target with gdb in the terminal, without flashing",
    "debugserver": "start a GDB server for the active target",
    "rtt": "show the active target's RTT output",
}


class TargetError(Exception):
    pass


def rel(path):
    """Return path relative to the repository, with forward slashes."""
    return Path(os.path.relpath(path, REPO)).as_posix()


def run_west(args):
    print(f"==> west {shlex.join(args)}", flush=True)
    return subprocess.call([sys.executable, "-m", "west", *args], cwd=REPO)


def find_samples():
    """Return {sample: contents of sample.yaml} for all samples."""
    samples = {}
    for sample_yaml in sorted(SAMPLES.glob("*/sample.yaml")):
        with open(sample_yaml, encoding="utf-8") as f:
            samples[sample_yaml.parent.name] = yaml.safe_load(f) or {}
    return samples


def allowed_boards(sample_meta):
    """Return the boards in platform_allow (common section and all tests)."""
    sections = [sample_meta.get("common")]
    sections += (sample_meta.get("tests") or {}).values()
    boards = []
    for section in sections:
        allow = (section or {}).get("platform_allow") or []
        if isinstance(allow, str):
            allow = allow.split()
        for board in allow:
            if board not in boards:
                boards.append(board)
    return boards


def board_full_names(boards):
    """Return {board: full name} from the board definitions (board.yml)."""
    names = {board: board.split("/")[0].split("@")[0] for board in boards}
    full_names = {}
    if names:
        pattern = "^(" + "|".join(re.escape(n) for n in set(names.values())) + ")$"
        result = subprocess.run(
            [sys.executable, "-m", "west", "boards", "-n", pattern, "-f", "{name}|{full_name}"],
            cwd=REPO, capture_output=True, text=True)
        if result.returncode == 0:
            for line in result.stdout.splitlines():
                name, _, full_name = line.partition("|")
                full_names[name] = full_name.strip()
    return {board: full_names.get(name, "") for board, name in names.items()}


def describe(state):
    if state.get("board_name"):
        return f"{state['board_name']} ({state['board']}) / {state['sample']}"
    return f"{state['board']} / {state['sample']}"


def build_dir(state):
    return BUILD / state["board"] / state["sample"]


def check_sample(samples, sample):
    if sample not in samples:
        raise TargetError(f"unknown sample '{sample}' (available: {', '.join(samples)})")


def check_target(samples, sample, board):
    check_sample(samples, sample)
    boards = allowed_boards(samples[sample])
    if board not in boards:
        raise TargetError(f"board '{board}' is not in platform_allow of "
                          f"samples/{sample}/sample.yaml (allowed: {', '.join(boards)})")


def choose(title, options, current=None):
    """Let the user pick one of options [(value, label)] in the terminal."""
    if len(options) == 1:
        print(f"{title}: {options[0][1]}")
        return options[0][0]
    print(f"{title}:")
    default = None
    for i, (value, label) in enumerate(options, 1):
        mark = ""
        if value == current:
            default = i
            mark = "  (current)"
        print(f"  {i}) {label}{mark}")
    hint = f"1-{len(options)}" + (f", Enter = {default}" if default else "")
    while True:
        answer = input(f"Select [{hint}]: ").strip()
        if not answer and default:
            return options[default - 1][0]
        if answer.isdigit() and 1 <= int(answer) <= len(options):
            return options[int(answer) - 1][0]
        print("Invalid selection.")


def load_state():
    try:
        with open(STATE_FILE, encoding="utf-8") as f:
            return json.load(f)
    except FileNotFoundError:
        return None


def write_atomic(dst, data):
    """Write data to dst so that readers never see a partially written file."""
    fd, tmp = tempfile.mkstemp(dir=dst.parent, prefix=dst.name + ".")
    try:
        with os.fdopen(fd, "wb") as f:
            f.write(data)
        os.chmod(tmp, 0o644)
        os.replace(tmp, dst)
    except BaseException:
        os.unlink(tmp)
        raise


def load_yaml(path):
    try:
        with open(path, encoding="utf-8") as f:
            return yaml.safe_load(f) or {}
    except FileNotFoundError:
        return None


def jlink_script(runners):
    """Return the J-Link script from the board's jlink runner arguments."""
    for arg in (runners.get("args") or {}).get("jlink") or []:
        if arg.startswith("--tool-opt="):
            opts = shlex.split(arg[len("--tool-opt="):])
            for opt, value in zip(opts, opts[1:]):
                if opt.lower() == "-jlinkscriptfile":
                    return Path(value)
    return None


def sync_build_outputs(state):
    """Copy the target's build outputs to their fixed paths in build/.

    Copies that the target has no build output for are removed, so that
    they never belong to a different target.
    """
    bdir = build_dir(state)
    domain = (load_yaml(bdir / "domains.yaml") or {}).get("default", state["sample"])
    zephyr_dir = bdir / domain / "zephyr"
    data = {}

    compile_commands = bdir / domain / "compile_commands.json"
    if compile_commands.is_file():
        data[COMPILE_COMMANDS] = compile_commands.read_bytes()

    # runners.yaml names the files that west flash and west debug use
    runners = load_yaml(zephyr_dir / "runners.yaml")
    if runners is not None:
        config = runners.get("config") or {}
        for dst, key in ((ACTIVE_ELF, "elf_file"), (ACTIVE_HEX, "hex_file")):
            src = zephyr_dir / config[key] if config.get(key) else None
            if src is not None and src.is_file():
                data[dst] = src.read_bytes()
        script = jlink_script(runners)
        data[ACTIVE_JLINK_SCRIPT] = script.read_bytes() if script else NO_JLINK_SCRIPT

    for dst in (COMPILE_COMMANDS, ACTIVE_ELF, ACTIVE_HEX, ACTIVE_JLINK_SCRIPT):
        if dst in data:
            write_atomic(dst, data[dst])
        else:
            dst.unlink(missing_ok=True)


def select(sample=None, board=None):
    samples = find_samples()
    if not samples:
        raise TargetError("no samples found (samples/*/sample.yaml)")
    current = load_state() or {}

    if sample is None:
        options = []
        for name, meta in samples.items():
            description = (meta.get("sample") or {}).get("description")
            options.append((name, f"{name} - {description}" if description else name))
        sample = choose("Sample", options, current.get("sample"))
    else:
        check_sample(samples, sample)

    if board is None:
        boards = allowed_boards(samples[sample])
        if not boards:
            raise TargetError(f"samples/{sample}/sample.yaml has no platform_allow")
        names = board_full_names(boards)
        width = max(len(n) for n in names.values())
        options = [(b, f"{names[b]:<{width}}  ({b})" if names[b] else b) for b in boards]
        board = choose("Board", options, current.get("board"))
    else:
        check_target(samples, sample, board)
        names = board_full_names([board])

    state = {"sample": sample, "board": board, "board_name": names[board]}
    BUILD.mkdir(exist_ok=True)
    with open(STATE_FILE, "w", encoding="utf-8") as f:
        json.dump(state, f, indent=2)
        f.write("\n")
    # If the target was built before, clangd and debugging follow the
    # selection right away
    sync_build_outputs(state)
    print(f"Active target: {describe(state)}")
    return state


def active_target():
    """Return the active target; ask for one if none is selected yet."""
    state = load_state()
    if state is None:
        if not sys.stdin.isatty():
            raise TargetError("no active target, run: python scripts/target.py select")
        print("No active target selected yet.")
        return select()
    try:
        check_target(find_samples(), state["sample"], state["board"])
    except TargetError as e:
        raise TargetError(f"active target is no longer valid: {e}; "
                          "run: python scripts/target.py select") from None
    print(f"==> Target: {describe(state)}", flush=True)
    return state


def build_args(state, west_args):
    # west_args last, so that CMake arguments after "--" work
    return ["build", "--sysbuild", "-b", state["board"], "-d", rel(build_dir(state)),
            rel(SAMPLES / state["sample"])] + west_args


def cmd_select(args, west_args):
    select(args.sample, args.board)
    return 0


def cmd_show(args, west_args):
    state = load_state()
    if state is None:
        print("No active target selected.")
        return 1
    bdir = build_dir(state)
    built = (bdir / "CMakeCache.txt").is_file()
    print(f"Active target:   {describe(state)}")
    print(f"Build directory: {rel(bdir)}" + ("" if built else " (not built yet)"))
    return 0


def cmd_build(args, west_args):
    state = active_target()
    returncode = run_west(build_args(state, west_args))
    sync_build_outputs(state)
    return returncode


def cmd_build_all(args, west_args):
    results = []
    for sample, meta in find_samples().items():
        for board in allowed_boards(meta):
            state = {"sample": sample, "board": board}
            results.append((sample, board, run_west(build_args(state, west_args))))
    active = load_state()
    if active is not None:
        sync_build_outputs(active)

    print("\n==> Summary:")
    for sample, board, returncode in results:
        print(f"  {'ok    ' if returncode == 0 else 'FAILED'}  {board} / {sample}")
    return 0 if all(returncode == 0 for _, _, returncode in results) else 1


def cmd_runner(args, west_args):
    state = active_target()
    bdir = build_dir(state)
    if not (bdir / "CMakeCache.txt").is_file():
        raise TargetError(f"{rel(bdir)} has not been built yet, "
                          "run: python scripts/target.py build")
    return run_west([args.command, "-d", rel(bdir)] + west_args)


def main():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter,
        allow_abbrev=False)
    commands = parser.add_subparsers(dest="command", required=True, metavar="command")

    p = commands.add_parser("select", allow_abbrev=False,
                            help="select the active target (interactively without arguments)")
    p.add_argument("sample", nargs="?", help="sample directory name, e.g. dectnrp-driver")
    p.add_argument("board", nargs="?", help="board target, e.g. nrf9151dk/nrf9151/ns")
    p.set_defaults(func=cmd_select, passthrough=False)

    p = commands.add_parser("show", allow_abbrev=False, help="show the active target")
    p.set_defaults(func=cmd_show, passthrough=False)

    p = commands.add_parser("build", allow_abbrev=False, help="build the active target")
    p.set_defaults(func=cmd_build, passthrough=True)

    p = commands.add_parser("build-all", allow_abbrev=False,
                            help="build all samples for all boards in their platform_allow")
    p.set_defaults(func=cmd_build_all, passthrough=True)

    for name, help_text in RUNNER_COMMANDS.items():
        p = commands.add_parser(name, allow_abbrev=False, help=help_text)
        p.set_defaults(func=cmd_runner, passthrough=True)

    args, west_args = parser.parse_known_args()
    if west_args and not args.passthrough:
        parser.error(f"unrecognized arguments: {' '.join(west_args)}")

    try:
        return args.func(args, west_args)
    except TargetError as e:
        print(f"error: {e}", file=sys.stderr)
        return 1
    except EOFError:
        print("\naborted", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        return 130


if __name__ == "__main__":
    sys.exit(main())
