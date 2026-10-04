# Castling (王车易位)

[简体中文](README.md) | [English](README.en.md)

A lightweight memory cleaner for Windows and macOS. How it works: flushes the system file cache, refreshes the modified page list, purges standby memory and trims process working sets.

## Features

- **Windows**: combines official/authoritative cleanup mechanisms — flushes the system file cache (`SetSystemFileCacheSize`), refreshes the modified page list and purges the standby list (`NtSetSystemInformation`), and trims per-process working sets (`SetProcessWorkingSetSize(-1,-1)` + `EmptyWorkingSet`); dark custom popup shows freed memory (MB/GB) and available memory before/after comparison
- **Run as administrator**: the full cleanup (per-process working-set trim, standby-list purge, system-file-cache flush) requires **"Run as administrator"**; a plain double-click only performs the parts available to a normal user (the popup hints this when you hover the OK button)
- **macOS**: invokes the official `purge` command to clear inactive memory and system caches; native popup shows the result; double-click to run, no terminal window

## Usage

### Windows

1. Double-click `Castling.exe`; a result window appears when the cleanup is done
2. For the full cleanup (purge standby memory + trim all process working sets): right-click it and choose "Run as administrator"
3. Press `Esc` or click OK to close the popup
4. If your antivirus flags it (e.g. Huorong), add it to the trust list

### macOS

1. Check your chip: Apple Silicon (M1/M2/M3/M4...) uses `Castling-arm64.app`; Intel uses `Castling-amd64.app`
   (Not sure? Use the Apple menu, then About This Mac, then look at "Chip")
2. Drag the matching `.app` into Applications (or your Desktop)
3. Double-click it, enter your password when prompted, and the before/after memory report appears
4. If macOS says the developer cannot be verified: right-click the `.app`, choose Open, then click Open again

## Directory Layout

```
src/        Windows C source + build scripts
macos/      macOS Go source
assets/     app icons
dist/       prebuilt releases
```

## Building

### Windows (MinGW-w64 + gcc)

```bat
cd src
windres -c 65001 memclean.rc -O coff -o memclean_res.o
gcc -mwindows -O2 -static -specs=gcc.specs memclean.c memclean_res.o -o Castling.exe -lcomctl32 -lpsapi -lgdiplus -lole32
```

Note: `gcc.specs` removes the default manifest (to avoid conflicts with the custom manifest).

### macOS (Go 1.21+)

```sh
cd macos
CGO_ENABLED=0 GOOS=darwin GOARCH=arm64 go build -ldflags "-s -w" -o mc-arm64 main.go   # Apple Silicon
CGO_ENABLED=0 GOOS=darwin GOARCH=amd64 go build -ldflags "-s -w" -o mc-amd64 main.go   # Intel
```

Then place the binary inside an `.app` bundle.

## Releases (dist/)

| File | Description |
|---|---|
| `Castling.exe` | Windows main program (runs directly on Win10/11, zero dependencies) |
| `Castling-macOS-app.tar.gz` | macOS native app (Intel + Apple Silicon) |

## System Requirements

- Windows 10 / 11 (Win7 needs UCRT update KB2999226)
- macOS 10.12+
