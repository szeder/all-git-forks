/**
 * This program takes a URL as an argument, downloads its contents over HTTP,
 * and writes it to stdout.  It's fairly standard UNIX code, so it should work
 * on most operating systems, but it targets z/OS.  TLS (HTTPS) connections are
 * supported on z/OS.  The goal here is to only depend on the system C library
 * (unless using a crypto library) for building on limited systems.
**/

#define _POSIX_C_SOURCE 200112L
#define _XOPEN_SOURCE_EXTENDED 1
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#ifdef ENABLE_GSKSSL
# include <gskssl.h>
#endif

#define HOST_LENGTH 256
#define SERVICE_LENGTH 256

struct connection_info {
  char ssl; /* boolean */
  int fd;
#ifdef ENABLE_GSKSSL
  gsk_handle gskenv;
  gsk_handle secure_socket;
#endif
};

static int
parse_url(const char *url, char *host, char *port, char **path, char *ssl)
{
  const char *end, *start;

  if (strncasecmp (url, "http://", 7) == 0)
    *ssl = 0;
  else if (strncasecmp (url, "https://", 8) == 0)
    *ssl = 1;
  else
    {
      fprintf (stderr, "Only URLs for HTTP and HTTPS requests are allowed\n");
      return -1;
    }

  start = url + 7 + *ssl;
  end = start + strcspn (start, "/:");

  if (start == end)
    {
      fprintf (stderr, "The URL is missing its host component\n");
      return -2;
    }
  else if (end - start > HOST_LENGTH - 1)
    {
      fprintf (stderr, "The URL host component is too long\n");
      return -3;
    }

  memcpy (host, start, end - start);
  host[end - start] = '\0';

  if (*end == ':')
    {
      start = end + 1;
      end = start + strcspn (start, "/");

      if (start == end)
        {
          fprintf (stderr, "The URL is missing its port component\n");
          return -4;
        }
      else if (end - start > SERVICE_LENGTH - 1)
        {
          fprintf (stderr, "The URL port component is too long\n");
          return -5;
        }

      memcpy (port, start, end - start);
      port[end - start] = '\0';
    }
  else
    if (*ssl) { port[0] = '4'; port[1] = '4'; port[2] = '3'; port[3] = '\0'; }
    else      { port[0] = '8'; port[1] = '0'; port[2] = '\0'; }

  if (*end != '/')
    {
      fprintf (stderr, "The URL is missing its path component\n");
      return -6;
    }

  *path = (char *)end;

  return 0;
}

