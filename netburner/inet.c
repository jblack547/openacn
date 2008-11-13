#include "opt.h"
#include "types.h"
#include "acn_arch.h"

#if CONFIG_STACK_NETBURNER

#include "utils.h"  // Netburner file, has AsciiToIp()

#ifdef __cplusplus
extern "C" {
#endif

uint32_t inet_aton(const char *cp)
{
  /* convert ascii IP string to a single number */
  return AsciiToIp(cp);
}
#ifdef __cplusplus
}extern "C" {
#endif

#endif /* CONFIG_STACK_NETBURNER */

