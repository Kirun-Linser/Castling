# MemoryCleaner

[简体中文](README.md) | [English](README.en.md)

A lightweight memory cleaner for Windows and macOS. How it works: trims process working sets and clears system standby memory.

## Features

- **Windows**: iterates all processes calling `SetProcessWorkingSetSize(-1,-1)` + `EmptyWorkingSet`; when run as administrator, additionally enables privileges to clean system processes and clear the Standby List; dark custom popup shows freed memory (MB/GB) and available memory before/after comparison
- **macOS**: invokes the official `purge` command to clear inactive memory and system caches; native popup shows the result; double-click to run, no terminal window

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
windres memclean.rc -O coff -o memclean_res.o
gcc -mwindows -O2 -static -specs=gcc.specs memclean.c memclean_res.o -o MemoryCleaner.exe -lcomctl32 -lpsapi -lgdiplus -lole32
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
| `MemoryCleaner.exe` | Windows main program (runs directly on Win10/11, zero dependencies) |
| `MemoryCleaner-便携版.zip` | Windows portable package (exe + instructions) |
| `MemoryCleaner-macOS.zip` | macOS script version (terminal + system popup) |
| `MemoryCleaner-macOS-app.tar.gz` | macOS native app (Intel + Apple Silicon) |

## System Requirements

- Windows 10 / 11 (Win7 needs UCRT update KB2999226)
- macOS 10.12+
