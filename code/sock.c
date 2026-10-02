/*****************************************************************************!
!                HyperScript Software Source Code.                            !
!                                                                             !
!          ***** Copyright: (c) 2002 Abinition (TM), Inc                      !
!          ***** Program property of Abinition, Inc                           !
!          ***** All rights reserved - Licensed Material.                     !
!
!          ***** Copyright: (c) 1994 Ab Initio Software                       !
!          ***** Program property of Ab Initio Software                       !
!          ***** All rights reserved - Licensed Material.                     !
!                                                                             !
!*****************************************************************************/

/*
 *  This program is dual-licensed: either;
 *
 *  Under the terms of the GNU General Public License version 3 as 
 *  published by the Free Software Foundation. For the terms of this 
 *  license, see licenses/gplv3.md or <http://www.gnu.org/licenses/>;
 *
 *  Under the terms of the Commercial License as set out in 
 *  licenses/commercial.md
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of 
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License or Commerical License for more details.
 */

/*
 * Modifications:
 *
 */
/********************** AUTOROUTER INTERFACE ********************************/
#if defined ( WIN32 ) || defined ( __WIN32 ) || defined ( _WIN32_WCE )

#define AS_WINDOWS 1

#ifdef _WIN32_WCE
#define AS_PPC 1
#endif

#elif defined ( __VMS )

#define AS_VMS 1

#else

#define AS_UNIX 1

#endif

#include <sys/stat.h>

#ifdef AS_VMS
#include <sys/file.h>
#endif

#ifndef AS_VAXC
#ifdef AS_WINDOWS
#include <fcntl.h>
#else
#include <sys/fcntl.h>
#endif
#endif

#ifdef AS_WINDOWS
#define W32_LEAN_AND_MEAN 
#include <io.h>
#include <winsock2.h>
#define EWOULDBLOCK WSAEWOULDBLOCK
#ifdef AS_ATL
#include "interface.h"
#endif /* AS_ATL */
#else
#include <sys/socket.h>         /* Socket structures and functions */
#endif

#ifdef AS_UNIX
#include <sys/ioctl.h>          /* Socket structures and functions */
#if HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include <time.h>	/* for localtime function */
#include <sys/wait.h>
#endif

/*
#ifdef AS_UCX
#include <in.h>         
#include <starlet.h>            
#include <unixio.h>
#include <ucx$inetdef.h>        
#endif
*/

#ifdef AS_VMS
#include <ssdef.h>              /* SS$_ defines */
#include <iodef.h>              /* IO$_ defines */
#include <prcdef.h>             /* Process related defines */
#include <prvdef.h>             /* Priviledge defines */
#include <pqldef.h>             /* Process quota defines */
#include <psldef.h>
#endif

#ifdef AS_DMBX
#include <CELLworks/mbx.h>      /* For Grapheq action schedules */
#endif

#ifndef F_OK
#define F_OK            0       /* does file exist */
#define X_OK            1       /* is it executable by caller */
#define W_OK            2       /* writable by caller */
#define R_OK            4       /* readable by caller */
#endif

#ifndef FD_SETSIZE
#define FD_SETSIZE      32
typedef long    fd_set ;
#define FD_SET(n, p)    (*p |= (1 << n))
#define FD_CLR(n, p)    (*p &= ~(1 << n))
#define FD_ISSET(n, p)  (*p & (1 << n))
#define FD_ZERO(p)      (*p = 0)
#endif

#ifdef AS_VMS
#define WAIT_EVENT_FLAG         13
#endif

#ifdef AS_SSL
/* SSL Stuff */
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>

/* May need one or more of these too.*/
#include <openssl/rsa.h>  
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>


#endif

#include "auto.h"       /* System Interface and Function Prototypes */

/********************** EXTERNAL FUNCTION DECLARATIONS ***********************/

/********************** EXTERNAL GLOBAL VARIABLES ****************************/

/********************** INTERNAL GLOBAL VARIABLES ****************************/

/* Buffer for fifo reads - twice the size of max msg length. */
static char             gzInboxBuf[MAX_BUFFER_SIZE+1];
static char*            gpzInboxOffset ;
static int              giInboxOffset ;
static int              giInboxMaxMsgSize ;
static int              giInboxNbytes ;
static OVERLAPPED       gsInboxOverlapped ;
static OVERLAPPED       gsLoopbackOverlapped ;
static OVERLAPPED       gsTCPlistenOverlapped ;

#ifdef AS_SSL
static char		gzSSLbuf[SSL_BUFFER_SIZE+1] ;
static sLOGICAL	guSSLinitialized = FALSE ;
static const char gzRandomSeed[] = "string to make the random number generator think it has entropy";
static char gzPass[VALUE_SIZE+1] ;
#endif


/* Time-related variables */
static int      giNextIdleBeat ;        /* Next IdleBeat time */
static int      giNextAlarm ;           /* Next Alarm time */
static int      giNumSelectEvents ;
static HANDLE   gsEvents[MAX_EVENTS] ;

#ifndef AS_WINDOWS
/* For the select call */
static int      giNfound ;
static fd_set   gsReadFDS ;
#endif

static int      giOffset ;
static int      giOffsetInbox ;
static int      giOffsetListen ;
static int      giOffsetFds ;
static int      giOffsetHosts ;
static int      giNumFds ;
static int      giNumHosts ;
static sLOGICAL	gbDoEagain ;
static sLOGICAL	gbIsEagain ;
static sBYTE	gpEagainBuf[HTTP_WRITE_SIZE+1] ;
static int	giEagainBufLen ;
/********************** INTERNAL OBJECT STRUCTURES ***************************/
#ifdef AS_SSL
struct ssl_t {
  SSL_CTX *sslCtx ;
  SSL *ssl ;
  BIO *filterBio ;
  BIO *outBio ;
  BIO *inBio ;
  sLOGICAL isClient ;
  int state ;
  char condition[VALUE_SIZE+1] ;
} ;
#endif

/********************** FUNCTION DEFINITIONS ********************************/
typedef struct _generic_64 sG64 ;
#ifdef __cplusplus
#ifdef AS_VMS
extern "C" int sys$qiow  (unsigned int efn, unsigned short int chan,
                      unsigned int func, short* iosb, void
                      (*astadr)(__unknown_params), __int64  astprm,
                      void *p1, __int64 p2, __int64  p3, __int64 p4,
                      __int64 p5, __int64  p6);
extern "C" int sys$qio  (unsigned int efn, unsigned short int chan,
                      unsigned int func, short* iosb, void
                      (*astadr)(__unknown_params), __int64  astprm,
                      void *p1, __int64 p2, __int64  p3, __int64 p4,
                      __int64 p5, __int64  p6);
extern "C" int sys$assign  (void *devnam, unsigned short int *chan,
                        unsigned int acmode, void *mbxnam,
                        __optional_params);
extern "C" void aeqssp_automan_setctrlast ( int(*pf)(int) );
/*extern "C" int aeqSsp_autoMan_crembx ( sDescr*, int*, int*, int* );*/
extern "C" int lib$sys_trnLog ( sDescr*, int*, sDescr*, int, int ) ;
extern "C" int access(const char *file_spec, int mode);
extern "C" int sys$dassgn ( unsigned short int ) ;
extern "C" int sys$creprc  (unsigned int *pidadr, void *image, void
                         *input, void *output, void *error, struct
                         _generic_64 *prvadr, unsigned int *quota, void
                         *prcnam, unsigned int baspri, unsigned int uic,
                         unsigned short int mbxunt, unsigned int stsflg,
                         __optional_params);
extern "C" int sys$crembx  (char prmflg, unsigned short int *chan,
                         unsigned int maxmsg, unsigned int bufquo,
                         unsigned int promsk, unsigned int acmode, void
                         *lognam,...);
extern "C" void sys$cancel ( short ) ;
extern "C" int sys$waitfr (unsigned int efn);
extern "C" int sys$clref (unsigned int efn);  
extern "C" int sys$setef (unsigned int efn); 
extern "C" int sys$setast (char flg);

#endif


#ifndef AS_WINDOWS
extern "C" int close ( int fd ) ;
extern "C" ssize_t write( int filedes, const void *buffer, size_t nbytes);
extern "C" ssize_t read( int filedes, void *buffer, size_t nbytes);
extern "C" pid_t waitpid(pid_t pid, int *status, int options);
#endif

#else


#ifdef AS_VMS
extern int sys$qiow  (unsigned int efn, unsigned short int chan,
                      unsigned int func, short* iosb, void
                      (*astadr)(__unknown_params), __int64  astprm,
                      void *p1, __int64 p2, __int64  p3, __int64 p4,
                      __int64 p5, __int64  p6);
extern int sys$qio  (unsigned int efn, unsigned short int chan,
                      unsigned int func, short* iosb, void
                      (*astadr)(__unknown_params), __int64  astprm,
                      void *p1, __int64 p2, __int64  p3, __int64 p4,
                      __int64 p5, __int64  p6);
extern int sys$assign  (void *devnam, unsigned short int *chan,
                        unsigned int acmode, void *mbxnam,
                        __optional_params);
extern void aeqssp_automan_setctrlast ( int(*pf)(int) );
/*extern int aeqSsp_autoMan_crembx ( sDescr*, int*, int*, int* );*/
extern int lib$sys_trnLog ( sDescr*, int*, sDescr*, int, int ) ;
extern int access(const char *file_spec, int mode);
extern int sys$dassgn ( unsigned short int ) ;
extern int sys$creprc  (unsigned int *pidadr, void *image, void
                         *input, void *output, void *error, struct
                         _generic_64 *prvadr, unsigned int *quota, void
                         *prcnam, unsigned int baspri, unsigned int uic,
                         unsigned short int mbxunt, unsigned int stsflg,
                         __optional_params);
extern int sys$crembx  (char prmflg, unsigned short int *chan,
                         unsigned int maxmsg, unsigned int bufquo,
                         unsigned int promsk, unsigned int acmode, void
                         *lognam,...);
extern void sys$cancel ( short ) ;
extern int sys$waitfr (unsigned int efn); 
extern int sys$clref (unsigned int efn); 
extern int sys$setef (unsigned int efn);
extern int sys$setast (char flg);

#endif

#ifndef AS_WINDOWS
extern int close ( int fd ) ;
extern ssize_t write( int filedes, const void *buffer, size_t nbytes);
extern ssize_t read( int filedes, void *buffer, size_t nbytes);
extern pid_t waitpid(pid_t pid, int *status, int options);
#endif
#endif

#if defined ( AS_VMS ) || defined ( AS_WINDOWS )
#ifndef AS_DECC
void usleep ( unsigned long useconds )
{
  struct timeval timeout ;
  timeout.tv_sec = useconds / 1000 ;
  timeout.tv_usec = useconds % 1000 ;
  select ( 0, NULL, NULL, NULL, &timeout ) ;
}
#endif
#endif

#ifdef SIGALRM
static int lHyp_sock_alarmHandler ( int signo )
{
  /* Description:
   *
   *    Handler invoked when SIGALRM.
   *
   * Arguments:
   *
   *    signo                                   [R]
   *    - signal number, ie: SIGALRM
   *
   * Return value:
   *
   *    none
   *
   */
  int
    nBytes ;

  /* Set global flag */
  guSIGALRM = 1 ;

  gHyp_util_logWarning("ALARM") ;

  if ( giLoopback != INVALID_SOCKET ) {
    /*gHyp_util_debug("ALARM message to loopback");*/
    nBytes = gHyp_sock_write ( giLoopback, "|SIGALRM|||", 11, giLoopbackType
                               ,&gsLoopbackOverlapped, NULL 
                               );
    if ( nBytes <= 0 ) guSIGTERM = 1 ;
  }
  
  /*gHyp_util_debug("Cancelling i/o on %d",gsSocketToCancel);*/
  if ( gsSocketToCancel != INVALID_SOCKET ) 
    gHyp_sock_cancelIO ( gsSocketToCancel ) ;   

#ifdef AS_VMS

  /* Re-establish handler */
  gHyp_signal_establish ( SIGALRM, lHyp_sock_alarmHandler ) ;

#else

  /* Longjmp out of here if a setjmp return point was set up */
  if ( giJmpOverride ) longjmp ( gsJmpOverride, 1 ) ;

  if ( giJmpEnabled && giJmpLevel >= 0 )
    longjmp ( gsJmpStack[giJmpLevel], COND_NORMAL ) ;
#endif

  /* Else.. do nothing... silently ignored */
  return 1 ;
}

#endif

#ifdef SIGPIPE
static int lHyp_sock_pipeHandler ( int signo )
{
  /* Description:
   *
   *    Handler invoked when SIGPIPE.
   *
   * Arguments:
   *
   *    signo                                   [R]
   *    - signal number, ie: SIGPIPE
   *
   * Return value:
   *
   *    none
   *
   */
  int
    nBytes ;

  /* Set global flag */
  guSIGPIPE = 1 ;

  gHyp_util_logWarning("PIPE");

  if ( giLoopback != INVALID_SOCKET ) {
    nBytes = gHyp_sock_write ( giLoopback, "|SIGPIPE|||", 11, giLoopbackType,
                               &gsLoopbackOverlapped, NULL );
    if ( nBytes <= 0 ) guSIGTERM = 1 ;
  }
  
  if ( gsSocketToCancel != INVALID_SOCKET ) 
    gHyp_sock_cancelIO(gsSocketToCancel) ;

#ifdef AS_VMS

  /* Re-establish handler */
  gHyp_signal_establish ( SIGPIPE, lHyp_sock_pipeHandler ) ;

#else

  /* Longjmp out of here if a setjmp return point was set up */
  if ( giJmpOverride ) longjmp ( gsJmpOverride, 1 ) ;

  /* Always fatal */
  if ( giJmpEnabled ) longjmp ( gsJmpStack[0], COND_FATAL ) ;
#endif

  return 1 ;
}
#endif

#ifdef SIGIO
static int lHyp_sock_ioHandler ( int signo )
{
  /* Description:
   *
   *    Handler invoked when SIGIO.
   *
   * Arguments:
   *
   *    signo                                   [R]
   *    - signal number, ie: SIGIO
   *
   * Return value:
   *
   *    none
   *
   */
      
  int
    nBytes ;

  /* Set global flag */
  guSIGIO = 1 ;
  gHyp_util_logWarning("IO");

  if ( giLoopback != INVALID_SOCKET ) {
    nBytes = gHyp_sock_write ( giLoopback, "|SIGIO|||", 9, giLoopbackType,
                                &gsLoopbackOverlapped, NULL      );
    if ( nBytes <= 0 ) guSIGTERM = 1 ;
  }

  if ( gsSocketToCancel != INVALID_SOCKET ) 
    gHyp_sock_cancelIO(gsSocketToCancel) ;      

#ifdef AS_VMS

  /* Re-establish handler */
  gHyp_signal_establish ( SIGIO, lHyp_sock_ioHandler ) ;

#else

  /* Longjmp out of here if a setjmp return point was set up */
  if ( giJmpOverride ) longjmp ( gsJmpOverride, 1 ) ;
  
  if ( giJmpEnabled ) longjmp ( gsJmpStack[0], COND_FATAL ) ;
#endif

  return 1 ;
}
#endif

#ifdef SIGINT
static int lHyp_sock_intHandler ( int signo )
{
  /* Description:
   *
   *    Handler invoked when SIGINT.
   *
   * Arguments:
   *
   *    signo                                   [R]
   *    - signal number, ie: SIGINT
   *
   * Return value:
   *
   *    none
   *
   */
   
  int
    nBytes ;

  /* Set global flag */
  guSIGINT = 1 ;

  gHyp_util_logWarning("CTRL/C");

  /*gHyp_util_debug( "Signal interrupt to instance" ) ;*/
  gHyp_instance_signalInterrupt ( 
	gHyp_concept_getConceptInstance ( gpsConcept ) ) ;

  if ( giLoopback != INVALID_SOCKET ) {
    /*gHyp_util_debug("Notifying through loopback");*/
    nBytes = gHyp_sock_write ( giLoopback, "|SIGINT|||", 10, giLoopbackType,
                              &gsLoopbackOverlapped, NULL ) ;
  }
  if ( gsSocketToCancel != INVALID_SOCKET ) {
    /*gHyp_util_debug( "Cancel IO on %u",gsSocketToCancel);*/
    gHyp_sock_cancelIO(gsSocketToCancel) ;
  }
  
#ifdef AS_VMS
  /* Re-establish handler */
  gHyp_signal_establish ( SIGINT, lHyp_sock_intHandler ) ;
  giCondition = 1620 ;
#else

  /* Longjmp out of here if a setjmp return point was set up */
  if ( giJmpOverride ) {
    gHyp_util_debug( "Longjmp override" ) ;
    longjmp ( gsJmpOverride, 1 ) ;
  }

  if ( giJmpEnabled && giJmpLevel >= 0 ) {
    gHyp_util_debug("Longjump outa here to level: %d, addr: %p",
	giJmpLevel, gsJmpStack[giJmpLevel] );
    longjmp ( gsJmpStack[giJmpLevel], COND_NORMAL ) ;
  }
#endif

  return 1 ;
}
#endif

#ifdef SIGTERM
static int lHyp_sock_termHandler ( int signo )
{
  /* Description:
   *
   *    Handler invoked when SIGTERM.
   *
   * Arguments:
   *
   *    signo                                   [R]
   *    - signal number, ie: SIGTERM
   *
   * Return value:
   *
   *    1
   *
   */   
  int
    nBytes ;

  /* Set global flag */
  guSIGTERM = 1 ;

  gHyp_util_logWarning("Terminate");

  if ( !giJmpEnabled && giLoopback != INVALID_SOCKET ) {
    nBytes = gHyp_sock_write ( giLoopback, "|SIGTERM|||", 11, giLoopbackType,
                            &gsLoopbackOverlapped, NULL );
    if ( nBytes <= 0 ) guSIGTERM = 1 ;
  }

  if ( gsSocketToCancel != INVALID_SOCKET ) 
    gHyp_sock_cancelIO(gsSocketToCancel) ;

#ifdef AS_VMS

  /* Re-establish handler */
  gHyp_signal_establish ( SIGTERM, lHyp_sock_termHandler ) ;
#else

  /* Longjmp out of here if a setjmp return point was set up */
  if ( giJmpOverride ) longjmp ( gsJmpOverride, 1 ) ;
  
  if ( giJmpEnabled && giJmpLevel >= 0 )
    longjmp ( gsJmpStack[giJmpLevel], COND_NORMAL ) ;
#endif

  return 1 ;
}
#endif

#ifdef SIGHUP
static int lHyp_sock_hupHandler ( int signo )
{
  /* Description:
   *
   *    Handler invoked when SIGHUP.
   *
   * Arguments:
   *
   *    signo                                   [R]
   *    - signal number, ie: SIGHUP
   *
   * Return value:
   *
   *    1
   *
   */
   
  int
    nBytes ;

  /* Set global flag */
  guSIGHUP = 1 ;

  gHyp_util_logWarning("Hangup on socket %d",gsSocketToCancel );

  if ( !giJmpEnabled && giLoopback != INVALID_SOCKET ) {
    nBytes = gHyp_sock_write ( giLoopback, "|SIGHUP|||", 10, giLoopbackType,
      &gsLoopbackOverlapped,NULL);
    if ( nBytes <= 0 ) guSIGTERM = 1 ;
  }

  if ( gsSocketToCancel != INVALID_SOCKET ) 
    gHyp_sock_cancelIO(gsSocketToCancel) ;

#ifdef AS_VMS

  /* Re-establish handler */
  gHyp_signal_establish ( SIGHUP, lHyp_sock_hupHandler ) ;
#else
  /* Longjmp out of here if a setjmp return point was set up */
  if ( giJmpOverride ) longjmp ( gsJmpOverride, 1 ) ;

  if ( giJmpEnabled && giJmpLevel >= 0 ) 
    longjmp ( gsJmpStack[giJmpLevel], COND_NORMAL ) ;
#endif

  return 1 ;
}
#endif
#ifdef AS_VMS
static int lHyp_sock_qioAST( int channel )
{

  /*gHyp_util_debug ("AST completion on channel %d",channel);*/

  return SS$_ABORT ;
}

static int lHyp_sock_qioAST2( int channel )
{
  int
    nBytes,
    i ;

  /* Set flag */
  guSIGMBX = 1 ;

  /*gHyp_util_debug ("Data ready on mailbox channel %d", channel);*/

#ifdef AS_MULTINET

  /* Who ever thought of this function makes things a lot easy. */
  select_wake () ;

#else

  if ( !giJmpEnabled && giLoopback != INVALID_SOCKET ) {
    /*gHyp_util_debug("SIGMBX");*/
    nBytes = gHyp_sock_write ( giLoopback, "|SIGMBX|||", 10, giLoopbackType,
      &gsLoopbackOverlapped,NULL);
    if ( nBytes <= 0 ) guSIGTERM = 1 ;
  }

#endif

  return 1 ;
}
#endif

sLOGICAL gHyp_sock_init ( )
{

#ifdef AS_WINDOWS
  WORD wVersionRequested;
  WSADATA wsaData;
  int err;
 
#ifdef AS_PPC
  wVersionRequested = MAKEWORD( 1, 1 );
#else
  wVersionRequested = MAKEWORD( 2, 2 );
#endif
  
  err = WSAStartup( wVersionRequested, &wsaData );
  if ( err != 0 )
    /* Tell the user that we could not find a usable */
    /* WinSock DLL.                                  */
    return gHyp_util_sysError ( "Failed to initialize windoze sockets" ) ;
 
  /* Confirm that the WinSock DLL supports 2.2.*/
  /* Note that if the DLL supports versions greater    */
  /* than 2.2 in addition to 2.2, it will still return */
  /* 2.2 in wVersion since that is the version we      */
  /* requested.                                        */
 
  if ( LOBYTE( wsaData.wVersion ) != 2 ||
       HIBYTE( wsaData.wVersion ) != 2 ) {
    /* Tell the user that we could not find a usable */
    /* WinSock DLL.                                  */
    WSACleanup( );
    return gHyp_util_logError ( "Failed to find useable windoze sockets DLL" ) ;
 
   }
#endif

  /* Mark the heartbeat start time. */
  gsCurTime = time(NULL);
  giNextIdleBeat = (int)gsCurTime + IDLE_INTERVAL ;
  giNextAlarm = giNextIdleBeat ;

  /* Zero the local buffers */
  gzInboxBuf[0] = '\0' ;
  gpzInboxOffset = gzInboxBuf ;
  giInboxMaxMsgSize = MIN_MESSAGE_SIZE ;

  gsSocketToCancel = INVALID_SOCKET ;

  gbIsEagain = 0 ;
  gbDoEagain = 0 ;
  giEagainBufLen = 0 ;
  memset( &gsInboxOverlapped, 0, sizeof (OVERLAPPED) ) ;
  memset( &gsLoopbackOverlapped, 0, sizeof (OVERLAPPED) ) ;
  memset ( &gsTCPlistenOverlapped, 0, sizeof (OVERLAPPED) ) ;
#ifdef AS_WINDOWS
  gsInboxOverlapped.hEvent = (HANDLE) gHyp_sock_createEvent () ;
  gsLoopbackOverlapped.hEvent = (HANDLE) gHyp_sock_createEvent () ;
  gsTCPlistenOverlapped.hEvent = (HANDLE) gHyp_sock_createEvent () ;
  gsInboxOverlapped.Internal = STATUS_READY ;
  gsLoopbackOverlapped.Internal = STATUS_READY ;
  gsTCPlistenOverlapped.Internal = STATUS_READY ;
#endif

  /* Establish signal handlers for environment level handlers. */
  guSIGIO = 0 ;
  guSIGTERM = 0 ;
  guSIGHUP = 0 ;
  guSIGALRM = 0 ;
  guSIGPIPE = 0 ;
  guSIGINT = 0 ;
  guSIGMBX = 1 ; /* SET TO 1  SO FIRST TIME AST IS SET */
  guSIGMSG = 0 ;

#ifdef SIGALRM
  gHyp_signal_establish ( SIGALRM, lHyp_sock_alarmHandler ) ;
  gHyp_signal_unblock ( SIGALRM ) ;
#endif
#ifdef SIGPIPE
  gHyp_signal_establish ( SIGPIPE, lHyp_sock_pipeHandler ) ;
  gHyp_signal_unblock ( SIGPIPE ) ;
#endif
#ifdef SIGINT
  gHyp_signal_establish ( SIGINT, lHyp_sock_intHandler ) ;
  gHyp_signal_unblock ( SIGINT ) ;
#endif
#ifdef SIGTERM
  gHyp_signal_establish ( SIGTERM, lHyp_sock_termHandler ) ;
  gHyp_signal_unblock ( SIGTERM ) ;
#endif
#ifdef SIGIO
  gHyp_signal_establish ( SIGIO, lHyp_sock_ioHandler ) ;
  gHyp_signal_unblock ( SIGIO ) ;
#endif
#ifdef SIGHUP
  gHyp_signal_establish ( SIGHUP, lHyp_sock_hupHandler ) ;
  gHyp_signal_unblock ( SIGHUP ) ;
#endif

#ifdef AS_PROMIS
  /* Try to trap CTRL/C */
  aeqssp_automan_setctrlast ( &lHyp_sock_intHandler ) ;
#endif

  return TRUE ;
}

void gHyp_sock_usleep( unsigned int waitTime ) {

  gHyp_util_logWarning ( "Sleeping for %u microseconds", waitTime ) ;

#ifndef AS_WINDOWS
  /* Microseconds */
  usleep ( waitTime ) ;

#else
  /* Milliseconds  */
  Sleep ( waitTime/1000 ) ;
#endif

}

void gHyp_sock_clearReadFDS ()
{
  /* Description:
   *
   *    After a signal occurs, but before a longjmp out of the select call,
   *    the file descriptor mask must be cleared and the select count zeroed.
   *
   * Arguments:
   *
   *    none
   *
   * Return value:
   *
   *    none
   *
   */
  
  giNumSelectEvents = 0 ;

#ifndef AS_WINDOWS
  FD_ZERO ( &gsReadFDS) ;
  giNfound = 0 ;
#endif

}

sLOGICAL gHyp_sock_isEagain() 
{
  return gbIsEagain;
}

void gHyp_sock_doEagain() 
{
  gbIsEagain = 0 ;
  gbDoEagain = 1 ;
  giEagainBufLen = 0 ;
}

sBYTE* gHyp_sock_eagainBuf() 
{
  return gpEagainBuf;
}

int gHyp_sock_eagainBufLen()
{
  return giEagainBufLen ;
}

void gHyp_sock_cancelIO ( SOCKET s )
{
#ifdef AS_VMS
  sys$cancel ( s ) ;
#else

#ifdef AS_WINDOWS

  CancelIo ( (HANDLE) s ) ;

#endif
#endif

  return ;
}

static HANDLE lHyp_sock_openFifo ( char *path, sLOGICAL isRead, sLOGICAL isWrite )
{
  /* Open a fifo */
  
  HANDLE 
    s ;

  int
    flags ;

#ifdef AS_UNIX
  struct stat
    buf ;
#endif

#ifdef AS_WINDOWS

  int
    flags2 ;

  if ( isRead && isWrite ) {
    /*gHyp_util_debug("Opening read-write");*/
    flags = (GENERIC_WRITE | GENERIC_READ) ;
    flags2 = FILE_SHARE_READ ;
  }
  else if ( isRead ) {
    /*gHyp_util_debug("Opening read-only");*/
    flags = GENERIC_READ ;
    flags2 = FILE_SHARE_READ ;
  }
  else if ( isWrite ) {
    /*gHyp_util_debug("Opening write-only");*/
    flags = GENERIC_WRITE ;
    flags2 = (FILE_SHARE_WRITE | FILE_SHARE_READ );
  }
  else {
    return INVALID_HANDLE_VALUE ;
  }

    s = CreateFile( 
#ifdef AS_PPC 
  (const unsigned short*)
#endif
		  path, 
                  flags,  
                  flags2,
                  NULL,                        
                  OPEN_EXISTING,              
                  (FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED),  
                  NULL);          

  if ( s == INVALID_HANDLE_VALUE) {
    if ( GetLastError() != ERROR_FILE_NOT_FOUND )
      gHyp_util_sysError ( "Failed to open mailslot '%s'", path ) ;
    return INVALID_HANDLE_VALUE ;  
  }

#else

  /* UNIX and VMS */

  if ( isRead && isWrite ) {
    /*gHyp_util_debug("Opening read-write");*/
    flags = ( O_RDWR | O_NONBLOCK ) ;
  }
  else if ( isRead ) {
    /*gHyp_util_debug("Opening read-only");*/
    flags = ( O_RDONLY | O_NONBLOCK ) ;
  }
  else if ( isWrite ) {
    /*gHyp_util_debug("Opening write-only");*/
    flags = ( O_WRONLY | O_NONBLOCK ) ;
  }
  else {
    /*gHyp_util_debug("Invalid args");*/
    return INVALID_HANDLE_VALUE ;
  }
  
#ifdef AS_UNIX
  /* Check to see if fifo already exists */
  if ( stat ( path, &buf ) == -1 || !S_ISFIFO ( buf.st_mode ) )  {
    /* Not a fifo, delete it */
    /*gHyp_util_logInfo ( "Removing '%s'",path ) ;*/
    unlink ( path ) ;
    return INVALID_HANDLE_VALUE ; 
  }
#endif

  if ( (s = open ( path, flags) ) == INVALID_HANDLE_VALUE ) {
    if ( errno != ENXIO )
      gHyp_util_sysError ( "Failed to open FIFO '%s'", path ) ;
#ifdef AS_UNIX
    /*gHyp_util_logInfo ( "Removing '%s'",path ) ;*/
    unlink ( path ) ;
#endif
    return INVALID_HANDLE_VALUE ;
  }


#ifndef AS_VMS

#if defined ( F_SETFD )

  if ( fcntl ( s, F_SETFD, 1 ) == -1 ) {
    gHyp_util_sysError ( "Failed to F_SETFD (FD_CLOEXEC) on FIFO" ) ;
    return INVALID_HANDLE_VALUE ;
  }
#elif defined ( FIOCLEX )

  
  gHyp_tcp_setup ( (SOCKET) s ) ;

/*
#ifdef AS_WINDOWS
  if ( ioctlsocket ( s, FIOCLEX ) == -1 ) {
#else
  if ( ioctl ( s, FIOCLEX ) == -1 ) {
#endif
*/

    gHyp_util_sysError ( "Failed to FIOCLEX on FIFO" ) ;
    return INVALID_HANDLE_VALUE ;
  }
#endif
#endif
#endif

  return s ;
}
  
