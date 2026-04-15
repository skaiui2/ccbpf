#ifndef PREFIX_H
#define PREFIX_H

struct prefix_map {
    void *root;
};

struct prefix_leaf {
    char *key;
    void *value;
};

struct prefix_node {
    void *child[2];
    unsigned int byte;
    unsigned char otherbits;
};

void prefix_map_init(struct prefix_map *m);
void *prefix_map_get(struct prefix_map *m, const char *key);
int prefix_map_set(struct prefix_map *m, const char *key, void *value);
int prefix_map_del(struct prefix_map *m, const char *key);
void prefix_map_clear(struct prefix_map *m);

int prefix_map_iter(struct prefix_map *m,
                    int (*fn)(const char *key, void *value, void *arg),
                    void *arg);

int prefix_map_iter_prefix(struct prefix_map *m, const char *prefix,
                           int (*fn)(const char *key, void *value, void *arg),
                           void *arg);

#endif
