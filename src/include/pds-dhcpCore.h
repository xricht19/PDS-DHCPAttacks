#ifndef _PDSDHCPCORE_H
#define _PDSDHCPCORE_H

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <sys/types.h>
#include <ifaddrs.h>
#include <arpa/inet.h>

#include <string>
#include <cstring>
#include <cstdio>
#include <cstdlib>


class DHCPCore
{
public:
	DHCPCore();
	~DHCPCore();

	// DHCP messages
	/*bool createDHCPDiscoverMessage();
	bool createDHCPOfferMessage();
	bool createDHCPRequestMessage();
	bool createDHCPPackMessage();
	*/
	// get device IP address
	void getDeviceIPAddressNetMask(std::string deviceName);

private:
	// create dhcp discover message with spoffed MAC adrress
	// create dhcp request message

	// device info
	struct ifaddrs* _deviceInfo;
	// device IP adress
	struct in_addr* _deviceIP;
	// net mask
	struct in_addr* _netMask;
};


#endif
