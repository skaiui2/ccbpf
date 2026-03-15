# ccbpf — A Lightweight Compiler and Virtual Machine for Embedded Systems

[中文介绍](docs/中文/readme中文.md)

ccbpf is a minimal dynamic‑programming component designed for **MCU / RTOS / bare‑metal systems**.
 It consists of a **C‑subset compiler** and a **BPF virtual machine**, inspired by Linux eBPF but smaller, simpler, and far more portable.

Its core mission is simple:

**Bring Linux‑style runtime code loading to MCUs — without reflashing firmware.**

```mermaid
graph LR
    A["Frontend<br/><sub>Lexer · Parser · AST</sub>"] 
        --> B["IR<br/><sub>Three‑Address Code</sub>"]
        --> C["Backend<br/><sub>Lowering to Classic BPF</sub>"]
        --> D[".ccbpf Image<br/><sub>BPF · Strings · Maps</sub>"]
        --> E["BPF VM<br/><sub> Native  Hooks</sub>"]

```



# Why ccbpf?

Traditional MCU firmware is static:
 update = recompile + flash + reboot.

ccbpf provides a **simple, verifiable, extremely lightweight** runtime‑loadable mechanism for:

- Inserting hooks into **RTOS kernels**
- Filtering, monitoring, and modifying data in **protocol stacks**
- Extending behavior in **file systems / drivers**
- Distributing logic across nodes (e.g., the lttit project)

In short:
 **No reboot. No rebuild. No reflashing.**

# ccbpf vs. Linux eBPF

| Feature      | Linux eBPF      | ccbpf                      |
| ------------ | --------------- | -------------------------- |
| Runtime      | Linux kernel    | MCU / RTOS / bare‑metal    |
| Complexity   | High            | Minimal                    |
| Toolchain    | LLVM/Clang      | Built‑in C subset compiler |
| Safety       | Verifier        | Language + VM limits       |
| Program Type | Many            | Hook programs              |
| Footprint    | MB‑level        | KB‑level                   |
| Portability  | Linux‑dependent | Fully platform‑agnostic    |

**eBPF makes Linux programmable;
 ccbpf brings the same idea to MCUs.**

# Design Highlights

ccbpf is not a port of eBPF — it is a ground‑up minimal design for MCUs:

- Restricted C subset (no loops, no pointer arithmetic)
- Tiny BPF VM (a few KB)
- Loadable program format
- Pluggable hook mechanism
- Simple map interface
- Fully platform‑independent (Linux / RTOS / bare‑metal)

Design goals:

- **Simple implementation**
- **Predictable behavior**
- **Easy verification**
- **Tiny footprint**
- **Embedded‑friendly**



# Memory Usage

- Compiling ~15 C statements on a 20KB‑RAM MCU: **~8KB**
- Compiling ~100 statements → 397 BPF instructions: **<60KB peak**
- VM running <200 instructions: **1–2KB RAM**

# Documentation

Design document: [design](docs/English/design.md)
Usage reference: [usage](docs/English/usage.md)

设计: [设计文档](docs/中文/设计文档.md)
使用: [使用文档](docs/中文/使用文档.md)

## Run

```
git clone https://github.com/skaiui2/ccbpf.git
cd ccbpf
```

Open two terminals: one for nodeA (compiler), one for nodeB (VM).

Run nodeB:

```
cd nodeB
mkdir build
cd build
cmake ..
make
sudo ./nodeB
```

You will see output like:

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

Build nodeA:

```
cd nodeA
mkdir build
cd build
cmake ..
make
```

Then,

Find `hello.c` in the project root.

Here is an example program to inject:

It parses a UDP packet, counts packets by source port, and records the destination port.
 The numbers 0, 1, etc. are map indices; you can configure how many maps to support.

The demo implements a token‑bucket rate limiter:

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

### Compile

Pass the path to `hello.c`:

```
sudo ./nodeA ../../hello.c -o out.ccbpf
```

You will see compiler output, including IR dumps and memory usage information.

### Attach Program

```
sudo ./nodeA attach hook_udp_input out.ccbpf
```

nodeB output will immediately change:

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

### Detach Program

```
sudo ./nodeA detach hook_udp_input
```

nodeB output returns to normal:

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

