#include "pds-dhcprogue.h"

// global variables
int volatile sigIntCatched = false;

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
struct addressPoolItem
{
	struct in_addr ipAddress;
	unsigned char chaddr[DHCPCORE_CHADDR_LENGTH];
	uint32_t leaseTime;		
	std::chrono::time_point<std::chrono::system_clock, std::chrono::system_clock::duration> timeOfRent;
	bool isFree;
};

class negotiatedClient
{
public:
	negotiatedClient(unsigned char* addr, uint32_t xidIn, addressPoolItem* addressPoolPointer, int addrLength = DHCPCORE_CHADDR_LENGTH) : 
		xid(xidIn), addrOffered(addressPoolPointer), chaddrLength(addrLength)
	{
		std::memcpy(chaddr, addr, DHCPCORE_CHADDR_LENGTH);
	}

	int chaddrLength;
	unsigned char chaddr[DHCPCORE_CHADDR_LENGTH];
	uint32_t xid;
	addressPoolItem* addrOffered;
};

int setPoolInfo(struct serverSettings &serverSet, std::string pool)
{
	size_t dashPos = pool.find("-");
	if(dashPos == pool.npos)
	{
		fprintf(stderr, "Cannot find dash in pool info!\n");
		return -1;
	}
	std::string first = pool.substr(0, dashPos);
	std::string last = pool.substr(dashPos+1);
	if ((inet_pton(AF_INET, first.c_str(), &serverSet.poolFirst)) <= 0)
	{
		fprintf(stderr, "Cannot convert string:(%s) to address -> inet_pton error.\n", first.c_str());
		return -1;
	}
	if ((inet_pton(AF_INET, last.c_str(), &serverSet.poolLast)) <= 0)
	{
		fprintf(stderr, "Cannot convert string:(%s) to address -> inet_pton error.\n", last.c_str());
		return -1;
	}
	return 0;
}

addressPoolItem* GetFreeIPAddressToOffer(std::vector<addressPoolItem*> &addrPool, uint32_t requestedIPAddress, unsigned char* chaddrClient)
{
	addressPoolItem* chosenAddr  = nullptr;
	bool assigned = false;
	for(std::vector<addressPoolItem*>::iterator it = addrPool.begin(); it != addrPool.end(); ++it)
	{
		// get first free address
		if(!assigned && (*it)->isFree)
		{
			std::memcpy(&((*it)->chaddr)[0], chaddrClient, DHCPCORE_CHADDR_LENGTH);
			(*it)->isFree = false;
			(*it)->leaseTime = BLOCK_ADDRESS_AFTER_DISCOVER_TIME;
			(*it)->timeOfRent = std::chrono::system_clock::now();
			chosenAddr = (*it);
			assigned = true;
		}
		// get requested address by client if free
		if((*it)->ipAddress.s_addr == requestedIPAddress && (*it)->isFree)
		{
			if(assigned)
			{
				chosenAddr->leaseTime = 0;
				chosenAddr->isFree = true;
			}
			std::memcpy(&((*it)->chaddr)[0], chaddrClient, DHCPCORE_CHADDR_LENGTH);
			(*it)->isFree = false;
			(*it)->leaseTime = BLOCK_ADDRESS_AFTER_DISCOVER_TIME;
			(*it)->timeOfRent = std::chrono::system_clock::now();
			chosenAddr = (*it);
			assigned = true;
		}
		// get the address the client already had in past if it's free
		if(std::memcmp((*it)->chaddr, chaddrClient, DHCPCORE_CHADDR_LENGTH) == 0)
		{
			if(assigned)
			{
				chosenAddr->leaseTime = 0;
				chosenAddr->isFree = true;
			}
			std::memcpy(&((*it)->chaddr)[0], chaddrClient, DHCPCORE_CHADDR_LENGTH);
			(*it)->isFree = false;
			(*it)->leaseTime = BLOCK_ADDRESS_AFTER_DISCOVER_TIME;
			(*it)->timeOfRent = std::chrono::system_clock::now();
			chosenAddr = (*it);
			assigned = true;
			break;
		}
	}
	return chosenAddr;
}

