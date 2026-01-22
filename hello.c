struct udp_hdr {
    int sport;
    int dport;
};

int hook(void *ctx, char *pkt)
{
    int x;
    int y;
    struct udp_hdr *uh;

    x = ntohl(pkt[34]);
    print(x);

    uh = (struct udp_hdr *)&pkt[34];
    y = uh->dport;
    print(y);
    return x + y;
}
