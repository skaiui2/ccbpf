# ccbpf
A lightweight compiler and virtual machine designed for embedded systems. Inspired by eBPF, built for MCU.

## Overview

This project is a lightweight language runtime designed for embedded systems— a compiler and virtual machine that can run on MCUs, inspired by eBPF but smaller, simpler, and far more portable.

It enables developers to inject dynamic hook code into components such as RTOS kernels, network protocol stacks, and file systems without recompiling firmware, providing a flexible and safe extension mechanism for embedded environments.

Of course, it can also run on general‑purpose operating systems like Linux, where it can serve as an extensible scripting engine or a safe, embeddable runtime for dynamic application logic.

## design

```mermaid
graph LR
A(frontend)-->B(IR)-->C(backend)-->D(bpf files)-->E(BPF VM)
```

The documents:   [设计文档](docs/中文/设计文档.md)

## use

edit:

```c
struct udp_hdr {
    int sport;
    int dport;
};

int hook(void *ctx, char *pkt) {
    return ((struct udp_hdr *)&pkt[34])->sport;
}
```

run:

```
mkdir build
cmake ..
make
./ccbpf
```

