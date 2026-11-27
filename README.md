# 📡 Opener Samples

Reference applications and sample projects for the **Opener DECT-2020 NR protocol stack implementation**.

> **Note**
>
> This repository contains reference applications built on top of the
> [Opener DECT-2020 NR protocol stack implementation](https://github.com/Opener-Initiative/opener),
> which is provided under the Apache 2.0 license.

[[*TOC*]]

---

# 🚀 Getting Started

## 📋 Prerequisites

Before building the samples, install the following tools:

1. **SEGGER J-Link Software and Documentation Pack**

   * https://www.segger.com/downloads/jlink/

2. **Zephyr Development Environment**

   Follow these steps from the Zephyr *Getting Started* guide:

   * Select and update your operating system
   * Install all required dependencies

   https://docs.zephyrproject.org/latest/develop/getting_started/index.html

3. **nRF Util** with the `device` command (used for flashing the modem firmware and applications)

   * https://www.nordicsemi.com/Products/Development-tools/nRF-Util
   * `nrfutil install device`

## ⚙️ Repository Setup

Create the Python virtual environment as `.venv` in the repository root and initialize the west workspace:

```bash
python3 -m venv .venv
source .venv/bin/activate   # Windows: .venv\Scripts\activate

pip install west

west init -l samples
west update
west zephyr-export
west packages pip --install -- -c constraints.txt
west sdk install
```

`west sdk install` downloads and installs the Zephyr SDK version that matches the Zephyr revision in `samples/west.yml`, so no separate SDK installation is needed.

## 📟 Board Setup

Program the DECT NR+ modem firmware once per board:

```bash
nrfutil device program --firmware <nr+-modem-firmware.zip>
```

The DECT NR+ PHY modem firmware is available from Nordic Semiconductor on request.

With several J-Links connected, select one with `--serial-number <serial>`.

For the Codium boards (connection and first-time setup), see the [README in codz-sdk](https://github.com/codium-electronique/codz-sdk/blob/main/README.md), which west checks out in `deps/codz-sdk/`.

---

# 🔨 Building and Flashing

Samples can be built with the helper script or with plain west. Both write to the same build folder, `build/<board>/<sample>`, so you can switch between them, e.g. build with the helper script and then flash with west. Only the helper script tracks the active target and updates the copies of its build outputs that VS Code uses for debugging and IntelliSense (see the VS Code section).

## Helper Script

`scripts/target.py` stores the active target (sample + board) in `build/active.json`. All further commands, and the VS Code tasks, work on that target. Only the boards listed under `platform_allow` in a sample's `sample.yaml` can be selected for that sample.

```bash
python scripts/target.py select           # select sample and board from a menu
python scripts/target.py show             # show the active target
python scripts/target.py build            # build the active target
python scripts/target.py build -p always  # pristine build
python scripts/target.py flash            # flash the active target
python scripts/target.py rtt              # show the RTT output
python scripts/target.py debug            # flash, then debug with gdb in the terminal
python scripts/target.py attach           # debug with gdb in the terminal, without flashing
python scripts/target.py build-all        # build all samples for all their boards
```

`select` also accepts the sample and board directly, e.g. `select dectnrp-driver cod_nrp_node/nrf9151/ns`. Further arguments to the other commands are passed to west, e.g. `build -- -DCONFIG_LOG=n` or `flash --dev-id <serial>`. `python scripts/target.py --help` lists all commands.

## Plain West

Build a sample application using:

```bash
west build -p always -b <board-name> -d <build-folder> samples/<example>/
```

For example following command builds dectnrp-driver sample:

```bash
west build -p always -b nrf9151dk/nrf9151/ns -d build/nrf9151dk/nrf9151/ns/dectnrp-driver/ samples/dectnrp-driver/
```

Flash it with:

```bash
west flash -d build/nrf9151dk/nrf9151/ns/dectnrp-driver/
```

The same `-d` option can also be passed to `west debug`, `west attach` and `west rtt`. With several J-Links connected, select one with `--dev-id <serial>`.

## ⚡ Optional: Configure West

To simplify future build commands, configure **west** once:

```bash
west config build.dir-fmt "{source_dir}/../../build/{board}/{app}"
west config build.pristine auto
west config build.sysbuild true
```

These settings are stored in:

```text
.west/config
```

Afterwards, builds can be started with a shorter command, which builds into the same folder as above:

```bash
west build -b nrf9151dk/nrf9151/ns samples/dectnrp-driver/
```

For additional configuration options, refer to the Zephyr documentation:

https://docs.zephyrproject.org/latest/develop/west/build-flash-debug.html#configuration-options

---

# 💻 VS Code

Open `opener-samples.code-workspace` and install the recommended extensions. Building and flashing only need the Python virtual environment in `.venv`; IntelliSense needs **clangd**, debugging needs **Cortex-Debug**.

* **Tasks**: run them from *Terminal → Run Task* or via *Tasks: Run Task* from the Command Palette (Ctrl+Shift+P, Cmd+Shift+P on macOS).
  * *Select target*: choose the sample and board to work on.
  * *Build* (Ctrl+Shift+B, Cmd+Shift+B on macOS): build the active target.
  * *Pristine build*: build the active target from scratch.
  * *Build all*: build every sample for all of its allowed boards.
  * *Flash*: flash the active target.
  * *RTT console*: open the RTT console of the active target.
  * *Format current file*: run clang-format on the open C source/header file.
* **Debugging**: choose a configuration in the *Run and Debug* view (Ctrl+Shift+D, Cmd+Shift+D on macOS), then press F5.
  * *Debug (nRF9151)*: build and flash the active target, then stop at `main`.
  * *Attach (nRF9151)*: attach to the firmware already running on the board.

Running `target.py select` on the command line also changes the active target in VS Code, and vice versa. clangd and the debug configurations follow the active target too: they use copies of its build outputs in `build/` (`compile_commands.json` and `active.*`), which only the helper script updates, on `select` and with each build. After building with plain west, run the *Build* task once to update them. A target that has not been built yet has no copies, so clangd has no compile database until its first build.

The debug configurations take the Zephyr SDK from `opener.zephyrSdkPath` in `.vscode/settings.json`, which points to the default location of `west sdk install`. If your SDK is installed elsewhere, change the value accordingly. Setting it in your user settings has no effect, because the value in `.vscode/settings.json` takes precedence.

---

# 🧩 Adding Samples, Boards and Chips

## New Sample

* Add `samples/<name>/` with a `sample.yaml` that lists the supported boards in `platform_allow`.
* For each chip and board the sample supports, add the matching `socs/` and `boards/` files (copy them from `samples/dectnrp-driver/`).
* The helper script, the VS Code tasks and `build-all` pick it up automatically.

## New Board

For boards with an already supported chip, e.g. nRF9151:

* The board definition must be available, in Zephyr/NCS or in a module from `samples/west.yml` (like `codz-sdk`).
* Add the board to `platform_allow` of each sample it runs on.
* Add `boards/<board>_<qualifiers>.overlay` to a sample to adjust the board's flash and SRAM layout (see the existing overlays). The layout can differ between boards; e.g. the Thingy:91 X has less SRAM reserved for the modem than the nRF9151 DK. Chip settings come from the sample's `socs/` automatically.
* Flashing, debugging and the J-Link script come from the board definition; the helper script and `.vscode/launch.json` need no changes.

## New Chip

* Add `socs/<soc>_<qualifiers>.conf`/`.overlay` to a sample for its chip-specific settings.
* Add a *Debug*/*Attach* pair for the chip to `.vscode/launch.json`, with its `device`, `svdPath` and toolchain.

---

# 📄 License

Copyright (c) 2026 Opener Initiative contributors

Licensed under the **Apache License, Version 2.0** (the "License"); you may not use this file except in compliance with the License.

You may obtain a copy of the License at:

https://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an **"AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND**, either express or implied.

See the License for the specific language governing permissions and limitations under the License.

---

# ⚠️ Disclaimer

This software is provided as a **reference implementation** for research and development purposes.

It is distributed in the hope that it will be useful, but **without any warranty**, including without limitation the implied warranties of **merchantability** or **fitness for a particular purpose**.

In no event shall the authors or copyright holders be liable for any claim, damages, or other liability, whether in an action of contract, tort, or otherwise, arising from, out of, or in connection with the software or the use of this software.
