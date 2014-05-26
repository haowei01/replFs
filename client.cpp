/****************/
/* Your Name	*/
/* Date		*/
/* CS 244B	*/
/* Spring 2014	*/
/****************/

#define DEBUG

#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <iostream>
#include <cstdlib>

#include "client.h"
using namespace std;

Client myClient;

/* ------------------------------------------------------------------ */

void sendTestPacket(Client *M)
{
  cout << "sending packet "<<M->theSocket() <<endl;
  RFPacket pack;
  pack.type = 0;
  memset(pack.body, 0, sizeof(pack.body));
  strcpy((char *)&pack.body, "Client Hello World\n");
  sendPacket(M, &pack);
}

void sendReqServerInit()
{
  RFPacket pack;
  pack.type = 1;
  struct IdPacket * id = (struct IdPacket *)&pack.body;
  id->Id = htonl(myClient.theId());
  sendPacket(&myClient, &pack);
}

void processAckInit(RFPacket *p)
{
  if(p->type != ACK_INIT) 
    return;
  struct IdPacket * id = (struct IdPacket *)&(p->body);
  myClient.addNewServer(ntohl(id->Id));
}

bool collectAckInit()
{
  RFEvent event;
  RFPacket incoming;
  event.eventDetail = & incoming;
  int retry = 0;
  while(retry <= myClient.maxRetry()) {
    NextEvent(&event, myClient.theSocket());
    switch(event.eventType) {
      case EVENT_TIMEOUT: {
        ++retry; 
        sendReqServerInit();
        break;
      }
      case EVENT_NETWORK: {
        if( rand() % 100 < myClient.packetLoss()) 
          break;
        if(event.eventDetail->type == ACK_INIT)
          processAckInit(event.eventDetail);
        break;
      }
    }
    if(myClient.enoughServers())
      break;
  }
#ifdef DEBUG
  for(set<unsigned int>::iterator it = myClient.servers_.begin();
      it != myClient.servers_.end(); ++it)
    cout << "server set " << *it <<endl;
#endif
  return myClient.enoughServers();
}


int
InitReplFs( unsigned short portNum, int packetLoss, int numServers ) {
#ifdef DEBUG
  printf( "InitReplFs: Port number %d, packet loss %d percent, %d servers\n", 
	  portNum, packetLoss, numServers );
#endif

  /****************************************************/
  /* Initialize network access, local state, etc.     */
  /****************************************************/
  srand(time(NULL));
  myClient.theIdIs(rand());
//  myClient.theIdIs(1);
#ifdef DEBUG
  cout << "Client ID " << myClient.theId() << endl;
#endif
  myClient.setMaxRetry(packetLoss);
  myClient.resetOldServers();
  myClient.numServersIs(numServers);
  myClient.portIs(portNum);

  //reset Client state
  myClient.clientStateReset();

  netInit(&myClient);
  sendReqServerInit();
  if(collectAckInit())
    return NormalReturn;
  else
    return ErrorReturn;  
}

/* ------------------------------------------------------------------ */

void sendReqOpenFilePacket(const char * fileName)
{
  RFPacket pack;
  pack.type = REQ_OPEN_FILE;
  struct ReqOpenFilePacket * p = (struct ReqOpenFilePacket *) & pack.body;
  int nameLen = strlen(fileName);
  strncpy(p->name, fileName, 128); 
  if(nameLen > 127){
    p->nameLen = htons(127);
    p->name[127] = 0;
  } else 
    p->nameLen = htons((unsigned short)nameLen);
  sendPacket(&myClient, &pack);
}

void processAckOpenFile(RFPacket * p)
{
#ifdef DEBUG
  cout << "Receive ACK OPEN FILE\n";
#endif
  if(p->type != ACK_OPEN_FILE)
    return;
  struct AckOpenFilePacket * ack = (struct AckOpenFilePacket *) & p->body;
  unsigned int serverId = ntohl(ack->serverId);
  if(myClient.servers_.find( serverId ) != myClient.servers_.end())
    if(ack->success)
      myClient.serversOpen_.insert( serverId );
}

