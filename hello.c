struct udp_hdr {
    unsigned short sport;
    unsigned short dport;
};

int hook(void *ctx)
{
    struct udp_hdr *uh;
    unsigned int sport; 
    unsigned int dport; 
    uh = (struct udp_hdr *)ctx;
    sport = ntohs(uh->sport);
    dport = ntohs(uh->dport);

    print("UDP ");
    print("sport=");
    print(sport);
    print(" dport=");
    print(dport);
    print("\n");

    return 0;
}
