struct udp_hdr {
    int sport;
    int dport;
};

int hook(void *ctx, char *pkt) {
    int x;
    x = ntohl(pkt[34]);
    print(x);
    return x;
}
