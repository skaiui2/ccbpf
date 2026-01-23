struct udp_hdr {
    unsigned short sport;
    unsigned short dport;
};

int hook(void *ctx, char *pkt)
{
    unsigned int x;
    unsigned int y;
    struct udp_hdr *uh;

    uh = (struct udp_hdr *)&pkt[34];
    x = ntohs(uh->sport);
    print(x);
    y = uh->dport;
    print(y);
    return x + y;
}
