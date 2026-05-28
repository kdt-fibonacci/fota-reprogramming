#include "LwIP_Cfg.h"
#include "LwIP.h"

eth_addr_t EthAddr =
{
    .addr =
    {
        LWIP_MAC_ADDR_0,
        LWIP_MAC_ADDR_1,
        LWIP_MAC_ADDR_2,
        LWIP_MAC_ADDR_3,
        LWIP_MAC_ADDR_4,
        LWIP_MAC_ADDR_5
    }
};