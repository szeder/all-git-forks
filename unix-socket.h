#ifndef UNIX_SOCKET_H
#define UNIX_SOCKET_H

int unix_stream_connect(const char *path);
int unix_stream_listen(const char *path, int replace);

#endif /* UNIX_SOCKET_H */
