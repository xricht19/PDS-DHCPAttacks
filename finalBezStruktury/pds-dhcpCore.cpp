#include "pds-dhcpCore.h"


DHCPCore::DHCPCore(int threadNumber) : _dhcpMessage(nullptr), _dhcpMessageResponse(nullptr), _dhcp_xid(0),
	_dhcpMessageResponseLength(0), _state(0), _offeredIPAddress(0)
{
	// init random generator
	srand(time(nullptr) * threadNumber);
	initDHCPCore();
}

DHCPCore::~DHCPCore()
{
	cleanDHCPCore();
}

void DHCPCore::cleanDHCPCore()
{
	// free dhcp message if exists
	if (_dhcpMessage != nullptr)
	{
		// TO-DO: THE DELETION OF SOME INNER PART MAY BE NEEDED.
		delete(_dhcpMessage);
	}
	if (_dhcpMessageResponse != nullptr)
	{
		delete(_dhcpMessageResponse);
	}
}

void DHCPCore::initDHCPCore()
{
	// free dhcp message if exists
	if (_dhcpMessage != nullptr)
	{
		// TO-DO: THE DELETION OF SOME INNER PART MAY BE NEEDED.
		delete(_dhcpMessage);
	}
	if (_dhcpMessageResponse != nullptr)
	{
		delete(_dhcpMessageResponse);
	}

	_dhcpMessage = new dhcp_packet();
	_dhcpMessageResponse = new dhcp_packet();
	_currentMessageSize = sizeof(dhcp_packet);

	_errorType = ERROROPTIONS::OK;
}

unsigned char* DHCPCore::getMessage()
{
	return (unsigned char*) (_dhcpMessage);
}

int DHCPCore::getSizeOfMessage()
{
	return _currentMessageSize;
}

void DHCPCore::genAndSetMACAddress()
{
	// randomly generate 6 octet and store it in uchar array, set rest to DHCP_CHADDR_SIZE to zero
	for (auto octetNumber = 0; octetNumber < DHCPCORE_HLEN; octetNumber++)
	{
		uint8_t oct = 0;
		if (octetNumber < DHCPCORE_HLEN)
		{
			oct = rand() % 255;
			// generate number 0-255
			_dhcpMessage->chaddr[octetNumber] = oct;
		}
		_dhcpMessage->chaddr[octetNumber] = oct;
	}
}

void DHCPCore::GetCurrentClientMacAddr(unsigned char* macAddr, int length)
{
	if(_dhcpMessageResponse == nullptr)
	{
		std::memset(macAddr, 0, length);
		return;
	}
	std::memcpy(macAddr, _dhcpMessageResponse->chaddr, length);
}

/**
 * \brief Function create dhcp discover packet and pointer to this structure is saved to _dhcpMessage variable.
 * \return True, if the packet was succesfully created. False otherwise.
 */
