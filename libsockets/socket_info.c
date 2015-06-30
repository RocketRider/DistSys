/*
 * socket_info.c
 *
 * $Id: socket_info.c,v 1.2 2004/12/29 00:37:29 ralf Exp $
 *
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>


#include "socket_info.h"

void
get_socket_info(struct sockaddr_in6 from_sa, struct socket_info *si)
{
  struct hostent *from_he;

  from_he = gethostbyaddr((char *)&from_sa.sin6_addr, sizeof(from_sa.sin6_addr), AF_INET6);

  inet_ntop(from_sa.sin6_family, &from_sa.sin6_addr, si->addr, INET6_ADDRSTRLEN);
  strncpy(si->name,(from_he) ? from_he->h_name : si->addr,sizeof(si->name));

  si->port = ntohs(from_sa.sin6_port);
} /* end of get_socket_info */


int
get_socket_name(int fd, struct socket_info *si)
{
  struct sockaddr_in6 sin;
  socklen_t sin_len;
  int retcode;

  sin_len = sizeof(sin);
  retcode = getsockname(fd, (struct sockaddr *)&sin, &sin_len);
  if (retcode == 0) {
    get_socket_info(sin, si);
  } /* end if */

  return retcode;
} /* end of get_socket_name */


int
get_socket_peer(int fd, struct socket_info *si)
{
  struct sockaddr_in6 sin;
  socklen_t sin_len;
  int retcode;

  sin_len = sizeof(sin);
  retcode = getpeername(fd, (struct sockaddr *)&sin, &sin_len);
  if (retcode == 0) {
    get_socket_info(sin, si);
  } /* end if */

  return retcode;
} /* end of get_socket_peer */
