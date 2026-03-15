# ccbpf User Manual

ccbpf consists of two major components: the **compiler** and the **virtual machine (VM)**.

The recommended usage pattern is: run the compiler on a resource‑rich node, send the compiled program to target nodes, and let the target nodes execute the program using the VM.

# ccbpf C Subset Specification

ccbpf uses a restricted subset of the C language as its source language.
 This subset is designed specifically for **safe, verifiable, and predictable kernel hook programs**, with syntax strictly defined by the compiler frontend.

This specification describes all syntax features supported by ccbpf.

# Design Goals

The ccbpf C subset is designed to be:

- **Verifiable**: no loops, no pointer arithmetic, no complex control flow
- **Predictable**: all memory accesses are statically known
- **Mappable**: syntax maps directly to BPF instructions
- **Safe**: ctx access is strictly constrained
- **Portable**: suitable for MCU environments

# 1. Program Structure

A ccbpf program must contain **exactly one** entry function:

```c
int hook(void *ctx)
{
    ...
}
```

Requirements:

- The function name must be `hook`
- The return type must be `int`
- The parameter must be `void *ctx`
- No other function definitions are allowed
- Recursion, function pointers, and variadic arguments are not supported

# 2. Type System

ccbpf supports the following types.

## 2.1 Basic Types

Since the VM currently does not support signed arithmetic, `unsigned int` and `int` behave identically. Signedness is reserved for future extension.

### **Integer Semantics (Important)**

ccbpf VM is based on classic BPF, so all arithmetic and comparisons use **unsigned semantics**:

- `int` behaves as `uint32_t`
- `short` behaves as `uint16_t`
- `char` behaves as `uint8_t`
- All comparisons are unsigned
- All right shifts are logical shifts
- All arithmetic is unsigned
- Casts like `(int)`, `(short)`, `(char)` do **not** change signedness

| Type             | Width | Semantics |
| ---------------- | ----- | --------- |
| `int`            | 4     | unsigned  |
| `bool`           | 1     | unsigned  |
| `char`           | 1     | unsigned  |
| `short`          | 2     | unsigned  |
| `unsigned int`   | 4     | unsigned  |
| `unsigned short` | 2     | unsigned  |
| `unsigned char`  | 1     | unsigned  |

## 2.2 Pointer Types

Pointers of any depth are supported:

```c
int *p;
struct hdr *h;
unsigned char **pp;
```

Restrictions:

- Pointer arithmetic is not supported (`p+1`, `p++`, etc.)
- Dereference (`*p`) is not supported
- Pointers are mainly used for **ctx‑derived struct pointers**

## 2.3 Arrays

```c
int a[10];
char buf[32];
```

Rules:

- Array size must be a constant
- `a[i]` is supported
- Multidimensional arrays are not supported

## 2.4 Struct Types

Struct definitions are supported:

```c
struct udp_hdr {
    unsigned short sport;
    unsigned short dport;
};
```

Supported:

- Multiple fields
- Field widths accumulate automatically
- Field offsets computed by the compiler

Not supported:

- Nested structs
- Struct assignment
- Struct as a local variable (only pointers allowed)

## 2.5 Struct Pointers

Supported:

```c
struct udp_hdr *uh;
uh->sport;
```

But only when derived from:

```c
(struct T *)&ctx[offset]
```

# 3. ctx Access Model (Core)

The core semantic of ccbpf is **ctx access**.

## 3.1 ctx Array Access

```c
ctx[0]
ctx[12]
```

Rules:

- Index must be a compile‑time constant
- Only used to derive struct pointers

## 3.2 ctx -> struct Pointer

```c
struct udp_hdr *uh = (struct udp_hdr *)&ctx[0];
```

The compiler will:

- Record the struct type
- Record the base offset
- Allow `uh->field` access

## 3.3 ctx Field Access

```c
x = uh->sport;
```

The compiler lowers this to:

```
ctx_load(offset + field_offset)
```

# 4. Expression Support

ccbpf supports the following expressions:

## 4.1 Arithmetic Expressions

```
+   -   *   /   %
```

## 4.2 Comparison Expressions

```
<   <=   >   >=   ==   !=
```

## 4.3 Logical Expressions

```
&&   ||
```

## 4.4 Bitwise Operations

```
&   |
```

## 4.5 Unary Operations

```
-   !   &ctx[CONST]
```

Restrictions:

- `&expr` is only allowed for `&ctx[constant]`
- General address‑of operations are not supported

