#ifndef __IP_ADDR_H__
#define __IP_ADDR_H__

#if CONFIG_STACK_NETBURNER /* don't build if not configured for use  */

//#ifdef __cplusplus
//extern "C" {
//#endif

struct ip_addr {
  unsigned long addr;
};

//#ifdef __cplusplus
//}
//#endif

#endif /* CONFIG_STACK_NETBURNER */

#endif /* __NETBURNER_IP_ADDR_H__ */