HANDLE gHyp_sock_fifo ( char *path, 
		        sLOGICAL create, 
		        sLOGICAL isRead, 
		        sLOGICAL isWrite,
			sLOGICAL alreadyExists ) 
{

#ifdef AS_WINDOWS
  SECURITY_ATTRIBUTES sa;
  SECURITY_DESCRIPTOR sd;
#endif

  HANDLE 
    s ;

#ifdef AS_UNIX

  /* UNIX */

  /* Make (or connect to an existing ) client fifo pipe */
  struct stat
    buf ;

  /* Check to see if fifo already exists */
  if ( stat ( path, &buf ) != -1 && S_ISFIFO ( buf.st_mode ) ) {
    /* Fifo already exists */
    if ( create && alreadyExists ) {
      gHyp_util_logError("Fifo %s already exists",path);
      return INVALID_HANDLE_VALUE ;
    }
    create = FALSE ;
  }
  else {
    /* Not a fifo, delete it */
    /*gHyp_util_logInfo ( "Removing '%s'",path ) ;*/
    unlink ( path ) ;
  }

  if ( create ) {

    gHyp_util_logInfo("Making Fifo %s",path);

    /* FIFO does not exist - create it */
    if ( mkfifo ( path, ( S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH ) ) == -1 ) {
      gHyp_util_sysError ( "Failed to create  FIFO '%s'", path ) ;
      return INVALID_HANDLE_VALUE ;
    }
  }

  /* Now open it */
  return lHyp_sock_openFifo ( path, isRead, isWrite ) ;

#elif defined ( AS_WINDOWS )

  /* Windows */
  if ( create ) {

    gHyp_util_logInfo("Making Mailslot %s",path);

    InitializeSecurityDescriptor( 
      &sd,
      SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl( &sd, TRUE, NULL, FALSE );

    sa.nLength= sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = FALSE;
    sa.lpSecurityDescriptor = &sd;

    s = CreateMailslot( 
#ifdef AS_PPC 
  (const unsigned short*)
#endif
			path, 
                        MAX_MESSAGE_SIZE,
                        MAILSLOT_WAIT_FOREVER,/* no time-out for operations */
                        &sa );                /* security attributes */
    
    if ( s == INVALID_HANDLE_VALUE ) { 
      gHyp_util_sysError ( "Failed to create mailslot '%s'", path ) ;
      return INVALID_HANDLE_VALUE ;  
    }
    else {
#ifdef AS_ATL
	/*	Notify CHyperScript2::Eval that the mailslot inBox has been opened */
	  gHyp_notify_client_mailslot ( ) ;
#endif /* AS_ATL */     
      return s ;
	}
  }
  else
    /* Open existing mailslot */
    return lHyp_sock_openFifo ( path, isRead, isWrite ) ;


#else

  /* VMS */
  char                  buffer[80];
  makeDSCs              ( buffer_d, buffer ) ;
  makeDSCz              ( client_d, path ) ;
  int                   status,
                        bufferLen = 0 ;
  unsigned long int prot = 0x0000ff00;
  int maxmsg ;
  int bufquo ;

  if ( create && !(guRunFlags & RUN_ROOT) ) create = FALSE ;

  /* Create or reconnect to the mailbox channel in which to send messages */
  lib$sys_trnLog ( &client_d, &bufferLen, &buffer_d, 0,0 ) ;
  buffer[bufferLen] = '\0' ;
  if ( strcmp ( buffer, path ) ) {

    /*gHyp_util_debug ( "MBX '%s' already exists", path ) ;*/
    s = 0 ;
    status = sys$assign ( &client_d, (unsigned short*)&s, 0, 0, 0 ) ;
    if ( !gHyp_util_check ( status, 1 ) ) {
      gHyp_util_logError ( "Failed to assign socket to MBX '%s'", path ) ;
      s = INVALID_HANDLE_VALUE ;
    }
  }
  else if ( create ) {

    s = 0 ;
    /*gHyp_util_debug("Creating MBX '%s'",path) ;*/
    
    status = sys$crembx( (char)0,
                         (unsigned short int*) &s,
                         (unsigned int)MAX_MESSAGE_SIZE,
                         (unsigned int)(MAX_MESSAGE_SIZE * MAX_INSTANCE_MESSAGES),
                         (unsigned int)0,
                         (unsigned int)PSL$C_USER,
                         (void*)&client_d ) ;

    if ( !gHyp_util_check ( status, 1 ) ) {
      gHyp_util_logError ( "Failed to create MBX '%s'", path ) ;
      s = INVALID_HANDLE_VALUE ;
    }

    /******
    maxmsg = MAX_MESSAGE_SIZE ;
    bufquo = MAX_MESSAGE_SIZE*MAX_INSTANCE_MESSAGES ;
    status = aeqSsp_autoMan_crembx ( 
      &client_d, 
      &maxmsg, 
      &bufquo, 
      &s );

    if ( !status ) {
      gHyp_util_logError ( "Failed to create MBX '%s'", path ) ;
      s = INVALID_HANDLE_VALUE ;
    }
    *******/
  }
  else
    s = INVALID_HANDLE_VALUE ;
#endif

  return s ;
}

sLOGICAL gHyp_sock_mkdir ( char *path )
{
#ifdef AS_UNIX
        /* Make (or connect to an existing ) client fifo pipe */
  struct stat  
    buf ;  

  /* Check to see if fifo already exists - create if necessarry */
  if (  stat ( path, &buf ) != -1 && S_ISDIR ( buf.st_mode ) ) 
    return TRUE ;
  else {
    /*gHyp_util_logInfo ( "Removing '%s'",path ) ;*/
    unlink ( path ) ;
  }

  if ( mkdir ( path, ( S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IROTH ) ) == -1 )
    return gHyp_util_sysError ( "Failed to create path '%s'", path ) ;
  else
    return TRUE ;
#else
  return TRUE ;
#endif
}

#ifdef AS_SSL

/* Print SSL errors and exit*/
static sLOGICAL lHyp_sock_sslError ( char *string )
{
  char 
    *pError,
    sslErrorBuf[SSL_ERROR_BUF_SIZE+1] ;
    
  int 
    sslErrno ;

  gHyp_util_logError ( "%s", string ) ;
  while ( (sslErrno = ERR_get_error()) != 0 ) {
     
    pError = ERR_error_string( sslErrno, sslErrorBuf ) ;

    gHyp_util_logError ( "%s", pError ) ;
  }
  return FALSE ;

}

static int lHyp_sock_verify_callback ( int ok, X509_STORE_CTX *ctx )
{
  char 
    *s,
    buf[256];

  X509
    *err_cert;

  int
    err,
    depth ;

  
  /* This callback routine is called so that we can have
   * control of certificate validation.
   *
   * This routine is a 'work-in-progress', there are
   * many ctx-error types that would otherwise cause
   * a reject of the certificate, we have but a few
   * cases for now.
   *
   */

  err_cert = X509_STORE_CTX_get_current_cert(ctx);
  err = X509_STORE_CTX_get_error(ctx);
  depth = X509_STORE_CTX_get_error_depth(ctx);

  s = X509_NAME_oneline ( X509_get_subject_name(ctx->current_cert), buf, 256 );

  if ( s != NULL ) {

    if (ok)
      gHyp_util_debug("OK: depth=%d %s\n",ctx->error_depth,buf);
    else
      gHyp_util_debug("NOT OK: depth=%d error=%d %s\n",
		ctx->error_depth,ctx->error,buf);
  }

  if ( ok == 0 ) {
		
    switch (ctx->error) {

      case X509_V_ERR_CERT_NOT_YET_VALID:  /* 9 */
	gHyp_util_logWarning("Certificate not yet valid");
	break ;

      case X509_V_ERR_CERT_HAS_EXPIRED: /* 10 */
	gHyp_util_logWarning("Certificate has expired");
	break ;

      case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT: /* 18 */
	gHyp_util_logWarning("Depth zero self signed cert");
	break ;

      case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY: /* 20 */

	gHyp_util_logWarning("Unable to get issuer cert locally");

	X509_NAME_oneline(
	  X509_get_issuer_name(ctx->current_cert), buf, 256);
	gHyp_util_logInfo("Issuer name is %s",buf);

	break ;

      case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE: /* 21 */

	gHyp_util_logWarning("Unable to verify leaf signature");
	break ;

      case X509_V_ERR_CERT_UNTRUSTED: /* 27 */
	gHyp_util_logWarning("Cert untrusted");

	break ;

    }
  }

  if ( ok ) gHyp_util_logInfo ("SSL CERTIFICATE is OK");
  return (ok);
}

int gHyp_sock_password_cb(char *buf, int num, int rwflag, void *userdata )
{
  /* A password callback routine. 
   *
   *
   */
  if ( num < (int) strlen(gzPass)+1 ) return 0;
  strcpy(buf,gzPass);
  gHyp_util_logInfo("SSL password check of %s",buf );

  return (strlen(gzPass)) ;
}

void *gHyp_sock_ctxInit( char *certFile, char *keyFile, char *password )
{
  SSL_METHOD *meth;
  SSL_CTX *ctx;
  
  /* Global system initialization*/
  if ( !guSSLinitialized ) {

    gHyp_util_logInfo("Initializing SSL");
    RAND_seed(gzRandomSeed, sizeof gzRandomSeed);
    SSL_library_init();
    SSL_load_error_strings();
    SSLeay_add_ssl_algorithms();
    OpenSSL_add_all_digests();

    /*meth = SSLv23_server_method();*/

    guSSLinitialized = TRUE ;
  }

  /* Create our context
   *
   * Optional:
   * - SSLv2_method for SSL2
   * - SSLv3_method for SSL3
   * - TLSv1_method for TLS1
   */
  meth = (SSL_METHOD *) SSLv23_method ();
  ctx  = SSL_CTX_new ( meth );

  if ( ctx == NULL ) {
    lHyp_sock_sslError ( "Failed to create SSL context" ) ;
    return NULL ;
  }

  /* SSL servers always have certificate files to present to the
   * client, clients sometimes do, sometimes do not.
   */
  if ( !certFile ) return (void*) ctx ;

  /* First see if the certificate file is valid */
  if ( !SSL_CTX_use_certificate_file( ctx,
				      certFile,
				      SSL_FILETYPE_PEM ) ) {
    lHyp_sock_sslError("No certificate in PEM file");
    return NULL ;
  }
  
  /* Next, sometimes the keyfile is protected by a password,
   * if so, we store the password in a temporary location, and
   * specify a callback routine that will be called when its time
   * to validate the password.
   */
  if ( password ) {
    strcpy (  gzPass, password ) ;
    SSL_CTX_set_default_passwd_cb ( ctx, gHyp_sock_password_cb ) ;
  }

  /* The keyfile contains the private key used to establish
   * the SSL session.
   */
  if ( keyFile ) {
    if ( !(SSL_CTX_use_PrivateKey_file (  ctx,
					  keyFile,
					  SSL_FILETYPE_PEM ) ) ) {
      lHyp_sock_sslError("Cannot read key file");
      return NULL ;
    }
  }

  return (void*) ctx ;

}
    
void gHyp_sock_destroyCTX ( void *ctx )
{
  SSL_CTX_free( (SSL_CTX*) ctx);
}

void gHyp_sock_deleteSSL ( sSSL *pSSL ) 
{  
  SSL_free(pSSL->ssl);          
  BIO_free(pSSL->outBio);  
  /*
  BIO_free(pSSL->filterBio);
  */
  ReleaseMemory ( pSSL ) ;
}

/*
 * HOW-TO SESSION RESUMPTION
 *
 * // A, Client SSL connection to socket h1.
 * //    It must save its session handle.
 *. 
 * handle hSSL = ssl_new() ;
 * handle sessionSSL == ssl_getSession ( hSSL ) ;
 * ssl_assign ( hSSL, h1 );
 *
 * // B. Second client SSL connection to socket h2
 * //    It uses the previous session handle
 *
 * handle hSSL2 = ssl_new() ;
 * ssl_assign ( hSSL2, h2 ) ;
 * ssl_setSession ( hSSL2, sessionSSL ) ;
 * ssl_assign ( hSSL2, h2 ) ;
 *
 * // At sever end, it must activate session caching
 *
 * handle h_listen = http_service ( 443 ) ;
 * handle hSSL_server = ssl_new()
 * ssl_assign ( hSSL_server, handle_listen ) ;
 * int context = 1 ;  // Any number will do.
 * ssl_enableSessions ( hSSL_server ) ;
 *
 */

void gHyp_sock_setSession ( void *ctx, void *session  ) 
{    
  SSL_set_session ( ctx, session ) ;
}

void *gHyp_sock_getSession ( void *ctx ) 
{  
  return SSL_get1_session ( ctx ) ;
}

static int s_server_session_id_context = 1 ;
void gHyp_sock_enableSessions ( void *ctx ) 
{  
   SSL_CTX_set_session_id_context(
      ctx,
      (void*)&s_server_session_id_context,
      sizeof s_server_session_id_context);
}

void gHyp_sock_setSessionObject ( sSSL *pSSL, sData *pSessionData )
{

  long
    sessionLen ;

  SSL_SESSION
    *session ;

  const unsigned char
    *sessionBytes ;

  if ( !pSSL ) return ;

  /* Get the existing session object */
  session = pSSL->ssl->session ;

  sessionBytes = gHyp_data_buffer ( pSessionData, 0 ) ;
  sessionLen = gHyp_data_bufferLen ( pSessionData, 0 ) ;

  /*gHyp_util_debug("d2i_SSL_SESSION ( %d bytes )", sessionLen ) ;*/
  session = d2i_SSL_SESSION( &session, &sessionBytes, sessionLen );

  return ;

}

sData *gHyp_sock_getSessionObject ( sSSL* pSSL ) 
{
  int 
    sessionLen ;

  SSL_SESSION 
    *session ;
  
  sBYTE
    *sessionBytes ;

  sData
    *pData = NULL ;

  if ( !pSSL ) return NULL ;

  session = pSSL->ssl->session ;

  sessionLen = i2d_SSL_SESSION( session, NULL ) ;

  if ( sessionLen > 0 ) {

    pData = gHyp_data_new ( "ASN1_SESSION" ) ;
    gHyp_data_newVector ( pData, TYPE_BINARY, sessionLen, FALSE ) ;
    sessionBytes = gHyp_data_buffer ( pData, 0 ) ;
    sessionLen = i2d_SSL_SESSION( session, &sessionBytes );
  }

  return pData ;
}


sData *gHyp_sock_getSSLstate( sSSL *pSSL  )
{
  /* Create a SSL state */

  sData
    *pParent,
    *pChild,
    *pData,
    *pValue ;

  SSL
    *ssl = NULL ;

  EVP_CIPHER 
    *evp_cipher = NULL;

  EVP_CIPHER_CTX 
    *evp_cipher_ctx = NULL;

  EVP_MD 
    *evp_md = NULL ;

  EVP_MD_CTX
    *evp_digest = NULL ;

  SSL3_BUFFER 
    *ssl3_buf = NULL;  /* s3->rbuf and s3->wbuf */

  SSL3_RECORD 
    *ssl3_rec = NULL;  /* s3->rrec and s3->wrec */

  SSL_SESSION 
    *session = NULL;

  SSL3_STATE 
    *ssl3_state = NULL ;

  /* We are building a structure like this:

    list ssl = {
      int version = 2,
      int type = 3,

  */

  ssl = pSSL->ssl ;

  pParent = gHyp_data_new ( "ssl" ) ;

  pChild = gHyp_data_new ( "version" ) ;
  gHyp_data_newVectorSrc ( pChild, TYPE_INTEGER, 1, FALSE,  &ssl->version ) ;
  gHyp_data_append ( pParent, pChild ) ;

  pChild = gHyp_data_new ( "type" ) ;
  gHyp_data_newVectorSrc ( pChild, TYPE_INTEGER, 1, FALSE,  &ssl->type ) ;
  gHyp_data_append ( pParent, pChild ) ;

  /* Skip method, these are standard */
  /* Skip rbio,wbio,bbio, these are empty or null */

  pChild = gHyp_data_new ( "rwstate" ) ;
  gHyp_data_newVectorSrc ( pChild, TYPE_INTEGER, 1, FALSE,  &ssl->rwstate ) ;
  gHyp_data_append ( pParent, pChild ) ;

  pChild = gHyp_data_new ( "in_handshake" ) ;
  gHyp_data_newVectorSrc ( pChild, TYPE_INTEGER, 1, FALSE,  &ssl->in_handshake ) ;
  gHyp_data_append ( pParent, pChild ) ;

  /* handshake_func is default ssl3_accept(ssl_st*) */

  pChild = gHyp_data_new ( "server" ) ;
  gHyp_data_newVectorSrc ( pChild, TYPE_INTEGER, 1, FALSE,  &ssl->server ) ;
  gHyp_data_append ( pParent, pChild ) ;

  pChild = gHyp_data_new ( "new_session" ) ;
  gHyp_data_newVectorSrc ( pChild, TYPE_INTEGER, 1, FALSE,  &ssl->new_session ) ;
  gHyp_data_append ( pParent, pChild ) ;

  pChild = gHyp_data_new ( "quiet_shutdown" ) ;
  gHyp_data_newVectorSrc ( pChild, TYPE_INTEGER, 1, FALSE,  &ssl->quiet_shutdown ) ;
  gHyp_data_append ( pParent, pChild ) ;

  pChild = gHyp_data_new ( "shutdown" ) ;
  gHyp_data_newVectorSrc ( pChild, TYPE_INTEGER, 1, FALSE,  &ssl->shutdown ) ;
  gHyp_data_append ( pParent, pChild ) ;

  pChild = gHyp_data_new ( "state" ) ;
  gHyp_data_newVectorSrc ( pChild, TYPE_INTEGER, 1, FALSE,  &ssl->state ) ;
  gHyp_data_append ( pParent, pChild ) ;

  pChild = gHyp_data_new ( "rstate" ) ;
  gHyp_data_newVectorSrc ( pChild, TYPE_INTEGER, 1, FALSE,  &ssl->rstate ) ;
  gHyp_data_append ( pParent, pChild ) ;

  /* init_buf seems to be null, but here's how to initialize it, if it needs to be available */
  /****************
  BUF_MEM *init_buf;	* buffer used during init *
  void *init_msg;	* pointer to handshake message body, set by ssl3_get_message() *
  int init_num;		* amount read/written *
  int init_off;		* amount read/written *
  *******************/

  /* packet and packet_len seem to be temp variables.
   * packet_len can be zero, in which case we don't care about it.
   */

  /* The s2 structure is not used.  It is NULL */

  /* The SSL3_STATE s3 structure is important */
  pChild = gHyp_data_new ( "s3" ) ;

  ssl3_state = ssl->s3 ;
  
  if ( ssl3_state ) {

    pData = gHyp_data_new ( "flags" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &ssl3_state->flags ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "delay_buf_pop_ret" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &ssl3_state->delay_buf_pop_ret ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "read_sequence" ) ;
    gHyp_data_newVectorSrc (  pData, TYPE_BINARY, sizeof(ssl3_state->read_sequence), 
			    FALSE,  &ssl3_state->read_sequence ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "read_mac_secret_size" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &ssl3_state->read_mac_secret_size ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "read_mac_secret" ) ;
    gHyp_data_newVectorSrc (  pData, TYPE_BINARY, sizeof(ssl3_state->read_mac_secret), 
			    FALSE,  &ssl3_state->read_mac_secret ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "write_sequence" ) ;
    gHyp_data_newVectorSrc (  pData, TYPE_BINARY, sizeof(ssl3_state->write_sequence), 
			    FALSE,  &ssl3_state->write_sequence ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "write_mac_secret_size" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &ssl3_state->write_mac_secret_size ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "write_mac_secret" ) ;
    gHyp_data_newVectorSrc (  pData, TYPE_BINARY, sizeof(ssl3_state->write_mac_secret), 
			    FALSE,  &ssl3_state->write_mac_secret ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "server_random" ) ;
    gHyp_data_newVectorSrc (  pData, TYPE_BINARY, sizeof(ssl3_state->server_random), 
			    FALSE,  &ssl3_state->server_random ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "client_random" ) ;
    gHyp_data_newVectorSrc (  pData, TYPE_BINARY, sizeof(ssl3_state->client_random), 
			    FALSE,  &ssl3_state->client_random ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "need_empty_fragments" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &ssl3_state->need_empty_fragments ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "empty_fragment_done" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &ssl3_state->empty_fragment_done ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "init_extra" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &ssl3_state->init_extra ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "rbuf" ) ;

    ssl3_buf = &ssl3_state->rbuf ;

    if ( ssl3_buf ) {

      pValue = gHyp_data_new ( "buf" ) ;
      gHyp_data_newVectorSrc (  pValue, TYPE_BINARY, ssl3_buf->left, 
			    FALSE,  &ssl3_buf->buf+ssl3_buf->offset ) ;
      gHyp_data_append ( pData, pValue ) ;

      pValue = gHyp_data_new ( "len" ) ;
      gHyp_data_newVectorSrc ( pValue, TYPE_INTEGER, 1, FALSE,  &ssl3_buf->len ) ;
      gHyp_data_append ( pData, pValue ) ;

      pValue = gHyp_data_new ( "offset" ) ;
      gHyp_data_newVectorSrc ( pValue, TYPE_INTEGER, 1, FALSE,  &ssl3_buf->offset ) ;
      gHyp_data_append ( pData, pValue ) ;

      pValue = gHyp_data_new ( "left" ) ;
      gHyp_data_newVectorSrc ( pValue, TYPE_INTEGER, 1, FALSE,  &ssl3_buf->left ) ;
      gHyp_data_append ( pData, pValue ) ;
    }
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "wbuf" ) ;
    ssl3_buf = &ssl3_state->wbuf ;

    if ( ssl3_buf ) {

      pValue = gHyp_data_new ( "buf" ) ;
      gHyp_data_newVectorSrc (  pValue, TYPE_BINARY, ssl3_buf->left, 
			    FALSE,  &ssl3_buf->buf+ssl3_buf->offset ) ;
      gHyp_data_append ( pData, pValue ) ;

      pValue = gHyp_data_new ( "len" ) ;
      gHyp_data_newVectorSrc ( pValue, TYPE_INTEGER, 1, FALSE,  &ssl3_buf->len ) ;
      gHyp_data_append ( pData, pValue ) ;

      pValue = gHyp_data_new ( "offset" ) ;
      gHyp_data_newVectorSrc ( pValue, TYPE_INTEGER, 1, FALSE,  &ssl3_buf->offset ) ;
      gHyp_data_append ( pData, pValue ) ;

      pValue = gHyp_data_new ( "left" ) ;
      gHyp_data_newVectorSrc ( pValue, TYPE_INTEGER, 1, FALSE,  &ssl3_buf->left ) ;
      gHyp_data_append ( pData, pValue ) ;
    }
    gHyp_data_append ( pChild, pData ) ;


    pData = gHyp_data_new ( "rrec" ) ;
    ssl3_rec = &ssl3_state->rrec ;

    if ( ssl3_rec ) {

      pValue = gHyp_data_new ( "data" ) ;
      gHyp_data_newVectorSrc (  pValue, TYPE_BINARY, ssl3_rec->length, 
			    FALSE,  &ssl3_rec->data+ssl3_rec->off ) ;
      gHyp_data_append ( pData, pValue ) ;

      pValue = gHyp_data_new ( "type" ) ;
      gHyp_data_newVectorSrc ( pValue, TYPE_INTEGER, 1, FALSE,  &ssl3_rec->type ) ;
      gHyp_data_append ( pData, pValue ) ;

      pValue = gHyp_data_new ( "length" ) ;
      gHyp_data_newVectorSrc ( pValue, TYPE_INTEGER, 1, FALSE,  &ssl3_rec->length ) ;
      gHyp_data_append ( pData, pValue ) ;

      pValue = gHyp_data_new ( "off" ) ;
      gHyp_data_newVectorSrc ( pValue, TYPE_INTEGER, 1, FALSE,  &ssl3_rec->off ) ;
      gHyp_data_append ( pData, pValue ) ;

      pValue = gHyp_data_new ( "epoch" ) ;
      gHyp_data_newVectorSrc ( pValue, TYPE_INTEGER, 1, FALSE,  &ssl3_rec->epoch ) ;
      gHyp_data_append ( pData, pValue ) ;
    }
    gHyp_data_append ( pChild, pData ) ;


    pData = gHyp_data_new ( "wrec" ) ;
    ssl3_rec = &ssl3_state->wrec ;

    if ( ssl3_rec ) {

      pValue = gHyp_data_new ( "data" ) ;
      gHyp_data_newVectorSrc (  pValue, TYPE_BINARY, ssl3_rec->length, 
			    FALSE,  &ssl3_rec->data+ssl3_rec->off ) ;
      gHyp_data_append ( pData, pValue ) ;

      pValue = gHyp_data_new ( "type" ) ;
      gHyp_data_newVectorSrc ( pValue, TYPE_INTEGER, 1, FALSE,  &ssl3_rec->type ) ;
      gHyp_data_append ( pData, pValue ) ;

      pValue = gHyp_data_new ( "length" ) ;
      gHyp_data_newVectorSrc ( pValue, TYPE_INTEGER, 1, FALSE,  &ssl3_rec->length ) ;
      gHyp_data_append ( pData, pValue ) ;

      pValue = gHyp_data_new ( "off" ) ;
      gHyp_data_newVectorSrc ( pValue, TYPE_INTEGER, 1, FALSE,  &ssl3_rec->off ) ;
      gHyp_data_append ( pData, pValue ) ;

      pValue = gHyp_data_new ( "epoch" ) ;
      gHyp_data_newVectorSrc ( pValue, TYPE_INTEGER, 1, FALSE,  &ssl3_rec->epoch ) ;
      gHyp_data_append ( pData, pValue ) ;
    }
    gHyp_data_append ( pChild, pData ) ;


    pData = gHyp_data_new ( "alert_fragment_len" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &ssl3_state->alert_fragment_len ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "alert_fragment" ) ;
    gHyp_data_newVectorSrc (  pData, TYPE_BINARY, sizeof(ssl3_state->alert_fragment), 
			    FALSE,  &ssl3_state->alert_fragment ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "handshake_fragment_len" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &ssl3_state->handshake_fragment_len ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "handshake_fragment" ) ;
    gHyp_data_newVectorSrc (  pData, TYPE_BINARY, sizeof(ssl3_state->handshake_fragment), 
			    FALSE,  &ssl3_state->handshake_fragment ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "wnum" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &ssl3_state->wnum ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "wpend_tot" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &ssl3_state->wpend_tot ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "wpend_type" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &ssl3_state->wpend_type ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "wpend_ret" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &ssl3_state->wpend_ret ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "wpend_buf" ) ;
    gHyp_data_newVectorSrc (  pData, TYPE_BINARY, (int) ssl3_state->wpend_tot, 
			    FALSE,  (void*) &ssl3_state->wpend_buf ) ;
    gHyp_data_append ( pChild, pData ) ;

    evp_digest = (EVP_MD_CTX*) *(ssl3_state->handshake_dgst) ;

    if ( evp_digest ) {

      pData = gHyp_data_new ( "handshake_dgst" ) ;
      evp_md = (EVP_MD*) evp_digest->digest ;

      if ( evp_md ) {

        pValue = gHyp_data_new ( "type" ) ;
        gHyp_data_newVectorSrc ( pValue, TYPE_INTEGER, 1, FALSE,  &evp_md->type ) ;
        gHyp_data_append ( pData, pValue ) ;
    
        pValue = gHyp_data_new ( "pkey_type" ) ;
        gHyp_data_newVectorSrc ( pValue, TYPE_INTEGER, 1, FALSE,  &evp_md->pkey_type ) ;
        gHyp_data_append ( pData, pValue ) ;

        pValue = gHyp_data_new ( "md_size" ) ;
        gHyp_data_newVectorSrc ( pValue, TYPE_INTEGER, 1, FALSE,  &evp_md->md_size ) ;
        gHyp_data_append ( pData, pValue ) ;

        pValue = gHyp_data_new ( "flags" ) ;
        gHyp_data_newVectorSrc ( pValue, TYPE_INTEGER, 1, FALSE,  &evp_md->flags ) ;
        gHyp_data_append ( pData, pValue ) ;

        pValue = gHyp_data_new ( "required_pkey_type" ) ;
        gHyp_data_newVectorSrc ( pValue, TYPE_BINARY, sizeof(evp_md->required_pkey_type), FALSE,  &evp_md->required_pkey_type ) ;
        gHyp_data_append ( pData, pValue ) ;

        pValue = gHyp_data_new ( "block_size" ) ;
        gHyp_data_newVectorSrc ( pValue, TYPE_INTEGER, 1, FALSE,  &evp_md->block_size ) ;
        gHyp_data_append ( pData, pValue ) ;

        pValue = gHyp_data_new ( "ctx_size" ) ;
        gHyp_data_newVectorSrc ( pValue, TYPE_INTEGER, 1, FALSE,  &evp_md->ctx_size ) ;
        gHyp_data_append ( pData, pValue ) ;
      }
      gHyp_data_append ( pChild, pData ) ;

      pData = gHyp_data_new ( "engine" ) ;
      gHyp_data_newVectorSrc ( pData, TYPE_BINARY, 1, FALSE,  &evp_digest->engine ) ;
      gHyp_data_append ( pChild, pData ) ;

      pData = gHyp_data_new ( "flags" ) ;
      gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &evp_digest->flags ) ;
      gHyp_data_append ( pChild, pData ) ;

      /********  md_data is not NULL */
      pData = gHyp_data_new ( "md_data" ) ;
      gHyp_data_newVectorSrc ( pData, TYPE_BINARY, evp_md->md_size, FALSE, evp_digest->md_data ) ;
      gHyp_data_append ( pChild, pData ) ;

    }

    /* Missing 
      change_cipher_spec
      warn_alert
      fatal_alert
      alert_dispatch
      send_alert
      renegotiate
      total_renegotations
      num_renegotiations
    */

    pData = gHyp_data_new ( "in_read_app_data" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &ssl3_state->in_read_app_data ) ;
    gHyp_data_append ( pChild, pData ) ;


    pData = gHyp_data_new ( "previous_client_finished_len" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_BYTE, 1, FALSE,  &ssl3_state->previous_client_finished_len ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "previous_client_finished" ) ;
    gHyp_data_newVectorSrc (  pData, TYPE_BINARY, sizeof(ssl3_state->previous_client_finished), 
			    FALSE,  &ssl3_state->previous_client_finished ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "previous_server_finished_len" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_BYTE, 1, FALSE,  &ssl3_state->previous_server_finished_len ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "previous_server_finished" ) ;
    gHyp_data_newVectorSrc (  pData, TYPE_BINARY, sizeof(ssl3_state->previous_server_finished), 
			    FALSE,  &ssl3_state->previous_server_finished ) ;
    gHyp_data_append ( pChild, pData ) ;

    /* Let's hope "tmp" is not needed, if so there's a bunch of stuff
     * cert_verify_md, finish_md, peer_finish_md, etc..... 
     *
     *
     */

    pData = gHyp_data_new ( "send_connection_binding" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &ssl3_state->send_connection_binding ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "next_proto_neg_seen" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &ssl3_state->next_proto_neg_seen ) ;
    gHyp_data_append ( pChild, pData ) ;
  }

  gHyp_data_append ( pParent, pChild ) ;

  /* We don't need "d1" */

  pChild = gHyp_data_new ( "read_ahead" ) ;
  gHyp_data_newVectorSrc ( pChild, TYPE_INTEGER, 1, FALSE,  &ssl->read_ahead ) ;
  gHyp_data_append ( pParent, pChild ) ;

  pChild = gHyp_data_new ( "hit" ) ;
  gHyp_data_newVectorSrc ( pChild, TYPE_INTEGER, 1, FALSE,  &ssl->hit ) ;
  gHyp_data_append ( pParent, pChild ) ;

  pChild = gHyp_data_new ( "mac_flags" ) ;
  gHyp_data_newVectorSrc ( pChild, TYPE_INTEGER, 1, FALSE,  &ssl->mac_flags ) ;
  gHyp_data_append ( pParent, pChild ) ;

#if 0
  pChild = gHyp_data_new ( "purpose" ) ;
  gHyp_data_newVectorSrc ( pChild, TYPE_INTEGER, 1, FALSE,  &ssl->purpose ) ;
  gHyp_data_append ( pParent, pChild ) ;

  pChild = gHyp_data_new ( "trust" ) ;
  gHyp_data_newVectorSrc ( pChild, TYPE_INTEGER, 1, FALSE,  &ssl->trust ) ;
  gHyp_data_append ( pParent, pChild ) ;
