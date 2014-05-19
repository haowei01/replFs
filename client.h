/****************/
/* Your Name	*/
/* Date		*/
/* CS 244B	*/
/* Spring 2013	*/
/****************/
#include "network.h"
#include <set>
using namespace std;

enum {
  NormalReturn = 0,
  ErrorReturn = -1,
};

/* ------------------------------------------------------------------ */

#ifdef ASSERT_DEBUG
#define ASSERT(ASSERTION) \
 { assert(ASSERTION); }
#else
#define ASSERT(ASSERTION) \
{ }
#endif

/* ------------------------------------------------------------------ */

	/********************/
	/* Client Functions */
	/********************/
#ifdef __cplusplus
extern "C" {
#endif

extern int InitReplFs(unsigned short portNum, int packetLoss, int numServers);
extern int OpenFile(char * strFileName);
extern int WriteBlock(int fd, char * strData, int byteOffset, int blockSize);
extern int Commit(int fd);
extern int Abort(int fd);
extern int CloseFile(int fd);

#ifdef __cplusplus
}
#endif

/* ------------------------------------------------------------------ */

enum ClientState { CheckStatus, ReqCommit, ReqAbort, ReqClose };

class Client : public NetworkInstance {
public: 
  Client() : writeSeq_(0), commitId_(0), writeStage_(0), 
             writeRetransRatio_(WRITE_RETRANS_RATIO) {}
  void numServersIs(int numServers) { numServers_ = numServers; }
  inline int numServers() const { return numServers_; }
  void resetOldServers() { servers_.clear(); serversOpen_.clear(); 
                           serversCommit_.clear(); }
  void addNewServer(unsigned int serverId) { servers_.insert(serverId); }
  inline bool enoughServers() { return servers_.size() >= size_t(numServers_); }
  void clientStateReset () {
    servers_.clear(); serversOpen_.clear(); serversCommit_.clear();
    writeSeq_ = 0; commitId_ = 0; writeStage_ = 0;
  }
  void clientResetBuffer () {
    commitId_ = (++commitId_  >= 0) ? commitId_ : 0;
    writeStage_ = 0;
    serversCommit_.clear();
  }
  set<unsigned int> servers_;
  set<unsigned int> serversOpen_;
  set<unsigned int> serversCommit_;
  int writeSeq_; //write sequence begin from 0, negative is invalid
  int commitId_;
  int writeStage_;
  int writeRetransRatio_;
private:
  int numServers_;
};

void sendReqServerInit();
bool collectAckInit();
void processAckInit(RFPacket *p);

bool clientReceiveReply(ClientState clientState);
void sendCheckCommitStatusPacket();
void processAckCommitStatus(RFPacket * pack);
void sendReqServerPacket(const int);
void processAckServerPacket(RFPacket * pack);
