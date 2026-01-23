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

hook:

Define the hook point, if we attach hook program, the hook program will be excute, else, normal.

```c
int main(void)
{
    if (hook_control_init() < 0) {
        fprintf(stderr, "failed to init hook control socket\n");
        return 1;
    }

    uint8_t buf[64] = {0};
    buf[34] = 1; 
    buf[35] = 0;  
    buf[36] = 0; 
    buf[37] = 1; 

    for (;;) {
        uint32_t r = hook_udp_input(buf, 64);//hook
        printf("hook_udp_input() returned %u\n", r);
        sleep(1);
    }

    close(g_hook_sock);
    unlink(HOOK_SOCK_PATH);
    return 0;
}
```

edit bpf file:

```c
struct udp_hdr {
    unsigned short sport;
    unsigned short dport;
};

int hook(void *ctx)
{
    unsigned int x;
    unsigned int y;
    struct udp_hdr *uh;

    uh = (struct udp_hdr *)&ctx[34];
    x = ntohs(uh->sport);
    print(x);
    y = ntohs(uh->dport);
    print(y);
    return x + y;
}
```

In complier, we have these line:

```
1. compiler hello.c -o out.ccbpf
2. attach hook_udp_input out.ccbpf
3. detach hook_udp_input
```

Run the hooked program:

```c
skaiuijing@ubuntu:~/compiler/hook_test/build$ ./hook_test 
hook_udp_input() returned 0
hook_udp_input() returned 0
hook_udp_input() returned 0
hook_udp_input() returned 0
hook_udp_input() returned 0
hook_udp_input() returned 0
hook_udp_input() returned 0
hook_udp_input() returned 0
```

Then we complier the bpf file:

```c
skaiuijing@ubuntu:~/compiler/build$ ./ccbpf ../hello.c -o out.ccbpf
```

attach it:

```c
skaiuijing@ubuntu:~/compiler/build$ ./ccbpf attach hook_udp_input ../build/out.ccbpf
```

Now the hooked program:

```c
hook_udp_input() returned 257
1
256
```

1 and 256 by print(x) in hello.c.

Then detach it:

```
skaiuijing@ubuntu:~/compiler/build$ ./ccbpf detach hook_udp_input 
```

Then run normal:

```
hook_udp_input() returned 0
hook_udp_input() returned 0
hook_udp_input() returned 0
```

We can rewrite our hello.c, then compiler and attach it, we don't need to compiler the hooked program, just use th CCBPF!

We can hook the value of udp_hdr, then print and return it.

You can drop the packet by the value of return.