#endif
  
  pChild = gHyp_data_new ( "enc_read_ctx" ) ;
  evp_cipher_ctx = ssl->enc_read_ctx ;

  if ( evp_cipher_ctx ) {

    pData = gHyp_data_new ( "cipher" ) ;
    evp_cipher = (EVP_CIPHER*) evp_cipher_ctx->cipher ;

    if ( evp_cipher ) {

      pValue = gHyp_data_new ( "nid" ) ;
      gHyp_data_newVectorSrc ( pValue, TYPE_INTEGER, 1, FALSE,  &evp_cipher->nid ) ;
      gHyp_data_append ( pData, pValue ) ;

      pValue = gHyp_data_new ( "block_size" ) ;
      gHyp_data_newVectorSrc ( pValue, TYPE_INTEGER, 1, FALSE,  &evp_cipher->block_size ) ;
      gHyp_data_append ( pData, pValue ) ;

      pValue = gHyp_data_new ( "key_len" ) ;
      gHyp_data_newVectorSrc ( pValue, TYPE_INTEGER, 1, FALSE,  &evp_cipher->key_len ) ;
      gHyp_data_append ( pData, pValue ) ;

      pValue = gHyp_data_new ( "iv_len" ) ;
      gHyp_data_newVectorSrc ( pValue, TYPE_INTEGER, 1, FALSE,  &evp_cipher->iv_len ) ;
      gHyp_data_append ( pData, pValue ) ;

      pValue = gHyp_data_new ( "flags" ) ;
      gHyp_data_newVectorSrc ( pValue, TYPE_INTEGER, 1, FALSE,  &evp_cipher->flags ) ;
      gHyp_data_append ( pData, pValue ) ;

      pValue = gHyp_data_new ( "ctx_size" ) ;
      gHyp_data_newVectorSrc ( pValue, TYPE_INTEGER, 1, FALSE,  &evp_cipher->ctx_size ) ;
      gHyp_data_append ( pData, pValue ) ;
    }
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "encrypt" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &evp_cipher_ctx->encrypt ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "buf_len" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &evp_cipher_ctx->buf_len ) ;
    gHyp_data_append ( pChild, pData ) ;
    
    pData = gHyp_data_new ( "oiv" ) ;
    gHyp_data_newVectorSrc (  pData, TYPE_BINARY, sizeof(evp_cipher_ctx->oiv), 
			    FALSE,  &evp_cipher_ctx->oiv ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "iv" ) ;
    gHyp_data_newVectorSrc (  pData, TYPE_BINARY, sizeof(evp_cipher_ctx->iv), 
			    FALSE,  &evp_cipher_ctx->iv ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "buf" ) ;
    gHyp_data_newVectorSrc (  pData, TYPE_BINARY, sizeof(evp_cipher_ctx->buf), 
			    FALSE,  &evp_cipher_ctx->buf ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "num" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &evp_cipher_ctx->num ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "key_len" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &evp_cipher_ctx->key_len ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "flags" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &evp_cipher_ctx->flags ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "cipher_data" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_BINARY, evp_cipher->ctx_size, FALSE, evp_cipher_ctx->cipher_data ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "final_used" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &evp_cipher_ctx->final_used ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "block_mask" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &evp_cipher_ctx->block_mask ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "final" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_BINARY, sizeof(evp_cipher_ctx->final), FALSE, &evp_cipher_ctx->final ) ;
    gHyp_data_append ( pChild, pData ) ;

  }
  gHyp_data_append ( pParent, pChild ) ;


  pChild = gHyp_data_new ( "read_hash" ) ;

  evp_digest = (EVP_MD_CTX*) ssl->read_hash ;

  if ( evp_digest ) {

    pData = gHyp_data_new ( "digest" ) ;
    evp_md = (EVP_MD*) ssl->read_hash->digest ;

    if ( evp_md ) {

      pValue = gHyp_data_new ( "type" ) ;
      gHyp_data_newVectorSrc ( pValue, TYPE_INTEGER, 1, FALSE,  &evp_md->type ) ;
      gHyp_data_append ( pData, pValue ) ;
    
      pValue = gHyp_data_new ( "pkey_type" ) ;
      gHyp_data_newVectorSrc ( pValue, TYPE_INTEGER, 1, FALSE,  &evp_md->pkey_type ) ;
      gHyp_data_append ( pData, pValue ) ;

      pValue = gHyp_data_new ( "md_size" ) ;
      gHyp_data_newVectorSrc ( pValue, TYPE_INTEGER, 1, FALSE,  &evp_md->md_size ) ;
      gHyp_data_append ( pData, pValue ) ;

      pValue = gHyp_data_new ( "flags" ) ;
      gHyp_data_newVectorSrc ( pValue, TYPE_INTEGER, 1, FALSE,  &evp_md->flags ) ;
      gHyp_data_append ( pData, pValue ) ;

      pValue = gHyp_data_new ( "required_pkey_type" ) ;
      gHyp_data_newVectorSrc ( pValue, TYPE_BINARY, sizeof(evp_md->required_pkey_type), FALSE,  &evp_md->required_pkey_type ) ;
      gHyp_data_append ( pData, pValue ) ;

      pValue = gHyp_data_new ( "block_size" ) ;
      gHyp_data_newVectorSrc ( pValue, TYPE_INTEGER, 1, FALSE,  &evp_md->block_size ) ;
      gHyp_data_append ( pData, pValue ) ;

      pValue = gHyp_data_new ( "ctx_size" ) ;
      gHyp_data_newVectorSrc ( pValue, TYPE_INTEGER, 1, FALSE,  &evp_md->ctx_size ) ;
      gHyp_data_append ( pData, pValue ) ;
    }
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "engine" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_BINARY, 1, FALSE,  &evp_digest->engine ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "flags" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &evp_digest->flags ) ;
    gHyp_data_append ( pChild, pData ) ;

  /********  md_data is usually NULL *
    pData = gHyp_data_new ( "md_data" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_BINARY, evp_md->md_size, FALSE, evp_digest->md_data ) ;
    gHyp_data_append ( pChild, pData ) ;
  *********/
  }
  gHyp_data_append ( pParent, pChild ) ;

  pChild = gHyp_data_new ( "enc_write_ctx" ) ;
  evp_cipher_ctx = ssl->enc_write_ctx ;

  if ( evp_cipher_ctx ) {

    pData = gHyp_data_new ( "cipher" ) ;
    evp_cipher = (EVP_CIPHER*) ssl->enc_write_ctx->cipher ;

    if ( evp_cipher ) {

      pValue = gHyp_data_new ( "nid" ) ;
      gHyp_data_newVectorSrc ( pValue, TYPE_INTEGER, 1, FALSE,  &evp_cipher->nid ) ;
      gHyp_data_append ( pData, pValue ) ;

      pValue = gHyp_data_new ( "block_size" ) ;
      gHyp_data_newVectorSrc ( pValue, TYPE_INTEGER, 1, FALSE,  &evp_cipher->block_size ) ;
      gHyp_data_append ( pData, pValue ) ;

      pValue = gHyp_data_new ( "key_len" ) ;
      gHyp_data_newVectorSrc ( pValue, TYPE_INTEGER, 1, FALSE,  &evp_cipher->key_len ) ;
      gHyp_data_append ( pData, pValue ) ;

      pValue = gHyp_data_new ( "iv_len" ) ;
      gHyp_data_newVectorSrc ( pValue, TYPE_INTEGER, 1, FALSE,  &evp_cipher->iv_len ) ;
      gHyp_data_append ( pData, pValue ) ;

      pValue = gHyp_data_new ( "flags" ) ;
      gHyp_data_newVectorSrc ( pValue, TYPE_INTEGER, 1, FALSE,  &evp_cipher->flags ) ;
      gHyp_data_append ( pData, pValue ) ;

      pValue = gHyp_data_new ( "ctx_size" ) ;
      gHyp_data_newVectorSrc ( pValue, TYPE_INTEGER, 1, FALSE,  &evp_cipher->ctx_size ) ;
      gHyp_data_append ( pData, pValue ) ;
    }
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "encrypt" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &evp_cipher_ctx->encrypt ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "buf_len" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &evp_cipher_ctx->buf_len ) ;
    gHyp_data_append ( pChild, pData ) ;
    
    pData = gHyp_data_new ( "oiv" ) ;
    gHyp_data_newVectorSrc (  pData, TYPE_BINARY, sizeof(evp_cipher_ctx->oiv), 
			    FALSE,  &evp_cipher_ctx->oiv ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "iv" ) ;
    gHyp_data_newVectorSrc (  pData, TYPE_BINARY, sizeof(evp_cipher_ctx->iv), 
			    FALSE,  &evp_cipher_ctx->iv ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "buf" ) ;
    gHyp_data_newVectorSrc (  pData, TYPE_BINARY, sizeof(evp_cipher_ctx->buf), 
			    FALSE,  &evp_cipher_ctx->buf ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "num" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &evp_cipher_ctx->num ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "key_len" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &evp_cipher_ctx->key_len ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "flags" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &evp_cipher_ctx->flags ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "cipher_data" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_BINARY, evp_cipher->ctx_size, FALSE, evp_cipher_ctx->cipher_data ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "final_used" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &evp_cipher_ctx->final_used ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "block_mask" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &evp_cipher_ctx->block_mask ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "final" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_BINARY, sizeof(evp_cipher_ctx->final), FALSE, &evp_cipher_ctx->final ) ;
    gHyp_data_append ( pChild, pData ) ;
  }
  gHyp_data_append ( pParent, pChild ) ;

  pChild = gHyp_data_new ( "write_hash" ) ;

  evp_digest = (EVP_MD_CTX*) ssl->write_hash ;

  if ( evp_digest ) {

    pData = gHyp_data_new ( "digest" ) ;
    evp_md = (EVP_MD*) ssl->write_hash->digest ;

    if ( evp_md ) {

      pValue = gHyp_data_new ( "type" ) ;
      gHyp_data_newVectorSrc ( pValue, TYPE_INTEGER, 1, FALSE,  &evp_md->type ) ;
      gHyp_data_append ( pData, pValue ) ;
    
      pValue = gHyp_data_new ( "pkey_type" ) ;
      gHyp_data_newVectorSrc ( pValue, TYPE_INTEGER, 1, FALSE,  &evp_md->pkey_type ) ;
      gHyp_data_append ( pData, pValue ) ;

      pValue = gHyp_data_new ( "md_size" ) ;
      gHyp_data_newVectorSrc ( pValue, TYPE_INTEGER, 1, FALSE,  &evp_md->md_size ) ;
      gHyp_data_append ( pData, pValue ) ;

      pValue = gHyp_data_new ( "flags" ) ;
      gHyp_data_newVectorSrc ( pValue, TYPE_INTEGER, 1, FALSE,  &evp_md->flags ) ;
      gHyp_data_append ( pData, pValue ) ;

      pValue = gHyp_data_new ( "required_pkey_type" ) ;
      gHyp_data_newVectorSrc ( pValue, TYPE_BINARY, sizeof(evp_md->required_pkey_type), FALSE,  &evp_md->required_pkey_type ) ;
      gHyp_data_append ( pData, pValue ) ;

      pValue = gHyp_data_new ( "block_size" ) ;
      gHyp_data_newVectorSrc ( pValue, TYPE_INTEGER, 1, FALSE,  &evp_md->block_size ) ;
      gHyp_data_append ( pData, pValue ) ;

      pValue = gHyp_data_new ( "ctx_size" ) ;
      gHyp_data_newVectorSrc ( pValue, TYPE_INTEGER, 1, FALSE,  &evp_md->ctx_size ) ;
      gHyp_data_append ( pData, pValue ) ;
    }
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "engine" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_BINARY, 1, FALSE,  &evp_digest->engine ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "flags" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &evp_digest->flags ) ;
    gHyp_data_append ( pChild, pData ) ;

  /********  md_data is usually NULL *
    pData = gHyp_data_new ( "md_data" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_BINARY, evp_md->md_size, FALSE, evp_digest->md_data ) ;
    gHyp_data_append ( pChild, pData ) ;
  *********/
  }
  gHyp_data_append ( pParent, pChild ) ;

  pChild = gHyp_data_new ( "sid_ctx_length" ) ;
  gHyp_data_newVectorSrc ( pChild, TYPE_INTEGER, 1, FALSE,  &ssl->sid_ctx_length ) ;
  gHyp_data_append ( pParent, pChild ) ;

  pChild = gHyp_data_new ( "sid_ctx" ) ;
  gHyp_data_newVectorSrc (  pChild, TYPE_BINARY, sizeof(ssl->sid_ctx), 
			    FALSE,  &ssl->sid_ctx ) ;
  gHyp_data_append ( pParent, pChild ) ;
    
  pChild = gHyp_data_new ( "session" ) ;
  session = (SSL_SESSION*) ssl->session ;
  if ( session ) {

    pData = gHyp_data_new ( "ssl_version" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &session->ssl_version ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "key_arg_length" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &session->key_arg_length ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "key_arg" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_BINARY, sizeof(session->key_arg), 
			    FALSE,  &session->key_arg ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "master_key_length" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &session->master_key_length ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "master_key" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_BINARY, sizeof(session->master_key), 
			    FALSE,  &session->master_key ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "session_id_length" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &session->session_id_length ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "session_id" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_BINARY, sizeof(session->session_id), 
			  FALSE,  &session->session_id ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "sid_ctx_length" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &session->sid_ctx_length ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "sid_ctx" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_BINARY, sizeof(session->sid_ctx),
			  FALSE,  &session->sid_ctx ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "not_resumable" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &session->not_resumable ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "verify_result" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &session->verify_result ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "references" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &session->references ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "timeout" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &session->timeout ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "time" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &session->time ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "compress_meth" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &session->compress_meth ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "cipher_id" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &session->cipher_id ) ;
    gHyp_data_append ( pChild, pData ) ;
  }
  gHyp_data_append ( pParent, pChild ) ;

  pChild = gHyp_data_new ( "error" ) ;
  gHyp_data_newVectorSrc ( pChild, TYPE_INTEGER, 1, FALSE,  &ssl->error ) ;
  gHyp_data_append ( pParent, pChild ) ;
  
  pChild = gHyp_data_new ( "error_code" ) ;
  gHyp_data_newVectorSrc ( pChild, TYPE_INTEGER, 1, FALSE,  &ssl->error_code ) ;
  gHyp_data_append ( pParent, pChild ) ;

  pChild = gHyp_data_new ( "debug" ) ;
  gHyp_data_newVectorSrc ( pChild, TYPE_INTEGER, 1, FALSE,  &ssl->debug ) ;
  gHyp_data_append ( pParent, pChild ) ;

  pChild = gHyp_data_new ( "verify_result" ) ;
  gHyp_data_newVectorSrc ( pChild, TYPE_INTEGER, 1, FALSE,  &ssl->verify_result ) ;
  gHyp_data_append ( pParent, pChild ) ;

  pChild = gHyp_data_new ( "references" ) ;
  gHyp_data_newVectorSrc ( pChild, TYPE_INTEGER, 1, FALSE,  &ssl->references ) ;
  gHyp_data_append ( pParent, pChild ) ;

  pChild = gHyp_data_new ( "options" ) ;
  gHyp_data_newVectorSrc ( pChild, TYPE_INTEGER, 1, FALSE,  &ssl->options ) ;
  gHyp_data_append ( pParent, pChild ) ;

  pChild = gHyp_data_new ( "mode" ) ;
  gHyp_data_newVectorSrc ( pChild, TYPE_INTEGER, 1, FALSE,  &ssl->mode ) ;
  gHyp_data_append ( pParent, pChild ) ;

  pChild = gHyp_data_new ( "client_version" ) ;
  gHyp_data_newVectorSrc ( pChild, TYPE_INTEGER, 1, FALSE,  &ssl->client_version ) ;
  gHyp_data_append ( pParent, pChild ) ;


  pChild = gHyp_data_new ( "inbio" ) ;
  {

    pData = gHyp_data_new ( "num_read" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &pSSL->inBio->num_read ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "num_write" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &pSSL->inBio->num_write ) ;
    gHyp_data_append ( pChild, pData ) ;

  }
  gHyp_data_append ( pParent, pChild ) ;

  pChild = gHyp_data_new ( "outbio" ) ;
  {

    pData = gHyp_data_new ( "num_read" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &pSSL->outBio->num_read ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "num_write" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &pSSL->outBio->num_write ) ;
    gHyp_data_append ( pChild, pData ) ;

  }
  gHyp_data_append ( pParent, pChild ) ;

  pChild = gHyp_data_new ( "filterbio" ) ;
  {
    pData = gHyp_data_new ( "num_read" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &pSSL->filterBio->num_read ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "num_write" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &pSSL->filterBio->num_write ) ;
    gHyp_data_append ( pChild, pData ) ;

  }
  gHyp_data_append ( pParent, pChild ) ;


  pChild = gHyp_data_new ( "nextbio" ) ;
  {
    pData = gHyp_data_new ( "num_read" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &pSSL->filterBio->next_bio->num_read ) ;
    gHyp_data_append ( pChild, pData ) ;

    pData = gHyp_data_new ( "num_write" ) ;
    gHyp_data_newVectorSrc ( pData, TYPE_INTEGER, 1, FALSE,  &pSSL->filterBio->next_bio->num_write ) ;
    gHyp_data_append ( pChild, pData ) ;

  }
  gHyp_data_append ( pParent, pChild ) ;

  pChild = gHyp_sock_getSessionObject ( pSSL ) ;
  gHyp_data_append ( pParent, pChild ) ;


  return pParent ;
}

sLOGICAL gHyp_sock_setSSLstate ( sSSL *pSSL, sData *pSSLdata, sSSL *pSSLORIG)
{

  sData
    *pData ;

  SSL
    *ssl ;

  EVP_CIPHER 
    *evp_cipher = NULL;

  EVP_CIPHER_CTX 
    *evp_cipher_ctx = NULL;

  EVP_MD 
    *evp_md = NULL ;

  EVP_MD_CTX
    *evp_digest = NULL ;

  SSL3_BUFFER 
    *ssl3_buf = NULL;  /* s3->rbuf and s3->wbuf */

  SSL3_RECORD 
    *ssl3_rec = NULL;  /* s3->rrec and s3->wrec */

  SSL_SESSION 
    *session = NULL;

  SSL3_STATE 
    *ssl3_state = NULL ;

  int
    version,
    ssl3_buf_len,
    ssl3_rec_len,
    ssl3_wpend_tot,
    md_size ;

  sLOGICAL
    DO_DEBUG = 1,
    DO_MAIN = 1,
    DO_CIPHER = 1,
    DO_CIPHER_INIT = 1,
    DO_BIO = 0,
    DO_S3 = 1,
      DO_HASH = 1,
      DO_WPEND = 0,
      DO_HANDSHAKE = 0,
      DO_REC = 0,
    DO_SESSION = 1,
    DO_SESSION_DI = 0,
    DO_REHANDSHAKE = 0;

  ssl = pSSL->ssl ;

  if ( !ssl ) return gHyp_util_logWarning ( "No SSL structure");

  /*=============================================================*/
  if ( DO_BIO ) {

    /* Transfering the BIO data does not seem to provide any benifit */

    pData = gHyp_ssl_getData ( pSSLdata, "inbio", "num_read", "" ) ;
    pSSL->inBio->num_read = gHyp_data_getInt( pData, 0, TRUE ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "inbio", "num_write", "" ) ;
    pSSL->inBio->num_write = gHyp_data_getInt( pData, 0, TRUE ) ;
    
    pData = gHyp_ssl_getData ( pSSLdata, "filterbio", "num_read", "" ) ;
    pSSL->filterBio->num_read = gHyp_data_getInt( pData, 0, TRUE ) ;
    
    pData = gHyp_ssl_getData ( pSSLdata, "filterbio", "num_write", "" ) ;
    pSSL->filterBio->num_write = gHyp_data_getInt( pData, 0, TRUE ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "outbio", "num_read", "" ) ;
    pSSL->outBio->num_read = gHyp_data_getInt( pData, 0, TRUE ) ;
    
    pData = gHyp_ssl_getData ( pSSLdata, "outbio", "num_write", "" ) ;
    pSSL->outBio->num_write = gHyp_data_getInt( pData, 0, TRUE ) ;

  }
  /*=============================================================*/
  
  ssl->handshake_func = ssl->method->ssl_accept;

  /*=============================================================*/
  if ( DO_MAIN ) {

  pData = gHyp_ssl_getData ( pSSLdata, "version", "", "" ) ;
  version = gHyp_data_getInt( pData, 0, TRUE ) ;

  if ( ssl->version != version ) {
    
    ssl->version = version ;

    if (ssl->version == SSL3_VERSION )
      ssl->method = SSLv3_server_method();
    else if ( ssl->version == TLS1_VERSION )
      ssl->method = TLSv1_server_method();
    else if ( ssl->version == TLS1_1_VERSION ) 
      ssl->method = TLSv1_1_server_method();
    else if ( ssl->version == TLS1_2_VERSION )
      ssl->method = TLSv1_2_server_method();
  }

  pData = gHyp_ssl_getData ( pSSLdata, "type", "", "" ) ;
  ssl->type = gHyp_data_getInt( pData, 0, TRUE ) ;

  pData = gHyp_ssl_getData ( pSSLdata, "rwstate", "", "" ) ;
  ssl->rwstate = gHyp_data_getInt( pData, 0, TRUE ) ;

  pData = gHyp_ssl_getData ( pSSLdata, "in_handshake", "", "" ) ;
  ssl->in_handshake = gHyp_data_getInt( pData, 0, TRUE ) ;

  pData = gHyp_ssl_getData ( pSSLdata, "server", "", "" ) ;
  ssl->server = gHyp_data_getInt( pData, 0, TRUE ) ;

  pData = gHyp_ssl_getData ( pSSLdata, "new_session", "", "" ) ;
  ssl->new_session = gHyp_data_getInt( pData, 0, TRUE ) ;

  pData = gHyp_ssl_getData ( pSSLdata, "quiet_shutdown", "", "" ) ;
  ssl->quiet_shutdown = gHyp_data_getInt( pData, 0, TRUE ) ;

  pData = gHyp_ssl_getData ( pSSLdata, "shutdown",  "",	"" ) ;
  ssl->shutdown	= gHyp_data_getInt( pData, 0, TRUE ) ;

  pData = gHyp_ssl_getData ( pSSLdata, "state",  "", "" ) ;
  ssl->state = gHyp_data_getInt( pData, 0, TRUE ) ;

  pData = gHyp_ssl_getData ( pSSLdata, "rstate",  "", "" ) ;
  ssl->rstate = gHyp_data_getInt( pData, 0, TRUE ) ;

  pData = gHyp_ssl_getData ( pSSLdata, "read_ahead",  "", "" ) ;
  ssl->read_ahead = gHyp_data_getInt( pData, 0, TRUE ) ;

  pData = gHyp_ssl_getData ( pSSLdata, "hit",  "", "" ) ;
  ssl->hit = gHyp_data_getInt( pData, 0, TRUE ) ;

  pData = gHyp_ssl_getData ( pSSLdata, "error",  "", "" ) ;
  ssl->error = gHyp_data_getInt( pData, 0, TRUE ) ;

  pData = gHyp_ssl_getData ( pSSLdata, "error_code",  "", "" ) ;
  ssl->error_code = gHyp_data_getInt( pData, 0, TRUE ) ;

  pData = gHyp_ssl_getData ( pSSLdata, "debug",  "", "" ) ;
  ssl->debug = gHyp_data_getInt( pData, 0, TRUE ) ;

  pData = gHyp_ssl_getData ( pSSLdata, "verify_result",  "", "" ) ;
  ssl->verify_result = gHyp_data_getInt( pData, 0, TRUE ) ;

  pData = gHyp_ssl_getData ( pSSLdata, "references",  "", "" ) ;
  ssl->references = gHyp_data_getInt( pData, 0, TRUE ) ;

  pData = gHyp_ssl_getData ( pSSLdata, "options",  "", "" ) ;
  ssl->options = gHyp_data_getInt( pData, 0, TRUE ) ;

  pData = gHyp_ssl_getData ( pSSLdata, "mode",  "", "" ) ;
  ssl->mode = gHyp_data_getInt( pData, 0, TRUE ) ;

  pData = gHyp_ssl_getData ( pSSLdata, "client_version",  "", "" ) ;
  ssl->client_version = gHyp_data_getInt( pData, 0, TRUE ) ;

  /* the session_id_context is used to ensure sessions are only reused
   * in the appropriate context 
   */
  pData = gHyp_ssl_getData ( pSSLdata, "sid_ctx_length",  "", "" ) ;
  ssl->sid_ctx_length = gHyp_data_getInt( pData, 0, TRUE ) ;

  pData = gHyp_ssl_getData ( pSSLdata, "sid_ctx",  "", "" ) ;
  memcpy( ssl->sid_ctx, 
	  gHyp_data_buffer ( pData, 0 ),
	  gHyp_data_bufferLen ( pData, 0 ) ) ;

  }
  /*=============================================================*/
  /*=============================================================*/
  if ( DO_CIPHER ) {

    /* These are the ones being used, the ones in SSL_SESSION are
     * the ones to be 'copied' into these ones 
     */

    /* READ CIPHER CONTEXT */
    if ( ssl->enc_read_ctx ) {

      EVP_CIPHER_CTX_cleanup ( ssl->enc_read_ctx ) ;
      /*OPENSSL_free ( ssl->enc_read_ctx ) ;*/
      ssl->enc_read_ctx = NULL ;
    }

    if ( !ssl->enc_read_ctx ) {

      if ( DO_DEBUG ) gHyp_util_debug("Allocating new read cipher context");
      ssl->enc_read_ctx = (EVP_CIPHER_CTX *) OPENSSL_malloc ( sizeof(EVP_CIPHER_CTX) ) ;

    }

    evp_cipher_ctx = ssl->enc_read_ctx ;
    if ( !evp_cipher_ctx ) 
      return gHyp_util_logWarning ( "No cipher read context" ) ;

    if ( DO_CIPHER_INIT ) {
      /* This resets cipher, oiv, iv, key_len, and cipher_data */
      if ( DO_DEBUG ) gHyp_util_debug("Initializing new read cipher context");
      EVP_CIPHER_CTX_init( evp_cipher_ctx );
    }

    pData = gHyp_ssl_getData ( pSSLdata, "enc_read_ctx","encrypt","" );
    evp_cipher_ctx->encrypt = gHyp_data_getInt ( pData, 0, TRUE ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "enc_read_ctx","buf_len","" );
    evp_cipher_ctx->buf_len = gHyp_data_getInt ( pData, 0, TRUE ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "enc_read_ctx","oiv","" );
    memcpy( evp_cipher_ctx->oiv,
	    gHyp_data_buffer ( pData, 0 ),
	    gHyp_data_bufferLen ( pData, 0 ) ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "enc_read_ctx","iv","" );
    memcpy( evp_cipher_ctx->iv, 
	    gHyp_data_buffer ( pData, 0 ),
	    gHyp_data_bufferLen ( pData, 0 ) ) ;

    
    pData = gHyp_ssl_getData ( pSSLdata, "enc_read_ctx","buf","" );
    memcpy( evp_cipher_ctx->buf,
	    gHyp_data_buffer ( pData, 0 ),
	    gHyp_data_bufferLen ( pData, 0 ) ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "enc_read_ctx","num","" );
    evp_cipher_ctx->num = gHyp_data_getInt ( pData, 0, TRUE ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "enc_read_ctx","key_len","" );
    evp_cipher_ctx->key_len = gHyp_data_getInt ( pData, 0, TRUE ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "enc_read_ctx","flags","" );
    evp_cipher_ctx->flags = gHyp_data_getInt ( pData, 0, TRUE ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "enc_read_ctx","cipher_data","" );
    
    if ( !evp_cipher_ctx->cipher_data ) {

      if ( DO_DEBUG ) gHyp_util_debug("Allocating new read cipher data ,of %d bytes",gHyp_data_bufferLen ( pData, 0 ));
      evp_cipher_ctx->cipher_data = (void *)OPENSSL_malloc(gHyp_data_bufferLen ( pData, 0 ));

    }

    memcpy (  (sBYTE*) evp_cipher_ctx->cipher_data,
	      gHyp_data_buffer ( pData, 0 ),
	      gHyp_data_bufferLen ( pData, 0 ) ) ;
  	    

    pData = gHyp_ssl_getData ( pSSLdata, "enc_read_ctx","final_used","" );
    evp_cipher_ctx->final_used = gHyp_data_getInt ( pData, 0, TRUE ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "enc_read_ctx","block_mask","" );
    evp_cipher_ctx->block_mask = gHyp_data_getInt ( pData, 0, TRUE ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "enc_read_ctx","final","" );
    memcpy( evp_cipher_ctx->final,
	    gHyp_data_buffer ( pData, 0 ),
	    gHyp_data_bufferLen ( pData, 0 ) ) ;

      
    pData = gHyp_ssl_getData ( pSSLdata, "enc_read_ctx","cipher","nid" );
    if ( !evp_cipher_ctx->cipher ) {
      if ( DO_DEBUG ) gHyp_util_debug("Creating new read cipher by nid %d",gHyp_data_getInt ( pData, 0, TRUE ));
      evp_cipher_ctx->cipher = EVP_get_cipherbynid( gHyp_data_getInt ( pData, 0, TRUE )  ) ;
    }

    /* WRITE CIPHER CONTEXT */

    if ( ssl->enc_write_ctx ) {
      EVP_CIPHER_CTX_cleanup ( ssl->enc_write_ctx ) ;
      /*OPENSSL_free ( ssl->enc_write_ctx ) ;*/
      ssl->enc_write_ctx = NULL ;
    }

    if ( !ssl->enc_write_ctx ) {
      if ( DO_DEBUG ) gHyp_util_debug("Allocating new write cipher context");
      ssl->enc_write_ctx = (EVP_CIPHER_CTX *) OPENSSL_malloc ( sizeof(EVP_CIPHER_CTX) ) ;
    }

    evp_cipher_ctx = ssl->enc_write_ctx ;
    if ( !evp_cipher_ctx ) 
      return gHyp_util_logWarning ( "No cipher write context" ) ;
    
    if ( DO_CIPHER_INIT ) {
      if ( DO_DEBUG ) gHyp_util_debug("Initializing write cipher context");
      EVP_CIPHER_CTX_init( evp_cipher_ctx );
    }

    pData = gHyp_ssl_getData ( pSSLdata, "enc_write_ctx","encrypt","" );
    evp_cipher_ctx->encrypt = gHyp_data_getInt ( pData, 0, TRUE ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "enc_write_ctx","buf_len","" );
    evp_cipher_ctx->buf_len = gHyp_data_getInt ( pData, 0, TRUE ) ;


    pData = gHyp_ssl_getData ( pSSLdata, "enc_write_ctx","oiv","" );
    memcpy( evp_cipher_ctx->oiv,
	    gHyp_data_buffer ( pData, 0 ),
	    gHyp_data_bufferLen ( pData, 0 ) ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "enc_write_ctx","iv","" );
    memcpy( evp_cipher_ctx->iv, 
	    gHyp_data_buffer ( pData, 0 ),
	    gHyp_data_bufferLen ( pData, 0 ) ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "enc_write_ctx","buf","" );
    memcpy( evp_cipher_ctx->buf,
	    gHyp_data_buffer ( pData, 0 ),
	    gHyp_data_bufferLen ( pData, 0 ) ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "enc_write_ctx","num","" );
    evp_cipher_ctx->num = gHyp_data_getInt ( pData, 0, TRUE ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "enc_write_ctx","key_len","" );
    evp_cipher_ctx->key_len = gHyp_data_getInt ( pData, 0, TRUE ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "enc_write_ctx","flags","" );
    evp_cipher_ctx->flags = gHyp_data_getInt ( pData, 0, TRUE ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "enc_write_ctx","cipher_data","" );
    if ( !evp_cipher_ctx->cipher_data ) {
      if ( DO_DEBUG ) gHyp_util_debug("Allocating new write cipher data of %d bytes",gHyp_data_bufferLen ( pData, 0 ));
      evp_cipher_ctx->cipher_data = (void *)OPENSSL_malloc(gHyp_data_bufferLen ( pData, 0 ));
    }
    
    memcpy (  (sBYTE*) evp_cipher_ctx->cipher_data,
	      gHyp_data_buffer ( pData, 0 ),
	      gHyp_data_bufferLen ( pData, 0 ) ) ;
    	    
    pData = gHyp_ssl_getData ( pSSLdata, "enc_write_ctx","final_used","" );
    evp_cipher_ctx->final_used = gHyp_data_getInt ( pData, 0, TRUE ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "enc_write_ctx","block_mask","" );
    evp_cipher_ctx->block_mask = gHyp_data_getInt ( pData, 0, TRUE ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "enc_write_ctx","final","" );
    memcpy( evp_cipher_ctx->final,
	    gHyp_data_buffer ( pData, 0 ),
	    gHyp_data_bufferLen ( pData, 0 ) ) ;


    pData = gHyp_ssl_getData ( pSSLdata, "enc_write_ctx","cipher","nid" );
    if ( !evp_cipher_ctx->cipher ) {
      if ( DO_DEBUG ) gHyp_util_debug("Creating new write cipher by nid %d",gHyp_data_getInt ( pData, 0, TRUE ));
      evp_cipher_ctx->cipher = EVP_get_cipherbynid( gHyp_data_getInt ( pData, 0, TRUE )  ) ;
    }
    

  }
  /*=============================================================*/

  /*=============================================================*/
  if ( DO_HASH ) {

    /* As with enc_read_ctx and enc_write_ctx, the read_hash and 
     * write hash are the ones being used, the ones in SSL_SESSION are
     * the ones to be 'copied' into these ones.
     *
     * It seems important that we manage these structures, however
     * if they are not present, the SSL infrastructure appears to
     * reconstruct them automatically.
     */

    pData = gHyp_ssl_getData ( pSSLdata, "read_hash","digest","type" );
    if ( !ssl->read_hash ) {
      if ( DO_DEBUG ) gHyp_util_debug("Creating new read hash from nid %d",gHyp_data_getInt ( pData, 0, TRUE ));
      ssl->read_hash = EVP_MD_CTX_create() ;
      EVP_DigestInit_ex( ssl->read_hash, EVP_get_digestbynid( gHyp_data_getInt(pData,0,TRUE)), NULL);
    }

    evp_digest = (EVP_MD_CTX*) ssl->read_hash ;
    if ( !evp_digest ) return gHyp_util_logWarning ( "No read hash" ) ;

    evp_md = (EVP_MD*) evp_digest->digest ;
    if ( !evp_md ) return gHyp_util_logWarning ( "No read hash digest" ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "write_hash","digest","type" );
    if ( !ssl->write_hash ) {
      if ( DO_DEBUG ) gHyp_util_debug("Creating new write hash from nid %d",gHyp_data_getInt ( pData, 0, TRUE ));
      ssl->write_hash = EVP_MD_CTX_create() ;
      EVP_DigestInit_ex( ssl->write_hash, EVP_get_digestbynid( gHyp_data_getInt(pData,0,TRUE)), NULL);
    }

    evp_digest = (EVP_MD_CTX*) ssl->write_hash ;
    if ( !evp_digest ) return gHyp_util_logWarning ( "No write hash" ) ;

    evp_md = (EVP_MD*) evp_digest->digest ;
    if ( !evp_md ) return gHyp_util_logWarning ( "No write hash digest" ) ;
  }
  /*=============================================================*/

  /*=============================================================*/
  if ( DO_S3 ) {

    /* S3 STATE */

    /*if ( !ssl->s3) ssl3_new(ssl);*/
    ssl3_state = ssl->s3 ;
    if ( !ssl3_state ) 
      return gHyp_util_logWarning ( "Could not initialize ssl->s3 state" );

    pData = gHyp_ssl_getData ( pSSLdata, "s3", "flags", "" ) ;
    ssl3_state->flags = gHyp_data_getInt ( pData, 0, TRUE ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "s3", "delay_buf_pop_ret", "" ) ;
    ssl3_state->delay_buf_pop_ret = gHyp_data_getInt ( pData, 0, TRUE ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "s3",  "read_sequence", "" ) ;
    memcpy ( ssl3_state->read_sequence, 
	     gHyp_data_buffer ( pData, 0 ),
	     gHyp_data_bufferLen ( pData, 0 ) ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "s3", "read_mac_secret_size", "" ) ;
    ssl3_state->read_mac_secret_size = gHyp_data_getInt ( pData, 0, TRUE ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "s3",  "read_mac_secret", "" ) ;
    memcpy ( ssl3_state->read_mac_secret, 
	     gHyp_data_buffer ( pData, 0 ),
	     gHyp_data_bufferLen ( pData, 0 ) ) ;

    
    pData = gHyp_ssl_getData ( pSSLdata, "s3",  "write_sequence",	"" ) ;
    memcpy ( ssl3_state->write_sequence, 
	     gHyp_data_buffer ( pData, 0 ),
	     gHyp_data_bufferLen ( pData, 0 ) ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "s3", "write_mac_secret_size", "" ) ;
    ssl3_state->write_mac_secret_size = gHyp_data_getInt ( pData, 0, TRUE ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "s3",  "write_mac_secret", "" ) ;
    memcpy ( ssl3_state->write_mac_secret, 
	     gHyp_data_buffer ( pData, 0 ),
	     gHyp_data_bufferLen ( pData, 0 ) ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "s3",  "server_random","" ) ;
    memcpy ( ssl3_state->server_random, 
	     gHyp_data_buffer ( pData, 0 ),
	     gHyp_data_bufferLen ( pData, 0 ) ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "s3",  "client_random", "" ) ;
    memcpy ( ssl3_state->client_random, 
	     gHyp_data_buffer ( pData, 0 ),
	     gHyp_data_bufferLen ( pData, 0 ) ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "s3",  "need_empty_fragments", "" ) ;
    ssl3_state->need_empty_fragments = gHyp_data_getInt ( pData, 0, TRUE ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "s3",  "empty_fragment_done", "" ) ;
    ssl3_state->empty_fragment_done = gHyp_data_getInt ( pData, 0, TRUE ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "s3",  "init_extra", "" ) ;
    ssl3_state->init_extra = gHyp_data_getInt ( pData, 0, TRUE ) ;

    /* S3 State Read Buf */

    ssl3_buf = (SSL3_BUFFER*) &ssl->s3->rbuf ;
    if ( !ssl3_buf ) return gHyp_util_logWarning ( "No ssl->s3->rbuf" ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "s3", "rbuf", "len" ) ;
    ssl3_buf_len	= gHyp_data_getInt ( pData, 0, TRUE ) ;

    if ( ssl3_buf_len > 0 && !ssl3_buf->buf ) {
      /* No rbuf, allocate one */
      if ( DO_DEBUG ) gHyp_util_debug("Initializing new ssl3 rbuf of %d bytes",ssl3_buf_len);
      ssl3_buf->buf = OPENSSL_malloc ( ssl3_buf_len ) ;
    }

    ssl3_buf->len	= ssl3_buf_len ;

    pData = gHyp_ssl_getData ( pSSLdata, "s3", "rbuf", "offset" ) ;
    ssl3_buf->offset = gHyp_data_getInt ( pData, 0, TRUE ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "s3", "rbuf", "left" ) ;
    ssl3_buf->left = gHyp_data_getInt ( pData, 0, TRUE ) ;

    if ( ssl3_buf->left > 0 ) {
      if ( DO_DEBUG ) gHyp_util_debug("S3 RBUF left = %d",ssl3_buf->left ) ;
      pData = gHyp_ssl_getData ( pSSLdata, "s3", "rbuf", "buf" ) ;
      memcpy ( ssl3_buf->buf+ssl3_buf->offset, 
	       gHyp_data_buffer ( pData, 0 ),
	       ssl3_buf->left ) ;
    }

    /* S3 State Write Buf */
    ssl3_buf = (SSL3_BUFFER*) &ssl->s3->wbuf ;
    if ( !ssl3_buf ) return gHyp_util_logWarning ( "No ssl->s3->wbuf" ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "s3", "wbuf", "len" ) ;
    ssl3_buf_len	= gHyp_data_getInt ( pData, 0, TRUE ) ;

    if ( ssl3_buf_len > 0 && !ssl3_buf->buf ) {
      /* No wbuf, allocate one */
      if ( DO_DEBUG ) gHyp_util_debug("Initializing new ssl3 wbuf of %d bytes",ssl3_buf_len);
      ssl3_buf->buf = OPENSSL_malloc ( ssl3_buf_len ) ;
    }
    ssl3_buf->len	= ssl3_buf_len ;

    pData = gHyp_ssl_getData ( pSSLdata, "s3", "wbuf", "offset" ) ;
    ssl3_buf->offset = gHyp_data_getInt ( pData, 0, TRUE ) ;
   
    pData = gHyp_ssl_getData ( pSSLdata, "s3", "wbuf", "left" ) ;
    ssl3_buf->left = gHyp_data_getInt ( pData, 0, TRUE ) ;

    if ( ssl3_buf->left > 0 ) {
      if ( DO_DEBUG ) gHyp_util_debug("S3 wbuf left = %d",ssl3_buf->left ) ;
      pData = gHyp_ssl_getData ( pSSLdata, "s3", "wbuf", "buf" ) ;
      memcpy ( ssl3_buf->buf+ssl3_buf->offset, 
	       gHyp_data_buffer ( pData, 0 ),
	       ssl3_buf->left ) ;
    }
    /*=============================================================*/

    /*=============================================================*/

    if ( DO_REC ) {

      /* S3 State Write Rec */
      ssl3_rec = (SSL3_RECORD*) &ssl->s3->wrec ;
      if ( !ssl3_rec ) return gHyp_util_logWarning ( "No ssl->s3->wrec" ) ;

      pData = gHyp_ssl_getData ( pSSLdata, "s3", "wrec", "length" ) ;
      ssl3_rec_len	= gHyp_data_getInt ( pData, 0, TRUE ) ;

      if ( ssl3_rec_len > 0 && !ssl3_rec->data ) {
        /* No wrec, allocate one */
        if ( DO_DEBUG ) gHyp_util_debug("Initializing new ssl3 wrec of %d bytes",ssl3_rec_len);
        ssl3_rec->data = OPENSSL_malloc ( ssl3_rec_len ) ;
        ssl3_rec->input = ssl3_rec->data ;
      }
      ssl3_rec->length = ssl3_rec_len ;

      pData = gHyp_ssl_getData ( pSSLdata, "s3", "wrec", "off" ) ;
      ssl3_rec->off = gHyp_data_getInt ( pData, 0, TRUE ) ;
     
      pData = gHyp_ssl_getData ( pSSLdata, "s3", "wrec", "type" ) ;
      ssl3_rec->type = gHyp_data_getInt ( pData, 0, TRUE ) ;

      pData = gHyp_ssl_getData ( pSSLdata, "s3", "wrec", "epoch" ) ;
      ssl3_rec->epoch = gHyp_data_getInt ( pData, 0, TRUE ) ;

      /*
      if ( ssl3_rec->length > 0 ) {
        if ( DO_DEBUG ) gHyp_util_debug("S3 wrec length = %d",ssl3_rec->length ) ;
        pData = gHyp_ssl_getData ( pSSLdata, "s3", "wrec", "data" ) ;
        memcpy ( ssl3_rec->data+ssl3_rec->off, 
	         gHyp_data_buffer ( pData, 0 ),
	         ssl3_rec->length ) ;
      }
      */

      /* S3 State Read Rec */
      ssl3_rec = (SSL3_RECORD*) &ssl->s3->rrec ;
      if ( !ssl3_rec ) return gHyp_util_logWarning ( "No ssl->s3->rrec" ) ;

      pData = gHyp_ssl_getData ( pSSLdata, "s3", "rrec", "length" ) ;
      ssl3_rec_len	= gHyp_data_getInt ( pData, 0, TRUE ) ;

      if ( ssl3_rec_len > 0 && !ssl3_rec->data ) {
        /* No wrec, allocate one */
        if ( DO_DEBUG ) gHyp_util_debug("Initializing new ssl3 rrec of %d bytes",ssl3_rec_len);
        ssl3_rec->data = OPENSSL_malloc ( ssl3_rec_len ) ;
        ssl3_rec->input = ssl3_rec->data ;
      }

      ssl3_rec->length = ssl3_rec_len ;

      pData = gHyp_ssl_getData ( pSSLdata, "s3", "rrec", "off" ) ;
      ssl3_rec->off = gHyp_data_getInt ( pData, 0, TRUE ) ;
     
      pData = gHyp_ssl_getData ( pSSLdata, "s3", "rrec", "type" ) ;
      ssl3_rec->type = gHyp_data_getInt ( pData, 0, TRUE ) ;

      pData = gHyp_ssl_getData ( pSSLdata, "s3", "rrec", "epoch" ) ;
      ssl3_rec->epoch = gHyp_data_getInt ( pData, 0, TRUE ) ;

      if ( ssl3_rec->length > 0 ) {
        if ( DO_DEBUG ) gHyp_util_debug("S3 rrec length = %d",ssl3_rec->length ) ;
        pData = gHyp_ssl_getData ( pSSLdata, "s3", "rrec", "data" ) ;
        memcpy ( ssl3_rec->data+ssl3_rec->off, 
	         gHyp_data_buffer ( pData, 0 ),
	         ssl3_rec->length ) ;
      }
    }
    /*=============================================================*/

    /*=============================================================*/

    if ( DO_WPEND ) {

      pData = gHyp_ssl_getData ( pSSLdata, "s3", "wnum", "" ) ;
      ssl3_state->wnum = gHyp_data_getInt ( pData, 0, TRUE ) ;

      pData = gHyp_ssl_getData ( pSSLdata, "s3", "wpend_type", "" ) ;
      ssl3_state->wpend_type = gHyp_data_getInt ( pData, 0, TRUE ) ;

      pData = gHyp_ssl_getData ( pSSLdata, "s3", "wpend_ret", "" ) ;
      ssl3_state->wpend_ret = gHyp_data_getInt ( pData, 0, TRUE ) ;
      
      pData = gHyp_ssl_getData ( pSSLdata, "s3", "wpend_tot", "" ) ;
      ssl3_wpend_tot = gHyp_data_getInt ( pData, 0, TRUE ) ;

      if ( ssl3_wpend_tot > 0 && !ssl3_state->wpend_buf ) {
        if ( DO_DEBUG ) gHyp_util_debug("Initializing new ssl3 wpend_buf of %d bytes",ssl3_wpend_tot);
        ssl3_state->wpend_buf = OPENSSL_malloc ( ssl3_wpend_tot ) ;
      }

      ssl3_state->wpend_tot = ssl3_wpend_tot ;

      if ( ssl3_wpend_tot > 0 ) {
        if ( DO_DEBUG ) gHyp_util_debug("S3 wpend_buf length = %d",ssl3_wpend_tot ) ;
        pData = gHyp_ssl_getData ( pSSLdata, "s3", "wpend_buf", "" ) ;
        memcpy ( ssl3_state->wpend_buf, 
	         gHyp_data_buffer ( pData, 0 ),
	         ssl3_wpend_tot ) ;
      }
    }
    /*=============================================================*/

    /*=============================================================*/
    if ( DO_HANDSHAKE ) {

      if ( DO_DEBUG ) gHyp_util_debug("handshake digest");
      pData = gHyp_ssl_getData ( pSSLdata, "s3", "handshake_fragment_len", "" ) ;
      ssl3_state->handshake_fragment_len = gHyp_data_getInt ( pData, 0, TRUE ) ;

      pData = gHyp_ssl_getData ( pSSLdata, "s3",  "handshake_fragment", "" ) ;
      memcpy ( ssl3_state->handshake_fragment, 
	       gHyp_data_buffer ( pData, 0 ),
	       gHyp_data_bufferLen ( pData, 0 ) ) ;

      /**** Handshake digest *****/
      pData = gHyp_ssl_getData ( pSSLdata, "s3","handshake_dgst","type" );
      if ( !ssl3_state->handshake_dgst || !(*ssl3_state->handshake_dgst) ) {
        if ( DO_DEBUG ) gHyp_util_debug("Allocating new handshake digest of nid %d",gHyp_data_getInt ( pData, 0, TRUE));
        evp_digest = EVP_get_digestbynid( gHyp_data_getInt ( pData, 0, TRUE ) ) ;
        if ( DO_DEBUG ) gHyp_util_debug("Setting new handshake digest");
        ssl3_state->handshake_dgst = &evp_digest ;
      }

      evp_digest = (EVP_MD_CTX*) *(ssl3_state->handshake_dgst) ;
      if ( !evp_digest ) return gHyp_util_logWarning ( "No handshake digest" ) ;

      evp_md = (EVP_MD*) evp_digest->digest ;
      if ( !evp_md ) return gHyp_util_logWarning ( "No handshake digest evp" ) ;

        if ( DO_DEBUG ) gHyp_util_debug("Load MD data");
      /* Load md_data */
      pData = gHyp_ssl_getData ( pSSLdata, "s3", "handshake_dgst", "md_size" ) ;
      md_size = gHyp_data_getInt ( pData, 0, TRUE ) ;

      /*
      if ( md_size > 0 ) {
        if ( DO_DEBUG ) gHyp_util_debug("S3 handshake digest md_data length = %d",md_size ) ;
        pData = gHyp_ssl_getData ( pSSLdata, "s3", "handshake_dgst", "md_data" ) ;
        memcpy ( evp_digest->md_data, 
	         gHyp_data_buffer ( pData, 0 ),
	         md_size ) ;
      }
      evp_md->md_size = md_size ;
      */
    }

    /*=============================================================*/

    /*=============================================================*/

    pData = gHyp_ssl_getData ( pSSLdata, "s3", "alert_fragment_len", "" ) ;
    ssl3_state->alert_fragment_len = gHyp_data_getInt ( pData, 0, TRUE ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "s3",  "alert_fragment", "" ) ;
    memcpy ( ssl3_state->alert_fragment, 
	     gHyp_data_buffer ( pData, 0 ),
	     gHyp_data_bufferLen ( pData, 0 ) ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "s3", "in_read_app_data", "" ) ;
    ssl3_state->in_read_app_data = gHyp_data_getInt ( pData, 0, TRUE ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "s3", "previous_client_finished_len", "" ) ;
    ssl3_state->previous_client_finished_len = gHyp_data_getInt ( pData, 0, TRUE ) ;
    
    pData = gHyp_ssl_getData ( pSSLdata, "s3",  "previous_client_finished", "" ) ;
    memcpy ( ssl3_state->previous_client_finished, 
	     gHyp_data_buffer ( pData, 0 ),
	     gHyp_data_bufferLen ( pData, 0 ) ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "s3", "previous_server_finished_len", "" ) ;
    ssl3_state->previous_server_finished_len = gHyp_data_getInt ( pData, 0, TRUE ) ;
    
    pData = gHyp_ssl_getData ( pSSLdata, "s3",  "previous_server_finished", "" ) ;
    memcpy ( ssl3_state->previous_server_finished, 
	     gHyp_data_buffer ( pData, 0 ),
	     gHyp_data_bufferLen ( pData, 0 ) ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "s3", "send_connection_binding", "" ) ;
    ssl3_state->send_connection_binding = gHyp_data_getInt ( pData, 0, TRUE ) ;
    
    pData = gHyp_ssl_getData ( pSSLdata, "s3", "next_proto_neg_seen", "" ) ;
    ssl3_state->next_proto_neg_seen = gHyp_data_getInt ( pData, 0, TRUE ) ;
    
  }
  /*=============================================================*/

  /*=============================================================*/
  if ( DO_SESSION_DI ) {

    if ( DO_DEBUG ) gHyp_util_debug ( "Using SESSION d2i_SSL_SESSION method" );
    pData = gHyp_ssl_getData ( pSSLdata, "ASN1_SESSION", "", "" ) ;
    gHyp_sock_setSessionObject ( pSSL, pData );

  }

  if ( DO_SESSION ) {

    /* SESSION */

    if ( !ssl->session ) {
      if ( DO_DEBUG ) gHyp_util_debug("Allocating new session");
      ssl->session = (SSL_SESSION *) OPENSSL_malloc ( sizeof ( SSL_SESSION ) ) ;
      session = (SSL_SESSION*) ssl->session ;
      memset ( session, 0, sizeof ( SSL_SESSION ) ) ;
    }

    session = (SSL_SESSION*) ssl->session ;

    pData = gHyp_ssl_getData ( pSSLdata, "session", "ssl_version", "" ) ;
    session->ssl_version = gHyp_data_getInt( pData, 0, TRUE ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "session",  "key_arg_length", "" ) ;
    session->key_arg_length = gHyp_data_getInt( pData, 0, TRUE ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "session",  "key_arg", "" ) ;
    memcpy( session->key_arg,
	    gHyp_data_buffer ( pData, 0 ),
	    gHyp_data_bufferLen ( pData, 0 ) ) ;


    pData = gHyp_ssl_getData ( pSSLdata, "session",  "master_key_length", "" ) ;
    session->master_key_length = gHyp_data_getInt( pData, 0, TRUE ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "session",  "master_key", "" ) ;
    memcpy( session->master_key,
	    gHyp_data_buffer ( pData, 0 ),
	    gHyp_data_bufferLen ( pData, 0 ) ) ;


    pData = gHyp_ssl_getData ( pSSLdata, "session",  "session_id_length", "" ) ;
    session->session_id_length = gHyp_data_getInt( pData, 0, TRUE ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "session",  "session_id", "" ) ;
    memcpy( session->session_id, 
	    gHyp_data_buffer ( pData, 0 ),
	    gHyp_data_bufferLen ( pData, 0 ) ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "session",  "sid_ctx_length", "" ) ;
    session->sid_ctx_length = gHyp_data_getInt( pData, 0, TRUE ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "session",  "sid_ctx", "" ) ;
    memcpy( session->sid_ctx,
	    gHyp_data_buffer ( pData, 0 ),
	    gHyp_data_bufferLen ( pData, 0 ) ) ;

    
    pData = gHyp_ssl_getData ( pSSLdata, "session",  "not_resumable", "" ) ;
    session->not_resumable = gHyp_data_getInt( pData, 0, TRUE ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "session",  "verify_result", "" ) ;
    session->verify_result = gHyp_data_getInt( pData, 0, TRUE ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "session",  "references", "" ) ;
    session->references = gHyp_data_getInt( pData, 0, TRUE ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "session",  "timeout", "" ) ;
    session->timeout = gHyp_data_getInt( pData, 0, TRUE ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "session",  "time", "" ) ;
    session->time = gHyp_data_getInt( pData, 0, TRUE ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "session",  "compress_meth", "" ) ;
    session->compress_meth = gHyp_data_getInt( pData, 0, TRUE ) ;

    pData = gHyp_ssl_getData ( pSSLdata, "session",  "cipher_id", "" ) ;
    session->cipher_id = gHyp_data_getInt( pData, 0, TRUE ) ;

  }
  /*=============================================================*/


  /* Testing *********************************
    pSSL->outBio = pSSLORIG->outBio ; 
    pSSL->filterBio = pSSLORIG->filterBio ;
    pSSL->inBio = pSSLORIG->inBio ;
    pSSL->ssl = pSSLORIG->ssl ;
    pSSL->isClient = pSSLORIG->isClient ;
    strcpy(pSSL->condition,pSSLORIG->condition);
    pSSL->sslCtx = pSSLORIG->sslCtx ;
    pSSL->state = pSSLORIG->state ;
    ssl->init_msg = pSSLORIG->ssl->init_msg ;
    ssl->packet = pSSLORIG->ssl->packet ;
    session->cipher = pSSLORIG->ssl->session->cipher ;
    ssl->cert = pSSLORIG->ssl->cert ;
    ssl->session = pSSLORIG->ssl->session ;
  */

  /*=============================================================*/
  if ( DO_REHANDSHAKE ) {

    SSL_renegotiate(ssl);
    SSL_do_handshake(ssl);
  }
  /*=============================================================*/

  return TRUE ;
}

