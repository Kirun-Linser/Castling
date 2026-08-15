# MemoryCleaner

轻量级内存清理工具，Windows / macOS 双平台。原理与 PCL2 启动器的"清理内存"一致：压缩进程工作集、清空系统备用内存。

## 功能

- **Windows**：遍历所有进程调用 `SetProcessWorkingSetSize(-1,-1)` + `EmptyWorkingSet`；以管理员运行时额外启用特权清理系统进程并清空 Standby List；深色自绘弹窗显示释放量（MB/GB）与清理前后可用内存对比
- **macOS**：调用官方 `purge` 命令清除非活跃内存与系统缓存，原生弹窗显示结果，双击即用、无终端窗口

## 目录结构

```
src/        Windows 版 C 源码 + 构建脚本
macos/      macOS 版 Go 源码
assets/     应用图标
dist/       编译好的发行物
```

## 构建

### Windows（MinGW-w64 + gcc）

```bat
cd src
windres memclean.rc -O coff -o memclean_res.o
gcc -mwindows -O2 -static -specs=gcc.specs memclean.c memclean_res.o -o MemoryCleaner.exe -lcomctl32 -lpsapi -lgdiplus -lole32
```

注意：`gcc.specs` 移除了默认 manifest（避免与自定义 manifest 冲突）。

### macOS（Go 1.21+）

```sh
cd macos
CGO_ENABLED=0 GOOS=darwin GOARCH=arm64 go build -ldflags "-s -w" -o mc-arm64 main.go   # Apple 芯片
CGO_ENABLED=0 GOOS=darwin GOARCH=amd64 go build -ldflags "-s -w" -o mc-amd64 main.go   # Intel
```

再将二进制放入 `.app` 包。

## 发行物（dist/）

| 文件 | 说明 |
|---|---|
| `MemoryCleaner.exe` | Windows 主程序（Win10/11 直接运行，零依赖） |
| `MemoryCleaner-便携版.zip` | Windows 便携包（exe + 说明） |
| `MemoryCleaner-macOS.zip` | macOS 脚本版（终端 + 系统弹窗） |
| `MemoryCleaner-macOS-app.tar.gz` | macOS 原生 App（Intel + Apple 芯片双版本） |

## 系统要求

- Windows 10 / 11（Win7 需 UCRT 更新 KB2999226）
- macOS 10.12+
