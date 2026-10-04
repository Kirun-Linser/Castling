# Castling（王车易位）

[English](README.en.md) | [简体中文](README.md)

轻量级内存清理工具，Windows / macOS 双平台。原理：清系统文件缓存、刷新已修改页、清空备用内存、压缩进程工作集。

## 功能

- **Windows**：整合 Windows 官方/权威清理机制——清系统文件缓存（`SetSystemFileCacheSize`）、刷新已修改页链表、清空备用内存（Standby List，`NtSetSystemInformation`），以及逐进程压缩工作集（`SetProcessWorkingSetSize(-1,-1)` + `EmptyWorkingSet`）；深色自绘弹窗显示释放量（MB/GB）与清理前后可用内存对比
- **以管理员运行**：完整清理（含逐进程工作集压缩、清空备用内存、清系统文件缓存）需**右键「以管理员身份运行」**；普通双击仅执行普通权限可用部分（弹窗悬停「确定」按钮会提示）
- **macOS**：调用官方 `purge` 命令清除非活跃内存与系统缓存，原生弹窗显示结果，双击即用、无终端窗口

## 使用方法

### Windows

1. 双击 `Castling.exe` 即可，清理完成后弹出结果窗口
2. 想要完整清理力度（清空备用内存 + 压缩全部进程工作集）：右键「以管理员身份运行」
3. 按 `Esc` 或点击「确定」关闭弹窗
4. 若火绒等安全软件误报，加入信任区即可

### macOS

1. 确认芯片类型：Apple 芯片（M1/M2/M3/M4...）用 `Castling-arm64.app`，Intel 芯片用 `Castling-amd64.app`
   （不确定时：点击屏幕左上角→「关于本机」→ 查看「芯片」一栏）
2. 把对应的 `.app` 拖进「应用程序」文件夹或桌面
3. 双击图标→ 弹出系统密码框（用于提权）→ 完成后显示清理前后内存状况
4. 首次打开若提示「无法验证开发者」：右键点击 `.app`→「打开」→ 再点一次「打开」

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
windres -c 65001 memclean.rc -O coff -o memclean_res.o
gcc -mwindows -O2 -static -specs=gcc.specs memclean.c memclean_res.o -o Castling.exe -lcomctl32 -lpsapi -lgdiplus -lole32
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
| `Castling.exe` | Windows 主程序（Win10/11 直接运行，零依赖） |
| `Castling-macOS-app.tar.gz` | macOS 原生 App（Intel + Apple 芯片双版本） |

## 系统要求

- Windows 10 / 11（Win7 需 UCRT 更新 KB2999226）
- macOS 10.12+