void DHCPCore::createDHCPDiscoverMessage()
{
	// init dhcp core to prepare it for new message
	//initDHCPCore();
	// start set values
	_dhcpMessage->op = DHCPCORE_DISCOVER_OP;
	_dhcpMessage->htype = DHCPCORE_HTYPE;
	_dhcpMessage->hlen = DHCPCORE_HLEN;
	_dhcpMessage->hops = DHCPCORE_HOPS;
	// generate random id and set it to message
	_dhcp_xid = (rand() % UINT32_MAX);
	_dhcpMessage->xid = _dhcp_xid;
	_dhcpMessage->secs = 0;
	_dhcpMessage->flags = htons(DHCPCORE_BROADCAST_FLAG);
	// convert IPv4 addresses in number and dots notation to byte notation -> inet_aton, 
	// return 1 if succesfully created, 0 otherwise
	// try set all ip addresses , if error occured, raise the flag and stop creating message
	int noError = inet_pton(AF_INET, DHCPCORE_DISCOVER_CLIENT_IP, &_dhcpMessage->ciaddr);
	if (!noError) { _errorType = INET_ATON_ERROR; return; }
	noError = inet_pton(AF_INET, DHCPCORE_DISCOVER_YOUR_IP, &_dhcpMessage->yiaddr);
	if (!noError) { _errorType = INET_ATON_ERROR; return; }
	noError = inet_pton(AF_INET, DHCPCORE_DISCOVER_SERVER_IP, &_dhcpMessage->siaddr);
	if (!noError) { _errorType = INET_ATON_ERROR; return; }
	noError = inet_pton(AF_INET, DHCPCORE_DISCOVER_GATEWAY_IP, &_dhcpMessage->giaddr);
	if(!noError) {_errorType = INET_ATON_ERROR; return;	}
	
	// generate and set spoffed MAC address
	genAndSetMACAddress();

	// set options
	int optionPos = 0;
	/* first four bytes of options field is magic cookie (as per RFC 2132) */
	_dhcpMessage->options[optionPos] = '\x63';
	_dhcpMessage->options[++optionPos] = '\x82';
	_dhcpMessage->options[++optionPos] = '\x53';
	_dhcpMessage->options[++optionPos] = '\x63';

	/* DHCP message type is embedded in options field */
	_dhcpMessage->options[++optionPos] = 53;					/* DHCP message type option identifier */
	_dhcpMessage->options[++optionPos] = '\x01';              /* DHCP message option length in bytes */
	_dhcpMessage->options[++optionPos] = DHCP_TYPE_DISCOVER;

	// end option
	_dhcpMessage->options[++optionPos] = 255;					/* DHCP message end of options */

	// optimize packet size
	_currentMessageSize = DHCPCore::DHCPHeaderSize() + ++optionPos;
	if(_currentMessageSize < MIN_DHCP_PACKET_SIZE)
		_currentMessageSize = MIN_DHCP_PACKET_SIZE;

	_state = DHCP_TYPE_DISCOVER;

	/*fprintf(stdout, "Op: %d\n", _dhcpMessage->op);
	fprintf(stdout, "Type: %d\n", _dhcpMessage->htype);
	fprintf(stdout, "hlen: %d\n", _dhcpMessage->hlen);
	fprintf(stdout, "hops: %d\n", _dhcpMessage->hops);
	fprintf(stdout, "xid: 0x%08X\n", _dhcpMessage->xid);
	fprintf(stdout, "secs: %d\n", _dhcpMessage->secs);
	fprintf(stdout, "flags: %d\n", _dhcpMessage->flags);*/
}

void DHCPCore::ProcessDHCPDiscoverMessage(unsigned char* message, int &messageLength)
{
	if (messageLength <= 0 || DHCPCore::IsDHCPMessage(message, messageLength) == false)
	{
		_errorType = NON_VALID_MESSAGE;
		return;
	}
	// load to client dhcp message
	std::memset(_dhcpMessageResponse, 0, sizeof(dhcp_packet));
	std::memcpy(_dhcpMessageResponse, message, messageLength);
	_dhcpMessageResponseLength = messageLength;

	_state = DHCP_TYPE_DISCOVER;
}

void DHCPCore::ProcessDHCPOfferMessage(unsigned char* message, int &messageLength)
{
	if (messageLength <= 0 || DHCPCore::IsDHCPMessage(message, messageLength) == false)
	{
		_errorType = NON_VALID_MESSAGE;
		return;
	}
	// init dhcp class and fill it with message
	std::memset(_dhcpMessageResponse, 0, sizeof(dhcp_packet));
	std::memcpy(_dhcpMessageResponse, message, messageLength * sizeof(char));
	_dhcpMessageResponseLength = messageLength;

	uint32_t dhcpType = 0;
	getOptionValue(53, dhcpType);
	if(dhcpType == DHCP_TYPE_OFFER)
	{
		_state = DHCP_TYPE_OFFER;
		createDHCPRequestMessage();
	}
	else
	{
		// unexpected message type
		fprintf(stderr, "Unexpected dhcp message type: %d. Waiting for DHCPOffer.\n", dhcpType);
	}
}

void DHCPCore::ProcessDHCPDeclineMessage(unsigned char* message, int &messageLength)
{
	if (messageLength <= 0 || DHCPCore::IsDHCPMessage(message, messageLength) == false)
	{
		_errorType = NON_VALID_MESSAGE;
		return;
	}
	// init dhcp class and fill it with message
	std::memset(_dhcpMessageResponse, 0, sizeof(dhcp_packet));
	std::memcpy(_dhcpMessageResponse, message, messageLength * sizeof(char));
	_dhcpMessageResponseLength = messageLength;
}

