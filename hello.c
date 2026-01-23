struct udp_hdr {
    unsigned short sport;
    unsigned short dport;
};

int hook(void *ctx)
{
    unsigned int x;
    unsigned int y;
    struct udp_hdr *uh;

    uh = (struct udp_hdr *)&ctx[0];
    x = ntohs(uh->sport);
    print(x);
    y = ntohs(uh->dport);
    print(y);
    return x + y;
}