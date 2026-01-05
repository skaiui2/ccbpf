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
TOKEN: tag=268, str=i
TOKEN: tag=285, str=;
TOKEN: tag=257, str=float
TOKEN: tag=268, str=x
TOKEN: tag=285, str=;
TOKEN: tag=257, str=float
TOKEN: tag=268, str=v
TOKEN: tag=285, str=;
TOKEN: tag=257, str=float
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=5
TOKEN: tag=302, str=]
TOKEN: tag=285, str=;
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=0
TOKEN: tag=302, str=]
TOKEN: tag=293, str==
TOKEN: tag=275, str=5.000000
TOKEN: tag=285, str=;
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=1
TOKEN: tag=302, str=]
TOKEN: tag=293, str==
TOKEN: tag=275, str=1.000000
TOKEN: tag=285, str=;
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=2
TOKEN: tag=302, str=]
TOKEN: tag=293, str==
TOKEN: tag=275, str=4.000000
TOKEN: tag=285, str=;
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=3
TOKEN: tag=302, str=]
TOKEN: tag=293, str==
TOKEN: tag=275, str=2.000000
TOKEN: tag=285, str=;
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=4
TOKEN: tag=302, str=]
TOKEN: tag=293, str==
TOKEN: tag=275, str=3.000000
TOKEN: tag=285, str=;
TOKEN: tag=268, str=v
TOKEN: tag=293, str==
TOKEN: tag=275, str=3.140000
TOKEN: tag=285, str=;
TOKEN: tag=268, str=x
TOKEN: tag=293, str==
TOKEN: tag=268, str=v
TOKEN: tag=261, str=&
TOKEN: tag=273, str=1
TOKEN: tag=285, str=;
TOKEN: tag=268, str=x
TOKEN: tag=293, str==
TOKEN: tag=268, str=x
TOKEN: tag=262, str=|
TOKEN: tag=273, str=1
TOKEN: tag=285, str=;
TOKEN: tag=269, str=if
TOKEN: tag=280, str=(
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=0
TOKEN: tag=302, str=]
TOKEN: tag=264, str=>
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=1
TOKEN: tag=302, str=]
TOKEN: tag=281, str=)
TOKEN: tag=282, str={
TOKEN: tag=268, str=x
TOKEN: tag=293, str==
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=0
TOKEN: tag=302, str=]
TOKEN: tag=285, str=;
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=0
TOKEN: tag=302, str=]
TOKEN: tag=293, str==
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=1
TOKEN: tag=302, str=]
TOKEN: tag=285, str=;
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=1
TOKEN: tag=302, str=]
TOKEN: tag=293, str==
TOKEN: tag=268, str=x
TOKEN: tag=285, str=;
TOKEN: tag=283, str=}
TOKEN: tag=269, str=if
TOKEN: tag=280, str=(
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=1
TOKEN: tag=302, str=]
TOKEN: tag=264, str=>
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=2
TOKEN: tag=302, str=]
TOKEN: tag=281, str=)
TOKEN: tag=282, str={
TOKEN: tag=268, str=x
TOKEN: tag=293, str==
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=1
TOKEN: tag=302, str=]
TOKEN: tag=285, str=;
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=1
TOKEN: tag=302, str=]
TOKEN: tag=293, str==
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=2
TOKEN: tag=302, str=]
TOKEN: tag=285, str=;
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=2
TOKEN: tag=302, str=]
TOKEN: tag=293, str==
TOKEN: tag=268, str=x
TOKEN: tag=285, str=;
TOKEN: tag=283, str=}
TOKEN: tag=269, str=if
TOKEN: tag=280, str=(
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=2
TOKEN: tag=302, str=]
TOKEN: tag=264, str=>
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=3
TOKEN: tag=302, str=]
TOKEN: tag=281, str=)
TOKEN: tag=282, str={
TOKEN: tag=268, str=x
TOKEN: tag=293, str==
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=2
TOKEN: tag=302, str=]
TOKEN: tag=285, str=;
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=2
TOKEN: tag=302, str=]
TOKEN: tag=293, str==
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=3
TOKEN: tag=302, str=]
TOKEN: tag=285, str=;
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=3
TOKEN: tag=302, str=]
TOKEN: tag=293, str==
TOKEN: tag=268, str=x
TOKEN: tag=285, str=;
TOKEN: tag=283, str=}
TOKEN: tag=269, str=if
TOKEN: tag=280, str=(
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=3
TOKEN: tag=302, str=]
TOKEN: tag=264, str=>
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=4
TOKEN: tag=302, str=]
TOKEN: tag=281, str=)
TOKEN: tag=282, str={
TOKEN: tag=268, str=x
TOKEN: tag=293, str==
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=3
TOKEN: tag=302, str=]
TOKEN: tag=285, str=;
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=3
TOKEN: tag=302, str=]
TOKEN: tag=293, str==
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=4
TOKEN: tag=302, str=]
TOKEN: tag=285, str=;
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=4
TOKEN: tag=302, str=]
TOKEN: tag=293, str==
TOKEN: tag=268, str=x
TOKEN: tag=285, str=;
TOKEN: tag=283, str=}
TOKEN: tag=269, str=if
TOKEN: tag=280, str=(
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=0
TOKEN: tag=302, str=]
TOKEN: tag=264, str=>
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=1
TOKEN: tag=302, str=]
TOKEN: tag=281, str=)
TOKEN: tag=282, str={
TOKEN: tag=268, str=x
TOKEN: tag=293, str==
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=0
TOKEN: tag=302, str=]
TOKEN: tag=285, str=;
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=0
TOKEN: tag=302, str=]
TOKEN: tag=293, str==
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=1
TOKEN: tag=302, str=]
TOKEN: tag=285, str=;
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=1
TOKEN: tag=302, str=]
TOKEN: tag=293, str==
TOKEN: tag=268, str=x
TOKEN: tag=285, str=;
TOKEN: tag=283, str=}
TOKEN: tag=269, str=if
TOKEN: tag=280, str=(
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=1
TOKEN: tag=302, str=]
TOKEN: tag=264, str=>
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=2
TOKEN: tag=302, str=]
TOKEN: tag=281, str=)
TOKEN: tag=282, str={
TOKEN: tag=268, str=x
TOKEN: tag=293, str==
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=1
TOKEN: tag=302, str=]
TOKEN: tag=285, str=;
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=1
TOKEN: tag=302, str=]
TOKEN: tag=293, str==
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=2
TOKEN: tag=302, str=]
TOKEN: tag=285, str=;
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=2
TOKEN: tag=302, str=]
TOKEN: tag=293, str==
TOKEN: tag=268, str=x
TOKEN: tag=285, str=;
TOKEN: tag=283, str=}
TOKEN: tag=269, str=if
TOKEN: tag=280, str=(
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=2
TOKEN: tag=302, str=]
TOKEN: tag=264, str=>
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=3
TOKEN: tag=302, str=]
TOKEN: tag=281, str=)
TOKEN: tag=282, str={
TOKEN: tag=268, str=x
TOKEN: tag=293, str==
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=2
TOKEN: tag=302, str=]
TOKEN: tag=285, str=;
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=2
TOKEN: tag=302, str=]
TOKEN: tag=293, str==
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=3
TOKEN: tag=302, str=]
TOKEN: tag=285, str=;
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=3
TOKEN: tag=302, str=]
TOKEN: tag=293, str==
TOKEN: tag=268, str=x
TOKEN: tag=285, str=;
TOKEN: tag=283, str=}
TOKEN: tag=269, str=if
TOKEN: tag=280, str=(
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=3
TOKEN: tag=302, str=]
TOKEN: tag=264, str=>
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=4
TOKEN: tag=302, str=]
TOKEN: tag=281, str=)
TOKEN: tag=282, str={
TOKEN: tag=268, str=x
TOKEN: tag=293, str==
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=3
TOKEN: tag=302, str=]
TOKEN: tag=285, str=;
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=3
TOKEN: tag=302, str=]
TOKEN: tag=293, str==
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=4
TOKEN: tag=302, str=]
TOKEN: tag=285, str=;
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=273, str=4
TOKEN: tag=302, str=]
TOKEN: tag=293, str==
TOKEN: tag=268, str=x
TOKEN: tag=285, str=;
TOKEN: tag=283, str=}
TOKEN: tag=283, str=}
TOKEN: tag=0, str=#0
L1:
        a [ 0 * 8 ] = 5.000000
