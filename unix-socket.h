#ifndef UNIX_SOCKET_H
#define UNIX_SOCKET_H

#ifdef NO_UNIX_SOCKETS
#define unix_stream_connect(x) -1
#define unix_stream_listen(x) -1
#else
int unix_stream_connect(const char *path);
int unix_stream_listen(const char *path);
#endif

#endif /* UNIX_SOCKET_H */
