#include "symbols.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


struct Type *type_new(enum type_tag tag, int width)
{
    struct Type *t = malloc(sizeof(struct Type));
    t->tag = tag;
    t->width = width;
    return t;
}

struct Array *array_new(struct Type *of, int size)
{
    struct Array *a = malloc(sizeof(struct Array));
    a->base.tag = TYPE_ARRAY;
    a->base.width = of->width * size;
    a->of = of;
    a->size = size;
    return a;
}

struct FuncType *func_new(struct Type *ret, struct Type **params, int count)
{
    struct FuncType *f = malloc(sizeof(struct FuncType));
    f->base.tag = TYPE_FUNC;
    f->base.width = 0;
    f->ret = ret;
    f->params = params;
    f->param_count = count;
    return f;
}

struct PtrType *ptr_new(struct Type *to)
{
    struct PtrType *p = malloc(sizeof(struct PtrType));
    p->base.tag = TYPE_PTR;
    p->base.width = sizeof(void *);
    p->to = to;
    return p;
}

struct StructType *struct_new(void)
{
    struct StructType *s = malloc(sizeof(struct StructType));
    s->base.tag = TYPE_STRUCT;
    s->base.width = 0;
    hashmap_init(&s->fields, 32, HASHMAP_KEY_STRING);
    return s;
}

struct EnumType *enum_new(void)
{
    struct EnumType *e = malloc(sizeof(struct EnumType));
    e->base.tag = TYPE_ENUM;
    e->base.width = sizeof(int);
    hashmap_init(&e->values, 32, HASHMAP_KEY_STRING);
    return e;
}

struct Env *env_new(struct Env *prev)
{
    struct Env *env = malloc(sizeof(struct Env));
    hashmap_init(&env->vars, 64, HASHMAP_KEY_STRING);
    hashmap_init(&env->types, 32, HASHMAP_KEY_STRING);
    env->prev = prev;
    env->level = prev ? prev->level + 1 : 0;
    return env;
}

void env_put_var(struct Env *env, const char *name, struct Id *id)
{
    hashmap_put(&env->vars, name, id);
}

void env_put_type(struct Env *env, const char *name, struct Type *type)
{
    hashmap_put(&env->types, name, type);
}

struct Id *env_get_var(struct Env *env, const char *name)
{
    for (; env; env = env->prev) {
        struct Id *id = hashmap_get(&env->vars, name);
        if (id) return id;
    }
    return NULL;
}

struct Type *env_get_type(struct Env *env, const char *name)
{
    for (; env; env = env->prev) {
        struct Type *t = hashmap_get(&env->types, name);
        if (t) return t;
    }
    return NULL;
}

int type_equal(struct Type *a, struct Type *b)
{
    if (a->tag != b->tag)
        return 0;

    switch (a->tag) {
        case TYPE_INT:
        case TYPE_FLOAT:
        case TYPE_CHAR:
        case TYPE_BOOL:
            return 1;

        case TYPE_PTR:
            return type_equal(((struct PtrType *)a)->to,
                              ((struct PtrType *)b)->to);

        case TYPE_ARRAY:
            return ((struct Array *)a)->size == ((struct Array *)b)->size &&
                   type_equal(((struct Array *)a)->of,
                              ((struct Array *)b)->of);

        case TYPE_FUNC: {
            struct FuncType *fa = (struct FuncType *)a;
            struct FuncType *fb = (struct FuncType *)b;
            if (!type_equal(fa->ret, fb->ret)) return 0;
            if (fa->param_count != fb->param_count) return 0;
            for (int i = 0; i < fa->param_count; i++)
                if (!type_equal(fa->params[i], fb->params[i]))
                    return 0;
            return 1;
        }

        case TYPE_STRUCT:
        case TYPE_ENUM:
            return a == b;
    }

    return 0;
}
