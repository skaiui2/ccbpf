# **ccBPF Compiler Design Document **

## **1. Overview**

The compiler is organized into three major layers:

- **Frontend**: lexical analysis, parsing, AST construction, type checking, and symbol table management
- **Intermediate Representation (IR)**: a structured three‑address‑style instruction sequence
- **Backend**: lowering IR into BPF instructions (4.4BSD classic BPF), and finally packaging them into a `.ccbpf` executable image

Typical pipeline:

```
C source
  → Lexical analysis (lexer)
  → Parsing + AST (parser + inter)
  → IR generation (AST.gen / jumping)
  → IR lowering to BPF (ir_lower_program)
  → Packaging into .ccbpf (ccbpf_pack_memory)
```

## **2. Memory Management and Region Allocation**

The compiler uses a **region allocator** to manage object lifetimes:

- `frontend_region`: temporary objects used during lexing/parsing
- `longterm_region`: AST nodes, types, symbol tables, and other long‑lived objects
- `ir_region`: IR instruction list
- `string_region`: string literal pool
- `backend_region`: backend layout, jump patching structures, etc.

Initialization entry point:

```c
void compiler_init(uint8_t region_bit,
                   uint32_t cap,
                   uint32_t string_cap,
                   uint32_t ir_cap);
```

All AST / Type / IR nodes are allocated via `mg_region_alloc()` from the appropriate region.
 No fine‑grained `free` is performed; each region is destroyed as a whole when its phase ends.

## **3. Lexing, Parsing, and Symbol System**

### **3.1 Lexical Analysis (lexer)**

`struct lexer` is responsible for:

- Maintaining current line number and peek character
- A keyword table `hashmap words` (e.g., `if/else/struct/enum/return`)
- Token type enumeration:

```
enum tag
```

including:

- Arithmetic: `PLUS / MINUS / STAR / SLASH / MOD`
- Comparison: `LT / LE / GT / GE / EQ / NE`
- Logical: `AND / OR / NOT`
- Bitwise: `AND_BIT / OR_BIT`
- Identifiers & literals: `ID / NUM / STRING / TRUE / FALSE`
- Syntax symbols: `LPAREN / RPAREN / LBRACE / RBRACE / COMMA / SEMICOLON / LBRACKET / RBRACKET`
- Types/structures: `BASIC / STRUCT / ENUM`

Core interfaces:

```c
void lexer_init(struct lexer *lex);
struct lexer_token *lexer_scan(struct lexer *lex);
void lexer_set_input_buffer(const char *buf, size_t len);
```

### 3.2 Symbol and Type System (symbols)

Type enumeration:

```c
enum type_tag {
    TYPE_INT, TYPE_CHAR, TYPE_SHORT, TYPE_BOOL,
    TYPE_ARRAY, TYPE_FUNC, TYPE_PTR, TYPE_STRUCT, TYPE_ENUM
};
```

Core structures:

- `struct Type`: primitive type (tag + width)
- `struct Array`: array type (element type + size)
- `struct PtrType`: pointer type
- `struct StructType`: structure type with a `hashmap fields`, where each field is:

```c
struct StructFieldInfo {
    int offset;
    struct Type *type;
};
```

- `struct EnumType`: enumeration with a `hashmap values`

Symbol environment:

```c
struct Env {
    struct hashmap vars;   // variables / parameters / fields
    struct hashmap types;  // type names
    struct Env *prev;      // scope chain
    int level;
};
```

Provides `env_put_var / env_put_type / env_get_var / env_get_type` for resolving identifiers, struct fields, and type names.

### **3.3 Parsing (parser)**

`struct Parser`:

```c
struct Parser {
    struct lexer       *lex;
    struct lexer_token *look;
    struct Env         *top;
    int                 used;
};
```

Main entry points:

- `parser_program()` — parse the entire C source (currently supports a single `hook()` function)
- `parser_block()` — `{ ... }`
- `parser_decls()` — local variable / struct / enum declarations
- `parser_stmt()` — statements (if/return/assignment/empty/block)
- Expression hierarchy:
   `parser_bool() / join() / rel() / expr() / term() / unary() / factor()`
- `parser_offset()` — array/struct offset computation

Native function declarations:

```c
extern struct hashmap native_decl_table;
void native_decl_register(const char *name, int id, int argc);
```

The frontend only records `name → (id, argc)`; it does not handle implementation.

## **4. AST Design and Expressive Power**

### **4.1 Node Base Class and Expr/Stmt Hierarchy**

All AST nodes derive from `Node`:

