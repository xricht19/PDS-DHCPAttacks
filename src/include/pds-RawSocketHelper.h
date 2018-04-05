#ifndef _PDSRAWSOCKETHELPER_H
#define _PDSRAWSOCKETHELPER_H

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <sys/types.h>
#include <ifaddrs.h>
#include <arpa/inet.h>

#include <stdint.h>
#include <unistd.h>
#include <netinet/udp.h>
#include <netinet/ip.h>
#include <string>
#include <cstring>
#include <cstdio>

class RawSocketHelper
{
private:
	int _socketID;
	char* _socketData;
	int _socketDataLenght;
	struct ip* _ipHeader;
	struct udphdr* _udpHeader;
	char* _dataPointer;
	// for datagrams the pseudoheader is needed
	struct pseudoUDPHeader
	{
		uint32_t source_address;
		uint32_t dest_address;
		uint8_t placeholder;
		uint8_t protocol;
		uint16_t udp_length;
	};

	enum ErrorStates
	{
		OK = 0,
		CANNOT_CREATE_SOCKET,
		SOCKET_IS_NOT_CREATED,
		CANNOT_BIND_SOCKET,
	};
	ErrorStates _errorCode;
	std::string _errorMessage;

public:
	RawSocketHelper(int socketDataMaxLength);
	~RawSocketHelper();
	
	void CreateRawSocket(int domain, int protocol);
	int GetRawSocket();
	void BindRawSocket(sockaddr_in* interfaceSettings);

	bool isErrorOccure();
	void printError(FILE* channel = stderr);
};

#endif