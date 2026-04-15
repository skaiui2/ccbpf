#ifndef PTTREE_H
#define PTTREE_H

#include <stdint.h>

struct route_entry {
    uint32_t prefix;
    uint32_t mask;
    void *next_hop;
};

struct trie_node {
    int bit;
    struct trie_node *left;
    struct trie_node *right;
    struct route_entry *route;
};

struct trie_node *trie_create(void);
void trie_destroy(struct trie_node *root);

int trie_insert(struct trie_node *root, uint32_t ip, uint32_t mask, void *next_hop);
int trie_delete(struct trie_node *root, uint32_t ip, uint32_t mask);

struct route_entry *trie_lookup(struct trie_node *root, uint32_t ip);

int mask_to_len(uint32_t mask);
uint32_t len_to_mask(int len);

#endif
