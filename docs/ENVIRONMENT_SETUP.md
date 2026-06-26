# Environment Setup

This project is intended to be developed in **C++** using the **official Raspberry Pi Pico SDK**.

It is **not** intended to use Arduino or MicroPython for the firmware path described in [`CODEX_SPEC.md`](CODEX_SPEC.md).

## Current Local Status

- Board is detected on `COM14`
- The official VS Code extension `raspberry-pi.raspberry-pi-pico` is installed
- Git is available
- The standalone Pico build prerequisites were **not** found on `PATH` in the terminal session:
  - `cmake`
  - `ninja`
  - `arm-none-eabi-gcc`
  - `arm-none-eabi-g++`
  - `picotool`
  - `PICO_SDK_PATH`

This is expected to be recoverable through the official Raspberry Pi Pico VS Code extension, which can manage the SDK and toolchain automatically on Windows.

## Recommended VS Code Flow

1. Open this repository in VS Code.
2. Trust the workspace if prompted.
3. Let the Raspberry Pi Pico extension activate for this project.
4. Run `Raspberry Pi Pico: Import Pico Project` from the Command Palette.
5. Allow the extension to install or manage the required Pico SDK and toolchain components.
6. If prompted for the project type, keep the existing **Pico SDK / C++** project layout.
7. After the import completes, run `Raspberry Pi Pico: Configure CMake`.
8. Then run `Raspberry Pi Pico: Compile Pico Project`.

## Why This Flow

- It keeps the project aligned with the official Pico SDK C++ workflow.
- It avoids hard-coding SDK/toolchain paths into the repository.
- It lets the extension manage Windows-specific tool locations cleanly.

## Serial Check

After a successful build/flash, open serial on:

- Port: `COM14`
- Baud: `115200`

Expected Milestone 1 output should include boot text from `main.cpp`, followed by the LED heartbeat running on hardware.
