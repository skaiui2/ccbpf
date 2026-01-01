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
TOKEN: tag=278, str={
TOKEN: tag=257, str=int
TOKEN: tag=264, str=i
TOKEN: tag=281, str=;
TOKEN: tag=257, str=int
TOKEN: tag=264, str=j
TOKEN: tag=281, str=;
TOKEN: tag=257, str=float
TOKEN: tag=264, str=v
TOKEN: tag=281, str=;
TOKEN: tag=257, str=float
TOKEN: tag=264, str=x
TOKEN: tag=281, str=;
TOKEN: tag=257, str=float
TOKEN: tag=297, str=[
TOKEN: tag=269, str=100
TOKEN: tag=298, str=]
TOKEN: tag=264, str=a
TOKEN: tag=281, str=;
TOKEN: tag=264, str=x
TOKEN: tag=289, str==
TOKEN: tag=271, str=3.140000
constant_float: v = 3.140000
TOKEN: tag=281, str=;
TOKEN: tag=274, str=while
TOKEN: tag=276, str=(
TOKEN: tag=273, str=true
TOKEN: tag=277, str=)
TOKEN: tag=278, str={
TOKEN: tag=259, str=do
TOKEN: tag=264, str=i
TOKEN: tag=289, str==
TOKEN: tag=264, str=i
TOKEN: tag=284, str=+
TOKEN: tag=269, str=1
TOKEN: tag=281, str=;
TOKEN: tag=274, str=while
TOKEN: tag=276, str=(
TOKEN: tag=264, str=a
TOKEN: tag=297, str=[
TOKEN: tag=264, str=i
TOKEN: tag=298, str=]
TOKEN: tag=60, str=#60
TOKEN: tag=264, str=v
TOKEN: tag=277, str=)
TOKEN: tag=281, str=;
TOKEN: tag=259, str=do
TOKEN: tag=264, str=j
TOKEN: tag=289, str==
TOKEN: tag=264, str=j
TOKEN: tag=285, str=-
TOKEN: tag=269, str=1
TOKEN: tag=281, str=;
TOKEN: tag=274, str=while
TOKEN: tag=276, str=(
TOKEN: tag=264, str=a
TOKEN: tag=297, str=[
TOKEN: tag=264, str=j
TOKEN: tag=298, str=]
TOKEN: tag=62, str=#62
TOKEN: tag=264, str=v
TOKEN: tag=277, str=)
TOKEN: tag=281, str=;
TOKEN: tag=265, str=if
TOKEN: tag=276, str=(
TOKEN: tag=264, str=i
TOKEN: tag=263, str=>=
TOKEN: tag=264, str=j
TOKEN: tag=277, str=)
TOKEN: tag=258, str=break
TOKEN: tag=281, str=;
TOKEN: tag=264, str=x
TOKEN: tag=289, str==
TOKEN: tag=264, str=a
TOKEN: tag=297, str=[
TOKEN: tag=264, str=i
TOKEN: tag=298, str=]
TOKEN: tag=281, str=;
TOKEN: tag=264, str=a
TOKEN: tag=297, str=[
TOKEN: tag=264, str=i
TOKEN: tag=298, str=]
TOKEN: tag=289, str==
TOKEN: tag=264, str=a
TOKEN: tag=297, str=[
TOKEN: tag=264, str=j
TOKEN: tag=298, str=]
TOKEN: tag=281, str=;
TOKEN: tag=264, str=a
TOKEN: tag=297, str=[
TOKEN: tag=264, str=j
TOKEN: tag=298, str=]
TOKEN: tag=289, str==
TOKEN: tag=264, str=x
TOKEN: tag=281, str=;
TOKEN: tag=279, str=}
TOKEN: tag=279, str=}
TOKEN: tag=0, str=#0
L1:
        x = 3.140000
L3:
L4:
        iffalse true goto L2
L6:
        i = i + 1
        if a [ i * 8 ] #60 v goto L6
L5:
L8:
        j = j - 1
        if a [ j * 8 ] #62 v goto L8
L7:
        iffalse i >= j goto L9
L10:
        goto L2
L9:
        x = a [ i * 8 ]
L11:
        a [ i * 8 ] = a [ j * 8 ]
L12:
        a [ j * 8 ] = x
        goto L4
L2:
```

