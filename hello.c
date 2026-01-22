struct udp_hdr {
    int sport;
    int dport;
};

int hook(void *ctx, char *pkt) {
    return ((struct udp_hdr *)&pkt[34])->sport;
}
