#ifndef __user_opt_h__
#define __user_opt_h__ 1

//#define CONFIG_MARSHAL_INLINE 0

//#define BYTE_ORDER BIG_ENDIAN

#define CONFIG_STACK_NETBURNER 0
#define CONFIG_STACK_LWIP      0
#define CONFIG_STACK_BSD       0
#define CONFIG_STACK_WIN32     1

#define MAX_RLP_SOCKETS 2    /* need 2 for sdt */
//#define MAX_LISTENERS      /* need 2 for sdt plus one for each component
//                              that wants to join us: 20 bytes each */


#define MAX_TXBUFS   50

#define CONFIG_SLP   1
#define CONFIG_RLP   1
#define CONFIG_SDT   1
#define CONFIG_DMP   1

#define CONFIG_RLPMEM_MALLOC 0
#define CONFIG_RLPMEM_STATIC 1

// see everything
#define CONFIG_LOGLEVEL LOG_DEBUG

#define SDT_MAX_COMPONENTS          10
#define SDT_MAX_CHANNELS            10
#define SDT_MAX_MEMBERS             40

// but filter on these
#define LOG_RLP    LOG_NONE
#define LOG_RLPM   LOG_NONE
#define LOG_SDT    LOG_NONE
#define LOG_SDTM   LOG_NONE
#define LOG_NSK    LOG_NONE
#define LOG_NETX   LOG_NONE
#define LOG_SLP    LOG_LOCAL0
#define LOG_DMP    LOG_NONE
#define LOG_DMPM   LOG_NONE
#define LOG_MISC   LOG_NONE
#define LOG_ASSERT LOG_NONE
#define LOG_STAT   LOG_LOCAL0


#define CONFIG_RLP_SINGLE_CLIENT 1 //PROTO_SDT

//#define CONFIG_ACNLOG ACNLOG_SYSLOG
//#define CONFIG_LOCALIP_ANY       0

#endif
