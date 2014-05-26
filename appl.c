/****************/
/* Your Name	*/
/* Date		*/
/* CS 244B	*/
/* Spring 2014	*/
/****************/

#define DEBUG

#include <stdio.h>
#include <string.h>

#include <client.h>
#include <appl.h>
#include <stdlib.h>

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


static void appl8() {
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

/* ------------------------------------------------------------------ */

int
main(int argc, char *argv[]) {

  

  int fd;
  int loopCnt;
  int byteOffset= 0;
  char strData[MaxBlockLength];

  char fileName[32] = "writeTest.txt";
  char dummyfile[32] = "dummyOpen.txt";

  /*****************************/
  /* Initialize the system     */
  /*****************************/
  int numServer = 0;
  if(argc >= 2)
    numServer = atoi(argv[1]);
  int packetLoss = 0;
  if(argc >= 3)
    packetLoss = atoi(argv[2]);
  
  if( InitReplFs( ReplFsPort, packetLoss, numServer ) < 0 ) {
    fprintf( stderr, "Error initializing the system\n" );
    return( ErrorExit );
  }

  appl8();

  /*****************************/
  /* Open the file for writing */
  /*****************************/

  fd = OpenFile( fileName );
  if ( fd < 0 ) {
    fprintf( stderr, "Error opening file '%s'\n", fileName );
    return( ErrorExit );
  }

  int fd_dummy = OpenFile(dummyfile);
  if ( fd_dummy >= 0 ) {
    fprintf( stderr, "Should not open this file '%s'\n", dummyfile );
    return ( ErrorExit );
  }

  /**************************************/
  /* Write incrementing numbers to the file */
  /**************************************/

  srand(time(NULL));
  for ( loopCnt=0; loopCnt<140; loopCnt++ ) {
    sprintf( strData, "%d\n", loopCnt );

#ifdef DEBUG
//    printf( "%d: Writing '%s' to file.\n", loopCnt, strData );
#endif

    if ( WriteBlock( fd, strData, byteOffset, strlen( strData ) ) < 0 ) {
      printf( "Error writing to file %s [LoopCnt=%d]\n", fileName, loopCnt );
//      return( ErrorExit );
    }
    byteOffset += strlen( strData );
//    byteOffset = rand() % (67 * strlen(strData));
    
  }
  Commit(fd);

/*  if ( Abort( fd ) < 0) {
    printf("Could not abort changes '%s'\n", fileName);
  }*/

  
  int randOffset = 0;
  for ( loopCnt=0; loopCnt<128; loopCnt++ ) {
    sprintf( strData, "%d\n", loopCnt + 128 );
    if ( WriteBlock( fd, strData, randOffset, strlen( strData ) ) < 0 ) {
      printf( "Error writing to file %s [LoopCnt=%d]\n", fileName, loopCnt );
      return( ErrorExit );
    }
    randOffset = rand() % (67 * strlen(strData));
//    byteOffset += strlen( strData );
  }
  if ( Abort( fd ) < 0) {
    printf("Could not abort changes '%s'\n", fileName);
  }

  /**********************************************/
  /* Can we commit the writes to the server(s)? */
  /**********************************************/
  if ( Commit( fd ) < 0 ) {
    printf( "Could not commit changes to File '%s'\n", fileName );
    return( ErrorExit );
  }
  
  byteOffset += 100;
  /** write with out explicit commit **/
  for ( loopCnt=0; loopCnt<128; loopCnt++ ) {
    sprintf( strData, "%d\n", loopCnt + 256 );
    if ( WriteBlock( fd, strData, byteOffset, strlen( strData ) ) < 0 ) {
      printf( "Error writing to file %s [LoopCnt=%d]\n", fileName, loopCnt );
      return( ErrorExit );
    }
    byteOffset += strlen( strData );
  }
  
/*  if ( Abort( fd ) < 0) {
    printf("Could not abort changes '%s'\n", fileName);
  }*/
  /**************************************/
  /* Close the writes to the server(s) */
  /**************************************/
  if ( CloseFile( fd ) < 0 ) {
    printf( "Error Closing File '%s'\n", fileName );
    return( ErrorExit );
  }

  printf( "Writes to file '%s' complete.\n", fileName );

  if ( WriteBlock( fd, strData, byteOffset, strlen( strData ) ) >= 0 ) {
    printf("Error, cannot write while this file is closed '%s'\n", fileName);
    return ( ErrorExit );
  }

  /*********************/
  /* Test open file again */
  /*********************/  
/*  fd = OpenFile( fileName );
  if ( fd < 0 ) {
    fprintf( stderr, "Error opening file '%s'\n", fileName );
    return( ErrorExit );
  }
  for ( loopCnt=0; loopCnt<128; loopCnt++ ) {
    sprintf( strData, "%d\n", loopCnt );
    if ( WriteBlock( fd, strData, byteOffset, strlen( strData ) ) < 0 ) {
      printf( "Error writing to file %s [LoopCnt=%d]\n", fileName, loopCnt );
      return( ErrorExit );
    }
    byteOffset = (rand() % 67) * strlen( strData );
  }
  if ( CloseFile( fd ) < 0 ) {
    printf( "Error Closing File '%s'\n", fileName );
    return( ErrorExit );
  }
*/
  return( NormalExit );
}

/* ------------------------------------------------------------------ */