void DHCPCore::ProcessDHCPReleaseMessage(unsigned char* message, int &messageLength)
{
	if (messageLength <= 0 || DHCPCore::IsDHCPMessage(message, messageLength) == false)
	{
		_errorType = NON_VALID_MESSAGE;
		return;
	}
	// init dhcp class and fill it with message
	std::memset(_dhcpMessageResponse, 0, sizeof(dhcp_packet));
	std::memcpy(_dhcpMessageResponse, message, messageLength * sizeof(char));
	_dhcpMessageResponseLength = messageLength;
}

void DHCPCore::ProcessDHCPInformMessage(unsigned char* message, int &messageLength)
{
	if (messageLength <= 0 || DHCPCore::IsDHCPMessage(message, messageLength) == false)
	{
		_errorType = NON_VALID_MESSAGE;
		return;
	}
	// init dhcp class and fill it with message
	std::memset(_dhcpMessageResponse, 0, sizeof(dhcp_packet));
	std::memcpy(_dhcpMessageResponse, message, messageLength * sizeof(char));
	_dhcpMessageResponseLength = messageLength;
}


void DHCPCore::createDHCPOfferMessage(in_addr &ipAddrToOffer, serverSettings &serverSet)
{
	// init message
	std::memset(_dhcpMessage, 0, sizeof(dhcp_packet));
	// set values
	_dhcpMessage->op = DHCPCORE_OFFER_OP;
	_dhcpMessage->htype = DHCPCORE_HTYPE;
	_dhcpMessage->hlen = DHCPCORE_HLEN;
	_dhcpMessage->hops = DHCPCORE_HOPS;
	// copy xid from discover
	_dhcp_xid = _dhcpMessageResponse->xid;
	_dhcpMessage->xid = _dhcp_xid;
	_dhcpMessage->secs = 0;
	_dhcpMessage->flags = _dhcpMessageResponse->flags;
	// convert IPv4 addresses in number and dots notation to byte notation -> inet_aton, 
	// return 1 if succesfully created, 0 otherwise
	// try set all ip addresses , if error occured, raise the flag and stop creating message
	int noError = inet_pton(AF_INET, DHCPCORE_OFFER_CLIENT_IP, &_dhcpMessage->ciaddr);
	if (!noError) { _errorType = INET_ATON_ERROR; return; }
	_dhcpMessage->yiaddr = ipAddrToOffer;
	_dhcpMessage->siaddr = serverSet.gateway;
	_dhcpMessage->giaddr = _dhcpMessageResponse->giaddr;
	std::memcpy(&_dhcpMessage->chaddr, &_dhcpMessageResponse->chaddr, DHCPCORE_CHADDR_LENGTH);
	// fill options for offer
	int optionPos = 0;
	// magic cookie - must
	_dhcpMessage->options[optionPos] = '\x63';
	_dhcpMessage->options[++optionPos] = '\x82';
	_dhcpMessage->options[++optionPos] = '\x53';
	_dhcpMessage->options[++optionPos] = '\x63';
	/* DHCP message type is embedded in options field */
	_dhcpMessage->options[++optionPos] = 53;					/* DHCP message type option identifier */
	_dhcpMessage->options[++optionPos] = '\x01';              /* DHCP message option length in bytes */
	_dhcpMessage->options[++optionPos] = DHCP_TYPE_OFFER;
	// dsn server
	_dhcpMessage->options[++optionPos] = 6;
	_dhcpMessage->options[++optionPos] = 4;
	std::memcpy(&_dhcpMessage->options[++optionPos], &serverSet.dnsServer.s_addr, 4);
	optionPos += 3;
	// domain
	_dhcpMessage->options[++optionPos] = 15;
	_dhcpMessage->options[++optionPos] = serverSet.domain.size();
	std::memcpy(&_dhcpMessage->options[++optionPos], serverSet.domain.c_str(), serverSet.domain.size());
	optionPos += serverSet.domain.size()-1;
	// lease time - must
	_dhcpMessage->options[++optionPos] = 51;
	_dhcpMessage->options[++optionPos] = 4;
	std::memcpy(&_dhcpMessage->options[++optionPos], &(serverSet.leaseTime), 4);
	optionPos += 3;
	// server identifier - must
	_dhcpMessage->options[++optionPos] = DHCPCORE_OPT_SERVER_IDENTIFIER;
	_dhcpMessage->options[++optionPos] = 4;
	std::memcpy(&_dhcpMessage->options[++optionPos], &(serverSet.serverIdentifier.s_addr), 4);
	optionPos += 3;
	// end option
	_dhcpMessage->options[++optionPos] = 255;					/* DHCP message end of options */

	// optimize size to send
	_currentMessageSize = DHCPCore::DHCPHeaderSize() + ++optionPos;
	if(_currentMessageSize < MIN_DHCP_PACKET_SIZE)
		_currentMessageSize = MIN_DHCP_PACKET_SIZE;

	_state = DHCP_TYPE_OFFER;
}