void gHyp_sock_digest ( char *text, char *text2, char *digest, char* digestType ) 
{
  
  EVP_MD_CTX 
    mdctx;

 const EVP_MD 
   *md;

 unsigned char 
   md_value[EVP_MAX_MD_SIZE];

 unsigned int 
   md_len,
   n ;

  OpenSSL_add_all_digests();

  /* By default, the digest will be "SHA1" */
  md = EVP_get_digestbyname(digestType);

  if(!md) {
    gHyp_util_logError ( "Unknown message digest %s",digestType);
    return ;     
  }

  EVP_MD_CTX_init(&mdctx);
  EVP_DigestInit_ex(&mdctx, md, NULL);
  EVP_DigestUpdate(&mdctx, text, strlen(text));
  if ( strlen ( text2 ) > 0 ) EVP_DigestUpdate(&mdctx, text2, strlen(text2));
  EVP_DigestFinal_ex(&mdctx, md_value, &md_len);
  EVP_MD_CTX_cleanup(&mdctx);

  n = gHyp_util_base64Encode( (char*) md_value, md_len, digest )  ;
  digest[n] = '\0' ;

  /*
  n = gHyp_util_unparseString ( digest, md_value, md_len, strlen ( digest ), 
				FALSE, FALSE, FALSE, "" ) ;
				*/
  return ;
}

void gHyp_sock_hmac256 ( char *data, char *key, char *digest ) 
{
  HMAC_CTX
     ctx ;

  unsigned int
    n ;

  unsigned char
    hash[SHA256_DIGEST_LENGTH] ;

  OpenSSL_add_all_algorithms();

  HMAC_CTX_init ( &ctx ) ;
  HMAC_Init_ex(&ctx, key, strlen(key), EVP_sha256(), NULL);
  HMAC_Update(&ctx, (const unsigned char*) data, strlen(data) );
  HMAC_Final(&ctx, hash, &n );
  HMAC_CTX_cleanup(&ctx);

  n = gHyp_util_base64Encode( (char*) hash, SHA256_DIGEST_LENGTH, digest )  ;
  digest[n] = '\0' ;

  return ;
}

void gHyp_sock_md5( char *text, int len, char *digest )
{
  EVP_MD_CTX mdctx;
  unsigned char md_value[EVP_MAX_MD_SIZE];
  unsigned int md_len;
  int i;

  EVP_DigestInit(&mdctx, EVP_md5());
  EVP_DigestUpdate(&mdctx, text, (size_t) len);
  EVP_DigestFinal_ex(&mdctx, md_value, &md_len);
  EVP_MD_CTX_cleanup(&mdctx);

  for(i = 0; i < (int)md_len; i++){
    sprintf ( digest+i*2, "%02x", md_value[i] );
  }
  digest[md_len*2] = '\0' ;

}


sLOGICAL gHyp_sock_ctxCApath ( void *ctx, char *CApath ) 
{
  /* Set the path to the Certificate Authority */

  if (	!SSL_CTX_load_verify_locations( ctx, NULL, CApath) ||
	!SSL_CTX_set_default_verify_paths ( ctx ) )
    return lHyp_sock_sslError("Failed to load/set verify of CA path");
  else
     return TRUE ;
}

sLOGICAL gHyp_sock_ctxCAfile ( void *ctx, char *CAfile ) 
{

  /* Set a certificate file to be used as the Certificate Authority..*/

  if (	!SSL_CTX_load_verify_locations( ctx, CAfile, NULL ) ||
	!SSL_CTX_set_default_verify_paths ( ctx ) )
    return lHyp_sock_sslError("Failed to load/set verify of CA file");
  else
     return TRUE ;
}

sLOGICAL gHyp_sock_ctxCiphers ( void *ctx, char *ciphers ) 
{ 
  /* Set a list of ciphers that we will support */

  SSL_CTX_set_cipher_list( (SSL_CTX*) ctx, ciphers ) ;
  return TRUE ;
}

sLOGICAL gHyp_sock_ctxAuth ( void *ctx )
{    
  /* Calling this method as a SSL server will request SSL to
   * verify a certificate from the client.but not force it.
   */
  SSL_CTX_set_verify( ctx,
		      SSL_VERIFY_PEER,
		      lHyp_sock_verify_callback);
  return TRUE ;
}

sLOGICAL gHyp_sock_ctxAuthClient ( void *ctx ) 
{
  /* Calling this method as a SSL server will force SSL to
   * verify a certificate from the client.
   */
  SSL_CTX_set_verify( ctx, 
		      SSL_VERIFY_PEER |
		      SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
		      lHyp_sock_verify_callback ) ;
  return TRUE ;
}

sSSL *gHyp_sock_copySSL ( sSSL *pSSL )
{
  if ( pSSL == NULL ) return NULL ;
  return gHyp_sock_createSSL ( pSSL->sslCtx, pSSL->isClient, pSSL->condition ) ;
}

sSSL *gHyp_sock_createSSL ( void *ctx, sLOGICAL isClient, char *condition )
{
  sSSL 
    *pSSL ;

  SSL
    *ssl ;
  
  BIO
    *filterBIO,
    *internalBIO,
    *networkBIO ;

  ssl = SSL_new ( (SSL_CTX*) ctx ) ;

  if ( guDebugFlags & DEBUG_DIAGNOSTICS )
      gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
      "Created new SSL object");

  if ( !(BIO_new_bio_pair (  &internalBIO, MAX_MESSAGE_SIZE, 
			     &networkBIO, MAX_MESSAGE_SIZE ) ) ) {

    lHyp_sock_sslError ( "Failed to create BIO pair" ) ;			    
    return NULL ;
  }

  filterBIO = BIO_new ( BIO_f_ssl () ) ;
  if ( !filterBIO ) {
    lHyp_sock_sslError ( "Failed to create filter BIO" ) ;
    return NULL ;
  }

  if ( isClient ) {

    if ( guDebugFlags & DEBUG_DIAGNOSTICS )
      gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
      "Setting SSL to connect state");

    SSL_set_connect_state ( ssl ) ;

  }
  else {

    if ( guDebugFlags & DEBUG_DIAGNOSTICS )
      gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
	"Setting SSL to accept state");

    SSL_set_accept_state( ssl);

    /*gHyp_sock_enableSessions ( ctx ) ;*/
  }
  SSL_set_bio ( ssl, internalBIO, internalBIO );

  (void)BIO_set_ssl( filterBIO, ssl, BIO_NOCLOSE );

  pSSL = (sSSL*) AllocMemory ( sizeof ( sSSL ) ) ;

  pSSL->filterBio = filterBIO ;
  pSSL->inBio = internalBIO ;
  pSSL->outBio = networkBIO ;
  pSSL->ssl = ssl ;
  pSSL->sslCtx = (SSL_CTX*) ctx ;
  pSSL->isClient = isClient ;
  strcpy ( pSSL->condition, condition ) ;
  pSSL->state = -1 ; /* No state */

  /*
  pSSL->keyFile = NULL ;
  pSSL->password = NULL ;
  */
  return pSSL ;
}
#endif

static int lHyp_sock_read ( SOCKET s, 
	                    char *pMsgOff,
			    int maxBytes,
			    short channelType,
			    int timeout,
			    int *pNbytes,
			    LPOVERLAPPED pOverlapped,
			    sLOGICAL silent)
{
  /* Description:
   *
   *    Read from a socket.
   *
   * The timeout argument is interpreted as follows:
   *
   * -timeout   Perform asynchronous read only, do not wait for result.
   *
   * 0          Get result of asynchronous read.
   *  
   * +timeout   Perform synchronous read, wait for and get result.
   */

  int   
    nBytes=0;

#if defined ( AS_WINDOWS )
  sLOGICAL
    status ;

  int
    dwError ;
  
  COMMTIMEOUTS CommTimeouts; 
#endif
  
  struct timeval
    timer ;

  fd_set 
    readFDS ;

#ifdef AS_VMS

  unsigned int
    func ;

  int           
    status ;
#endif

  if ( giLoopback == INVALID_SOCKET ) gsSocketToCancel = s ;

  if ( timeout != 0 ) {

    /************************************************************************************
     * Set up read. 
     *************************************************************************************/

    switch ( channelType ) {

#if defined ( AS_WINDOWS )

    case SOCKET_TCP :

      if ( timeout < 0 ) {

	if ( WSAEventSelect( s, pOverlapped->hEvent, FD_CLOSE+FD_READ) == SOCKET_ERROR ) {
          gHyp_util_sysError ( "Failed to set event on TCP socket %d",s ) ;
          return -1 ;
	}
	
      }
      else {
 	/*gHyp_util_debug("FD SET on %d",s) ;*/
        /* Set the read for the TCP port */
        FD_ZERO ( &readFDS) ;
        FD_SET ( s, &readFDS ) ;
      }    
      break ;

    case SOCKET_LISTEN :
    
      if ( timeout < 0 ) {
 	/*gHyp_util_debug("WSA event 2 on %d",s) ;*/
        if ( WSAEventSelect( s, pOverlapped->hEvent, FD_ACCEPT+FD_CLOSE ) == SOCKET_ERROR ) {
	  if ( !silent )
	    gHyp_util_sysError ( "Failed to set event on LISTEN socket %d",s ) ;
          return -1 ;
        }
      }
      else {
 	/*gHyp_util_debug("FD SET 2 on %d",s) ;*/
        /* Set the read for the FIFO port */
        FD_ZERO ( &readFDS) ;
        FD_SET ( s, &readFDS ) ;
      }    
      break ;
   
    case SOCKET_SERIAL :
    case SOCKET_FIFO :

      /* Do not perform the 'ReadFile' if there is already an overlapped IO in progress */
      if ( (timeout < 0 && !IsSocketReady(pOverlapped)) ||
	    !HasOverlappedIoCompleted( pOverlapped ) ) {
	/*gHyp_util_debug("Read already in progress or already completed for %d",s);*/
	return 0 ;
      }
      
      if ( channelType == SOCKET_SERIAL && timeout != 0 ) {

        /*gHyp_util_debug("Setting comm timeouts for socket %d at %d msecs",s,abs(timeout));*/

        /* Retrieve the time-out parameters for all read/write operations on the port. */ 
        GetCommTimeouts ( (HANDLE)s, &CommTimeouts);
    
        /* Change the COMMTIMEOUTS structure settings */
        CommTimeouts.ReadIntervalTimeout = 100 ;  
        CommTimeouts.ReadTotalTimeoutMultiplier = 0 ;  
        CommTimeouts.ReadTotalTimeoutConstant = abs ( timeout ) ;    
        CommTimeouts.WriteTotalTimeoutMultiplier = 0;  
        CommTimeouts.WriteTotalTimeoutConstant = 0;    
    
        /* Set the time-out parameters for all read/write operation on the port.*/ 
        if (!SetCommTimeouts ((HANDLE) s, &CommTimeouts)) {
          if ( !silent ) gHyp_util_sysError ( "Unable to set the time-out parameters") ;
          return -1 ;
        }
      }

      /*gHyp_util_debug("Reading from socket %d, address %x, timeout=%d",s,pMsgOff,timeout) ;*/
      status = ReadFile( (HANDLE) s, 
                         pMsgOff, 
                         maxBytes, 
                         pNbytes, 
                         (LPOVERLAPPED) pOverlapped); 
      if ( !status ) {

        dwError = GetLastError() ; 

        if ( dwError == ERROR_IO_PENDING ) {

          /*gHyp_util_debug("Pending I/O on socket %d",s );*/
          if ( timeout < 0 ) return 0 ;

        }
        else {
          if ( !silent) gHyp_util_sysError ( "Failed to read from socket %d",s ) ;
          return -1 ;
        }
      }
      break ;

#elif defined ( AS_UNIX )

    case SOCKET_TCP :
    case SOCKET_LISTEN :
    case SOCKET_UNIX :
    case SOCKET_UNIX_LISTEN :
    case SOCKET_SERIAL :
    case SOCKET_FIFO :

      if ( timeout < 0 ) {
	/*gHyp_util_debug("FD_SET %d",s);*/
        FD_SET ( s, &gsReadFDS ) ;
      }
      else {
        /* Set the read for the single port */
        FD_ZERO ( &readFDS) ;
        FD_SET ( s, &readFDS ) ;
      }       
      nBytes = 0 ;
      break ;

#else
  
    /* VMS */

    /* Do not perform the 'qio' if there is already an overlapped IO in progress */
    /*if ( pOverlapped && !(pOverlapped)->ioStatus != 0) ) return 0 ;*/

    case SOCKET_TCP :
    case SOCKET_LISTEN :

      if ( timeout < 0 ) {
        FD_SET ( s, &gsReadFDS ) ;
      }
      else {
        /* Set the read for the single port */
        FD_ZERO ( &readFDS) ;
        FD_SET ( s, &readFDS ) ;
      }       
      nBytes = 0 ;
      break ;

    case SOCKET_FIFO :

      if ( timeout < 0 ) {

	/* Using select().
	 * Just set a write attention AST.  Blocking occurs in the select call,
	 * where the timeout is set as well.
	 * When data is available to be read, an event will be set in the AST.
	 */
	if ( guSIGMBX ) {

	  guSIGMBX = 0 ;

	  /*gHyp_util_debug("Setting writeAttn on %d",s);*/
	  status = sys$qiow( 0,
			     s,
			     IO$_SETMODE|IO$M_WRTATTN ,
			     (short*)pOverlapped,
			     0,0,
			     (void*)lHyp_sock_qioAST2,s,
			     0,0,0,0 ) ;
	  
	  /* Check for error */
	  if ( silent ) {
	    if ( !(status & 1) || !(pOverlapped->ioStatus & 1) ) return COND_ERROR ;
	  }
	  else if ( !gHyp_util_check ( status, pOverlapped->ioStatus ) ) {
	    gHyp_util_logError ( "qio failed on socket (%d)", s) ;
	    return COND_ERROR ;
	  }
	}
      }
      nBytes = 0 ;
      break ;

    case SOCKET_SERIAL :
      /**** NOTE **** 
       *
       * This is an asynchronous qio! 
       * We set a qio AST to fire when the read come in.
       * Not tested, so don't really know if it will work.
       */
      status = sys$qio ( 0,
                         s,
                         IO$_READVBLK,
                         (short*)pOverlapped,
                         (void (*)(...))lHyp_sock_qioAST,s,
                         pMsgOff,
                         maxBytes,
                         0,0,0,0 ) ;                                                

      /* Check for error */
      if ( silent ) {
	if ( !(status & 1) || !(pOverlapped->ioStatus & 1) ) return COND_ERROR ;
      }
      else if ( !gHyp_util_check ( status, pOverlapped->ioStatus ) ) {
        gHyp_util_logError ( "qio failed on socket (%d)", s) ;
        return COND_ERROR ;
      }
      nBytes = 0 ;
      break ;
#endif
    }
  }

  if ( timeout > 0 ) {

    /************************************************************************************
     * Wait for read to complete. 
     *************************************************************************************/

    switch ( channelType ) {

#if defined ( AS_WINDOWS )
        
    case SOCKET_TCP :

      /* Set timeout argument to useconds in select call 
       * Timeout is in milliseconds.
       */
      timer.tv_sec = timeout/1000 ;
      timer.tv_usec = ((timeout%1000)*1000) % 1000000 ;
      
      nBytes = select ( FD_SETSIZE, &readFDS, NULL, NULL, &timer ) ; 
      if ( nBytes < 0 ) {
        if ( !silent ) gHyp_util_sysError ( "Select failed on socket %d", s ) ;
        return COND_ERROR ;
      }
      else if ( nBytes == 0 ) {
        /* Timeout */
        return 0 ;
      }
      break;

    case SOCKET_LISTEN :
    case SOCKET_SERIAL :
    case SOCKET_FIFO :
      /*gHyp_util_debug("returning 0 bytes for ",s) ;*/
      nBytes = 0 ;
      break ;

#elif defined ( AS_UNIX )

    case SOCKET_UNIX :
    case SOCKET_UNIX_LISTEN :
    case SOCKET_TCP :
    case SOCKET_LISTEN :
    case SOCKET_SERIAL :
    case SOCKET_FIFO :

      /* Set timeout argument to t1 second in select call 
       * Timeout is in milliseconds
       */ ;
      timer.tv_sec = timeout / 1000 ;
      timer.tv_usec = ((timeout%1000)*1000) % 1000000 ;
      
      nBytes = select ( FD_SETSIZE, &readFDS, NULL, NULL, &timer ) ; 
      if ( nBytes < 0 ) {
        if ( !silent ) gHyp_util_sysError ( "Select failed on socket %d", s ) ;
        return COND_ERROR ;
      }
      else if ( nBytes == 0 ) {
        /* Timeout */
        return 0 ;
      }
#else

    /* VMS */
    case SOCKET_TCP :
    case SOCKET_LISTEN :

      /* Set timeout argument to t1 second in select call */
      timeout *= 1000 ;
      timer.tv_sec = timeout / 1000000 ;
      timer.tv_usec = timeout % 1000000 ;
      
      nBytes = select ( FD_SETSIZE, &readFDS, NULL, NULL, &timer ) ; 
      if ( nBytes < 0 ) {
        if ( !silent ) gHyp_util_sysError ( "Select failed on socket %d", s ) ;
        return COND_ERROR ;
      }
      else if ( nBytes == 0 ) {
        /* Timeout */
        return 0 ;
      }

      nBytes = 0 ;
      break ;

    case SOCKET_SERIAL :
    case SOCKET_FIFO :

      nBytes = 0 ;
      break ;

#endif
    }
  }

  if ( timeout >= 0 )  {

    /************************************************************************************
     * Get the result.
     *************************************************************************************/

    switch ( channelType ) {

#if defined ( AS_WINDOWS )

    case SOCKET_TCP :
      /* Get the result */
      nBytes = recv ( s, pMsgOff, maxBytes, 0 ) ;
      break ;

    case SOCKET_LISTEN :
      /* Not used here. See gHyp_tcp_checkInbound */
      nBytes = 0 ;
      break ;

    case SOCKET_SERIAL :
    case SOCKET_FIFO :
      if ( HasOverlappedIoCompleted( pOverlapped ) ) {

        /*gHyp_util_debug("Getting overlapped result");*/

        if ( GetOverlappedResult( (HANDLE) s, (LPOVERLAPPED) pOverlapped, pNbytes, FALSE ) ) {
          nBytes = *pNbytes ;
          /*gHyp_util_debug("Got %d bytes from overlapped I/O on socket %d at %x",nBytes,s,pMsgOff);*/
	  ResetEvent ( pOverlapped->hEvent ) ;
 	  pOverlapped->Internal = STATUS_READY ;
        }
        else {
          if ( !silent ) gHyp_util_sysError ( "Failed to get overlapped result from socket %d", s ) ;
          return COND_ERROR ;
        }
      }
      else {

        /*gHyp_util_debug("Waiting for overlapped I/O");*/

        if ( GetOverlappedResult( (HANDLE) s, (LPOVERLAPPED) pOverlapped, pNbytes, TRUE ) ) {
          nBytes = *pNbytes ;
          /*gHyp_util_debug("Got %d bytes from overlapped I/O on socket %d",nBytes,s);*/
          ResetEvent ( pOverlapped->hEvent ) ;
	  pOverlapped->Internal = STATUS_READY ;
        }
        else {
          if ( !silent ) gHyp_util_sysError ( "Failed to get overlapped result from socket %d", s ) ;
          return COND_ERROR ;
        }
      }
      break ;

#elif defined ( AS_UNIX )

    case SOCKET_UNIX :
      nBytes = 0 ;
      break ;

    case SOCKET_UNIX_LISTEN :
      nBytes = 0 ;
      break ;

    case SOCKET_TCP :
      nBytes = recv ( s, pMsgOff, maxBytes, 0 ) ;
      break ;

    case SOCKET_LISTEN :
      nBytes = 0 ;
      break ;

    case SOCKET_SERIAL :
    case SOCKET_FIFO :

      nBytes = read ( s, pMsgOff, maxBytes ) ;
      break ;

#else

    /* VMS */
    case SOCKET_TCP :
      nBytes = recv ( s, pMsgOff, maxBytes, 0 ) ;
      break ;
      
    case SOCKET_LISTEN :
      nBytes = 0 ;
      break ;
      
    case SOCKET_FIFO :

      if ( timeout == 0 ) 
        func = (IO$_READVBLK|IO$M_NOW) ;
      else {
        func = (IO$_READVBLK) ;
	/*gHyp_util_debug("ALARM in %d",timeout);*/
        alarm ( timeout ) ;
      }
      status = sys$qiow ( 0,
                          s,
                          func,
                          (short*)pOverlapped,
                          (void(*)(...))lHyp_sock_qioAST,s,
                          pMsgOff,
                          maxBytes,
                          0,0,0,0 ) ;
      
      if ( timeout > 0 ) {

	/*gHyp_util_debug("status=%d,iostatus=%d",status,pOverlapped->ioStatus);*/
        alarm ( 0 ) ;

        /* Return silently from timeout, i.e. I/O abort from sys$cancel */
        if ( (status & 1) && pOverlapped->ioStatus == SS$_ABORT ) {
	  /*gHyp_util_debug("ABORT on channel %d",s);*/
	  return COND_SILENT ;
        }
      }

      /* Check for zero bytes - equivalent to EWOULDBLOCK */
      if ( (status & 1) && 
           (pOverlapped->ioStatus & 1) && 
           pOverlapped->nBytes == 0 ) return 0 ;
      
      /* Convert end-of file errors to zero bytes read. */
      if ( (status & 1) && pOverlapped->ioStatus == SS$_ENDOFFILE ) return 0 ;
      
      /* Check for bad socket */
      if ( silent ) {
	if ( !(status & 1) || !(pOverlapped->ioStatus & 1) ) return COND_ERROR ;
      }
      else if ( !gHyp_util_check ( status, pOverlapped->ioStatus ) ) {
        gHyp_util_logError ( "Failed to read from MBX socket (%d)", s) ;
        return -1 ;
      }

      nBytes = pOverlapped->nBytes ;

      break ;
      
    case SOCKET_SERIAL :
      nBytes = pOverlapped->nBytes ;
      break ;
#endif
    }
  }
  
  if ( nBytes < 0 ) {
    
    /* Read error */
    
#ifdef AS_WINDOWS
    errno = GetLastError() ;
#endif
    
    if (  errno == EAGAIN || 
	  errno == EWOULDBLOCK ) return 0 ;
    
    /* Otherwise there was a problem. */
    if ( !silent ) gHyp_util_sysError ( "Failed to read from socket (%d)", s ) ;
    nBytes = -1 ;
  }
  
  if ( pNbytes ) *pNbytes = nBytes ;
  
  /* If no data received, then the socket is bad. */
  if ( nBytes <= 0 ) return nBytes ;
  
  /* Data received, null terminate the message. */
  pMsgOff[nBytes] = '\0' ;
  /*gHyp_util_debug("[%s]",pMsgOff);*/
  return nBytes ;
}

