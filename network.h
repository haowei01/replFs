/* everything related to network behavior */
#include <sys/select.h>
#include <netdb.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <string>
#include <vector>

//int const ReplFsPort = 44033;
#define RFSGROUP       0xe0010101
#define TIMEOUT 200
#define MAX_MISSING_WRITE 40
#define RETRANS_RATIO 5
#define WRITE_RETRANS_RATIO 4
#define FD_OPEN 4
#define MAX_WRITE_STAGE 128
#define MAX_FILE_LEN 1048576

#define EVENT_TIMEOUT 1
#define EVENT_NETWORK 2

#define REQ_SERVER_INIT 1
#define ACK_INIT 2
#define REQ_OPEN_FILE 3
#define ACK_OPEN_FILE 4
#define REQ_WRITE 5
#define CHECK_COMMIT_STATUS 6
#define ACK_COMMIT_STATUS 7
#define REQ_COMMIT 8
#define ACK_COMMIT 9
#define REQ_ABORT 10
#define ACK_ABORT 11
#define REQ_CLOSE 12
#define ACK_CLOSE 13

typedef struct sockaddr_in Sockaddr;

typedef struct {
  unsigned char type;
	char body [540];
} RFPacket;

typedef struct {
  short eventType;
	RFPacket *eventDetail;
	Sockaddr eventSource;
}  RFEvent;

class NetworkInstance {
public:
	inline Sockaddr* myAddr() const { return myAddr_; }
  void myAddrIs(Sockaddr *myAddr) { this->myAddr_ = myAddr; }
	inline int theSocket() const {return theSocket_; }
	void theSocketIs(int theSocket) { this->theSocket_ = theSocket; }
  inline int port() const {return port_;}
  void portIs(int port) {this->port_ = port; } 
	inline unsigned int theId() const { return this->Id_; }
	void theIdIs(unsigned int Id) { this->Id_ = Id; }
	void setMaxRetry(int packetLoss) { packetLoss_ = packetLoss/2;
		if(packetLoss_ < 0) packetLoss_ = 0;
		if(packetLoss_ > 99)packetLoss_ = 99; 
		maxRetry_ = packetLoss_/10; 
		if(packetLoss_ % 10) maxRetry_ ++;
    maxRetry_ *= RETRANS_RATIO;
	}
	inline int packetLoss() const { return packetLoss_; } 
	inline int maxRetry() const { return maxRetry_; }
	void fdIs(int fd) { fd_ = fd; }
	inline int fd() const { return fd_; }
  NetworkInstance ();
	std::vector<struct StagedWrite> stagedWrites;
protected:
  int port_;
  Sockaddr *myAddr_;
  int theSocket_;
	unsigned int Id_;
	int packetLoss_;
	int maxRetry_;
	int fd_;
};

struct StagedWrite {
	int writeSeq;
	size_t byteOffset;
	size_t blockSize;
	char buffer[512];
};

struct IdPacket {
  unsigned int Id;
} __attribute__((packed));

struct CommitIdPacket {
	int commitId;
};

struct ServerIdPacket {
	unsigned int serverId;
	bool success;
} __attribute__((packed));

struct ReqOpenFilePacket {
	unsigned short nameLen;
	char name[128];
} __attribute__((packed));

struct AckOpenFilePacket {
	unsigned int serverId;
	bool success;
} __attribute__((packed));

struct ReqWritePacket {
	unsigned int writeSeq;
	size_t	stagedSlot;
	size_t	byteOffset;
	size_t	blockSize;
	char	buffer[512];
} __attribute__((packed));

struct CheckCommitStatusPacket {
	int commitId;
	size_t totalWriteCount;
	int writeSeqFirst;
	int writeSeqLast;
} __attribute__((packed));

struct AckCommitStatusPacket {
	unsigned int serverId;
	int commitId;
	unsigned short totalMissingWrite;
	size_t missingWriteStages[MAX_MISSING_WRITE];
} __attribute__((packed));

Sockaddr *resolveHost(register char *name);
void netInit(NetworkInstance *M);

void sendPacket(NetworkInstance *M, RFPacket *pack);
void NextEvent(RFEvent *event, int socket);