void DHCPCore::createDHCPRequestMessage()
{
	// edit _dhcpMessage to request
	std::memset(&_dhcpMessage->options, 0, sizeof(_dhcpMessage->options));
	// set options
	// first four bytes of options field is magic cookie (as per RFC 2132) 
	_dhcpMessage->options[0] = '\x63';
	_dhcpMessage->options[1] = '\x82';
	_dhcpMessage->options[2] = '\x53';
	_dhcpMessage->options[3] = '\x63';

	// DHCP message type is embedded in options field 
	_dhcpMessage->options[4] = 53;					/* DHCP message type option identifier */
	_dhcpMessage->options[5] = '\x01';              /* DHCP message option length in bytes */
	_dhcpMessage->options[6] = DHCP_TYPE_REQUEST;

	// fill options for dhcp request
	_dhcpMessage->options[7] = 50;
	_dhcpMessage->options[8] = '\x04';
	std::memcpy(&_dhcpMessage->options[9], &_dhcpMessageResponse->yiaddr.s_addr, 4);
	// save ip we are requesting
	_offeredIPAddress = _dhcpMessageResponse->yiaddr.s_addr;

	uint32_t serverIdentifier = 0;
	getOptionValue(DHCPCORE_OPT_SERVER_IDENTIFIER, serverIdentifier);
	if(serverIdentifier != 0)
	{
		_dhcpMessage->options[13] = DHCPCORE_OPT_SERVER_IDENTIFIER;
		_dhcpMessage->options[14] = '\x04';
		std::memcpy(&_dhcpMessage->options[15], &serverIdentifier, 4);
		// end option
		_dhcpMessage->options[19] = 255;
	}
	else
	{
		// end option
		_dhcpMessage->options[13] = 255;					/* DHCP message end of options */
	}

	_state = DHCP_TYPE_REQUEST;
}

void DHCPCore::ProcessDHCPRequestMessage(unsigned char* message, int &messageLength) 
{
	//fprintf(stdout, "Processign DHCPRequest\n");

	if (messageLength <= 0 || DHCPCore::IsDHCPMessage(message, messageLength) == false)
	{
		_errorType = NON_VALID_MESSAGE;
		return;
	}
	// init dhcp class and fill it with message
	std::memset(_dhcpMessageResponse, 0, sizeof(dhcp_packet));
	std::memcpy(_dhcpMessageResponse, message, messageLength * sizeof(char));
	_dhcpMessageResponseLength = messageLength;

	_state = DHCP_TYPE_REQUEST;
}

void DHCPCore::ProcessDHCPAckMessage(unsigned char* message, int &messageLength)
{
	if (messageLength <= 0)
	{
		_errorType = NON_VALID_MESSAGE;
		return;
	}
	// init dhcp class and fill it with message
	std::memset(_dhcpMessageResponse, 0, sizeof(dhcp_packet));
	std::memcpy(_dhcpMessageResponse, message, messageLength * sizeof(char));
	// check if it is correct message
	uint32_t dhcpType = 0;
	getOptionValue(53, dhcpType);
	if(dhcpType == DHCP_TYPE_ACK)
	{ 	// finish
		_state = DHCP_TYPE_ACK;
	}
	else
	{
		// unexpected message type
		fprintf(stderr, "Unexpected dhcp message type: %d\n", dhcpType);
	}
}

