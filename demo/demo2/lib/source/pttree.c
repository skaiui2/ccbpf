#include "pttree.h"
#include <stdlib.h>

static inline int ip_get_bit(uint32_t ip, int bit)
{
    return (ip >> (31 - bit)) & 1;
}

int mask_to_len(uint32_t mask)
{
    int len = 0;
    for (int i = 31; i >= 0; i--) {
        if (mask & (1u << i))
            len++;
        else
            break;
    }
    return len;
}

uint32_t len_to_mask(int len)
{
    if (len <= 0)
        return 0;
    if (len >= 32)
        return 0xffffffffu;
    return (~0u) << (32 - len);
}

static struct trie_node *node_create(int bit)
{
    struct trie_node *n = calloc(1, sizeof(*n));
    if (!n)
        return NULL;
    n->bit = bit;
    return n;
}

static void node_destroy(struct trie_node *n)
{
    if (!n)
        return;
    node_destroy(n->left);
    node_destroy(n->right);
    if (n->route)
        free(n->route);
    free(n);
}

struct trie_node *trie_create(void)
{
    return node_create(0);
}

void trie_destroy(struct trie_node *root)
{
    node_destroy(root);
}

int trie_insert(struct trie_node *root, uint32_t ip, uint32_t mask, void *next_hop)
{
    int len = mask_to_len(mask);
    uint32_t prefix = ip & mask;

    struct trie_node *node = root;

    for (int i = 0; i < len; i++) {
        int b = ip_get_bit(prefix, i);
        struct trie_node **child = (b == 0) ? &node->left : &node->right;
        if (!*child) {
            *child = node_create(i + 1);
            if (!*child)
                return -1;
        }
        node = *child;
    }

    if (!node->route) {
        node->route = malloc(sizeof(*node->route));
        if (!node->route)
            return -1;
    }

    node->route->prefix = prefix;
    node->route->mask = mask;
    node->route->next_hop = next_hop;

    return 0;
}

struct route_entry *trie_lookup(struct trie_node *root, uint32_t ip)
{
    struct trie_node *node = root;
    struct route_entry *best = NULL;

    if (node->route)
        best = node->route;

    for (int i = 0; i < 32; i++) {
        int b = ip_get_bit(ip, i);
        node = (b == 0) ? node->left : node->right;
        if (!node)
            break;
        if (node->route)
            best = node->route;
    }

    return best;
}

int trie_delete(struct trie_node *root, uint32_t ip, uint32_t mask)
{
    int len = mask_to_len(mask);
    uint32_t prefix = ip & mask;

    struct trie_node *stack[33];
    struct trie_node *node = root;

    for (int i = 0; i < len; i++) {
        stack[i] = node;
        int b = ip_get_bit(prefix, i);
        node = (b == 0) ? node->left : node->right;
        if (!node)
            return -1;
    }

    if (!node->route)
        return -1;

    free(node->route);
    node->route = NULL;

    for (int i = len - 1; i >= 0; i--) {
        struct trie_node *parent = stack[i];
        if (node->route || node->left || node->right)
            break;

        int b = ip_get_bit(prefix, i);
        if (b == 0)
            parent->left = NULL;
        else
            parent->right = NULL;

        free(node);
        node = parent;
    }

    return 0;
}
