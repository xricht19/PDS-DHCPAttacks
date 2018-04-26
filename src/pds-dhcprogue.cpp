#include "include/pds-dhcprogue.h"

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
struct addressPool
{
	struct in_addr ipAddress;
	unsigned char chaddr[CHADDR_LENGTH];
	uint32_t leaseTime;					// if set to 0 and !isFree, do not free this address, it's blocked by decline
	std::chrono::time_point<std::chrono::system_clock, std::chrono::system_clock::duration> timeOfRent;
	bool isFree;
};

class negotiatedClient
{
public:
	negotiatedClient(unsigned char* addr, uint32_t xidIn, int addrLength = CHADDR_LENGTH) : 
		chaddr(addr), xid(xidIn), chaddrLength(addrLength){}

	int chaddrLength;
	unsigned char* chaddr;
	uint32_t xid;
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

in_addr GetFreeIPAddressToOffer(std::vector<addressPool*> &addrPool)
{
	struct in_addr chosenAddr;
	chosenAddr.s_addr = 0;
	for(std::vector<addressPool*>::iterator it = addrPool.begin(); it != addrPool.end(); ++it)
	{
		if((*it)->isFree)
		{
			(*it)->isFree = false;
			(*it)->leaseTime = BLOCK_ADDRESS_AFTER_DISCOVER_TIME;
			(*it)->timeOfRent = std::chrono::system_clock::now();
			chosenAddr = (*it)->ipAddress;
			break;
		}
	}
	return chosenAddr;
}


void preProcessDHCPRequest(unsigned char* message, int messageLength, std::vector<negotiatedClient*> &dealInProgress)
{
	fprintf(stdout, "preProcessing DHCPRequest.\n");
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
	// prepare free addresses structure
	uint32_t addrCount = ntohl(serverSet.poolLast.s_addr) - ntohl(serverSet.poolFirst.s_addr) + 1;
	// address pool
	std::vector<addressPool*> addrPool;
	// fill address pool
	for(uint32_t i = 0; i < addrCount; ++i)
	{
		struct addressPool *temp = new addressPool();
		temp->ipAddress.s_addr = serverSet.poolFirst.s_addr + htonl(i);
		std::memset(&temp->chaddr, 0, CHADDR_LENGTH);
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
	struct sockaddr_in clientSettings;
	memset(&clientSettings, 0, sizeof(clientSettings));
	clientSettings.sin_family = AF_INET;
	clientSettings.sin_port = htons(DHCP_CLIENT_PORT);
	// set IPv4 broadcast
	if ((inet_pton(AF_INET, DHCP_CLIENT_ADDRESS, &clientSettings.sin_addr)) <= 0)
	{
		fprintf(stderr, "Cannot set server address (%s) -> inet_pton error.\n", DHCP_SERVER_ADDRESS);
		exit(1);
	}

	/*for(std::vector<addressPool*>::iterator it = addrPool.begin(); it != addrPool.end(); ++it)
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

	// start process messages from clients
	while(true)
	{		
		if(sigIntCatched)
		{
			break;
		}
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
			std::cout << "Type: " << type << std::endl;
			bool success = true;
			// switch dhcp types and generate response
			switch(type)
			{
				case DHCP_TYPE_DISCOVER:
					{
						// create DHCP core class
						DHCPCore* dhcpCoreInstance = new DHCPCore(1);
						dhcpCoreInstance->ProcessDHCPDiscoverMessage(message, receivedSize);
						in_addr ipAddrTemp = GetFreeIPAddressToOffer(addrPool);
						// is not free address, send DHCPNack
						if(ipAddrTemp.s_addr == 0)
						{
							dhcpCoreInstance->createDHCPNAckMessage();
						}
						// send DHCPOffer
						else
						{
							dhcpCoreInstance->createDHCPOfferMessage(ipAddrTemp, serverSet);
							if(dhcpCoreInstance->isError())
							{
								fprintf(stderr, "DHCP_TYPE_DISCOVER -> Error in create DHCPOfferMessage!\n");
								success = false;
								break;
							}
							// save the xid and MAC identifier to dealInProgress
							unsigned char macAddr[CHADDR_LENGTH];
							dhcpCoreInstance->GetCurrentClientMacAddr(&macAddr[0], CHADDR_LENGTH);
							negotiatedClient* item = new negotiatedClient(macAddr, dhcpCoreInstance->getCurrentXID());
							dealInProgress.push_back(item);
							// send message
						}
						sendto(senderSocket, dhcpCoreInstance->getMessage(), dhcpCoreInstance->getSizeOfMessage(), 0, (sockaddr*)&clientSettings, sizeof(sockaddr));
						delete(dhcpCoreInstance);
					}
					break;
				case DHCP_TYPE_REQUEST:
					preProcessDHCPRequest(message, receivedSize, dealInProgress);
					// check if MAC and xid combination is known, if not, check if the mac address and ip is already assigned, extend lease time
					// process dhcp request

					// save response to client by broadcast
					break;
				case DHCP_TYPE_DECLINE:
					// client refuse, because somebody is using this message, no answer only mark IP not to use
					break;
				case DHCP_TYPE_RELEASE:
					// check if server id is yours and release address
					break;
				case DHCP_TYPE_INFORM:
					// send response by direct message!
					// send DHCPAck response with all info
					break;
			}
			if(!success)
				break;
		}
		// check if lease time of some addresses do not end, set them free if yes
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
	for(std::vector<addressPool*>::iterator it = addrPool.begin(); it != addrPool.end(); ++it)
	{
		delete(*it);
	}
	addrPool.clear();
}