bool collectAckOpenFile(const char * fileName)
{
  RFEvent event;
  RFPacket incoming;
  event.eventDetail = & incoming;
  int retry = 0;
  while(retry <= myClient.maxRetry()) {
    NextEvent(&event, myClient.theSocket());
    switch(event.eventType) {
      case EVENT_TIMEOUT: {
        ++retry; 
        sendReqOpenFilePacket(fileName);
        break;
      }
      case EVENT_NETWORK: {
        if( rand() % 100 < myClient.packetLoss()) 
          break;
        if(event.eventDetail->type == ACK_OPEN_FILE)
          processAckOpenFile(event.eventDetail);
        break;
      }
    }
    if(myClient.serversOpen_.size() >= size_t(myClient.numServers()) )
      break;
  }
#ifdef DEBUG
  for(set<unsigned int>::iterator it = myClient.serversOpen_.begin();
      it != myClient.serversOpen_.end(); ++it)
    cout << "server open file set " << *it <<endl; 
#endif
  if(myClient.serversOpen_.size() >= size_t(myClient.numServers()) )
    return true;
  else
    return false;
}

int
OpenFile( char * fileName ) {
  ASSERT( fileName );

#ifdef DEBUG
  printf( "OpenFile: Opening File '%s'\n", fileName );
#endif
  if( myClient.fd() > 0)  //already opened a file
    return ErrorReturn;

#ifdef DEBUG
  int fd; 
  fd = open( fileName, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR );
#endif

  myClient.serversOpen_.clear();
  sendReqOpenFilePacket(fileName); 
  if(collectAckOpenFile(fileName))
#ifdef DEBUG
    myClient.fdIs(fd);
#else
    myClient.fdIs(FD_OPEN);
#endif
  else
    myClient.fdIs(ErrorReturn); 

#ifdef DEBUG
  if ( myClient.fd() < 0 )
    perror( "OpenFile" );
  cout << "fd is " << myClient.fd() <<endl;
#endif
  
  return( myClient.fd() );
}

/* ------------------------------------------------------------------ */

void sendReqWritePacket(size_t stagedSlot)
{
  RFPacket pack;
  pack.type = REQ_WRITE;
  struct ReqWritePacket * p = (struct ReqWritePacket *) & pack.body;

  p->writeSeq = htonl(myClient.stagedWrites[stagedSlot].writeSeq);
  p->stagedSlot = htonl(stagedSlot);
  p->byteOffset = htonl(myClient.stagedWrites[stagedSlot].byteOffset);
  size_t blockSize = myClient.stagedWrites[stagedSlot].blockSize;
  p->blockSize  = htonl(blockSize);

  memcpy(p->buffer, myClient.stagedWrites[stagedSlot].buffer, blockSize);

  sendPacket(&myClient, &pack);
}

int
WriteBlock( int fd, char * buffer, int byteOffset, int blockSize ) {
  //char strError[64];
  int bytesWritten;

  ASSERT( fd >= 0 );
  ASSERT( byteOffset >= 0 );
  ASSERT( buffer );
  ASSERT( blockSize >= 0 && blockSize < MaxBlockLength );

  if(fd != myClient.fd() || byteOffset >= MAX_FILE_LEN 
    || myClient.writeStage_ + 1 > MAX_WRITE_STAGE )
    return (ErrorReturn);

#ifdef DEBUG
  printf( "WriteBlock: Writing FD=%d, Offset=%d, Length=%d\n",
	fd, byteOffset, blockSize );

  if ( lseek( fd, byteOffset, SEEK_SET ) < 0 ) {
    perror( "WriteBlock Seek" );
    return(ErrorReturn);
  }

  if ( ( bytesWritten = write( fd, buffer, blockSize ) ) < 0 ) {
    perror( "WriteBlock write" );
    return(ErrorReturn);
  }
#endif
  
  int writeStage = myClient.writeStage_;
  struct StagedWrite & stagedWrite = myClient.stagedWrites[writeStage];
  stagedWrite.writeSeq = myClient.writeSeq_++;
  if(myClient.writeSeq_ < 0)
    myClient.writeSeq_ = 0; //handle rounding up
//  cout << "write seq is "<<stagedWrite.writeSeq << endl;
  stagedWrite.byteOffset = size_t (byteOffset);
  stagedWrite.blockSize  = size_t (blockSize);
  memcpy(stagedWrite.buffer, buffer, blockSize);

  sendReqWritePacket(writeStage);
  ++myClient.writeStage_;

#ifdef DEBUG
  return( bytesWritten );
#endif

  return blockSize;

}