static int lHyp_sock_write ( SOCKET s, 
			     char *pMsg,  
			     int msgLen, 
			     short channelType,
			     LPOVERLAPPED pOverlapped,
			     sLOGICAL silent )
{
  /* Description:
   *
   *    Write to a socket
   *
   * Arguments:
   *
   *    s                       [R]
   *    - socket descriptor
   *
   *    pMsg                    [R]
   *    - message to send 
   *
   *    channelType                     [R]
   *    - type of channel
   *
   * Return value:
   *
   *    Returns number of bytes written.
   *
   */
   
#ifdef AS_VMS
  short int
    iosb[4] ;
  int           
    func,
    status ;
#endif

#ifdef AS_WINDOWS
  sLOGICAL
    status ;
  int
    dwError ;

#endif

  sLOGICAL 
    socketFull=FALSE ;

#ifdef AS_WINDOWS

#define INITIAL_WAIT_TIME       64
#define MAX_WAIT_TIME	      2048

#else

#define INITIAL_WAIT_TIME       64000
#define MAX_WAIT_TIME	      2048000

#endif

  unsigned int
    waitTime = INITIAL_WAIT_TIME ;
  
  int
    nBytes=0 ;
  
  while ( nBytes == 0 ) {

    switch ( channelType ) {

      case SOCKET_TCP:

	/*gHyp_util_debug("writing to socket %d, %d....",s, msgLen );*/
        nBytes = send ( s, pMsg, msgLen, 0 ) ;
        /*if(nBytes<0)gHyp_util_debug("Failed!");else gHyp_util_debug("Succeeded %d bytes",nBytes );*/

      break ;
    
    case SOCKET_SERIAL:
    case SOCKET_FIFO:
    
#ifdef AS_VMS
      /* VMS land */

      if ( channelType == SOCKET_FIFO )
	func = (IO$_WRITEVBLK | IO$M_NOW | IO$M_NORSWAIT ) ;
      else if ( channelType == SOCKET_SERIAL )
        func = IO$_WRITEVBLK ;

      status = sys$qiow ( 0,
                        s,
                        func,
                        (short*)pOverlapped,
                        0,0,
                        pMsg,
                        msgLen,
                        0,0,0,0 ) ;

      if ( silent ) {
	if ( !(status & 1) || !(pOverlapped->ioStatus & 1) ) return COND_ERROR ;
      }
      else if ( !gHyp_util_check ( status, pOverlapped->ioStatus ) ) {
	gHyp_util_logError ( 
	  "Failed to write to MBX socket (%u)", s ) ;
	return -1 ;
      }
      nBytes = pOverlapped->nBytes ;
      break ;
#else
#ifndef AS_WINDOWS
      /*gHyp_util_debug("write to fifo %d, %s",s, pMsg );*/
      nBytes = write ( s, pMsg, msgLen ) ;
      break ;
#else
      status = WriteFile( (HANDLE) s, 
			pMsg, 
                        msgLen, 
                        &nBytes, 
                        pOverlapped) ;
        
        
      if ( !status ) {
	dwError = GetLastError() ;
        if ( dwError == ERROR_IO_PENDING ) {
	  /*gHyp_util_debug("Pending write I/O, blocking for result");*/
          if ( GetOverlappedResult( (HANDLE) s, pOverlapped, &nBytes, TRUE ) ) {
	    ResetEvent ( pOverlapped->hEvent ) ;
   	    pOverlapped->Internal = STATUS_READY ;
          }
          else {
            if ( !silent ) gHyp_util_sysError ( "Failed to get overlapped result from socket %d",s ) ;
            nBytes = -1 ;
	  }
	}
      }

      break ;
#endif
#endif
    }

    if ( nBytes == -1 ) {

      /* Write error */
    
#ifdef AS_WINDOWS
      errno = GetLastError() ;
#endif
    
      if (  errno != EAGAIN &&
	    errno != EWOULDBLOCK ) {

	if ( !silent ) gHyp_util_sysError ( "Failed to write to socket (%d)", s ) ;
	return -1 ;
      }
      else {
	if ( !silent ) gHyp_util_logWarning ( "Socket %d is full", s ) ;
	socketFull = TRUE ;	
	nBytes = 0 ;
      }
    }

    if ( nBytes < msgLen && nBytes >= 0 ) {

      if ( !silent )
	gHyp_util_logWarning ( "Wrote %u out of %u bytes to socket %d", nBytes, msgLen, s ) ; 

      if ( gbDoEagain ) {

	/* Let the caller know about this special failure, call must CHECK! */
	gbIsEagain = 1 ;

	/* Clear the enabling of this feature.  Must be set before each use. */
	gbDoEagain = 0 ;

	/* Indicate where the buffer that wasn't written starts */
	memcpy ( gpEagainBuf, pMsg+nBytes,msgLen-nBytes ) ;
	giEagainBufLen = msgLen-nBytes ;
	    
	/* Let the caller know how many bytes were read. 
	 * We don't want to confuse normal return of 0 with a 0 from EAGAIN, so
	 * we make nBytes = msgLen when it is zero.
	 */
	if ( nBytes == 0 ) nBytes = msgLen ;

	return nBytes ;
      }

      if ( socketFull || nBytes > 0 ) {

	/* We need to set the write selector so that we can know when its
	 * ok to write to the socket
	 */
      
#ifndef AS_WINDOWS
	/* Microseconds - 4 million is the highest - 4 seconds */
	gHyp_util_logWarning ( "Sleeping for %u milliseconds", waitTime/1000 ) ;
	usleep ( waitTime ) ;

#else
	/* Milliseconds - 4000 is the highest = 4 seconds */
	gHyp_util_logWarning ( "Sleeping for %u milliseconds", waitTime ) ;
	Sleep ( waitTime ) ;
 
#endif

	if ( nBytes > 0 ) {
	  /* Case where at least some of the bytes were written */

	  /* Advance the buffer pointer accordingly */
	  pMsg += nBytes ;
	  msgLen -= nBytes ;

	  /* Reset the timer, we are hopeful again */
	  waitTime = INITIAL_WAIT_TIME ;
	} 
	else
	  /* The next time we wait, it'll be for twice as long */
	  waitTime *= 2 ; 

	nBytes = 0 ;

        /* Wait successively twice as long as the time before.
         * 64000 128000 256000 512000 1024000 2048000 micoseconds.
         * That sum totals almost 4 seconds, long enough because otherwise
	 * other sockets would not get serviced.  Another solution is to
	 * save the buffers and go back to the main FD_ISSET select mechanism, where the
	 * write operation would be acted on simultaneously with
	 * other sockets to be serviced.  But  this in itself is potentially
	 * problematic.  Consider secs1_rawOutgoing with SSL enabled for example.
	 * If one piece of a large chunk gets an EAGAIN, you would then have to save the 
	 * remainder of the data, go back and use FS_SET and FD_ISSET on the 
	 * socket to get the signal when it was ready to write again.
         */
	if ( waitTime > MAX_WAIT_TIME ) break ;

      }
      else
        /* Not socket full */
	nBytes = -1 ;
    }
  }

  return nBytes ;   
}

#ifdef AS_SSL
static int lHyp_sock_doSSL( sSSL* pSSL,
			    SOCKET s, 
			    char *pMsg,
			    int nBytes,
			    short channelType,
			    int timeout,
			    int *pNbytes,
			    LPOVERLAPPED pOverlapped)
{
  /* CASE 1: We have n bytes to encrypt and write. 
   * - Perform BIO_write .
   * - Do additional BIO_write and BIO_read until timeout or 
   *   SSL transaction is completed.
   *
   * CASE 2: We have n bytes we want to decrypt and read.
   * - Perform BIO_read.
   *   Do additional BIO_write and BIO_read until timeout or 
   *   SSL transaction is completed.	
   */

  int 
    n,
    tickCount1,
    tickCount2,
    maxWait,
    sslTimeout,
    outBioPending,
    filterBioPending=0,
    maxBytes,
    numBytes,
    bytesWrote=0,
    bytesRead=0,
    nReadFilter,
    nReadOut,
    nWriteFilter,
    nWriteOut,
    nReadSock,
    nWriteSock ;

  sLOGICAL 
    isReader=FALSE,
    isWriter=FALSE,
    shouldRead = FALSE,
    shouldWrite = FALSE,
    DEBUG=FALSE ;

  char 
    debug[60],
    *pBuf,
    *pDataBuf,
    *pSSLbuf ;

  if ( pNbytes == NULL ) {
    if ( guDebugFlags & DEBUG_DIAGNOSTICS )
      gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
	"SSL write, %d bytes",nBytes);
    isWriter = TRUE ;
    shouldWrite = TRUE ;
    sslTimeout = SSL_WAIT_INCREMENT ;
    maxBytes = SSL_BUFFER_SIZE ;
    numBytes = nBytes ;
  }
  else {
    if ( guDebugFlags & DEBUG_DIAGNOSTICS )
      gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
	"SSL read, max %d bytes, to=%d",nBytes,timeout) ;
    isReader = TRUE ;
    sslTimeout = timeout ;
    maxBytes = nBytes ;
    numBytes = 0 ;
  }
  if ( SSL_in_init ( pSSL->ssl ) ) {

    if ( guDebugFlags & DEBUG_DIAGNOSTICS )
      gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
	"In SSL handshake - %s", SSL_state_string_long( pSSL->ssl ) ) ;

    pSSL->state = -1 ;

    if ( pSSL->isClient ) {
      /* Client - we do sessions */
    }
    else {
      /* Server - we do sessions */
      /*
      SSL_set_verify(ssl,SSL_VERIFY_PEER |
      SSL_VERIFY_FAIL_IF_NO_PEER_CERT,0);
      */
      /* Stop the client from just resuming the
         un-authenticated session */
      /*
      SSL_set_session_id_context(ssl,
        (void *)&s_server_auth_session_id_context,
        sizeof(s_server_auth_session_id_context));
      
      if(SSL_renegotiate(ssl)<=0)
        berr_exit("SSL renegotiation error");
      if(SSL_do_handshake(ssl)<=0)
        berr_exit("SSL renegotiation error");
      ssl->state=SSL_ST_ACCEPT;
      if(SSL_do_handshake(ssl)<=0)
        berr_exit("SSL renegotiation error");
      */
    }
  }

  /********************************************************************
   *                 BIO_read                     BIO_write 
   *                (filterBio)   ____________     (outBio)
   *              /____(2)______ |            | /____(3)______
   *     APP       _____________ |   ENGINE   |  _____________  SOCKET   
   * (decrypted)       (1)     / |____________|      (4)     /  (encrypted)
   *                 BIO_write                    BIO_nread
   *                (filterBio)                   (outBio)
   *
   *  Modes:  1) isWriter (APP->SOCKET)  2) isReader (SOCKET->APP)
   *  States: 1) shouldRead  2) shouldWrite  3) filterBioPending
   *
   ********************************************************************/
  pBuf = pMsg ;
  pSSLbuf = gzSSLbuf ;
  maxWait = SSL_TIMEOUT  ;
  do {
  
    /*****************************************************************
     * Write decrypted data from application into the engine
     * if the SSL engine says we should and if there are
     * still decrypted bytes left to write.
     * 
     * This section is only done for SSL write operations.
     */
    if ( (shouldWrite && numBytes > 0) || (isWriter && filterBioPending > 0 )) {

      /* We must always write what the application requested */
      nWriteFilter = BIO_write( pSSL->filterBio, pBuf, numBytes );

      if ( nWriteFilter < 0 ) {

	if ( !BIO_should_retry ( pSSL->filterBio ) ) {
	  gHyp_util_logError ( "SSL App->Filter handshake error" ) ;
	  return -1 ;
	}
        if ( guDebugFlags & DEBUG_DIAGNOSTICS )
	  gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
	    "Retry App->Filter BIO_write later" ) ;
      }	
  
      else if ( nWriteFilter == 0 ) {

	if ( filterBioPending > 0 ) {
	  gHyp_util_logWarning ( "SSL App->Filter BIO_write error" ) ;
	  return -1 ;
	}
      }
      else {

	/* Engine accepted write. */
        if ( guDebugFlags & DEBUG_DIAGNOSTICS )
	  gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
	    "App->Filter BIO_write, %d bytes",nWriteFilter ) ;
        /*if ( DEBUG ) gHyp_util_debug("[%.*s]",nWriteFilter,pBuf);*/
	 
	numBytes -= nWriteFilter ;
	bytesWrote += nWriteFilter ;
        maxWait = SSL_TIMEOUT  ;
	if ( numBytes <= 0 ) {
	  shouldWrite = FALSE ;
          if ( guDebugFlags & DEBUG_DIAGNOSTICS )
	    gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
	      "App->Filter BIO_write all done" ) ;
	}
      }
    }
   
  
    /*****************************************************************
     * Read encrypted data from engine and return it to application.
     *
     * This section will only happen for SSL read operations.
     *
     * The 'shouldRead' flag, once set, will complete the 
     * operation the first occurence of any actual decrypted bytes 
     * being produced.
     * 
     */
    if ( shouldRead || (isReader && filterBioPending > 0 ) ) {

      if ( bytesRead >= maxBytes ) {
	 if ( guDebugFlags & DEBUG_DIAGNOSTICS )
            gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
	      "Received maximum of %d bytes",bytesRead);
      }

      /* Read decrypted data from engine into application */
      nReadFilter = BIO_read ( pSSL->filterBio, pSSLbuf, maxBytes );

      if ( nReadFilter < 0 ) {

	if ( !BIO_should_retry ( pSSL->filterBio ) ) {
	  gHyp_util_logError ( "SSL App<-Filter handshake error" ) ;
	  return -1 ;
	}
        if ( guDebugFlags & DEBUG_DIAGNOSTICS )
	  gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
	    "Retry App<-Filter BIO_read later" ) ;
      }	

      else if ( nReadFilter == 0 ) {

	if ( filterBioPending > 0 ) {
	  gHyp_util_logWarning ( "SSL App<-Filter BIO_read error" ) ;
	  return -1 ; 
	}
      }
      else {

	/* Read succeeded from engine..
	 * Pull decrypted data from engine.
	 */
	shouldRead = FALSE ;

	/* If have already given the application some data, don't
	 * give it any more right now
	 */

        if ( guDebugFlags & DEBUG_DIAGNOSTICS )
	  gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
	  "App<-Filter BIO_read, %d bytes",nReadFilter ) ;
	memcpy ( pBuf, pSSLbuf, nReadFilter ) ;
	pBuf[nReadFilter] = '\0' ;
	if ( DEBUG ) {
	  n = gHyp_util_unparseString ( debug, pBuf, nReadFilter, 60, FALSE, FALSE,FALSE, "" ) ; 
	  debug[n] = '\0' ;
	  gHyp_util_debug(debug) ;
	}
	pBuf += nReadFilter ;
	bytesRead += nReadFilter ;
      }
    }
    
    /******************************************************
     * Read encrypted data from socket and push into engine.
     *
     * This section is always done for SSL read operations,
     * but only done for SSL write operations when there
     * are still bytes left to write.
     * 
     */
    if ( (isReader && bytesRead==0) || numBytes > 0 ) {

      nWriteOut = BIO_get_write_guarantee ( pSSL->outBio ) ;

      if ( guDebugFlags & DEBUG_DIAGNOSTICS )
	gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
	  "Guarantee %d bytes write into engine",nWriteOut,maxBytes ) ;
      
      /*
      nReadSock = BIO_get_read_request ( pSSL->outBio ) ;
      if ( DEBUG ) gHyp_util_debug("Read request is %d bytes",nReadSock ) ;
      */

      if ( nWriteOut > 0 ) {

	nWriteOut = MIN ( SSL_BUFFER_SIZE, nWriteOut ) ;

	/*
	nWriteOut2 = nWriteOut ;
	if ( disguiseSSL ) {
	  nWriteOut2 = nWriteOut + disguisedHTTPheaderLen ;
	}
	*/
	if ( filterBioPending > 0 ) sslTimeout = 0 ;

        tickCount1 = gHyp_util_getclock() ;
	nReadSock = lHyp_sock_read (  s, 
				  pSSLbuf,
				  nWriteOut, /* nWriteOut2 */
				  channelType,
				  sslTimeout,
				  &nReadSock, /*nReadSock2 */
				  pOverlapped,
				  FALSE ) ;

	/* 
	nReadSock = nReadSock2 ;
	if ( disguiseSSL ) {
	  nReadSock = nReadSock2 - disguisedHTTPheaderLen ;
	}
	*/

	/* The first time around the timeout is zero, after that we
	 * set it to a small amount, so that we don't do a tight CPU spin
         */
	/*if ( sslTimeout == 0 ) sslTimeout = SSL_WAIT_INCREMENT ;*/

	if ( guDebugFlags & DEBUG_DIAGNOSTICS )
	  gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
	    "Engine<-Socket read, %d bytes, timeout=%d ms",nReadSock,sslTimeout ) ;

	/* 0 = no data, -1 = error, > 1 = nBytes */
	if ( nReadSock < 0 ) {
	  gHyp_util_logWarning ( "Failed Engine<-Socket" ) ;
	  return -1 ;
	}
	else if ( nReadSock == 0 ) {
          /*
	  if ( sslTimeout == 0 && filterBioPending == 0 ) {
	    gHyp_util_logWarning ( "Expecting bytes but zero received. Failed Engine<-Socket read" ) ;
	    return COND_SILENT ;
	  }
          */

          /* Did the select really wait?  If not, kill it */
          if ( sslTimeout > 0 ) {

            tickCount2 = gHyp_util_getclock() ;
            if ( tickCount2 < tickCount1 ) tickCount2 += 1000 ;
            if ( ( tickCount2 - tickCount1 ) < sslTimeout ) {
  	      /* Wait some more - SSL could be slower than expected */
	      gHyp_util_logWarning ( "Zero bytes received. Failed Engine<-Socket read" ) ;
	      return COND_SILENT ;
            }
          }
	  /* The first time around the timeout is zero, after that we
	   * set it to a small amount, so that we don't do a tight CPU spin
	   */
	  sslTimeout = SSL_WAIT_INCREMENT ;
	  maxWait -= SSL_WAIT_INCREMENT ;
        }
	else {

	  /* If conditional SSL was set, then this is the point we
	   * can redirect the user to an https connection
	   * Basically, if the flag is set and the buffer looks like a
	   * plaintext HTTP get request, then we redirect to https.
	   *
	   *  GET / HTTP/1.1
	   *
	   *  HTTP/1.1 301 Moved Permanently
	   *  Location: https://securebob.com/
	   *
	   */
	  if ( pSSL->condition[0] ) {
	    if ( nReadSock > 14 && strstr ( pSSLbuf, "GET / HTTP/1.1" ) == pSSLbuf ) {
	      /* ZAP that bastard! */ 
	      if ( guDebugFlags & DEBUG_DIAGNOSTICS )
		gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
		  "Redirection" ) ;
	      
 	      lHyp_sock_write (	
		s,
		pSSL->condition,
		strlen(pSSL->condition),
		channelType,
		pOverlapped,
		FALSE ) ;
		
	      /* We don't care about the socket nomore, let it hangup */
	      return -1 ;
	    }
	  }

	  sslTimeout = SSL_WAIT_INCREMENT ; /* If was zero, no longer */
	  nWriteOut = BIO_write( pSSL->outBio, pSSLbuf, nReadSock ) ;
	  maxWait = SSL_TIMEOUT ; /* Reset maximum wait */
      
          if ( guDebugFlags & DEBUG_DIAGNOSTICS )
	    gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
	      "Filter<-Engine BIO_write, %d bytes",nWriteOut ) ;
	}
      }
    }

    /*****************************************************************
     * Read encrypted data from engine and write out through socket.
     *
     * This occurs in both SSL read and write operations
     */

    outBioPending = BIO_ctrl_pending ( pSSL->outBio ) ;
    if ( guDebugFlags & DEBUG_DIAGNOSTICS )
      gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
	"Out BIO Pending = %d bytes",outBioPending);     
    
    if ( outBioPending > 0 ) {

      nReadOut = BIO_nread ( pSSL->outBio, &pDataBuf, outBioPending ) ;

      if ( guDebugFlags & DEBUG_DIAGNOSTICS )
	gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
	  "Engine->Socket, %d bytes ready to write",nReadOut ) ;

      if ( nReadOut > 0 ) {

	/*
	nReadOut2 = nReadOut ;
	pDataBuf2 = pDataBuf ;
	if ( disguisedSSL ) {
	  pDataBuf2 = diguisedSSLBuf ( pDataBuf, &nReadOut2 ) ;
	}
	*/

	/* nWriteSock2 = */
	nWriteSock = lHyp_sock_write (	s,
	 				pDataBuf, /* pDataBuf2 */
					nReadOut, /* nReadOut2 */
					channelType,
					pOverlapped,
					FALSE ) ;

	/*
	nWriteSock = nWriteSock2 ;
	if ( disguisedSSL ) {
	  nWriteSock = nWriteSock2 - disguisedSSLheaderLen
	*/

	if ( nWriteSock <= 0 ) {
	  gHyp_util_logWarning ( "Failed Engine->Socket write" ) ;
	  return -1 ;
	}
	else {
	  if ( guDebugFlags & DEBUG_DIAGNOSTICS )
            gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
	      "Engine->Socket write, %d bytes",nWriteSock ) ;
	  maxWait = SSL_TIMEOUT  ;
	  outBioPending -= nWriteSock ;
	}
      }
    }

    /* Determine if we still need to do more */
    filterBioPending = BIO_pending ( pSSL->filterBio ) ; 
    shouldWrite = BIO_should_write ( pSSL->filterBio ) ;
    shouldRead = BIO_should_read   ( pSSL->filterBio ) ;
    
    if ( guDebugFlags & DEBUG_DIAGNOSTICS )
      gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
	"Filter BIO: shouldWrite = %d, shouldRead = %d, pending = %d, outBioPending = %d",
	shouldWrite,
        shouldRead,
        filterBioPending,
        outBioPending );
  
    if (  isWriter && 
	  outBioPending == 0 && 
	  !shouldWrite &&
	  filterBioPending == 0 &&
	  numBytes == 0 ) {
      if ( guDebugFlags & DEBUG_DIAGNOSTICS )
	gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
	  "SSL write, handshake finished");

      break ;
    }

    else if ( isReader && 
	      bytesRead > 0 &&
	      outBioPending == 0 && 
	      filterBioPending == 0 ) {
      if ( guDebugFlags & DEBUG_DIAGNOSTICS )
	gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
	  "SSL read, handshake finished");

      break ;
    }

    if ( maxWait <= 0 ) { 

      gHyp_util_logWarning("SSL timeout - maybe increase past %d milliseconds",SSL_TIMEOUT);
      break ;
    }

  }
  while ( shouldRead || shouldWrite || filterBioPending > 0 || outBioPending > 0 ) ;

  if ( isReader )
    nBytes = bytesRead ;
  else
    nBytes = bytesWrote ;

  if ( pNbytes ) *pNbytes = nBytes ;

  if ( guDebugFlags & DEBUG_DIAGNOSTICS ) {
    if ( isReader ) 
      gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
	"DONE SSL, received %d bytes",nBytes);
    else
      gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
	"DONE SSL, wrote %d bytes",nBytes);
  }

  if ( pSSL->state == -1 ) {

    pSSL->state = SSL_session_reused( pSSL->ssl );
    if ( guDebugFlags & DEBUG_DIAGNOSTICS ) {

      if ( pSSL->state == 1 ) {
        gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
		"Previous SSL session has been reused" ) ;
      }
      else if ( pSSL->state == 0 ) {
	gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
	  "SSL session state is null" ) ;
      }
      else {
	gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
	  "New SSL state %d negotiated", pSSL->state ) ;
      }
    }

    /*gHyp_util_debug("Cache mode is %x",SSL_CTX_get_session_cache_mode( pSSL->sslCtx ) ) ;*/
  }

  return nBytes ;
}
#endif

int gHyp_sock_readJNI ( SOCKET s, 
			char *pMsgOff,
			int maxBytes,
			int timeout,
			int *pNbytes )
{

    return lHyp_sock_read ( s, 
			    pMsgOff,
			    maxBytes,
			    SOCKET_TCP,
			    timeout,
			    pNbytes,
			    NULL,
			    TRUE ) ;
}

int gHyp_sock_writeJNI (  SOCKET s, 
			  char *pMsg,  
			  int msgLen )
{
  return lHyp_sock_write (  s, 
			    pMsg,  
			    msgLen, 
			    SOCKET_TCP,
			    NULL,
			    TRUE ) ;
}

int gHyp_sock_read ( SOCKET s, 
                     char *pMsgOff,
                     int maxBytes,
                     short channelType,
                     int timeout,
                     int *pNbytes,
                     LPOVERLAPPED pOverlapped,
		     sSSL *pSSL )
{
#ifdef AS_SSL
  /* If the socket requires SSL, then we must process the
   * contents of the read through the SSL engine
   */
  if ( pSSL == NULL )
#endif
    return lHyp_sock_read ( s, 
			    pMsgOff,
			    maxBytes,
			    channelType,
			    timeout,
			    pNbytes,
			    pOverlapped,
			    FALSE) ;
#ifdef AS_SSL
  else
    return lHyp_sock_doSSL (pSSL,
			    s,
			    pMsgOff,
			    maxBytes,
			    channelType,
			    timeout,
			    pNbytes,
			    pOverlapped) ;
#endif
}



int gHyp_sock_write ( SOCKET s, 
                      char *pMsg,  
                      int msgLen, 
                      short channelType,
                      LPOVERLAPPED pOverlapped,
		      sSSL *pSSL )
{
#ifdef AS_SSL
  /* If the socket requires SSL, then we must process the
   * contents of the write through the SSL engine first.
   */
  if ( pSSL == NULL )
#endif
    return lHyp_sock_write (  s, 
			    pMsg,  
			    msgLen, 
			    channelType,
			    pOverlapped,
			    FALSE ) ;
#ifdef AS_SSL
  else
    return lHyp_sock_doSSL (	pSSL,
				s,
				pMsg,
				msgLen,
				channelType,
				0,
				NULL,
				pOverlapped) ;
#endif
}


#ifdef AS_DMBX
static sLOGICAL lHyp_sock_setDMBX ( char *mbxName ) 
{
  /* Description:
   *
   *    Set the name of the FASTech DMBX.
   *    If the the FASTech DMBX does not exist, the function call still
   *    return true. Psycho!
   *
   * Arguments:
   *
   *    mbxName         [R]
   *    - name of the DMBX
   *
   * Return value:
   *
   *    TRUE if function succeeded, FALSE otherwise
   *
   */
  if ( mbxSetName ( mbxName ) != MBX_SUCCESS )
    return gHyp_util_logError ( "Failed to set FASTech mbx %s: %s",
                                mbxName, mbxStringPerror() ) ;
  else
    return TRUE ;
}
#endif
  
void gHyp_sock_closeJNI ( SOCKET socket) 
{
  /* Description:
   *
   *    Close a socket.
   *
   */

#ifdef AS_WINDOWS
    closesocket ( socket ) ;
#else
#if defined(AS_GONG) && defined(AS_VAXC)
    netclose ( socket ) ;
#else
    close ( socket ) ;
#endif
#endif

  return ;
}

void gHyp_sock_close ( SOCKET socket, short channelType, char* target, char* path ) 
{
  /* Description:
   *
   *    Close a socket.
   *
   */

#ifdef AS_UNIX
  struct stat
    buf ;
#endif
#ifdef AS_VMS
  int
    status ;
#endif

  switch ( channelType ) {

  case SOCKET_FIFO :
    gHyp_util_logInfo ( "Closing FIFO connection (%d) to device '%s'",
                         socket, target ) ;

  case SOCKET_SERIAL :
    if ( channelType == SOCKET_SERIAL )
      gHyp_util_logInfo ( "Closing Serial connection (%d) to device '%s'",
                          socket, target ) ;
#ifdef AS_WINDOWS
    CloseHandle ( (HANDLE) socket ) ;
#else
#ifdef AS_VMS
    status = sys$dassgn ( (short int) socket ) ;
    if ( !gHyp_util_check ( status, 1 ) ) 
      gHyp_util_logWarning ( "Failed to deassign MBX connection" ) ;
#else
    close ( socket ) ; 
    if ( *path ) 
      if ( stat ( path, &buf ) != -1 ) {
        /*gHyp_util_logInfo ( "Removing '%s'",path ) ;*/
        unlink ( path ); 
      }
#endif
#endif
    break ;
    
  case SOCKET_DMBX :
    gHyp_util_logInfo ( "Closing FASTech mbx connection (%d) to %s at %s", 
                        socket, target, path ) ;
#ifdef AS_DMBX 
    lHyp_sock_setDMBX ( object ) ;
    if ( mbxClose ( socket ) != MBX_SUCCESS ) 
      gHyp_util_logError ( "Failed to close FASTech mbx: %s", mbxStringPerror() ) ; 
#endif
    break ;

  case SOCKET_TCP:
    gHyp_util_logInfo ( "Closing TCP client connection (%d) to device '%s'",
                        socket, target ) ;

  case SOCKET_UNIX_LISTEN:
    if ( channelType == SOCKET_UNIX_LISTEN )
      gHyp_util_logInfo ( "Closing UNIX listen connection (%d) to device '%s'",
                          socket, target ) ;
  case SOCKET_LISTEN:
    if ( channelType == SOCKET_LISTEN )
      gHyp_util_logInfo ( "Closing TCP listen connection (%d) to device '%s'",
                          socket, target ) ;
  case SOCKET_UNIX:
    if ( channelType == SOCKET_UNIX )
      gHyp_util_logInfo ( "Closing UNIX connection (%d) to device '%s'",
                          socket, target ) ;

    
#ifdef AS_WINDOWS
    closesocket ( socket ) ;
#else
#if defined(AS_GONG) && defined(AS_VAXC)
    netclose ( socket ) ;
#else
    close ( socket ) ;
#endif
#endif
    break ;
  }

  return ;
}


void gHyp_sock_shutdown ( SOCKET listenTCP,
                          sData *pClients,
                          sData *pHosts,
                          sData *pSockets,
                          sConcept *pConcept )
{     
  /* Description:
   *
   *    Do a orderly shutdown of all sockets.
   */
  sChannel
    *pChannel ;

  sAImsg
    *pAImsg ;

  sLOGICAL
    f ;

  sData
    *pFirst,
    *pData ;

  char
    msg[MIN_MESSAGE_SIZE+1],
    target[OBJECT_SIZE+1],
    instance[INSTANCE_SIZE+1],
    concept[OBJECT_SIZE+1],
    parent[OBJECT_SIZE+1],
    root[OBJECT_SIZE+1],
    host[HOST_SIZE+1] ; 

#ifdef AS_VMS
  int i ;
#endif

  gHyp_util_output ( "==Shutdown===\n" ) ;

  /* Free the socket structures */
  gHyp_util_logInfo ( "Closing application channels" ) ;
  gHyp_data_delete ( pSockets ) ;

  /* List open channels */
  for ( f=TRUE, pData = pFirst = gHyp_data_getFirst ( pClients ) ;
        (f && pData) || pData != pFirst;
        f=FALSE, pData = gHyp_data_getNext ( pData ) ) {

    pChannel = (sChannel*) gHyp_data_getObject ( pData ) ;
    /*gHyp_util_debug( "   %s at socket %d",
		     gHyp_channel_target(pChannel),
		     gHyp_channel_socket(pChannel) ) ;
    */
  }

  /* Close all the client channels */
  gHyp_util_logInfo ( "Closing client channels" ) ;
  pAImsg = gHyp_aimsg_new() ;
  for ( f=TRUE, pData = pFirst = gHyp_data_getFirst ( pClients ) ;
        (f && pData) || pData != pFirst;
        f=FALSE, pData = gHyp_data_getNext ( pData ) ) {

    pChannel = (sChannel*) gHyp_data_getObject ( pData ) ;

    strcpy ( target, gHyp_channel_targetId(pChannel) );  

    gHyp_util_breakTarget ( target, instance, concept, parent, root, host ) ;

    if ( (gHyp_channel_flags ( pChannel ) & MASK_SOCKET) != SOCKET_DMBX ) {
      sprintf ( msg,
                "|%s%s|event|ABORT|%s%s|%s|%s|||", 
                concept,
		root,
                gzConcept,
                gzRoot,
                gHyp_util_random8 (),
                gHyp_util_timeStamp ( gsCurTime ) ) ;
      gHyp_aimsg_parse ( pAImsg, msg ) ;
      gHyp_sock_putChannel ( pChannel, msg, pAImsg ) ;
    }
  }
  gHyp_aimsg_delete ( pAImsg ) ;
  gHyp_data_delete ( pClients ) ;

  /* Shutdown all the hosts */
  gHyp_util_logInfo ( "Closing network channels" ) ;
  gHyp_data_delete ( pHosts ) ;
  
  /* Remove the table of aliases */
  gHyp_tcp_deleteAliasTable() ;
  
  gHyp_concept_closeReader ( pConcept ) ;
  gHyp_concept_closeWriter ( pConcept ) ;

  if( listenTCP != INVALID_SOCKET ) 
    gHyp_sock_close ( listenTCP, SOCKET_LISTEN, gzLocalHost, gzLocalAddr ) ;

  if ( giListenLoopback_R != INVALID_SOCKET )
    gHyp_sock_close ( giListenLoopback_R, SOCKET_TCP, gzLocalHost, gzLocalAddr ) ;

  if ( giListenLoopback_W != INVALID_SOCKET )
    gHyp_sock_close ( giListenLoopback_W, SOCKET_TCP, gzLocalHost, gzLocalAddr ) ;

  return ;

}

#ifdef AS_VMS
static sChannel* lHyp_sock_getMBOXchannel ( char *object, 
					    char *targetId,
					    sLOGICAL create,
					    sLOGICAL alreadyExists,
					    sLOGICAL isParent ) 
{
  /* Description:
   *
   *    Create (or re-open) a channel for a VMS mailbox client.
   *
   */

  char
    path[MAX_PATH_SIZE+1] ;

  HANDLE
    s ;
 
  sChannel
    *pChannel = NULL ;

  sLOGICAL
    isRead=FALSE, isWrite=TRUE ;

  if ( isParent ) {
    sprintf ( path, "%s", gzOutboxPath ) ;
  }
  else {
    /* Client */
    sprintf ( path, "%s_%s", gzConceptPath, object ) ;
  }

  /* Create path to mailbox */
  gHyp_util_upperCase ( path, strlen ( path ) ) ;


  if ( (s = gHyp_sock_fifo (  path, 
			      create, 
			      isRead, 
			      isWrite, 
			      alreadyExists ) ) != INVALID_HANDLE_VALUE ) {	
  
    pChannel = gHyp_channel_new ( object,
                                  path,
                                  targetId,
                                  (SOCKET_FIFO | PROTOCOL_AI),
                                  s ) ;

    /* Set or update attributes */ 
    gHyp_util_logInfo ( "Connected to mailbox %s (%d) for client MBX '%s'", 
                        object,
                        s,
                        path ) ;
    /* Force evaluation of timeout */
    giNextAlarm = gsCurTime ;

  }
  return pChannel ;
}
#endif

#ifdef AS_UNIX
static sChannel* lHyp_sock_getFIFOchannel ( char *object, 
					    char *targetId,
					    sLOGICAL create,
					    sLOGICAL alreadyExists,
					    sLOGICAL isParent ) 
{
  /* Description:
   *
   *    Create (or re-open) a socket for a UNIX fifo client.
   */

  char
    path[MAX_PATH_SIZE+1] ;

  HANDLE
    s ;

  sChannel
    *pChannel = NULL ;

  sLOGICAL
    isRead=FALSE, isWrite=TRUE ;

  if ( isParent ) {
    sprintf ( path, "%s", gzOutboxPath ) ;
  }
  else {
    /* Client */
    sprintf ( path, "%s/%s", gzConceptPath, object ) ;
  }


  /* Create (or re-open) a FIFO to read from */ 
  if ( (s = gHyp_sock_fifo (  path, 
			      create, 
			      isRead, 
			      isWrite, 
			      alreadyExists ) ) != INVALID_HANDLE_VALUE ) {
  
    pChannel = gHyp_channel_new ( object,
                                  path, 
                                  targetId,
                                  (SOCKET_FIFO | PROTOCOL_AI),
                                  s ) ;

    /* Set or update attributes */ 
    gHyp_util_logInfo ( "Connected to fifo %s (%d) for client MBX '%s'", 
                        object,
                        s,
                        path ) ;

    /* Force evaluation of timeout */
    giNextAlarm = gsCurTime ;

  }
  return pChannel ;
}
#endif

