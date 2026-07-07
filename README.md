# 📡 DECT NR+ Opener Samples

Reference applications and sample projects for the **Opener DECT-2020 NR+ protocol stack**.

> **Note**
>
> This repository contains reference applications built on top of the
> [Opener DECT-2020 NR+ protocol stack](https://github.com/Opener-Initiative/opener),
> which is licensed under the Apache License 2.0.

[[*TOC*]]

---

# 🚀 Getting Started

## 📋 Prerequisites

Before building the samples, install the following tools:

1. **SEGGER J-Link Software and Documentation Pack**

   * https://www.segger.com/downloads/jlink/

2. **Zephyr SDK**

   * Follow the official installation guide:
   * https://docs.zephyrproject.org/latest/develop/toolchains/zephyr_sdk.html#toolchain-zephyr-sdk

3. **Zephyr Development Environment**

   Complete the steps from the Zephyr *Getting Started* guide:

   * Select and update your operating system
   * Install all required dependencies

   https://docs.zephyrproject.org/latest/develop/getting_started/index.html

## ⚙️ Repository Setup

Create the Python virtual environment in the repository root (for example `~/opener-samples/.venv`) and initialize the Zephyr workspace:

```bash
python3 -m venv .venv
source .venv/bin/activate

pip install west

west init -l samples
west update
west zephyr-export
west packages pip --install
west sdk install
```

---

# 🔨 Building

Build a sample application using:

```bash
west build -p always -b <board-name> samples/dectnrp-driver/
```

Example:

```bash
west build -p always -b nrf9151dk/nrf9151/ns samples/dectnrp-driver/
```

---

# ⚡ Optional: Configure West

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

Afterwards, builds can be started with a shorter command:

```bash
west build -b nrf9151dk/nrf9151/ns samples/dectnrp-driver/
```

For additional configuration options, refer to the Zephyr documentation:

https://docs.zephyrproject.org/latest/develop/west/build-flash-debug.html#configuration-options

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