```c
struct Node {
    int  lexline;
    enum NodeTag tag;
    void (*gen)(struct Node *self, int b, int a);
    void (*jumping)(struct Node *self, int t, int f);
    char *(*tostring)(struct Node *self);
};
```

- `gen(self, b, a)`: generate IR (`b/a` are control‑flow labels, mainly for statements)
- `jumping(self, t, f)`: encode boolean expressions as conditional jumps
- `tostring(self)`: debugging representation

Expression base:

```c
struct Expr {
    struct Node        base;
    struct lexer_token *op;
    struct Type        *type;
    int                 temp_no;  // IR temporary number
};
```

Statement base:

```c
struct Stmt {
    struct Node base;
    int         after;   // reserved for control flow
};
```

### **4.2 Supported Expression Nodes**

- **Constant** (`Constant`)
  - `int int_val`
  - `Constant_true` / `Constant_false` are singletons
- **Identifier** (`Id`)
  - `int offset` — logical stack‑frame offset (bytes)
  - `int is_ctx_ptr` — whether this is the special `ctx` pointer
- **Array access** (`Access`)
  - `Expr *array; Expr *index;`
  - `int slot; int width;`
- **ctx access**
  - `CtxExpr`: `ctx[offset]` → generates `IR_LOAD_CTX`
  - `CtxPtrExpr`: `&ctx[CONST]` (restricted unary `&`)
- **Arithmetic** (`Arith`) — `+ - * /`
- **Unary** (`Unary`) — `-` / `!`
- **Logical** (`Logical / And / Or / Not`)
- **Relational** (`Rel`) — `< <= > >= == !=`
- **String literal** (`StringLiteral`)
  - `int str_id` — index in string pool
- **Builtin/native call** (`BuiltinCall`)
  - `name`, `native_id`, `argc`, `args[4]`

### **4.3 Supported Statement Nodes**

- **Assignment** (`Set`)
  - `id = expr` → `IR_STORE`
- **Array element assignment** (`SetElem`)
  - `arr[i] = expr` (index must be constant)
- **if / if‑else** (`If / Else`)
  - implemented using `jumping()` + explicit labels + `IR_GOTO`
- **Sequential composition** (`Seq`)
- **return** (`Return`)
- **Empty statement** (`Stmt_Null`)

## **5. IR Design and Semantics**

### **5.1 IR Instruction Set**

```c
enum IR_Op {
    IR_NOP,
    IR_MOVE,          // dst = imm / value
    IR_ADD, IR_SUB, IR_MUL, IR_DIV,
    IR_AND, IR_OR,
    IR_NEG, IR_NOT,

    IR_LOAD_CTX,      // dst = ctx[offset, width]
    IR_LOAD,          // dst = MEM[base + index]
    IR_STORE,         // MEM[base + index] = src

    IR_RET,           // return src1
    IR_IF_FALSE,      // if !(relop(src1, src2)) goto label
    IR_GOTO,          // goto label
    IR_LABEL,         // label definition

    IR_NATIVE_CALL,   // dst = native(native_id, args...)
};
```

Relational operators:

```c
enum IR_RelOp { IR_GT, IR_GE, IR_EQ, IR_NE };
```

IR structure:

```c
struct IR {
    enum IR_Op   op;

    int dst, src1, src2;      // temp registers

    int array_base;           // logical base (0/4/8/12/...)
    int array_index;          // logical offset (usually 0)
    int array_width;          // element width (bytes)

    enum IR_RelOp relop;      // comparison type
    int label;                // target label

    int native_id;            // native function ID
    int arg_width;            // argument width
    int argc;                 // number of arguments
    int args[4];              // temp numbers of arguments

    struct IR *next;          // linked list
};
```

IR emission:

```c
void ir_emit(struct IR ir);
void ir_mes_get(struct ir_mes *im); // { ir_head, label_count }
```

# **6.1 Temporary Variable Allocation**

Global counter:

```c
static int temp_count = 1;
int new_temp(void) { return temp_count++; }
```

Each `Expr` has a `temp_no`, assigned on the first call to `expr_gen()`.
 All IR fields `dst/src1/src2/args[i]` refer to this temporary number.

# **6.2 Expression Generation (expr_gen)**

Core logic (dispatched by `tag`):

### **Constant (`TAG_CONSTANT`)**

```c
IR_MOVE: dst = int_val;
```

### **Identifier (`TAG_ID`)**

```c
IR_LOAD:
    dst         = temp_no;
    array_base  = id->offset;
    array_index = 0;
    array_width = type->width;
```

### **Array Access (`TAG_ACCESS`)**

(Only constant indices are supported)