# 5. Statement Support

## 5.1 Assignment

```c
x = y;
arr[i] = v;
```

## 5.2 return

```c
return x + y;
```

## 5.3 if / else

```c
if (cond) { ... }
else { ... }
```

## 5.4 Code Blocks

```c
{
    ...
}
```

## 5.5 Empty Statement

```c
;
```

# 6. Native Functions (User‑Registered)

ccbpf allows **users to register their own callable functions**.
 The compiler treats them purely as “symbol + argument count”, while the VM provides the actual implementation.

Native functions form a **user‑defined ABI**.

# 6.1 How Native Functions Work (Important)

The entire mechanism consists of two parts:

## (1) Compiler Side: Declaring a Native Function

You tell the compiler:

- Function name (string)
- Function ID (`func_id`)
- Number of arguments (`argc`)

Example:

```c
native_decl_register("print", 3, 1);
native_decl_register("map_update", 6, 3);
native_decl_register("now_ms", 7, 0);
```

What the compiler does:

- Recognizes the function name
- Checks the argument count
- Emits `NATIVE_CALL func_id=3 argc=1` in the IR
- **Does not care about the implementation**

## (2) VM Side: Implementing the Native Function

You must write a wrapper:

```c
uint32_t native_print(struct ccbpf_program *p,
                      uint32_t a0,
                      uint32_t a1,
                      uint32_t a2,
                      uint32_t a3)
{
    printf("%u", a0);
    return 0;
}
```

And register it:

```c
native_register(3, 1, native_print);
```

The VM is responsible for:

- Looking up the function by `func_id`
- Passing arguments according to `argc`
- Calling the wrapper
- Returning the result

# 6.2 Argument Passing Mechanism

ccbpf always uses **four argument slots**:

```
a0, a1, a2, a3
```

If the function is declared as:

```c
map_update(mapid, key, value)
```

The compiler generates:

```
a0 = mapid
a1 = key
a2 = value
```

The VM calls:

```c
native_map_update(p, a0, a1, a2, a3);
```

**Unused arguments are always zero.**

# 6.3 String Mechanism

String literal:

```c
print_str("hello");
```

The compiler places `"hello"` into the **string pool**, and replaces it with a string ID:

```
print_str(0);   // 0 is the ID of "hello"
```

VM wrapper:

```c
uint32_t native_print_str(struct ccbpf_program *p,
                          uint32_t a0,
                          uint32_t a1,
                          uint32_t a2,
                          uint32_t a3)
{
    if (a0 >= p->string_count)
        return 0;

    printf("%s", p->strings[a0]);
    return 0;
}
```

Meaning:

- **`a0` is the string ID**
- **`p->strings[a0]` is the actual string**

# 6.4 Map Mechanism

Maps are simple key‑value stores.

### Lookup

```c
uint32_t map_lookup(uint32_t mapid, uint32_t key);
```

VM:

```c
void *val_ptr = hashmap_get(&p->maps[map_id], key);
return val_ptr ? (uint32_t)val_ptr : 0;
```

### Update

```c
void map_update(uint32_t mapid, uint32_t key, uint32_t value);
```

VM:

```c
hashmap_put(&p->maps[map_id], key, value);
```

Characteristics:

- Keys and values are 32‑bit integers
- Complex types are not supported
- Deletion is not supported (use value=0 as a workaround)

# 6.5 Example: Writing a Native Function

Suppose you want to add:

```c
add(a, b)
```

## Compiler Side

```c
native_decl_register("add", 10, 2);
```

## VM Side

```c
uint32_t native_add(struct ccbpf_program *p,
                    uint32_t a0,
                    uint32_t a1,
                    uint32_t a2,
                    uint32_t a3)
{
    return a0 + a1;
}

native_register(10, 2, native_add);
```

## User C Program

```c
int hook(void *ctx)
{
    return add(10, 20);
}
```

# 6.6 Default Native Functions in the Demo

| Name       | func_id | Args | Description                    |
| ---------- | ------- | ---- | ------------------------------ |
| ntohl      | 1       | 1    | Network → host (32‑bit)        |
| ntohs      | 2       | 1    | Network → host (16‑bit)        |
| print      | 3       | 1    | Print integer                  |
| print_str  | 4       | 1    | Print string (via string pool) |
| map_lookup | 5       | 2    | Lookup map                     |
| map_update | 6       | 3    | Update map                     |
| now_ms     | 7       | 0    | Current time in ms             |

These are only defaults—users may extend freely.