addressPoolItem* GetItemFromAddressPoolByMAC(unsigned char* clientChaddr, std::vector<addressPoolItem*> &addrPool)
{
	addressPoolItem* item = nullptr;
	for(std::vector<addressPoolItem*>::iterator it = addrPool.begin(); it != addrPool.end(); ++it)
	{
		if(std::memcmp((*it)->chaddr, clientChaddr, DHCPCORE_CHADDR_LENGTH) == 0)
		{
			item = (*it);
			break;
		}
	}
	return item;
}

// return true if ip is in pool
bool AddressInPool(uint32_t ip, serverSettings &serverSet)
{
	uint32_t first = ntohl(serverSet.poolFirst.s_addr);
	uint32_t last = ntohl(serverSet.poolLast.s_addr);
	uint32_t lookFor = ntohl(ip);
	if(lookFor >= first && lookFor <= last)
	{
		return true;
	}
	return false;
}

bool AddressPoolItemTimeLeaseElapsed(addressPoolItem* item)
{
	if(item->isFree)
		return true;
	else if(item->leaseTime == std::numeric_limits<uint32_t>::max())
	{
		return false;
	}
	else
	{
		// check if time of lease elapsed and free address
		std::chrono::time_point<std::chrono::system_clock, std::chrono::system_clock::duration> currentTime = 
			std::chrono::system_clock::now();
		auto difference = std::chrono::duration_cast<std::chrono::seconds>(currentTime - item->timeOfRent);
		//std::cout << "Time elapsed: " << difference.count() << std::endl;
		if(difference.count() > item->leaseTime)
		{
			item->isFree = true;
			item->leaseTime = 0;
			return true;
		}
	}
	return false;
}

/*
 * Process the DHCP Request message. 
 */