```c
elem_offset = slot + idx * width;
IR_LOAD:
    dst         = temp_no;
    array_base  = elem_offset;
    array_index = 0;
    array_width = width;
```

### **ctx Access (`TAG_CTX`)**

```c
IR_LOAD_CTX:
    dst  = temp_no;
    src1 = offset;
    src2 = type->width;
```

### **String Literal (`TAG_STRING`)**

```c
str_id = intern_string(unescaped);
IR_MOVE: dst = str_id;
```

### **Arithmetic (`TAG_ARITH`)**

```c
Generate e1 and e2 first
IR_ADD/SUB/MUL/DIV:
    dst  = temp_no;
    src1 = e1->temp_no;
    src2 = e2->temp_no;
```

### **Unary (`TAG_UNARY`)**

```c
Generate expr first
MINUS → IR_NEG: dst = -expr
NOT   → IR_NOT: dst = !expr
```

### **BuiltinCall / Native Call (`TAG_BUILTIN_CALL`)**

```c
Generate each args[i] first
IR_NATIVE_CALL:
    dst       = temp_no;
    native_id = b->native_id;
    argc      = b->argc;
    arg_width = type->width;
    args[i]   = args[i]->temp_no;
```

# **6.3 Logical Expressions and Conditional Jumping**

Boolean expressions do **not** produce a boolean temporary.
 Instead, they are encoded as control flow via `jumping(t, f)`:

### **And**

```c
e1.jumping(0, Lfalse);
e2.jumping(t, f);
```

### **Or**

```c
e1.jumping(Ltrue, 0);
e2.jumping(t, f);
```

### **Not**

```c
e1.jumping(f, t);
```

### **Relational (`Rel`)**

- Generate temps for `e1` and `e2`
- If `f != 0`, emit an `IR_IF_FALSE` using `relop + src1/src2 + label=f`
- Also build a string `"e1 op e2"` for debugging output via `node_emit_jumps()`

# **6.4 Statement Generation**

### **return**

```c
expr_gen(expr);
IR_RET: src1 = expr->temp_no;
```

### **Assignment (`Set`)**

```c
expr_gen(expr);
IR_STORE:
    array_base  = id->offset;
    array_index = 0;
    array_width = id->type->width;
    src1        = expr->temp_no;
```

### **Array Element Assignment (`SetElem`)**

(Only constant indices supported)

```c
idx = const_index;
elem_offset = slot + idx * width;
expr_gen(expr);
IR_STORE:
    array_base  = elem_offset;
    array_index = 0;
    array_width = width;
    src1        = expr->temp_no;
```

### **Sequential Composition (`Seq`)**

```c
if s1 == Null → generate s2 only
if s2 == Null → generate s1 only
else:
    L = newlabel();
    s1.gen(b, L);
    emit_label(L);
    s2.gen(L, a);
```

### **if**

```c
Lthen = newlabel();
Lelse = newlabel();
Lend  = newlabel();

expr.jumping(0, Lelse);

emit_label(Lthen);
stmt.gen(Lthen, a);

IR_GOTO Lend;

emit_label(Lelse);
emit_label(Lend);
```

### **if-else**

```c
Lthen = newlabel();
Lelse = newlabel();
Lend  = newlabel();

expr.jumping(0, Lelse);

emit_label(Lthen);
stmt1.gen(Lthen, a);
IR_GOTO Lend;

emit_label(Lelse);
stmt2.gen(Lelse, a);

emit_label(Lend);
```

# **7. Backend: IR → BPF Mapping**

## **7.1 Memory Layout (backend_layout)**

```c
struct backend_layout {
    int temp_base;      // starting slot for temporaries (default 64)
    int temp_count;     // number of temporaries (default 64)

    int mem_a;          // 0  → mem_a (8)
    int mem_b;          // 4  → mem_b (16)
    int mem_c;          // 8  → mem_c (24)

    int mem_arr_base[4];// 12/16/20/24 → 32/40/48/56
};
```

Mapping functions:

```c
int temp_slot(const backend_layout *l, int t)
    => l->temp_base + t * 4;

int map_array_base(const backend_layout *l, int base)
    // 0/4/8/12/16/20/24 → mem_a/mem_b/mem_c/mem_arr_base[i]
```

All `offset/slot` values in the frontend/IR are **logical** (0,4,8,...).
 The backend maps them into actual BPF `mem[]` slots.

## **7.2 Instruction Selection (selection)**

### **IR_MOVE**

```c
LD  #imm
ST  MEM[temp_slot(dst)]
```

### **IR_ADD/SUB/MUL/DIV**

```c
LD   MEM[temp_slot(src1)]
LDX  MEM[temp_slot(src2)]
ALU{ADD/SUB/MUL/DIV} X
ST   MEM[temp_slot(dst)]
```

