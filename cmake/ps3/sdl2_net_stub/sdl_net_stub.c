/*
 * Stub SDL2_net implementation for the PS3 build. See SDL_net.h for the
 * rationale. All entrypoints fail or no-op so that callers see networking
 * as unavailable rather than crashing.
 */

#include "SDL_net.h"
#include <string.h>

static char net_error_buf[256] = "";

static void NET_SetError(const char *msg) {
    strncpy(net_error_buf, msg, sizeof(net_error_buf) - 1);
    net_error_buf[sizeof(net_error_buf) - 1] = '\0';
}

/* PS3 newlib doesn't expose gethostname; provide a trivial stub so the
 * link succeeds. Networking is no-op'd by the rest of this stub. */
int gethostname(char *name, size_t len) {
    if (name && len > 0) {
        strncpy(name, "ps3", len);
        name[len - 1] = '\0';
    }
    return 0;
}

int SDLNet_Init(void) {
    NET_SetError("SDL2_net stub: networking not available on PS3");
    return -1;
}

void SDLNet_Quit(void) {}

const char *SDLNet_GetError(void) {
    return net_error_buf;
}

int SDLNet_ResolveHost(IPaddress *address, const char *host, Uint16 port) {
    (void)host; (void)port;
    if (address) {
        address->host = 0;
        address->port = 0;
    }
    NET_SetError("SDL2_net stub: networking not available on PS3");
    return -1;
}

const char *SDLNet_ResolveIP(const IPaddress *ip) {
    (void)ip;
    return NULL;
}

UDPsocket SDLNet_UDP_Open(Uint16 port) {
    (void)port;
    NET_SetError("SDL2_net stub: networking not available on PS3");
    return NULL;
}

void SDLNet_UDP_Close(UDPsocket sock) {
    (void)sock;
}

int SDLNet_UDP_Bind(UDPsocket sock, int channel, const IPaddress *address) {
    (void)sock; (void)channel; (void)address;
    return -1;
}

void SDLNet_UDP_Unbind(UDPsocket sock, int channel) {
    (void)sock; (void)channel;
}

IPaddress *SDLNet_UDP_GetPeerAddress(UDPsocket sock, int channel) {
    (void)sock; (void)channel;
    return NULL;
}

int SDLNet_UDP_Send(UDPsocket sock, int channel, UDPpacket *packet) {
    (void)sock; (void)channel; (void)packet;
    return 0;
}

int SDLNet_UDP_Recv(UDPsocket sock, UDPpacket *packet) {
    (void)sock; (void)packet;
    return 0;
}

UDPpacket *SDLNet_AllocPacket(int size) {
    (void)size;
    return NULL;
}

void SDLNet_FreePacket(UDPpacket *packet) {
    (void)packet;
}

UDPpacket **SDLNet_AllocPacketV(int howmany, int size) {
    (void)howmany; (void)size;
    return NULL;
}

void SDLNet_FreePacketV(UDPpacket **packets) {
    (void)packets;
}

int SDLNet_GetLocalAddresses(IPaddress *addresses, int maxcount) {
    (void)addresses; (void)maxcount;
    return 0;
}
