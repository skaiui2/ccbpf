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



# 文档

详细设计文档请见：[设计文档](设计文档.md)

使用参考：[使用文档](使用文档.md)



## 运行

```
git clone https://github.com/skaiui2/ccbpf.git
cd ccbpf
```

然后再开两个终端，一个运行节点A（编译器），一个运行节点B（虚拟机）。

运行节点nodeB：

```
cd nodeB
mkdir build
cd build
cmake ..
make
sudo ./nodeB
```

然后你就能看到一些输出信息：

```
skaiuijing@skaiuijing-virtual-machine:~/Documents/ccbpf_git/ccbpf/nodeB/build$ sudo ./nodeB 
[sudo] password for skaiuijing: 
[wirefisher] pps=1, bps=208
[wirefisher] pps=36, bps=7488
[wirefisher] pps=37, bps=7696
[wirefisher] pps=36, bps=7488
[wirefisher] pps=36, bps=7488
[wirefisher] pps=40, bps=8320
[wirefisher] pps=39, bps=8112
[wirefisher] pps=37, bps=7696
[wirefisher] pps=37, bps=7696
[wirefisher] pps=35, bps=7280
[wirefisher] pps=34, bps=7072
[wirefisher] pps=37, bps=7696
```

编译nodeA：

```
cd nodeA
mkdir build
cd build
cmake ..
make
```



## demo

找到根目录下的hello.c文件：

我们可以这样编写我们要注入的程序：

解析一个udp数据包，然后统计源端口相同的数据包的个数，不同的源端口单独计算。

再用源端口作key，记录数据包的目的端口。

前面的0，1这些是我们的map序号，我们可以配置支持多少个map。

我们的demo是一个网络限速的令牌桶算法：

```c
struct udp_hdr {
    unsigned short sport;
    unsigned short dport;
    unsigned short len;
    unsigned short cksum;
};

int hook(void *ctx)
{
    struct udp_hdr *uh;
    unsigned int sport;
    unsigned int dport;
    unsigned int len;
    unsigned int now;
    unsigned int tokens;
    unsigned int last_ts;
    unsigned int rate;
    unsigned int burst;
    unsigned int delta;
    unsigned int add;

    uh = (struct udp_hdr *)ctx;

    sport = ntohs(uh->sport);
    dport = ntohs(uh->dport);
    len   = ntohs(uh->len);

    now = now_ms();
    print_str("now_time=");
    print(now);
    print_str("\n");

    tokens  = map_lookup(0, sport);
    print_str("tokens=");
    print(tokens);
    print_str("\n");
    last_ts = map_lookup(1, sport);

    print_str("last_ts=");
    print(last_ts);
    print_str("\n");

    rate  = 5000;
    burst = 3000;

    if (last_ts == 0) {
        tokens  = burst;
        last_ts = now;
    } else {
        delta = now - last_ts;
        add   = delta * rate / 1000;
        print_str("add=");
        print(add);
        print_str("\n");
        tokens = tokens + add;
        if (tokens > burst)
            tokens = burst;
        last_ts = now;
    }

    print_str("tokens2=");
    print(tokens);
    print_str("\n");

    if (tokens <= 1000) {
        print_str("[DROP] sport=");
        print(sport);
        print_str(" dport=");
        print(dport);
        print_str(" len=");
        print(len);
        print_str("\n");

        map_update(0, sport, tokens);
        map_update(1, sport, last_ts);

        return 0;
    }

    tokens = tokens - len;

    map_update(0, sport, tokens);
    map_update(1, sport, last_ts);

    print_str("[PASS] sport=");
    print(sport);
    print_str(" dport=");
    print(dport);
    print_str(" len=");
    print(len);
    print_str(" tokens=");
    print(tokens);
    print_str("\n");

    return sport + dport;
}
```



### 编译

把我们的hello.c文件位置传递给程序：

```
sudo ./nodeA ../../hello.c -o out.ccbpf
```

然后我们就能看到一堆编译器输出的打印：

这里展示部分：

```
skaiuijing@skaiuijing-virtual-machine:~/Documents/ccbpf/nodeA/build$ sudo ./nodeA ../../hello.c -o out.ccbpf
L1:
[IR] LABEL L1
[IR] STORE MEM[8 + t0 * 8] <- t1
[IR] LOAD_CTX t3 <- CTX[0]
[IR] NATIVE_CALL func=2 dst=t2 argc=1 (args: t3 ...)
[IR] STORE MEM[16 + t0 * 4] <- t2
[IR] LOAD_CTX t5 <- CTX[2]
[IR] NATIVE_CALL func=2 dst=t4 argc=1 (args: t5 ...)
[IR] STORE MEM[20 + t0 * 4] <- t4
[IR] LOAD_CTX t7 <- CTX[4]
[IR] NATIVE_CALL func=2 dst=t6 argc=1 (args: t7 ...)
[IR] STORE MEM[24 + t0 * 4] <- t6
[IR] NATIVE_CALL func=7 dst=t8 argc=0 (args: t-1 ...)
[IR] STORE MEM[28 + t0 * 4] <- t8
[IR] MOVE  t10 <- 0
[IR] NATIVE_CALL func=4 dst=t9 argc=1 (args: t10 ...)
[IR] LOAD  t12 <- MEM[28 + t0 * 4]
........
```

还有一些内存占用，因为内存管理算法是我们自己写的，所以我们可以根据内存占用调整前端、ir、后端分配的内存大小。

### 附加程序

使用命令：

```
 sudo ./nodeA attach hook_udp_input out.ccbpf
```

我们会发现，nodeB的打印瞬间改变了：

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
[PASS] sport=10000 dport=20000 len=208 tokens=2629
now_time=32082276
tokens=2629
last_ts=32082229
add=235
tokens2=2864
[PASS] sport=10000 dport=20000 len=208 tokens=2656
now_time=32082325
```



我们的count统计会不断更新，同时解析udp数据包的源端口和目的端口。

由于这里制造的数据包源端口和目的端口都是一样的（大小不一样），所以只有count会变化。

### 卸载程序

使用以下命令卸载我们的bpf程序：

```
 sudo ./nodeA detach hook_udp_input
```

我们会发现，nodeB的输出又恢复正常了：

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
[wirefisher] pps=38, bps=7904
[wirefisher] pps=39, bps=8112
[wirefisher] pps=39, bps=8112
```

