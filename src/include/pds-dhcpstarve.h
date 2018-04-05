#ifndef _PDSDHCPSTARVE_H
#define _PDSDHCPSTARVE_H

#include <unistd.h>
#include <netinet/udp.h>
#include <netinet/ip.h>
#include <string>
#include <stdint.h>

#include <iostream>

// general DHCP settings
#define DHCP_CLIENT_PORT 68
#define DHCP_SERVER_PORT 67
#define DHCP_SERVER_ADDRESS "255.255.255.255"
#define ETHERNET_MTU 1500

enum ERR_CODES {
	OK = 0,
	INCORRECT_PARAMS,

};



#endif
