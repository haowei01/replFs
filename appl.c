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
  for ( loopCnt=0; loopCnt<128; loopCnt++ ) {
    sprintf( strData, "%d\n", loopCnt );

#ifdef DEBUG
//    printf( "%d: Writing '%s' to file.\n", loopCnt, strData );
#endif

    if ( WriteBlock( fd, strData, byteOffset, strlen( strData ) ) < 0 ) {
      printf( "Error writing to file %s [LoopCnt=%d]\n", fileName, loopCnt );
      return( ErrorExit );
    }
    byteOffset += strlen( strData );
//    byteOffset = rand() % (67 * strlen(strData));
    
  }

/*  if ( Abort( fd ) < 0) {
    printf("Could not abort changes '%s'\n", fileName);
  }

  for ( loopCnt=0; loopCnt<128; loopCnt++ ) {
    sprintf( strData, "%d\n", loopCnt );
    if ( WriteBlock( fd, strData, byteOffset, strlen( strData ) ) < 0 ) {
      printf( "Error writing to file %s [LoopCnt=%d]\n", fileName, loopCnt );
      return( ErrorExit );
    }
    byteOffset += strlen( strData );
  } */

  /**********************************************/
  /* Can we commit the writes to the server(s)? */
  /**********************************************/
  if ( Commit( fd ) < 0 ) {
    printf( "Could not commit changes to File '%s'\n", fileName );
    return( ErrorExit );
  }
  
  /** write with out explicit commit **/
  for ( loopCnt=0; loopCnt<128; loopCnt++ ) {
    sprintf( strData, "%d\n", loopCnt );
    if ( WriteBlock( fd, strData, byteOffset, strlen( strData ) ) < 0 ) {
      printf( "Error writing to file %s [LoopCnt=%d]\n", fileName, loopCnt );
      return( ErrorExit );
    }
    byteOffset = (rand() % 67) * strlen( strData );
  }
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
  fd = OpenFile( fileName );
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

  return( NormalExit );
}

/* ------------------------------------------------------------------ */
