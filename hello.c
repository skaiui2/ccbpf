
struct udp_hdr {
    int sport;
    int dport;
};

int hook(void *ctx, char *pkt) {
    struct udp_hdr *uh;
    uh =(struct udp_hdr *)&pkt[34];
    return 1;
}
