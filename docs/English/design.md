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

A region handle points to `mg_region`. Pool regions group allocations by block size; bump regions advance an offset within one buffer:
```c
#define MG_MAX_CLASS_BITS 12

enum mg_region_kind {
    MG_REGION_POOL = 0,
    MG_REGION_BUMP = 1,
};
```

```c
struct mg_pool_node {
    membit_pool_t pool;
    uint16_t      block_count;
    uint16_t      block_size;
    struct mg_pool_node *next;
};

struct mg_class {
    struct mg_pool_node *pools;
    size_t               block_size;
};

struct mg_block_header {
    membit_pool_t pool;
};

struct mg_bump_state {
    uint8_t *buf;
    size_t   cap;
    size_t   off;
};

struct mg_region {
    enum mg_region_kind kind;
    uint8_t             max_bits;

    union {
        struct {
            struct mg_class classes[MG_MAX_CLASS_BITS + 1];
        } pool;

        struct mg_bump_state bump;
    } u;
};
```

Bump allocation rounds the size up to four bytes, checks `off + size <= cap`, returns `buf + off`, and advances `off`. Pool allocation selects a size class and searches its pools, adding a pool when necessary. Reset reuses storage; destruction releases the region and its backing allocations.

| Region | Stored objects | Lifetime |
| --- | --- | --- |
| `frontend_region` | Pool-allocated frontend temporaries | Reset after generating each outer-block statement |
| `longterm_region` | Tokens, names, types, symbols and most syntax nodes | `frontend_destroy()` |
| `ir_region` | IR list | `ir_free()` after lowering |
| `string_region` | String literals | `bpf_builder_free()` after packing |
| `backend_region` | Instructions, label table and pending jumps | `bpf_builder_free()` |
| `pack_region` | Serialized image | `bpf_builder_free()` |

Resetting frontend temporaries leaves IR and long-lived nodes intact. Strings remain available through packing. Write, copy or load the image before releasing the builder. Peak memory depends on overlapping lifetimes and growth buffers.

## **3. Lexing, Parsing, and Symbol System**

### **3.1 Lexical Analysis (lexer)**

The lexer holds the current character and keyword map. A token’s tag selects whether its payload is a name, an integer value or punctuation:
```c
struct lexer {
    int line;
    char peek;
    char *filename;
    struct hashmap words;
};

struct lexer_token {
    int tag;
    int line;
    int int_val;
    float real_val;
    char *lexeme;
    char ch;
};
```

`struct lexer` is responsible for:

- Maintaining current line number and peek character
- A keyword table `hashmap words` (e.g., `if/else/struct/return`)
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

Scanning skips whitespace, then reads an identifier, integer, string or operator. Identifiers are checked against the keyword map and become `ID` when no keyword matches. The `real_val` field exists, but floating-point literal scanning is not implemented. Registered keywords include `if/else/struct/return` and basic types; the keyword table does not register `enum` and the parser has no enum-declaration branch, so source enum declarations are not accepted. Scanning is linear in input length.

### 3.2 Symbol and Type System (symbols)

```c
enum type_tag {
    TYPE_INT,
    TYPE_CHAR,
    TYPE_SHORT,
    TYPE_BOOL,
    TYPE_ARRAY,
    TYPE_FUNC,
    TYPE_PTR,
    TYPE_STRUCT,
    TYPE_ENUM
};
```

Type width determines local offsets, array storage and context access width. Arrays store their element type and length; pointers store their target type; function types store result and parameter types. Struct and enum types contain field and value maps. These declarations are in `symbols.h`:
```c
struct Type {
    enum type_tag tag;
    int width;
};

struct Array {
    struct Type base;
    struct Type *of;
    int size;
};

struct PtrType {
    struct Type base;
    struct Type *to;
};

struct FuncType {
    struct Type base;
    struct Type *ret;
    struct Type **params;
    int param_count;
};

struct StructType {
    struct Type base;
    struct hashmap fields;
};

struct StructFieldInfo {
    int offset;
    struct Type *type;
};

struct EnumType {
    struct Type base;
    struct hashmap values;
};
```

Scopes are linked through `Env.prev`:
```c
struct Env {
    struct hashmap vars;
    struct hashmap types;
    struct Env *prev;
    int level;
};
```

Name lookup uses a chained hash map; collisions are stored in doubly linked bucket lists:
```c
struct list_node {
    struct list_node *next;
    struct list_node *prev;
};
```

```c
struct hashmap_entry {
    struct list_node node;
    void *key;
    void *value;
};

struct hashmap {
    struct list_node *buckets;
    uword_t bucket_count;
    int key_type;
};
```

Declarations enter the current scope through `env_put_var()` or `env_put_type()`. Lookup checks the current map, then follows `prev` outward until it finds a match or reaches the end. Inner declarations can therefore shadow outer declarations. Struct fields are looked up separately as `StructFieldInfo`, whose offset is added to the context base. Lookup cost depends on scope depth and bucket-chain lengths.

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
- `parser_decls()` — local variable declarations within a block
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

A Native declaration records its name, ID and argument count:
```c
struct NativeDecl {
    const char *name;
    int         id;
    int         argc;
};
```

The parser uses one lookahead token, recurses through expression precedence levels, and uses loops for consecutive operators at one level. Entering a block creates an `Env`; leaving restores its parent. `parser_program()` accepts top-level struct declarations followed by `int hook(void *ctx)`. `parser_block_gen()` parses and generates each outer-block statement immediately, so a complete function AST need not be retained first.

`Parser.used` advances by each declared type’s width and becomes `Id.offset`. Expression constructors perform type checks; `arith_new()` selects arithmetic result types, while `set_new()` does not check assignment type compatibility.

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

`Op` supplies a common expression base for arithmetic, unary and access nodes:
```c
struct Op {
    struct Expr base;
};
```

Each base occupies the first field of its containing structure. Generation dispatches by `Node.tag`, casts back to the concrete node and visits its operands.

### **4.2 Supported Expression Nodes**

Expression structures separate storage from computation. Variables carry offsets, constants carry values, operations carry operands, and calls carry a Native ID and arguments:
```c
struct Constant {
    struct Expr base;
    int int_val;
};

struct Id {
    struct Expr base;
    int         offset;
    int         base_offset;
    struct StructType *st;
    int is_ctx_ptr;
};

struct Access {
    struct Op  base;
    struct Expr *array;
    struct Expr *index;
    int slot;
    int width;
};

struct CtxExpr {
    struct Expr base;
    int offset;
};

struct CtxPtrExpr {
    struct Expr base;
    int base_offset;
    struct StructType *st;
};

struct Arith {
    struct Op  base;
    struct Expr *e1;
    struct Expr *e2;
};

struct Unary {
    struct Op  base;
    struct Expr *expr;
};

struct Logical {
    struct Expr base;
    struct Expr *e1;
    struct Expr *e2;
    int temp_no;
};

struct And {
    struct Logical base;
};

struct Or {
    struct Logical base;
};

struct Not {
    struct Logical base;
};

enum AST_RelOp {
    AST_LT,
    AST_LE,
    AST_GT,
    AST_GE,
    AST_EQ,
    AST_NE,
};

struct Rel {
    struct Logical base;
    enum AST_RelOp relop;
};

struct BitAnd {
    struct Op base;
    struct Expr *e1;
    struct Expr *e2;
};

struct BitOr {
    struct Op base;
    struct Expr *e1;
    struct Expr *e2;
};

struct StringLiteral {
    struct Expr base;
    int str_id;
};

struct BuiltinCall {
    struct Expr base;
    const char *name;
    int         native_id;
    int         argc;
    struct Expr *args[4];
};
```

