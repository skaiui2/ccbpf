#ifndef SYMBOLS_H
#define SYMBOLS_H

#include "hashmap.h"

enum type_tag {
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_CHAR,
    TYPE_BOOL,
    TYPE_ARRAY,
    TYPE_FUNC,
    TYPE_PTR,
    TYPE_STRUCT,
    TYPE_ENUM
};

struct Type {
    enum type_tag tag;
    int width;
};

struct Array {
    struct Type base;
    struct Type *of;
    int size;
};

struct FuncType {
    struct Type base;
    struct Type *ret;
    struct Type **params;
    int param_count;
};

struct PtrType {
    struct Type base;
    struct Type *to;
};

struct StructType {
    struct Type base;
    struct hashmap fields;
};

struct EnumType {
    struct Type base;
    struct hashmap values;
};

enum symbol_kind {
    SYM_VAR,
    SYM_FUNC,
    SYM_TYPE,
    SYM_PARAM,
    SYM_FIELD,
    SYM_ENUM_CONST
};

struct Env {
    struct hashmap vars;
    struct hashmap types;
    struct Env *prev;
    int level;
};

struct Type *type_new(enum type_tag tag, int width);
struct Array *array_new(struct Type *of, int size);
struct FuncType *func_new(struct Type *ret, struct Type **params, int count);
struct PtrType *ptr_new(struct Type *to);
struct StructType *struct_new(void);
struct EnumType *enum_new(void);

struct Env *env_new(struct Env *prev);
void env_put_var(struct Env *env, const char *name, struct Id *id);
void env_put_type(struct Env *env, const char *name, struct Type *type);

struct Id *env_get_var(struct Env *env, const char *name);
struct Type *env_get_type(struct Env *env, const char *name);

int type_equal(struct Type *a, struct Type *b);


#endif