bool DHCPRequestProcessor(unsigned char* message, int messageLength, std::vector<negotiatedClient*> &dealInProgress, 
	DHCPCore* dhcpCoreInstance, serverSettings &serverSet, sockaddr_in &clientSettings, sockaddr_in &sendSettings,
	std::vector<addressPoolItem*> &addrPool)
{
	//fprintf(stdout, "preProcessing DHCPRequest.\n");
	// load response to dhcp core class
	dhcpCoreInstance->ProcessDHCPRequestMessage(message, messageLength);
	if(dhcpCoreInstance->isError()) 
	{
		fprintf(stderr, "DHCPRequestProcessor -> Error in process DHCPOfferMessage!\n");
		return false;
	}
	unsigned char chaddr[DHCPCORE_CHADDR_LENGTH];
	dhcpCoreInstance->GetCurrentClientMacAddr(&chaddr[0], DHCPCORE_CHADDR_LENGTH);
	// determine what the client want
	uint32_t requestContainServerID = 0;
	dhcpCoreInstance->getOptionValue(DHCPCORE_OPT_SERVER_IDENTIFIER, requestContainServerID);
	if(requestContainServerID != 0)
	{
		// response to DHCPOffer
		// find record in dealInProgress
		for(std::vector<negotiatedClient*>::iterator it = dealInProgress.begin(); it != dealInProgress.end(); ++it)
		{
			if(std::memcmp((*it)->chaddr, chaddr, DHCPCORE_CHADDR_LENGTH) == 0 && (*it)->xid == dhcpCoreInstance->getCurrentXID(true))
			{
				// send dhcp ack message - broadcast
				dhcpCoreInstance->createDHCPAckMessage((*it)->addrOffered->ipAddress, serverSet);
				if(dhcpCoreInstance->isError())
				{
					fprintf(stderr, "DHCPRequestProcessor -> Error in create DHCPAckMessage!\n");
					return false;
				}
				// mark address as used
				std::memcpy((*it)->addrOffered->chaddr, chaddr, DHCPCORE_CHADDR_LENGTH);
				(*it)->addrOffered->leaseTime = serverSet.leaseTime;
				(*it)->addrOffered->timeOfRent = std::chrono::system_clock::now();
				(*it)->addrOffered->isFree = false;
				// remove from dealInprogress
				it = dealInProgress.erase(it);
				fprintf(stdout, "Address: %s given.\n", inet_ntoa((*it)->addrOffered->ipAddress));
				return true;
			}
		}
		fprintf(stderr, "DHCPRequestProcessor -> Cannot find ip address temporary blocked for this client. Invalid DHCPRequest message.\n");	
	}
	else
	{
		// record of client must be in address pool (if for some reason it is not, remain silent)
		addressPoolItem* recordOfClient = GetItemFromAddressPoolByMAC(chaddr, addrPool);
		if(recordOfClient != nullptr)
		{
			// request to verify and extend lease time
			uint32_t optionRequestIPAddress = 0;
			dhcpCoreInstance->getOptionValue(50, optionRequestIPAddress);
			in_addr ciaddr = dhcpCoreInstance->getCurrentClientCiaddr();
			if(optionRequestIPAddress != 0 && ciaddr.s_addr == 0)
			{	// INIT-REBOOT STATE			
				// check if requested ip address is in pool and the mask is good (the mask is not set in server settings, skipping check of NETMASK).
				in_addr giaddr = dhcpCoreInstance->getCurrentClientGiaddr();
				if(AddressInPool(optionRequestIPAddress, serverSet))
				{	
					// extend lease time, and extend lease time from server settings
					recordOfClient->timeOfRent = std::chrono::system_clock::now();
					recordOfClient->leaseTime = serverSet.leaseTime;
					recordOfClient->isFree = false;
					if(giaddr.s_addr == 0) // send  255.255.255.255, broadcast bit 0
					{
						dhcpCoreInstance->createDHCPAckMessage(recordOfClient->ipAddress, serverSet);
						if(dhcpCoreInstance->isError())
						{
							fprintf(stderr, "DHCPRequestProcessor -> Error in create DHCPAckMessage!\n");
							return false;
						}
					}
					else	// send 255.255.255.255, broadcast bit 1
					{
						dhcpCoreInstance->createDHCPAckMessage(recordOfClient->ipAddress, serverSet, true);
						if(dhcpCoreInstance->isError())
						{
							fprintf(stderr, "DHCPRequestProcessor -> Error in create DHCPAckMessage!\n");
							return false;
						}
					}
				}
				else // send NACK
				{
					if(giaddr.s_addr == 0) // send  255.255.255.255, broadcast bit 0
					{
						dhcpCoreInstance->createDHCPNackMessage(serverSet);
						if(dhcpCoreInstance->isError())
						{
							fprintf(stderr, "DHCPRequestProcessor -> Error in create DHCPNackMessage!\n");
							return false;
						}
					}
					else	// send 255.255.255.255, broadcast bit 1
					{
						dhcpCoreInstance->createDHCPNackMessage(serverSet, true);
						if(dhcpCoreInstance->isError())
						{
							fprintf(stderr, "DHCPRequestProcessor -> Error in create DHCPNackMessage!\n");
							return false;
						}
					}
				}
			}
			else
			{	
				// extend lease time, and extend lease time from server settings
				recordOfClient->timeOfRent = std::chrono::system_clock::now();
				recordOfClient->leaseTime = serverSet.leaseTime;
				recordOfClient->isFree = false;
				// create DHCPAck
				dhcpCoreInstance->createDHCPAckMessage(recordOfClient->ipAddress, serverSet);
				// if RENEWING send by unicast to ciaddr
				if(!AddressPoolItemTimeLeaseElapsed(recordOfClient))
				{	
					sendSettings.sin_addr = ciaddr;
				}
			}
			return true;
		}
		else
		{
			fprintf(stderr, "Unknown client is trying to INIT-REBOOT/RENEWING/REBINDING. Ignoring.\n");
		}		
	}
	return false;
}


void sigIntSet(int s)
{
	if (s == SIGINT)
		sigIntCatched = true;
}