`Constant_true` and `Constant_false` are boolean singletons. `Id.offset` is a local byte offset; `is_ctx_ptr`, `st` and `base_offset` identify ctx-derived struct access. `Access.slot` and `width` determine constant-index byte offsets, while `CtxExpr.offset` selects a host-context location.

`Arith` operators `+`, `-`, `*`, `/` and `%` pass through `expr_gen()` to `IR_ADD/SUB/MUL/DIV/MOD`, then through `lower_binop()` to VM arithmetic instructions. The other paths are:

| Operation | Frontend path | Instruction generation |
| --- | --- | --- |
| Logical negation `!` in a condition | `parser_unary()` → `not_new()` | `not_jumping()` swaps the true and false labels and invokes the operand's `jumping` callback |
| Bitwise `&` and `|` | `bitand_new()` and `bitor_new()` store two operands but do not set the node `tag` | `expr_gen()` has no bitwise dispatch and lowering has no `IR_AND/IR_OR` dispatch; these paths cannot compile bitwise programs |
| Unary minus `-x` | `parser_unary()` → `unary_new()` → `expr_gen()` → `IR_NEG` | `ir_lower_program()` reaches its default branch and calls `abort()`, terminating compilation |

`Logical` stores two operands. `And/Or/Not/Rel` use `jumping` callbacks for conditional control flow. Conditional branching and computing a logical expression as a stored value are separate paths.

`StringLiteral.str_id` indexes the string table. `BuiltinCall.args[4]` stores argument expressions, evaluated before emitting a host call.

### **4.3 Supported Statement Nodes**

Statement structures store execution relationships: assignments hold a destination and expression, branches hold conditions and bodies, and sequence nodes hold two statements:
```c
struct Set {
    struct Stmt base;
    struct Id   *id;
    struct Expr *expr;
};

struct SetElem {
    struct Stmt base;
    struct Id   *array;
    struct Expr *index;
    struct Expr *expr;
    int slot;
    int width;
};

struct If {
    struct Stmt base;
    struct Expr *expr;
    struct Stmt *stmt;
};

struct Else {
    struct Stmt base;
    struct Expr *expr;
    struct Stmt *stmt1;
    struct Stmt *stmt2;
};

struct Seq {
    struct Stmt base;
    struct Stmt *s1;
    struct Stmt *s2;
};

struct Return {
    struct Stmt base;
    struct Expr *expr;
};
```

`Set` stores its evaluated expression into a local. `SetElem` first computes a constant-index byte offset. `If/Else` emit labels and conditional jumps; `Seq` generates its children in order; `Return` emits `IR_RET` after evaluating its result. `Stmt_Null` represents an empty statement and emits no operation instructions.

## **5. IR Design and Semantics**

### **5.1 IR Instruction Set**

```c
enum IR_Op {
    IR_NOP = 0,

    IR_MOVE,

    IR_ADD,
    IR_SUB,
    IR_MUL,
    IR_DIV,
    IR_MOD,

    IR_AND,
    IR_OR,

    IR_RET,
    IR_LOAD_CTX,

    IR_LOAD,

    IR_RELOP,

    IR_NEG,
    IR_NOT,

    IR_STORE,

    IR_IF_FALSE,

    IR_GOTO,

    IR_LABEL,
    IR_NATIVE_CALL,
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

    int array_base;           // local or array-element byte offset
    int array_index;          // logical index; emitted as 0 by this frontend
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

```c
struct ir_mes {
    int label_count;
    struct IR *ir_head;
};
```

`ir_emit()` allocates a record in `ir_region` and appends it in O(1) using a tail pointer. `ir_mes_get()` scans the list to return its head and required label-table size. Arithmetic links temporary values through `dst/src1/src2`; memory operations carry an already-computed byte offset in `array_base`.

This is a linear three-address-style IR, with no SSA construction or general optimizer. `ir_lower_program()` accepts `IR_MOVE`, `IR_ADD/SUB/MUL/DIV/MOD`, `IR_RET`, `IR_LOAD_CTX`, `IR_NATIVE_CALL`, `IR_LOAD/STORE`, `IR_IF_FALSE`, `IR_GOTO` and `IR_LABEL`. Other opcodes enter the default branch, print `ir_lowering abort` and call `abort()`.

# **6.1 Temporary Variable Allocation**

Global counter:

```c
static int temp_count = 1;
int new_temp(void) { return temp_count++; }
```

An `Expr` receives `temp_no` on first generation. A parent allocates its number before visiting operands, so numbering differs from evaluation order. Destination and argument fields hold temporary numbers, but source-field meanings depend on the opcode: `IR_MOVE.src1` is an immediate, and `IR_LOAD_CTX.src1/src2` hold an offset and access width.

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
IR_ADD/SUB/MUL/DIV/MOD:
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

The table lists the mappings in the `TAG_UNARY` branch. `ir_lower_program()` calls `abort()` for both `IR_NEG` and `IR_NOT`. For source `!`, `parser_unary()` creates a `Not`; its conditional path calls `not_jumping()` to swap labels rather than evaluating a `Unary`.

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

Generation recursively follows the syntax tree. Assignments evaluate before storing, returns evaluate before returning, and short-circuit conditions visit only the required branch. Current if/else lowering still emits an end jump after a returning arm. If both arms return with no following instruction, that jump can target the end of code and fail validation. Assign within the arms and use one final return for that case.

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

Temporary numbers become byte offsets through `temp_slot()`. The active `IR_LOAD/IR_STORE` cases in `ir_lower_program()` use `array_base` directly, as the frontend has already computed a byte offset. They do not call `map_array_base()`, which is used by separate helper functions in `selection.c`. Local and temporary ranges must not overlap, and every four-byte access must fit in `ccbpf_ctx.mem`. The layout’s `temp_count` field does not itself enforce a temporary limit.

## **7.2 Instruction Selection (selection)**

Instruction construction uses these builder and instruction structures:
```c
struct bpf_builder {
    struct bpf_insn *insns;
    int count;
    int capacity;
};
```

```c
struct bpf_insn {
	unsigned short	code;
	unsigned char 	jt;
	unsigned char 	jf;
	long	k;
};
```

`bpf_builder_emit()` appends an instruction and returns its index. Capacity starts at 128 and doubles when full, giving amortized O(1) append. Old arrays remain in the bump region until destruction, so memory budgeting includes growth copies. `code` selects the operation, `jt/jf` hold conditional offsets, and `k` holds an immediate, memory offset or Native ID. The size of C `long` depends on the ABI.

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
// Remainder uses ALU{MOD} X
ST   MEM[temp_slot(dst)]
```