#ifdef AS_WINDOWS
static sChannel* lHyp_sock_getMSLOTchannel ( char *object, 
					     char *targetId,
					     sLOGICAL create,
					     sLOGICAL alreadyExists,
					     sLOGICAL isParent ) 
{
  /* Description:
   *
   *    Create (or re-open) a socket for a WINDOWS mailslot client.
   */

  char
    path[MAX_PATH_SIZE+1] ;

  HANDLE
    s ;

  sChannel
    *pChannel = NULL ;

  sLOGICAL
    isRead=FALSE, isWrite=TRUE ;

  if ( isParent ) {
    sprintf ( path, "%s", gzOutboxPath ) ;
  }
  else {
    /* Client */
    /* In windows, the client must create their own mailslot because the creator only gets 
     * read-access and we (the parent) need to have write-access. 
     * Therefore, we can only open existing mailslots, created by the client.
     *
     * Force 'create' to be FALSE
     */
    create = FALSE ;
    sprintf ( path, "%s\\%s", gzConceptPath, object ) ;
  }

  /* Create (or re-open) a mailslot to read from */ 

  if ( (s = gHyp_sock_fifo ( path,
                             create,   /* Must be false for clients */ 
                             isRead,    
                             isWrite,         
                             alreadyExists ) ) != INVALID_HANDLE_VALUE ) {
  
    pChannel = gHyp_channel_new ( object,
                                  path, 
                                  targetId,
                                  (SOCKET_FIFO | PROTOCOL_AI),
                                  (SOCKET) s ) ;

    /* Set or update attributes */ 
    gHyp_util_logInfo ( "Connected to mailslot %s (%d) for client MBX '%s'", 
                        object,
                        s,
                        path ) ;

    /* Force evaluation of timeout */
    giNextAlarm = (int)gsCurTime ;

  }
  return pChannel ;
}
#endif

#ifdef AS_DMBX
static sChannel* lHyp_sock_getDMBXchannel ( char *object,
					    char *targetId,
                                            sLOGICAL create,
					    sLOGICAL alreadyExists,
					    sLOGICAL isParent ) 
{
  /* Description:
   *
   *    Create (or re-open) a channel for a FASTech DMBX mailbox.
   *
   */

  int   
    i,
    s ;

  sChannel
    *pChannel = NULL ;

  if ( (s = mbxOpen ( object, "q" ) ) != DMBX_INVALID_BOX ) {
  
    pChannel = gHyp_channel_new ( object,
                                  targetId,
                                  targetId,
                                  (SOCKET_DMBX | PROTOCOL_AI),
                                  s ) ;
    /* Set or update attributes */ 
    gHyp_util_logInfo ( "Created DMBX read socket %s (%d) for client MBX '%s'", 
                        object,
                        s,
                        targetId ) ;
  }
  return pChannel ;
}
#endif

static sChannel* lHyp_sock_channel ( char *object, 
				     char *targetId,
				     sLOGICAL create, 
				     sLOGICAL alreadyExists,
				     sLOGICAL isParent )
{
  /* Description:
   *
   *    Common routine to create (or re-open) a client socket.
   *
   */

  sChannel
    *pChannel = NULL ;

#ifdef AS_DMBX

  /* If the FASTech mbx exists, then the mailbox has to be there. */
  if ( lHyp_sock_setDMBX ( object ) )
    pChannel = lHyp_sock_getDMBXchannel ( object, targetId, create, alreadyExists, isParent ) ;

#endif

#ifdef AS_UNIX

  /* Create a FIFO */
  if ( !pChannel) 
    pChannel = lHyp_sock_getFIFOchannel ( object, targetId, create, alreadyExists, isParent ) ;
  
#endif

#ifdef AS_WINDOWS

  /* Create a Mailslot */
  if ( !pChannel) 
    pChannel = lHyp_sock_getMSLOTchannel ( object, targetId, create, alreadyExists, isParent ) ;
  
#endif

#ifdef AS_VMS

  /* Create a VMS mailbox */
  if ( !pChannel)
    pChannel = lHyp_sock_getMBOXchannel (  object, targetId, create, alreadyExists, isParent ) ;
  
#endif

  return pChannel ;
}

sData* gHyp_sock_createNetwork ( sData *pHosts, char *host, char *addr, SOCKET s )
{
  /* Description:
   *
   *    Create a network (network) sChannel object from newly created
   *    socket channel (s).
   *
   */

  /* Find the socket that identifies the specified host target */
  sData
    *pData = gHyp_data_getChildByName ( pHosts, addr )  ;

  sChannel
    *pChannel = NULL ;

  SOCKET
    socket ;

  /* Look to see if the connection already exists */
  if ( pData ) pChannel = (sChannel *) gHyp_data_getObject ( pData ) ;

  if ( pChannel ) {

    /* Get old socket value */
    socket = gHyp_channel_socket ( pChannel ) ; 
     
    gHyp_util_logInfo ( 
		"Closing existing network socket (%u) assigned to host '%s' at '%s'", 
		socket,
		gHyp_channel_path ( pChannel ),
		gHyp_channel_target ( pChannel ) ) ;

    gHyp_data_detach ( pData ) ;
    gHyp_data_delete ( pData ) ;
    pData = NULL ;
    pChannel = NULL ;
  }
  
  if ( !pChannel ) {
    /* Create new socket descriptor */
    pChannel = gHyp_channel_new (	addr, 
					host,
                                        host,
					(short)(SOCKET_TCP | PROTOCOL_AI),
					s ) ;
    gHyp_util_logInfo ( 
		"Created network socket (%u) assigned to host '%s' at '%s'", 
		s,
		host,
		addr ) ;
  }

  if ( !pData ) {
    pData = gHyp_data_new ( addr ) ;
    gHyp_data_setObject ( pData,
                          pChannel,
                          DATA_OBJECT_CHANNEL,
                          (void (*)(void*)) gHyp_channel_delete ) ;
    gHyp_data_append ( pHosts, pData ) ;
  }

  return pData ;
}
 
sData * gHyp_sock_findClient ( sData *pClients,  char *object, char *targetId )
{
  /* Description:
   *
   *    Find (or re-open) a client socket if it already exists.
   */

  sData
    *pData = gHyp_data_getChildByName ( pClients, object ) ;

  sChannel
    *pChannel ;

  if ( !pData ) {
    /* Maybe fifo is still there and we can reconnect to it */
    pChannel = lHyp_sock_channel ( object, targetId, CHANNEL_FIND, FALSE, FALSE ) ;
    if ( pChannel ) {
      pData = gHyp_data_new ( object ) ;
      gHyp_data_setObject ( pData,
                            pChannel,
                            DATA_OBJECT_CHANNEL,
                            (void (*)(void*)) gHyp_channel_delete ) ;
      gHyp_data_append ( pClients, pData ) ;
      if ( guDebugFlags & DEBUG_DIAGNOSTICS )
        gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
                             "Created client %s",object );
    }
    else
      if ( guDebugFlags & DEBUG_DIAGNOSTICS )
        gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
                             "No client %s",object );
    
  }
  else
     if ( guDebugFlags & DEBUG_DIAGNOSTICS )
      gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
                           "Found client %s",object );
  

  return pData ;
}

 
sData * gHyp_sock_createClient ( sData *pClients, 
				 char *object,
				 char *targetId,
				 sLOGICAL alreadyExists )
{
  /* Description:
   *
   *    Find, re-open, or create a client socket.
   */

  sData
    *pData = gHyp_data_getChildByName ( pClients, object ) ;

  sChannel
    *pChannel ;

  if ( !pData ) {
    /* Maybe fifo is still there and we can reconnect to it */
    pChannel = lHyp_sock_channel ( object, targetId, CHANNEL_CREATE, alreadyExists, FALSE ) ;
    if ( pChannel ) {
      pData = gHyp_data_new ( object ) ;
      gHyp_data_setObject ( pData,
                            pChannel,
                            DATA_OBJECT_CHANNEL,
                            (void (*)(void*)) gHyp_channel_delete ) ;
      gHyp_data_append ( pClients, pData ) ;
      if ( guDebugFlags & DEBUG_DIAGNOSTICS )
        gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
                             "Created client %s",object );
    }
    else
      if ( guDebugFlags & DEBUG_DIAGNOSTICS )
        gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
                             "No client %s",object );
    
  }
  else
     if ( guDebugFlags & DEBUG_DIAGNOSTICS )
      gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
                           "Found client %s",object );
  

  return pData ;
}

sData * gHyp_sock_findParent ( char *object, char *targetId )
{
  /* Description:
   *
   *    Find (or re-open) a parent socket if it already exists.
   */

  sData
    *pData=NULL ;

  sChannel
    *pChannel ;

  pChannel = lHyp_sock_channel ( object, targetId, CHANNEL_FIND, FALSE, TRUE ) ;
  if ( pChannel ) {

    pData = gHyp_data_new ( object ) ;
    gHyp_data_setObject ( pData,
                          pChannel,
                          DATA_OBJECT_CHANNEL,
                          NULL ) ;

    if ( guDebugFlags & DEBUG_DIAGNOSTICS )
      gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
                           "Created parent %s",object );

  }
  else {
    if ( guDebugFlags & DEBUG_DIAGNOSTICS )
      gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
                             "No parent %s",object );
    
  }

  return pData ;
}

 
sData * gHyp_sock_findNetwork ( sData *pHosts, char *addr )
{
  /* Description:
   *
   *  Find the network socket that identifies the specified target host.
   *
   * Arguments:
   *
   *    addr                                            [R]
   *    - ip address
   *
   * Return value:
   *
   *    Pointer to network sChannel object.
   *
   */


  /* Find the socket that identifies the specified client target */
  return gHyp_data_getChildByName ( pHosts, addr ) ;
}
   
void gHyp_sock_cleanClient ( sData *pClients )
{
  /* Description:
   *
   *    Clean out idle sockets and stuck fifos.
   *    Close client sockets idling for more than IDLE_INTERVAL seconds
   *
   * Arguments:
   *
   *    none
   *
   * Return value:
   *
   *    none
   *
   */

  int
    nBytes,
    elapsedTime ;
  
  sData
    *pData,
    *pFirst,
    *pNext ;
  
  sChannel
    *pChannel ;
  
  char
    *target,
    *pBuf,
    timeStamp[DATETIME_SIZE+1] ;

  time_t
    updateTime ;

  sLOGICAL
    f ;

  SOCKET
    s ;

  OVERLAPPED *pOverlapped ;

  char
    msg[MIN_MESSAGE_SIZE+1],
    instance[INSTANCE_SIZE+1],
    concept[OBJECT_SIZE+1],
    parent[OBJECT_SIZE+1],
    root[OBJECT_SIZE+1],
    host[HOST_SIZE+1] ; 

  sAImsg
    *pAImsg ;

  gsCurTime = time(NULL) ;
  if ( guDebugFlags & DEBUG_DIAGNOSTICS )
    gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
                         "Checking for idle connections" ) ;

  for ( f=TRUE, pData = pFirst = gHyp_data_getFirst ( pClients ) ;
        (f && pData) || (pFirst && pData != pFirst );
        f=FALSE, pData = pNext, pFirst = gHyp_data_getFirst ( pClients ) ) {
        
    pNext = gHyp_data_getNext ( pData ) ;

    pChannel = (sChannel*) gHyp_data_getObject ( pData ) ;
    /*gHyp_util_debug("Looking at socket %d",gHyp_channel_socket(pChannel));*/
   
#if AS_DMBX
    /* DMBX sockets never expire */
    if ( gHyp_channel_flags ( pChannel ) & SOCKET_DMBX ) 
      gHyp_channel_setUpdateTime ( pChannel, gsCurTime ) ;
#endif

    updateTime = gHyp_channel_updateTime ( pChannel ) ;
    strcpy ( timeStamp, gHyp_util_timeStamp ( updateTime ) ) ; ;
    elapsedTime = (int)(gsCurTime - updateTime) ;

    if ( elapsedTime < 0 ) elapsedTime = IDLE_INTERVAL ;
    if ( elapsedTime >= IDLE_INTERVAL ) {
      
      /* Try to read from the client socket */
      s = gHyp_channel_socket ( pChannel ) ;
      pOverlapped = gHyp_channel_overlapped( pChannel ) ;
      pBuf = gHyp_channel_buffer ( pChannel ) ;
#ifdef AS_VMS
      nBytes = lHyp_sock_read ( s, 
                                pBuf,
                                MAX_MESSAGE_SIZE,
                                (short)(gHyp_channel_flags(pChannel)&MASK_SOCKET),
                                0,
                                &nBytes,
                                pOverlapped,
                                FALSE) ;
#else
      nBytes = 0 ;
#endif
      if ( nBytes > 0 ) {
        
        /* Client has not read from socket. */
        gHyp_util_logInfo (     
                           "STUCK FIFO: Client '%s' has not read from socket since %s",
                           gHyp_channel_target ( pChannel ),
                           timeStamp) ;
        *(pBuf+nBytes) = '\0' ;
        gHyp_util_logInfo( "Discarding unread message of %d bytes: '%s'",nBytes,pBuf ) ;
        gHyp_util_logInfo( "First byte is %d", (int) *pBuf ) ;

        if ( pNext == pData ) pNext = NULL ;
        gHyp_data_detach ( pData ) ;
        gHyp_data_delete ( pData ) ;
	pData = NULL ;
        pFirst = gHyp_data_getFirst ( pClients ) ;

      }
      else if ( nBytes < 0 ) {
        
        /* Bad socket or end-of-file on socket. */
        gHyp_util_logInfo (     "BROKEN FIFO: Client '%s' is disconnected from socket", 
                                gHyp_channel_target ( pChannel ) ) ;
        if ( pNext == pData ) pNext = NULL ;
        gHyp_data_detach ( pData ) ;
        gHyp_data_delete ( pData ) ;
	pData = NULL ;
        pFirst = gHyp_data_getFirst ( pClients ) ;
      }
      else {    /* nBytes == 0 */
        
	/* Close targets beginning with a random 8 digit hex number */
        target = gHyp_channel_targetId ( pChannel ) ;
        if ( strspn ( target, "0123456789abcdef" ) == 8 ) {

	  gHyp_util_logInfo (
              "Client instance '%s' has been idle since %s",
              target,
              timeStamp ) ;

          gHyp_util_breakTarget ( target, instance, concept, parent, root, host ) ;
	  pAImsg = gHyp_aimsg_new() ;
          sprintf ( msg,
                "|%s%s|event|ABORT|%s%s|%s|%s|||", 
                concept,
		root,
                gzConcept,
                gzRoot,
                gHyp_util_random8 (),
                gHyp_util_timeStamp ( gsCurTime ) ) ;
          gHyp_aimsg_parse ( pAImsg, msg ) ;
          gHyp_sock_putChannel ( pChannel, msg, pAImsg ) ;
          gHyp_aimsg_delete ( pAImsg ) ;

          if ( pNext == pData ) pNext = NULL ;
          gHyp_data_detach ( pData ) ;
          gHyp_data_delete ( pData ) ;
	  pData = NULL ;
          pFirst = gHyp_data_getFirst ( pClients ) ;
        }
	else {
          /* Give the object client another IDLE_INTERVAL seconds */
          /*gHyp_util_debug (     "EMPTY FIFO: Client '%s'", 
                                gHyp_channel_target ( pChannel ) ) ;
	  */
          gHyp_channel_setUpdateTime ( pChannel, gsCurTime ) ;
          gHyp_channel_clearStats ( pChannel ) ;
	}
	}
    }
  }
  return ;  
}

void gHyp_sock_list ( sData *pClients, sData *pHosts ) 
{
  /* Description:
   *
   *    List (and display statistics for) all of the sockets.
   *
   * Arguments:
   *
   *    none
   *
   * Return value:
   *
   *    none
   *
   */

  sChannel
    *pChannel ;

  sData
    *pData,
    *pFirst ;

  sLOGICAL
    f ;
  
  gHyp_util_output ( "==Statistics=\n" ) ;
  gHyp_util_logInfo ( "HyperScript Version %s (%s)", VERSION_HYPERSCRIPT, VERSION_BUILD ) ;

  gHyp_util_output ( "\nFifo clients:" ) ;
  for ( f=TRUE, pData = pFirst = gHyp_data_getFirst ( pClients ) ;
        (f && pData) || pData != pFirst;
        f=FALSE, pData = gHyp_data_getNext ( pData )) {
    pChannel = (sChannel*) gHyp_data_getObject ( pData ) ;
    gHyp_channel_displayStats ( pChannel, FALSE ) ;

  }
  
  gHyp_util_output ( "\nNetwork connections:" ) ;
  for ( f=TRUE, pData = pFirst = gHyp_data_getFirst ( pHosts ) ;
      (f && pData) || pData != pFirst;
        f=FALSE, pData = gHyp_data_getNext ( pData )) {
    pChannel = (sChannel*) gHyp_data_getObject ( pData ) ;
    gHyp_channel_displayStats ( pChannel, FALSE ) ;
  }

  return ;
}
/****************
static int lHyp_sock_mapUnMapDrive(
			  char *pLocalAddr,
			  char *pServerName,
			  char *pDriveLetter,
			  sLOGICAL  doMap )
{

  if ( doMap ) 
    gHyp_util_debug ( "Drive mapping %s to %s to %s",
		      pLocalAddr, pServerName, pDriveLetter ) ;
  else
    gHyp_util_debug ( "Drive un-mapping %s to %s to %s",
		      pLocalAddr, pServerName, pDriveLetter ) ;

  if ( doMap )  {

      for ( int i=0 ; i<iSizeUTF ; i++ ) { 
 	shareDirUTF[i] = sdUTF[i] ;
      } 
      shareDirUTF[iSizeUTF] = '\0' ;
      pShareDir = shareDirUTF ;
      debug_printf (1, "Drive mapping share (UTF) is : %s\n", pShareDir ) ;

      rc = MapDrive(
	     NULL,	      // IN: Owner window
	     pLocalAddr,      // IN: eg: 127.0.0.2
	     pServerName,     // IN:
	     pShareDir,	      // IN:
	     pDriveLetter,    // IN: Takes next available if = ""
	     szAccessName,    // OUT: Local device name (drive letter) that was assigned.
	     &accessNameSize, // IN:OUT: The size, in characters, of the lpAccessName buffer
	     szErrorText,     // OUT: If error, this tells what might have happened
	     VALUE_SIZE ) ;

      if ( rc != 0 ) {
        debug_printf (1, "Drive mapping failed, reason is : %s\n", szErrorText ) ;
        accessNameSize = sprintf ( szAccessName, "%%%s", szErrorText ) ;
      }
      else {
        szAccessName[accessNameSize] = '\0' ;
      }

    }
    else {

      debug_printf (1, "Removing lmhosts entries" ) ;
      rc = iUpdateLMHOSTSFile(Flags, "" ) ;
      if ( rc != 0 ) {
        debug_printf (1, "Remove lmhosts entries failed" ) ;
      }

      debug_printf (1, "Unmapping drives" ) ;
      rc = UnmapDrive( "*", szErrorText, VALUE_SIZE ) ;
      if ( rc != 0 ) {
        debug_printf (1, "Unmap drive entry failed" ) ;
        accessNameSize = sprintf ( szAccessName, "%%%s", szErrorText ) ;
      }
      else {

      strcpy ( szAccessName, "*" ) ;
	accessNameSize = 1 ;
      }
    }    
}

int lHyp_sock_driveMapperThread(
	)
{

    HookStruct *psPorts;
    HANDLE hThread;
    ULONG ulThreadId;
    int rc;

    psPorts = (HookStruct *)malloc(sizeof(*psPorts));

    psPorts->iExchangeRFRPort = iExchangeRFRPort;
    psPorts->iExchangeDIRPort = iExchangeDIRPort;
    psPorts->iExchangeSTOREPort = iExchangeSTOREPort;



    hThread = CreateThread (
		NULL,			    // SecurityAttributes,
		(16*1024),		    // Thread stack size.
		(LPTHREAD_START_ROUTINE)iHookIntoPortMapperThread,  // StartFunction,
		psPorts,		    // Parmeter to pass to the start function
		0L,			    // CreationFlags,
		&ulThreadId);		    // ThreadId

    rc = GetLastError();

    debug_printf(1, "SPMH: Returning rc = %s (%d)\n", pstrGetErrorString(rc), rc);
    return rc;
}

************************************/

void gHyp_sock_logger ( char *where, char *pMsg, sAImsg *pAImsg )
{
  char
    header[MAX_PATH_SIZE+1] ;

  sprintf( header, "--To %s ----\n",where ) ;
  gHyp_util_output ( header ) ;
  gHyp_util_output ( pMsg ) ;
  gHyp_util_output ( "\n" ) ;
}

sLOGICAL gHyp_sock_putChannel ( sChannel *pChannel, char *pMsg, sAImsg *pAImsg )
{

  SOCKET
    socket ;

  int
    nBytes ;

  short
    flags = gHyp_channel_flags ( pChannel ) ;

  char
    *path = gHyp_channel_path ( pChannel ) ;

  sSSL
    *pSSL ;

  socket = gHyp_channel_socket ( pChannel ) ;

  pSSL = gHyp_channel_getSSL ( pChannel ) ;

  gHyp_sock_logger ( path, pMsg, pAImsg ) ;

  switch ( (flags&MASK_SOCKET) ) {

#ifdef AS_DMBX

  case SOCKET_DMBX:
  
    /* Set mbx name */
    if ( !lHyp_sock_setDMBX ( object ) ) return FALSE ;
    
    /* Put message */
    gHyp_channel_updateStats ( pChannel, strlen ( pMsg ), TRUE ) ;
    if ( mbxPuts ( socket, pMsg ) == MBX_SUCCESS ) {
      return TRUE ;
    }
    
    /* If put didn't succeed, try to re-connect */
    gHyp_util_logError (        "Failed to send message to FASTech mbx %s: %s",
                                target, mbxStringPerror() ) ;
    gHyp_util_logInfo (         "...retrying connection to FASTech mbx %s",
                                target  ) ;
    
    /* First try to re-open the mailbox */
    if ( !lHyp_sock_getDMBXClient ( object, CHANNEL_CREATE)) 
      return FALSE ;
    
    /* Re-try the put */
    gHyp_sock_logger ( path, pMsg, pAImsg ) ;

    gHyp_channel_updateStats ( pChannel, strlen ( pMsg ), TRUE ) ;
    if ( mbxPuts ( socket, pMsg ) == MBX_SUCCESS ) return TRUE ;
    
    /* Give up */
    return gHyp_util_logError ( 
                "Failed to re-send message to FASTech mbx %s: %s",
                target, mbxStringPerror() ) ;

#endif
  
  case SOCKET_TCP :
    nBytes = gHyp_sock_write ( socket, pMsg, strlen(pMsg), SOCKET_TCP, NULL, pSSL ) ;

    if ( nBytes <= 0 ) return FALSE ;
    return gHyp_channel_updateStats ( pChannel, nBytes,TRUE ) ;

  case SOCKET_FIFO :
  case SOCKET_SERIAL :

    nBytes = gHyp_sock_write ( socket, pMsg, strlen(pMsg), SOCKET_FIFO,
      gHyp_channel_overlapped(pChannel), pSSL );    
    if ( nBytes <= 0 ) return FALSE ;
    return gHyp_channel_updateStats ( pChannel, nBytes,TRUE ) ;

  }

  return FALSE ;
}

sLOGICAL gHyp_sock_putWildCardClients ( sData *pClients, char* targetId, sAImsg *pAImsg )
{
  /* Description:
   *
   *  Find the network socket that identifies the specified target host.
   */
  sData
    *pData,
    *pFirst,
    *pNext ;

  sChannel
    *pChannel ;
 
  char
    *pMsg ;
  
  sLOGICAL
    f,
    foundSome = FALSE ;
 
  for ( f=TRUE, pData = pFirst = gHyp_data_getFirst ( pClients ) ;
        (f && pData) || (pFirst && pData != pFirst );
        f=FALSE, pData = pNext, pFirst = gHyp_data_getFirst ( pClients ) ) {

    pNext = gHyp_data_getNext ( pData ) ;

    pChannel = (sChannel*) gHyp_data_getObject ( pData ) ;
    
    if ( gHyp_util_match ( gHyp_channel_target ( pChannel ), targetId ) ) {
      
      /* Set the correct target address in the message */
      gHyp_aimsg_setTargetId ( pAImsg, gHyp_channel_target ( pChannel ) ) ;
        
      /* Unparse the message and send it */
      pMsg = gHyp_aimsg_unparse ( pAImsg ) ;
      
      /* Send message.  If not successful, delete client socket */
      if ( gHyp_sock_putChannel ( pChannel, pMsg, pAImsg ) ) 
        foundSome = TRUE ;
      else {
        if ( pNext == pData ) pNext = NULL ;
        gHyp_data_detach ( pData ) ;
        gHyp_data_delete ( pData ) ;
        pFirst = gHyp_data_getFirst ( pClients ) ; 
      }
    }
  }

  return foundSome ;
}


static sLOGICAL lHyp_sock_exec (        char *file,
                                        char *path,
                                        char *target,
                                        char *log,
                                        char *argv[],
                                        int argc )
{
  /* Description:
   *
   *    Fork and exec (or VMS $CREPRC) a new application object 
   *
   *    The UNIX fork is done twice to prevent zombie processes.
   *    The VMS fork/exec is not used, $CREPRC is much cleaner.
   *
   * Arguments:
   *
   *    file                                                    [R]
   *    - name of shell or image to execute, ie: for argv[0]
   *
   *    path                                                    [R]
   *    - path where shell or image is found
   *
   *    target                                                  [R]
   *    - target Id of process
   *
   *    log                                                     [R]
   *    - log file specification
   *
   *    argv[]                                                  [R]
   *    - argv[1] through argv[n]
   *
   *    argc
   *    - count of argv
   *
   * Return value:
   *
   *    Returns TRUE if process was launched ok
   *
   */
#ifdef AS_WINDOWS

  STARTUPINFO si;
  PROCESS_INFORMATION pi;
  int
     i ;
  char
    argvs[MAX_PATH_SIZE+1] ;

  sprintf ( argvs, "%s", argv[0] ) ;
  for ( i=1; i<argc; i++ ) {
      strcat ( argvs, " " ) ;
      strcat ( argvs, argv[i] ) ;
  }
  ZeroMemory( &si, sizeof(si) );
  si.cb = sizeof(si);

  /*gHyp_util_debug("(%s) %s",file,argvs);*/
  // Start the child process. 
  if( !CreateProcess(
#ifdef AS_PPC 
  (const unsigned short*)
#endif     
	file, // No module name (use command line).
#ifdef AS_PPC 
  (unsigned short*)
#endif 	
        argvs,            // Command line.args 
        NULL,             // Process handle not inheritable. 
        NULL,             // Thread handle not inheritable. 
        FALSE,            // Set handle inheritance to FALSE. 
        DETACHED_PROCESS, // Detached process. 
        NULL,             // Use parent's environment block. 
        NULL,             // Use parent's starting directory. 
        &si,              // Pointer to STARTUPINFO structure.
        &pi ) ) {         // Pointer to PROCESS_INFORMATION structure.
    return gHyp_util_sysError ( 
      "Create Process failed for '%s', reason is %d",path ) ;
  }

  // Wait until child process exits.
  //    WaitForSingleObject( pi.hProcess, INFINITE );

#else
#ifndef AS_VMS

  int
    pid ;

  /* Flush output before we do it */
  if ( gsLog ) fflush ( gsLog ) ;

#ifndef AS_TRU64
  if ( (pid = vfork ()) < 0 )
    return gHyp_util_sysError ( "Problem with vfork" ) ;
#else
  if ( (pid = fork ()) < 0 )
    return gHyp_util_sysError ( "Problem with fork" ) ;
#endif

  else if ( pid == 0 ) {
        
    /* First child */

    if ( (pid = vfork ()) < 0 ) {
      gHyp_util_sysError ( "Problem with vfork - second child" ) ;
      _exit(2) ;  
    }
    else if ( pid > 0 )
     _exit(2) ; /* Parent from second fork == first child */

    /* Second child.
     *
     * Its parent becomes init when its real parent dies; ie: calls _exit(2) in
     * the statement above.  When the second child completes, the init
     * process will reap its status and it will not become a zombie.
     *
     * Zombies are created if AutoRouter forks only once to create a 
     * daemon process because:
     *          1) AutoRouter cannot wait for its child to complete and thus
     *             must rely on SIGCHLD to catch the termination
     *          2) When two or more daemon processes terminate at the same
     *             time, the AutoRouter catches only one SIGCHLD because the
     *             system does not queue them.  The remaining daemon 
     *             processes that did not get their status reaped by AutoRouter
     *             become zombies.
     */
     
    if ( execvp ( file, argv ) < 0 )
        gHyp_util_sysError ( "Failed to exec '%s'", file ) ;
    _exit(0);
  }

  if ( waitpid ( (pid_t) pid, (int *)0, 0 ) != pid ) /* Wait for first child */
    gHyp_util_sysError ( "Failed to waitpid" ) ;

#else

  int
    status ;
  
  unsigned
    pid ;

  unsigned long
    privs[2] ;

  makeDSCz      ( image_d, "SYS$SYSTEM:LOGINOUT.EXE" ) ;
  makeDSCz      ( input_d, file ) ;
  makeDSCz      ( output_d, log ) ;
  char          process[VALUE_SIZE+1] ;
  makeDSCs      ( process_d, process );

#ifdef AS_ALPHA
  struct { 
    char name ; 
    unsigned long value ; 
  } quota[1] = { PQL$_LISTEND, 0 } ;    
#else
  struct { 
    char name ; 
    unsigned long value ; 
  } quota[14] = {
                PQL$_PGFLQUOTA,         100000,
                PQL$_WSDEFAULT,         2048,
                PQL$_WSQUOTA,           4096,
                PQL$_WSEXTENT,          8192,
                PQL$_ENQLM,             300,
                PQL$_BIOLM,             100,
                PQL$_BYTLM,             30000,
                PQL$_FILLM,             100,
                PQL$_PRCLM,             6,
                PQL$_TQELM,             20,
                PQL$_ASTLM,             100,
                PQL$_DIOLM,             120,
                PQL$_JTQUOTA,           16184,
                PQL$_LISTEND,           0 } ;   
                /*
                PQL$_CPULM
                */
#endif
                    
  privs[0] = PRV$M_TMPMBX + PRV$M_SYSNAM ;
  privs[1] = 0 ;
  strcpy ( process, target ) ;
  process_d.dsc_w_length = strlen ( process ) ;

  status = sys$creprc ( &pid,           /* Return pid of created process */
                        &image_d,       /* SYS$SYSTEM:LOGINOUT.EXE - CLI */
                        &input_d,       /* SYS$INPUT - .COM file */
                        &output_d,      /* SYS$OUTPUT - .LOG fle */
                        0,              /* SYS$ERROR - .LOG file */
                        (sG64*)&privs,          /* Priviledges */
                        (unsigned int *)&quota,         /* Process quota's */
                        &process_d,     /* Process name */
                        4,              /* Base priority */
                        0,              /* Same UIC */
                        0,              /* Notify parent. */
                        PRC$M_DETACH ) ;
  if ( status == SS$_DUPLNAM ) 
    return gHyp_util_logError ( 
      "Process name '%s' already exists",
      target ) ;   
  if ( !gHyp_util_check ( status, 1 ) ) 
    return gHyp_util_logError ( 
      "Failed to execute '%s'", 
      target ) ;

#endif
#endif

  return TRUE ;

}

sData* gHyp_sock_findHYP ( sData *pClients, char *targetObject, char *targetId,
                           char* pMsg, sAImsg* pAImsg, sLOGICAL* pIsRouted )
{
  /* Description:
   *
   *    Look for a .HYP file and execute a new HyperScript.
   *
   *    HyperScripts are created when... 
   *
   *    1. The message method is CONNECT
   *    2. A HyperScript file named "targetObject" exists.
   *
   *    HyperScripts are invoked by...
   *
   *    1. Creating a socket for "targetObject"
   *    2. Writing the connect message into the new socket.
   *    3. Fork'ing and exec'ing a HyperScript program.
   */

  struct stat
    buf ;
  
  char
    hsexe[MAX_PATH_SIZE+1],
    path[MAX_PATH_SIZE+1],
    log[MAX_PATH_SIZE+1],
    heap[6],
    *argv[15] = {
      NULL,
      "-f", NULL, 
      "-e", "CONNECT", 
      "-e", "DISCONNECT", 
      "-e", "ABORT", 
      "-t", NULL, 
      "-l", NULL, 
      NULL, NULL } ;
 
   sData
     *pData ;
  
   int
     argc,
     i ;

   sChannel
      *pTargetChannel ;


  /* Construct path to  HyperScript executable */
#ifdef AS_VMS
  sprintf ( hsexe, "%shs.com", gzAUTOBIN ) ;
#else
#ifdef AS_WINDOWS
  sprintf ( hsexe, "%s\\hs.exe", gzAUTOBIN ) ;
#else
  sprintf ( hsexe, "%s/hs", gzAUTOBIN  ) ;
#endif
#endif

  if ( stat ( hsexe, &buf ) < 0 ) return NULL ;

#ifndef AS_WINDOWS
  /* File must be accessable and executable */
  if ( access ( hsexe, (R_OK|X_OK) ) < 0 ) {
    gHyp_util_sysError ( "No read-execute access to EXE file '%s'", hsexe ) ;
    return NULL ;
  }
#endif

  /* Construct path for .hyp file */
#ifdef AS_VMS
  sprintf ( path, "%s%s.hyp", gzAUTORUN, targetObject ) ;
#else
#ifdef AS_WINDOWS
  sprintf ( path, "%s\\%s.hyp", gzAUTORUN, targetObject ) ;
#else
  sprintf ( path, "%s/%s.hyp", gzAUTORUN, targetObject ) ;
#endif
#endif

  if ( stat ( path, &buf ) < 0 ) return NULL ;

#ifdef AS_WINDOWS
#else
  /* File must be readable */
  if ( access ( path, R_OK ) < 0 ) {
    gHyp_util_sysError ( "No read-access to HyperScript file '%s'", path ) ;
    return NULL ;
  }
#endif

  /* Construct log file path */
#ifdef AS_VMS
  sprintf ( log,  "%s%s.log", gzAUTOLOG, targetObject ) ;
#else
#ifdef AS_WINDOWS
  sprintf ( log,  "%s\\%s.log", gzAUTOLOG, targetObject ) ;
#else
  sprintf ( log,  "%s/%s.log", gzAUTOLOG, targetObject ) ;
#endif
#endif

#ifdef AS_VMS
  /* Create a client socket */
  if ( pClients ) gHyp_sock_createClient ( pClients, targetObject, targetId, TRUE ) ;
#endif

  gHyp_util_logInfo ( "...executing '%s' on file '%s', log = '%s', target = '%s'", 
			hsexe, path, log, targetId ) ; 
  argv[0] = hsexe ;
  argv[2] = path ;
  argv[10] = targetId ;
  argv[12] = log ;

  if ( guHeapMultiplier == 0 ) 
    argc = 13 ;
  else {
    heap[0] = '-' ;
    for ( i=1;i<=guHeapMultiplier;i++ ) heap[i] = 'x' ;
    heap[guHeapMultiplier+1] = '\0' ;
    argv[13] = heap ;
    argc = 14 ;
  }

  if ( !lHyp_sock_exec ( argv[0], path, targetObject, log, argv, argc ) ) return NULL ;

  /* Wait for the mailbox to be created.  This could be a separate thread */
  pData = NULL ;
  for ( i=0; (i < MAX_CONNECT_TRIES) && !pData; i++ ) {

     if ( i>0 ) {
       gHyp_util_logInfo ("...waiting for channel '%s'",targetObject) ;
#ifdef AS_WINDOWS
       Sleep ( RETRY_INTERVAL * 1000 ) ;
#else
       sleep ( RETRY_INTERVAL ) ;
#endif
     }
     if ( pClients )
       pData = gHyp_sock_findClient ( pClients, targetObject, targetId ) ; 
     else
       pData = gHyp_sock_findParent ( targetObject, targetId ) ; 
  }

  if ( pData ) {
    pTargetChannel = (sChannel*) gHyp_data_getObject( pData ) ;
    if ( pTargetChannel )
      *pIsRouted = gHyp_sock_putChannel ( pTargetChannel, pMsg, pAImsg ) ;
  }

  return pData ;
}

