
#ifdef WIN32
#include <winsock2.h>
extern int system(const char *);
#pragma comment(lib, "Ws2_32.lib")
#include "getopt.h"
#else
#include <sys/types.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#ifdef WIN32
#define socklen_t int
#define S_CLOSE(s) closesocket(s)
#define S_READ(fd, buf, len) recv(fd, buf, len, 0)
#define S_WRITE(fd, buf, len) send(fd, buf, len, 0)
#else
#define S_CLOSE(s) close(s)
#define S_READ(fd, buf, len) read(fd, buf, len)
#define S_WRITE(fd, buf, len) write(fd, buf, len)
#endif
