#include "include/pds-dhcprogue.h"

// global variables
int volatile sigIntCatched = false;

struct serverSettings
{
	std::string interfaceName = "";
	struct in_addr poolFirst;
	struct in_addr poolLast;
	struct in_addr gateway;
	struct in_addr dnsServer;
	std::string domain;
	int leaseTime = -1;
};

struct addressPool
{
	struct in_addr ipAddress;
	unsigned char chaddr[CHADDR_LENGTH];
	uint32_t leaseTime;
	std::chrono::time_point<std::chrono::system_clock, std::chrono::system_clock::duration> timeOfRent;
	bool isFree;
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
	// fill server settings from parameters strings
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
	serverSet.leaseTime = std::stoi(leaseTime);
	if(serverSet.leaseTime < 0) {
		fprintf(stderr, "Cannot convert string:(%s) to int -> std::stoi error.\n", leaseTime.c_str());
		return INCORRECT_PARAMS;
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

	// prepare free addresses structure
	uint32_t addrCount = ntohl(serverSet.poolLast.s_addr) - ntohl(serverSet.poolFirst.s_addr) + 1;
	//fprintf(stderr, "Free addresses: %d\n", addrCount);
	std::vector<addressPool*> addrPool;
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
	char message[ETHERNET_MTU];
	int messageLength = ETHERNET_MTU;
	// start process messages from clients
	while(true)
	{		
		if(sigIntCatched)
		{
			break;
		}
		std::memset(&message, 0, sizeof(message));
		int receivedSize = recvfrom(senderSocket, message, messageLength, MSG_DONTWAIT, (sockaddr *)&si_other, &slen);
		if(receivedSize > 0)
		{
			fprintf(stdout, "Some message obtained. Length: %d\n", receivedSize);
			// check if DHCP

			// get DHCP type

		}

	}


	// ---------------------------------------- CLEAN -----------------------------------------------------
	// close socket
	close(senderSocket);
	// delete address pool
	for(std::vector<addressPool*>::iterator it = addrPool.begin(); it != addrPool.end(); ++it)
	{
		delete(*it);
	}
	addrPool.clear();
}