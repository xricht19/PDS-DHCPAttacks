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
#include <iostream>
#include <ctime>

// DHCP Message settings
#define DHCPCORE_CHADDR_LENGTH 16
#define DHCPCORE_BROADCAST_FLAG 32768
#define DHCPCORE_OPTION_LENGTH 1220 //bytes -> MTU(=1500) - (DHCPLength(=236) + HEADERS(42))
#define DHCPCORE_HTYPE 1
#define DHCPCORE_HLEN 6
#define DHCPCORE_HOPS 0
// DHCP Discover settings
#define DHCPCORE_DISCOVER_OP 1 // BOOTREQUEST
#define DHCPCORE_DISCOVER_CLIENT_IP "0.0.0.0"
#define DHCPCORE_DISCOVER_YOUR_IP "0.0.0.0"
#define DHCPCORE_DISCOVER_SERVER_IP "0.0.0.0"
#define DHCPCORE_DISCOVER_GATEWAY_IP "0.0.0.0"
// DHCP Offer settings

class DHCPCore
{
public:
	DHCPCore();
	~DHCPCore();

	// DHCP messages
	void createDHCPDiscoverMessage();
	/*bool createDHCPOfferMessage();
	bool createDHCPRequestMessage();
	bool createDHCPPackMessage();

	void waitForAndProcessDHCPDiscover();*/
	void waitForAndProcessDHCPOffer();
	/*void waitForAndProcessDHCPRequest();
	void waitForAndProcessDHCPAck();
	*/
	// get device IP address
	void getDeviceIPAddressNetMask(std::string deviceName);

	// function to print error
	bool isError();
	void printDHCPCoreError() const;

	// clean everything from dhcpCore, end the end or after error
	void cleanDhcpCore();

	char* getMessage();
	int getSizeOfMessage();

private:
	// varible flag for errors
	enum ERROROPTIONS
	{
		OK = 0,
		INET_ATON_ERROR,
	};
	ERROROPTIONS _errorType;
	// device info
	struct ifaddrs _deviceInfo;
	// device IP adress
	struct in_addr _deviceIP;
	// net mask
	struct in_addr _netMask;

	// dhcp packet format according to RFC2131, figure 1
	struct dhcp_packet
	{
		uint8_t op;
		uint8_t htype;
		uint8_t hlen;
		uint8_t hops;
		uint32_t xid;
		uint16_t secs;
		uint16_t flags;
		struct in_addr ciaddr;
		struct in_addr yiaddr;
		struct in_addr siaddr;
		struct in_addr giaddr;
		unsigned char chaddr[DHCPCORE_CHADDR_LENGTH];
		char sname[64];
		char file[128];
		unsigned char options[DHCPCORE_OPTION_LENGTH];
	};

	// dhcp packet which is worked on
	struct dhcp_packet* _dhcpMessage;
	uint32_t _dhcp_xid;

	// spoofed MAC address generator
	void genAndSetMACAddress();
};


#endif