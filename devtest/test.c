/* Test Code for ReplFS
   Bob Lantz (based on code from Fusun Ertemalp)
   CS244B, Spring 2008

   Revised Diego Ongaro
   CS244B, Spring 2012
*/

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <sys/wait.h>

#include "client.h"

// The port number to pass to InitReplFs().
// It is set by the -port command-line argument.
static int port = 0;

// The number of servers to pass to InitReplFs().
// It is set by the -servers command-line argument.
static int servers = 0;

// The packet loss percentage to pass to InitReplFs().
// It is set by the -packetloss command-line argument.
static int packetloss = 0;

// The index into 'tests' of the test to run.
// It is set by the -test command-line argument.
static int testcase = 0;

static const char** serverNames = NULL;

#define log printf

static void appl1(void);
static void appl2(void);
static void appl3(void);
static void appl4(void);
static void appl5(void);
static void appl6(void);
static void appl7(void);
static void appl8(void);
static void appl9(void);
static void appl10(void);
static void appl11(void);
static void appl12(void);
static void appl13(void);

enum { TESTCOUNT = 13};
typedef void testfunc(void);

static testfunc *tests[TESTCOUNT+1] = {
  0,
  appl1,
  appl2,
  appl3,
  appl4,
  appl5,
  appl6,
  appl7,
  appl8,
  appl9,
  appl10,
  appl11,
  appl12,
  appl13,
};

static void
parseOptions(int argc, char **argv)
{
   int i;
   /* Parse 'em */
   for (i=1; i < argc; i += 2) {
     if (strcmp(argv[i], "-packetloss") == 0)
       packetloss = atoi(argv[i+1]);
     else if (strcmp(argv[i], "-port") == 0)
       port = atoi(argv[i+1]);
     else if (strcmp(argv[i], "-servers") == 0)
       servers = atoi(argv[i+1]);
     else if (strcmp(argv[i], "-test") == 0)
       testcase = atoi(argv[i+1]);
     else {
       serverNames = &argv[i];
       break;
     }
   }
   /* Check 'em */
   if (port <= 0 || servers <= 0 ||
       testcase < 1 || testcase > TESTCOUNT ||
       packetloss < 0 || packetloss > 100 ||
       serverNames == NULL) {
     printf("usage: %s "
            "-packetloss <%%> "
            "-port <port> "
            "-servers <numServers> "
            "-test <testnum> "
            "serverNames...\n", argv[0]);
     exit(1);
   }
}

static void
init(void)
{
  /* Just in case... */
  signal(SIGCLD, SIG_IGN);
  /* Max 180 seconds to run test */
  /* Reduced to 15 sec, not sure if this is right. */
  alarm(15);
  /* Exit if we time out and get a SIGALRM */
  signal(SIGALRM, exit);
}

static char *stopcmd = "ssh -n %s killall -s 9 replFsServer";

static void
stopServer(int i)
{
  static char command[1024];
  snprintf(command, sizeof(command), stopcmd, serverNames[i]);
  command[sizeof(command)-1] = '\0';
  log("stopServer: %s\n", command);
  fflush(stdout);
  system(command);
}

static void
runTest(int nservers, int packetloss, int testindex)
{
  log("Initializing client\n");
  InitReplFs(port, packetloss, nservers);
  log("Running test case %d\n", testindex);
  tests[testindex]();
  log("Test case #%d DONE\n", testindex);
}

static int
openFile(char *file)
{
  int fd = OpenFile(file);
  if (fd < 0) printf("OpenFile(%s): failed (%d)\n", file, fd);
  return fd;
}

static int
commit(int fd)
{
  int result = Commit(fd);
  if (result < 0) printf("Commit(%d): failed (%d)\n", fd, result);
  return fd;
}

static int
closeFile(int fd)
{
  int result = CloseFile(fd);
  if (result < 0) printf("CloseFile(%d): failed (%d)\n", fd, result);
  return fd;
}

int
main(int argc, char **argv)
{
  init();
  parseOptions(argc, argv);
  runTest(servers, packetloss, testcase);
  log("Exiting\n");
  return 0;
}


static void
appl1() {
    // simple case - just commit a single write update.

    int fd;
    int retVal;

    fd = openFile( "file1" );
    retVal = WriteBlock( fd, "abcdefghijkl", 0, 12 );
    retVal = commit( fd );
    retVal = closeFile( fd );
}

static void
appl2() {

    // simple case - just commit a single write update using closeFile.

    int fd;
    int retVal;

    fd = openFile( "file2" );
    retVal = WriteBlock( fd, "abcdefghijkl", 0, 12 );
    retVal = closeFile( fd ); // should commit the changes
}

static void
appl3() {

    //  simple case - just abort a single update on a new file
    //  the file  should not be in the mount directory at the end given
    //  it was not existing to start with. Script should check that


    int fd;
    int retVal;

    fd = openFile( "file3" );
    retVal = WriteBlock( fd, "abcdefghijkl", 0, 12 );
    Abort( fd );
    retVal = closeFile( fd ); // should abort and remove the file
}