### **IR_LOAD** (locals/arrays)

```c
base_slot = array_base;
LD  MEM[base_slot]
ST  MEM[temp_slot(dst)]
```

### **IR_STORE**

```c
src_slot  = temp_slot(src1);
base_slot = array_base;

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

- Evaluate arguments; load the first two into A/X and store the next two at `mem[0..3]` and `mem[4..7]`. Reload argument 0 into A after staging the later arguments.
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

Jump repair stores unresolved targets in `pending`:
```c
struct pending {
    int insn;
    int label;
    int is_cond;
    int true_branch;
};
```

A label records `label_pc[label] = current instruction count`. A jump first emits a placeholder and records its index, target label and branch to patch. Once emission finishes, `target_pc - (jump_pc + 1)` is written to `jt`, `jf` or `k`. A jump from index 5 to index 9 therefore has offset 3.

Selection and repair are linear in the IR and pending-jump counts. `patch_jumps()` casts a conditional offset to `unsigned char` and writes it to `jt/jf` without a range check. Nonnegative offsets above 255 retain only their low eight bits: 256 becomes 0. Unconditional offsets are written to `k`.

# **8. Compiler Summary**

- **AST explicitly models control flow and boolean logic**  
   Using the dual `gen/jumping` interface, expression values and control flow are separated.
   Short‑circuit logic and relational operations are expanded at the AST level.
- **IR is simple, stable, and close to three‑address code**  
   All operations revolve around `dst/src1/src2` and logical stack‑frame offsets, making backend mapping straightforward.
- **Backend layout is fixed and predictable**  
   Temporaries use `temp_slot()`; locals and arrays use frontend-computed byte offsets. Context reads use ABS access to the host buffer.
- **Native call ABI is explicit**  
   The compiler only cares about `native_id + argc + args[temp_no]`.
   The backend emits argument staging instructions, and the VM invokes the host function using the same ABI.

# **ccBPF Virtual Machine Design Document**

## **1. Overview of the Virtual Machine**

The VM has three main components:

1. **BPF interpreter (`ccbpf_vm_step`)**: fetches at `ccbpf_ctx.pc`, updates A/X and scratch, and returns execution status.
2. **Program loader (`ccbpf_load_from_memory`)**: parses the image, copies instructions and strings, and creates a program object.
3. **Hook system (`hook_register / hook_attach / hook_run`)**: resolves host event names and runs attached programs.

A loaded program holds its code, strings and host-context pointer:
```c
struct ccbpf_program {
    struct bpf_insn *insns; //.text
    size_t insn_count;

    uint8_t *data;     //.data or .radata
    size_t data_size;

    uint32_t entry;

    int string_count;
    char **strings;
    void *ctx;
};
```

`ccbpf_program` is reusable code, while `ccbpf_ctx` holds one execution. Separating them permits pause, resume or restart. The root program object has no `maps[]`; the host supplies Maps through Native helpers. The SCP demo uses a separate per-program layout. `ccbpf_unload()` releases loader-owned instructions and strings; host resources remain host-owned.

# **2. cbpf Virtual Machine Execution Model**

The step interface accepts persistent execution state, the program, host data and an instruction budget:
```c
enum ccbpf_status ccbpf_vm_step(struct ccbpf_ctx *ctx,
                                struct ccbpf_program *prog,
                                unsigned char *p,
                                unsigned int wirelen,
                                unsigned int buflen,
                                int max_insn);
```

```c
enum ccbpf_status {
    CCBPF_OK,
    CCBPF_FINISHED,
    CCBPF_MIGRATE,
    CCBPF_ERROR
};
```

Each iteration fetches `prog->insns[pc]`, dispatches by `code`, updates registers or memory, and advances pc. Status distinguishes exhausted step budget (`CCBPF_OK`), return, migration yield and error. Resume by reusing the same `ccbpf_ctx`.

`ccbpf_run_frame()` creates zeroed state and starts at instruction 0, stepping 64 instructions at a time until return or error. It continues locally after migration status. The chunk size is not a total execution limit; a scheduler needing yields or migration should drive the step interface.

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
#define CCBPF_STACK_SIZE (512)

struct ccbpf_ctx {
    uint32_t A;
    uint32_t X;
    uint32_t pc;
    uint32_t ret;
    uint8_t mem[CCBPF_STACK_SIZE];
};
```

`ccbpf_ctx` holds the VM state for one execution: A and X are working registers, pc is the instruction index, and ret holds the return value. Its mem array holds local variables and temporary results at byte offsets. `CCBPF_STACK_SIZE` in `ccBPF/cbpf.h` defines its capacity; changes to that capacity must agree with compiler layout and VM access checks.

The scratch area is used for:

- Temporaries (`temp_slot`)
- Local variables (frontend-computed byte offsets)
- Native call argument slots (`mem[0]`, `mem[4]`)
- All logical stack‑frame accesses generated by IR_LOAD / IR_STORE

Starting a new execution clears the context; resuming a step preserves existing state:

```c
struct ccbpf_ctx ctx;
memset(&ctx, 0, sizeof(ctx));
```

# **3. Instruction Semantics**

The VM follows classic BPF register and instruction categories with ccBPF extensions. Memory offsets are bytes; registers and ordinary stored values are four bytes.

### **3.1 Load Instructions (LD / LDX)**

- **ABS mode**: read from the frame (`ctx`)

```c
BPF_LD | BPF_W | BPF_ABS   → A = EXTRACT_LONG(&p[k])
BPF_LD | BPF_H | BPF_ABS   → A = EXTRACT_SHORT(&p[k])
BPF_LD | BPF_B | BPF_ABS   → A = p[k]
```

`EXTRACT_LONG/EXTRACT_SHORT` use `memcpy` to read raw four-byte or two-byte values without network byte-order conversion. The host context buffer p is separate from `ctx->mem`, which stores local values.

- **IND mode**: read from `p[X + k]`
- **IMM mode**:

```c
BPF_LD  | BPF_IMM → A = k
BPF_LDX | BPF_IMM → X = k
```

- **MEM mode**:

```c
BPF_LD  | BPF_MEM → memcpy(&A, &ctx->mem[k], sizeof(uint32_t))
BPF_LDX | BPF_MEM → memcpy(&X, &ctx->mem[k], sizeof(uint32_t))
```

### **3.2 Store Instructions (ST / STX)**