/* ------------------------------------------------------------------ */

void sendCheckCommitStatusPacket()
{
  RFPacket pack;
  pack.type = CHECK_COMMIT_STATUS;
  struct CheckCommitStatusPacket* p=(struct CheckCommitStatusPacket*)&pack.body;

  p->commitId = htonl(myClient.commitId_);
  p->totalWriteCount = htonl(myClient.writeStage_);
  p->writeSeqFirst = htonl(myClient.stagedWrites[0].writeSeq);
//  cout << "First write seq is "<< myClient.stagedWrites[0].writeSeq << endl;
  if(myClient.writeStage_ > 0) {
    size_t lastWriteStage = myClient.writeStage_ - 1;
    p->writeSeqLast = htonl(myClient.stagedWrites[lastWriteStage].writeSeq);
  } 

  sendPacket(&myClient, &pack);
}

void processAckCommitStatus(RFPacket * pack)
{
  ASSERT(pack->type == ACK_COMMIT_STATUS);
  struct AckCommitStatusPacket *ack=(struct AckCommitStatusPacket*)&pack->body;
#ifdef DEBUG
  cout << "------check server id" <<endl;
#endif
  unsigned int serverId = ntohl(ack->serverId);
  if(myClient.servers_.find(serverId) == myClient.serversCommit_.end())
    return;
  int commitId = ntohl(ack->commitId);
#ifdef DEBUG
  cout << "---------check commit id " << commitId << endl;
#endif
  if(commitId != myClient.commitId_)
    return;
  unsigned short totalMissing = ntohs(ack->totalMissingWrite);
#ifdef DEBUG
  cout << "Total Missing Write "<< totalMissing << endl;
#endif
  if(!totalMissing) {
    myClient.serversCommit_.insert(serverId);
    return;
  }
  for(unsigned short i = 0; i < totalMissing; ++ i)
    sendReqWritePacket(ntohl(ack->missingWriteStages[i]));
}

void sendReqServerPacket(const int type)
{
  RFPacket pack;
  pack.type = type;
  struct CommitIdPacket * commit = (struct CommitIdPacket *) & pack.body;
  commit->commitId = htonl(myClient.commitId_);
  sendPacket(&myClient, &pack);
}

void processAckServerPacket(RFPacket * pack)
{
  struct ServerIdPacket *ack=(struct ServerIdPacket*)&pack->body;
  unsigned int serverId = ntohl(ack->serverId);
  if(myClient.servers_.find(serverId) == myClient.serversCommit_.end())
    return;
  if(ack->success)
    myClient.serversCommit_.insert(serverId);
}

