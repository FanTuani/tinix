# Tinix

Tinix 是一个用户态的操作系统机制模拟器（teaching simulator），用于演示与验证进程管理、内存管理、文件系统等关键机制。项目聚焦“机制推演与可观测性”，不追求真实内核的工程完备性与性能。

## 项目简介

Tinix 采用 C++ 实现，通过 `tick` 推进系统时间（离散事件/时钟驱动），并提供交互式 Shell 来观察与操作系统状态。系统支持两种“交互入口”：

- **交互式 Shell**：用于手动创建进程、推进时钟、查看内存/页表、操作文件系统等。
- **.pc 指令流**：用于描述进程的指令序列（Compute / MemRead / MemWrite / Sleep 等）。

## 特性概览

- **进程管理**：五态进程模型、时间片轮转（Round-Robin）、阻塞/唤醒（sleep）。
- **内存管理**：分页与页表、缺页处理、Clock 页面置换、swap（基于 `disk.img`）。
- **文件系统**：基于 inode 的简化文件系统、目录管理、位图分配、状态持久化（`disk.img`）。
- **磁盘设备**：统一的块设备抽象（同时为文件系统与 swap 提供后备存储）。
- **可观测性**：关键路径均输出日志，便于跟踪状态变化。

说明：`.pc` 指令集目前包含文件/设备相关操作码（如 `FO/FR/FW`、`DR/DD`），但这些指令在进程执行路径中**仅做解析与日志输出**；实际文件系统读写通过 Shell 命令完成（见下文）。

## 快速开始

### 构建要求

- C++20 编译器
- CMake 3.20+
- （可选）Python 3：用于运行 `ctest` 测试用例

### 编译并运行

一键构建并启动：

```bash
./bdrun.sh
```

或手动构建：

```bash
cmake -S . -B build
cmake --build build
./build/tinix
```

首次运行会在当前工作目录创建 `disk.img`；若未检测到可挂载的文件系统，会自动格式化（见 `src/kernel.cpp`）。

## 使用示例

### 进程与时钟

```bash
# 创建进程
create 10                  # 创建纯计算进程（10条指令）
create -f t1.pc            # 从 .pc 文件创建进程

# 查看进程状态
ps

# 执行时钟滴答
tick                       # 执行1个tick
tick 10                    # 执行10个tick

# 进程控制
run <pid>                  # 手动调度进程
block <pid> 5              # 阻塞进程5个tick
wakeup <pid>               # 唤醒阻塞进程
kill <pid>                 # 终止进程

# 批量执行 Shell 命令脚本
script sh1.tsh

# 退出
exit
```

提示：完整命令列表以 `help` 输出为准。

### 文件系统（Shell 命令）

```bash
format
mount
mkdir /a
cd /a
touch f
echo hello > f
ls
cat f
pwd
```

## .pc 脚本格式

进程可通过 `.pc` 文件定义指令序列（支持十种操作码；地址参数支持十进制或 `0x` 前缀的十六进制）：

```
# 注释
C                    # Compute - 计算
R <addr>             # MemRead - 读内存
W <addr>             # MemWrite - 写内存
S <duration>         # Sleep - 睡眠
FO <filename>        # FileOpen - 打开文件
FC <fd>              # FileClose - 关闭文件
FR <fd> <size>       # FileRead - 读文件
FW <fd> <size>       # FileWrite - 写文件
DR <dev_id>          # DevRequest - 请求设备
DD <dev_id>          # DevRelease - 释放设备
```

仓库内提供了示例程序：`t1.pc`（混合计算/访存/睡眠）、`t2.pc`（密集访存）。例如 `t1.pc`：
```
# Simple test program
C
C
R 0x0000
R 0x1000
W 0x2000
C
S 3
C
R 0x0064
W 0x00C8
```

## 项目结构

```
tinix/
├── include/          # 头文件
│   ├── proc/        # 进程管理
│   ├── mem/         # 内存管理
│   ├── fs/          # 文件系统
│   ├── dev/         # 设备管理
│   └── shell/       # 交互Shell
├── src/             # 源代码
├── docs/            # 文档
├── *.pc             # 进程脚本示例
└── *.tsh            # Shell脚本示例
```

## 开发进度（摘要）

- [x] **进程管理基础**
  - [x] PCB 数据结构
  - [x] 五态进程模型（New/Ready/Running/Blocked/Terminated）
  - [x] 时间片轮转调度（Round-Robin）
  - [x] 阻塞与唤醒机制
  - [x] 进程创建与终止

- [x] **指令流架构**
  - [x] 指令类型定义（10种操作）
  - [x] Program 类封装
  - [x] .pc 脚本文件解析
  - [x] 指令执行框架
  - [x] 进程-指令绑定

- [x] **交互式 Shell**
  - [x] 命令解析器
  - [x] 进程控制命令
  - [x] 脚本执行（.tsh）
  - [x] 状态查看命令

- [x] **内存管理**
  - [x] 物理内存管理器
  - [x] 页表结构
  - [x] 地址转换机制
  - [x] Clock 页面置换算法
  - [x] Swap 后备存储
  - [x] 缺页统计
  - [x] 进程-内存集成

- [x] **文件系统**
  - [x] Superblock 结构
  - [x] Inode 管理
  - [x] 目录结构
  - [x] 文件操作接口
  - [x] 位图管理
  - [x] 持久化存储

- [ ] **设备管理**
  - [x] 块设备抽象（DiskDevice）
  - [ ] 设备分配机制
  - [ ] 阻塞队列
  - [ ] 设备释放

## 技术要点

- **时钟驱动**：通过 `tick` 推进系统时间，便于推演调度与阻塞。
- **指令流**：进程行为由 `.pc` 指令序列描述；可扩展与文件系统/设备管理的联动。
- **模块化**：进程/内存/文件系统/设备各自独立，边界清晰。
- **共享磁盘镜像**：文件系统使用 `disk.img` 的前半区间；swap 保留尾部块区间（详见 `include/common/config.h`、`include/fs/fs_defs.h`）。

## 测试

项目提供了基于 CTest 的回归用例（需要 Python 3）：

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## 许可证

本项目基于 GNU General Public License v3.0 开源发布。
详见 LICENSE 文件。

## 相关文档

详细的设计要求和实现细节请参考 [docs/项目要求.md](docs/项目要求.md)