int main(int argc, char* argv[])
{
	// ctrl + c signal catching set
	struct sigaction sigIntHandler;
	sigIntHandler.sa_handler = sigIntSet;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGINT, &sigIntHandler, NULL);

	// parse the parameters using getopt
	std::string chosenInterface = "";
	std::string pool = "";
	std::string gateway = "";
	std::string dnsServer = "";
	std::string domain = "";
	std::string leaseTime = "";


	if (argc != 13) {
		fprintf(stderr, "Incorrect params, exiting program.\n");
		return INCORRECT_PARAMS;
	}

	bool errorFlag = false;
	int c;
	while ((c = getopt(argc, argv, "i:p:g:n:d:l:")) != -1) {
		switch (c) {
		case 'i':
			chosenInterface = optarg;
			break;
		case 'p':
			pool = optarg;
			break;
		case 'g':
			gateway = optarg;
			break;
		case 'n':
			dnsServer = optarg;
			break;
		case 'd':
			domain = optarg;
			break;
		case 'l':
			leaseTime = optarg;
			break;
		case ':':
			std::cerr << "Option -" << optopt << "requires an operand" << std::endl;
			errorFlag = true;
			break;
		case '?':
			std::cerr << "Unrecognized option: " << optopt << std::endl;
			errorFlag = true;
			break;
		}
	}

	if (errorFlag) {
		fprintf(stderr, "Incorrect params, exiting program.\n");
		return INCORRECT_PARAMS;
	}	
	// fill server settings from parameters strings -----------------------------------------------------------
	// struct serverSettings + address pool
	struct serverSettings serverSet;
	serverSet.interfaceName = chosenInterface;
	if(setPoolInfo(serverSet, pool) < 0) {
		fprintf(stderr, "Cannot get pool addresses from: %s\n", strerror(errno));
		return INET_PTON_ERROR;
	}
	if ((inet_pton(AF_INET, gateway.c_str(), &serverSet.gateway)) <= 0) {
		fprintf(stderr, "Cannot convert string:(%s) to address -> inet_pton error.\n", gateway.c_str());
		return INET_PTON_ERROR;
	}
	if ((inet_pton(AF_INET, dnsServer.c_str(), &serverSet.dnsServer)) <= 0) {
		fprintf(stderr, "Cannot convert string:(%s) to address -> inet_pton error.\n", dnsServer.c_str());
		return INET_PTON_ERROR;
	}
	serverSet.domain = domain;
	serverSet.leaseTime = static_cast<uint32_t>(std::stoi(leaseTime));
	if(serverSet.leaseTime < 0) {
		fprintf(stderr, "Cannot convert string:(%s) to int -> std::stoi error.\n", leaseTime.c_str());
		return INCORRECT_PARAMS;
	}
	// fill server identifier
	serverSet.serverIdentifier = DHCPCore::getDeviceIP(chosenInterface);
	if(serverSet.serverIdentifier.s_addr == 0)
	{
		fprintf(stderr, "Cannot get IP address of server interface: %s\n", chosenInterface.c_str());
		exit(1);
	}
	// prepare free addresses structure
	uint32_t addrCount = ntohl(serverSet.poolLast.s_addr) - ntohl(serverSet.poolFirst.s_addr) + 1;
	// address pool
	std::vector<addressPoolItem*> addrPool;
	// fill address pool
	for(uint32_t i = 0; i < addrCount; ++i)
	{
		struct addressPoolItem *temp = new addressPoolItem();
		temp->ipAddress.s_addr = serverSet.poolFirst.s_addr + htonl(i);
		std::memset(&temp->chaddr, 0, DHCPCORE_CHADDR_LENGTH);
		temp->leaseTime = 0;
		temp->timeOfRent = std::chrono::system_clock::now();
		temp->isFree = true;
		addrPool.push_back(temp);
	}

	// --------------------------- CREATE SOCKET FOR COMMUNICATION --------------------------------------
	int senderSocket = 0;
	if ((senderSocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		fprintf(stderr, "Cannot create socket.");
		exit(1);
	}

	// bind socket to given interface
	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	memcpy(&ifr.ifr_name, chosenInterface.c_str(), sizeof(ifr.ifr_name));
	if(setsockopt(senderSocket, SOL_SOCKET, SO_BINDTODEVICE, (void*)&ifr, sizeof(ifr)) < 0)
	{
		fprintf(stderr, "Cannot assign socket to interface: %s. %s\n", chosenInterface.c_str(), strerror(errno));
		exit(1);
	}
	// enable broadcast on socket
	int optVal = 1;
	if(setsockopt(senderSocket, SOL_SOCKET, SO_BROADCAST, &optVal, sizeof(optVal)) < 0)
	{
		fprintf(stderr, "Cannot set socket to broadcast: %s. %s\n", chosenInterface.c_str(), strerror(errno));
		exit(1);
	}
	if(setsockopt(senderSocket, SOL_SOCKET, SO_REUSEADDR, &optVal, sizeof(optVal)) < 0)
	{
		fprintf(stderr, "Cannot set socket to broadcast: %s. %s\n", chosenInterface.c_str(), strerror(errno));
		exit(1);
	}	
	struct sockaddr_in interfaceSettings;
	// set parameter and bind to socket to specific interface
	interfaceSettings.sin_family = AF_INET;
	interfaceSettings.sin_port = htons(DHCP_SERVER_PORT);
	interfaceSettings.sin_addr.s_addr = INADDR_ANY;
	if (bind(senderSocket, (sockaddr*)&interfaceSettings, sizeof(interfaceSettings)) < 0)
	{ // cannot bind socket to given address and port
		fprintf(stderr, "Cannot bind socket to interface: %s. %s\n", chosenInterface.c_str(), strerror(errno));
		exit(1);
	}	
	// set parameters for server address and connect socket to it
	struct sockaddr_in clientSettings, sendSettings;
	memset(&clientSettings, 0, sizeof(clientSettings));
	memset(&clientSettings, 0, sizeof(sendSettings));
	clientSettings.sin_family = AF_INET;
	sendSettings.sin_family = AF_INET;
	clientSettings.sin_port = htons(DHCP_CLIENT_PORT);
	sendSettings.sin_port = htons(DHCP_CLIENT_PORT);
	// set IPv4 broadcast
	if ((inet_pton(AF_INET, DHCP_CLIENT_ADDRESS, &clientSettings.sin_addr)) <= 0)
	{
		fprintf(stderr, "Cannot set server address (%s) -> inet_pton error.\n", DHCP_SERVER_ADDRESS);
		exit(1);
	}
	sendSettings.sin_addr = clientSettings.sin_addr;

	/*for(std::vector<addressPoolItem*>::iterator it = addrPool.begin(); it != addrPool.end(); ++it)
	{
		fprintf(stdout, "Address: %s\n", inet_ntoa((*it)->ipAddress));
		fprintf(stdout, "MAC: %s\n", (*it)->chaddr);
		fprintf(stdout, "leaseTime: %d\n", (*it)->leaseTime);
		std::chrono::time_point<std::chrono::system_clock, std::chrono::system_clock::duration> temp;
		std::cout << "timePoint: " << std::chrono::duration_cast<std::chrono::milliseconds>((*it)->timeOfRent - temp).count() << std::endl;
	}*/

	// --------------------------------- START CATCHING PACKETS -------------------------------------------
	sockaddr_in si_other;
	unsigned slen = sizeof(sockaddr);
	uint8_t type = 0;
	unsigned char message[ETHERNET_MTU];
	int messageLength = ETHERNET_MTU;
	// deal in process
	std::vector<negotiatedClient*> dealInProgress;

	// index of address pool
	int addressPoolIndex = 0;
	// start process messages from clients
	while(true)
	{		
		if(sigIntCatched)
		{
			break;
		}
		// reset sending address
		sendSettings.sin_port = clientSettings.sin_port;
		sendSettings.sin_addr = clientSettings.sin_addr;
		std::memset(&message, 0, sizeof(message));
		int receivedSize = recvfrom(senderSocket, &message[0], messageLength, MSG_DONTWAIT, (sockaddr *)&si_other, &slen);
		if(receivedSize > 0)
		{
			// check if DHCP
			if(!DHCPCore::IsDHCPMessage(message, receivedSize))
			{
				fprintf(stderr, "No DHCP message on port 67! Length: %d\n", receivedSize);
				break;
			}
			// get DHCP type
			int type = DHCPCore::GetDHCPMessageType(message, receivedSize);
			if(type == -1)
			{
				fprintf(stderr, "Cannot find DHCP message type in packet!\n");
				break;
			}
			bool success = true;
			// switch dhcp types and generate response
			switch(type)
			{
				case DHCP_TYPE_DISCOVER:
					{
						// create DHCP core class
						DHCPCore* dhcpCoreInstance = new DHCPCore(1);
						dhcpCoreInstance->ProcessDHCPDiscoverMessage(message, receivedSize);
						if(dhcpCoreInstance->isError())
						{
							fprintf(stderr, "DHCP_TYPE_DISCOVER -> Error in create DHCPOfferMessage!\n");
							success = false;
							delete(dhcpCoreInstance);
							break;
						}
						// check if DISCOVER contain option 50 -> requested IP Address
						uint32_t requestedIPAddress = 0;
						dhcpCoreInstance->getOptionValue(50, requestedIPAddress);
						// get currect client MAC Address
						unsigned char chaddr[DHCPCORE_CHADDR_LENGTH];
						dhcpCoreInstance->GetCurrentClientMacAddr(&chaddr[0], DHCPCORE_CHADDR_LENGTH);
						// get ip according MAC or IP assigned to MAC in past which is free or some free IP address
						addressPoolItem* ipAddrTemp = GetFreeIPAddressToOffer(addrPool, requestedIPAddress, &chaddr[0]);
						if(ipAddrTemp != nullptr)
						{
							dhcpCoreInstance->createDHCPOfferMessage(ipAddrTemp->ipAddress, serverSet);
							if(dhcpCoreInstance->isError())
							{
								fprintf(stderr, "DHCP_TYPE_DISCOVER -> Error in create DHCPOfferMessage!\n");
								success = false;
								delete(dhcpCoreInstance);
								break;
							}
							// save the xid and MAC identifier to dealInProgress
							unsigned char macAddr[DHCPCORE_CHADDR_LENGTH];
							dhcpCoreInstance->GetCurrentClientMacAddr(&macAddr[0], DHCPCORE_CHADDR_LENGTH);
							negotiatedClient* item = new negotiatedClient(&macAddr[0], dhcpCoreInstance->getCurrentXID(), ipAddrTemp);
							dealInProgress.push_back(item);

							// send message
							sendto(senderSocket, dhcpCoreInstance->getMessage(), dhcpCoreInstance->getSizeOfMessage(), 0, (sockaddr*)&sendSettings, sizeof(sockaddr));
						}
						else
						{
							fprintf(stderr, "No free address on server.\n");
						}
						delete(dhcpCoreInstance);
					}
					break;
				case DHCP_TYPE_REQUEST:
					{
						DHCPCore* dhcpCoreInstance = new DHCPCore(1);
						bool shouldSend = DHCPRequestProcessor(message, receivedSize, dealInProgress, dhcpCoreInstance, serverSet, clientSettings, sendSettings,
											addrPool);
						// send message
						if(shouldSend)
						{
							sendto(senderSocket, dhcpCoreInstance->getMessage(), dhcpCoreInstance->getSizeOfMessage(), 0, (sockaddr*)&sendSettings, sizeof(sockaddr));
						}
						delete(dhcpCoreInstance);
					}
					break;
				case DHCP_TYPE_DECLINE:
					{
						DHCPCore* dhcpCoreInstance = new DHCPCore(1);
						dhcpCoreInstance->ProcessDHCPDeclineMessage(message, receivedSize);
						if(dhcpCoreInstance->isError())
						{
							fprintf(stderr, "DHCP_TYPE_DECLINE -> Error in create ProcessDHCPDeclineMessage!\n");
							success = false;
							delete(dhcpCoreInstance);
							break; 
						}
						uint32_t serverID = 0;
						dhcpCoreInstance->getOptionValue(DHCPCORE_OPT_SERVER_IDENTIFIER, serverID);
						if(serverID == serverSet.serverIdentifier.s_addr)
						{
							// block IP address in pool
							uint32_t ipAddrToBlock = 0;
							dhcpCoreInstance->getOptionValue(DHCPCORE_OPT_REQUESTED_IP, ipAddrToBlock);
							for(std::vector<addressPoolItem*>::iterator it = addrPool.begin(); it != addrPool.end(); ++it)
							{
								if((*it)->ipAddress.s_addr == ipAddrToBlock)
								{
									(*it)->leaseTime = std::numeric_limits<uint32_t>::max();
									(*it)->isFree = false;
									std::memset((*it)->chaddr, 0, DHCPCORE_CHADDR_LENGTH);
								}
							}
						}
						delete(dhcpCoreInstance);

					}
					// client refuse, because somebody is using this address, no answer only mark IP not to use
					break;
				case DHCP_TYPE_RELEASE:
					{
						DHCPCore* dhcpCoreInstance = new DHCPCore(1);
						dhcpCoreInstance->ProcessDHCPReleaseMessage(message, receivedSize);
						if(dhcpCoreInstance->isError())
						{
							fprintf(stderr, "DHCP_TYPE_DECLINE -> Error in create ProcessDHCPReleaseMessage!\n");
							success = false;
							delete(dhcpCoreInstance);
							break;
						}
						uint32_t serverID = 0;
						dhcpCoreInstance->getOptionValue(DHCPCORE_OPT_SERVER_IDENTIFIER, serverID);
						if(serverID == serverSet.serverIdentifier.s_addr)
						{
							// block IP address in pool
							in_addr ipAddrToBlock = dhcpCoreInstance->getCurrentClientCiaddr();
							for(std::vector<addressPoolItem*>::iterator it = addrPool.begin(); it != addrPool.end(); ++it)
							{
								if((*it)->ipAddress.s_addr == ipAddrToBlock.s_addr)
								{
									(*it)->leaseTime = 0;
									(*it)->isFree = true;
								}
							}
						}
						delete(dhcpCoreInstance);
					}
					break;
				case DHCP_TYPE_INFORM:
					// send response by direct message!
					// send DHCPAck response with all info
					{
						DHCPCore* dhcpCoreInstance = new DHCPCore(1);
						dhcpCoreInstance->ProcessDHCPInformMessage(message, receivedSize);
						if(dhcpCoreInstance->isError())
						{
							fprintf(stderr, "DHCP_TYPE_INFORM -> Error in create ProcessDHCPInformMessage!\n");
							success = false;
							delete(dhcpCoreInstance);
							break;
						}
						// no ip for this request
						in_addr clientIP = dhcpCoreInstance->getCurrentClientCiaddr();
						dhcpCoreInstance->createDHCPAckMessage(clientIP, serverSet);
						if(dhcpCoreInstance->isError())
						{
							fprintf(stderr, "DHCP_TYPE_INFORM -> Error in create createDHCPAckMessage!\n");
							success = false;
							delete(dhcpCoreInstance);
							break;
						}
						// send informations
						sendSettings.sin_addr = clientIP;
						sendto(senderSocket, dhcpCoreInstance->getMessage(), dhcpCoreInstance->getSizeOfMessage(), 0, (sockaddr*)&sendSettings, sizeof(sockaddr));
						delete(dhcpCoreInstance);
					}
					break; 
			}
			if(!success)
				break;
		}
		// check if lease time of some addresses do not end, set them free if yes
		// managing address pool -> relase if lease time over
		int counterLimit = ADDRESS_POOL_MANAGING_COUNT;
		if(ADDRESS_POOL_MANAGING_COUNT > addrPool.size())	// no need to check one address more than once in each round
			counterLimit = addrPool.size();
		for(int i = 0; i < counterLimit; ++i)
		{	
			addressPoolIndex = (++addressPoolIndex) % addrPool.size();
			addressPoolItem* item = addrPool.at(addressPoolIndex);
			AddressPoolItemTimeLeaseElapsed(item);
		}
	}


	// ---------------------------------------- CLEAN -----------------------------------------------------
	for(std::vector<negotiatedClient*>::iterator it = dealInProgress.begin(); it!=dealInProgress.end(); ++it)
	{
		delete(*it);
	}
	dealInProgress.clear();
	// close socket
	close(senderSocket);
	// delete address pool
	for(std::vector<addressPoolItem*>::iterator it = addrPool.begin(); it != addrPool.end(); ++it)
	{
		delete(*it);
	}
	addrPool.clear();
}