sData* gHyp_sock_findSCR ( sData *pClients, char *targetObject, char *targetId,
                          char* pMsg, sAImsg* pAImsg, sLOGICAL* pIsRouted )
{
  /* Description:
   *
   *    Look for a .SCR file and execute a new HyperScript.
   *
   *    HyperScripts are created when... 
   *
   *    1. The message method is CONNECT
   *    2. A HyperScript file named "targetObject" exists.
   *
   *    HyperScripts are invoked by...
   *
   *    1. Creating a socket for "targetObject"
   *    2. Writing the connect message into the new socket.
   *    3. $CREPREC a MEPMAIN (PROMIS) program.
   */
        
  struct stat
    buf ;
  
  char
    path[MAX_PATH_SIZE+1],
    log[MAX_PATH_SIZE+1],
    *argv[2] = { "mm", NULL } ;

  sData
    *pData ;

   int
     i ;

   sChannel
      *pTargetChannel ;

#ifdef AS_VMS
  sprintf ( path, "%s%s.scr", gzAUTORUN, targetObject ) ;
#else
#ifdef AS_WINDOWS
  sprintf ( path, "%s\\%s.scr", gzAUTORUN, targetObject ) ;
#else
  sprintf ( path, "%s/%s.scr", gzAUTORUN, targetObject ) ;
#endif
#endif

  if ( stat ( path, &buf ) < 0 ) return NULL ;

#ifndef AS_WINDOWS
  /* File must be accessable */
  if ( access ( path, R_OK ) < 0 ) {
    gHyp_util_sysError ( "No read-access to Script file '%s'", path ) ;
    return NULL ;
  }
#endif

  /* Construct log file path */
#ifdef AS_VMS
  sprintf ( log,  "%s%s.log", gzAUTOLOG, targetObject ) ;
#else
#ifdef AS_WINDOWS
  sprintf ( log,  "%s\\%s.log", gzAUTOLOG, targetObject ) ;
#else
  sprintf ( log,  "%s/%s.log", gzAUTOLOG, targetObject ) ;
#endif
#endif
    
#ifdef AS_VMS
  /* Create a client socket */
  if ( pClients ) gHyp_sock_createClient ( pClients, targetObject, targetId, TRUE ) ;
#endif

  gHyp_util_logInfo ( "...executing PROMIS script '%s'", path ) ;
    
  if ( !lHyp_sock_exec ( argv[0], path, targetObject, log, argv, 1 ) ) return NULL ;

  /* Wait for the mailbox to be created.  This should be a separate thread */
  pData = NULL ;
  for ( i=0; (i < MAX_CONNECT_TRIES) && !pData; i++ ) {

     if ( i>0 ) {
       gHyp_util_logInfo ("...waiting for channel '%s'",targetObject) ;
#ifdef AS_WINDOWS
       Sleep ( RETRY_INTERVAL * 1000 ) ;
#else
       sleep ( RETRY_INTERVAL ) ;
#endif
     }
     if ( pClients ) 
       pData = gHyp_sock_findClient ( pClients, targetObject, targetId ) ; 
     else
       pData = gHyp_sock_findParent ( targetObject, targetId ) ; 
  }
  if ( pData ) {
    pTargetChannel = (sChannel*) gHyp_data_getObject( pData ) ;
    if ( pTargetChannel )
      *pIsRouted = gHyp_sock_putChannel ( pTargetChannel, pMsg, pAImsg ) ;
  }

  return pData ;

}

sData* gHyp_sock_findEXE ( sData *pClients,  char *targetObject, char *targetId,
                          char* pMsg, sAImsg* pAImsg, sLOGICAL* pIsRouted )
{
  /* Description:
   *
   *    Look for a image or .EXE file and execute a new client.
   *
   */
        
  struct stat
    buf ;
  
  char
    path[MAX_PATH_SIZE+1],
    log[MAX_PATH_SIZE+1],
    *argv[12] = { 
      NULL,
      "-t", NULL, 
      "-e", "CONNECT", 
      "-e", "DISCONNECT", 
      "-e", "ABORT", 
      "-l", NULL, 
      NULL } ;
  
  sData
    *pData ;
  
   int
     i ;

   sChannel
      *pTargetChannel ;

#ifdef AS_VMS
  sprintf ( path, "%s%s.exe", gzAUTORUN, targetObject ) ;
#else
#ifdef AS_WINDOWS
  sprintf ( path, "%s\\%s.exe", gzAUTORUN, targetObject ) ;
#else
  sprintf ( path, "%s/%s", gzAUTORUN, targetObject ) ;
#endif
#endif

  if ( stat ( path, &buf ) < 0 ) return NULL ;

#ifndef AS_WINDOWS
  /* File must be accessable and executable */
  if ( access ( path, (R_OK|X_OK) ) < 0 ) {
    gHyp_util_sysError ( "No read-execute access to EXE file '%s'", path ) ;
    return NULL ;
  }
#endif

  /* Construct log file path */
#ifdef AS_VMS
  sprintf ( log,  "%s%s.log", gzAUTOLOG, targetObject ) ;
#else
#ifdef AS_WINDOWS
  sprintf ( log,  "%s\\%s.log", gzAUTOLOG, targetObject ) ;
#else
  sprintf ( log,  "%s/%s.log", gzAUTOLOG, targetObject ) ;
#endif
#endif
  
#ifdef AS_VMS
  /* Create a client socket */
  if ( pClients ) gHyp_sock_createClient ( pClients, targetObject, targetId, TRUE ) ;
#endif

  gHyp_util_logInfo ( "...executing image file '%s'", path ) ;
    
  argv[0] = path ;
  argv[2] = targetId ;
  argv[10] = log ;
  if ( !lHyp_sock_exec ( path, path, targetObject, log, argv, 11 ) ) return NULL ;

  /* Wait for the mailbox to be created.  This should be a separate thread */
  pData = NULL ;
  for ( i=0; (i < MAX_CONNECT_TRIES) && !pData; i++ ) {

     if ( i>0 ) {
       gHyp_util_logInfo ("...waiting for channel '%s'",targetObject) ;
#ifdef AS_WINDOWS
       Sleep ( RETRY_INTERVAL * 1000 ) ;
#else
       sleep ( RETRY_INTERVAL ) ;
#endif
     }
     if ( pClients ) 
       pData = gHyp_sock_findClient ( pClients, targetObject, targetId ) ; 
     else
       pData = gHyp_sock_findParent ( targetObject, targetId ) ; 

  }
  if ( pData ) {
    pTargetChannel = (sChannel*) gHyp_data_getObject( pData ) ;
    if ( pTargetChannel )
      *pIsRouted = gHyp_sock_putChannel ( pTargetChannel, pMsg, pAImsg ) ;
  }

  return pData ;
}

sData* gHyp_sock_findCOM ( sData *pClients,  char *targetObject, char *targetId,
                          char* pMsg, sAImsg* pAImsg, sLOGICAL* pIsRouted )
{
  /* Description:
   *
   *    Look for a .COM or .PL file and execute a new client.
   */
        
  struct stat
    buf ;
  
  char
    path[MAX_PATH_SIZE+1],
    log[MAX_PATH_SIZE+1],
    *argv[3] = { NULL, NULL, NULL } ;
  
  sData
    *pData ;
  
   int
     i ;

   sChannel
      *pTargetChannel ;

#ifdef AS_VMS
  sprintf ( path, "%s%s.com", gzAUTORUN, targetObject ) ;
#else
#ifdef AS_WINDOWS
  sprintf ( path, "%s\\%s.bat", gzAUTORUN, targetObject ) ;
#else
  sprintf ( path, "%s/%s.pl", gzAUTORUN, targetObject ) ;
#endif
#endif

  if ( stat ( path, &buf ) < 0 ) return NULL ;

#ifndef AS_WINDOWS
  /* File must be accessable */
  if ( access ( path, (R_OK|X_OK) ) < 0 ) {
    gHyp_util_sysError ( "No read-execute access to Perl script '%s'", path ) ;
    return NULL ;
  }
#endif

  /* Construct log file path */
#ifdef AS_VMS
  sprintf ( log,  "%s%s.log", gzAUTOLOG, targetObject ) ;
#else
#ifdef AS_WINDOWS
  sprintf ( log,  "%s\\%s.log", gzAUTOLOG, targetObject ) ;
#else
  sprintf ( log,  "%s/%s.log", gzAUTOLOG, targetObject ) ;
#endif
#endif

#ifdef AS_VMS
  /* Create a client socket */
  if ( pClients ) gHyp_sock_createClient ( pClients, targetObject, targetId, TRUE ) ;
#endif

  gHyp_util_logInfo ( "...executing command script '%s'", path ) ;
    
  argv[0] = targetId ;
  argv[1] = log ;
  if ( !lHyp_sock_exec ( path,  path, targetObject, log, argv, 2 ) ) return NULL ;

  /* Wait for the mailbox to be created.  This should be a separate thread */
  pData = NULL ;
  for ( i=0; (i < MAX_CONNECT_TRIES) && !pData; i++ ) {

     if ( i>0 ) {
       gHyp_util_logInfo ("...waiting for channel '%s'",targetObject) ;
#ifdef AS_WINDOWS
       Sleep ( RETRY_INTERVAL * 1000 ) ;
#else
       sleep ( RETRY_INTERVAL ) ;
#endif
     }
     if ( pClients ) 
       pData = gHyp_sock_findClient ( pClients, targetObject, targetId ) ; 
     else
       pData = gHyp_sock_findParent ( targetObject, targetId ) ; 

  }
  if ( pData ) {
    pTargetChannel = (sChannel*) gHyp_data_getObject( pData ) ;
    if ( pTargetChannel )
      *pIsRouted = gHyp_sock_putChannel ( pTargetChannel, pMsg, pAImsg ) ;
  }

  return pData ;
}

static int lHyp_sock_nextAlarmTime ( sData *pClients )
{
  /* Description:
   *
   *    Find the next socket timeout or the next heartbeat time, which ever
   *    is sooner.
   */
  
  sChannel
    *pChannel ;
 
  int
    nextAlarmTime,
    nextUpdateTime ;

  sLOGICAL
    f ;

  sData
    *pData,
    *pFirst ;

  gsCurTime = time(NULL) ;
  if ( gsCurTime >= giNextIdleBeat ) {
    
    /*gHyp_util_debug ( "Idle HeartBeat" ) ;*/
    giNextIdleBeat += IDLE_INTERVAL ;
  }

  /* Check for time warps */
  if ( giNextIdleBeat <= gsCurTime ||
       giNextIdleBeat >  gsCurTime + IDLE_INTERVAL )
    /* Clock was set ahead or behind! */
    giNextIdleBeat = (int)gsCurTime + IDLE_INTERVAL ;
  
  /* Assume next alarm will occur at the heartbeat interval */
  nextAlarmTime = giNextIdleBeat ;

  /* Look for socket timeouts that will come earlier */
  for ( f=TRUE, pData = pFirst = gHyp_data_getFirst ( pClients ) ;
        (f && pData) || pData != pFirst;
        f=FALSE, pData = gHyp_data_getNext ( pData )) {

    pChannel = (sChannel*) gHyp_data_getObject ( pData ) ;
    nextUpdateTime = (int)gHyp_channel_updateTime (pChannel) + IDLE_INTERVAL ;

    /* Correct for time warps - Correct and delay evaluation. */
    if ( nextUpdateTime <= gsCurTime ||
         nextUpdateTime >  gsCurTime + IDLE_INTERVAL ) {
      gHyp_util_logInfo("Invalid update time %d on client socket %s (%u)",
                        gHyp_channel_updateTime(pChannel),
                        gHyp_channel_target(pChannel),
                        gHyp_channel_socket(pChannel) ) ;
      gHyp_channel_setUpdateTime ( pChannel, gsCurTime ) ;
    }
    else
      nextAlarmTime = MIN ( nextAlarmTime, nextUpdateTime ) ;
  }

  return nextAlarmTime ;
}
       
sLOGICAL gHyp_sock_flushBuffer ( char *path )
{
  int
        nBytes ;
        
  sLOGICAL
    status = TRUE ;
  
  HANDLE 
    fd ;

#ifdef AS_WINDOWS
  fd = CreateFile(
#ifdef AS_PPC 
  (const unsigned short*)
#endif
	    path,           // create path
             GENERIC_WRITE,                // open for writing 
             0,                            // do not share 
             NULL,                         // no security 
             CREATE_ALWAYS,                // overwrite existing 
             FILE_ATTRIBUTE_NORMAL |       // normal file 
             FILE_FLAG_OVERLAPPED,         // asynchronous I/O 
             NULL);                        // no attr. template 

  if (fd == INVALID_HANDLE_VALUE) 
    return gHyp_util_sysError ( "Failed to create flush file '%s'",path ) ;

   status = WriteFile( (HANDLE) fd, 
                      gzInboxBuf, 
                      strlen ( gzInboxBuf ), 
                      &nBytes, 
                      (LPOVERLAPPED) NULL); 
   CloseHandle ( (HANDLE) fd ) ;
#else
  if ( (fd = creat ( path, 00755 )) == INVALID_HANDLE_VALUE )
    return gHyp_util_sysError ( "Failed to create flush file '%s'",
                                  path ) ;

  if ( (nBytes = write ( fd, gzInboxBuf, strlen ( gzInboxBuf ) )) == -1 )
    status = gHyp_util_sysError ( "Failed to write to flush file '%s'",
                                  path ) ;    
  close ( fd ) ;
#endif
  gzInboxBuf[0] = '\0' ;
  return status ;
}

sLOGICAL gHyp_sock_loadBuffer ( char *path )
{
  sLOGICAL
    status = TRUE ;
  
  int 
    nBytes ;

  HANDLE
    fd ;
  
  char
    *eom ;

  char
    *d3 = "|||" ;
 
#ifndef AS_WINDOWS
  struct stat
    buf ;

  if ( stat ( path, &buf ) < 0 ) return status ;

  if ( ( fd = open ( path, O_RDONLY | O_NONBLOCK ) ) == INVALID_HANDLE_VALUE )
      return gHyp_util_sysError ( 
        "Failed to open message initialization file '%s'", path ) ;
   
  if ( (nBytes = read ( fd, gzInboxBuf, MAX_MESSAGE_SIZE ) ) < 0 )
    status = gHyp_util_sysError ( "Failed to read from load file '%s'",
                                  path ) ;
  
  close ( fd ) ;
  remove ( path ) ;

#else

  fd = CreateFile ( 
#ifdef AS_PPC 
  (const unsigned short*)
#endif		    
		    path,
                    GENERIC_READ,
                    0,                /* Share mode */
                    NULL,             /* Pointer to security attribute */
                    OPEN_EXISTING,    /* How to open */
                    FILE_ATTRIBUTE_NORMAL,                /* Port attributes */
                    NULL);            
  if ( fd  == INVALID_HANDLE_VALUE ) return status ;
  
    status = ReadFile( fd, 
                       gzInboxBuf, 
                       MAX_MESSAGE_SIZE, 
                       &nBytes, 
                      (LPOVERLAPPED) NULL); 
        if ( !status )
      return gHyp_util_sysError ( "Failed to read from load file '%s'",path ) ;

    CloseHandle ( fd ) ;
#endif 

  /* Check that loaded message starts with a delimiter and ends with 3 delimiters */
  if ( nBytes > 0 ) {

    /* Look for message termination */
    if ( gzInboxBuf[0] != DEFAULT_DELIMITER ) {
      gzInboxBuf[0] = '\0'  ;
      return gHyp_util_logError ( 
        "First message does begins with '|', discarding flush buffer." ) ;
    }

    eom = strstr ( gzInboxBuf, d3 ) ;
    if ( !eom ) {
      gzInboxBuf[0] = '\0'  ;
      return gHyp_util_logError ( 
        "First message does not end with '|||', discarding flush buffer." ) ;
    }
  }
  return status ;
}

static void lHyp_sock_select_checkLog()
{
 
  struct stat
    buf ;
  
  /* Check to see if the log was renamed. If so, start a new one. */
  if ( gzLogName[0] ) {
    
    if ( stat ( gzLogName, &buf ) < 0 ) {
      
      /* File has been renamed */       
      fclose ( gsLog ) ;        
      if ( (gsLog = fopen ( gzLogName, "a+"
#ifdef AS_VMS
                            , "shr=get,put"
#endif
                            ) ) == NULL ) {
        gsLog = stdout ;
        gHyp_util_sysError ( "Failed to open log file '%s'",
                             gzLogName ) ;
        return ;
      }
    }
  }
}

HANDLE gHyp_sock_createEvent ( )
{
#ifdef AS_WINDOWS
  return CreateEvent ( NULL, TRUE, FALSE, NULL ) ;
#else
  return INVALID_HANDLE_VALUE ;
#endif
}

static int lHyp_sock_select_FDSET_objects ( sConcept *pConcept, 
                                           sData *pSockets, 
                                           int timeout ) 
{

  sLOGICAL
    f; 
  
  SOCKET
    socket=INVALID_SOCKET ;

  sData
    *pData,
    *pFirst,
    *pNext ;

  sBYTE
    objectType ;

  void
    *pObject ;

  short
    flags = 0;

  char 
    *pMsgOff ;
  
  int
    *pNbytes,
    to,
    n ;

  LPOVERLAPPED
    pOverlapped ;
    
  giOffsetFds = giNumSelectEvents ;

  for ( f=TRUE, pData = pFirst = gHyp_data_getFirst ( pSockets ) ;
        (f && pData) || (pData != pFirst);
        f=FALSE, pData = pNext ) {
          
    pNext = gHyp_data_getNext ( pData ) ;
    objectType = gHyp_data_getObjectType ( pData ) ;
    pObject = gHyp_data_getObject ( pData ) ;

    /*gHyp_util_debug("selecting object %p",pData);*/
    /*gHyp_util_debug("pData = %p, pFirst = %p, pNext= %p",pData,pFirst,pNext);*/
    switch ( objectType ) {
    case DATA_OBJECT_HSMS :
      flags = gHyp_hsms_flags ( (sHsms*) pObject ) ;
      socket = (SOCKET) gHyp_hsms_socket ( (sHsms*) pObject ) ;
      break ;
    case DATA_OBJECT_SECS1 :
    case DATA_OBJECT_PORT :
    case DATA_OBJECT_HTTP :
      flags = gHyp_secs1_flags ( (sSecs1*) pObject ) ;
      socket = (SOCKET) gHyp_secs1_fd ( (sSecs1*) pObject ) ;
      break ;
    }

    /* If the port is stopped, don't include it *
     *
     * This method was abandoned in favour of allowing the FDSET 
     * but then just ignoring it until reading was possible
     * 
     if ( !gHyp_concept_getPortStopGo ( pConcept, socket ) ) continue ;
     */


    if ( flags & (PROTOCOL_SECS1 | PROTOCOL_NONE | PROTOCOL_HTTP ) ) {

      pOverlapped = gHyp_secs1_overlapped ( (sSecs1*) pObject ) ;
      pMsgOff = (char*) gHyp_secs1_buf ( (sSecs1*) pObject ) ;
      pNbytes = gHyp_secs1_pNbytes ( (sSecs1*) pObject ) ;
      n = MAX_SECS1_BUFFER ;
    }
    else /*if ( flags & PROTOCOL_HSMS ) */ {

      pOverlapped = gHyp_hsms_overlapped ( (sHsms*) pObject ) ;
      pMsgOff = (char*) gHyp_hsms_buf ( (sHsms*) pObject ) ;
      pNbytes = gHyp_hsms_pNbytes ( (sHsms*) pObject ) ;
      n = MAX_HSMS_BUFFER_SIZE ;
    }

    to = MIN( -1, -(timeout*1000) ); 
    *pNbytes = 0 ;
    if ( lHyp_sock_read ( socket, 
                          pMsgOff,
                          n,
                          (sBYTE)(flags&MASK_SOCKET),
                          to,
                          pNbytes,
                          pOverlapped,
			  FALSE ) == 0 ) {

#ifdef AS_WINDOWS
      gsEvents[giNumSelectEvents++] = pOverlapped->hEvent ;      
#else
      gsEvents[giNumSelectEvents++] = socket ;
#endif
	    
    }
    else {
      gHyp_util_logError ( "Failed to read overlapped I/O from socket %u",socket ) ;
      if ( pNext == pData ) pNext = NULL ;
      gHyp_data_detach ( pData ) ;
      gHyp_data_delete ( pData ) ;
      pFirst = gHyp_data_getFirst ( pSockets ) ;
    }

  }

  giNumFds = giNumSelectEvents - giOffsetFds ;

  return COND_SILENT ;
}

