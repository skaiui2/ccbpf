struct udp_hdr {
    unsigned short sport;
    unsigned short dport;
};

int hook(void *ctx)
{
    unsigned int x;
    unsigned int y;
    unsigned int key;
    unsigned int val;
    struct udp_hdr *uh;

    uh = (struct udp_hdr *)&ctx[0];
    x = ntohs(uh->sport);
    print(x);
    y = ntohs(uh->dport);
    print(y);

    key = x;
    val = y;

    map_update(0, key, val);

    val = map_lookup(0, key);
    print(val);

    print(map_lookup(0, 9999)); 
    map_update(0, 1, 11);
    map_update(0, 2, 22);
    map_update(0, 3, 33);
    print(map_lookup(0, 1));
    print(map_lookup(0, 2));
    print(map_lookup(0, 3));

    return x + y;
}
