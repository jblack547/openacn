#ifndef __IP_ADDR_H__
#define __IP_ADDR_H__

#if CONFIG_WIN32 /* don't build if not configured for use  */

//#ifdef __cplusplus
//extern "C" {
//#endif

struct ip_addr {
  unsigned long addr;
};

//#ifdef __cplusplus
//}
//#endif

#endif /* CONFIG_WIN32 */

#endif /* __IP_ADDR_H__ */
