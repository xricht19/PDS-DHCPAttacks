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

//#define _DEBUG_

// DHCP Message settings
#define DHCPCORE_CHADDR_LENGTH 16
#define DHCPCORE_BROADCAST_FLAG 32768
#define MIN_DHCP_PACKET_SIZE 300
#define MAX_DHCP_PACKET_SIZE 576
#define DHCPCORE_OPTION_LENGTH 298 //bytes -> MAX_DHCP_PACKET_SIZE(=576) - (DHCPHeaderLength(=236) + Other HEADERS(42))
#define DHCPCORE_HTYPE 1
#define DHCPCORE_HLEN 6
#define DHCPCORE_HOPS 0

#ifndef DHCP_MSG_TYPES
	#define DHCP_MSG_TYPES
	#define DHCP_TYPE_DISCOVER 1
	#define DHCP_TYPE_OFFER 2
	#define DHCP_TYPE_REQUEST 3
	#define DHCP_TYPE_DECLINE 4
	#define DHCP_TYPE_ACK 5
	#define DHCP_TYPE_NACK 6
	#define DHCP_TYPE_RELEASE 7
	#define DHCP_TYPE_INFORM 8
#endif
// DHCP Discover settings
#define DHCPCORE_DISCOVER_OP 1 // BOOTREQUEST
#define DHCPCORE_DISCOVER_CLIENT_IP "0.0.0.0"
#define DHCPCORE_DISCOVER_YOUR_IP "0.0.0.0"
#define DHCPCORE_DISCOVER_SERVER_IP "0.0.0.0"
#define DHCPCORE_DISCOVER_GATEWAY_IP "0.0.0.0"

// DHCP Offer settings
#define DHCPCORE_OFFER_OP 2 // BOOTREPLY
#define DHCPCORE_OFFER_CLIENT_IP "0.0.0.0"

#define DHCPCORE_ACK_OP 2
#define DHCPCORE_NACK_OP 2

// DHCP options
#define DHCPCORE_OPT_SERVER_IDENTIFIER 54
#define DHCPCORE_OPT_REQUESTED_IP 50


#ifndef SERVER_SETTINGS_STRUCT
#define SERVER_SETTINGS_STRUCT 
struct serverSettings
{
	std::string interfaceName = "";
	struct in_addr poolFirst;
	struct in_addr poolLast;
	struct in_addr gateway;
	struct in_addr dnsServer;
	std::string domain;
	uint32_t leaseTime;
	struct in_addr serverIdentifier;
};
#endif


class DHCPCore
{
private:
	// varible flag for errors
	enum ERROROPTIONS
	{
		OK = 0,
		INET_ATON_ERROR,
		CANNOT_FIND_GIVEN_DEVICE_IP,
		NON_VALID_MESSAGE,
		INCORRECT_XID,
	};
	ERROROPTIONS _errorType;

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
	struct dhcp_packet* _dhcpMessageResponse;
	int _dhcpMessageResponseLength;
	uint32_t _dhcp_xid;
	uint32_t _offeredIPAddress;	// set in little endian

	int _state;
	int _currentMessageSize;

	// spoofed MAC address generator
	void genAndSetMACAddress();

	
public:
	DHCPCore(int threadNumber);
	~DHCPCore();

	// DHCP messages
	void createDHCPDiscoverMessage(); 
	void createDHCPOfferMessage(in_addr &ipAddrToOffer, serverSettings &serverSet);
	void createDHCPRequestMessage();
	void createDHCPAckMessage(in_addr &ipAddrToOffer, serverSettings &serverSet, bool broadCastBitSet = false);
	void createDHCPNackMessage(serverSettings &serverSet, bool broadCastBitSet = false);

	void ProcessDHCPDiscoverMessage(unsigned char* message, int &messageLength); 
	void ProcessDHCPOfferMessage(unsigned char* message, int &messageLength);
	void ProcessDHCPRequestMessage(unsigned char* message, int &messageLength);
	void ProcessDHCPAckMessage(unsigned char* message, int &messageLength);
	void ProcessDHCPDeclineMessage(unsigned char* message, int &messageLength);
	void ProcessDHCPReleaseMessage(unsigned char* message, int &messageLength);
	void ProcessDHCPInformMessage(unsigned char* message, int &messageLength);

	// function to print error
	bool isError();
	void printDHCPCoreError() const;

	// clean everything from dhcpCore, end the end or after error
	void cleanDHCPCore();
	// init dhcp core class to be able to process new message
	void initDHCPCore();

	unsigned char* getMessage();
	int getSizeOfMessage();

	uint32_t getCurrentXID(bool fromResponse = false);
	uint32_t getOfferedIPAddress() { return _offeredIPAddress; }
	in_addr getCurrentClientCiaddr() { return _dhcpMessageResponse->ciaddr; }
	in_addr getCurrentClientGiaddr() { return _dhcpMessageResponse->giaddr; }
	void GetCurrentClientMacAddr(unsigned char* macAddr, int length);

	// returns the value of given option number from response, work only if value can be stored in uint32_t (max 4 octets)
	void getOptionValue(const int optionNumber, uint32_t &value);

	void setState(int value) { _state = value; }
	int getState() { return _state; }
	// return device ip according to given interface name
	static in_addr getDeviceIP(std::string deviceName);
	// check if message is DHCP, contain magic cookie as first value in options
	static bool IsDHCPMessage(unsigned char* message, int &messageLength);
	// check if it's DHCP and return its type, or -1 if not dhcp 
	static int GetDHCPMessageType(unsigned char* message, int &messageLength);
	// return xid form dhcp message packet
	static int getXID(unsigned char* message, int &messageLength);

	static int DHCPHeaderSize() { return (4*sizeof(uint8_t) + sizeof(uint32_t) + 
		2*sizeof(uint16_t) + 4*sizeof(in_addr) + 64 + 128 + DHCPCORE_CHADDR_LENGTH); }
	static int MinDHCPPacketSize() { return (DHCPHeaderSize()+5); }
	static int MaxDHCPPacketSize() { return MAX_DHCP_PACKET_SIZE; }
};


#endif