# 7. Constant Support

## 7.1 Integer Constants

```
123
0
42
```

## 7.2 Boolean Constants

```
true
false
```

## 7.3 String Constants

```
"hello"
```

# Unsupported Syntax

The following constructs are **explicitly unsupported** because they are not implemented in `parser.c`:

| Unsupported             | Reason                   |
| ----------------------- | ------------------------ |
| for / while / do        | No loop support          |
| Multiple functions      | Only `hook()` is allowed |
| switch                  | `if` is sufficient       |
| typedef                 | Unnecessary              |
| Global variables        | Use maps instead         |
| Pointer arithmetic      | Unsafe                   |
| Dereference `*p`        | Unsafe                   |
| `&expr` (except ctx)    | Unsafe                   |
| Struct assignment       | Unnecessary              |
| Nested structs          | Unnecessary              |
| Multidimensional arrays | Unnecessary              |
| Floating point          | Unnecessary              |

# Example Program

```c
struct udp_hdr {
    unsigned short sport;
    unsigned short dport;
};

int hook(void *ctx)
{
    struct udp_hdr *uh = (struct udp_hdr *)&ctx[0];

    unsigned int s = ntohs(uh->sport);
    unsigned int d = ntohs(uh->dport);

    print(s);
    print(d);

    return s + d;
}
```

## Usage Examples

Here are several demo programs that can be used as references when writing ccbpf programs.

### Basic Arithmetic

```c
int hook(void *ctx)
{
    int a;
    int b;
    int c;
    int arr[4];

    a = 3;
    b = 4;
    c = a + b * 2;

    arr[1] = c;
    a = arr[1];

    if (a < b && c == 10) {
        c = c - 1;
    }

    if (a > b || !(c == 10)) {
        c = c + 1;
    }

    if (!(a < b) && !(c != 10)) {
        c = c + 2;
    }
    return 0;
}
```

### Map Usage and Updates

Here, `count` is used to track how many UDP packets have been observed:

```c
struct udp_hdr {
    unsigned int sport;
    unsigned int dport;
};

int hook(void *ctx)
{
    struct udp_hdr *uh;
    unsigned int key;
    unsigned int count;
    uh = (struct udp_hdr *)ctx;

    key = ntohs(uh->sport);
    count = map_lookup(0, key);

    count = count + 1;
    map_update(0, key, count);

    print(count);

    return 0;
}
```

### Test Demo

```c
struct udp_hdr {
    unsigned short sport;
    unsigned short dport;
};

int hook(void *ctx)
{
    unsigned int x;
    unsigned int y;
    unsigned int key;
    unsigned int val;
    struct udp_hdr *uh;

    uh = (struct udp_hdr *)&ctx[0];
    x = ntohs(uh->sport);
    print(x);
    y = ntohs(uh->dport);
    print(y);

    key = x;
    val = y;

    map_update(0, key, val);

    val = map_lookup(0, key);
    print(val);

    print(map_lookup(0, 9999)); 
    map_update(0, 1, 11);
    map_update(0, 2, 22);
    map_update(0, 3, 33);
    print(map_lookup(0, 1));
    print(map_lookup(0, 2));
    print(map_lookup(0, 3));

    return x + y;
}
```

# Printing Capabilities

The `print` facility only supports two operations:

- printing **strings**, and
- printing **numbers**.

However, by combining these two operations, you can implement any form of formatted or structured output.

Corrected example (using `print_str` for string literals):

```c
struct udp_hdr {
    unsigned short sport;
    unsigned short dport;
};

int hook(void *ctx)
{
    struct udp_hdr *uh;
    unsigned int sport;
    unsigned int dport;

    uh = (struct udp_hdr *)ctx;
    sport = ntohs(uh->sport);
    dport = ntohs(uh->dport);

    print_str("UDP ");
    print_str("sport=");
    print(sport);
    print_str(" dport=");
    print(dport);
    print_str("\n");

    return 0;
}
```

# Compiling C Source Code into a `.ccbpf` Program

The full compilation pipeline of ccbpf is:

- Input: a piece of C source code
- Output: a loadable `.ccbpf` program image
- Steps: **Lexing → Parsing → IR → BPF → Packaging → (optional) write to filesystem**

## Providing C Source Code

You may obtain the C source string from a file or any other input method.

