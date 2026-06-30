#ifndef SDL_NET_H
#define SDL_NET_H

/*
 * Minimal SDL2_net-compatible header for the PS3 build.
 *
 * The ps3dev image ships SDL2 + SDL2_mixer etc. but not SDL2_net, and
 * SDL1's SDL_net is ABI-incompatible with SDL2. Networking is therefore
 * not supported in this PS3 port. This header declares just enough of the
 * API that chocolate-quake's net module compiles; the corresponding .c
 * file provides no-op stubs so the link succeeds and the binary loads,
 * with the network driver reporting failure at runtime as expected by
 * the rest of the engine.
 */
//#include <SDL.h>
#include <stdint.h>
#include <stddef.h>

typedef uint32_t Uint32;
typedef uint16_t Uint16;
typedef uint8_t  Uint8;

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    Uint32 host;
    Uint16 port;
} IPaddress;

typedef struct _UDPsocket *UDPsocket;

typedef struct {
    int channel;
    Uint8 *data;
    int len;
    int maxlen;
    int status;
    IPaddress address;
} UDPpacket;

#define INADDR_ANY      0x00000000
#define INADDR_BROADCAST 0xFFFFFFFF
#define INADDR_LOOPBACK 0x7F000001   /* 127.0.0.1 */
#define INADDR_NONE     0xFFFFFFFF

/* net_udp.c calls gethostname() directly. The PS3 newlib doesn't always
 * pull this in via SDL_net.h; declare it so the call is not implicit. */
extern int gethostname(char *name, size_t len);

#define SDLNET_MAX_UDPCHANNELS 1
#define SDLNET_MAX_UDPADDRESSES 1

int  SDLNet_Init(void);
void SDLNet_Quit(void);
const char *SDLNet_GetError(void);

int SDLNet_ResolveHost(IPaddress *address, const char *host, Uint16 port);
const char *SDLNet_ResolveIP(const IPaddress *ip);

UDPsocket SDLNet_UDP_Open(Uint16 port);
void SDLNet_UDP_Close(UDPsocket sock);

int SDLNet_UDP_Bind(UDPsocket sock, int channel, const IPaddress *address);
void SDLNet_UDP_Unbind(UDPsocket sock, int channel);
IPaddress *SDLNet_UDP_GetPeerAddress(UDPsocket sock, int channel);

int SDLNet_UDP_Send(UDPsocket sock, int channel, UDPpacket *packet);
int SDLNet_UDP_Recv(UDPsocket sock, UDPpacket *packet);

UDPpacket *SDLNet_AllocPacket(int size);
void SDLNet_FreePacket(UDPpacket *packet);
UDPpacket **SDLNet_AllocPacketV(int howmany, int size);
void SDLNet_FreePacketV(UDPpacket **packets);

int SDLNet_GetLocalAddresses(IPaddress *addresses, int maxcount);

/* Inline byte-order helpers (match SDL2_net semantics). */
static inline Uint32 SDLNet_Read32(const void *area) {
    const Uint8 *p = (const Uint8 *)area;
    return ((Uint32)p[0] << 24) | ((Uint32)p[1] << 16)
         | ((Uint32)p[2] << 8)  |  (Uint32)p[3];
}
static inline void SDLNet_Write32(Uint32 value, void *area) {
    Uint8 *p = (Uint8 *)area;
    p[0] = (Uint8)(value >> 24);
    p[1] = (Uint8)(value >> 16);
    p[2] = (Uint8)(value >> 8);
    p[3] = (Uint8)(value);
}
static inline Uint16 SDLNet_Read16(const void *area) {
    const Uint8 *p = (const Uint8 *)area;
    return (Uint16)(((Uint16)p[0] << 8) | (Uint16)p[1]);
}
static inline void SDLNet_Write16(Uint16 value, void *area) {
    Uint8 *p = (Uint8 *)area;
    p[0] = (Uint8)(value >> 8);
    p[1] = (Uint8)(value);
}

#ifdef __cplusplus
}
#endif

#endif /* SDL_NET_H */
