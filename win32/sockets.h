#ifndef __SOCKETS_H__
#define __SOCKETS_H__

#if NEVER /* don't need for WIN32  */

#include "opt.h"
#include "types.h"
#include "acn_arch.h"

/* members are in network byte order */
struct sockaddr_in {
  uint16_t sin_port;
  uint32_t sin_addr;
  char sin_zero[8];
};

#endif /* NEVER */

#endif /* __SOCKETS_H__ */
