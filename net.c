// Copyright (c) 2012-2020 YAMAMOTO Masaya
// SPDX-License-Identifier: MIT

#include "types.h"
#include "defs.h"
#include "net.h"
#include "ip.h"

struct netproto {
    struct netproto *next;
    uint16_t type;
    void (*handler)(uint8_t *packet, size_t plen, struct netdev *dev);
};

static struct netdev *devices;
static struct netproto *protocols;

struct netdev *
netdev_root(void)
{
    return devices;
}

struct netdev *
netdev_alloc(void (*setup)(struct netdev *))
{
    struct netdev *dev;
    static unsigned int index = 0;

    dev = (struct netdev *)kalloc();
    if (!dev) {
        return NULL;
    }
    memset(dev, 0, sizeof(struct netdev));
    snprintf(dev->name, sizeof(dev->name), "net%d", index++);
    setup(dev);
    return dev;
}

int
netdev_register(struct netdev *dev)
{
    cprintf("[net] netdev_register: <%s>\n", dev->name);
    dev->next = devices;
    devices = dev;
    return 0;
}

void
netdev_receive(struct netdev *dev, uint16_t type, uint8_t *packet, unsigned int plen)
{
    struct netproto *entry;
#ifdef DEBUG
    cprintf("[net] netdev_receive: dev=%s, type=%04x, packet=%p, plen=%u\n", dev->name, type, packet, plen);
#endif
    for (entry = protocols; entry; entry = entry->next) {
        if (hton16(entry->type) == type) {
            entry->handler(packet, plen, dev);
            return;
        }
    }
}

int
netdev_add_netif(struct netdev *dev, struct netif *netif)
{
    struct netif *entry;

    for (entry = dev->ifs; entry; entry = entry->next) {
        if (entry->family == netif->family) {
            return -1;
        }
    }
    if (netif->family == NETIF_FAMILY_IPV4) {
        char addr[IP_ADDR_STR_LEN];
        cprintf("[net] Add <%s> to <%s>\n", ip_addr_ntop(&((struct netif_ip *)netif)->unicast, addr, sizeof(addr)), dev->name);
    }
    netif->next = dev->ifs;
    netif->dev  = dev;
    dev->ifs = netif;
    return 0;
}

struct netif *
netdev_get_netif(struct netdev *dev, int family)
{
    struct netif *entry;

    for (entry = dev->ifs; entry; entry = entry->next) {
        if (entry->family == family) {
            return entry;
        }
    }
    return NULL;
}

int
netproto_register(unsigned short type, void (*handler)(uint8_t *packet, size_t plen, struct netdev *dev))
{
    struct netproto *entry;

    for (entry = protocols; entry; entry = entry->next) {
        if (entry->type == type) {
            return -1;
        }
    }
    entry = (struct netproto *)kalloc();
    if (!entry) {
        return -1;
    }
    entry->next = protocols;
    entry->type = type;
    entry->handler = handler;
    protocols = entry;
    return 0;
}

void
netinit(void)
{
    arp_init();
    ip_init();
    icmp_init();

    // dummy setting
    for (struct netdev *dev = devices; dev; dev = dev->next) {
        if (strncmp(dev->name, "net0", 4) == 0)
            ip_netif_register(dev, "10.0.2.15", "255.255.255.0", NULL);
        if (strncmp(dev->name, "net1", 4) == 0)
            ip_netif_register(dev, "192.168.100.10", "255.255.255.0", NULL);
    }
}
