#include "netdb.h"

void win32_ensure_socket_initialization(void);

#undef gethostbyname
struct hostent *mingw_gethostbyname(const char *host)
{
	ensure_socket_initialization();
	return gethostbyname(host);
}

void mingw_freeaddrinfo(struct addrinfo *res)
{
	ipv6_freeaddrinfo(res);
}

int mingw_getaddrinfo(const char *node, const char *service,
		      const struct addrinfo *hints, struct addrinfo **res)
{
	ensure_socket_initialization();
	return ipv6_getaddrinfo(node, service, hints, res);
}

int mingw_getnameinfo(const struct sockaddr *sa, socklen_t salen,
		      char *host, DWORD hostlen, char *serv, DWORD servlen,
		      int flags)
{
	ensure_socket_initialization();
	return ipv6_getnameinfo(sa, salen, host, hostlen, serv, servlen, flags);
}