```c
BPF_ST  → memcpy(&ctx->mem[k], &A, sizeof(uint32_t))
BPF_STX → memcpy(&ctx->mem[k], &X, sizeof(uint32_t))
```

### **3.3 ALU Instructions**

Supported operations:

- ADD / SUB / MUL / DIV / MOD
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

A/X use unsigned arithmetic. `RET A` writes A to `ctx->ret` and returns `CCBPF_FINISHED`. For each instruction, `bpf_validate()` rejects jump targets at or beyond the instruction count, rejects negative or out-of-bounds four-byte offsets for `ST` and `LD MEM`, and rejects immediate division by zero. It requires a final `BPF_RET`-class instruction. It does not check scratch bounds for `LDX MEM/STX`, the lower bound of an unconditional jump target, or loop termination. `ccbpf_vm_step()` checks scratch accesses and division by zero at runtime, but does not check `pc < insn_count` before fetching an instruction.

# **4. Native Call Mechanism (BPF_COP)**

This is the key extension of ccBPF.

### **4.1 IR_NATIVE_CALL → BPF_COP**

The backend evaluates and stages arguments according to the ABI, then emits `BPF_MISC | BPF_COP` with the Native ID in k. Runtime lookup uses an integer ID and resolves a record containing the argument count and host function:
```c
struct native_entry {
    int         func_id;
    int         argc;
    native_fn_t fn;
};
```

```c
int func_id = ins->k;
struct native_entry *e =
    hashmap_get(&native_table, (void *)(uintptr_t)func_id);
```

The VM reads arguments from A, X and scratch according to argc, calls `e->fn()`, and places the result in A. The following emitted store saves that result into its temporary slot. A missing ID currently yields zero and continues execution. Native functions are host C code; their pointer, length, resource and blocking contracts belong to the host.

### **4.2 Argument ABI**

Up to four arguments use these locations:

- `A` → a0
- `X` → a1
- `mem[0..3]` → a2
- `mem[4..7]` → a3

```c
uint32_t a0 = A;
uint32_t a1 = X;
uint32_t a2 = 0;
uint32_t a3 = 0;
if (e->argc > 2)
    memcpy(&a2, &ctx->mem[0], sizeof(uint32_t));
if (e->argc > 3)
    memcpy(&a3, &ctx->mem[4], sizeof(uint32_t));
```

Staging the third and fourth arguments overwrites A, so the backend reloads argument 0 before COP. Unused register arguments are not guaranteed to be zero; host functions should consume only declared arguments.

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

`hook_table` maps a name to a `hook_entry`, whose head points to a program list. Attach inserts at the head, so newer attachments execute first. Repeated attach adds nodes rather than replacing existing programs.

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

Detach releases each program and list node, then clears head while retaining the registered hook name. Detach before attaching a replacement.

### **5.5 Running a Hook**

```c
uint32_t hook_run(const char *hook_name, uint8_t *frame, size_t frame_size)
```

- Executes all attached programs in order
- Returns the return value of the **last** program

Hook execution resolves the name and walks the list, setting host context before each program. An empty hook returns zero; otherwise it returns the last executed result, from the oldest attachment. Traversal is linear in the attachment count plus program execution cost. These operations have no internal synchronization; the host must coordinate concurrent run, attach and detach.

# **6. `.ccbpf` File Format**

An image contains a header, instruction array and string table. The header records region offsets and lengths:
```c
struct CCBPF_Header {
    uint32_t magic;
    uint16_t version;
    uint16_t flags;

    uint32_t code_offset;
    uint32_t code_size;

    uint32_t data_offset;
    uint32_t data_size;

    uint32_t entry;
};
```

Packing writes version 1 and entry 0. Instructions use the native `struct bpf_insn` layout. The string table stores a native int count, native int lengths and NUL-terminated strings. Compiler and target must agree on sizes, alignment, byte order and Native IDs.

Loading proceeds as follows:

1. Check header size, magic and code region.
2. Copy instructions and call `bpf_validate()`.
3. Allocate `ccbpf_program` and reconstruct the string table.
4. Store entry; current execution wrappers still begin at instruction 0.

The root loader does not create Maps. It checks the minimum image length, magic and the code/data end offsets against the image length, then calls `bpf_validate()`. It does not check version, flags, offset-addition overflow, instruction-size divisibility or a nonempty instruction sequence before validation. String counts and lengths are read directly; each count, length and string read has no individual bounds check. A loaded program owns its copies; unloading releases instructions, strings and the program object.

`ccbpf_pack_memory(insns, cap, count, out_len)` returns borrowed storage in the packing region. Pack, save or load, then call `bpf_builder_free()`. The MCU demo’s three-argument variant returns heap storage with a different cleanup contract.

# **7. Program Execution Flow**

The host registers a hook, then attaches the image. `hook_attach()` calls the loader itself, so no separate load is needed first:

```text
Register hook
    ↓ hook_attach(image)
Load and validate → ccbpf_program
    ↓ insert hook_node
Host event → hook_run(frame)
    ↓ ccbpf_run_frame
Zero ccbpf_ctx → ccbpf_vm_step
    ↓ dispatch instruction
Update A/X, mem and pc
    ↓ RET
ctx.ret → host result
```

For step scheduling, retain a `ccbpf_ctx` and drive the step interface. A call to `native_migrate` saves A/X and the next pc and yields `CCBPF_MIGRATE`; context pack/unpack copies execution state. Resuming also requires compatible code, strings, Native functions, context and external resources; copied addresses do not automatically become valid on another device.

# **8. Virtual Machine Summary**

The ccBPF VM has the following characteristics:

- **Classic BPF execution model with extended instructions**
- **Scratch mem[] serves as a unified “virtual stack frame”**
- **ctx access implemented via ABS mode**
- **Native calls implemented via BPF_COP**
- **Hook system supports dynamic attach/detach**
- **Loader-owned instructions and strings; host-provided Maps**

The VM is extremely small, portable, and suitable for MCU/RTOS environments, while also running cleanly in Linux userspace.

# 9. Complete example: from C source to a VM result of 8

This program assigns values to `sport` and `dport`, calculates `count`, and returns it. Follow the same program through lexing, parsing, IR generation, instruction selection and VM execution:

```c
int hook(void *ctx)
{
    int sport;
    int dport;
    int count;

    sport = 1;
    dport = 2;
    count = ((sport * dport) + dport) * dport;
    return count;
}
```

By hand, `1 * 2 = 2`, `2 + 2 = 4`, and `4 * 2 = 8`. Now let the compiler and VM perform the same calculation.

This example uses the Windows x64 MinGW data layout: pointers are 8 bytes, while `int` and C `long` are 4 bytes. The variable offsets, instruction layout and image sizes below follow that layout.

### 9.1 Lexing: identify tokens before evaluating anything

The lexer splits the character stream into tokens. For this assignment:

```c
count = ((sport * dport) + dport) * dport;
```

the token order is:

```text
ID(count) ASSIGN LPAREN LPAREN
ID(sport) STAR ID(dport) RPAREN
PLUS ID(dport) RPAREN STAR ID(dport) SEMICOLON
```

`count`, `sport` and `dport` are identifiers whose names remain in their tokens. `1` and `2` are `NUM` tokens carrying integer values; `int` is a basic-type keyword. The lexer does not yet know that `sport` holds 1, and it does not replace every variable with `int`. Types come from symbol lookup; variable values come from executing assignments.

The data structure is `lexer_token`: `tag` distinguishes token categories, `lexeme` stores names, and `int_val` stores integer values. The algorithm in `lexer.c` scans characters and creates the corresponding identifier, number or operator token.

Execution starts in `compile_and_run_example(source)`:

```text
compiler_init()
  → mg_region_create_pool()       frontend_region
  → mg_region_create_bump() × 3   longterm_region / string_region / ir_region
  → init_stmt_singletons()
  → init_constant_singletons()
lexer_init(&lex) → hashmap_init() → lexer_reserve() → hashmap_put()
lexer_set_input_buffer(source, strlen(source))
parser_new(&lex)
  → hashmap_init(native_decl_table)
  → mg_region_alloc(longterm_region, sizeof(Parser))
  → env_new(NULL) → hashmap_init(vars/types) → track_hashmap()
  → parser_move() → lexer_scan()
```

`lexer_set_input_buffer()` retains the source pointer and length and sets the read position to zero; the source must remain valid until scanning ends. `parser_move()` writes the next token to `Parser.look`. `lexer_scan()` calls `readch()` → `reader_next_char()`, retaining the current character in `lexer.peek`. It looks up completed identifiers with `hashmap_get()`; ordinary names use `new_lexer_token_word()`, and integers use `new_lexer_token_num()`. Decimal scanning applies `v = 10 × v + digit` and stores the result in `lexer_token.int_val`. Single-character symbols produce tokens too; `readch_match()` checks the second character of two-character operators.

Each successful `parser_match()` calls `parser_move()`. Branches that advance directly use the same scanning chain. `Parser.look` therefore holds the next unconsumed token.

### 9.2 Parsing: encode precedence in the tree

This grammar describes arithmetic precedence, including identifiers as operands:

```text
Expr   → Expr + Term | Expr - Term | Term
Term   → Term * Factor | Term / Factor | Factor
Factor → ( Expr ) | NUM | ID
```

The recursive-descent implementation uses loops for consecutive operators at the same precedence level rather than directly recursing into the left-recursive rules. `parser_expr()` handles addition and subtraction, `parser_term()` handles multiplication, division and remainder, and `parser_factor()` enters the inner expression when it encounters parentheses. It builds the inner multiplication first, then the addition, then the outer multiplication.

Removing parentheses and grammar-level wrappers such as `Expr`, `Term` and `Factor` leaves this abstract syntax tree for the assignment:

```text
Set(count)
└─ Mul
   ├─ Add
   │  ├─ Mul
   │  │  ├─ Id(sport)
   │  │  └─ Id(dport)
   │  └─ Id(dport)
   └─ Id(dport)
```

A derivation tree includes every grammar rule. An abstract syntax tree retains the operations and operands. This tree determines the calculation order: inner multiplication, addition, then outer multiplication.

The entry and function-body call chain is:

```text
parser_program()
  → node_newlabel() × 2 → node_emitlabel(L1) → ir_emit(IR_LABEL)
  → parse_hook_function_gen()
      → parser_move()/parser_match()      consume int hook(void *ctx)
      → ptr_new() → id_new_from_name() → id_new() → env_put_var()
      → parser_block_gen()
          → parser_match(LBRACE) → env_new(saved)
          → parser_decls()
          → parser_stmt() → statement's Node.gen callback  (loop)
          → mg_region_reset(frontend_region) → parser_match(RBRACE)
  → node_emitlabel(L2) → ir_emit(IR_LABEL)
```

The assignment expression enters `parser_bool()` → `parser_join()` → `parser_bitand()` → `parser_bitor()` → `parser_rel()` → `parser_expr()` → `parser_term()` → `parser_unary()` → `parser_postfix()` → `parser_factor()`. With no logical or relational operators here, the outer layers return the next layer's result. On `(`, `parser_factor()` first calls `parser_type()` to recognize a type. These parentheses contain expressions, so it proceeds to `parser_bool()` and matches `)`. Identifiers use symbol lookup; integers use `constant_int()` → `constant_new()`.

For the inner multiplication, `parser_term()` calls `arith_new(STAR,sport,dport)`. For addition, `parser_expr()` calls `arith_new(PLUS,inner multiplication,dport)`. The outer `parser_term()` calls `arith_new(STAR,addition,dport)`. Each constructor writes `Arith.e1/e2`; these three calls create the nesting shown above.

### 9.3 Semantics and symbols: resolve types and storage

`parser_decls()` creates an `Id` for each declaration and inserts it into the current scope with `env_put_var()`. A later reference looks up that name and obtains the same `Id` object.

The frontend assigns these offsets by incrementing `Parser.used` by each type's width:

| Name | Type width | Frontend offset | Role in this example |
| --- | --- | --- | --- |
| `ctx` | 8-byte pointer | 0 | Entry parameter; the example does not read context |
| `sport` | 4-byte `int` | 8 | Holds 1 |
| `dport` | 4-byte `int` | 12 | Holds 2 |
| `count` | 4-byte `int` | 16 | Holds the result, 8 |

The offset for `ctx` comes from registering the parameter; it does not mean the VM automatically writes a host pointer into scratch memory. With 4-byte pointers in a 32-bit build, this example assigns local offsets 4, 8 and 12.

Type checking happens during parsing. `arith_new()` selects a result type from the operand types. Every operand here is `int`, so all three arithmetic results are also `int`. Undeclared variables and unsupported arithmetic operand types produce frontend errors. `set_new()` stores the assignment target and source expression; it does not check assignment type compatibility.

The parameter belongs to the outer `Env`. `parser_block_gen()` creates a body scope whose `prev` points to that environment; the three locals belong to its `vars` table. Declaration processing is:

```text
parser_decls()
  → parser_type() → basic_type_from_token() → Type_Int
  → parser_match(BASIC/ID/SEMICOLON)
  → id_new(token, Type_Int, Parser.used)
  → env_put_var() → sym_strdup() → hashmap_put()
  → Parser.used += Type_Int->width
```

After the parameter, `used=8`; after `sport`, `dport` and `count`, it becomes 12, 16 and 20. `id_new()` stores `op/type/offset`, sets `TAG_ID` and installs `expr_gen`. For a reference, `parser_factor()` first calls `hashmap_get(native_decl_table,name)`. None of these names denotes a Native function, so it calls `env_get_var()`, which searches `Env.vars` with `hashmap_get()` and follows `prev` only when no entry is found.

