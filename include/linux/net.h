#ifndef _LINUX_NET_H
#define _LINUX_NET_H

#include <linux/wait.h>

typedef enum {
  SS_FREE = 0,				/* not allocated		*/
  SS_UNCONNECTED,			/* unconnected to any socket	*/
  SS_CONNECTING,			/* in process of connecting	*/
  SS_CONNECTED,				/* connected to socket		*/
  SS_DISCONNECTING			/* in process of disconnecting	*/
} socket_state;

struct socket {
  short			type;		/* SOCK_STREAM, ...		*/
  socket_state		state;
  long			flags;
  struct proto_ops	*ops;		/* protocols do most everything	*/
  void			*data;		/* protocol data		*/
  struct socket		*conn;		/* server socket connected to	*/
  struct socket		*iconn;		/* incomplete client conn.s	*/
  struct socket		*next;
  struct wait_queue	**wait;		/* ptr to place to wait on	*/
  struct inode		*inode;
};

struct proto_ops {
  int	family;

  int	(*create)	(struct socket *sock, int protocol);
  int	(*dup)		(struct socket *newsock, struct socket *oldsock);
  int	(*release)	(struct socket *sock, struct socket *peer);
  int	(*bind)		(struct socket *sock, struct sockaddr *umyaddr,
			 int sockaddr_len);
  int	(*connect)	(struct socket *sock, struct sockaddr *uservaddr,
			 int sockaddr_len, int flags);
  int	(*socketpair)	(struct socket *sock1, struct socket *sock2);
  int	(*accept)	(struct socket *sock, struct socket *newsock,
			 int flags);
  int	(*getname)	(struct socket *sock, struct sockaddr *uaddr,
			 int *usockaddr_len, int peer);
  int	(*read)		(struct socket *sock, char *ubuf, int size,
			 int nonblock);
  int	(*write)	(struct socket *sock, char *ubuf, int size,
			 int nonblock);
  int	(*select)	(struct socket *sock, int sel_type,
			 select_table *wait);
  int	(*ioctl)	(struct socket *sock, unsigned int cmd,
			 unsigned long arg);
  int	(*listen)	(struct socket *sock, int len);
  int	(*send)		(struct socket *sock, void *buff, int len, int nonblock,
			 unsigned flags);
  int	(*recv)		(struct socket *sock, void *buff, int len, int nonblock,
			 unsigned flags);
  int	(*sendto)	(struct socket *sock, void *buff, int len, int nonblock,
			 unsigned flags, struct sockaddr *, int addr_len);
  int	(*recvfrom)	(struct socket *sock, void *buff, int len, int nonblock,
			 unsigned flags, struct sockaddr *, int *addr_len);
  int	(*shutdown)	(struct socket *sock, int flags);
  int	(*setsockopt)	(struct socket *sock, int level, int optname,
			 char *optval, int optlen);
  int	(*getsockopt)	(struct socket *sock, int level, int optname,
			 char *optval, int *optlen);
  int	(*fcntl)	(struct socket *sock, unsigned int cmd,
			 unsigned long arg);	
};

#endif