### **IR_LOAD** (locals/arrays)

```c
base_slot = map_array_base(array_base) + array_index;
LD  MEM[base_slot]
ST  MEM[temp_slot(dst)]
```

### **IR_STORE**

```c
src_slot  = temp_slot(src1);
base_slot = map_array_base(array_base) + array_index;

LD  MEM[src_slot]
ST  MEM[base_slot]
```

### **IR_LOAD_CTX**

```c
// ctx is accessed via ABS mode in the BPF VM
switch (width):
  1 → LD B ABS offset
  2 → LD H ABS offset
  4 → LD W ABS offset

ST MEM[temp_slot(dst)]
```

### **IR_RET**

```c
LD  MEM[temp_slot(src1)]
RET A
```

### **IR_NATIVE_CALL** (critical ABI)

- Load arguments from temp slots into the VM’s “argument slots” (`BPF_ST` to fixed offsets)
- Emit:

```c
BPF_STMT(BPF_MISC | BPF_COP, native_id);
```

- Store return value from A:

```c
ST MEM[temp_slot(dst)]
```

### **Control Flow** (`IR_IF_FALSE / IR_GOTO / IR_LABEL`)

- Lowered by `lower_if_false / lower_goto / lower_label`
- `patch_jumps()` fills in final jump targets using `label_pc[]`

# **8. Compiler Summary**

- **AST explicitly models control flow and boolean logic**  
   Using the dual `gen/jumping` interface, expression values and control flow are separated.
   Short‑circuit logic and relational operations are expanded at the AST level.
- **IR is simple, stable, and close to three‑address code**  
   All operations revolve around `dst/src1/src2` and logical stack‑frame offsets, making backend mapping straightforward.
- **Backend layout is fixed and predictable**  
   `backend_layout` maps temporaries, locals, arrays, and ctx into BPF `mem[]`, enabling a consistent VM execution model.
- **Native call ABI is explicit**  
   The compiler only cares about `native_id + argc + args[temp_no]`.
   Argument slot layout and invocation logic are handled entirely by the VM, keeping frontend and backend decoupled.

# **ccBPF Virtual Machine Design Document**

## **1. Overview of the Virtual Machine**

The ccBPF virtual machine consists of three major components:

1. **cbpf Executor (`ccbpf_vm_exec`)**
   - A classic 4.4BSD‑lite style BPF interpreter
   - Supports A/X registers, scratch `mem[]`, ABS/IND loads, ALU operations, jumps, RET, etc.
2. **ccbpf Program Loader (`ccbpf_load_from_memory`)**
   - Parses the `.ccbpf` executable image
   - Loads BPF instructions, string pool, map array, entry point, and other metadata
3. **Hook System (`hook_register / hook_attach / hook_run`)**
   - Allows dynamic registration of hook points
   - Multiple programs can be attached to a hook
   - When a hook is triggered, all attached programs run sequentially

# **2. cbpf Virtual Machine Execution Model**

Entry point:

```c
unsigned int ccbpf_vm_exec(struct ccbpf_program *prog,
                           struct bpf_insn *pc,
                           unsigned char *p,
                           unsigned int wirelen,
                           unsigned int buflen);
```

### **2.1 Register Model**

The VM has only two registers:

- `A`: accumulator (primary register)
- `X`: index register

Initialization:

```c
uint32_t A = 0, X = 0;
```

### **2.2 Scratch Memory (`mem[]`)**

```c
#define CCBPF_STACK_SIZE (10*1024)
uint8_t mem[CCBPF_STACK_SIZE];
```

The scratch area is used for:

- Temporaries (`temp_slot`)
- Local variables (`map_array_base`)
- Native call argument slots (`mem[0]`, `mem[4]`)
- All logical stack‑frame accesses generated by IR_LOAD / IR_STORE

Before each execution, the VM clears the scratch memory:

```c
memset(mem, 0, sizeof(mem));
```

# **3. Instruction Semantics**

The VM supports the full classic BPF instruction set.

### **3.1 Load Instructions (LD / LDX)**

- **ABS mode**: read from the frame (`ctx`)

```c
BPF_LD | BPF_W | BPF_ABS   → A = *(uint32_t*)(p + k)
BPF_LD | BPF_H | BPF_ABS   → A = *(uint16_t*)(p + k)
BPF_LD | BPF_B | BPF_ABS   → A = p[k]
```

- **IND mode**: read from `p[X + k]`
- **IMM mode**:

```c
BPF_LD  | BPF_IMM → A = k
BPF_LDX | BPF_IMM → X = k
```

