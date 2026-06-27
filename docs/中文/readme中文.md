# ccbpf — 面向嵌入式系统的类eBPF功能

ccbpf 是一个为 **MCU / RTOS / 裸机系统** 设计的极简动态编程组件。
 它由 **C 子集编译器** + **BPF 虚拟机** 组成，灵感来自 Linux eBPF，但更小、更简单、更易移植。

它的核心目标只有一个：

**让 MCU 也能像 Linux的eBPF 一样，在运行时加载代码，而不是重新烧录固件。**

# 为什么是 ccbpf？

传统 MCU 固件是静态的：
 更新 = 重编译 + 烧录 + 重启。

ccbpf 提供一种 **简单、可验证、资源占用极低** 的运行时可加载机制，用于：

- 在 **RTOS 内核** 中插入 hook
- 在 **协议栈** 中过滤、监控、修改数据
- 在 **文件系统 / 驱动** 中扩展行为
- 在 **分布式节点** 中下发策略（如 lttit 项目）

一句话：**无需重启、无需重编译、无需重新烧录固件。**

# ccbpf vs. Linux eBPF

| 项目     | Linux eBPF | ccbpf             |
| -------- | ---------- | ----------------- |
| 运行环境 | Linux 内核 | MCU / RTOS / 裸机 |
| 复杂度   | 高         | 极简              |
| 程序来源 | LLVM/Clang | 内置 C 子集编译器 |
| 安全模型 | Verifier   | 语法 + VM 限制    |
| 程序类型 | 多种       | Hook 程序         |
| 资源占用 | MB 级      | KB 级             |
| 可移植性 | 依赖 Linux | 完全平台无关      |

**eBPF 让 Linux 可编程；
 ccbpf 让 MCU 也能做到同样的事。**

# 设计特点

ccbpf 不是 eBPF 的移植，而是面向 MCU 重新设计的极简方案：

- 受限 C 子集（无循环、无指针算术）
- 小型 BPF VM（几 KB）
- 可加载程序格式
- 可插拔 hook 机制
- 简单 map 接口
- 完全平台无关（Linux / RTOS / 裸机均可移植）

设计重点：

- **实现简单**
- **行为可预测**
- **易于验证**
- **资源占用小**
- **适合嵌入式环境**

# 内存占用

- 在 20KB RAM MCU 上编译 ~15 条 C 语句：**约 8KB**
- 编译近 100 条语句、生成 397 条指令：**峰值 < 60KB**
- VM 虚拟机运行 <200 条指令的程序：1–2KB RAM

# 快速运行demo

```
git clone https://github.com/skaiui2/ccbpf.git
cd ccbpf
chmod +x *.sh
```

## 动态注入程序demo

然后再开两个终端，一个运行节点A，一个运行节点B。

运行节点nodeA：

```
./run_nodeA.sh
```

然后你就能看到一些输出信息：

这代表我们的nodeA程序正在运行，它会统计每一个UDP数据包。

```
skaiuijing@skaiuijing-virtual-machine:~/Documents/ccbpf_git/ccbpf/nodeB/build$ ./run_nodeA.sh
[sudo] password for skaiuijing: 
[wirefisher] pps=1, bps=208
[wirefisher] pps=36, bps=7488
[wirefisher] pps=37, bps=7696
[wirefisher] pps=36, bps=7488
[wirefisher] pps=36, bps=7488
[wirefisher] pps=40, bps=8320
[wirefisher] pps=39, bps=8112
[wirefisher] pps=37, bps=7696
```

我们要注入的demo程序是一个网络限速的令牌桶算法，使用命令：

```
./attach.sh
```

然后我们就能看到一堆编译器输出的打印，并且我们会发现，nodeA的打印瞬间改变了：

```
[wirefisher] pps=37, bps=7696
[wirefisher] pps=40, bps=8320
[hook] ATTACH hook_udp_input (prog=0x607398e83968)
now_time=32082220
tokens=0
last_ts=0
tokens2=3000
[PASS] sport=10000 dport=20000 len=208 tokens=2792
now_time=32082229
tokens=2792
last_ts=32082220
add=45
tokens2=2837
```

我们的count统计会不断更新，同时解析udp数据包的源端口和目的端口。

由于这里制造的数据包源端口和目的端口都是一样的（大小不一样），所以只有count会变化。

### 卸载程序

使用以下命令卸载我们的bpf程序：

```
 ./detach.sh
```

我们会发现，nodeA的输出又恢复正常了：

```
last_ts=32096900
add=80
tokens2=976
[DROP] sport=10000 dport=20000 len=208
[hook] DETACH hook_udp_input
[wirefisher] pps=37, bps=7696
[wirefisher] pps=34, bps=7072
[wirefisher] pps=34, bps=7072
[wirefisher] pps=34, bps=7072
[wirefisher] pps=35, bps=7280
[wirefisher] pps=35, bps=7280
[wirefisher] pps=35, bps=7280
[wirefisher] pps=38, bps=7904
[wirefisher] pps=38, bps=7904
```

## 执行迁移demo

在这里，我们的程序会在nodeD先执行几行，接下来会迁移到nodeC执行剩下的代码：

执行进程：

```
./run_nodeD.sh 
```

然后在另一个终端执行nodeC：

```
./run_nodeC.sh 
```

你会发现，程序在nodeC执行后，打印输出：

```
nodeC: runing....
nodeC: 1
nodeC: 2
nodeC: 3
nodeC: 4
nodeC: 5
nodeC: migration_start
nodeC: migrate PC is 87
```

接下来nodeD开始输出：

```
nodeC: migration_end
nodeC: 6
nodeC: 7
nodeC: 8
nodeC: 9
nodeC: 10
nodeC: 11
nodeC: ok!!!
nodeD: finished 0
```

这就是发生了执行迁移，虚拟机被暂停，然后被打包到nodeD上继续执行。

# 文档

详细设计文档请见：[设计文档](设计文档.md)

使用参考：[使用文档](使用文档.md)