```c
const char *src =
    "struct udp_hdr {\n"
    "    unsigned short sport;\n"
    "    unsigned short dport;\n"
    "};\n"
    "\n"
    "int hook(void *ctx)\n"
    "{\n"
    "    unsigned int x;\n"
    "    unsigned int y;\n"
    "    struct udp_hdr *uh;\n"
    "\n"
    "    uh = (struct udp_hdr *)&ctx[0];\n"
    "    x = ntohs(uh->sport);\n"
    "    print(x);\n"
    "    y = ntohs(uh->dport);\n"
    "    print(y);\n"
    "    return x + y;\n"
    "}\n";
```

## Initializing the Compiler

```c
compiler_init(16, (4*1024), (2*1024));
lexer_set_input_buffer(src, strlen(src));
```

- ```
  compiler_init()
  ```

   initializes memory pools for the frontend, IR, and backend.

  - `16` is the maximum memory block size (2^15).
  - The second argument configures the frontend memory pool.
  - The third argument configures the IR memory pool.

- `lexer_set_input_buffer()` sets the input source code.

## Lexical Analysis

```c
struct lexer lex;
lexer_init(&lex);
```

## Parsing (Generating the AST)

```c
struct Parser *p = parser_new(&lex);
parser_program(p);
```

## Cleaning Up Frontend Resources (Recommended)

```c
frontend_destroy(&lex);
```

## IR Stage

```c
struct ir_mes im;
ir_mes_get(&im);
```

At this point, the IR has been fully generated by the parser.

## IR → BPF Instruction Lowering

```c
struct bpf_builder b;
bpf_builder_init(&b, (3*1024));

ir_lower_program(im.ir_head, im.label_count, &b);

struct bpf_insn *prog = bpf_builder_data(&b);
int prog_len = bpf_builder_count(&b);
```

- `ir_lower_program()` lowers IR into a BPF instruction sequence
- `bpf_builder_data()` retrieves the final BPF program
- `bpf_builder_count()` returns the number of instructions

## Packaging into a `.ccbpf` Executable

```c
size_t image_len = 0;
uint8_t *image = ccbpf_pack_memory(prog, (size_t)prog_len, &image_len);

printf("=== CCBPF IMAGE READY ===\n");
printf("Image at %p, size = %u bytes\n", image, (unsigned)image_len);
```

`ccbpf_pack_memory()` produces a loadable `.ccbpf` program image.

## Freeing Resources

```c
bpf_builder_free(&b);
```

## Full Usage Example

```c
int main(void)
{
    // C source code to compile
    const char *src =
        "struct udp_hdr {\n"
        "    unsigned short sport;\n"
        "    unsigned short dport;\n"
        "};\n"
        "\n"
        "int hook(void *ctx)\n"
        "{\n"
        "    unsigned int x;\n"
        "    unsigned int y;\n"
        "    struct udp_hdr *uh;\n"
        "\n"
        "    uh = (struct udp_hdr *)&ctx[0];\n"
        "    x = ntohs(uh->sport);\n"
        "    print(x);\n"
        "    y = ntohs(uh->dport);\n"
        "    print(y);\n"
        "    return x + y;\n"
        "}\n";

    // Initialize compiler memory pools
    compiler_init(16, (4*1024), (2*1024));
    lexer_set_input_buffer(src, strlen(src));

    // Lexing
    struct lexer lex;
    lexer_init(&lex);

    // Parsing (build AST)
    struct Parser *p = parser_new(&lex);
    parser_program(p);

    // Free frontend resources
    frontend_destroy(&lex);

    // IR → BPF lowering
    struct bpf_builder b;
    bpf_builder_init(&b, (3*1024));

    struct ir_mes im;
    ir_mes_get(&im);

    ir_lower_program(im.ir_head, im.label_count, &b);

    struct bpf_insn *prog = bpf_builder_data(&b);
    int prog_len = bpf_builder_count(&b);

    // Package into .ccbpf image
    size_t image_len = 0;
    uint8_t *image = ccbpf_pack_memory(prog, (size_t)prog_len, &image_len);

    printf("=== CCBPF IMAGE READY ===\n");
    printf("Image at %p, size = %u bytes\n", image, (unsigned)image_len);

    // Cleanup
    bpf_builder_free(&b);

    return 0;
}
```

# VM Usage

The ccbpf virtual machine provides the following runtime capabilities:

- Load a `.ccbpf` program from memory (or from a file, if desired)
- Unload a program and free all associated resources
- Execute a program with a given frame/ctx
- Attach a program to a hook
- Execute all programs attached to a hook when the hook is triggered

The VM is the execution environment for ccbpf programs.

## Structure Overview