L3:
        a [ 1 * 8 ] = 1.000000
L4:
        a [ 2 * 8 ] = 4.000000
L5:
        a [ 3 * 8 ] = 2.000000
L6:
        a [ 4 * 8 ] = 3.000000
L7:
        v = 3.140000
L8:
        x = v & 1
L9:
        x = x | 1
L10:
        iffalse a [ 0 * 8 ] > a [ 1 * 8 ] goto L11
L12:
        x = a [ 0 * 8 ]
L13:
        a [ 0 * 8 ] = a [ 1 * 8 ]
L14:
        a [ 1 * 8 ] = x
L11:
        iffalse a [ 1 * 8 ] > a [ 2 * 8 ] goto L15
L16:
        x = a [ 1 * 8 ]
L17:
        a [ 1 * 8 ] = a [ 2 * 8 ]
L18:
        a [ 2 * 8 ] = x
L15:
        iffalse a [ 2 * 8 ] > a [ 3 * 8 ] goto L19
L20:
        x = a [ 2 * 8 ]
L21:
        a [ 2 * 8 ] = a [ 3 * 8 ]
L22:
        a [ 3 * 8 ] = x
L19:
        iffalse a [ 3 * 8 ] > a [ 4 * 8 ] goto L23
L24:
        x = a [ 3 * 8 ]
L25:
        a [ 3 * 8 ] = a [ 4 * 8 ]
L26:
        a [ 4 * 8 ] = x
L23:
        iffalse a [ 0 * 8 ] > a [ 1 * 8 ] goto L27
L28:
        x = a [ 0 * 8 ]
L29:
        a [ 0 * 8 ] = a [ 1 * 8 ]
L30:
        a [ 1 * 8 ] = x
L27:
        iffalse a [ 1 * 8 ] > a [ 2 * 8 ] goto L31
L32:
        x = a [ 1 * 8 ]
L33:
        a [ 1 * 8 ] = a [ 2 * 8 ]
L34:
        a [ 2 * 8 ] = x
L31:
        iffalse a [ 2 * 8 ] > a [ 3 * 8 ] goto L35
L36:
        x = a [ 2 * 8 ]
L37:
        a [ 2 * 8 ] = a [ 3 * 8 ]
L38:
        a [ 3 * 8 ] = x
L35:
        iffalse a [ 3 * 8 ] > a [ 4 * 8 ] goto L2
L39:
        x = a [ 3 * 8 ]
L40:
        a [ 3 * 8 ] = a [ 4 * 8 ]
L41:
        a [ 4 * 8 ] = x
L2:
BPF sum result: 150
```

