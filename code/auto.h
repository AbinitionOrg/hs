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
#if defined ( WIN32 ) || defined ( __WIN32 ) || defined ( _WIN32_WCE )
#define AS_WINDOWS 1
#include <windows.h>
#ifdef _WIN32_WCE
#define AS_PPC 1
#endif
#elif defined ( __VMS )
#define AS_VMS 1
#else
#define AS_UNIX 1
#include <unistd.h>	/* UNIX standard functions */
#endif

#ifdef __alpha
#define AS_ALPHA 1
#endif

#include <stdarg.h>	/* va* functions */
#include <stdio.h>      /* standard IO functions */
#include <stddef.h>     /* standard types */
#ifdef AS_UNIX
#include <stdint.h>	/* int32_t def */
#endif
#include <stdlib.h>	/* standard library functions */
#if HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include <time.h>	/* for localtime function */
#include <errno.h>	/* errno */
#include <signal.h>	/* signal processing functions and structures */
#include <assert.h>     /* assert function */
#include <ctype.h>	/* is* macros and functions */
#include <string.h>	/* string functions */
#include <setjmp.h>	/* setjmp and longjmp functions */
/*#include <math.h>	 * math functions */

#ifdef AS_MEMTRACK
#include "memtrack.h"
#else

#define AllocMemory(size) malloc(size)

#define AllocCopyString(s) strdup(s)

#define ReAllocMemory(p, newsize) realloc(p,newsize)

#define ReleaseMemory(p) free(p)
  
#endif

struct dsc_t {
  unsigned short	dsc_w_length ;
  char			dsc_b_dtype ;
  char			dsc_b_class ;
  char			*dsc_a_pointer ;
} ;

struct endian_t 
{
  union {
    double		d ;
    float		f ;
#ifdef AS_UNIX
    int32_t		sl ;
    uint32_t		ul ;
#else
    long		sl ;
    unsigned long	ul ;
#endif
    short		ss ;
    unsigned short	us ;
    unsigned char       b[8] ;		
  } x ;
} ;


struct iosb_t {
    short   ioStatus ;
    short   nBytes ;
    long    buf;
} ;  

#ifndef	FD_SETSIZE
#define	FD_SETSIZE	32
#endif

/* Local Includes for HYPERSCRIPT */
#include "typdef.h"	/* Typdef's */
#include "hypdef.h"	/* HyperScript specific */

/* External Global variables used HYPERSCRIPT */

/* File names and other string variables */
extern char		gzLogName[MAX_PATH_SIZE+1] ;
extern FILE		*gsLog ;	/* Log */

/* Socket channels */
extern int		giServicePort ;	/* Service Channel id */
extern int		giARservicePort ;	/* Service Channel id */
extern SOCKET		giLoopback ;		/* Loopback socket */
extern sBYTE		giLoopbackType ;	/* Loopback socket type */
extern HANDLE		giInboxLoopback ;	/* Loopback socket to inbox */
extern SOCKET		giListenLoopback_R ;	/* Loopback socket (Read) to listen socket */
extern SOCKET		giListenLoopback_W ;	/* Loopback socket (Write) to listen socket */

/* For resolving address */
extern sLOGICAL		giResolveFlag1 ;
extern sLOGICAL		giResolveFlag2 ;
extern char		gzResolveAddr[HOST_SIZE+1];

/* Run-time variables */
extern int		giPID ;		/* PID of process */
extern char		gzPIDfileSpec[MAX_PATH_SIZE+1] ; /* PID file */
extern time_t		gsCurTime ; 	/* Current time, updated regularily. */
extern sLOGICAL 	guTimeStamp ;	/* Integer timeStamp. */
extern unsigned short	guRunFlags ; 	/* General flags */
extern unsigned short	guHeapMultiplier ;

extern sInstance *gpAI ;	/* HyperScript instance */
extern sInstance *gpAImain  ;	/* Main HyperScript instance */

