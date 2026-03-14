#ifndef _RBTREE_H_
#define _RBTREE_H_

#include <stdint.h>
#include <stddef.h>

#define RB_RED   0
#define RB_BLACK 1

struct rb_node;
struct rb_root;

typedef struct rb_node *rb_node_handle;
typedef struct rb_root *rb_root_handle;

struct rb_node {
    struct rb_node *rb_parent;
    rb_root_handle root;
    uint8_t rb_color;
    struct rb_node *rb_right;
    struct rb_node *rb_left;
    uint32_t value;
};

struct rb_root {
    struct rb_node *rb_node;
    struct rb_node *first_node;
    struct rb_node *last_node;
    uint32_t count;
};

void rb_root_init(rb_root_handle root);
void rb_node_init(rb_node_handle node);
void rb_insert_node(rb_root_handle root, rb_node_handle new_node);
void rb_remove_node(rb_root_handle root, rb_node_handle node);

rb_node_handle rb_first(rb_root_handle root);
rb_node_handle rb_last(rb_root_handle root);
rb_node_handle rb_next(rb_node_handle node);
rb_node_handle rb_prev(rb_node_handle node);
rb_node_handle rb_first_greater(rb_root_handle root, size_t key);

void rb_replace_node(rb_node_handle victim, rb_node_handle new_node,
                     rb_root_handle root);

#endif
