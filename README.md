# ccbpf
A compiler, implement a hook language similar to 4.4BSD-lite's BPF.

## run

```
mkdir build
cmake ..
make
./ccbpf
```

like this:

```
skaiuijing@ubuntu:~/compiler/build$ ./ccbpf 
TOKEN: tag=282, str={
TOKEN: tag=257, str=int
TOKEN: tag=268, str=a
TOKEN: tag=285, str=;
TOKEN: tag=257, str=int
TOKEN: tag=268, str=b
TOKEN: tag=285, str=;
TOKEN: tag=257, str=int
TOKEN: tag=268, str=c
TOKEN: tag=285, str=;
TOKEN: tag=257, str=int
TOKEN: tag=268, str=arr
TOKEN: tag=301, str=[
TOKEN: tag=273, str=4
TOKEN: tag=302, str=]
TOKEN: tag=285, str=;
TOKEN: tag=268, str=a
TOKEN: tag=293, str==
TOKEN: tag=273, str=3
TOKEN: tag=285, str=;
TOKEN: tag=268, str=b
TOKEN: tag=293, str==
TOKEN: tag=273, str=4
TOKEN: tag=285, str=;
TOKEN: tag=268, str=c
TOKEN: tag=293, str==
TOKEN: tag=268, str=a
TOKEN: tag=288, str=+
TOKEN: tag=268, str=b
TOKEN: tag=290, str=*
TOKEN: tag=273, str=2
TOKEN: tag=285, str=;
TOKEN: tag=268, str=arr
TOKEN: tag=301, str=[
TOKEN: tag=273, str=1
TOKEN: tag=302, str=]
TOKEN: tag=293, str==
TOKEN: tag=268, str=c
TOKEN: tag=285, str=;
TOKEN: tag=268, str=a
TOKEN: tag=293, str==
TOKEN: tag=268, str=arr
TOKEN: tag=301, str=[
TOKEN: tag=273, str=1
TOKEN: tag=302, str=]
TOKEN: tag=285, str=;
TOKEN: tag=269, str=if
TOKEN: tag=280, str=(
TOKEN: tag=268, str=a
TOKEN: tag=263, str=<
TOKEN: tag=268, str=b
TOKEN: tag=256, str=&&
TOKEN: tag=268, str=c
TOKEN: tag=265, str===
TOKEN: tag=273, str=10
TOKEN: tag=281, str=)
TOKEN: tag=282, str={
TOKEN: tag=268, str=c
TOKEN: tag=293, str==
TOKEN: tag=268, str=c
TOKEN: tag=289, str=-
TOKEN: tag=273, str=1
TOKEN: tag=285, str=;
TOKEN: tag=283, str=}
TOKEN: tag=269, str=if
TOKEN: tag=280, str=(
TOKEN: tag=268, str=a
TOKEN: tag=264, str=>
TOKEN: tag=268, str=b
TOKEN: tag=274, str=||
TOKEN: tag=279, str=#279
TOKEN: tag=280, str=(
TOKEN: tag=268, str=c
TOKEN: tag=265, str===
TOKEN: tag=273, str=10
TOKEN: tag=281, str=)
TOKEN: tag=281, str=)
TOKEN: tag=282, str={
TOKEN: tag=268, str=c
TOKEN: tag=293, str==
TOKEN: tag=268, str=c
TOKEN: tag=288, str=+
TOKEN: tag=273, str=1
TOKEN: tag=285, str=;
TOKEN: tag=283, str=}
TOKEN: tag=269, str=if
TOKEN: tag=280, str=(
TOKEN: tag=279, str=#279
TOKEN: tag=280, str=(
TOKEN: tag=268, str=a
TOKEN: tag=263, str=<
TOKEN: tag=268, str=b
TOKEN: tag=281, str=)
TOKEN: tag=256, str=&&
TOKEN: tag=279, str=#279
TOKEN: tag=280, str=(
TOKEN: tag=268, str=c
TOKEN: tag=272, str=!=
TOKEN: tag=273, str=10
TOKEN: tag=281, str=)
TOKEN: tag=281, str=)
TOKEN: tag=282, str={
TOKEN: tag=268, str=c
TOKEN: tag=293, str==
TOKEN: tag=268, str=c
TOKEN: tag=288, str=+
TOKEN: tag=273, str=2
TOKEN: tag=285, str=;
TOKEN: tag=283, str=}
TOKEN: tag=283, str=}
TOKEN: tag=0, str=#0
L1:
[IR] MOVE  t1 <- 3
        a = 3
[IR] STORE MEM[0 + t0 * 4] <- t1
L3:
[IR] MOVE  t2 <- 4
        b = 4
[IR] STORE MEM[4 + t0 * 4] <- t2
L4:
[IR] LOAD  t4 <- MEM[0 + t0 * 4]
[IR] LOAD  t6 <- MEM[4 + t0 * 4]
[IR] MOVE  t7 <- 2
[IR] MUL   t5 <- t6 * t7
[IR] ADD   t3 <- t4 + t5
        c = a + b * 2
[IR] STORE MEM[8 + t0 * 4] <- t3
L5:
[IR] MOVE  t9 <- 1
[IR] MOVE  t10 <- 4
[IR] MUL   t8 <- t9 * t10
[IR] LOAD  t11 <- MEM[8 + t0 * 4]
        arr [ 1 * 4 ] = c
[IR] STORE MEM[12 + t8 * 4] <- t11
L6:
[IR] MOVE  t14 <- 1
[IR] MOVE  t15 <- 4
[IR] MUL   t13 <- t14 * t15
[IR] LOAD  t12 <- MEM[12 + t13 * 4]
        a = arr [ 1 * 4 ]
[IR] STORE MEM[0 + t0 * 4] <- t12
L7:
[IR] LOAD  t4 <- MEM[0 + t0 * 4]
[IR] LOAD  t6 <- MEM[4 + t0 * 4]
[IR] IFFALSE (t4 < t6) goto L8
        iffalse a < b goto L8
[IR] LOAD  t11 <- MEM[8 + t0 * 4]
[IR] MOVE  t16 <- 10
[IR] IFFALSE (t11 == t16) goto L8
        iffalse c == 10 goto L8
L9:
[IR] LOAD  t11 <- MEM[8 + t0 * 4]
[IR] MOVE  t18 <- 1
[IR] SUB   t17 <- t11 - t18
        c = c - 1
[IR] STORE MEM[8 + t0 * 4] <- t17
L8:
[IR] LOAD  t4 <- MEM[0 + t0 * 4]
[IR] LOAD  t6 <- MEM[4 + t0 * 4]
[IR] IFFALSE (t4 > t6) goto L0
        if a > b goto L12
[IR] LOAD  t11 <- MEM[8 + t0 * 4]
[IR] MOVE  t19 <- 10
[IR] IFFALSE (t11 == t19) goto L0
        if c == 10 goto L10
L12:
L11:
[IR] LOAD  t11 <- MEM[8 + t0 * 4]
[IR] MOVE  t21 <- 1
[IR] ADD   t20 <- t11 + t21
        c = c + 1
[IR] STORE MEM[8 + t0 * 4] <- t20
L10:
[IR] LOAD  t4 <- MEM[0 + t0 * 4]
[IR] LOAD  t6 <- MEM[4 + t0 * 4]
[IR] IFFALSE (t4 < t6) goto L0
        if a < b goto L2
[IR] LOAD  t11 <- MEM[8 + t0 * 4]
[IR] MOVE  t22 <- 10
[IR] IFFALSE (t11 != t22) goto L0
        if c != 10 goto L2
L13:
[IR] LOAD  t11 <- MEM[8 + t0 * 4]
[IR] MOVE  t24 <- 2
[IR] ADD   t23 <- t11 + t24
        c = c + 2
[IR] STORE MEM[8 + t0 * 4] <- t23
L2:
BPF sum result: 150
```