static int lHyp_sock_select_read_objects ( sConcept *pConcept, sData *pSockets ) 
{  
  SOCKET
    parentSocket=INVALID_SOCKET,
    socket=INVALID_SOCKET,
    newSocket=INVALID_SOCKET ;

  int
    port = 0,
    msgLen=0;

  sData
    *pData ;

  sSecs1
    *pSecs1,
    *pPort,
    *pListenSecs1,
    *pListenPort ;

  sHsms
    *pListenHsms,
    *pHsms ;

  sInstance
    *pAIassigned,
    *pAI=NULL ;

  sWORD
    id ;

  sLOGICAL
    isGo ;

  sBYTE
    objectType ;

  void
    *pObject ;

  short
    flags=0 ;

 OVERLAPPED
    *pOverlapped ;

#ifdef AS_WINDOWS

  int
    i ;

  /*WSANETWORKEVENTS networkEvents ;*/

#else

  sLOGICAL
    f; 

  sData
    *pNext,
    *pFirst ;

#endif

  char
    addr[HOST_SIZE+1],
    node[HOST_SIZE+1] ;

#ifdef AS_WINDOWS
  
  /* For WINDOWS, there is only one socket to look at */
  i = giOffset - giOffsetFds ;

  if ( i >=0 && i < giNumFds ) {

    pData = gHyp_data_getChildBySS ( pSockets, i ) ;

    if ( pData ) {

#else

  /* For UNIX and VMS, which use the select() statement, 
   * we have to look at all sockets 
   */
  for ( f=TRUE, pData = pFirst = gHyp_data_getFirst ( pSockets ) ;
        (f && pData) || (pFirst && pData != pFirst );
        f=FALSE, pData = pNext, pFirst = gHyp_data_getFirst ( pSockets ) ) {

    pNext = gHyp_data_getNext ( pData ) ;

#endif
    
    objectType = gHyp_data_getObjectType ( pData ) ;
    pObject = gHyp_data_getObject ( pData ) ;
  
    switch ( objectType ) {
    case DATA_OBJECT_HSMS :
      flags = gHyp_hsms_flags ( (sHsms*) pObject ) ;
      socket = gHyp_hsms_socket ( (sHsms*) pObject ) ;
      pAI = gHyp_hsms_parentAI ( (sHsms*) pObject ) ;
      parentSocket = gHyp_hsms_parentSocket ( (sHsms*) pObject ) ;
      port = gHyp_hsms_port ( (sHsms*) pObject ) ;
      pOverlapped = gHyp_hsms_overlapped ( (sHsms*) pObject ) ;
      break ;
    case DATA_OBJECT_SECS1 :
    case DATA_OBJECT_PORT :
    case DATA_OBJECT_HTTP :
      flags = gHyp_secs1_flags ( (sSecs1*) pObject ) ;
      socket = (SOCKET) gHyp_secs1_fd ( (sSecs1*) pObject ) ;
      pAI = gHyp_secs1_parentAI ( (sSecs1*) pObject ) ;
      parentSocket = gHyp_secs1_parentSocket ( (sSecs1*) pObject ) ;
      pOverlapped = gHyp_secs1_overlapped ( (sSecs1*) pObject ) ;
      port = gHyp_secs1_port ( (sSecs1*) pObject ) ;
      break ;
    }

    /* If the port is stopped, don't even bother to check if 
     * the FDSET is true, see if another socket is ready 
     */
    isGo = gHyp_concept_getPortStopGo ( pConcept, socket ) ;

#ifndef AS_WINDOWS
    if ( isGo && FD_ISSET ( socket, &gsReadFDS ) ) {

#else

    /* Enumerate network events *
    WSAEnumNetworkEvents ( socket, pOverlapped->hEvent, &networkEvents ) ;
    if ( networkEvents.lNetworkEvents & FD_READ )
    else if ( networkEvents.lNetworkEvents & FD_CONNECT )
    else if ( networkEvents.lNetworkEvents & FD_CLOSE )
    else if ( networkEvents.lNetworkEvents & FD_WRITE )
    */

    /* For WINDOZE, we just pretend the sock has no data */
    if ( !isGo ) return COND_SILENT ;

#endif
      if ( flags == (PROTOCOL_HSMS | SOCKET_LISTEN) ) {
        
        /* Incoming Internet connection request */
        newSocket = gHyp_tcp_checkInbound ( socket, node, addr, TRUE ) ;
        
        if ( newSocket == INVALID_SOCKET ) return COND_ERROR ;

        /* HSMS state has transitioned.
         * HSMS_EXPECT_RECV_CONNECT 
         * ->HSMS_EXPECT_SEND_ACCEPT
         * ->HSMS_EXPECT_RECV_SELECTREQ
         */
        pHsms = gHyp_hsms_new ( (SOCKET_TCP|PROTOCOL_HSMS),
                                newSocket,
                                node,
                                port,
                                HSMS_DEFAULT_T5,
                                HSMS_DEFAULT_T6,
                                HSMS_DEFAULT_T7,
                                HSMS_DEFAULT_T8,
                                HSMS_EXPECT_RECV_SELECTREQ,
                                socket,
                                pAI ) ; /* This is the AI from the parent */

        gHyp_concept_newSocketObject ( pConcept,
                                       newSocket, 
                                       (void*) pHsms, 
                                       DATA_OBJECT_HSMS,
                                       (void (*)(void*))gHyp_hsms_delete ) ;
        msgLen = 0 ;
        pAIassigned = gHyp_concept_getInstForFd ( pConcept, socket ) ;
	id = (pAIassigned == NULL) ? NULL_DEVICEID : gHyp_instance_getDeviceId ( pAIassigned, socket ) ;
	if ( pAIassigned ) gHyp_instance_signalConnect ( pAIassigned, newSocket, port, id ) ;
 
	if ( id != NULL_DEVICEID ) {
	
	  /* Take any SSL structures as well */
	  pListenHsms = (sHsms*) gHyp_concept_getSocketObject ( pConcept, 
					(SOCKET) socket, 
					DATA_OBJECT_NULL ) ;

#ifdef AS_SSL
	  gHyp_hsms_setSSL ( 
	    pHsms,  gHyp_sock_copySSL ( gHyp_hsms_getSSL ( pListenHsms ))) ;
#endif
	  /* Take this id */
	  gHyp_instance_updateFd ( pAIassigned, newSocket, id, NULL, FALSE ) ;

	}
	else {
	  gHyp_util_logWarning ( "No id is assigned to new socket %d from HSMS listen socket %d",newSocket, socket ) ;
	}
      }
      else if ( flags == (PROTOCOL_SECS1 | SOCKET_LISTEN) ) {

        /* Incoming Internet connection request */
        newSocket = gHyp_tcp_checkInbound ( socket, node, addr, TRUE ) ;

        if ( newSocket == INVALID_SOCKET ) return COND_ERROR ;
        pSecs1 = gHyp_secs1_new ( (short)(PROTOCOL_SECS1 | SOCKET_TCP),
                                  newSocket, 
                                  node,
                                  port,
				  SECS_DEFAULT_T1,
				  SECS_DEFAULT_T2,
				  SECS_DEFAULT_T4,
				  SECS_DEFAULT_RTY,
                                  socket,
                                  pAI ) ; /* This is the parent */

        gHyp_concept_newSocketObject ( pConcept, 
                                       newSocket, 
                                       (void*)pSecs1, 
                                       DATA_OBJECT_SECS1,
                                       (void (*)(void*))gHyp_secs1_delete ) ;
        msgLen = 0 ;

	/* Find any instance that has assigned this socket to an id */ 
        pAIassigned = gHyp_concept_getInstForFd ( pConcept, socket ) ;

	/* Find any device id from that instance */
	id = pAIassigned == NULL ? NULL_DEVICEID : gHyp_instance_getDeviceId ( pAIassigned, socket ) ;
	if ( pAIassigned ) gHyp_instance_signalConnect ( pAIassigned, newSocket, port, id ) ;

	if ( id != NULL_DEVICEID ) {
	
	  /* Take any SSL structures as well */
	  pListenSecs1 = (sSecs1*) gHyp_concept_getSocketObject ( pConcept, 
					(SOCKET) socket, 
					DATA_OBJECT_NULL ) ;

#ifdef AS_SSL
	  gHyp_secs1_setSSL ( 
	    pSecs1,  gHyp_sock_copySSL ( gHyp_secs1_getSSL ( pListenSecs1 ))) ;
#endif
	  /* Take this id */
	  gHyp_instance_updateFd ( pAIassigned, newSocket, id, NULL, FALSE ) ;

	}
	else {
	  gHyp_util_logWarning ( "No id is assigned to new socket %d from SECS1 listen socket %d",newSocket, socket ) ;
	}

      }
      else if ( flags == (PROTOCOL_NONE | SOCKET_LISTEN) ) {

        /* Incoming Internet connection request */
        newSocket = gHyp_tcp_checkInbound ( socket, node, addr, TRUE ) ;

        if ( newSocket == INVALID_SOCKET ) return COND_ERROR ;
        pPort = gHyp_secs1_new ( (short)(SOCKET_TCP|PROTOCOL_NONE),
                                 newSocket,
                                 node,
                                 port,
				 SECS_DEFAULT_T1,
				 SECS_DEFAULT_T2,
				 SECS_DEFAULT_T4,
				 SECS_DEFAULT_RTY,
                                 socket,
                                 pAI ) ; /* This is the parent */

        gHyp_concept_newSocketObject ( pConcept, 
                                       newSocket, 
                                       (void*) pPort, 
                                       DATA_OBJECT_PORT,
                                       (void (*)(void*))gHyp_secs1_delete ) ;
        msgLen = 0 ;

	/* Get the first instance that references the socket */ 
	pAIassigned = gHyp_concept_getInstForFd ( pConcept, socket ) ;

	/* From that instance, get the next available id */
	id = pAIassigned == NULL ? NULL_DEVICEID : gHyp_instance_getDeviceId ( pAIassigned, socket ) ;

	/* Create the signal that specifies what we just selected */
	if ( pAIassigned ) gHyp_instance_signalConnect ( pAIassigned, newSocket, port, id ) ;

	if ( id != NULL_DEVICEID ) {

	  /* Take any SSL structures as well */
	  pListenPort = (sSecs1*) gHyp_concept_getSocketObject ( 
	      pConcept, 
	      (SOCKET) socket, 
	      DATA_OBJECT_NULL ) ;
#ifdef AS_SSL
	  gHyp_secs1_setSSL ( 
	    pPort,  gHyp_sock_copySSL ( gHyp_secs1_getSSL ( pListenPort ))) ;
#endif
	  gHyp_secs1_setForwardPorts ( pPort, 
	    gHyp_secs1_copyForwardPort ( pListenPort, id ) ) ; 

	  /* Transfer the id. */
	  gHyp_instance_updateFd ( pAIassigned, newSocket, id, NULL, FALSE ) ;

	  /* And and and.... what? */
	  gHyp_secs1_setBinary ( pPort, gHyp_secs1_isBinary ( pListenPort ) ) ;


	}	
	else {
	  gHyp_util_logWarning ( "No id is assigned to new socket %d from port listen socket %d",newSocket, socket ) ;
	}
      
      }
      else if ( flags == (PROTOCOL_HTTP | SOCKET_LISTEN) ) {

        /* Incoming Internet connection request */
        newSocket = gHyp_tcp_checkInbound ( socket, node, addr, TRUE ) ;

        if ( newSocket == INVALID_SOCKET ) return COND_ERROR ;
        pPort = gHyp_secs1_new ( (short)(SOCKET_TCP|PROTOCOL_HTTP),
                                 newSocket,
                                 node,
                                 port,
				 SECS_DEFAULT_T1,
				 SECS_DEFAULT_T2,
				 SECS_DEFAULT_T4,
				 SECS_DEFAULT_RTY,
                                 socket,
                                 pAI ) ; /* This is the parent */

        gHyp_concept_newSocketObject ( pConcept, 
                                       newSocket, 
                                       (void*) pPort, 
                                       DATA_OBJECT_HTTP,
                                       (void (*)(void*))gHyp_secs1_delete ) ;
        msgLen = 0 ;

	/* Get the first instance that references the socket */ 
        pAIassigned = gHyp_concept_getInstForFd ( pConcept, socket ) ;

	/* From that instance, get the next available id */
	id = pAIassigned == NULL ? NULL_DEVICEID : gHyp_instance_getDeviceId ( pAIassigned, socket ) ;

	/* Create the signal that specifies what we just selected */
	if ( pAIassigned ) gHyp_instance_signalConnect ( pAIassigned, newSocket, port, id ) ;

	if ( id != NULL_DEVICEID ) {

	  /* Take any SSL structures as well */
	  pListenPort = (sSecs1*) gHyp_concept_getSocketObject ( 
	      pConcept, 
	      (SOCKET) socket, 
	      DATA_OBJECT_NULL ) ;
#ifdef AS_SSL
	  gHyp_secs1_setSSL ( 
	    pPort,  gHyp_sock_copySSL ( gHyp_secs1_getSSL ( pListenPort ))) ;
#endif
	  gHyp_secs1_setForwardPorts ( pPort, 
	    gHyp_secs1_copyForwardPort ( pListenPort, id ) ) ; 

	  /* Transfer the id. */
	  gHyp_instance_updateFd ( pAIassigned, newSocket, id, NULL, FALSE ) ;

	  /* And and and.... what? */
	  gHyp_secs1_setBinary ( pPort, gHyp_secs1_isBinary ( pListenPort ) ) ;
	}
	else {
	  gHyp_util_logWarning ( "No id is assigned to new socket %d from http listen socket %d",newSocket, socket ) ;
	}
	
      }
#if defined ( AS_UNIX ) 

      else if ( flags == (PROTOCOL_NONE | SOCKET_UNIX_LISTEN) ) {

        /* Incoming connection request over UNIX socket */
        newSocket = gHyp_tcp_checkInboundUNIX ( socket ) ;

        if ( newSocket == INVALID_SOCKET ) return COND_ERROR ;

        pPort = gHyp_secs1_new ( (short)(SOCKET_UNIX|PROTOCOL_NONE),
                                 newSocket,
                                 gzLocalHost,
                                 port,
				 SECS_DEFAULT_T1,
				 SECS_DEFAULT_T2,
				 SECS_DEFAULT_T4,
				 SECS_DEFAULT_RTY,
                                 socket,
                                 pAI ) ; /* This is the parent */

        gHyp_concept_newSocketObject ( pConcept, 
                                       newSocket, 
                                       (void*) pPort, 
                                       DATA_OBJECT_PORT,
                                       (void (*)(void*))gHyp_secs1_delete ) ;
        msgLen = 0 ;

	/* Get the first instance that references the socket */ 
        pAIassigned = gHyp_concept_getInstForFd ( pConcept, socket ) ;

	/* From that instance, get the next available id */
	id = pAIassigned == NULL ? NULL_DEVICEID : gHyp_instance_getDeviceId ( pAIassigned, socket ) ;

	/* Create the signal that specifies what we just selected */
        port = 0 ;  /* For UNIX sockets, there really isn't a port number */
	if ( pAIassigned ) gHyp_instance_signalConnect ( pAIassigned, newSocket, port, id ) ;

	if ( id != NULL_DEVICEID ) {

	  /* Transfer the id. */
	  gHyp_instance_updateFd ( pAIassigned, newSocket, id, NULL, FALSE ) ;

	}	
	else {
	  gHyp_util_logWarning ( "No id is assigned to new socket %d from UNIX listen socket %d",newSocket, socket ) ;
	}
      
      }
      else if ( flags == (PROTOCOL_NONE | SOCKET_UNIX) ) {

	/* A socket descriptor is being passed to us.  The 'port' is optional, a secret. */
	newSocket = gHyp_tcp_recvmsg ( socket, &port ) ;

        if ( newSocket == INVALID_SOCKET ) return COND_SILENT ; /*COND_ERROR ;*/

	/* NOTE HERE. the socket that is assigned as the "parent" in gHyp_secs1_new is
	 * not the UNIX socket we just got the recvmsg() from, rather it is
	 * the original UNIX listen socket. (We don't care about the intermediary, its just a means
	 * to get the passed socket via sendmsg/recvmsg)..
	 * On the listen UNIX socket, we can assign an SSL structure so that we'll take it here
	 * on the handoff.  But, in order for the SSL to work, we must setup a 
	 * CONNECT_HANDLER, and in it we must do a "ssl_setState (deviceId, SSLdata)" function
	 * where SSLdata was produced by the hyperscript doing the socket handoff.
	 * It would haved called ssl_getState() on the socket just before
	 * handoff (via port_sendmsg()).  A good way to get the SSL state data from the handoff
	 * server is via HTTP.
	 */
        pPort = gHyp_secs1_new ( (short)(SOCKET_TCP|PROTOCOL_NONE),
                                 newSocket,
                                 gzLocalHost,
                                 port,
				 SECS_DEFAULT_T1,
				 SECS_DEFAULT_T2,
				 SECS_DEFAULT_T4,
				 SECS_DEFAULT_RTY,
                                 parentSocket,
                                 pAI ) ; /* This is the parent */

        gHyp_concept_newSocketObject ( pConcept, 
                                       newSocket, 
                                       (void*) pPort, 
                                       DATA_OBJECT_PORT,
                                       (void (*)(void*))gHyp_secs1_delete ) ;
        msgLen = 0 ;

	/* Get the first instance that references the socket */ 
        pAIassigned = gHyp_concept_getInstForFd ( pConcept, socket ) ;

	/* From that instance, get the next available id */
	id = pAIassigned == NULL ? NULL_DEVICEID : gHyp_instance_getDeviceId ( pAIassigned, socket ) ;

	/* Create the signal that specifies what we just selected */
	if ( pAIassigned ) gHyp_instance_signalConnect ( pAIassigned, newSocket, port, id ) ;

	if ( id != NULL_DEVICEID ) {

	  /* Take any SSL structures as well */
	  pListenPort = (sSecs1*) gHyp_concept_getSocketObject ( 
	      pConcept, 
	      (SOCKET) parentSocket, 
	      DATA_OBJECT_NULL ) ;
#ifdef AS_SSL
	  gHyp_secs1_setSSL ( 
	    pPort,  gHyp_sock_copySSL ( gHyp_secs1_getSSL ( pListenPort ))) ;
#endif
	  gHyp_secs1_setForwardPorts ( pPort, 
	    gHyp_secs1_copyForwardPort ( pListenPort, id ) ) ; 

	  /* Transfer the id. */
	  gHyp_util_logInfo ( "Id %d is assigned to new UNIX handoff socket %d",id, newSocket ) ;
	  gHyp_instance_updateFd ( pAIassigned, newSocket, id, NULL, FALSE ) ;

	  /* And and and.... what? */
	  gHyp_secs1_setBinary ( pPort, gHyp_secs1_isBinary ( pListenPort ) ) ;

	}
	else {
	  gHyp_util_logWarning ( "No id is assigned to new UNIX socket %d from handoff socket %d",newSocket, socket ) ;
	}

	gHyp_util_logInfo ( "Closing UNIX handoff socket %d",socket ) ;
	gHyp_concept_deleteSocketObject ( pConcept, socket ) ;

	/* Incase the new socket was used by something else, clear it now */
	FD_CLR ( newSocket, &gsReadFDS ) ;
        /*FD_CLR ( socket, &gsReadFDS ) ;*/

      }
#endif
      else if ( flags & PROTOCOL_NONE ) {
          msgLen = gHyp_secs1_rawIncoming ( (sSecs1*) pObject, pConcept, pAI, DATA_OBJECT_PORT ) ;
      }
      else if ( flags & PROTOCOL_HTTP ) {
          msgLen = gHyp_secs1_rawIncoming ( (sSecs1*) pObject, pConcept, pAI, DATA_OBJECT_HTTP ) ;
	  /* Sometimes two HTTP messages are stuck together, if so, dive back in for more! */
	  while ( msgLen >= 0 && guSIGMSG ) {
            msgLen = gHyp_secs1_rawIncoming ( (sSecs1*) pObject, pConcept, pAI, DATA_OBJECT_HTTP ) ;
	  }
      }
      else if ( flags & PROTOCOL_SECS1 ) {
        msgLen = gHyp_secs1_incoming ( (sSecs1*) pObject, pConcept, pAI ) ;
      }
      else if ( flags & PROTOCOL_HSMS) {
          msgLen = gHyp_hsms_incoming ( (sHsms*) pObject,pConcept,pAI ) ; 
      }

#ifdef AS_WINDOWS  
      /* Reset event only if structure is still valid. */
      if ( gHyp_concept_getSockObjType ( pConcept, socket ) != DATA_OBJECT_NULL ) {
	 ResetEvent ( pOverlapped->hEvent ) ;
      }
#endif
      if ( msgLen < 0 ) {

	/* We don't do the following, because secs1_rawIncoming and hsms_incoming
	 * will have already done this for us.
	 *

        gHyp_util_logError ( "Failed to write to socket %d",s ) ;
        if ( pNext == pData ) pNext = NULL ;
        gHyp_data_detach ( pData ) ;
        gHyp_data_delete ( pData ) ;
        pFirst = gHyp_data_getFirst ( pSockets ) ;

	*/

	return msgLen ;
      }
    }
  }
  return COND_SILENT ;
}

static int lHyp_sock_select_write_objects ( sConcept *pConcept, sData *pSockets, int mOffset ) 
{  
  sData
    *pData ;

  sInstance
    *pAI=NULL ;

  sBYTE
    objectType ;

  void
    *pObject ;

  sLOGICAL
    f; 

  sData
    *pNext,
    *pFirst ;

  for ( f=TRUE, pData = pFirst = gHyp_data_getFirst ( pSockets ) ;
        (f && pData) || (pFirst && pData != pFirst );
        f=FALSE, pData = pNext, pFirst = gHyp_data_getFirst ( pSockets ) ) {

    pNext = gHyp_data_getNext ( pData ) ;

    objectType = gHyp_data_getObjectType ( pData ) ;
    pObject = gHyp_data_getObject ( pData ) ;

    switch ( objectType ) {

    case DATA_OBJECT_SECS1 :
    case DATA_OBJECT_PORT :
    case DATA_OBJECT_HTTP :

      pAI = gHyp_secs1_parentAI ( (sSecs1*) pObject ) ;

      /* Write the next part of eAgain out to the socket, 
       * then set wait for the socket to be clear again. 
       */
      gHyp_secs1_rawOutgoingEagain ( (sSecs1*) pObject, pAI, mOffset ) ;

    }
  }
  return COND_SILENT ;
}

static int lHyp_sock_select_egain ( sConcept *pConcept, sData *pSockets, int timeout ) 
{  

  /* Of the sockets that have pEagain pending write data, find any that has a
   * timeout which is less than 'timeout' and return the number of milliseconds.
   */
  int 
    mTimeoutMax,
    mTimeoutMin,
    mTimeout ;

  sData
    *pData ;

  sBYTE
    objectType ;

  void
    *pObject ;

  sLOGICAL
    f; 

  sData
    *pNext,
    *pFirst ;

  mTimeoutMax = timeout * 1000 ;
  mTimeoutMin = timeout * 1000 ;

  for ( f=TRUE, pData = pFirst = gHyp_data_getFirst ( pSockets ) ;
        (f && pData) || (pFirst && pData != pFirst );
        f=FALSE, pData = pNext, pFirst = gHyp_data_getFirst ( pSockets ) ) {

    pNext = gHyp_data_getNext ( pData ) ;
    objectType = gHyp_data_getObjectType ( pData ) ;
    pObject = gHyp_data_getObject ( pData ) ;

    switch ( objectType ) {
    case DATA_OBJECT_SECS1 :
    case DATA_OBJECT_PORT :
    case DATA_OBJECT_HTTP :

      mTimeout = gHyp_secs1_eagainTimeout ( (sSecs1*) pObject, timeout ) ;
      if ( mTimeout < mTimeoutMin ) mTimeoutMin = mTimeout ;

    }
  }

  if ( mTimeoutMin < mTimeoutMax ) {
    /*gHyp_util_debug("Wait %d milliseconds instead of %d seconds",mTimeoutMin,timeout);*/
    return mTimeoutMin ;
  }
  else
    return 0 ;
}




static int lHyp_sock_select_FDSET_hosts ( sConcept *pConcept, sData *pHosts )
{    
  sChannel
    *pChannel ;

  sLOGICAL
    f; 
  
  SOCKET
    s ;

  char
    *pMsgOff ;

  int  
    *pNbytes ;

  OVERLAPPED
          *pOverlapped ;

  sData
    *pData,
    *pFirst,
    *pNext ;

  giOffsetHosts = giNumSelectEvents ;

  for ( f=TRUE, pData = pFirst = gHyp_data_getFirst ( pHosts ) ;
        (f && pData) || (pFirst && pData != pFirst );
        f=FALSE, pData = pNext, pFirst = gHyp_data_getFirst ( pHosts ) ) {

    pNext = gHyp_data_getNext ( pData ) ;
    pChannel = (sChannel*) gHyp_data_getObject ( pData ) ;
    s = gHyp_channel_socket ( pChannel ) ;
    pMsgOff = (char*) gHyp_channel_buffer ( pChannel ) ;
    pNbytes = gHyp_channel_pNbytes ( pChannel ) ;
    *pNbytes = 0 ;
    pOverlapped = gHyp_channel_overlapped ( pChannel ) ;
    if ( lHyp_sock_read ( s, 
                          pMsgOff,
                          MAX_MESSAGE_SIZE,
                          SOCKET_TCP,
                          -1,
                          pNbytes,
                          pOverlapped,
			  FALSE) == 0 ) {

      /*gHyp_util_debug("Adding host %d to offset %d",s,giNumSelectEvents);*/
#ifdef AS_WINDOWS
      gsEvents[giNumSelectEvents++] = pOverlapped->hEvent ;      
#else
      gsEvents[giNumSelectEvents++] = s ;      
#endif
    }
    else {
      gHyp_util_logError ( "Failed to read overlapped I/O from socket %d",s ) ;
      if ( pNext == pData ) pNext = NULL ;
      gHyp_data_detach ( pData ) ;
      gHyp_data_delete ( pData ) ;
      pFirst = gHyp_data_getFirst ( pHosts ) ;
    }
  }   

  giNumHosts = giNumSelectEvents - giOffsetHosts ;

  return COND_SILENT;
}

static int lHyp_sock_select_read_hosts ( sConcept *pConcept, sData *pClients, sData *pHosts )
{
  char
    *pBuf,
    *pMsgOff ;

  SOCKET
    socket ;

  int
    ret,
    offset=0,
    nBytes ;

  sChannel
    *pChannel ;

  sSSL
    *pSSL ;
  
  sData
     *pData, 
     *pNext=NULL,
     *pFirst;

#ifdef AS_WINDOWS

  int
    i;

#else

  sLOGICAL
    f ;

#endif

  OVERLAPPED
     *pOverlapped ;

#ifdef AS_WINDOWS

  i = giOffset - giOffsetHosts ;

  if ( i >=0 && i < giNumHosts ) {

    pData = gHyp_data_getChildBySS ( pHosts, i ) ;
    pChannel = (sChannel*) gHyp_data_getObject ( pData ) ;
    socket = gHyp_channel_socket ( pChannel ) ; 

#else

  /* VMS and UNIX */
  for ( f=TRUE, pData = pFirst = gHyp_data_getFirst ( pHosts ) ;
        (f && pData) || (pFirst && pData != pFirst );
        f=FALSE, pData = pNext, pFirst = gHyp_data_getFirst ( pHosts ) ) {

    pNext = gHyp_data_getNext ( pData ) ;
    pChannel = (sChannel*) gHyp_data_getObject ( pData ) ;
    socket = gHyp_channel_socket ( pChannel ) ;  

    if ( FD_ISSET ( socket, &gsReadFDS ) ) 

#endif
    {
      /* Determine buffer position in which to read message. */
      pBuf = gHyp_channel_buffer ( pChannel ) ;
      offset = gHyp_channel_offset ( pChannel )  ;
      pMsgOff = pBuf + offset ;
      pOverlapped = gHyp_channel_overlapped ( pChannel ) ;
      pSSL = gHyp_channel_getSSL ( pChannel ) ;
      nBytes = gHyp_sock_read ( socket, 
                                pMsgOff,
                                MAX_MESSAGE_SIZE,
                                SOCKET_TCP,
                                0,
                                NULL,
                                pOverlapped,
				pSSL ) ;
      
      /* Process results of read operation */
      if ( nBytes > 0 ) {
          
#ifdef AS_WINDOWS
        ResetEvent ( pOverlapped->hEvent ) ;
#endif
        /* Keep track of maximum TCP/IP message size */
        if ( nBytes > gHyp_channel_maxMsgSize ( pChannel ) ) 
          gHyp_channel_setMaxMsgSize ( pChannel, nBytes ) ;
      
        if ( socket == giLoopback ||
	     strcmp ( gHyp_channel_target (pChannel), gzLocalAddr ) == 0  ) {

          /* Don't send messages to the localHost or loopback!! */
	  /*gHyp_util_debug("Received message on loopback channel");*/
	  gHyp_channel_resetBuffer ( pChannel ) ;

	  return COND_SILENT ;

	}
	else {

          /* Process/route the message */
          gHyp_util_logInfo ( "==Net Read==\n" ) ;
          ret = gHyp_router_process ( pConcept,
				      pClients,
				      pHosts,
				      pBuf, 
				      pMsgOff, 
				      nBytes,
				      &offset,
				      gHyp_channel_path ( pChannel ) ) ;
	  gHyp_channel_setOffset ( pChannel, offset ) ;
	  return ret ;
	}
      }
      else {
        if ( nBytes < 0 ) {
        
          /*gHyp_util_output ( "==NET Error==\n" ) ;*/
          gHyp_util_logError ( "Lost connection to '%s' at '%s'",
                               gHyp_channel_path ( pChannel ),
                               gHyp_channel_target ( pChannel ) ) ;
        }
        else {
          /*gHyp_util_output ( "==NET I/O====\n" ) ;*/
          gHyp_util_logError( "Zero bytes from '%s' at %s, closing connection.",
                              gHyp_channel_path ( pChannel ),
                              gHyp_channel_target ( pChannel ) ) ;
        }

	/* Remove any alias references from the cache */
	gHyp_tcp_removeAliases ( gHyp_channel_target ( pChannel ) ) ;

        if ( pNext == pData ) pNext = NULL ;
        gHyp_data_detach ( pData ) ;
        gHyp_data_delete ( pData ) ;
        pFirst = gHyp_data_getFirst ( pHosts ) ;
      }
    }
  }
  return COND_SILENT ;
}

static int lHyp_sock_select_FDSET_listenTCP ( sConcept *pConcept, SOCKET listenTCP )
{
  if ( listenTCP != INVALID_SOCKET ) {

    giOffsetListen = giNumSelectEvents ;

    if ( lHyp_sock_read ( listenTCP, 
                          NULL,
                          0,
                          SOCKET_LISTEN,
                          -1,
                          NULL,
                          &gsTCPlistenOverlapped,
			  FALSE) == 0 ) {

    /*gHyp_util_debug("Adding listen TCP %d to offset %d",listenTCP,giNumSelectEvents);*/

#ifdef AS_WINDOWS
      gsEvents[giNumSelectEvents++] = gsTCPlistenOverlapped.hEvent ;      
#else
      gsEvents[giNumSelectEvents++] = listenTCP ;
#endif
    }
    else {
      gHyp_util_logError ( "Failed to read overlapped I/O from socket %d",listenTCP ) ;
      return COND_ERROR ;
    }
  }
  return COND_SILENT ;
}

static int lHyp_sock_select_read_listenTCP ( sConcept *pConcept, 
                                             sData *pHosts,
                                             SOCKET listenTCP )
{
  char
    addr[HOST_SIZE+1],
    host[HOST_SIZE+1] ; 

  SOCKET 
    socket ;

#ifdef AS_SSL
  sSSL
    *pSSL ;
  sChannel
    *pChannel ;
#endif

  sData
    *pData ;

  if ( listenTCP != INVALID_SOCKET ) {

#ifdef AS_WINDOWS
    if ( giOffset == giOffsetListen ) {
#else
      /* Read connection requests from TCP/IP */
      if ( FD_ISSET ( listenTCP, &gsReadFDS ) ) {
#endif
    
        /* Incoming Internet connection request */
        socket = gHyp_tcp_checkInbound ( listenTCP, 
                                         host,
                                         addr,
					 FALSE ) ;
        if ( socket != INVALID_SOCKET ) {
          pData = gHyp_sock_createNetwork ( pHosts, host, addr, socket ) ;
#ifdef AS_SSL
	  pSSL = gHyp_concept_getSSL ( pConcept ) ;
	  if ( pSSL ) {
	    pChannel = (sChannel*) gHyp_data_getObject( pData ) ;
	    pSSL = gHyp_sock_copySSL ( pSSL ) ;
	    gHyp_channel_setSSL ( pChannel, pSSL ) ;
	  }
#endif
	}

	gHyp_instance_signalConnect ( gHyp_concept_getConceptInstance( pConcept), socket, giARservicePort, NULL_DEVICEID ) ;

#ifdef AS_WINDOWS
        ResetEvent ( gsTCPlistenOverlapped.hEvent ) ;
#endif
      } 
    }
    return COND_SILENT ;
  }
  
static int lHyp_sock_select_FDSET_inbox ( sConcept *pConcept, HANDLE inbox )
{
#ifdef AS_WINDOWS
  int
    offset = 0 ;
#endif

  if ( inbox == INVALID_HANDLE_VALUE ) return COND_SILENT ;

  giOffsetInbox = giNumSelectEvents ;

#ifdef AS_WINDOWS
  /* Windows overlapped I/O requires that specify the buffer at the FDSET step so we
   * must determine where the buffer read should start */
  offset = giInboxOffset ;
  gpzInboxOffset = gzInboxBuf + offset ;
#endif

#if defined ( AS_VMS ) && !defined ( AS_MULTINET )  
  if ( giNumSelectEvents == 0 ) { 
    /* The inbox is the only socket. Set the flag so we will read
     * and block from the mailbox.
     */
    guSIGMBX = 1 ;
    return COND_SILENT ;
  }
#endif

  /*if ( offset > 0 ) gHyp_util_logWarning("Buffer %d before read = '%s'",offset,gzInboxBuf ) ;*/

  giInboxNbytes = 0 ;
  if ( lHyp_sock_read ( (SOCKET) inbox, 
                        gpzInboxOffset, 
                        MAX_MESSAGE_SIZE, 
                        SOCKET_FIFO, 
                        -1, 
                        &giInboxNbytes,
                        &gsInboxOverlapped,
			FALSE) == 0 ) {
 
    /*gHyp_util_debug("Adding inbox %d to offset %d",inbox,giNumSelectEvents);*/

#ifdef AS_WINDOWS
    gsEvents[giNumSelectEvents++] = gsInboxOverlapped.hEvent ;
#else
    gsEvents[giNumSelectEvents++] = inbox ;
#endif
  }
  else {
    gHyp_util_logError ( "Failed to read overlapped I/O from inbox (%d)",inbox ); 
    gHyp_concept_closeReader ( pConcept ) ;
    return COND_ERROR ;
  }

  return COND_SILENT ;
}

static int lHyp_sock_select_read_inbox ( sConcept *pConcept,
                                         sData *pClients, 
                                         sData *pHosts, 
                                         HANDLE inbox,
					 int timeout )
{ 
  int
    offset = 0,
    nBytes = 0 ;
  
  /* If inbox is undefined, then its because we are ROOT and we have no inbox */
  if ( inbox == INVALID_HANDLE_VALUE ) return COND_SILENT ;

  /* Read from:
   *    hs -t ar -r
   *            /local/fifo/ar                  ROOT fifo       
   *            /local/fifo/.ar                 ROOT directory
   *                                            
   *    ROOT Suffix R = "/local/fifo/.ar"
   *
   *    hs -t concept
   *            R/concept                       concept fifo            
   *            R/.concept                      concept directory
   *    
   *    hs -t child/concept
   *            R/.concept/child                child fifo
   *            R/.concept/.child               child directory
   *    
   *    hs -t subchild/child/concept
   *            R/.concept/.child/subchild      subchild fifo
   *            R/.concept/.child/.subchild     subchild directory
   */

#ifdef AS_WINDOWS

  /* WINDOWS and VMS do not use select */
  if ( giOffset == giOffsetInbox ) {

    /* gpzInboxOffset was already determined in sock_select_FDSET_inbox */

#else

  /* UNIX and VMS */

#ifdef AS_UNIX

  if ( FD_ISSET ( inbox, &gsReadFDS ) ) { 

#else

  if ( guSIGMBX ) {

#endif

    /* For UNIX and VMS, the buffer offset is needed at this time */
    offset = giInboxOffset ;
    gpzInboxOffset = gzInboxBuf + offset ;
    
#endif

    /*if ( offset > 0 ) gHyp_util_logWarning ("Buffer before read = '%s'",gzInboxBuf ) ;*/
    nBytes = gHyp_sock_read ( (SOCKET) inbox, 
                              gpzInboxOffset,
                              MAX_MESSAGE_SIZE,
                              SOCKET_FIFO,
                              timeout, /* A 0 tells sock_read that overlapped I/O has completed */
                              &giInboxNbytes,
                              &gsInboxOverlapped,
			      NULL ) ;
    if ( nBytes > 0 ) {
      
      gHyp_util_logInfo ( "==FIFO Read==\n" ) ;
#ifdef AS_WINDOWS
      ResetEvent ( gsInboxOverlapped.hEvent ) ;
#endif
      giInboxMaxMsgSize = MAX ( nBytes, giInboxMaxMsgSize ) ;

      return gHyp_router_process ( pConcept,
                                   pClients,
                                   pHosts,
                                   gzInboxBuf,
                                   gpzInboxOffset,
                                   nBytes,
				   &giInboxOffset,
				   "" ) ;
    }
    else if ( nBytes < 0 ) {
      /* gHyp_util_output ( "==FIFO Error=\n" ) ;*/
      gHyp_util_logError ( "Lost connection to %s",gzInboxPath ) ;
      gHyp_instance_signalHangup ( gHyp_concept_getConceptInstance (pConcept), 
			    (SOCKET) inbox, 0, NULL_DEVICEID ) ;
      gHyp_concept_closeReader ( pConcept ) ;
      return COND_ERROR ;
    }
    else {
      /* Zero bytes 
       * Only in VMS is the inbox owned by the parent, so we need to know when the
       * parent detaches.
       */
      /*gHyp_util_output ( "==FIFO I/O===\n" ) ;*/
      /*gHyp_util_logError ( "Zero bytes from %s",gzInboxPath ) ;*/
      
#ifdef AS_VMS
      /* If not the AUTOROUTER ROOT, 
       * and if not a single MBX channel,
       * and not an ALARM trigger
      then close */
      if ( !(guRunFlags & RUN_ROOT) && timeout > 0 && !guSIGALRM ) 
	gHyp_concept_closeReader ( pConcept ) ;
#endif
    }
  }

  return COND_SILENT ;
}

int gHyp_sock_select ( sConcept* pConcept,
                       HANDLE inbox,
                       SOCKET listenTCP,
                       sData *pClients,
                       sData *pHosts,
                       sData *pSockets )
{
  /* Description:
   *
   *    Use the select function to find the sockets that are ready to read.
   */

  
  int
    timeout,
    mTimeout,
    sTimeout,
    uTimeout,
    eagainTimeout,
    timeToAlarm,
    cond ;

  sLOGICAL
    signalInstance = TRUE ;

#ifndef AS_WINDOWS
   struct timeval
    timer ;
#endif

  /* Append to the log */
  lHyp_sock_select_checkLog() ;
  
  /* Stamp time before posting read. */
  gsCurTime = time(NULL) ;

  /* Find the lowest timeout value for all the instances. This value is
   * used in the select() call.
   * If any instances have a zero timeout value, then set an alarm signal 
   * for that instance.
   */
  timeout = gHyp_concept_setAlarms ( pConcept ) ;
  
  /* Is garbage collection needed? */
  if ( gsCurTime >= giNextAlarm ) {
    /* Do garbage collection on idle sockets */
    gHyp_sock_cleanClient( pClients ) ;
    giNextAlarm = lHyp_sock_nextAlarmTime( pClients ) ;
    /*gHyp_util_debug("Next clean in %d seconds",(giNextAlarm-gsCurTime));*/
   }
  /* Calculate time to next check. */
  timeToAlarm = giNextAlarm - (int)gsCurTime + 1 ;
  timeToAlarm = MIN ( timeToAlarm, IDLE_INTERVAL ) ;

  /* The timeout suplied is the first event time from all the instances.
   * If the garbage collection timeout occurs before the event time, then we need
   * to update the timeout and we would also not signal an alarm condition to the instances
   * should the timeout occur.
   */
  if ( timeToAlarm < timeout ) {
    signalInstance = FALSE ;
    timeout = timeToAlarm ;
  }
  

#ifndef AS_WINDOWS
  /* Clear read descriptor mask */
  FD_ZERO ( &gsReadFDS) ;
#endif

  giNumSelectEvents = 0 ;
  giOffset = -1 ;
  giNumFds = 0 ;
  giNumHosts = 0 ;
  gsSocketToCancel = INVALID_SOCKET ;

  if ( guDebugFlags & DEBUG_DIAGNOSTICS )
    gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS, 
			   "diag:  Select timeout is %d seconds",timeout );

  /* Application fd's */
  cond = lHyp_sock_select_FDSET_objects ( pConcept, pSockets, timeout ) ;
  if ( cond < 0 ) return cond ;

  /* Internet listen port */
  cond = lHyp_sock_select_FDSET_listenTCP ( pConcept, listenTCP ) ;
  if ( cond < 0 ) return cond ;

  /* Set channels to hosts */
  cond = lHyp_sock_select_FDSET_hosts ( pConcept, pHosts ) ;
  if ( cond < 0 ) return cond ;

  /* Inbox */
  cond = lHyp_sock_select_FDSET_inbox ( pConcept, inbox ) ;
  if ( cond < 0 ) return cond ;

  /* We have a timeout in seconds.  But, there may be a pending write which should 
   * be attempted before timeout.  
   * If that is the case, get the total number of milliseconds to the closest one. 
   * Otherwise, eagainTimeout will be zero.
   */
  eagainTimeout = lHyp_sock_select_egain ( pConcept, pSockets, timeout ) ;

  /* Set up timer controls - total millseconds, whole second, and fractional useconds */
  if ( eagainTimeout == 0 ) {
    /* Use timeout value */
    mTimeout = timeout * 1000 ;
    sTimeout = timeout ;
    uTimeout = 0 ;
    /*gHyp_util_debug("We will wait %d seconds (No writes before first timeout)",timeout ) ;*/
  }
  else {

    /* Use eagainTimeout value */
    mTimeout = eagainTimeout ;
    sTimeout = eagainTimeout/1000 ;
    uTimeout = (eagainTimeout-((eagainTimeout/1000)*1000))*1000 ;
    /*gHyp_util_debug("We will wait %d milliseconds",mTimeout ) ;*/

  }

#ifdef AS_WINDOWS

  /*gHyp_util_debug("Waiting, timeout in %d milliseconds",mTimeout);*/
  if ( giNumSelectEvents == 0 ) {

    /* Cannot do a WaitForMultipleObjects with zero objects.  If the
     * timeout is greater than zero, we had better do a sleep otherwise
     * we would just spin (be back at this same point again).
     */

    if ( timeout > 0 ) {
#ifdef AS_WINDOWS
       Sleep ( mTimeout ) ;
#else
       sleep ( timeout ) ;
#endif
    }
    return COND_SILENT ;
  }

  giOffset = WaitForMultipleObjects ( giNumSelectEvents, 
                                      (CONST HANDLE*)&gsEvents, 
                                      FALSE,
                                      mTimeout ) ;

  if ( giOffset == WAIT_FAILED ) {
    gHyp_util_sysError ( "WaitForMultipleObjects failed" ) ;
    return COND_ERROR ;
  }
  else if ( giOffset == WAIT_TIMEOUT ) {
    /* Timeout occurred */
    /*gHyp_util_debug("Timeout occurred");*/
    /* Could be that there is something to write */
    if ( eagainTimeout > 0 ) lHyp_sock_select_write_objects ( pConcept,pSockets, mTimeout ) ;
    if ( signalInstance ) gHyp_concept_setAlarms ( pConcept ) ;
    return COND_SILENT ;
  }
  else {
    giOffset = giOffset - WAIT_OBJECT_0 ;
    /*gHyp_util_debug("%d found.",giOffset) ;*/
    timeout = 0 ;
  }
#else  /* UNIX and VMS */

  /* Set timeout argument in select call */
  timer.tv_sec = sTimeout ;
  timer.tv_usec = uTimeout ;
      
#if defined ( AS_VMS ) && !defined ( AS_MULTINET )

  /* NO select_wake() utility!!!  
   * How do we break from the select() in order to service a message on the inbox channel.? */

  if ( giNumSelectEvents > 0 ) {

    /* There is at least one event that must be select()'d.
     * 
     * VMS mailboxes and LAT connections cannot be used in select(), so
     * we have to use a QIO instead.  With mailboxes, there is a WRITEATTN modifier that
     * signals an AST when there is data to read on the port.
     *
     * With MULTINET TCP, we can use select_wake() in the AST to unblock the select()
     * call.  But with UCX, we have no such function.  And if you try to break out of it,
     * with a longjmp, bad OS things happen.  But, if we have a loopback TCP channel
     * open, we will send the signal that way, otherwise we have only two options left.
     * 1. If there are no other ports in use except a single mailbox channel
     * (such as with HyperScript embedded in PROMIS), then we do not need to perform the 
     * select() at all,  We simply read and block on the mailbox channel. 
     * But if there are other select() ports, we put a 0.1 second timeout on the select call, 
     * essentially creating a cheap poll. 
     * Not ideal, the alternative is to use QIO's for the reads and ASTs.  That has
     * been initially coded in sock.qio 
     */
    if ( timeout > 0 &&
         inbox != INVALID_HANDLE_VALUE &&
	 (giLoopbackType != SOCKET_TCP || giLoopback == INVALID_SOCKET) ) {
      /* We have a mailbox to read from without a loopback port or select_wake() utility */
      /*gHyp_util_debug("WARNING - FAST POLLING 1/10 second");*/
      timer.tv_sec = 0 ;
      timer.tv_usec = 100000 ;
    }
  }
  else {

    /* No select events, just the mailbox to read. */
    if ( inbox != INVALID_HANDLE_VALUE ) {
      /*gHyp_util_debug("Lone reader");*/
#ifdef SIGALRM
      gHyp_signal_establish ( SIGALRM, lHyp_sock_alarmHandler ) ;
      gHyp_signal_unblock ( SIGALRM ) ;    /* Re-establish handler */
#endif
    }
  }
#endif

#ifdef AS_VMS
  /* For VMS, we DO want to use the select() call when the number of 
   * select events is > 0 or the inbox channel is not defined.
   * (We don't want to use the select() call when the number of select events
   * is zero and the inbox channel is defined.)
   */
  if ( giNumSelectEvents > 0 || inbox == INVALID_HANDLE_VALUE ) { 

#endif
    /* If there's no select events, and there's no timeout, then there's no point */
    if ( giNumSelectEvents == 0 && timeout == 0 ) return COND_SILENT ;

    /*gHyp_util_debug("Selecting, timeout in %d seconds",timeout);*/
    giNfound = select ( FD_SETSIZE, &gsReadFDS, NULL, NULL, &timer ) ;
    if ( giNfound < 0 ) {
      gHyp_util_sysError ( "Select failed" ) ;
      return COND_ERROR ;
    }
    else if ( giNfound == 0 ) {

      /* Timeout occurred */
      /*gHyp_util_debug("Timeout!!!");*/

      /* Could be that there is something to write */
      if ( eagainTimeout > 0 ) lHyp_sock_select_write_objects ( pConcept,pSockets, mTimeout ) ;

      if ( signalInstance ) gHyp_concept_setAlarms ( pConcept ) ;
#ifdef AS_VMS
      if ( !guSIGMBX ) 
#endif
	return COND_SILENT ;
    }
    else 
      /*gHyp_util_debug("%d found.",giNfound) ;*/
    
    /* Reset timeout because its been done. */
    timeout = 0 ;

#ifdef AS_VMS

  }
#endif

#endif

  /*gHyp_util_debug("READING, timeout = %d",timeout);*/
  cond = lHyp_sock_select_read_inbox( pConcept,pClients,pHosts,inbox,timeout) ;
  if ( cond < 0 ) return cond ;
  
  cond = lHyp_sock_select_read_hosts( pConcept, pClients, pHosts ) ;
  if ( cond < 0 ) return cond ;

  cond = lHyp_sock_select_read_listenTCP ( pConcept, pHosts, listenTCP ) ;
  if ( cond < 0 ) return cond ;

  cond = lHyp_sock_select_write_objects ( pConcept,pSockets,timeout ) ;
  if ( cond < 0 ) return cond ;

  cond = lHyp_sock_select_read_objects ( pConcept,pSockets ) ;
  if ( cond < 0 ) return cond ;

  return COND_SILENT ;
}