### **struct ccbpf_program**

```c
struct ccbpf_program {
    struct bpf_insn *insns;   // Instruction sequence
    size_t insn_count;        // Number of instructions

    char **strings;           // String constant pool
    int string_count;

    void *data;               // Reserved for future extensions
    size_t data_size;

    size_t map_count;         // Number of maps
    struct hashmap maps[CCBPF_MAX_MAPS];

    uint32_t entry;           // Entry offset
};
```

# API Overview

| API                        | Description                             |
| -------------------------- | --------------------------------------- |
| `ccbpf_load_from_memory()` | Load a `.ccbpf` program from memory     |
| `ccbpf_load()`             | Load a `.ccbpf` program from filesystem |
| `ccbpf_unload()`           | Unload a program and free resources     |
| `ccbpf_run_frame()`        | Execute a program with a frame/ctx      |
| `hook_attach()`            | Attach a program to a hook              |
| `hook_detach()`            | Detach all programs from a hook         |
| `hook_run()`               | Execute all programs attached to a hook |

Below is a detailed explanation of each API.

# ccbpf_load_from_memory()

```c
struct ccbpf_program *ccbpf_load_from_memory(const uint8_t *image, size_t len);
```

### **Description**

Loads a `.ccbpf` program from an in‑memory image.

### **Parameters**

| Name    | Type        | Description         |
| ------- | ----------- | ------------------- |
| `image` | `uint8_t *` | The `.ccbpf` image  |
| `len`   | `size_t`    | Length of the image |

### **Return Value**

- On success: a pointer to `struct ccbpf_program`
- On failure: `NULL`

### **Example**

```c
uint8_t *image = ...; // Read from file or network
size_t len = ...;

struct ccbpf_program *p = ccbpf_load_from_memory(image, len);
if (!p) {
    printf("load failed\n");
}
```

# ccbpf_unload()

```c
void ccbpf_unload(struct ccbpf_program *p);
```

### **Description**

Frees all resources associated with a program:

- Instruction memory
- String pool
- Maps
- Program structure itself

### **Parameters**

| Name | Type                     | Description       |
| ---- | ------------------------ | ----------------- |
| `p`  | `struct ccbpf_program *` | Program to unload |

# ccbpf_run_frame()

```c
uint32_t ccbpf_run_frame(struct ccbpf_program *prog,
                         void *frame,
                         size_t frame_size);
```

### **Description**

Executes the program’s entry point with the given frame (ctx).

### **Parameters**

| Name         | Type                     | Description    |
| ------------ | ------------------------ | -------------- |
| `prog`       | `struct ccbpf_program *` | Loaded program |
| `frame`      | `void *`                 | Context data   |
| `frame_size` | `size_t`                 | Size of ctx    |

### **Return Value**

- The program’s 32‑bit return value

### **Example**

```c
uint8_t frame[64];
fill_frame(frame);

uint32_t ret = ccbpf_run_frame(p, frame, sizeof(frame));
printf("hook returned %u\n", ret);
```

# hook_attach()

```c
int hook_attach(const char *hook_name, uint8_t *image, size_t len);
```

### **Description**

Attaches a `.ccbpf` program to a hook.

### **Parameters**

| Name        | Type           | Description            |
| ----------- | -------------- | ---------------------- |
| `hook_name` | `const char *` | Name of the hook       |
| `image`     | `uint8_t *`    | `.ccbpf` program image |
| `len`       | `size_t`       | Length of the image    |

### **Return Value**

- `0` on success
- `<0` on failure

### **Notes**

- A hook may have multiple programs attached
- Programs are executed in linked‑list order

# hook_detach()

```c
int hook_detach(const char *hook_name);
```

### **Description**

Detaches **all** programs from a hook.

### **Return Value**

- `0` on success
- `<0` on failure

# hook_run()

```c
uint32_t hook_run(const char *hook_name, uint8_t *frame, size_t frame_size);
```

### **Description**

Executes all programs attached to a hook.

### **Execution Order**

- Programs run sequentially in linked‑list order
- The return value is the return value of the **last** program

### **Parameters**

| Name         | Type           | Description        |
| ------------ | -------------- | ------------------ |
| `hook_name`  | `const char *` | Hook name          |
| `frame`      | `uint8_t *`    | Context/frame data |
| `frame_size` | `size_t`       | Size of ctx        |

### **Return Value**

- Return value of the last program executed

# Typical Runtime Workflow

