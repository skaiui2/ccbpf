# ccbpf
A compiler, implement a hook language similar to 4.4FreeBSD's BPF.

## run

```
mkdir build
cmake ..
make
./littlecompiler
```

like this:

```
skaiuijing@ubuntu:~/compiler/build$ ./littleCompiler 
TOKEN: tag=282, str={
TOKEN: tag=257, str=int
TOKEN: tag=268, str=i
TOKEN: tag=285, str=;
TOKEN: tag=257, str=int
TOKEN: tag=268, str=j
TOKEN: tag=285, str=;
TOKEN: tag=257, str=float
TOKEN: tag=268, str=v
TOKEN: tag=285, str=;
TOKEN: tag=257, str=float
TOKEN: tag=268, str=x
TOKEN: tag=285, str=;
TOKEN: tag=257, str=float
TOKEN: tag=301, str=[
TOKEN: tag=273, str=100
TOKEN: tag=302, str=]
TOKEN: tag=268, str=a
TOKEN: tag=285, str=;
TOKEN: tag=268, str=x
TOKEN: tag=293, str==
TOKEN: tag=275, str=3.140000
TOKEN: tag=285, str=;
TOKEN: tag=268, str=x
TOKEN: tag=293, str==
TOKEN: tag=268, str=x
TOKEN: tag=261, str=&
TOKEN: tag=273, str=1
TOKEN: tag=285, str=;
TOKEN: tag=268, str=x
TOKEN: tag=293, str==
TOKEN: tag=268, str=x
TOKEN: tag=262, str=|
TOKEN: tag=273, str=1
TOKEN: tag=285, str=;
TOKEN: tag=278, str=while
TOKEN: tag=280, str=(
TOKEN: tag=277, str=true
TOKEN: tag=281, str=)
TOKEN: tag=282, str={
TOKEN: tag=259, str=do
TOKEN: tag=268, str=i
TOKEN: tag=293, str==
TOKEN: tag=268, str=i
TOKEN: tag=288, str=+
TOKEN: tag=273, str=1
TOKEN: tag=285, str=;
TOKEN: tag=278, str=while
TOKEN: tag=280, str=(
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=268, str=i
TOKEN: tag=302, str=]
TOKEN: tag=263, str=<
TOKEN: tag=268, str=v
TOKEN: tag=281, str=)
TOKEN: tag=285, str=;
TOKEN: tag=259, str=do
TOKEN: tag=268, str=j
TOKEN: tag=293, str==
TOKEN: tag=268, str=j
TOKEN: tag=289, str=-
TOKEN: tag=273, str=1
TOKEN: tag=285, str=;
TOKEN: tag=278, str=while
TOKEN: tag=280, str=(
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=268, str=j
TOKEN: tag=302, str=]
TOKEN: tag=264, str=>
TOKEN: tag=268, str=v
TOKEN: tag=281, str=)
TOKEN: tag=285, str=;
TOKEN: tag=269, str=if
TOKEN: tag=280, str=(
TOKEN: tag=268, str=i
TOKEN: tag=267, str=>=
TOKEN: tag=268, str=j
TOKEN: tag=281, str=)
TOKEN: tag=258, str=break
TOKEN: tag=285, str=;
TOKEN: tag=268, str=x
TOKEN: tag=293, str==
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=268, str=i
TOKEN: tag=302, str=]
TOKEN: tag=285, str=;
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=268, str=i
TOKEN: tag=302, str=]
TOKEN: tag=293, str==
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=268, str=j
TOKEN: tag=302, str=]
TOKEN: tag=285, str=;
TOKEN: tag=268, str=a
TOKEN: tag=301, str=[
TOKEN: tag=268, str=j
TOKEN: tag=302, str=]
TOKEN: tag=293, str==
TOKEN: tag=268, str=x
TOKEN: tag=285, str=;
TOKEN: tag=283, str=}
TOKEN: tag=283, str=}
TOKEN: tag=0, str=#0
L1:
        x = 3.140000
L3:
        x = x & 1
L4:
        x = x | 1
L5:
L6:
        iffalse true goto L2
L8:
        i = i + 1
        if a [ i * 8 ] < v goto L8
L7:
L10:
        j = j - 1
        if a [ j * 8 ] > v goto L10
L9:
        iffalse i >= j goto L11
L12:
        goto L2
L11:
        x = a [ i * 8 ]
L13:
        a [ i * 8 ] = a [ j * 8 ]
L14:
        a [ j * 8 ] = x
        goto L6
L2:
BPF sum result: 150
```

