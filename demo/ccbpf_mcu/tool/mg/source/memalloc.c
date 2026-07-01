#include "memalloc.h"
#include "rbtree.h"
#include "macro.h"
#include "link_list.h"

#define MIN_SIZE ((size_t)(heap_struct_size << 1))

#define CONFIG_HEAP (10 * 1024)
#define ALIGNMENT_BYTE 0x07

struct heap_node {
    struct list_node link_node;
    struct rb_node iter_node;
    char used;
};

struct xheap {
    struct list_node cache_node;
    size_t all_size;
};

static struct xheap the_heap = {
        .cache_node.next = NULL,
        .cache_node.prev = NULL,
        .all_size = CONFIG_HEAP,
};

static uint8_t all_heap[CONFIG_HEAP];

static const size_t heap_struct_size =
        (sizeof(struct heap_node) + (size_t)ALIGNMENT_BYTE) & ~(ALIGNMENT_BYTE);

static struct rb_root mem_tree;

static void insert_free_block(struct heap_node *node);

void mem_init(void)
{
    struct heap_node *first;
    size_t start;

    rb_root_init(&mem_tree);
    list_node_init(&the_heap.cache_node);

    start = (size_t)all_heap;

    if (start & ALIGNMENT_BYTE) {
        start += ALIGNMENT_BYTE;
        start &= ~ALIGNMENT_BYTE;
        the_heap.all_size -= (size_t)(start - (size_t)all_heap);
    }

    first = (struct heap_node *)start;

    list_node_init(&first->link_node);
    list_add_next(&the_heap.cache_node, &first->link_node);

    rb_node_init(&first->iter_node);
    first->iter_node.value = the_heap.all_size;
    first->used = 0;

    rb_insert_node(&mem_tree, &first->iter_node);
}

void *mem_malloc(size_t want)
{
    struct heap_node *use_node, *new_node;
    struct rb_node *find;
    size_t align_req;
    size_t block_size;
    void *ret = NULL;

    if (!want)
        goto out;

    want += heap_struct_size;

    if (want & ALIGNMENT_BYTE) {
        align_req = (ALIGNMENT_BYTE + 1) - (want & ALIGNMENT_BYTE);
        want += align_req;
    }

    if (the_heap.cache_node.prev == NULL)
        mem_init();

    find = mem_tree.last_node;
    if (!find || find->value < want)
        goto out;

    find = rb_first_greater(&mem_tree, want);
    if (!find)
        goto out;

    block_size = find->value;
    rb_remove_node(&mem_tree, find);

    use_node = container_of(find, struct heap_node, iter_node);
    use_node->used = 1;

    ret = (void *)(((uint8_t *)use_node) + heap_struct_size);

    if ((block_size - want) > MIN_SIZE) {
        new_node = (struct heap_node *)(((uint8_t *)use_node) + want);
        new_node->used = 0;

        use_node->iter_node.value = want;

        list_add_next(&use_node->link_node, &new_node->link_node);

        new_node->iter_node.value = block_size - want;
        rb_insert_node(&mem_tree, &new_node->iter_node);
    }

    the_heap.all_size -= use_node->iter_node.value;

    out:
    return ret;
}

void mem_free(void *ptr)
{
    struct heap_node *node, *adj, *insert;
    struct list_node *iter;
    uint8_t *raw;

    if (!ptr)
        return;

    raw = (uint8_t *)ptr - heap_struct_size;
    node = (struct heap_node *)raw;

    node->used = 0;
    the_heap.all_size += node->iter_node.value;

    insert = node;

    iter = node->link_node.next;
    if (iter != &the_heap.cache_node) {
        adj = container_of(iter, struct heap_node, link_node);
        if (!adj->used) {
            list_remove(&adj->link_node);
            node->iter_node.value += adj->iter_node.value;
            rb_remove_node(&mem_tree, &adj->iter_node);
            insert = node;
        }
    }

    iter = node->link_node.prev;
    if (iter != &the_heap.cache_node) {
        adj = container_of(iter, struct heap_node, link_node);
        if (!adj->used) {
            list_remove(&node->link_node);
            adj->iter_node.value += node->iter_node.value;
            rb_remove_node(&mem_tree, &adj->iter_node);
            insert = adj;
        }
    }

    rb_insert_node(&mem_tree, &insert->iter_node);
}
