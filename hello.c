struct udp_hdr {
    unsigned short sport;
    unsigned short dport;
    unsigned short len;
    unsigned short cksum;
};

int hook(void *ctx)
{
    struct udp_hdr *uh;
    unsigned int sport;
    unsigned int dport;
    unsigned int len;
    unsigned int now;
    unsigned int tokens;
    unsigned int last_ts;
    unsigned int rate;
    unsigned int burst;
    unsigned int delta;
    unsigned int add;

    uh = (struct udp_hdr *)ctx;

    sport = ntohs(uh->sport);
    dport = ntohs(uh->dport);
    len   = ntohs(uh->len);

    now = now_ms();
    print_str("now_time=");
    print(now);
    print_str("\n");

    tokens  = map_lookup(0, sport);
    print_str("tokens=");
    print(tokens);
    print_str("\n");
    last_ts = map_lookup(1, sport);

    print_str("last_ts=");
    print(last_ts);
    print_str("\n");

    rate  = 5000;
    burst = 3000;

    if (last_ts == 0) {
        tokens  = burst;
        last_ts = now;
    } else {
        delta = now - last_ts;
        add   = delta * rate / 1000;
        print_str("add=");
        print(add);
        print_str("\n");
        tokens = tokens + add;
        if (tokens > burst)
            tokens = burst;
        last_ts = now;
    }

    print_str("tokens2=");
    print(tokens);
    print_str("\n");

    if (tokens <= 1000) {
        print_str("[DROP] sport=");
        print(sport);
        print_str(" dport=");
        print(dport);
        print_str(" len=");
        print(len);
        print_str("\n");

        map_update(0, sport, tokens);
        map_update(1, sport, last_ts);

        return 0;
    }

    tokens = tokens - len;

    map_update(0, sport, tokens);
    map_update(1, sport, last_ts);

    print_str("[PASS] sport=");
    print(sport);
    print_str(" dport=");
    print(dport);
    print_str(" len=");
    print(len);
    print_str(" tokens=");
    print(tokens);
    print_str("\n");

    return sport + dport;
}