void DHCPCore::createDHCPAckMessage(in_addr &ipAddrToOffer, serverSettings &serverSet, bool broadCastBitSet)
{
	//fprintf(stdout, "Creating DHCPAck message.\n");
	// init message
	std::memset(_dhcpMessage, 0, sizeof(dhcp_packet));
	// set values
	_dhcpMessage->op = DHCPCORE_ACK_OP;
	_dhcpMessage->htype = DHCPCORE_HTYPE;
	_dhcpMessage->hlen = DHCPCORE_HLEN;
	_dhcpMessage->hops = DHCPCORE_HOPS;
	// copy xid from discover
	_dhcp_xid = _dhcpMessageResponse->xid;
	_dhcpMessage->xid = _dhcp_xid;
	_dhcpMessage->secs = 0;
	if(broadCastBitSet)
		_dhcpMessage->flags = htons(DHCPCORE_BROADCAST_FLAG); 
	else
		_dhcpMessage->flags = _dhcpMessageResponse->flags;
	// convert IPv4 addresses in number and dots notation to byte notation -> inet_aton, 
	// return 1 if succesfully created, 0 otherwise
	// try set all ip addresses , if error occured, raise the flag and stop creating message
	_dhcpMessage->ciaddr = _dhcpMessageResponse->ciaddr;
	_dhcpMessage->yiaddr = ipAddrToOffer;
	_dhcpMessage->siaddr = serverSet.gateway;
	_dhcpMessage->giaddr = _dhcpMessageResponse->giaddr;
	std::memcpy(&_dhcpMessage->chaddr, &_dhcpMessageResponse->chaddr, DHCPCORE_CHADDR_LENGTH);
	// fill options for offer
	int optionPos = 0;
	// magic cookie - must
	_dhcpMessage->options[optionPos] = '\x63';
	_dhcpMessage->options[++optionPos] = '\x82';
	_dhcpMessage->options[++optionPos] = '\x53';
	_dhcpMessage->options[++optionPos] = '\x63';
	/* DHCP message type is embedded in options field */
	_dhcpMessage->options[++optionPos] = 53;					/* DHCP message type option identifier */
	_dhcpMessage->options[++optionPos] = '\x01';              /* DHCP message option length in bytes */
	_dhcpMessage->options[++optionPos] = DHCP_TYPE_ACK;
	// dsn server
	_dhcpMessage->options[++optionPos] = 6;
	_dhcpMessage->options[++optionPos] = 4;
	std::memcpy(&_dhcpMessage->options[++optionPos], &serverSet.dnsServer.s_addr, 4);
	optionPos += 3;
	// domain
	_dhcpMessage->options[++optionPos] = 15;
	_dhcpMessage->options[++optionPos] = serverSet.domain.size();
	std::memcpy(&_dhcpMessage->options[++optionPos], serverSet.domain.c_str(), serverSet.domain.size());
	optionPos += serverSet.domain.size()-1;
	// lease time - must if request, must not if inform
	if(_state == DHCP_TYPE_REQUEST)
	{
		_dhcpMessage->options[++optionPos] = 51;
		_dhcpMessage->options[++optionPos] = 4;
		std::memcpy(&_dhcpMessage->options[++optionPos], &serverSet.leaseTime, 4);
		optionPos += 3;
	}
	// server identifier - must
	_dhcpMessage->options[++optionPos] = DHCPCORE_OPT_SERVER_IDENTIFIER;
	_dhcpMessage->options[++optionPos] = 4;
	std::memcpy(&_dhcpMessage->options[++optionPos], &serverSet.serverIdentifier, 4);
	optionPos += 3;
	// end option
	_dhcpMessage->options[++optionPos] = 255;					/* DHCP message end of options */

	// optimize size to send
	_currentMessageSize = DHCPCore::DHCPHeaderSize() + ++optionPos;
	if(_currentMessageSize < MIN_DHCP_PACKET_SIZE)
		_currentMessageSize = MIN_DHCP_PACKET_SIZE;

	_state = DHCP_TYPE_ACK;
}