- **MEM mode**:

```c
BPF_LD  | BPF_MEM → A = mem[k]
BPF_LDX | BPF_MEM → X = mem[k]
```

### **3.2 Store Instructions (ST / STX)**

```c
BPF_ST  → mem[k] = A
BPF_STX → mem[k] = X
```

### **3.3 ALU Instructions**

Supported operations:

- ADD / SUB / MUL / DIV
- AND / OR
- LSH / RSH
- NEG

Both immediate and X‑register variants are supported.

### **3.4 Jump Instructions (JMP)**

Supported:

- Unconditional jump: `JA`
- Conditional jumps: `JGT / JGE / JEQ / JSET` (both K and X variants)
- `jt/jf` branch offsets

### **3.5 Return Instructions (RET)**

```c
BPF_RET | BPF_K → return k
BPF_RET | BPF_A → return A
```

# **4. Native Call Mechanism (BPF_COP)**

This is the key extension of ccBPF.

### **4.1 IR_NATIVE_CALL → BPF_COP**

The backend emits:

```c
BPF_MISC | BPF_COP, k = native_id
```

During execution:

```c
case BPF_MISC | BPF_COP: {
    int func_id = pc->k;
    struct native_entry *e = hashmap_get(&native_table, func_id);
```

### **4.2 Argument ABI**

The VM uses the following calling convention:

- `A` → a0
- `X` → a1
- `mem[0]` → a2
- `mem[4]` → a3

Code:

```c
uint32_t a0 = A;
uint32_t a1 = X;
uint32_t a2 = mem[0];
uint32_t a3 = mem[4];
```

### **4.3 Native Function Signature**

```c
typedef uint32_t (*native_fn_t)(struct ccbpf_program *prog,
                                uint32_t a0, uint32_t a1,
                                uint32_t a2, uint32_t a3);
```

### **4.4 Registering a Native Function**

```c
void native_register(int func_id, int argc, native_fn_t fn)
```

Native functions are stored in `native_table` (a hashmap).

# **5. Hook System Design**

The hook system allows dynamic registration of hook points and attaching multiple programs.

### **5.1 hook_entry Structure**

```c
struct hook_entry {
    const char *name;
    struct hook_node *head;
};

struct hook_node {
    struct ccbpf_program *prog;
    struct hook_node *next;
};
```

### **5.2 Registering a Hook**

```c
void hook_register(const char *name)
```

- Allocates a `hook_entry`
- Inserts it into `hook_table` (hashmap)

### **5.3 Attaching a Program**

```c
int hook_attach(const char *hook_name, uint8_t *image, size_t len)
```

Process:

1. Find the hook entry
2. Load the program via `ccbpf_load_from_memory(image)`
3. Insert into the head of the linked list
4. Program becomes active immediately

### **5.4 Detaching Programs**

```c
int hook_detach(const char *hook_name)
```

- Unloads all programs (each via `ccbpf_unload`)
- Clears the linked list

### **5.5 Running a Hook**

```c
uint32_t hook_run(const char *hook_name, uint8_t *frame, size_t frame_size)
```

- Executes all attached programs in order
- Returns the return value of the **last** program

# **6. `.ccbpf` File Format**

From `ccbpf_load_from_memory()`:

```c
struct CCBPF_Header {
    uint32_t magic;
    uint32_t code_offset;
    uint32_t code_size;
    uint32_t data_offset;
    uint32_t data_size;
    uint32_t entry;
};
```

Loading process:

1. Validate `magic`
2. Copy `.text` (BPF instructions)
3. Parse `.data` (string pool)
4. Initialize `maps[]` (8 hashmaps)
5. Set entry point `entry`

# **7. Program Execution Flow**

Full execution pipeline:

```
.ccbpf image
    ↓ ccbpf_load_from_memory
ccbpf_program { insns, strings, maps, entry }
    ↓ hook_attach
hook_entry list
    ↓ hook_run
ccbpf_run_frame
    ↓
ccbpf_vm_exec
    ↓
BPF instruction interpretation
    ↓
native call (BPF_COP)
    ↓
return uint32_t
```

# **8. Virtual Machine Summary**

The ccBPF VM has the following characteristics:

- **Fully compatible with classic BPF** (4.4BSD‑lite)
- **Scratch mem[] serves as a unified “virtual stack frame”**
- **ctx access implemented via ABS mode**
- **Native calls implemented via BPF_COP**
- **Hook system supports dynamic attach/detach**
- **Program loader supports string pools and map arrays**

The VM is extremely small, portable, and suitable for MCU/RTOS environments, while also running cleanly in Linux userspace.
