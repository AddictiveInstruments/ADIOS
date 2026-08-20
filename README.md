# MIOS32 (trunk_dyn_bsl)

Firmware "OS" for STM32-based instrument retrofits - MIDI-centric, derived
from [MIOS32](http://www.ucapps.de/mios32.html), heavily
reworked: STM32G0xx + STM32F4xx families, LL-driver based, dynamic
bootloader/app flash boundary, opt-in peripheral model, bare-metal or
FreeRTOS scheduling tiers.

## Prerequisites

- **arm-none-eabi-gcc** (any recent release; 13.x is what the reference
  builds use)
  - macOS: `brew install --cask gcc-arm-embedded` or the
    [Arm GNU toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) package
  - Windows: same Arm GNU toolchain, plus MSYS2 (or Git Bash) for `make`/`sh`
  - Linux: distro package (`gcc-arm-none-eabi`) or the Arm release
- **GNU make** and a POSIX `sh` on the PATH (Apple's bundled make 3.81 is
  old but sufficient; on Windows use the MSYS2 one, NOT a native Win32 make)
- **perl** (bundled with macOS/Linux/MSYS2 - used by the bootloader image
  generator)

## Building

No environment setup is needed - every app Makefile self-locates the repo
root:

```
cd apps/Bruno/g030k6_test      # or any other app directory
make
```

An exported `MIOS32_PATH` (absolute path to the repo root, POSIX style -
e.g. `/e/MIOS32/trunk_dyn_bsl` under MSYS) overrides the self-location if
you need to build an app that lives OUTSIDE the repo tree.

Apps that opt into the dynamic bootloader mechanism
(`MIOS32_USE_DYNAMIC_BSL_BOUNDARY = 1` in their Makefile) produce two
deliverables in the app directory on every `make`:

- `<app>_full_bsl_app.bin` - combined bootloader+app image, flash it at
  0x08000000 over SWD for a factory-fresh board
- `<app>_app_only.hex` - app only, for field updates over MIDI
  (MIOS Studio), never touches the protected bootloader region

The bootloader is rebuilt and measured automatically on every make (see
`etc/gen_bsl_boundary.sh` - the app/bootloader flash split is computed from
the real compiled bootloader size, not a hardcoded constant). Concurrent
builds are safe: the shared bootloader build directory is protected by a
lock, a second build simply waits.

## IDE use

Any IDE that can run `make` in the app directory works - no IDE-specific
build configuration is required:

- **STM32CubeIDE / Eclipse CDT**: import the app as an existing project
  (the ones with tracked `.project` files) or create a Makefile project;
  the builder just runs `make all`
- **Xcode**: External Build System target, build tool `make`, directory =
  the app folder
- Plain terminal: `make`, `make clean`, `make cleanall`

## Repo layout (short)

- `apps/` - one directory per application/instrument; each is the place you
  run `make` from
- `mios32/` - the OS core: `common/` (family-independent) + one directory
  per chip family (`STM32G0xx/`, `STM32F4xx/`)
- `programming_models/traditional/` - `main.c` (FreeRTOS or bare-metal
  scheduling), shared build logic (`programming_model.mk`)
- `include/makefile/common.mk` - common make rules
- `bootloader/src/` - the MIDI bootloader (one generic `Makefile.bsl` for
  every chip)
- `etc/ld/` - linker script reference library (per-chip memory maps),
  `etc/startup/` - startup files, `etc/gen_bsl_boundary.sh` - the dynamic
  boundary generator
- `modules/` - optional application-level modules (LCD drivers, ...)
