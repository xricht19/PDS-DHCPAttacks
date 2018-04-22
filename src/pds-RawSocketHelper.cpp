#include "include/pds-RawSocketHelper.h"
#include "include/pds-dhcpstarve.h"

RawSocketHelper::RawSocketHelper(int socketDataMaxLength)
{
	_socketID = -1;
	_socketDataLenght = socketDataMaxLength;
	_socketData = new char[_socketDataLenght];
	memset(_socketData, 0, _socketDataLenght);
	// set pointer to correct data parts
	_ipHeader = (struct ip*) _socketData;
	_udpHeader = (struct udphdr*) (_socketData + sizeof(struct ip));
	_dataPointer = (char*)(_socketData + sizeof(struct ip) + sizeof(struct udphdr));
	_errorCode = OK;
	_errorMessage = "";
}

RawSocketHelper::~RawSocketHelper()
{
	// free socket data
	delete(_socketData);
}

void RawSocketHelper::CreateRawSocket(int domain, int protocol)
{
	_socketID = socket(domain, SOCK_RAW, protocol);
	if (_socketID == -1)
	{
		_errorCode = CANNOT_CREATE_SOCKET;
	}
}

int RawSocketHelper::GetRawSocket()
{
	if (_socketID == -1)
	{
		_errorCode = SOCKET_IS_NOT_CREATED;
	}
	return _socketID;
}

void RawSocketHelper::BindRawSocket(sockaddr_in* interfaceSettings)
{
	if (bind(_socketID, (sockaddr*)&interfaceSettings, sizeof(sockaddr_in)) < 0)
	{
		_errorCode = CANNOT_BIND_SOCKET;
		_errorMessage = "ip to bind: ";
		_errorMessage.append(inet_ntoa(interfaceSettings->sin_addr));
	}
}

void RawSocketHelper::FillIPHeader(sockaddr_in * srcIP, sockaddr_in * destIP)
{
    _ipHeader->ip_hl = 5;
    _ipHeader->ip_v = 4;
    _ipHeader->ip_tos = 0;
    _ipHeader->ip_len = sizeof(struct ip) + sizeof(struct udphdr); // no data for now
    _ipHeader->ip_id = htonl(54321);							 //Id of this packet
    _ipHeader->ip_off = 0;
    _ipHeader->ip_ttl = 255;
    _ipHeader->ip_p = IPPROTO_UDP;
    _ipHeader->ip_sum = 0;								     //Set to 0 before calculating checksum
    _ipHeader->ip_src = srcIP->sin_addr;       //TO-DO: Spoof the source ip address.
	_ipHeader->ip_dst = destIP->sin_addr;
}

bool RawSocketHelper::isErrorOccure()
{
	if (_errorCode == OK)
		return false;
	
	return true;
}

void RawSocketHelper::printError(FILE * channel)
{
	switch (_errorCode)
	{
	case OK:
		fprintf(channel, "No error occure. :)");
		break;
	case CANNOT_CREATE_SOCKET:
		fprintf(channel, "Cannot create socket with current settings.");
		break;
	case SOCKET_IS_NOT_CREATED:
		fprintf(channel, "Socket is not exists.");
		break;
	case CANNOT_BIND_SOCKET:
		fprintf(channel, "Cannot bind socket to device. [%s]", _errorMessage.c_str());
		break;
	default:
		fprintf(channel, "Unknown error occure.");
		break;
	}
}