void DHCPCore::createDHCPNackMessage(serverSettings &serverSet, bool broadCastBitSet)
{
	//fprintf(stdout, "Creating DHCPNack message.\n");
	// init message
	std::memset(_dhcpMessage, 0, sizeof(dhcp_packet));
	// set values
	_dhcpMessage->op = DHCPCORE_NACK_OP;
	_dhcpMessage->htype = DHCPCORE_HTYPE;
	_dhcpMessage->hlen = DHCPCORE_HLEN;
	_dhcpMessage->hops = DHCPCORE_HOPS;
	// copy xid from discover
	_dhcp_xid = _dhcpMessageResponse->xid;
	_dhcpMessage->xid = _dhcp_xid;
	_dhcpMessage->secs = 0;
	if(broadCastBitSet)
		_dhcpMessage->flags = htons(DHCPCORE_BROADCAST_FLAG);
	else
		_dhcpMessage->flags = _dhcpMessageResponse->flags;
	_dhcpMessage->giaddr = _dhcpMessageResponse->giaddr;
	std::memcpy(&_dhcpMessage->chaddr, &_dhcpMessageResponse->chaddr, DHCPCORE_CHADDR_LENGTH);
	// fill options for offer
	int optionPos = 0;
	// magic cookie - must
	_dhcpMessage->options[optionPos] = '\x63';
	_dhcpMessage->options[++optionPos] = '\x82';
	_dhcpMessage->options[++optionPos] = '\x53';
	_dhcpMessage->options[++optionPos] = '\x63';
	/* DHCP message type is embedded in options field */
	_dhcpMessage->options[++optionPos] = 53;					/* DHCP message type option identifier */
	_dhcpMessage->options[++optionPos] = '\x01';              /* DHCP message option length in bytes */
	_dhcpMessage->options[++optionPos] = DHCP_TYPE_NACK;
	// server identifier - must
	_dhcpMessage->options[++optionPos] = DHCPCORE_OPT_SERVER_IDENTIFIER;
	_dhcpMessage->options[++optionPos] = 4;
	std::memcpy(&_dhcpMessage->options[++optionPos], &serverSet.serverIdentifier, 4);
	optionPos += 3;
	// end option
	_dhcpMessage->options[++optionPos] = 255;					/* DHCP message end of options */

	// optimize size to send
	_currentMessageSize = DHCPCore::DHCPHeaderSize() + ++optionPos;
	if(_currentMessageSize < MIN_DHCP_PACKET_SIZE)
		_currentMessageSize = MIN_DHCP_PACKET_SIZE;

	_state = DHCP_TYPE_NACK;
}

/**
 * \brief Check if the error occured in dhcpCore. If the error really occur, the cleaning is performed and the instance cannot be used anymore.
 * \return True if the error occur, false otherwise.
 */
bool DHCPCore::isError()
{
	if(_errorType != ERROROPTIONS::OK)
	{
		printDHCPCoreError();
		return true;
	}
	return false;
}

void DHCPCore::printDHCPCoreError() const
{
	switch (_errorType)
	{
	case ERROROPTIONS::INET_ATON_ERROR:
		fprintf(stderr, "Cannot convert string IP Address to byte order using 'inet_aton'!/n");
		break;
	case ERROROPTIONS::CANNOT_FIND_GIVEN_DEVICE_IP:
		fprintf(stderr, "Cannot get IP Address of given device!\n");
		break;
	case ERROROPTIONS::NON_VALID_MESSAGE:
		fprintf(stderr, "Message received but is not valid!\n");
		break;
	case INCORRECT_XID:
		fprintf(stderr, "Message has invalid xid!\n");
		break;
	default:
		fprintf(stderr, "Something happened! Sadly, I cannot continue. ErrorCode: %d\n", _errorType);
	}
}

uint32_t DHCPCore::getCurrentXID(bool fromResponse)
{
	if(fromResponse)
		return _dhcpMessageResponse->xid;
	return _dhcpMessage->xid;
}

