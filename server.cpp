#include<cstdlib>
#include<iostream>
#include "network.h"
#include "server.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <map>

//#define DEBUG

using namespace std;
Server M;

void sendTestPacket(Server *M)
{
  cout << "sending packet "<<M->theSocket() <<endl;
  RFPacket pack;
  pack.type = 0;
  memset(pack.body, 0, sizeof(pack.body));
  strcpy((char *)&pack.body, "Hello World\n");
  sendPacket(M, &pack);
}

void handleReqServerInit(struct IdPacket * id)
{
  unsigned int clientId = ntohl(id->Id);
#ifdef DEBUG
  cout << "Handle Server Init Req from " << ntohl(id->Id) << endl;
#endif
  if(M.clientId() != clientId) {
    M.clientIdIs(clientId);
    if(M.fd() >= 0)
      close(M.fd());
    M.fdIs(-1);
    M.openFile_ = false;
    M.serverBufferReset();
  }
  RFPacket pack;
  pack.type = ACK_INIT;
  struct IdPacket * sid = (struct IdPacket *) & pack.body;
  sid->Id = htonl(M.theId());
  sendPacket(&M, &pack);
}

void handleOpenFileReq(struct ReqOpenFilePacket * req)
{
#ifdef DEBUG
  cout << "Handle Server Open File Req\n";
#endif
  RFPacket pack;
  pack.type = ACK_OPEN_FILE;
  struct AckOpenFilePacket * ack = (struct AckOpenFilePacket *) & pack.body;
  ack->serverId = htonl(M.theId());

  unsigned short nameLen = ntohs(req->nameLen);
  const char * fileName = req->name;
  char nameBuf[128];
  if(nameLen > 127) {
    nameLen = 127;
    strncpy(nameBuf, fileName, 127);
  } else
    strncpy(nameBuf, fileName, nameLen);
  nameBuf[nameLen] = 0;
  if(M.file() != string(nameBuf) && M.fd() > 0){
    close(M.fd());
    M.fdIs(-1);
  }
  M.fileIs(string(nameBuf));
  M.openFile_ = true;

  ack->success = true;
  sendPacket(&M, &pack);
}

void handleReqWriteFile(struct ReqWritePacket * reqWrite)
{
  size_t stagedSlot = ntohl(reqWrite->stagedSlot);

  struct StagedWrite & stagedWrite = M.stagedWrites[stagedSlot]; 
  stagedWrite.writeSeq = ntohl(reqWrite->writeSeq), 
  stagedWrite.byteOffset = ntohl(reqWrite->byteOffset),
  stagedWrite.blockSize = ntohl(reqWrite->blockSize) ;

  memcpy(stagedWrite.buffer, reqWrite->buffer, stagedWrite.blockSize);
}

void handleCheckCommitStatus(struct CheckCommitStatusPacket *check)
{
#ifdef DEBUG
  cout << "Handle Check Commit "<<endl;
#endif

  int commitId = ntohl(check->commitId);
  int curCommitId =  M.lastCommitId_ + 1;
  if(curCommitId < 0)
    curCommitId = 0;
  if(commitId < curCommitId) {
    cout << "Don't handle\n";
    cout << "Cur commit Id "<< curCommitId << endl;
    cout << "req Commit Id "<<commitId << endl;
    return;
  }

  size_t totalWriteCount = ntohl(check->totalWriteCount);
  int writeSeqFirst = ntohl(check->writeSeqFirst);
  M.totalWriteCount_ = totalWriteCount;
  int writeSeq = writeSeqFirst;
  vector<size_t>missing;
  for(size_t i=0; i < totalWriteCount; ++i){
    if(M.stagedWrites[i].writeSeq != writeSeq) {
#ifdef DEBUG
      cout << i <<" Slot writeSeq is "<<writeSeq<<endl;
#endif
      missing.push_back(i);
      if(missing.size() >= MAX_MISSING_WRITE)
        break;
    }
    writeSeq++;
    if(writeSeq < 0)
      writeSeq = 0;
  }

  if(missing.size() == 0) {
    M.readyToCommit_ = true;
#ifdef DEBUG
    cout << "++++ Ready to commit \n";
#endif
  } else
    M.readyToCommit_ = false;

  RFPacket pack;
  pack.type = ACK_COMMIT_STATUS;
  struct AckCommitStatusPacket *ack =(struct AckCommitStatusPacket*)&pack.body;
  ack->serverId = htonl(M.theId());
  ack->commitId = htonl(commitId);
  ack->totalMissingWrite = htons((unsigned short)( missing.size() ) );
  for(size_t i=0; i < missing.size(); ++i ) {
    ack->missingWriteStages[i] = htonl( missing[i] );
  }
  sendPacket(&M, &pack);
}