```c
// 1. Load program from memory or file
struct ccbpf_program *p = ccbpf_load("/prog.ccbpf");

// 2. Attach to a hook
hook_attach("hook_udp_input", image, image_len);

// 3. Execute when the hook is triggered
uint32_t r = hook_run("hook_udp_input", frame, frame_size);

// 4. Detach and unload
hook_detach("hook_udp_input");
ccbpf_unload(p);
```

# 7. Hook Mechanism (Dynamic Registration)

The hook system in ccbpf is **fully dynamic**.
 The VM no longer uses a fixed list of hook points; instead, users may register new hook points at runtime.

# 7.1 Registering a Hook

A hook can be registered on the VM side using:

```c
void hook_register(const char *name)
{
    struct hook_entry *e = heap_malloc(sizeof(*e));
    e->name = heap_strdup(name);
    e->head = NULL;

    hashmap_put(&hook_table, (void*)e->name, e);
}
```

This function:

- Creates a new `hook_entry`
- Inserts it into the `hook_table` hashmap
- Initializes it with no attached programs (`head = NULL`)

**You may register any number of hook points.**

# 7.2 Hook Structure

The current `hook_entry` structure is:

```c
struct hook_entry {
    const char *name;
    struct ccbpf_program *head;   // Linked list of attached programs
};
```

Explanation:

- `name` is the hook name
- `head` is the head of the linked list of programs attached to this hook
- Each hook may have multiple attached programs
- Programs execute sequentially in linked‑list order

# 7.3 Attaching a Program to a Hook

If node A sends:

```
./nodeA attach hook_udp_input out.ccbpf
```

Then node B will:

- Look up the corresponding `hook_entry`
- Insert the program at the head (or tail) of the list
   *(your current implementation inserts at the head)*
- The program becomes active immediately

# 7.4 Detaching Programs

### Detach all programs from a hook

like:

```
./nodeB detach hook_udp_input
```

# 7.5 Hook Execution Flow

When a hook is triggered:

```c
struct hook_entry *e = hashmap_get(&hook_table, name);
for (prog = e->head; prog; prog = prog->next) {
    ccbpf_run(prog, ctx);
}
```

Characteristics:

- Multiple programs execute sequentially
- Each program receives the same ctx
- Each program has its own map storage
- Programs do not interfere with each other

# 7.6 Example: Registering a New Hook

Suppose you want to add hooks for a filesystem:

```c
hook_register("hook_fs_open");
hook_register("hook_fs_close");
```

Then users can write:

```c
int hook(void *ctx)
{
    print_str("file opened\n");
    return 0;
}
```

And attach it:

```
./node attach hook_fs_open out.ccbpf
```

# Usage Example

You may place a hook anywhere in your system—for example, inside a scheduler or protocol handler.

When no program is attached, the return value defaults to whatever your system defines.

```c
uint32_t udp_input(uint8_t *frame, size_t frame_size)
{
    // Trigger the hook here
    uint32_t ret = hook_run("hook_udp_input", frame, frame_size);

    // Normal UDP input processing
}
```

Another example:

```c
void task_switch_context(void)
{
    // Simulate a UDP packet event
    uint8_t pkt[64];
    struct udp_hdr *uh = (struct udp_hdr *)pkt;

    uh->sport = htons(10000);
    uh->dport = htons(20000);
    uh->len   = htons(20);
    uh->checksum = 0;

    // Assume this packet enters the UDP input path
    uint32_t ret = udp_input(pkt, 20);

    // Scheduler logic continues...
}
```

If we compile and attach the following program:

```c
const char *src =
            "struct udp_hdr {\n"
            "    unsigned short sport;\n"
            "    unsigned short dport;\n"
            "};\n"
            "\n"
            "int hook(void *ctx)\n"
            "{\n"
            "    unsigned int x;\n"
            "    unsigned int y;\n"
            "    struct udp_hdr *uh;\n"
            "\n"
            "    uh = (struct udp_hdr *)&ctx[0];\n"
            "    x = ntohs(uh->sport);\n"
            "    print(x);\n"
            "    y = ntohs(uh->dport);\n"
            "    print(y);\n"
            "    return x + y;\n"
            "}\n";
```

Then attaching it:

```c
if (hook_attach("hook_udp_input", image, size) != 0) {
    printf("hook_attach failed\n");
    heap_free(image);
    return 0;
}
```

The return value becomes **30000** (10000 + 20000).

You may also detach the program to restore normal behavior:

```c
hook_detach("hook_udp_input");
```
