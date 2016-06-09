#ifndef UNIX_SOCKET_H
#define UNIX_SOCKET_H

#ifdef NO_UNIX_SOCKETS
static inline int unix_stream_connect(const char *path)
{
	return -1;
}
static inline int unix_stream_listen(const char *path)
{
	return -1;
}
#else
int unix_stream_connect(const char *path);
int unix_stream_listen(const char *path);
#endif

#endif /* UNIX_SOCKET_H */
