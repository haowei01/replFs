#include <cstdlib>
#include <iostream>

#include "network.h"
using namespace std;
static Sockaddr groupAddr;

NetworkInstance::NetworkInstance() : fd_(-1) 
{
	myAddr_ = (Sockaddr *)malloc(sizeof(Sockaddr));
  stagedWrites = vector<StagedWrite>(MAX_WRITE_STAGE);
}

void sendPacket(NetworkInstance *M, RFPacket *pack)
{
	if (sendto(M->theSocket(), pack, sizeof(RFPacket), 0,
			(const struct sockaddr*)&groupAddr , sizeof(Sockaddr)) < 0)
		cerr << "Sendpacket error" <<endl;
}

inline void setNextTimeout (struct timeval & nexttimeout)
{
  nexttimeout.tv_sec = TIMEOUT/1000;
  nexttimeout.tv_usec = 1000 * (TIMEOUT % 1000);
}

void NextEvent(RFEvent *event, int socket)
{
  fd_set fdmask;
  static int nexttimeoutinit = 0;
  static struct timeval nexttimeout;
  struct timeval timeout, oldtime, newtime;
  int ret;
  if(!nexttimeoutinit) {
    nexttimeoutinit = 1;
    setNextTimeout(nexttimeout);
  }
  while(1) {
    FD_ZERO(&fdmask);
    FD_SET(socket, &fdmask);
    for (ret=0;ret<=0;) {
      if ((nexttimeout.tv_sec<0)||
      ((nexttimeout.tv_sec==0)&&(nexttimeout.tv_usec<=0))) 
        setNextTimeout(nexttimeout);
      timeout = nexttimeout;
      gettimeofday(&oldtime, 0);
      ret=select(32,&fdmask,NULL,NULL,&timeout);
      gettimeofday(&newtime,0);
      nexttimeout.tv_sec-=newtime.tv_sec-oldtime.tv_sec;
      nexttimeout.tv_usec-=newtime.tv_usec-oldtime.tv_usec;
      if (nexttimeout.tv_usec<0) {
        nexttimeout.tv_sec--;
        nexttimeout.tv_usec+=1000000;
      }
        if (ret==-1) 
          cerr << "select error on events" <<endl;
        else if (ret==0) {
          setNextTimeout(nexttimeout);
          event->eventType = EVENT_TIMEOUT;
//          cout << "TIMEOUT EVENT" <<endl;
          return;
        }
    }
  	if (FD_ISSET (socket, &fdmask)) {
	  	socklen_t fromLen = sizeof(event->eventSource);
      int cc;
      event->eventType = EVENT_NETWORK;
      cc = recvfrom(socket, (char *)event->eventDetail, sizeof(RFPacket), 0,
                    (struct sockaddr *)&event->eventSource, &fromLen);
      if (cc <= 0)
        cout << "event recvfrom error"<<endl;
      if (fromLen != sizeof(struct sockaddr_in))
        continue;
      if(size_t(cc) < sizeof(RFPacket)) 
        continue;
      return;
	  }
  }
}


void netInit(NetworkInstance *M)
{
  Sockaddr nullAddr;
  Sockaddr *thisHost;

  char buf[128];
  int reuse;
  u_char ttl;
  struct ip_mreq mreq;

  gethostname(buf, sizeof(buf));
  cout << buf <<endl;

	if((thisHost = resolveHost(buf)) == (Sockaddr *) NULL)
		cerr << "Error, who am I?" <<endl;
	bcopy((caddr_t) thisHost, (caddr_t) (M->myAddr()), sizeof(Sockaddr));

/*	
	char ip4[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &(M->myAddr()->sin_addr), ip4, INET_ADDRSTRLEN);
  std::cout << ip4 << std::endl;
*/
	M->theSocketIs(socket(AF_INET, SOCK_DGRAM, 0));
  if (M->theSocket() < 0)
    cerr <<"can't get socket" <<endl;

  /* SO_REUSEADDR allows more than one binding to the same
     socket - you cannot have more than one player on one
     machine without this */
  reuse = 1;
  if (setsockopt(M->theSocket(), SOL_SOCKET, SO_REUSEADDR, &reuse,
       sizeof(reuse)) < 0) {
    cerr <<"setsockopt failed (SO_REUSEADDR)";
  }

  nullAddr.sin_family = AF_INET;
  nullAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  nullAddr.sin_port = M->port();

  int bind_err = bind(M->theSocket(), (struct sockaddr *)&nullAddr,
     sizeof(nullAddr));
  if(bind_err < 0)
    cerr << "netInit binding " << bind_err <<endl;

	ttl = 1;
  if (setsockopt(M->theSocket(), IPPROTO_IP, IP_MULTICAST_TTL, &ttl,
       sizeof(ttl)) < 0) {
    cerr << "setsockopt failed (IP_MULTICAST_TTL)";
  }

  /* join the multicast group */
  mreq.imr_multiaddr.s_addr = htonl(RFSGROUP);
  mreq.imr_interface.s_addr = htonl(INADDR_ANY);
  if (setsockopt(M->theSocket(), IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)
       &mreq, sizeof(mreq)) < 0) {
    cerr << "setsockopt failed (IP_ADD_MEMBERSHIP)";
  }

  /*
   * Now we can try to find a game to join; if none, start one.
   */

//  cout <<endl;

  /* set up some stuff strictly for this local sample */

  /* Get the multi-cast address ready to use in SendData()
           calls. */
  memcpy(&groupAddr, &nullAddr, sizeof(Sockaddr));
  groupAddr.sin_addr.s_addr = htonl(RFSGROUP);

/*
  inet_ntop(AF_INET, &(groupAddr.sin_addr), ip4, INET_ADDRSTRLEN);
  std::cout << ip4 << std::endl;
*/
}

Sockaddr *
resolveHost(register char *name)
{
  register struct hostent *fhost;
  struct in_addr fadd;
  static Sockaddr sa;

  struct in_addr **addr_list;

  if ((fhost = gethostbyname(name)) != NULL) {
    sa.sin_family = fhost->h_addrtype;
    sa.sin_port = 0;
    bcopy(fhost->h_addr, &sa.sin_addr, fhost->h_length);
    addr_list = (struct in_addr **)fhost->h_addr_list;
/*    for(int i=0; addr_list[i] != NULL; ++i) {
      cout << "cout get host by name: " <<inet_ntoa(*addr_list[i]) <<endl;
    }*/
  } else {
    fadd.s_addr = inet_addr(name);
    if (fadd.s_addr != (unsigned long)(-1)) {
      sa.sin_family = AF_INET;  /* grot */
      sa.sin_port = 0;
      sa.sin_addr.s_addr = fadd.s_addr;
    } else
      return(NULL);
  }
  return(&sa);
}