void DHCPCore::getOptionValue(const int optionNumber, uint32_t &value)
{
	int currentOption = 0; 
	uint8_t currentOptionSize = 0;
	int position = 4; // skip magic cookie
	unsigned char* options = _dhcpMessageResponse->options;
	while(currentOption != 255)
	{
		currentOption = options[position++];
		currentOptionSize = options[position++];
		//fprintf(stdout, "Founded options and size: %d | %d\n", currentOption, currentOptionSize);
		if(currentOption == optionNumber)
		{
			if(currentOptionSize > 4)
			{
				fprintf(stderr, "Too large option, cannot process!\n");
				return;
			}
			for(auto i = 0; i < currentOptionSize; ++i)
			{
				uint32_t partVal = options[position+i];
				value = value | (partVal << (8*i));
			}
			return;
		}
		else
		{
			position += currentOptionSize;
		}
	}
}

// ------------------------ STATIC FUNCTIONS ---------------------------------------------
bool DHCPCore::IsDHCPMessage(unsigned char* message, int &messageLength)
{
	if(messageLength < MinDHCPPacketSize()) // not minimal size for DHCP, something missing
		return false;
	int optSt = DHCPHeaderSize();
	if(message[optSt] == static_cast<unsigned char>('\x63') && 
		message[++optSt] == static_cast<unsigned char>('\x82') && 
		message[++optSt] == static_cast<unsigned char>('\x53') && 
		message[++optSt] == static_cast<unsigned char>('\x63'))
	{
		// it has dhcp magic cookie
		return true;
	}
	return false;
}

int DHCPCore::GetDHCPMessageType(unsigned char* message, int &messageLength)
{
	int optSt = DHCPHeaderSize();
	unsigned char* options = &message[optSt];
	int currentOption = 0; 
	uint8_t currentOptionSize = 0;
	int position = 4; // skip magic cookie
	while(currentOption != 255)
	{
		currentOption = options[position++];
		currentOptionSize = options[position++];
		if(currentOption == 53 && currentOptionSize == 1)
		{
			return options[position];
		}
		else
		{
			position += currentOptionSize;
		}
	}
	return -1;
}

int DHCPCore::getXID(unsigned char* message, int &messageLength)
{
	uint32_t xid = 0;
	// check if dhcp message
	if(IsDHCPMessage(message, messageLength))
	{
		std::memcpy(&xid, &message[4], 4);
	}	
	return xid;
}

in_addr DHCPCore::getDeviceIP(std::string deviceName)
{
	struct ifaddrs * ifAddrStruct = NULL;
	struct ifaddrs * ifa = NULL;
	void * tmpAddrPtr = NULL;

	// device info
	struct ifaddrs _deviceInfo;
	// device IP adress
	struct in_addr _deviceIP;
	_deviceIP.s_addr = 0;
	// net mask
	struct in_addr _netMask;

	// get info about all devices on this computer -> linked list
	getifaddrs(&ifAddrStruct);

	// go through all items in linked list and check which one has correct name, save its IP Adress and net mask
	for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
		// if the device dont have IP address, skip it
		if (!ifa->ifa_addr) { 
			continue;
		}
		// check if it is valid IP4 address
		if (ifa->ifa_addr->sa_family == AF_INET) { 
			if (strcmp(ifa->ifa_name, deviceName.c_str()) == 0)
			{
				tmpAddrPtr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
#ifdef _DEBUG_
				// load address to buffer
				printf("Found correct device. YEY!\n");
				char addressBuffer[INET_ADDRSTRLEN];
				inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
				printf("%s IP Address %s\n", ifa->ifa_name, addressBuffer);
#endif
				// save info
				_deviceInfo = *ifa;
				_deviceIP = ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
			}
		}
		// check if it is valid IP6 address -> not using, skip
		else if (ifa->ifa_addr->sa_family == AF_INET6) {
			tmpAddrPtr = &((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr;
			char addressBuffer[INET6_ADDRSTRLEN];
			inet_ntop(AF_INET6, tmpAddrPtr, addressBuffer, INET6_ADDRSTRLEN);
			//printf("%s IP Address %s\n", ifa->ifa_name, addressBuffer);
		}
	}
	if (ifAddrStruct != NULL) freeifaddrs(ifAddrStruct);


	return _deviceIP;
}
