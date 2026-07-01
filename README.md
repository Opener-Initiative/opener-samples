# Dectnrp opener sample

[[_TOC_]]

This demo application

# Project setup

## Development host setup recipe
1. Install Segger JLink Tools: https://www.segger.com/downloads/jlink/
4. Install the [Zephyr SDK](https://docs.zephyrproject.org/latest/develop/toolchains/zephyr_sdk.html#toolchain-zephyr-sdk) with instructions from this page
5. Please take assistance from the ZephyrOS ['Getting Started'](https://docs.zephyrproject.org/latest/develop/getting_started/index.html) web page for the following steps:
* [Select and Update OS](https://docs.zephyrproject.org/latest/develop/getting_started/index.html#select-and-update-os)
* [Install dependencies](https://docs.zephyrproject.org/latest/develop/getting_started/index.html#install-dependencies)
* Create the virtual `.venv` environment in the project's cloned root directory like `~/opener-sample/.venv` instead of the suggested directory in the ZephyrOS 'Getting Started' web page:
```
python3 -m venv ./.venv
source ./.venv/bin/activate
pip install west
west init -l app
west update
west zephyr-export
west packages pip --install
west sdk install
```

### Build the Sample

```
west build -p always -b <your-board-name> app/dectnrp-driver/
```



### West configuration and Quicker usage

If you want to reduce the west command parameters, you can set west config for shorter build commands ([see configuration-options](https://docs.zephyrproject.org/latest/develop/west/build-flash-debug.html#configuration-options)).
```bash
west config build.dir-fmt "{source_dir}/../../build/{board}/{app}"
west config build.pristine auto
west config build.sysbuild true
```
This setting will be stored at `.west/config`.
With this config you can build the firmware with the following statement:

```bash
west build -b nrf9151dk/nrf9151/ns app/dectnrp-driver/
```