/* Environment variables */
extern char    		gzAUTOROUTER[OBJECT_SIZE+1];	/* AUTOROUTER root name "router" */
extern char    		gzAUTOHOST[OBJECT_SIZE+1];	/* AUTOHOST root name "hostname" */
extern char    		gzAUTOFIFO[MAX_PATH_SIZE+1];	/* AUTOFIFO path */
extern char    		gzAUTOBIN[MAX_PATH_SIZE+1];	/* AUTOBIN path */
extern char    		gzAUTORUN[MAX_PATH_SIZE+1];	/* AUTORUN path */
extern char    		gzAUTOLOG[MAX_PATH_SIZE+1];	/* AUTOLOG path */
extern char    		gzAUTOSPOOL[MAX_PATH_SIZE+1];	/* AUTOLOG path */
extern char		gzInstance[INSTANCE_SIZE+1] ;
extern char		gzConcept[TARGET_SIZE+1] ;
extern char		gzConceptPath[MAX_PATH_SIZE+1] ;
extern char		gzParent[MAX_PATH_SIZE+1] ;
extern char		gzRoot[MAX_PATH_SIZE+1] ;
extern char		gzInboxPath[MAX_PATH_SIZE+1] ;
extern char		gzOutboxPath[MAX_PATH_SIZE+1] ;
extern char		gzLocalHost[MAX_PATH_SIZE+1] ;
extern char		gzLocalAddr[MAX_PATH_SIZE+1] ;

/* HyperScript code variables */
extern sData*		gpsTempData ;	/* If defined, its a leak */
extern sData*		gpsTempData2 ;	/* If defined, its a leak */
extern int		giMaxExprSize ; /* Heap allocation for expression and stack */
extern int		giMaxStackDepth ; /* Heap allocation for stack */
extern int		giMaxFrameDepth ; /* Heap allocation for frame */
extern sConcept*       	gpsConcept ;    /* Pointer to concept */
extern unsigned short	guDebugFlags ; 	/* Debug flags */
extern unsigned		guErrorCount ;	/* HyperScript error count */
extern int		giLineCount ;
extern int		giProgramCount ;
extern int		giCondition ;   /* Return condition */
extern sLOGICAL		giJmpEnabled ;  /* If longjmp is enabled from handlers*/
extern SOCKET           gsSocketToCancel ;/* Should we want to cancel I/O */
extern int		giJmpLevel ;    /* Current depth of saved jmp env */
extern int		giJmpRootLevel ;  /* Current root depth of jmp, default = 1 */
extern jmp_buf		gsJmpStack[MAX_JMP_LEVEL+1] ; /* For setjmp,longjmp */ 
extern jmp_buf		gsJmpOverride ;  /* Jump Override */
extern sLOGICAL		giJmpOverride ;  /* If longjmp override should be done */
extern FILE*		gsPP ;		/* Hyperscript input stream */
extern sAImsg*		gpsAImsg  ;	/* Working message structure */
extern char             *gzaInstanceState[6] ;

/* Variables for the gHyp_util_getopt function call - in util.c */
extern int		giOpterr ;	/* Useless, never set or used */
extern int		giOptind ;	/* Index into parent argv vector */
extern int		giOptopt ;	/* Character checked for validity */
extern char		*gzOptarg ;	/* Argument associated with option */

/* Global signal flags */
extern unsigned		guSIGIO ;      	/* Set when SIGIO */
extern unsigned		guSIGPIPE ;    	/* Set when SIGPIPE */
extern unsigned		guSIGALRM ;    	/* Set when SIGALRM */
extern unsigned		guSIGINT ;     	/* Set when SIGINT */
extern unsigned		guSIGTERM ;    	/* Set when SIGTERM */
extern unsigned		guSIGHUP ;     	/* Set when SIGHUP */
extern unsigned		guSIGMBX ;     	/* Set when SIGMBX */
extern unsigned		guSIGMSG ;	/* Set when SIGMSG */

/* Function prototypes */
#include "aimsg.h"
#include "data.h"
#include "channel.h"
#include "hash.h"
#include "util.h"
#include "router.h"
#include "sock.h"
#include "signl.h"
#include "tcp.h"
#include "branch.h"
#include "concept.h" 
#include "cgi.h"
#include "dateparse.h"
#include "env.h"
#include "fileio.h"
#include "frame.h"
#include "function.h"
#include "hyp.h"
#include "gd.h"
#include "instance.h"
#include "label.h"
#include "load.h"
#include "method.h"
#include "operand.h"
#include "operator.h"
#include "promis.h"
#include "parse.h"
#include "hsms.h"
#include "secs.h"
#include "secs1.h"
#include "secs2.h"
#include "route.h"
#include "port.h"
#include "http.h"
#include "sort.h"

#ifdef AS_SSL
#include "ssl.h"
#endif

#include "stack.h"
#include "stmt.h"
#include "system.h"

/* Don't make sql functions an option anymore */
#include "sql.h"

#ifdef AS_MAPI
#include "mapi.h"
#endif

#include "type.h"