`hashmap_get()` and `hashmap_put()` call `hashmap_bucket()` → `hashmap_hash()` to select a bucket, then traverse its list and compare string keys. New entries are heap allocated and linked into the bucket. `arith_new()` calls `type_max()`: a null type returns null; either `TYPE_INT` returns `Type_Int`; two `TYPE_CHAR` types return the first; other combinations return null and make `arith_new()` call `node_error()`. All three arithmetic nodes here receive `Type_Int`.

### 9.4 Store the tree in C structures

Expression nodes need an operator, a type and operands. Constants also need a value, and variables need a storage location. ccBPF represents these with the following structures:


| Structure | Stored data | Role here |
| --- | --- | --- |
| `Node` | Node tag and generation functions | Dispatch for assignments, arithmetic and returns |
| `Expr` | Operator, type and `temp_no` | Expression type and IR result number |
| `Id` | An `Expr` and a local offset | `sport`, `dport` and `count` |
| `Constant` | An `Expr` and an integer value | 1 and 2 |
| `Arith` | Operation information and `e1`, `e2` | Two multiplications and one addition |
| `Set` | Target `Id` and source expression | Three assignments |
| `Return` | Return expression | `return count` |

The structures store relationships; generation functions traverse them. The root frontend parses and generates one statement at a time in the entry function's outer block. The tree above therefore describes one assignment, without requiring a complete function AST first.

`Node.gen` connects parsing to IR generation:

```text
parser_block_gen() → parser_stmt()
  assignment: → parser_assign() → env_get_var() → parser_bool() → set_new()
              → s->base.gen(...) → set_gen() → expr_gen(rhs) → ir_emit(IR_STORE)
  return: → parser_bool() → return_new()
          → s->base.gen(...) → return_gen() → expr_gen(count) → ir_emit(IR_RET)
```

`Set.id` points to the target and `Set.expr` to the right-hand tree; its `Node.tag=TAG_SET` and `Node.gen=set_gen`. The three `Arith` nodes have `Expr.op=STAR/PLUS/STAR`, `Expr.type=Type_Int`, `Node.tag=TAG_ARITH` and the `expr_gen` callback. `Return.expr` points to the symbol-table `count` node and uses `return_gen`. Constructors allocate these nodes through `mg_region_alloc(longterm_region,sizeof(...))`; resetting `frontend_region` in the function body does not release them.

After emitting the store, `set_gen()` invokes both `tostring` callbacks and `node_emit()`. Here the text path uses `id_tostring()`, `constant_tostring()` and `arith_tostring()`, with `token_to_string()` and `region_strdup()`. This creates textual representations, not additional IR computations.

### 9.5 IR: separate loads, arithmetic and stores

`expr_gen()` assigns a result number before recursively generating operands, then emits the operation. `set_gen()` emits `IR_STORE` after its right-hand expression; `return_gen()` generates its expression before emitting `IR_RET`.

The generated IR is shown below. `t1` through `t8` identify temporary expression results; `mem[n]` means a four-byte access starting at scratch byte offset n:

```text
LABEL L1
MOVE  t1 <- 1
STORE mem[8] <- t1
MOVE  t2 <- 2
STORE mem[12] <- t2
LOAD  t6 <- mem[8]
LOAD  t7 <- mem[12]
MUL   t5 <- t6 * t7
LOAD  t7 <- mem[12]
ADD   t4 <- t5 + t7
LOAD  t7 <- mem[12]
MUL   t3 <- t4 * t7
STORE mem[16] <- t3
LOAD  t8 <- mem[16]
RET   t8
LABEL L2
```

Why is the outer result `t3`, while the inner multiplication uses `t5`? The generator allocates `t3` for the outer multiplication before visiting its children, then `t4` for the addition and `t5` for the inner multiplication. Number allocation is different from execution order: inner operations still execute first.

Why are there three loads into `t7`? All three references to `dport` resolve to the same `Id`. Its first generation assigns `temp_no = 7`, and subsequent references reuse that number but still emit a load. The current generator does not eliminate these repeated reads. A separate read places `count` in `t8` for the return.

IR records form a linked sequence through `next`. Local sources and destinations use `array_base` to carry byte offsets. The diagnostic form `MEM[8 + t0 * 4]` has an index field of zero here, so the address is simply offset 8; it does not require reading a runtime temporary named `t0`.

For the assignment to `count`, recursion and emitted fields are:

| Current `expr_gen()` node | Structure update | IR emitted after its children |
| --- | --- | --- |
| Outer `Arith(STAR)` | `new_temp()` → `temp_no=3`; recurse into `e1/e2` | `op=MUL,dst=3,src1=4,src2=7` |
| `Arith(PLUS)` | Allocate `temp_no=4`; recurse into both children | `op=ADD,dst=4,src1=5,src2=7` |
| Inner `Arith(STAR)` | Allocate `temp_no=5`; recurse into both children | `op=MUL,dst=5,src1=6,src2=7` |
| `Id(sport)` | First allocation gives `temp_no=6`; read `offset=8` | `op=LOAD,dst=6,array_base=8,array_index=0,array_width=4` |
| `Id(dport)`, three visits | First allocation gives `temp_no=7`; reuse it, reading `offset=12` on every visit | Each visit emits `op=LOAD,dst=7,array_base=12,array_index=0,array_width=4` |
| Return to `Set(count)` | Read `id->offset=16` and `expr->temp_no=3` | `op=STORE,src1=3,array_base=16,array_index=0,array_width=4` |

Numbers are allocated on entry, while arithmetic records are appended after child generation: numbering is preorder, arithmetic emission is postorder. The earlier assignments pass through `constant_int()` → `constant_new()` → `expr_gen()`, emitting `IR_MOVE(dst=1,src1=1)` and `IR_MOVE(dst=2,src1=2)`; `set_gen()` stores them at offsets 8 and 12. For the return, `return_gen()` → `expr_gen(Id(count))` allocates `t8` and emits `IR_LOAD`, followed by `IR_RET(src1=8)`.

Every record enters `ir_emit()`: allocate an `IR` in `ir_region`, copy its fields and set `next=NULL`; write `ir_tail->next` for a nonempty list or `ir_head` for an empty one, then update `ir_tail`. There are 16 records, including two labels. The parser emits each statement before reading the next. After the final label, `frontend_destroy()` calls `hashmap_destroy()`, `symbol_destroy()` and `mg_region_destroy()` to release lexical tables, scope tables and both frontend regions. The IR list remains in `ir_region`.

### 9.6 Backend: place temporaries and select instructions

The VM has two working registers, A and X. Temporaries live in scratch memory; `temp_slot()` computes their offsets as `64 + 4 × temporary_number`:

