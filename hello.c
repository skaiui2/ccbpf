struct udp_hdr {
    unsigned short sport;
    unsigned short dport;
};

int hook(void *ctx)
{
    struct udp_hdr *uh;
    unsigned int sport;
    unsigned int dport;
    unsigned int count;
    unsigned int last;

    uh = (struct udp_hdr *)&ctx[0];

    sport = ntohs(uh->sport);
    dport = ntohs(uh->dport);

    print("=== packet ===\n");

    print("sport=");
    print(sport);
    print("\n");

    print("dport=");
    print(dport);
    print("\n");

    count = map_lookup(0, sport);
    count = count + 1;
    map_update(0, sport, count);

    print("count=");
    print(count);
    print("\n");

    map_update(1, sport, dport);

    last = map_lookup(1, sport);

    print("last_dport=");
    print(last);
    print("\n");

    return sport + dport;
}