static void
appl4() {

    //  simple case - just do a single commit and then single abort
    //  the file should be in the  mount directory with the committed
    //  write only thus no "mno"in it


    int fd;
    int retVal;

    fd = openFile( "file4" );
    retVal = WriteBlock( fd, "abcdefghijkl", 0, 12 );
    retVal = commit( fd );
    retVal = WriteBlock( fd, "mno", 12, 3 );
    Abort( fd );
    retVal = closeFile( fd );
}

static void
appl5() {

    // checks simple overwrite case

    int fd;
    int retVal;

    fd = openFile( "file5" );
    retVal = WriteBlock( fd, "aecdefghijkl", 0, 12 );
    retVal = WriteBlock( fd, "b", 1, 1 );
    retVal = commit( fd );
    retVal = closeFile( fd );
}

static void
appl6() {
    // try out commit - abort with multiple writes
    // No XXX should be in the file

    int fd;
    int retVal;
    int i;
    char * commitStr = "deadbeef";
    char * abortStr = "XXX";

    int commitStrLength = strlen( commitStr );
    int abortStrLength = strlen( abortStr );


    fd = openFile( "file6" );
    for (i = 0; i < 29; i++)
        retVal = WriteBlock( fd, commitStr, i * commitStrLength ,
                             commitStrLength );
    retVal = commit( fd );

    for (i = 0; i < 7; i++)
        retVal = WriteBlock( fd, abortStr, i * abortStrLength ,
                             abortStrLength );
    Abort( fd );

    retVal = closeFile( fd );

}


static void
appl7() {
    // not provided
}

static void
appl8() {
    // multiple openFiles of the same file. As a consequence,
    // this also checks that
    // when a file exists in the mount directory, they should openFile it
    // and not create a new one.

    int fd;
    int retVal;
    int i;
    char commitStrBuf[512];

    for( i = 0; i < 512; i++ )
        commitStrBuf[i] = '1';

    fd = openFile( "file8" );

    // write first transaction starting at offset 512
    for (i = 0; i < 50; i++)
        retVal = WriteBlock( fd, commitStrBuf, 512 + i * 512 , 512 );

    retVal = commit( fd );
    retVal = closeFile( fd );

    for( i = 0; i < 512; i++ )
        commitStrBuf[i] = '2';

    fd = openFile( "file8" );

    // write second transaction starting at offset 0
    retVal = WriteBlock( fd, commitStrBuf, 0 , 512 );

    retVal = commit( fd );
    retVal = closeFile( fd );


    for( i = 0; i < 512; i++ )
        commitStrBuf[i] = '3';

    fd = openFile( "file8" );

    // write third transaction starting at offset 50*512
    for (i = 0; i < 100; i++)
        retVal = WriteBlock( fd, commitStrBuf, 50 * 512 + i * 512 , 512 );

    retVal = commit( fd );
    retVal = closeFile( fd );
}



static void
appl9() {
    // not provided
}


void
static appl10() {
    // test that if a server is crashed at write updates time,
    // the library aborts the transaction at commit time
    // the file should have only 0's in it.

    int fd;
    int retVal;
    int i;
    char commitStrBuf[512];

    for( i = 0; i < 256; i++ )
        commitStrBuf[i] = '0';

    fd = openFile( "file10" );

    // zero out the file first
    for (i = 0; i < 100; i++)
        retVal = WriteBlock( fd, commitStrBuf, i * 256 , 256 );

    retVal = commit( fd );
    retVal = closeFile( fd );

    fd = openFile( "file10" );
    retVal = WriteBlock( fd, "111111", 0 , 6 );

    // KILL ONE OF THE  SERVERS HERE BY ISSUING A SYSTEM CALL
    log("killing server\n");
    stopServer(0);
    log("committing\n");

    retVal = commit( fd ); // this should return in abort
    retVal = closeFile( fd );
}



// Some lower priority error cases for coding sanity

static void
appl11() {

    // checks that a WriteBlock to a non-openFile file descriptor
    // is skipped
    // There should be only 12 0's at the end in the file

    int fd;
    int retVal;

    fd = openFile( "file11" );
    retVal = WriteBlock( fd, "000000000000", 0, 12 );

    // the following should not be performed due to wrong fd
    retVal = WriteBlock( fd + 1, "abcdefghijkl", 0, 12 );

    retVal = commit( fd );
    retVal = closeFile( fd );

}


static void
appl12() {

    // checks that funky order of operations does not cause crash or
    // bad behavior
    // there should be only 12 0's in the file

    int fd;
    int retVal;

    fd = openFile( "file12" );
    retVal = WriteBlock( fd, "000000000000", 0, 12 );
    retVal = closeFile( fd );

    // the following should not be performed due to file not openFile
    retVal = WriteBlock( fd, "abcdefghijkl", 0, 12 );
    retVal = commit( fd );
    retVal = closeFile( fd );

}

static void
appl13() {
    // not provided!
}