bool clientReceiveReply(ClientState clientState)
{
  RFEvent event;
  RFPacket incoming;
  event.eventDetail = & incoming;
  int retry = 0;
  int maxRetry = myClient.maxRetry();
  if(clientState == CheckStatus)
    maxRetry *= WRITE_RETRANS_RATIO;
#ifdef DEBUG
  cout << "++++++ max Retrans is "<< maxRetry << " "<< clientState << endl;
#endif
  while(retry <= maxRetry) {
    NextEvent(&event, myClient.theSocket());
    switch(event.eventType) {
      case EVENT_TIMEOUT: {
        ++retry;
#ifdef DEBUG
        printf("++ retry %d\n", retry); 
#endif
        if(clientState == CheckStatus)  
          sendCheckCommitStatusPacket();
        if(clientState == ReqCommit) 
          sendReqServerPacket(REQ_COMMIT);
        if(clientState == ReqAbort)
          sendReqServerPacket(REQ_ABORT);
        if(clientState == ReqClose)
          sendReqServerPacket(REQ_CLOSE); 
        break;
      }
      case EVENT_NETWORK: {
        if( rand() % 100 < myClient.packetLoss()) 
          break;
        unsigned char eventType = event.eventDetail->type;
        if(clientState == CheckStatus && eventType == ACK_COMMIT_STATUS)
          processAckCommitStatus(event.eventDetail);
        if((clientState == ReqCommit && eventType == ACK_COMMIT)
          || (clientState == ReqAbort && eventType == ACK_ABORT) 
          || (clientState == ReqClose && eventType == ACK_CLOSE))
          processAckServerPacket(event.eventDetail);
        break;
      }
    }
    //check if need to retry
#ifdef DEBUG
    if(myClient.serversCommit_.size() == myClient.numServers()) {
      for(set<unsigned int>::iterator it = myClient.serversCommit_.begin();
          it != myClient.serversCommit_.end(); ++ it)
          cout << "serversCommit_ :" << *it << endl;
    }
#endif
    if(myClient.serversCommit_.size() >= size_t(myClient.numServers()) )
      return true;
  }
  return false;
}

int
Commit( int fd ) {
  ASSERT( fd >= 0 );

#ifdef DEBUG
  printf( "Commit: FD=%d\n", fd );
#endif

  if(fd != myClient.fd())
    return (ErrorReturn);

	/****************************************************/
	/* Prepare to Commit Phase			    */
	/* - Check that all writes made it to the server(s) */
	/****************************************************/
  sendCheckCommitStatusPacket();
  if(!clientReceiveReply(CheckStatus))
    return (ErrorReturn);
 
  myClient.serversCommit_.clear(); 
	/****************/
	/* Commit Phase */
	/****************/
  sendReqServerPacket(REQ_COMMIT);
  if(!clientReceiveReply(ReqCommit))
    return (ErrorReturn);

  //reset the buffer
  myClient.clientResetBuffer();

  return( NormalReturn );

}

/* ------------------------------------------------------------------ */

int
Abort( int fd )
{
  ASSERT( fd >= 0 );

#ifdef DEBUG
  printf( "Abort: FD=%d\n", fd );
#endif

  if(fd != myClient.fd())
    return (ErrorReturn);

  /*************************/
  /* Abort the transaction */
  /*************************/
  sendReqServerPacket(REQ_ABORT);
  myClient.serversCommit_.clear();
  clientReceiveReply(ReqAbort);

//reset the buffer
  myClient.clientResetBuffer();
  
  return(NormalReturn);
}

/* ------------------------------------------------------------------ */

int
CloseFile( int fd ) {

  ASSERT( fd >= 0 );

#ifdef DEBUG
  printf( "Close: FD=%d\n", fd );
#endif
  if(fd != myClient.fd())
    return (ErrorReturn);

	/*****************************/
	/* Check for Commit or Abort */
	/*****************************/
  int ret = NormalReturn;
  sendCheckCommitStatusPacket();
  if(!clientReceiveReply(CheckStatus)) {
    ret = ErrorReturn;
    // if some server is not available, cannot commit changes
  } else {
    myClient.serversCommit_.clear(); 
	/****************/
	/* Commit Phase */
	/****************/
    sendReqServerPacket(REQ_COMMIT);
    if(!clientReceiveReply(ReqCommit))
      ret = ErrorReturn;
    myClient.clientResetBuffer();
  }

  sendReqServerPacket(REQ_CLOSE);
  if(!clientReceiveReply(ReqClose))
    ret = ErrorReturn;
  myClient.clientResetBuffer();
  myClient.serversOpen_.clear();

#ifdef DEBUG
  if ( close( fd ) < 0 ) {
    perror("Close");
    return(ErrorReturn);
  }
#endif
  myClient.fdIs(-1);
  return ret;
}

/* ------------------------------------------------------------------ */




