/* Replicated Filesystem Server */
#include <string>
using namespace std;

class Server : public NetworkInstance {
public:
  Server() : readyToCommit_(false), lastCommitId_(-1) { }
  void clientIdIs(unsigned clientId) { clientId_ = clientId; }
  inline unsigned int clientId() { return clientId_; }
  void dirIs(string dirname) { dirname_ = dirname; }
  inline const string dir() { return dirname_; }
  void fileIs(string fileName) { fileName_ = fileName; }
  inline const string file() { return fileName_; }
  void sendServerIdPacket(const int);
  void serverBufferReset();
  bool readyToCommit_;
  int lastCommitId_;
  size_t totalWriteCount_;
private:
  unsigned int clientId_;
  string dirname_;
  string fileName_;
};


