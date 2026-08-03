/*
===========================================================================

DarklightNG Source Code
Copyright (C) 2026 - Justin Marshall (aka IceColdDuke).

Linux sockets used by the SDL2 platform build. SDL2 itself intentionally does
not provide a networking API.

===========================================================================
*/

#include "../idlib/precompiled.h"
#pragma hdrstop

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

idCVar net_ip( "net_ip", "localhost", CVAR_SYSTEM, "local IP address" );
idCVar net_port( "net_port", "0", CVAR_SYSTEM | CVAR_INTEGER, "local IP port number" );
idCVar net_forceLatency( "net_forceLatency", "0", CVAR_SYSTEM | CVAR_INTEGER, "milliseconds latency" );
idCVar net_forceDrop( "net_forceDrop", "0", CVAR_SYSTEM | CVAR_INTEGER, "percentage packet loss" );

static void NetadrToSockadr( const netadr_t &address, sockaddr_in &socketAddress ) {
	memset( &socketAddress, 0, sizeof( socketAddress ) );
	socketAddress.sin_family = AF_INET;
	socketAddress.sin_port = htons( address.port );
	if ( address.type == NA_BROADCAST ) {
		socketAddress.sin_addr.s_addr = INADDR_BROADCAST;
	} else if ( address.type == NA_LOOPBACK ) {
		socketAddress.sin_addr.s_addr = htonl( INADDR_LOOPBACK );
	} else {
		memcpy( &socketAddress.sin_addr.s_addr, address.ip, sizeof( address.ip ) );
	}
}

static void SockadrToNetadr( const sockaddr_in &socketAddress, netadr_t &address ) {
	memset( &address, 0, sizeof( address ) );
	memcpy( address.ip, &socketAddress.sin_addr.s_addr, sizeof( address.ip ) );
	address.port = ntohs( socketAddress.sin_port );
	address.type = socketAddress.sin_addr.s_addr == htonl( INADDR_LOOPBACK ) ? NA_LOOPBACK : NA_IP;
}

bool Sys_StringToNetAdr( const char *text, netadr_t *address, bool doDNSResolve ) {
	if ( address == NULL || text == NULL || text[0] == '\0' ) return false;
	memset( address, 0, sizeof( *address ) );
	idStr host = text;
	const int colon = host.Last( ':' );
	if ( colon >= 0 ) {
		address->port = (unsigned short)atoi( host.c_str() + colon + 1 );
		host.CapLength( colon );
	}
	if ( !host.Icmp( "localhost" ) ) {
		address->type = NA_LOOPBACK;
		address->ip[0] = 127;
		address->ip[3] = 1;
		return true;
	}
	in_addr parsed;
	if ( inet_aton( host.c_str(), &parsed ) != 0 ) {
		address->type = NA_IP;
		memcpy( address->ip, &parsed.s_addr, sizeof( address->ip ) );
		return true;
	}
	if ( !doDNSResolve ) return false;
	addrinfo hints = {};
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	addrinfo *result = NULL;
	if ( getaddrinfo( host.c_str(), NULL, &hints, &result ) != 0 || result == NULL ) return false;
	address->type = NA_IP;
	memcpy( address->ip, &( (sockaddr_in *)result->ai_addr )->sin_addr.s_addr, sizeof( address->ip ) );
	freeaddrinfo( result );
	return true;
}

const char *Sys_NetAdrToString( const netadr_t address ) {
	static char strings[4][64];
	static int index;
	char *text = strings[index++ & 3];
	if ( address.type == NA_LOOPBACK ) {
		idStr::snPrintf( text, 64, address.port ? "localhost:%u" : "localhost", address.port );
	} else if ( address.type == NA_IP || address.type == NA_BROADCAST ) {
		idStr::snPrintf( text, 64, "%u.%u.%u.%u:%u", address.ip[0], address.ip[1], address.ip[2], address.ip[3], address.port );
	} else {
		idStr::Copynz( text, "bad", 64 );
	}
	return text;
}

bool Sys_IsLANAddress( const netadr_t address ) {
	if ( address.type == NA_LOOPBACK ) return true;
	if ( address.type != NA_IP ) return false;
	return address.ip[0] == 10 || address.ip[0] == 127 ||
		( address.ip[0] == 172 && address.ip[1] >= 16 && address.ip[1] <= 31 ) ||
		( address.ip[0] == 192 && address.ip[1] == 168 ) ||
		( address.ip[0] == 169 && address.ip[1] == 254 );
}

bool Sys_CompareNetAdrBase( const netadr_t a, const netadr_t b ) {
	if ( a.type != b.type ) return false;
	if ( a.type == NA_LOOPBACK ) return true;
	return a.type == NA_IP && memcmp( a.ip, b.ip, sizeof( a.ip ) ) == 0;
}

void Sys_InitNetworking( void ) {
	common->Printf( "SDL2 Linux networking initialized\n" );
}

void Sys_ShutdownNetworking( void ) {}