static int
open_socket_to_host(const char *host, const char *service)
{
  struct addrinfo hints, *info, *results;
  int fd, status;

  /* Allow either IPv4 or IPv6, and specify a stream socket (TCP). */
  memset (&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  /* Retrieve information for the given host/service. */
  status = getaddrinfo (host, service, &hints, &results);
  if (status != 0)
    {
      fprintf (stderr, "getaddrinfo: %s\n", gai_strerror (status));
      return -1;
    }

  /* Loop through the returned addresses until one successfully connects. */
  for (info = results; info != NULL; info = info->ai_next)
    {
      fd = socket (info->ai_family, info->ai_socktype, info->ai_protocol);
      if (fd == -1)
        continue;

      if (connect (fd, info->ai_addr, info->ai_addrlen) != -1)
        break;

      close (fd);
    }

  /* Release the memory allocated from resolving hosts. */
  freeaddrinfo (results);

  /* Abort if the loop was exhausted without completing a connection. */
  if (info == NULL)
    {
      fprintf (stderr, "Could not connect to a host\n");
      return -2;
    }

  /* Pass back the socket file descriptor. */
  return fd;
}

#ifdef ENABLE_GSKSSL
static int
open_socket_via_gskssl(struct connection_info *conn)
{
  gsk_handle *gskenv = &conn->gskenv;
  gsk_handle *secure_socket = &conn->secure_socket;
  int fd = conn->fd, rc;

  /* Create the TLS environment. */
  rc = gsk_environment_open (gskenv);
  if (rc != GSK_OK)
  {
    fprintf (stderr, "Could not create the TLS environment\n");
    gsk_environment_close (gskenv);
    return 1;
  }

  /* Set up a TLS client. */
  gsk_attribute_set_enum (*gskenv, GSK_SESSION_TYPE, GSK_CLIENT_SESSION);
#define protocol(p, s) \
    gsk_attribute_set_enum (*gskenv, GSK_PROTOCOL_##p, GSK_PROTOCOL_##p##_##s)
  protocol(SSLV2,   OFF);
  protocol(SSLV3,   OFF);
  protocol(TLSV1,   ON);
  protocol(TLSV1_1, ON);
  protocol(TLSV1_2, ON);
#undef protocol

  /* Look for a keystore in the current directory. */
  gsk_attribute_set_buffer (*gskenv, GSK_KEYRING_FILE, "key.kdb", 0);
  gsk_attribute_set_buffer (*gskenv, GSK_KEYRING_PW, "password", 0);

  /* Initialize the TLS environment. */
  rc = gsk_environment_init (*gskenv);
  if (rc != GSK_OK)
  {
    fprintf (stderr, "Could not initialize the TLS environment\n");
    gsk_environment_close (gskenv);
    return 2;
  }

  /* Create a secure socket. */
  rc = gsk_secure_socket_open (*gskenv, secure_socket);
  if (rc != GSK_OK)
  {
    fprintf (stderr, "Could not create a secure socket\n");
    gsk_environment_close (gskenv);
    return 3;
  }

  /* Run the TLS session on the given socket descriptor. */
  gsk_attribute_set_numeric_value (*secure_socket, GSK_FD, fd);

  /* Perform the handshake. */
  rc = gsk_secure_socket_init (*secure_socket);
  if (rc != GSK_OK)
  {
    fprintf (stderr, "Could not create a secure socket\n");
    gsk_environment_close (gskenv);
    return 4;
  }

  return 0;
}
#endif

static int
connection_connect(struct connection_info *conn,
                   const char *host, const char *service)
{
  int rc;

  rc = open_socket_to_host (host, service);
  if (rc < 0)
    return -1;
  conn->fd = rc;

  if (conn->ssl)
    {
#ifdef ENABLE_GSKSSL
      return open_socket_via_gskssl (conn);
#else
      fprintf (stderr, "HTTPS is not supported on this platform\n");
      return -2;
#endif
    }

  return 0;
}

static int
connection_send(struct connection_info *conn, char *buf, size_t size)
{
  int len;

  if (conn->ssl)
#ifdef ENABLE_GSKSSL
    gsk_secure_socket_write (conn->secure_socket, buf, (int)size, &len);
#else
    return -1;
#endif
  else
    len = (int)write (conn->fd, buf, size);

  return len;
}

static int
connection_recv(struct connection_info *conn, char *buf, size_t size)
{
  int len;

  if (conn->ssl)
#ifdef ENABLE_GSKSSL
    gsk_secure_socket_read (conn->secure_socket, buf, (int)size, &len);
#else
    return -1;
#endif
  else
    len = (int)read (conn->fd, buf, size);

  return len;
}

static int
connection_disconnect(struct connection_info *conn)
{
  if (conn->ssl)
    {
#ifdef ENABLE_GSKSSL
      gsk_secure_socket_close (&conn->secure_socket);
      gsk_environment_close (&conn->gskenv);
#endif
      conn->ssl = 0;
    }

  close (conn->fd);

  return 0;
}

int
main(int argc, char **argv)
{
  struct connection_info connection;
  char host[HOST_LENGTH], port[SERVICE_LENGTH], *path;
  char buf[4096], *p = NULL, skip_headers = 1;
  ssize_t count;

  if (argc != 2)
    {
      fprintf (stderr, "Usage: %s http[s]://<host>[:<port>]/<path>\n", *argv);
      return 1;
    }

  if (parse_url (argv[1], host, port, &path, &connection.ssl) < 0)
    return 2;

  if (connection_connect (&connection, host, port) != 0)
    return 3;

  count = snprintf (buf, (size_t)sizeof(buf),
                    "GET %s HTTP/1.1\r\nHost: %s\r\n\r\n", path, host);
#ifdef __MVS__
# ifdef __XPLINK__
  if (__e2a_l (buf, (size_t)count) != (size_t)count)
    return 4;
# else
#  error "Add the compiler option -qXPLINK to support communicating in ASCII."
# endif
#endif
  connection_send (&connection, buf, count);

  while ((count = connection_recv (&connection, buf, (size_t)sizeof(buf))) > 0)
    {
      if (skip_headers)
        {
          p = strstr (buf, "\x0D\x0A\x0D\x0A"); /* ASCII \r\n\r\n */
          if (p != NULL)
            {
              p += 4;
              count -= p - buf;
              skip_headers = 0;
            }
        }

      write (1, p == NULL ? buf : p, (size_t)count);
      p = NULL;
    }

  connection_disconnect (&connection);

  return 0;
}