void handleReqCommit(struct CommitIdPacket *req)
{
  int reqCommitId = ntohl(req->commitId);
  int currentCommitId = M.lastCommitId_ + 1;
  if(currentCommitId < 0)
    currentCommitId = 0;
  if(M.readyToCommit_ && currentCommitId <= reqCommitId) {
    int fd = M.fd();
    if(fd < 0 && M.openFile_ == true && M.totalWriteCount_) {
      fd = open((M.dir() + '/' + M.file()).c_str(),
                O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR );
      if(fd >= 0) 
        M.fdIs(fd);
      else
        cerr << "**** Open file failed in the server *******\n";
    }

    for(size_t i = 0; i < M.totalWriteCount_; ++i) {
      struct StagedWrite & stagedWrite = M.stagedWrites[i];
 
      if ( lseek( M.fd(), stagedWrite.byteOffset, SEEK_SET ) < 0 ) {
        cerr<< "WriteBlock Seek" << endl;
      }
      if ( write( M.fd(), stagedWrite.buffer, stagedWrite.blockSize )  < 0 ) {
        cerr<< "WriteBlock write" << endl;
      }
    }   
    M.lastCommitId_ = reqCommitId;
    M.readyToCommit_ = false;
  } 
  if(M.lastCommitId_ == reqCommitId) {
    M.sendServerIdPacket(ACK_COMMIT);
  }
}

void Server::sendServerIdPacket(const int type)
{
  RFPacket pack;
  pack.type = type;
  struct ServerIdPacket *ack = (struct ServerIdPacket *) &pack.body;
  ack->serverId = htonl(this->theId());
  ack->success = true;
  sendPacket(this, &pack);
}

void handleReqAbort(struct CommitIdPacket *req)
{
  int reqAbortId = ntohl(req->commitId);
  int currentCommitId = M.lastCommitId_ + 1;
  if(currentCommitId < 0)
    currentCommitId = 0;
  if(currentCommitId <= reqAbortId) {
    for(size_t i=0; i < MAX_WRITE_STAGE; ++i) {
      M.stagedWrites[i].writeSeq = -1;
    }
    M.lastCommitId_ = reqAbortId;
    M.readyToCommit_ = false;
    M.totalWriteCount_ = 0;
  }
  if(M.lastCommitId_ == reqAbortId) {
    M.sendServerIdPacket(ACK_ABORT);
  }
}

void Server::serverBufferReset()
{
  for(size_t i=0; i < MAX_WRITE_STAGE; ++i)
    M.stagedWrites[i].writeSeq = -1;
  M.readyToCommit_ = false;
  M.lastCommitId_ = -1;
  M.totalWriteCount_ = 0;
}

void handleReqClose(struct CommitIdPacket *req)
{
  if(M.fd() >= 0) {
    if (close( M.fd()) < 0 ) 
      cerr << "Close error" << endl;
    M.fdIs(-1);
    M.openFile_ = false;
    M.serverBufferReset();
  }
  M.sendServerIdPacket(ACK_CLOSE);
}

void processPacket(RFEvent *eventPacket)
{
  RFPacket * pack = eventPacket->eventDetail;
  switch(pack->type) {
    case 0:  cout << (char *)&pack->body << endl; break;
    case REQ_SERVER_INIT: 
          handleReqServerInit((struct IdPacket *) &pack->body); break;
    case REQ_OPEN_FILE:
          handleOpenFileReq((struct ReqOpenFilePacket *) &pack->body); break;
    case REQ_WRITE:
          handleReqWriteFile((struct ReqWritePacket *) &pack->body); break;
    case CHECK_COMMIT_STATUS:
        handleCheckCommitStatus((struct CheckCommitStatusPacket*)&pack->body);
        break;
    case REQ_COMMIT:
        handleReqCommit((struct CommitIdPacket *) &pack->body); break;
    case REQ_ABORT:
        handleReqAbort((struct CommitIdPacket *) &pack->body); break;
    case REQ_CLOSE:
        handleReqClose((struct CommitIdPacket *) &pack->body); break;
  }
}

void serve(Server *M)
{
  RFEvent event;
  RFPacket incoming;
  event.eventDetail = &incoming;
  while(1) {
    NextEvent(&event, M->theSocket());
    switch(event.eventType) {
      case EVENT_TIMEOUT: break;
      case EVENT_NETWORK: 
        if(rand() % 100 < M->packetLoss() )
          break;
        processPacket(&event); break;
    }
  }
}

int main (int argc, char *argv[])
{
  map<string, string> param;
  if(argc<3)
    exit(0);
  for(int i=1; i+1<argc; i += 2) 
    param[string(argv[i])] = string(argv[i+1]);
  
  if(param.find(string("-port")) == param.end() || 
     param.find(string("-mount")) == param.end() || 
     param.find(string("-drop")) == param.end() ) {
    cout << "Usage: replFsServer -port ... -mount ... -drop ...\n";
    exit(0);
  }

  if(mkdir( param[string("-mount")].c_str(), 0777 )) {
    if(errno == EEXIST) {
      cout << "machine alredy in use\n";
#ifndef DEBUG
      exit(1);
#endif
    }
  }
  M.dirIs(param[string("-mount")]);

  char buf[128];
  gethostname(buf, sizeof(buf));

  int seed = 0;
  for(int i=0; i<128 && buf[i]; ++i)
    seed += buf[i];
  srand(seed);
  M.theIdIs(rand());
#ifdef DEBUG
  cout << "The server ID is " << M.theId() << endl;
#endif
  int portNum = atoi(param[string("-port")].c_str());
  M.portIs(portNum);
  M.setMaxRetry( atoi(param[string("-drop")].c_str()) );
  netInit(&M);
  serve(&M);

  return 0;
}