| Temporary | Byte offset | Final value | Meaning |
| --- | --- | --- | --- |
| `t1` | 68 | 1 | Constant for the first assignment |
| `t2` | 72 | 2 | Constant for the second assignment |
| `t3` | 76 | 8 | Outer multiplication result |
| `t4` | 80 | 4 | Addition result |
| `t5` | 84 | 2 | Inner multiplication result |
| `t6` | 88 | 1 | Loaded `sport` |
| `t7` | 92 | 2 | Loaded `dport` |
| `t8` | 96 | 8 | Loaded `count` |

`ir_lower_program()` walks the IR list and selects instructions. An immediate is loaded into A and stored in its temporary slot. A local load or store takes two instructions. A binary operation loads its operands into A and X, performs the operation, and stores A in the result slot.

For example, `MUL t5 <- t6 * t7` becomes:

```text
LD  mem[88]     A = t6
LDX mem[92]     X = t7
MUL X          A = A * X
ST  mem[84]     t5 = A
```

The whole program produces **34 instructions**: 8 for the two constant assignments, 8 for four variable reads, 12 for three binary operations, 2 to store `count`, and 2 to return it. Labels record positions but emit no instructions; there are no branches to patch. The execution table below lists the complete instruction sequence.

Backend calls and transformations are:

```text
bpf_builder_init() → mg_region_create_bump() → mg_region_alloc()
ir_mes_get() → traverse IR.next; return ir_head and label_count=3
ir_lower_program() → default_bpf_layout() → allocate pending[] / label_pc[]
  IR_LABEL → lower_label() → label_pc[label] = builder.count
  IR_MOVE  → lower_move() → temp_slot() → bpf_builder_emit() × 2
  IR_LOAD/STORE → temp_slot() → bpf_builder_emit() × 2
  IR_MUL/ADD → lower_binop() → temp_slot() × 3 → bpf_builder_emit() × 4
  IR_RET → temp_slot() → bpf_builder_emit() × 2
  → patch_jumps()
ir_free() → mg_region_destroy(ir_region)
```

`bpf_builder` starts with `count=0,capacity=128`; `insns` points into its backend region. `bpf_builder_emit()` writes `insns[count]` and increments `count`. With 34 instructions, this example does not call `bpf_builder_grow()`. `default_bpf_layout()` supplies `temp_base=64`, and `temp_slot()` converts temporary IDs into byte offsets. L1 and L2 set `label_pc[1]=0` and `label_pc[2]=34`. There are no pending jumps, so `patch_jumps()` changes no instruction.

| IR records in emission order | Selection path | Instruction indices |
| --- | --- | --- |
| `LABEL L1` | `lower_label()` | No instruction; record 0 |
| `MOVE t1; STORE mem[8]` | `lower_move()`; STORE branch | 0–1; 2–3 |
| `MOVE t2; STORE mem[12]` | `lower_move()`; STORE branch | 4–5; 6–7 |
| `LOAD t6; LOAD t7; MUL t5` | LOAD branch; `lower_binop()` | 8–9; 10–11; 12–15 |
| `LOAD t7; ADD t4` | LOAD branch; `lower_binop()` | 16–17; 18–21 |
| `LOAD t7; MUL t3` | LOAD branch; `lower_binop()` | 22–23; 24–27 |
| `STORE mem[16]; LOAD t8; RET t8` | STORE, LOAD and RET branches | 28–29; 30–31; 32–33 |
| `LABEL L2` | `lower_label()` | No instruction; record 34 |

### 9.7 Packing and loading: create an executable program object

The header stores `version` and `flags` alongside code and string offsets.



`ccbpf_pack_memory()` writes a header, the 34 instructions and a string table. In this build, the header is 28 bytes and each instruction is 8 bytes. There are no strings, but the data section still contains a four-byte string count of zero:

```text
0..27       CCBPF_Header
28..299     34 × 8-byte instructions
300..303    string count: 0
total       304 bytes
```

The header contains `code_offset = 28`, `code_size = 272`, `data_offset = 300`, `data_size = 4` and `entry = 0`. The 304-byte image size is separate from compiler or VM RAM usage.

The loader checks the image header and code region, copies instructions, runs `bpf_validate()`, and creates a `ccbpf_program`. This program's instructions and scratch accesses pass validation. Once loading succeeds, the program owns its instruction copy and the packing region can be released. Releasing the builder before loading would invalidate the image pointer.

`bpf_builder_data()` returns the instruction-array pointer and `bpf_builder_count()` returns 34. Both feed `ccbpf_pack_memory()`, which calls `mg_region_create_bump()` and `mg_region_alloc()`, copies the header, instructions and string count with `memcpy()`, and writes 304 through `out_len`.

Loading follows `ccbpf_load_from_memory(image,304)` → check header and code range → heap allocate and copy 272 instruction bytes → `bpf_validate(insns,34)` → allocate and clear `ccbpf_program` → read the string table → return the program pointer. There are no jumps; all checked scratch accesses fit, and the last instruction is `RET A`, so validation returns 1. The program receives `insns`, `insn_count=34`, `string_count=0` and `entry=0`; no string contents are allocated.

`bpf_builder_free()` clears the builder fields and destroys `string_region`, `backend_region` and `pack_region`. The image and builder pointers are now invalid, while `prog.insns` remains executable because the loader copied the instructions.

### 9.8 VM execution: update registers and scratch one instruction at a time

A zero-initialized `ccbpf_ctx` starts with `pc = 0`, `A = 0` and `X = 0`. For this trace, each `ccbpf_vm_step()` call executes one instruction and reuses the same context.

The table's `pc` is the executed instruction index; A and X are their values after that instruction. `LD` and `ST` access four-byte values. Every instruction has `jt = jf = 0`, so those fields are omitted.