idPort::idPort() {
	memset( &bound_to, 0, sizeof( bound_to ) );
	netSocket = 0;
	packetsRead = bytesRead = packetsWritten = bytesWritten = 0;
}

idPort::~idPort() { Close(); }

bool idPort::InitForPort( int portNumber ) {
	Close();
	netSocket = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
	if ( netSocket < 0 ) {
		netSocket = 0;
		return false;
	}
	int enabled = 1;
	setsockopt( netSocket, SOL_SOCKET, SO_BROADCAST, &enabled, sizeof( enabled ) );
	fcntl( netSocket, F_SETFL, fcntl( netSocket, F_GETFL, 0 ) | O_NONBLOCK );

	sockaddr_in socketAddress = {};
	socketAddress.sin_family = AF_INET;
	socketAddress.sin_port = htons( portNumber == PORT_ANY ? 0 : portNumber );
	if ( !net_ip.GetString()[0] || !idStr::Icmp( net_ip.GetString(), "localhost" ) ) {
		socketAddress.sin_addr.s_addr = htonl( INADDR_ANY );
	} else if ( inet_aton( net_ip.GetString(), &socketAddress.sin_addr ) == 0 ) {
		socketAddress.sin_addr.s_addr = htonl( INADDR_ANY );
	}
	if ( bind( netSocket, (sockaddr *)&socketAddress, sizeof( socketAddress ) ) != 0 ) {
		Close();
		return false;
	}
	socklen_t length = sizeof( socketAddress );
	getsockname( netSocket, (sockaddr *)&socketAddress, &length );
	SockadrToNetadr( socketAddress, bound_to );
	return true;
}

void idPort::Close() {
	if ( netSocket != 0 ) close( netSocket );
	netSocket = 0;
	memset( &bound_to, 0, sizeof( bound_to ) );
}

bool idPort::GetPacket( netadr_t &from, void *data, int &size, int maxSize ) {
	if ( netSocket == 0 ) return false;
	sockaddr_in sender;
	socklen_t senderLength = sizeof( sender );
	const int result = recvfrom( netSocket, data, maxSize, 0, (sockaddr *)&sender, &senderLength );
	if ( result < 0 ) {
		if ( errno != EAGAIN && errno != EWOULDBLOCK && errno != ECONNREFUSED ) {
			common->DPrintf( "UDP recvfrom failed: %s\n", strerror( errno ) );
		}
		return false;
	}
	size = result;
	SockadrToNetadr( sender, from );
	++packetsRead;
	bytesRead += size;
	return true;
}

bool idPort::GetPacketBlocking( netadr_t &from, void *data, int &size, int maxSize, int timeout ) {
	if ( netSocket == 0 ) return false;
	fd_set readSet;
	FD_ZERO( &readSet );
	FD_SET( netSocket, &readSet );
	timeval wait = { timeout / 1000, ( timeout % 1000 ) * 1000 };
	if ( select( netSocket + 1, &readSet, NULL, NULL, timeout < 0 ? NULL : &wait ) <= 0 ) return false;
	return GetPacket( from, data, size, maxSize );
}

void idPort::SendPacket( const netadr_t to, const void *data, int size ) {
	if ( netSocket == 0 || to.type == NA_BAD ) return;
	sockaddr_in destination;
	NetadrToSockadr( to, destination );
	if ( sendto( netSocket, data, size, 0, (sockaddr *)&destination, sizeof( destination ) ) < 0 ) {
		common->DPrintf( "UDP sendto failed: %s\n", strerror( errno ) );
		return;
	}
	++packetsWritten;
	bytesWritten += size;
}

idTCP::idTCP() : fd( 0 ) { memset( &address, 0, sizeof( address ) ); }
idTCP::~idTCP() { Close(); }

bool idTCP::Init( const char *host, short port ) {
	Close();
	if ( !Sys_StringToNetAdr( host, &address, true ) ) return false;
	address.type = NA_IP;
	if ( address.port == 0 ) address.port = port;
	fd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
	if ( fd < 0 ) { fd = 0; return false; }
	sockaddr_in destination;
	NetadrToSockadr( address, destination );
	if ( connect( fd, (sockaddr *)&destination, sizeof( destination ) ) != 0 ) {
		Close();
		return false;
	}
	fcntl( fd, F_SETFL, fcntl( fd, F_GETFL, 0 ) | O_NONBLOCK );
	return true;
}

void idTCP::Close() {
	if ( fd != 0 ) close( fd );
	fd = 0;
}

int idTCP::Read( void *data, int size ) {
	if ( fd == 0 ) return -1;
	const int result = recv( fd, data, size, 0 );
	if ( result > 0 ) return result;
	if ( result < 0 && ( errno == EAGAIN || errno == EWOULDBLOCK ) ) return 0;
	Close();
	return -1;
}

int idTCP::Write( void *data, int size ) {
	if ( fd == 0 ) return -1;
	const int result = send( fd, data, size, MSG_NOSIGNAL );
	if ( result >= 0 ) return result;
	if ( errno == EAGAIN || errno == EWOULDBLOCK ) return 0;
	Close();
	return -1;
}
