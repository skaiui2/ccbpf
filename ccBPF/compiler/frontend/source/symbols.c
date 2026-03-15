#include <memory.h>
#include "symbols.h"
#include "heap.h"
#include "lexer.h"

#define ALL_MAP_SIZE 32
#define STRUCT_MAP_SIZE 32
#define ENUM_MAP_SIZE 32
#define ENV_VAR_MAP_SIZE 64
#define ENV_TYPE_MAP_SIZE 32

static struct hashmap *all_hashmaps[ALL_MAP_SIZE];
static int hashmap_count = 0;

static void track_hashmap(struct hashmap *h) {
    all_hashmaps[hashmap_count++] = h;
}

static char *sym_strdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *p = mg_region_alloc(longterm_region, n);
    memcpy(p, s, n);
    return p;
}

struct Type *type_new(enum type_tag tag, int width)
{
    struct Type *t = mg_region_alloc(longterm_region, sizeof(struct Type));
    t->tag = tag;
    t->width = width;
    return t;
}

struct Array *array_new(struct Type *of, int size)
{
    struct Array *a = mg_region_alloc(longterm_region,sizeof(struct Array));
    a->base.tag = TYPE_ARRAY;
    a->base.width = of->width * size;
    a->of = of;
    a->size = size;
    return a;
}

struct FuncType *func_new(struct Type *ret, struct Type **params, int count)
{
    struct FuncType *f = mg_region_alloc(longterm_region,sizeof(struct FuncType));
    f->base.tag = TYPE_FUNC;
    f->base.width = 0;
    f->ret = ret;
    f->params = params;
    f->param_count = count;
    return f;
}

struct PtrType *ptr_new(struct Type *to)
{
    struct PtrType *p = mg_region_alloc(longterm_region,sizeof(struct PtrType));
    p->base.tag = TYPE_PTR;
    p->base.width = sizeof(void *);
    p->to = to;
    return p;
}

struct StructType *struct_new(void)
{
    struct StructType *s = mg_region_alloc(longterm_region,sizeof(struct StructType));
    s->base.tag = TYPE_STRUCT;
    s->base.width = 0;
    hashmap_init(&s->fields, STRUCT_MAP_SIZE, HASHMAP_KEY_STRING);
    track_hashmap(&s->fields);
    return s;
}

struct EnumType *enum_new(void)
{
    struct EnumType *e = mg_region_alloc(longterm_region,sizeof(struct EnumType));
    e->base.tag = TYPE_ENUM;
    e->base.width = sizeof(int);
    hashmap_init(&e->values, ENUM_MAP_SIZE, HASHMAP_KEY_STRING);
    track_hashmap(&e->values);
    return e;
}

struct Env *env_new(struct Env *prev)
{
    struct Env *env = mg_region_alloc(longterm_region,sizeof(struct Env));
    hashmap_init(&env->vars, ENV_VAR_MAP_SIZE, HASHMAP_KEY_STRING);
    track_hashmap(&env->vars);
    hashmap_init(&env->types, ENV_TYPE_MAP_SIZE, HASHMAP_KEY_STRING);
    track_hashmap(&env->types);
    env->prev = prev;
    env->level = prev ? prev->level + 1 : 0;
    return env;
}

void env_put_var(struct Env *env, const char *name, struct Id *id)
{
    char *k = sym_strdup(name);
    hashmap_put(&env->vars, k, id);
}

void env_put_type(struct Env *env, const char *name, struct Type *type)
{
    char *k = sym_strdup(name);
    hashmap_put(&env->types, k, type);
}

struct Id *env_get_var(struct Env *env, const char *name)
{
    for (; env; env = env->prev) {
        struct Id *id = hashmap_get(&env->vars, (void *)name);
        if (id) return id;
    }
    return NULL;
}

struct Type *env_get_type(struct Env *env, const char *name)
{
    for (; env; env = env->prev) {
        struct Type *t = hashmap_get(&env->types, (void *)name);
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
        case TYPE_SHORT:
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

void symbol_destroy(void)
{
    for (int i = 0; i < hashmap_count; i++) {
        if (!all_hashmaps[i]) {
            break;
        }
        hashmap_destroy(all_hashmaps[i]);
    }
    hashmap_count = 0;
}