| pc | code | Instruction | A | X | Store or return |
| --- | --- | --- | --- | --- | --- |
| 0 | `0x0000` | `LD #1` | 1 | 0 | — |
| 1 | `0x0002` | `ST mem[68]` | 1 | 0 | `mem[68] = 1` |
| 2 | `0x0060` | `LD mem[68]` | 1 | 0 | — |
| 3 | `0x0002` | `ST mem[8]` | 1 | 0 | `mem[8] = 1` |
| 4 | `0x0000` | `LD #2` | 2 | 0 | — |
| 5 | `0x0002` | `ST mem[72]` | 2 | 0 | `mem[72] = 2` |
| 6 | `0x0060` | `LD mem[72]` | 2 | 0 | — |
| 7 | `0x0002` | `ST mem[12]` | 2 | 0 | `mem[12] = 2` |
| 8 | `0x0060` | `LD mem[8]` | 1 | 0 | — |
| 9 | `0x0002` | `ST mem[88]` | 1 | 0 | `mem[88] = 1` |
| 10 | `0x0060` | `LD mem[12]` | 2 | 0 | — |
| 11 | `0x0002` | `ST mem[92]` | 2 | 0 | `mem[92] = 2` |
| 12 | `0x0060` | `LD mem[88]` | 1 | 0 | — |
| 13 | `0x0061` | `LDX mem[92]` | 1 | 2 | — |
| 14 | `0x002c` | `MUL X` | 2 | 2 | — |
| 15 | `0x0002` | `ST mem[84]` | 2 | 2 | `mem[84] = 2` |
| 16 | `0x0060` | `LD mem[12]` | 2 | 2 | — |
| 17 | `0x0002` | `ST mem[92]` | 2 | 2 | `mem[92] = 2` |
| 18 | `0x0060` | `LD mem[84]` | 2 | 2 | — |
| 19 | `0x0061` | `LDX mem[92]` | 2 | 2 | — |
| 20 | `0x000c` | `ADD X` | 4 | 2 | — |
| 21 | `0x0002` | `ST mem[80]` | 4 | 2 | `mem[80] = 4` |
| 22 | `0x0060` | `LD mem[12]` | 2 | 2 | — |
| 23 | `0x0002` | `ST mem[92]` | 2 | 2 | `mem[92] = 2` |
| 24 | `0x0060` | `LD mem[80]` | 4 | 2 | — |
| 25 | `0x0061` | `LDX mem[92]` | 4 | 2 | — |
| 26 | `0x002c` | `MUL X` | 8 | 2 | — |
| 27 | `0x0002` | `ST mem[76]` | 8 | 2 | `mem[76] = 8` |
| 28 | `0x0060` | `LD mem[76]` | 8 | 2 | — |
| 29 | `0x0002` | `ST mem[16]` | 8 | 2 | `mem[16] = 8` |
| 30 | `0x0060` | `LD mem[16]` | 8 | 2 | — |
| 31 | `0x0002` | `ST mem[96]` | 8 | 2 | `mem[96] = 8` |
| 32 | `0x0060` | `LD mem[96]` | 8 | 2 | — |
| 33 | `0x0016` | `RET A` | 8 | 2 | `ret = 8` |

Instruction 14 computes 2, instruction 20 computes 4, and instruction 26 computes 8. Instruction 29 writes 8 to `count`. At instruction 33, `BPF_RET | BPF_A` writes A to `state.ret` and returns `CCBPF_FINISHED`. The return does not increment pc, so the final context still has `pc = 33`.

The calculation is complete: source names became symbol entries, the expression tree became IR, IR became instructions, and those instructions changed registers and scratch until the VM returned 8.

Every call follows the same fetch loop: `compile_and_run_example()` → `ccbpf_vm_step(&state,prog,NULL,0,0,1)` → read `A/X/pc` from `state` → fetch `prog->insns[pc]` → dispatch on `bpf_insn.code`. Immediate loads put `k` into A. `LD MEM/LDX MEM` check the four-byte range at `k` and use `memcpy()` to read A/X. `ST` checks the range and copies A into `state.mem+k`. `MUL X` and `ADD X` update A directly.

An ordinary instruction increments the local pc. Once the one-instruction budget is exhausted, the function saves A/X/pc to `state` and returns `CCBPF_OK`; the host calls it again. Instruction 33 enters `BPF_RET|BPF_A`, writes A to `state.ret`, saves A/X/pc and immediately returns `CCBPF_FINISHED`. The host exits its loop and checks `ret==8`. This path does not call `ccbpf_vm_run()` or create another execution state.

### 9.9 Connect the stages through the host API

Pass the source program at the start of this section to `compile_and_run_example()`. It compiles and loads the program, then executes one instruction at a time while printing register states. A return value of zero means execution finished with a result of 8. Adjust the pool capacities to suit the host memory budget and program size.

```c
#include <stdio.h>
#include <string.h>
#include "lexer.h"
#include "parser.h"
#include "ir.h"
#include "ir_lowering.h"
#include "bpf_builder.h"
#include "ccbpf.h"

int compile_and_run_example(const char *source)
{
    struct lexer lex;
    struct bpf_builder builder;
    struct ir_mes ir;
    size_t image_len = 0;

    compiler_init(16, 30 * 1024, 1024, 15 * 1024);
    lexer_init(&lex);
    lexer_set_input_buffer(source, strlen(source));
    struct Parser *parser = parser_new(&lex);
    parser_program(parser);
    frontend_destroy(&lex);

    bpf_builder_init(&builder, 24 * 1024);
    ir_mes_get(&ir);
    ir_lower_program(ir.ir_head, ir.label_count, &builder);
    ir_free();

    uint8_t *image = ccbpf_pack_memory(
        bpf_builder_data(&builder), 7 * 1024,
        (size_t)bpf_builder_count(&builder), &image_len);
    struct ccbpf_program *prog = image
        ? ccbpf_load_from_memory(image, image_len) : NULL;
    bpf_builder_free(&builder);
    if (!prog)
        return -1;

    struct ccbpf_ctx state = {0};
    enum ccbpf_status status;
    do {
        uint32_t pc = state.pc;
        status = ccbpf_vm_step(&state, prog, NULL, 0, 0, 1);
        printf("pc=%u A=%u X=%u ret=%u status=%d\n",
               pc, state.A, state.X, state.ret, status);
    } while (status == CCBPF_OK);

    int ok = status == CCBPF_FINISHED && state.ret == 8;
    ccbpf_unload(prog);
    return ok ? 0 : -1;
}
```

The program does not read `ctx` or call Native functions, so the step loop needs neither a context buffer nor registered host helpers. To obtain just the final result, call `ccbpf_run_frame(prog, NULL, 0)`, which returns 8. Alternatively, attach the same image to a registered hook and execute it with `hook_run()`.

After checking the result, `compile_and_run_example()` calls `ccbpf_unload(prog)` to release the instruction copy, optional data, individual strings, string-pointer table and `ccbpf_program`. `state`, `lex`, `builder` and `ir` are local host objects whose lifetime ends on return. Region allocation dispatches through `mg_region_alloc()` to pool or bump storage: bump allocation aligns to four bytes and advances its offset; pool allocation selects a block class and calls `membit_alloc()`. `mg_region_destroy()` frees a bump buffer, or calls `membit_destroy()` and frees pool nodes.

The same program has two other host execution paths:

```text
Result only: ccbpf_run_frame() → clear ccbpf_ctx
            → loop over ccbpf_vm_step(...,64) → return ctx.ret on FINISHED

Attachment: ccbpf_system_init() → initialize Hook / Native name tables
            hook_register() → create hook_entry → hashmap_put()
            hook_attach() → find_hook() → ccbpf_load_from_memory()
                          → create hook_node, insert at hook_entry.head
            hook_run() → find_hook() → traverse hook_node.next
                       → set prog.ctx → ccbpf_run_frame() → ccbpf_vm_step()
            hook_detach() → find_hook() → ccbpf_unload() → free hook_node
```

Direct execution returns 8; attached execution uses the same 34 instructions and returns 8. The host owns the attachment list and program instructions, while each `ccbpf_run_frame()` call owns its execution state; their lifetimes are separate.
