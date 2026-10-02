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

/**********************	HYPERSCRIPT INTERFACE ********************************/
#include "auto.h"	/* System Interface and Function Prototypes */

/**********************	EXTERNAL GLOBAL VARIABLES ****************************/

/**********************	INTERNAL GLOBAL VARIABLES ****************************/

/********************** INTERNAL OBJECT STRUCTURES ***************************/

/* There is one instance object for every 'instance' of a HyperScript
 * program.
 * The instance object contains the current state of the HyperScript
 * program, including the program stack, the program frame, and a data area for 
 * processing messages.
 */

struct secs_t {
  sWORD		id ;
  int		stream ;
  int		function ;
  int		TID ;
  int		SID ;
} ;

struct reply_t {

  /* REPLY message area:
   * Two uses:
   * 1. For an incoming query, 'msg' is used for outgoing reply.
   * 2. For an outgoing query, 'msg' is used for incoming reply.
   */
  sAImsg	*msg ;
  char		sender[SENDER_SIZE+1] ;
  char		method[METHOD_SIZE+1] ;
  char		transactionID[FIELD_SIZE+1];
  int		timeoutTime ;
  int           frameDepth ;
  sStack        *pStack ;
  sSecs	secs ;
} ;

struct handler_t {
  sHyp*		pHyp ;
  int		hypIndex ;
} ;

struct instance_t
{
  /* Attached objects, handlers, timeouts */
  struct {
    sConcept	*pConcept ;	/* Parent concept */
    sFrame	*pFrame ;      	/* Execution frames */
    unsigned char state ;      	/* State of instance */	
    int		timeOut ;      	/* TimeOut, in seconds */
    sHandler	handler[NUM_HANDLER_TYPES] ;
    sBYTE	eventType ;    	/* Type of event that's pending */
    time_t	eventTime ;    	/* Time of next event */
    time_t	createTime ;   	/* Time when instance was created */
    time_t	deathTime ;    	/* Scheduled death signal */
    time_t	timeOutTime ;  	/* Scheduled timeOut signal */
    time_t	alarmTime ;    	/* Scheduled alarm signal */
    time_t	beatTime ;    	/* Scheduled heartbeat signal */
    time_t	wakeTime ;    	/* Scheduled heartbeat signal */
  } exec ;	/* Execution area */

  struct {
    unsigned	     uERROR ;	/* Set when HyperScript error */
    unsigned	     uWARNING ;	/* Set when HyperScript warning */
    unsigned	     uMSG ;/* Set when message arrives */
    unsigned	     uMSGINTERRUPT ;/* Set when message interrupts */
    unsigned	     uMSGPENDING ;	/* Set when message pending */
    unsigned         uMETHODCALL ;  /* Set when a method is called */
    unsigned         uDEREFERENCE ; /* Set when dereferencing */
    unsigned         uIO ;
    unsigned         uTERM  ;
    unsigned         uHUP ; 
    int		     numHup ;
    int		     hupArg[MAX_SIGNALS] ;	/* The socket where the error occurred */
    int		     hupArg2[MAX_SIGNALS] ;	/* The port number where the error occurred */
    int		     hupArg3[MAX_SIGNALS] ;	/* The device id where the error occurred */
    unsigned         uALRM ;
    unsigned         uPIPE ;
    int		     numPipe ;
    int		     pipeArg[MAX_SIGNALS] ;	/* The socket where the error occurred */
    int		     pipeArg2[MAX_SIGNALS] ;	/* The port number where the error occurred */
    int		     pipeArg3[MAX_SIGNALS] ;	/* The device id where the error occurred */
    unsigned         uINT ;
    unsigned	     uCONNECT ;	/* Set when incoming connection */
    int		     numCon ;
    int		     conArg[MAX_SIGNALS] ;	/* The socket where the error occurred */
    int		     conArg2[MAX_SIGNALS] ;	/* The port number where the error occurred */
    int		     conArg3[MAX_SIGNALS] ;	/* The device id where the error occurred */
    unsigned         tokenSignal ;              /* Set-cleared when any signal condition has occurred */
  } signal ;

  struct {
    char	targetId[TARGET_SIZE+1] ;  /* Instance name without parent */
    char	targetPath[TARGET_SIZE+1] ;  /* Instance name with parent */
    int		outgoingDepth ;		/* Reply depth - outgoing */
    int		incomingDepth ;		/* Reply depth - incoming */
    int		rejectCount ;	/* To prevent MESSAGE spinning */
    int		startRQ ;	/* Pointer to rq start */
    int		startQQ ;	/* Pointer to qq start */
    int		endRQ ;		/* Pointer to rq end */
    int		endQQ ;		/* Pointer to qq end */
    sAImsg	*current ;	/* Current mesage */
    sAImsg	*incoming ;	/* Incoming mesage */
    sAImsg	*outgoing ;	/* Outgoing mesage */\
    sReply      *outgoingReply[MAX_REPLY_DEPTH];/* Outgoing reply message stack */
    sReply      *incomingReply[MAX_REPLY_DEPTH];/* Incoming reply message stack */
    sAImsg	*rq[MAX_QUEUE_DEPTH]; /* Outstanding requeue message queue */
    sAImsg	*qq[MAX_QUEUE_DEPTH]; /* Outstanding query message queue */
    sSecs	inSecs ;
    sSecs	qqSecs[MAX_QUEUE_DEPTH] ; /* Secs attachment */
    sSecs	rqSecs[MAX_QUEUE_DEPTH] ; /* Secs attachment */
  } msg ;	/* Message area */

  /* Device area:
   * 
   * This is a merge of the secs and port structures.
   */
  struct {

    /* Indexed by fd, which is either a serial or tcp value */
    int			numDevices ;
    SOCKET		fd[MAX_DEVICES] ;
    sWORD		id[MAX_DEVICES] ;
    
    /* Hash table for assigning methods to stream and function codes. */
    sHash		*(*pHash)[MAX_HASH_TABLE_SIZE] ;

    /* Secs area */
    sSecs2		*pSecs2[MAX_DEVICES] ;
    sLOGICAL		rBit[MAX_DEVICES] ;
    sBYTE		mlb ; /* Minimumn length bytes */
    sWORD		mhp ; /* Minimumn hsms port */
    sLOGICAL		xml ; /* If stored in xml format */

  } device ;

} ;


/***************	FUNCTION DEFINITIONS ********************************/

static void lHyp_instance_consumeMessage ( sInstance *pAI, 
					   int outgoingDepth )
{
  /* Description:
   *
   *	When a message arrives and invokes a method to execute, the tokens
   *	and values of the message must be consumed before the method
   *	executes.  A list of the tokens consumed is stored in the 
   *	variable 'args'.
   *
   * Arguments:
   *
   *	pAI						[R]
   *	- pointer to instance
   *
   *	outgoingDepth	      			[R]
   *	- where to put the outgoing reply
   *
   * Return value:
   *
   *	none
   *
   */
   
  /*
   * Take the incoming message and initialize the instance with its contents.
   * If the incoming message is a QUERY, then setup a REPLY message to be
   * sent when the method returns.
   */
  sData
    *pResult,
    *pData,
    *pMethodArgs=NULL,
    *pMethodVariable,
    *pLvalue,
    *pArgs=NULL,
    *pTV,
    *pReplyTV = NULL,
    *pVariable,
    *pToken,
    *pValue ;
  
  int
    ss,
    context;
  
  sLOGICAL
    isVector,
    isReply,
    isQuery,
    isEvent,
    deleteMethodArgs ;
  
  char
    *pMethodStr,
    *pVariableStr,
    target[TARGET_SIZE+1],
    sender[SENDER_SIZE+1] ;
  
  sAImsg
    *pAImsg ;

  sMethod
    *pMethod ;

  /* If the message mode is QUERY, initialize a list of REPLY tokens. */
  pAImsg = pAI->msg.incoming ;
  isQuery = gHyp_aimsg_isQuery ( pAImsg ) ;
  isEvent = gHyp_aimsg_isEvent ( pAImsg ) ;
  if ( isQuery ) pReplyTV = gHyp_data_new ( "_replytv_" ) ;
  isReply = gHyp_aimsg_isReply ( pAImsg ) ;

  /* Get a pointer to the tokens and values in the message */
  pTV = gHyp_aimsg_tv ( pAImsg ) ;
  /*gHyp_util_logInfo("pTV = %p",pTV);*/

  if ( !isReply ) {

    /* Merge the incoming pTV with pMethodArgs */

    pMethodStr = gHyp_aimsg_method ( pAImsg ) ; 
    pMethodVariable = gHyp_frame_findMethodVariable ( pAI->exec.pFrame, pMethodStr, pAI ) ;
    pMethod = (sMethod*) gHyp_data_getObject ( pMethodVariable ) ;
    pMethodArgs = gHyp_method_getArgs ( pMethod ) ;

    /* Create or retrieve the "args" variable, and clear it of any values. */
    pArgs = gHyp_frame_createVariable ( pAI, pAI->exec.pFrame, "args" ) ;
    gHyp_data_deleteValues ( pArgs ) ;
    deleteMethodArgs = FALSE ;

  }
  else {
    pMethodArgs = gHyp_data_new ( NULL ) ;
    deleteMethodArgs = TRUE ;

    pArgs = gHyp_frame_createVariable ( pAI, pAI->exec.pFrame, "replyargs" ) ;
    gHyp_data_deleteValues ( pArgs ) ;

  }
  /*gHyp_util_debug ( gHyp_data_print ( pTV ) ) ;*/

  /* Merge the argument values into the defined method args 
   *
   * Why is pReplyTV passed.  
   * Because if we abort, we need to delete it, otherwise it will leak.
   */
  pResult = gHyp_env_mergeData ( pMethodArgs, pTV, pAI, 0, FALSE, TRUE, FALSE, FALSE, pReplyTV, 0 ) ;
  
  /* Process pResult .*/
  pValue = NULL ;
  ss = -1 ;
  pLvalue = gHyp_data_new ( NULL ) ;
  isVector = (gHyp_data_getDataType( pResult ) > TYPE_STRING ) ;

  /* Get next defined method argument */
  while ( (pValue = gHyp_data_nextValue ( pResult, 
					  pValue, 
					  &context,
					  ss ) ) ) {
      	
    /* Create a new variable or overwrite an existing variable. */
    pVariableStr = gHyp_data_getLabel ( pValue ) ;
	
    if ( strcmp ( pVariableStr, "STATUS" ) == 0 )
      /* Special case: STATUS must be set as a global variable */
      pVariable = gHyp_frame_findRootVariable ( pAI->exec.pFrame, 
						"STATUS" ) ;
    else {
      pVariable = gHyp_frame_findVariable ( pAI, pAI->exec.pFrame, pVariableStr ) ; 
      if ( !pVariable )
        pVariable = gHyp_frame_createVariable ( pAI, pAI->exec.pFrame, 
					      pVariableStr ) ;
    }
    gHyp_data_setReference ( pLvalue, pVariableStr, pVariable);
	
    /* Store a reference to the variable in the "args" variable */
    pData = gHyp_data_new ( NULL ) ;
    gHyp_data_setReference ( pData, pVariableStr, NULL ) ;
    gHyp_data_append ( pArgs, pData ) ;
	
    if ( isQuery &&  gHyp_data_getCount ( pValue ) == 0 ) {
      /* The current message is a QUERY and the token has no values.
       * Keep a reference to it, that it is requesting a return value.
       */
      pToken = gHyp_data_new ( pVariableStr ) ;
      gHyp_data_append ( pReplyTV, pToken ) ;
    }
    else {
      /* Assign the token's values to the variable */
      pData = gHyp_type_assign ( pAI,
				 pAI->exec.pFrame,
				 pLvalue,
				 pValue,
				 gHyp_data_dataType ( pValue ),
				 FALSE, FALSE  ) ;
      gHyp_data_delete ( pData ) ;
    }
  }
 
  /* Delete the merged data and the working variable */
  gHyp_data_delete ( pResult ) ;
  gHyp_data_delete ( pLvalue ) ;
  if ( deleteMethodArgs ) gHyp_data_delete ( pMethodArgs ) ;
  
  if ( isQuery ) {
    
    /* Prepare the reply message */
    
    /* Add STATUS token. */
    pToken = gHyp_data_new ( "STATUS" ) ;
    gHyp_data_append ( pReplyTV, pToken ) ;
    
    /* Construct the REPLY message - return values are added later... */
    gHyp_aimsg_getTargetAddress ( pAImsg, sender ) ;
    gHyp_aimsg_getSenderAddress ( pAImsg, target ) ;
    
    if ( guDebugFlags & DEBUG_DIAGNOSTICS )
      gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
			   "diag : Storing outgoing reply for %s at depth %d", 
			   gHyp_aimsg_method(pAImsg), outgoingDepth ) ;

    pAI->msg.outgoingReply[outgoingDepth]->msg = gHyp_aimsg_new() ;
    gHyp_aimsg_initUnparse (	pAI->msg.outgoingReply[outgoingDepth]->msg,
				target,
				"reply",
				gHyp_aimsg_method ( pAImsg ),
				sender,
				gHyp_aimsg_getTID ( pAImsg ),
				gHyp_aimsg_getTS ( pAImsg ),
				pReplyTV ) ;
    pReplyTV = NULL ;
  }    

  if ( pReplyTV ) gHyp_data_delete ( pReplyTV ) ;

  return ;
}

sInstance* gHyp_instance_new ()
{
  /* Description:
   *
   *	Allocate memory for a new instance object.
   *
   * Arguments:
   *
   *	none
   *
   * Return value:
   *
   *	Pointer to new instance object.
   *
   */
  /* Create new instance */
  sInstance *pAI ;
  pAI = (sInstance*) AllocMemory ( sizeof ( sInstance ) ) ;
  assert ( pAI ) ;
  return pAI ;
}

sConcept *gHyp_instance_getConcept ( sInstance *pAI )
{
  return pAI->exec.pConcept ;
}

void gHyp_instance_swapFrames ( sInstance *pAI, sInstance *pAImain )
{
  sFrame
    *pFrameSave ;

  /* Switch frames */
  pFrameSave = pAI->exec.pFrame ;
  pAI->exec.pFrame = pAImain->exec.pFrame ;
  pAImain->exec.pFrame = pFrameSave ;

  return ;
}

void gHyp_instance_swapData ( sInstance *pAI, sInstance *pAImain )
{
  /* Swap the root area. */
  gHyp_frame_swapRootMethodData ( pAI->exec.pFrame, pAImain->exec.pFrame ) ;
  
  return ;
}

void gHyp_instance_swapDevices ( sInstance *pAI, sInstance *pAImain )
{

  int 
    numDevices,
    i ;

  sHash
    *pHash ;

  numDevices = pAI->device.numDevices ;
  i = 0 ;
  while ( numDevices < MAX_DEVICES && i< pAImain->device.numDevices ) {

    pAI->device.fd[numDevices] = pAImain->device.fd[i] ;
    pAImain->device.fd[i] = INVALID_SOCKET ;

    pAI->device.id[numDevices] = pAImain->device.id[i] ;
    pAImain->device.id[i] = NULL_DEVICEID ;

    pAI->device.pSecs2[numDevices] = pAImain->device.pSecs2[i] ;
    pAImain->device.pSecs2[i] = NULL ;

    pAI->device.rBit[numDevices] = pAImain->device.rBit[i] ;

    numDevices++ ;
  }
  pAI->device.numDevices = numDevices ;
  pAImain->device.numDevices = 0 ;

  pAI->device.mhp = pAImain->device.mhp ;
  pAI->device.mlb = pAImain->device.mlb ;
  pAI->device.xml = pAImain->device.xml ;

  for ( i=0; i<DEVICE_HASH_TABLE_SIZE; i++ ) {

    pHash = (*pAI->device.pHash)[i] ;
    (*pAI->device.pHash)[i] = (*pAImain->device.pHash)[i] ;
    (*pAImain->device.pHash)[i] = pHash ;
  }

  /* Transfer all handlers */
  for ( i=0; i<NUM_HANDLER_TYPES; i++ ) {
    if ( pAI->exec.handler[i].pHyp == NULL &&
         pAImain->exec.handler[i].pHyp != NULL ) {

      /*gHyp_util_debug("transfering handler %d from %x to %x",
	i, pAImain, pAI ) ;
      */

      pAI->exec.handler[i].pHyp = pAImain->exec.handler[i].pHyp ;
      pAImain->exec.handler[i].pHyp = NULL ;

      pAI->exec.handler[i].hypIndex = pAImain->exec.handler[i].hypIndex ;
      pAImain->exec.handler[i].hypIndex = -1 ;
    }

  }

  /* Transfer the signal info */
  pAI->exec.eventType = pAImain->exec.eventType ;
  pAI->exec.eventTime  = pAImain->exec.eventTime ;   
  pAI->exec.createTime  = pAImain->exec.createTime;
  pAI->exec.deathTime  = pAImain->exec.deathTime ;
  pAI->exec.timeOutTime  = pAImain->exec.timeOutTime ;
  pAI->exec.alarmTime  = pAImain->exec.alarmTime ;
  pAI->exec.beatTime  = pAImain->exec.beatTime ;
  pAI->exec.wakeTime  = pAImain->exec.wakeTime ;
}


void gHyp_instance_init ( sInstance *pAI, 
			  sConcept *pConcept, 
			  char *targetId,
			  char *targetPath )
{
  /* Description:
   *
   *	Initialize an instance object.
   *
   */
  sData
    *pMethodVariable ;

  char
    message[MIN_MESSAGE_SIZE+1] ;

  sReply
    *pReply ;

  sMethod
    *pMethod ;
 
  int 
    i ;

  sHyp
    *pHyp ;

  sData
    *pMethodData ;

  /* Initialize the state */
  pAI->exec.state = STATE_PARSE ;

  /*if ( guRunFlags & RUN_COMPILEONLY ) return ;*/

  pAI->exec.pConcept = pConcept ;

  /* Initialize life and death times */
  pAI->exec.createTime = gsCurTime  ;
  pAI->exec.deathTime = pAI->exec.createTime + MAX_LIFETIME_SECONDS ;

  /* Heartbeat interval */
  pAI->exec.beatTime = gsCurTime + HEARTBEAT_INTERVAL ;

  /* Default event is heartbeat */
  pAI->exec.eventTime = pAI->exec.beatTime ;
  pAI->exec.eventType =  EVENT_HEARTBEAT ;
  
  /* Initialize alarm and sleep handling */
  pAI->exec.alarmTime = 0 ;
  pAI->exec.wakeTime = 0 ;
  
  /* Initialize timeOut handling */
  pAI->exec.timeOut = DEFAULT_TIMEOUT ;
  pAI->exec.timeOutTime = 0 ;
  
  /* Initialize all handlers */
  for ( i=0; i<NUM_HANDLER_TYPES; i++ ) {
    pAI->exec.handler[i].pHyp = NULL ;
    pAI->exec.handler[i].hypIndex = -1 ;
  }

  /* Create the execution frame and the _main_ variable storage area. */
  pAI->exec.pFrame = gHyp_frame_new () ;
  pMethodData = gHyp_data_new ( "_main_" ) ;
  gHyp_data_setHashed ( pMethodData, TRUE ) ;
  gHyp_frame_setMethodData ( pAI->exec.pFrame, pMethodData ) ;

  /* The _main_ method variable holds the _main_  hyp area. and is stored
   * in the _main_ data variable.
   */
  pMethodVariable = gHyp_frame_createRootVariable ( pAI->exec.pFrame, "_main_" ) ;

  /* Create the area for the main and temporary code areas */
  giLineCount = 0 ;
  pHyp = gHyp_hyp_new ( "_main_" ) ;
  pMethod = gHyp_method_new () ;
  gHyp_method_setHyp ( pMethod, pHyp ) ;
  gHyp_data_setObject ( pMethodVariable, 
			(void*) pMethod, 
			DATA_OBJECT_METHOD, 
			(void(*) (void*)) gHyp_method_delete ) ;
  gHyp_frame_setMethodVariable ( pAI->exec.pFrame, pMethodVariable ) ;
  gHyp_frame_setHyp ( pAI->exec.pFrame, pHyp ) ;
  gHyp_frame_setHypIndex ( pAI->exec.pFrame, 0 ) ;  

  /* Set the method arguments */
  gHyp_method_setArgs ( pMethod, gHyp_data_new("_argv_") ) ;

  /* Initialize/clear the device areas */
  pAI->device.mlb = -1 ;
  pAI->device.xml = FALSE ; 
  pAI->device.mhp = SECS_MINIMUM_HSMS_PORT ;
  pAI->device.pHash = gHyp_hash_new ( DEVICE_HASH_TABLE_SIZE ) ;
  pAI->device.numDevices = 0 ;
  for ( i=0; i<MAX_DEVICES; i++ ) {
    pAI->device.id[i] = NULL_DEVICEID ;
    pAI->device.fd[i] = INVALID_SOCKET ;
    pAI->device.rBit[i] = FALSE ;
    pAI->device.pSecs2[i] = NULL ;
  }

  /* Initialize message variables. */
  pAI->msg.outgoingDepth = 0 ;
  pAI->msg.incomingDepth = 0 ;
  pAI->msg.rejectCount = 0 ;

  /* Create array space for the message levels */
  pAI->msg.incomingReply[0] = (sReply*) AllocMemory(sizeof(sReply)*MAX_REPLY_DEPTH);
  assert (  pAI->msg.incomingReply[0] ) ;
  pAI->msg.outgoingReply[0] = (sReply*) AllocMemory(sizeof(sReply)*MAX_REPLY_DEPTH);
  assert  (  pAI->msg.outgoingReply[0] ) ;

  /* Set the array pointers */
  for ( i=1; i<MAX_REPLY_DEPTH; i++ ) {
    pAI->msg.incomingReply[i] = pAI->msg.incomingReply[0] + i ;
    pAI->msg.outgoingReply[i] = pAI->msg.outgoingReply[0] + i ;
  }
    
  /* Initialize the levels */
  for ( i=0; i<MAX_REPLY_DEPTH; i++ ) {

    pReply = pAI->msg.incomingReply[i] ;
    pReply->msg		= NULL ;
    pReply->sender[0]	= '\0' ;
    pReply->method[0]	= '\0' ;
    pReply->transactionID[0]	= '\0' ;
    pReply->frameDepth = -1 ;
    pReply->pStack = NULL ;
    pReply->timeoutTime	= 0 ;

    pReply->secs.id	= NULL_DEVICEID ;
    pReply->secs.stream	= -1 ;
    pReply->secs.function= -1 ;
    pReply->secs.TID = -1 ;
    pReply->secs.SID = -1 ;

    pReply = pAI->msg.outgoingReply[i] ;
    pReply->msg		= NULL ;
    pReply->sender[0]	= '\0' ;
    pReply->method[0]	= '\0' ;
    pReply->transactionID[0]	= '\0' ;
    pReply->frameDepth = -1 ;
    pReply->pStack = NULL ;
    pReply->timeoutTime	= 0 ;

    pReply->secs.id	= NULL_DEVICEID ;
    pReply->secs.stream	= -1 ;
    pReply->secs.function= -1 ;
    pReply->secs.TID    = -1 ;
    pReply->secs.SID    = -1 ;
  } 

  /* Current message is initialized elsewhere */
  pAI->msg.current = NULL ;
  pAI->msg.incoming = NULL ;
  pAI->msg.outgoing = NULL ;

  /* The rq queue is where messages go if they get a requeue() 
   * The qq is the main message queue.
   */
  for ( i=0; i<MAX_QUEUE_DEPTH; i++ ) {
    pAI->msg.qq[i] = NULL ;
    pAI->msg.rq[i] = NULL ;
    pAI->msg.qqSecs[i].id = NULL_DEVICEID ;
    pAI->msg.qqSecs[i].stream = -1 ;
    pAI->msg.qqSecs[i].function = -1 ;
    pAI->msg.qqSecs[i].TID = -1 ;
    pAI->msg.qqSecs[i].SID = -1 ;
    pAI->msg.rqSecs[i].id = NULL_DEVICEID ;
    pAI->msg.rqSecs[i].stream = -1 ;
    pAI->msg.rqSecs[i].function = -1 ;
    pAI->msg.rqSecs[i].TID = -1 ;
    pAI->msg.rqSecs[i].SID = -1 ;
  }

  pAI->msg.startQQ = 0 ;
  pAI->msg.endQQ = MAX_QUEUE_DEPTH ;
  pAI->msg.startRQ = 0 ;
  pAI->msg.endRQ = MAX_QUEUE_DEPTH ;

  /* Initialize application level handler flags  */
  pAI->signal.uERROR = 0 ;
  pAI->signal.uWARNING  = 0 ;
  pAI->signal.uMSG = 0 ;
  pAI->signal.uMSGINTERRUPT = 0 ;
  pAI->signal.uMSGPENDING = 0 ;
  pAI->signal.uCONNECT = 0 ;
  pAI->signal.numCon = 0 ;
  pAI->signal.uMETHODCALL = 0 ;
  pAI->signal.uDEREFERENCE = 0 ;
  pAI->signal.tokenSignal = 0 ;

  /* Establish signal handlers for environment level handlers. */
  pAI->signal.uIO = 0 ;
  pAI->signal.uTERM = 0 ;
  pAI->signal.uHUP = 0 ;
  pAI->signal.numHup = 0 ;
  pAI->signal.uALRM = 0 ;
  pAI->signal.uPIPE = 0 ;
  pAI->signal.numPipe = 0 ;
  pAI->signal.uINT = 0 ;

  /* Load the instance with some predefined variables */
  /*gHyp_util_debug("Setting new msg.targetid = %s",targetId ) ;*/
  strcpy ( pAI->msg.targetId, targetId ) ;
  gHyp_instance_setTargetPath ( pAI, targetPath ) ;
  sprintf (	message, 
		"|%s|event|_main_|%s|%s|%s||STATUS|$ACKNOWLEDGE||DEBUG|0||console|1|||", 
		targetPath, 
		targetPath,
		gHyp_util_random8(),
		gHyp_util_timeStamp ( gsCurTime ) ) ;

  /* Parse the message */
  pAI->msg.incoming = gHyp_aimsg_new() ;
  gHyp_aimsg_initParse ( pAI->msg.incoming, message );
  gHyp_frame_createRootVariable ( pAI->exec.pFrame, "STATUS" ) ;
  gHyp_frame_createRootVariable ( pAI->exec.pFrame, "DEBUG" ) ;
  lHyp_instance_consumeMessage ( pAI, 0 ) ;
  pAI->msg.current = pAI->msg.incoming ;
  pAI->msg.incoming = NULL ;
  pAI->msg.inSecs.id = NULL_DEVICEID ;
  pAI->msg.inSecs.stream = -1 ;
  pAI->msg.inSecs.function = -1 ;
  pAI->msg.inSecs.TID = -1 ;
  pAI->msg.inSecs.SID = -1 ;

  /*gHyp_util_debug("tp = %s, ti = %s", gHyp_instance_getTargetPath ( pAI ),gHyp_instance_getTargetId ( pAI ) ) ;*/

  gHyp_util_logInfo ( "Initialized instance '%s'", targetPath ) ;
  gHyp_hyp_traceReset ( "_main_" ) ;

  return ;
}

void gHyp_instance_delete ( sInstance * pAI )
{
  /* Description:
   *
   *	De-allocate memory for the instance.
   *
   * Arguments:
   *
   *	pAI						[R]
   *	- pointer to instance
   *
   * Return value:
   *
   *	none
   *
   */
   
  int
    i ;

  sHash
    *pHash,
    *pNext ;

  sReply
    *pReply ;

  /* Delete an instance */
  gHyp_util_logInfo("Deleting instance %s",pAI->msg.targetId);

  /* Left over stack variables */
  gHyp_stack_flush ( gHyp_frame_stack ( pAI->exec.pFrame ) ) ;

  /* Free execution area */
  /*gHyp_util_logInfo("deleting frame");*/
  gHyp_frame_delete ( pAI->exec.pFrame ) ;

  /*gHyp_util_logInfo("deleting qq and rq");*/
  for ( i=0; i<MAX_QUEUE_DEPTH; i++ ) {
    if ( pAI->msg.qq[i] ) {
      gHyp_aimsg_delete ( pAI->msg.qq[i] ) ;
      pAI->msg.qq[i] = NULL ;
    }
    if ( pAI->msg.rq[i] ) {
      gHyp_aimsg_delete ( pAI->msg.rq[i] ) ;
      pAI->msg.rq[i] = NULL ;
    }
  }

  /* Free messages */
  /*gHyp_util_logInfo("deleting reply messages");*/
  for ( i=0; i<MAX_REPLY_DEPTH; i++ ) {

    /* For next level */
    pReply = pAI->msg.outgoingReply[i] ;
    if ( pReply->msg )
      gHyp_aimsg_delete ( pReply->msg ) ;
    pReply->msg = NULL ;

    pReply = pAI->msg.incomingReply[i] ;
    if ( pReply->msg )
      gHyp_aimsg_delete ( pReply->msg ) ;
    pReply->msg = NULL ;
  }

  /*gHyp_util_logInfo("deleting reply arrays");*/
  ReleaseMemory ( pAI->msg.incomingReply[0] ) ;
  ReleaseMemory ( pAI->msg.outgoingReply[0] ) ;

  /*gHyp_util_logInfo("deleting current message");*/
  if ( pAI->msg.current ) gHyp_aimsg_delete ( pAI->msg.current ) ; 

  /* Free the secs structures */
  /*gHyp_util_logDebug("Deleting device hash");*/

  for ( i=0; i<DEVICE_HASH_TABLE_SIZE; i++ )
    for ( pHash = (*pAI->device.pHash)[i]; pHash; pHash=pNext ) {
      pNext = gHyp_hash_next ( pHash ) ;
      gHyp_hash_free ( pHash ) ;
    }
  gHyp_hash_delete ( pAI->device.pHash ) ;

  /*gHyp_util_logInfo("deleting secs2");*/
  for ( i=0; i<pAI->device.numDevices; i++ )
    if ( pAI->device.pSecs2[i] ) gHyp_secs2_delete ( pAI->device.pSecs2[i] ) ;

  /*gHyp_util_logInfo("deleting incoming/outgoing");*/
  if ( pAI->msg.outgoing ) gHyp_aimsg_delete ( pAI->msg.outgoing ) ; 
  if ( pAI->msg.incoming ) gHyp_aimsg_delete ( pAI->msg.incoming ) ; 

  /* Free the instance */
  /*gHyp_util_logInfo("deleting instance");*/
  ReleaseMemory ( pAI ) ;

  return ;
}

sFrame *gHyp_instance_frame ( sInstance *pAI ) 
{
  /* Description:
   *
   *	Returns a pointer to the HyperScript frame.
   *
   * Arguments:
   *
   *	pAI						[R]
   *	- pointer to instance
   *
   * Return value:
   *
   *	Returns pointer to sFrame object
   *
   */
  return pAI->exec.pFrame ;
}

int gHyp_instance_nextEvent ( sInstance *pAI )
{
  /* Description:
   *
   *	Calculates and sets the timestamp for the next alarm,
   *	death, or timeout event.
   *
   * Arguments:
   *
   *	pAI						[R]
   *	- pointer to instance
   *
   * Return value:
   *
   *	next event time
   *
   */
   
  /* Determine which event will happen first. */ 
  int	
    farAndAway,
    alarmed,
    death,
    timeOut,
    heartbeat,
    wakeup;
	  
  /* Check the sanity of each time */
  farAndAway = (int) gsCurTime + MAX_LIFETIME_SECONDS + 1 ;

  if ( pAI->exec.alarmTime != 0 ) {
    if ( pAI->exec.alarmTime < gsCurTime )
      pAI->exec.alarmTime = gsCurTime ;
    else if ( pAI->exec.alarmTime > farAndAway )
      pAI->exec.alarmTime = 0 ;
  }

  if ( pAI->exec.deathTime != 0 ) {
    if ( pAI->exec.deathTime < gsCurTime )
      pAI->exec.deathTime = gsCurTime ;
    else if ( pAI->exec.deathTime > farAndAway )
      pAI->exec.deathTime = gsCurTime + MAX_LIFETIME_SECONDS  ;
  }

  if ( pAI->exec.timeOutTime != 0 ) {
    if ( pAI->exec.timeOutTime < gsCurTime )
      pAI->exec.timeOutTime = gsCurTime ;
    else if ( pAI->exec.timeOutTime > farAndAway )
      pAI->exec.timeOutTime = 0 ;
  }

  if ( pAI->exec.beatTime != 0 ) {
    if ( pAI->exec.beatTime < gsCurTime )
      pAI->exec.beatTime = gsCurTime ;
    else if ( pAI->exec.beatTime > farAndAway )
      pAI->exec.beatTime = gsCurTime + HEARTBEAT_INTERVAL ;
  }

  if ( pAI->exec.wakeTime != 0 ) {
    if ( pAI->exec.wakeTime < gsCurTime )
      pAI->exec.wakeTime = gsCurTime ;
    else if ( pAI->exec.wakeTime > farAndAway )
      pAI->exec.wakeTime = 0 ;
  }

  alarmed = (int)   (pAI->exec.alarmTime   ? pAI->exec.alarmTime   : farAndAway );
  death = (int)     (pAI->exec.deathTime   ? pAI->exec.deathTime   : farAndAway );
  timeOut = (int)   (pAI->exec.timeOutTime ? pAI->exec.timeOutTime : farAndAway );
  heartbeat = (int) (pAI->exec.beatTime    ? pAI->exec.beatTime    : farAndAway );
  wakeup = (int)    (pAI->exec.wakeTime    ? pAI->exec.wakeTime    : farAndAway );

  /* CLEAR IT */
  pAI->exec.eventType = 0 ;

  if ( timeOut <= alarmed && timeOut <= death && timeOut <= heartbeat && timeOut <= wakeup ) {
		
    /* The next event is a timeout */
    pAI->exec.eventTime = pAI->exec.timeOutTime ;
    pAI->exec.eventType = EVENT_TIMEOUT ;
  }
  else if ( alarmed <= death && alarmed <= timeOut && alarmed <= heartbeat && alarmed <= wakeup ) {
		
    /* The next event is an alarm */
    pAI->exec.eventTime = pAI->exec.alarmTime ;
    pAI->exec.eventType = EVENT_ALARM ;
  }  
  else if ( death <= alarmed && death <= timeOut && death <= heartbeat && death <= wakeup ) {
      
    /* The next event is death. */
    pAI->exec.eventTime = pAI->exec.deathTime ;
    pAI->exec.eventType = EVENT_DEATH ;

  }
  else if ( heartbeat <= alarmed && heartbeat <= timeOut && heartbeat <= death && heartbeat <= wakeup )  {
  
    /* Heartbeat is the 'next' event. */
    pAI->exec.eventTime = pAI->exec.beatTime ;
    pAI->exec.eventType = EVENT_HEARTBEAT ;
  }
  else if ( wakeup <= heartbeat && wakeup <= alarmed && wakeup <= timeOut && wakeup <= death )  {
 
    /* Wakup from sleep is the 'next' event. */
    pAI->exec.eventTime = pAI->exec.wakeTime ;
    pAI->exec.eventType = EVENT_WAKEUP ;
  }
  
  /*gHyp_util_debug("Next event(s) is %x in %d seconds", pAI->exec.eventType,(pAI->exec.eventTime-gsCurTime));*/
  
 
  return (int) pAI->exec.eventTime ;
}

void gHyp_instance_setBeatTime ( sInstance *pAI, int eventTime )
{
  pAI->exec.beatTime = eventTime ;
}

void gHyp_instance_setWakeTime ( sInstance *pAI, int eventTime )
{
  pAI->exec.wakeTime = eventTime ;
}

time_t gHyp_instance_getWakeTime ( sInstance *pAI )
{
  return pAI->exec.wakeTime  ;
}

void  gHyp_instance_signalAlarm ( sInstance *pAI )
{
  pAI->signal.uALRM = 1 ;
}

void  gHyp_instance_signalInterrupt ( sInstance *pAI )
{
  pAI->signal.uINT = 1 ;
}

void  gHyp_instance_signalTerm ( sInstance *pAI )
{
  pAI->signal.uTERM = 1 ;
}

void  gHyp_instance_signalMsg ( sInstance *pAI )
{
  pAI->signal.uMSG = 1 ;
}

sLOGICAL gHyp_instance_isSignal ( sInstance *pAI )
{
  /* If any one of the following signals has occured,
   * then the select() timeout will be zero.
   */
  return ( pAI->signal.uMSG  == 1 ||
	   pAI->signal.uALRM == 1 ||
	   pAI->signal.uTERM == 1 ||
	   pAI->signal.uINT  == 1 ||
	   pAI->signal.uPIPE == 1 ||
	   pAI->signal.uHUP  == 1 ||
	   pAI->signal.uCONNECT == 1 ) ;
}

void  gHyp_instance_signalPipe ( sInstance *pAI, int sigarg, int sigarg2, int sigarg3 )
{
  int n = pAI->signal.numPipe ;
  if ( n < MAX_SIGNALS ) {
    pAI->signal.uPIPE = 1 ;
    pAI->signal.pipeArg[n] = sigarg ;
    pAI->signal.pipeArg2[n] = sigarg2 ;
    pAI->signal.pipeArg3[n] = sigarg3 ;
    pAI->signal.numPipe++ ;
  }
}

void  gHyp_instance_signalHangup ( sInstance *pAI, int sigarg, int sigarg2, int sigarg3 )
{
  int n = pAI->signal.numHup ;
  if ( n < MAX_SIGNALS ) {
    /*gHyp_util_debug("New HUP at [%d] s=%d x=%d id=%d",n,sigarg,sigarg2,sigarg3) ;*/
    pAI->signal.uHUP = 1 ;
    pAI->signal.hupArg[n] = sigarg ;
    pAI->signal.hupArg2[n] = sigarg2 ;
    pAI->signal.hupArg3[n] = sigarg3 ;
    pAI->signal.numHup++ ;
  }
}

void  gHyp_instance_signalConnect ( sInstance *pAI, int sigarg, int sigarg2, int sigarg3 )
{
  int n = pAI->signal.numCon ;
  if ( n < MAX_SIGNALS ) {
    pAI->signal.uCONNECT = 1 ;
    /*gHyp_util_debug("New CON at [%d] s=%d x=%d id=%d",n,sigarg,sigarg2,sigarg3) ;*/
    pAI->signal.conArg[n]  = sigarg ;
    pAI->signal.conArg2[n] = sigarg2 ;
    pAI->signal.conArg3[n] = sigarg3 ;
    pAI->signal.numCon++ ;
  }
}

int gHyp_instance_read ( sInstance * pAI, sLOGICAL queueOnly  )
{
  /* Description:
   *
   *	Read the next message.
   *
   *	Upon entering this routine, the HyperScript program can be in one 
   *	of 2 possible states:
   *
   *	1. QUERY state.
   *	   - a query message has been sent and HyperScript is waiting
   *	     for a REPLY message
   *
   *	2. IDLE state.
   *	   - the HyperScript program is in an idle state, waiting for
   *	     and incoming QUERY or EVENT message that it can service
   *
   *	There are 3 general types of messages involed:
   *	1. Unsolicited incoming event messages.
   *	   - probably that calls a method.  
   *	   - increments the incoming message depth
   *	2. Unsolicited incoming query messages.
   *	   - like event messages, plus
   *	   - pre-initialize reply at the incoming depth
   *	3. Solicited incoming reply messages. (when state == QUERY)
   *	   - match to current message depth decements it.
   *	   - match to lower message depth is noop.
   *
   *	The tyes of inputs are:
   *	- messages from the fifo
   *	- messages from the serial ports (secs 1)
   *	- messages from tcp/ip channels (hsms)
   *
   * Arguments:
   *
   *	pConcept						[R]
   *	- pointer to instance
   *
   *    queueOnly
   *    - only from queues, do not block using readSelect
   *
   * Return value:
   *
   *	Returns
   *           -2 : COND_FATAL: fatal for all
   *	       -1 : COND_ERROR: condition for this instance.
   *		0 : COND_SILENT: No activity for this instance, another instance needs to be run
   *	        1 : COND_NORMAL: Successful read, or signal
   */

  int
    cond ;

  sConcept
    *pConcept = pAI->exec.pConcept ;
  
  /* In order to receive asynchronous events, the program must
   * first either be in interactive mode or already have a reader (parent) open.
   */
  if ( !gHyp_concept_returnToStdIn ( pConcept ) &&
       !gHyp_concept_openReader ( pConcept ) ) {
    gHyp_util_logError("Failed to connect to inbox channel");
    return COND_ERROR ;
  }

  /* We will loop until we get something to work on or nothing to work on */
  cond = COND_SILENT ;
  while ( TRUE ) {

    /* With a signal, we exit to deal with it, and then come back
     * to get more events.
     */
    if ( cond == COND_SILENT ) {
      cond = gHyp_instance_readSignals ( pAI )  ;
      if ( cond == COND_NORMAL ) break ;
    }

    /* No signals (yet).
     *
     * Check for queued (or re-queued) (event, query, or reply) messages. 
     * A queued message is one that has already been read from a socket or fifo 
     * and is ready to process.
     *
     * The message is put in 'incoming' and cond is set to COND_NORMAL.
     */
    while ( (cond = gHyp_instance_readQueue ( pAI )) == COND_NORMAL ) {
      /* Got a message, an event, query, or reply */
      cond = gHyp_instance_readProcess ( pAI, pAI->exec.state ) ;
      if ( cond == COND_NORMAL ) {
        /* Got an interrupting message. Service it.  */
	break ;
      }
      else if ( cond == COND_SILENT ) {
        /* Got a reply message */
	cond = gHyp_instance_readReply ( pAI ) ;
	if ( cond == COND_NORMAL ) {
          if ( pAI->exec.state == STATE_QUERY ) {
	    /* Current reply is satisfied, we can continue parsing */
            pAI->exec.state = STATE_PARSE ;
            gHyp_frame_setState ( pAI->exec.pFrame, STATE_PARSE ) ;
	    /*gHyp_instance_pushLocalSTATUS ( pAI, gHyp_frame_stack (pAI->exec.pFrame ) ) ;*/
	    break ;
	  }
        }
        /* Look for more messages */
      }
    }

    /* We exit if either a message is pending or the reply was satisfied at the correct point */
    if ( cond == COND_NORMAL ) break ;

    if ( !queueOnly ) {

      if ( cond == COND_SILENT ) {

	/* Nothing happening from queue or reply queue, 
	 * So, try reading from the devices, sockets, and ports.
         *
         * This function only returns COND_NORMAL when something
         * has been put in the message queue.
         *
         * Otherwise, it returns COND_SILENT or COND_ERROR.
         *
         * For incoming connection requests or hangups. a connect 
         * or hangup signal will be set, but COND_SILENT will be
         * returned.
         *
         * For reads off of a device, socket, or port, the
         * message may be routed.
         *
         */
        cond = gHyp_concept_readSelect ( pConcept ) ;

        if ( cond < 0 ) {
          /* Something bad has happened. See if a signal was generated */
          gHyp_instance_readSignals ( pAI )  ;
  	  break ;
        }
      }
    }

    /* If nothing going on at the end of the loop, then exit */
    if ( cond == COND_SILENT ) break ;
    
  }

  /* Check for signals once more */
  if ( cond == COND_SILENT ) {
    cond = gHyp_instance_readSignals ( pAI )  ;
  }

  return cond ; 
}

int gHyp_instance_readReply ( sInstance *pAI )
{
  int
    n = pAI->msg.incomingDepth-1 ;

  if ( pAI->msg.incoming ) {
    /*gHyp_util_debug("Message %s available",gHyp_aimsg_method(pAI->msg.incoming));*/
    return COND_SILENT ;
  }

  if ( pAI->msg.incomingDepth > 0 &&
       pAI->msg.incomingReply[n]->msg != NULL &&
       pAI->msg.incomingReply[n]->frameDepth == gHyp_frame_depth( pAI->exec.pFrame ) ) {
    
    if ( guDebugFlags & DEBUG_DIAGNOSTICS )
      gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
			   "diag : Consuming incoming reply for %s at depth %d and frame %d",
			   gHyp_aimsg_method(pAI->msg.incomingReply[n]->msg),
			   n, pAI->msg.incomingReply[n]->frameDepth);


    pAI->msg.incoming = pAI->msg.incomingReply[n]->msg; 
    pAI->msg.inSecs = pAI->msg.incomingReply[n]->secs ;

    /* Consume the REPLY message and resume execution after the query */
    lHyp_instance_consumeMessage ( pAI, 0 ) ;
    gHyp_instance_pushLocalSTATUS ( pAI, pAI->msg.incomingReply[n]->pStack ) ;

    pAI->msg.incomingReply[n]->msg = NULL ;
    pAI->msg.incomingReply[n]->frameDepth = -1 ;
    pAI->msg.incomingReply[n]->pStack = NULL ;
    pAI->msg.incomingReply[n]->secs.id = NULL_DEVICEID ;
    pAI->msg.incomingReply[n]->secs.stream = -1 ;
    pAI->msg.incomingReply[n]->secs.function = -1 ;
    pAI->msg.incomingReply[n]->secs.TID = -1 ;
    pAI->msg.incomingReply[n]->secs.SID = -1 ;
        
    gHyp_aimsg_delete ( pAI->msg.incoming ) ;
    pAI->msg.incoming = NULL ;

    gHyp_instance_decIncomingDepth ( pAI ) ;
    
    /* RETURN 1, SATISFYING THE QUERY */
    return COND_NORMAL ;
  }
  else
    return COND_SILENT ;
}

int gHyp_instance_readQueue ( sInstance* pAI )
{
  int
    n ;

  if ( guDebugFlags & DEBUG_DIAGNOSTICS )
    gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
	"Looking for next queued message at [%d]", pAI->msg.startQQ);  

  /* Clear the flag to begin with */	
  pAI->signal.uMSG = 0 ;

  if ( pAI->msg.incoming != NULL ) {

    /* Has to be one just pulled off the queue, 
     * we are coming back for it after servicing the interrupt handler
     */
    if ( !pAI->signal.uMSGPENDING ) {
      if ( guDebugFlags & DEBUG_DIAGNOSTICS )
          gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
          "Processing message '%s'",gHyp_aimsg_method ( pAI->msg.incoming) ) ;
      gHyp_instance_signalMsg( pAI ) ;
      return COND_NORMAL ;
    }
    else {
      if ( guDebugFlags & DEBUG_DIAGNOSTICS )
         gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
          "Pending message '%s'",gHyp_aimsg_method ( pAI->msg.incoming) ) ;
      return COND_SILENT ;
    }
  }

  if ( pAI->msg.qq[pAI->msg.startQQ] != NULL ) {

    /* There are queued messages */

    n = pAI->msg.startQQ ;

    if ( pAI->msg.incoming ) {
      /*gHyp_util_debug("Deleting message %s",gHyp_aimsg_method(pAI->msg.incoming ));*/
      gHyp_aimsg_delete ( pAI->msg.incoming ) ;
      pAI->msg.incoming = NULL;
      pAI->signal.uMSGINTERRUPT = 0 ;
      pAI->signal.uMSGPENDING = 0 ;
    }
    pAI->msg.incoming = pAI->msg.qq[n] ;
    pAI->msg.qq[n] = NULL ;

    pAI->msg.inSecs = pAI->msg.qqSecs[n] ;  
    pAI->msg.qqSecs[n].id  = NULL_DEVICEID ;
    pAI->msg.qqSecs[n].stream = -1 ;
    pAI->msg.qqSecs[n].function = -1 ;
    pAI->msg.qqSecs[n].TID = -1 ;
    pAI->msg.qqSecs[n].SID = -1 ; 

    if ( guDebugFlags & DEBUG_DIAGNOSTICS ) {
      if ( gHyp_aimsg_isReply ( pAI->msg.incoming ) )
	gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
			   "Found queued reply message(%d) '%s' for %s",
			     pAI->msg.startQQ,
			     gHyp_aimsg_method ( pAI->msg.incoming),
			     pAI->msg.targetId ) ;
      else
	gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
			     "Found queued event or query message(%d) '%s' for %s",
			     pAI->msg.startQQ,
			     gHyp_aimsg_method ( pAI->msg.incoming),
			     pAI->msg.targetId ) ;
    }

    /* Advance start of queue */
    pAI->msg.startQQ++ ;
    if ( pAI->msg.startQQ >= MAX_QUEUE_DEPTH ) pAI->msg.startQQ = 0 ;

    /* Make sure any read has a timeout of zero,
     * in which case this requeued message will be processed.
     */
    gHyp_instance_signalMsg( pAI ) ;
    return COND_NORMAL ;
  }

  /* Check for requeued messages, but only if we are not in a QUERY state */
  if ( pAI->exec.state != STATE_QUERY &&
       pAI->msg.rq[pAI->msg.startRQ] != NULL ) {
    
    n = pAI->msg.startRQ ;
    if ( pAI->msg.incoming ) {
      /*gHyp_util_debug("Deleting message %s",gHyp_aimsg_method(pAI->msg.incoming ));*/
      gHyp_aimsg_delete ( pAI->msg.incoming ) ;
      pAI->msg.incoming = NULL;
      pAI->signal.uMSGINTERRUPT = 0 ;
      pAI->signal.uMSGPENDING = 0 ;
    }
    pAI->msg.incoming = pAI->msg.rq[n] ;
    pAI->msg.rq[n] = NULL ;
    
    /* Erase */
    pAI->msg.inSecs = pAI->msg.rqSecs[n] ;
    pAI->msg.rqSecs[n].id = NULL_DEVICEID ;
    pAI->msg.rqSecs[n].stream = -1 ;
    pAI->msg.rqSecs[n].function = -1 ;
    pAI->msg.rqSecs[n].TID = -1 ;
    pAI->msg.rqSecs[n].SID = -1 ;

    gHyp_util_logInfo ( "Found re-queued message(%d) '%s'",
			pAI->msg.startRQ,
			gHyp_aimsg_method ( pAI->msg.incoming) ) ;
    
    /* Advance start of queue */
    pAI->msg.startRQ++ ;

    if ( pAI->msg.startRQ >= MAX_QUEUE_DEPTH ) pAI->msg.startRQ = 0 ;

    /* Make sure any read has a timeout of zero,
     * in which case this requeued message will be processed.
     */
    gHyp_instance_signalMsg( pAI ) ;
    return COND_NORMAL ;
  }

  
  return COND_SILENT ;
}

int gHyp_instance_readSignals ( sInstance *pAI ) 
{
  char
    status[VALUE_SIZE+1] ;
  int 
    n ;
  
  /* Check for alarm */
  if ( pAI->signal.uALRM ) {
    gHyp_instance_warning ( pAI, STATUS_ALARM, "Timer signal:%s: instance = %s",
			    gHyp_util_timeStamp ( gsCurTime ),
			    pAI->msg.targetId );
    return COND_NORMAL ;
  }
  else if ( pAI->signal.uINT ) {
    /* SIGINT was received. */
    gHyp_instance_warning ( pAI, STATUS_INTERRUPT, "Interrupt signal:%s: instance = %s",
			    gHyp_util_timeStamp ( gsCurTime ),
			    pAI->msg.targetId );
    gHyp_instance_setStatus ( pAI,STATUS_INTERRUPT); 
    return COND_NORMAL ;
  }
  else if ( pAI->signal.uERROR ) {
    /* An ERROR occurred, the STATUS is already set. */
    return COND_NORMAL ;
  }
  else if ( pAI->signal.uPIPE ) {
    /* SIGPIPE was received. */
    n = pAI->signal.numPipe-1;
    gHyp_instance_warning ( 	pAI, STATUS_PIPE,
				"Pipe signal:%s: socket = %u, port = %u, id = %u, instance = %s",
			        gHyp_util_timeStamp ( gsCurTime ),
				pAI->signal.pipeArg[n],
				pAI->signal.pipeArg2[n],
				pAI->signal.pipeArg3[n],
			        pAI->msg.targetId ) ;
		
    sprintf ( status, "%s %d %d %d", STATUS_PIPE, pAI->signal.pipeArg[n], pAI->signal.pipeArg2[n], pAI->signal.pipeArg3[n]  ) ; 
    gHyp_instance_setStatus ( pAI,status);
    return COND_NORMAL ;
  }
  else if ( pAI->signal.uHUP ) {
    /* SIGHUP was received. */
    n = pAI->signal.numHup-1;
    /*
    *gHyp_util_debug("Info HUP at [%d] s=%d x=%d id=%d",n,pAI->signal.hupArg[n], 
	      pAI->signal.hupArg2[n], 
	      pAI->signal.hupArg3[n] ) ;
    */
    gHyp_instance_warning (	pAI, STATUS_HANGUP, 
				"Hangup signal:%s: socket = %u, port = %u, id = %u, instance = %s",
			        gHyp_util_timeStamp ( gsCurTime ),
				pAI->signal.hupArg[n],
				pAI->signal.hupArg2[n],
				pAI->signal.hupArg3[n],
			        pAI->msg.targetId ) ;
    sprintf ( status, "%s %d %d %d", STATUS_HANGUP, pAI->signal.hupArg[n], pAI->signal.hupArg2[n], pAI->signal.hupArg3[n]  ) ; 
    gHyp_instance_setStatus ( pAI,status);
    return COND_NORMAL ;
  }
  else if ( pAI->signal.uCONNECT ) {
    /* A connect signal.  */
    n = pAI->signal.numCon-1;
    /*
    *gHyp_util_debug("Info CON at [%d] s=%d x=%d id=%d",n,pAI->signal.conArg[n], 
	      pAI->signal.conArg2[n], 
	      pAI->signal.conArg3[n] ) ;
    */
    gHyp_instance_warning (	pAI, STATUS_CONNECT,
				"Connect signal:%s: socket = %u, port = %u, id = %u, instance = %s",
			        gHyp_util_timeStamp ( gsCurTime ),
				pAI->signal.conArg[n],
				pAI->signal.conArg2[n],
				pAI->signal.conArg3[n],
			        pAI->msg.targetId ) ;
    sprintf ( status, "%s %d %d %d", STATUS_CONNECT, pAI->signal.conArg[n], pAI->signal.conArg2[n], pAI->signal.conArg3[n] ) ; 
    gHyp_instance_setStatus ( pAI,status);
    return COND_NORMAL ;
  }
  return COND_SILENT ;
}

int gHyp_instance_readProcess ( sInstance *pAI, sBYTE state )
{
  int	       
    i ;

  sLOGICAL
    isSenderSelf,
    isReplyMsg,
    isF0,isS9,
    matchedReply,
    currentQuery,
    okInstance=FALSE,
    okWildCard,
    okTID=FALSE,
    okMethod=FALSE,
    isSECSmsg ;

  char
    message[MIN_MESSAGE_SIZE+1],
    targetPath[TARGET_SIZE+1],
    senderPath[SENDER_SIZE+1],
    senderAddress[SENDER_SIZE+1],
    targetAddress[TARGET_SIZE+1],
    *pTargetInstance,
    *pTargetConcept,
    *pTransactionID,
    *pMethodStr,
    *pSenderInstance,
    *pSenderConcept ;

  sData
    *pMethodVariable ;

  sMethod
    *pMethod = NULL ;

  if ( pAI->msg.incoming == NULL ) return COND_SILENT ;

    /* Clear the flag that there's a message in pAI->msg.incoming */	
  pAI->signal.uMSG = 0 ;

  /* The incoming message header items for target and sender have not been altered in
   * any way.  
   * The incoming message has already been screened for receipt by this node only.
   *
   * The component values must be extracted the implied paths be determined.
   */

  gHyp_aimsg_getTargetPath ( pAI->msg.incoming, targetPath ) ;

  pTargetInstance = gHyp_aimsg_targetInstance ( pAI->msg.incoming ) ;
  pTargetConcept = gHyp_aimsg_targetConcept ( pAI->msg.incoming ) ;

  /*gHyp_util_debug("TARGET COMPARE %s == %s",pTargetInstance, pTargetConcept) ;*/

  if ( strcmp ( pTargetInstance, pTargetConcept ) != 0 ) {

    /* Instance target */
    /*gHyp_util_debug("INSTANCE COMPARE %s == %s",pAI->msg.targetPath, targetPath) ;*/
    if ( strcmp ( pAI->msg.targetPath, targetPath ) != 0 ) {
      gHyp_util_logWarning ( 
        "Message ignored, implied target of message '%s' is incorrect - should be '%s'",
        targetPath,
        pAI->msg.targetPath ) ;
      gHyp_aimsg_delete ( pAI->msg.incoming ) ;
      pAI->msg.incoming = NULL ;
      return COND_SILENT ;
    }
  }
  else {
    /* Concept target */
    /*gHyp_util_debug("PARENT COMPARE %s == %s",gHyp_aimsg_targetParent ( pAI->msg.incoming ), gzParent ) ;*/
    if ( strcmp ( gHyp_aimsg_targetParent ( pAI->msg.incoming ), gzParent ) != 0 ) {
      gHyp_util_logWarning ( 
        "Message ignored, implied target of message '%s%s' is incorrect - should be '%s%s'",
        pTargetConcept,
	targetPath,
        pTargetConcept,
	gzParent ) ;
      gHyp_aimsg_delete ( pAI->msg.incoming ) ;
      pAI->msg.incoming = NULL ;
      return COND_SILENT ;
    }
  }

  
  /* Get sender of message */
  gHyp_aimsg_getSenderPath ( pAI->msg.incoming, senderPath ) ;

  isSenderSelf = ( strcmp (  targetPath, senderPath ) == 0 ) ;
  pMethodStr = gHyp_aimsg_method ( pAI->msg.incoming ) ;
  pSenderInstance = gHyp_aimsg_senderInstance ( pAI->msg.incoming ) ;
  pSenderConcept = gHyp_aimsg_senderConcept ( pAI->msg.incoming ) ;
  pTransactionID = gHyp_aimsg_getTID ( pAI->msg.incoming ) ;
  isSECSmsg = ( pAI->msg.inSecs.id != NULL_DEVICEID ) ;
  isReplyMsg = gHyp_aimsg_isReply ( pAI->msg.incoming ) ;


  /* Check for a SnF0 or S9F* abort transaction. 
   * Make them reply messages so we can match them by TID to a
   * primary message.
   */
  isS9 = ( isSECSmsg && pAI->msg.inSecs.stream == 9 ) ; 
  isF0 = ( isSECSmsg && pAI->msg.inSecs.function == 0); 
  if ( isS9 || isF0 ) isReplyMsg = TRUE ;

  if ( guDebugFlags & DEBUG_PROTOCOL )
    gHyp_util_logDebug (FRAME_DEPTH_NULL, DEBUG_PROTOCOL,
			"Target=%s, Sender=%s, Method=%s, TID=%s, isReply=%d, isSECS=%d",
			targetPath,
			senderPath, 
			pMethodStr,
			pTransactionID,
			isReplyMsg,
			isSECSmsg ) ;
  
  if ( isReplyMsg ) {
    
    /* A REPLY message was received.  Was is expected? */
    
    /* Search for the matching query */
    matchedReply = FALSE ;
    currentQuery = TRUE ;
    for ( i=pAI->msg.incomingDepth-1; i>=0; i-- ) {
      
      if ( strcmp ( pSenderInstance, pSenderConcept ) != 0 ) {
	/* Instance sender */
        okInstance = ( strcmp ( pAI->msg.incomingReply[i]->sender, senderPath ) == 0 ) ;
      }
      else {
        okInstance = ( strcmp ( gHyp_aimsg_senderParent ( pAI->msg.incoming ), gzParent ) == 0 ) ;
      }

      /*okInstance = ( strcmp ( senderPath, pAI->msg.incomingReply[i]->sender ) == 0 ) ;*/
      /*okParent = ( strcmp ( parentPath,  senderPath ) == 0 ) ;*/

      okWildCard = !okInstance ? 
			gHyp_util_match( 
			  senderPath, 
			  pAI->msg.incomingReply[i]->sender ) : FALSE ;

      if ( strlen ( pTransactionID ) == 0 )
	okTID = TRUE ;
      else
        okTID = ( strcmp ( pTransactionID, 
	  		 pAI->msg.incomingReply[i]->transactionID ) == 0) ;
      
      if ( isF0 || isS9 )
	okMethod = TRUE ;
      
      else
	okMethod = ( strcmp ( pMethodStr,
			      pAI->msg.incomingReply[i]->method ) == 0) ;
      
      if ( guDebugFlags & DEBUG_DIAGNOSTICS )
	gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
                            "diag: Matching %s=%s, %s=%s, %s=%s at depth %d",
			     senderPath, pAI->msg.incomingReply[i]->sender,
			     pMethodStr,pAI->msg.incomingReply[i]->method,
			     pTransactionID,pAI->msg.incomingReply[i]->transactionID,
			     i ) ;
      
      if ( okTID && okMethod && ( okInstance || okWildCard ) ) {
	
	/* Got a matching REPLY message for an outstanding QUERY.
	 * Check that the method matched as well.
	 */
	matchedReply = TRUE ;
	
	gHyp_util_logInfo ( "Matched TID=%s, Method=%s, Depth=%d, Sender=%s",
			    pTransactionID,
			    pMethodStr,
			    i,
			    pAI->msg.incomingReply[i]->sender ) ;
	
	if ( guDebugFlags & DEBUG_DIAGNOSTICS ) {
	  if ( currentQuery )
	    gHyp_util_logDebug ( 
	      FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
	      "diag : Matched current reply %s at incoming depth %u",
	      pMethodStr, i);
	  else
	    gHyp_util_logDebug ( 
	      FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
	      "diag : Matched lower reply %s at incoming depth %u",
	      pMethodStr, i);
	}
	
        /* Cancel timeout. Determine next critical event. */
        gHyp_instance_cancelTimeOut ( pAI, i ) ;

	if ( guDebugFlags & DEBUG_DIAGNOSTICS )
	  gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
			    "Matched %s=%s, %s=%s, %s=%s",
			     senderPath, pAI->msg.incomingReply[i]->sender,
			     pMethodStr,pAI->msg.incomingReply[i]->method,
			     pTransactionID,pAI->msg.incomingReply[i]->transactionID ) ;
	
	/* Save the incoming reply message at the correct
	 * depth.
	 */
	if ( pAI->msg.incomingReply[i]->msg ) 
	  gHyp_aimsg_delete (pAI->msg.incomingReply[i]->msg);
	
	pAI->msg.incomingReply[i]->msg = pAI->msg.incoming ;
        pAI->msg.incomingReply[i]->secs = pAI->msg.inSecs ;

	pAI->msg.incoming = NULL ;
	pAI->msg.inSecs.id = NULL_DEVICEID ;
	pAI->msg.inSecs.stream = -1 ;
	pAI->msg.inSecs.function = -1 ;
	pAI->msg.inSecs.TID = -1 ;
	pAI->msg.inSecs.SID = -1 ;

	break ;
      }
      currentQuery = FALSE ;
    }
 
    if ( !matchedReply ) {
      if ( !isS9 ) {
	gHyp_util_logWarning ( "Reply '%s' message ignored, no match, reason = %s",
				pMethodStr,
                                (!okTID?"TID mismatch":(!okMethod?"Method mismatch":(!okInstance?"Instance":"Unknown?"))) ) ;
        gHyp_aimsg_delete ( pAI->msg.incoming ) ;
        pAI->msg.incoming = NULL ;
	/* To do: Send S9F3 (stream mismatch) or S9F5 (function mismatch) */
      }
      else {
	/*gHyp_util_debug("Not a reply message");*/
	isReplyMsg = FALSE ;
      }
    }
  }

  if ( !isReplyMsg ) {
    
    /* EVENT or QUERY message */
    
    /* Can the message invoke a method? */
    pMethodVariable = gHyp_frame_findMethodVariable ( pAI->exec.pFrame,
						      pMethodStr,
						      pAI ) ;
    if ( pMethodVariable )
      pMethod = (sMethod*) gHyp_data_getObject ( pMethodVariable ) ;

    /* The method must be enabled */
    if ( !pMethodVariable || !pMethod || !gHyp_method_isEnabled(pMethod) ) { 
      
      /* REJECT THE MESSAGE - SEND THE SENDER A MESSAGE REJECTING IT */
      /*gHyp_util_debug("Message %s rejected, %d times",pMethodStr,pAI->msg.rejectCount);*/

      /* Don't reject back to ourselves */
      if ( !isSenderSelf && !isSECSmsg ) {
	
	/* If the incoming msg is not a reject MESSAGE, restore count*/
	if ( strcmp ( pMethodStr, "MESSAGE" ) != 0 ) pAI->msg.rejectCount = 0 ;
	
	/* As long as the # rejects sent is < 2, send back another one.*/
	if ( pAI->msg.rejectCount < 2 ) {
	  
	  /* Method not enabled - send event message back to sender */
	  gHyp_aimsg_getSenderAddress ( pAI->msg.incoming, senderAddress ) ;
	  gHyp_aimsg_getTargetAddress ( pAI->msg.incoming, targetAddress ) ;

	  sprintf ( message, 
		    "|%s|event|MESSAGE|%s|%s|%s||TEXT|%s|%s|||",
		    senderAddress,
		    targetAddress,
		    gHyp_util_random8(),
		    gHyp_util_timeStamp ( gsCurTime ),
		    "%MESSAGE rejected, not found or not enabled for ",
		    pMethodStr) ;
	  
	  /* Prevent infinite looping between HyperScripts, which are
	   * both rejecting MESSAGE event messages. Allow only 3
	   * MESSAGE method rejects in a row.
	   */	    

	  pAI->msg.rejectCount++ ;
	  gHyp_concept_route ( pAI->exec.pConcept, message ) ;
	}
      }
      if ( pAI->msg.incoming ) {
        /*gHyp_util_debug("Deleting message %s",gHyp_aimsg_method(pAI->msg.incoming ));*/
        gHyp_aimsg_delete ( pAI->msg.incoming ) ;
        pAI->msg.incoming = NULL;
        pAI->signal.uMSGINTERRUPT = 0 ;
        pAI->signal.uMSGPENDING = 0 ;
      }
    }
    else {
      
      /* Message can invoke a method */

      /* Flag there's a message in pAI->msg.incoming */	
      pAI->signal.uMSGINTERRUPT = 1 ;
      pAI->signal.uMSGPENDING = 1 ;

      if ( state == STATE_QUERY ) {
	/* An incoming message has interrupted a QUERY state. 
	 * Make a noise about it.
	 */
	gHyp_instance_warning ( pAI, STATUS_MESSAGE, 
				"Message %s interrupted query at incoming depth %d",
				pMethodStr, pAI->msg.incomingDepth ) ;
      } 
      else if ( state == STATE_SLEEP ) {
	/* An incoming message has interrupted a SLEEP state. 
	 * Make a noise about it.
	 */
	gHyp_instance_warning ( pAI, STATUS_MESSAGE, 
				"Message interrupted sleep at incoming depth %d",
				pAI->msg.incomingDepth ) ;
      } 
      else if ( state == STATE_IDLE ) {
	/* An incoming message has interrupted an IDLE  state. 
	 * Make a noise about it - but don't abort if there's no message handler./
	 */
	gHyp_instance_warning ( pAI, STATUS_ACKNOWLEDGE, 
				"Message interrupted IDLE at incoming depth %d",
				pAI->msg.incomingDepth ) ;
	 
      }

      return COND_NORMAL ;
    }
  }
  return COND_SILENT ;
}

void gHyp_instance_incIncomingDepth ( sInstance *pAI )
{
  pAI->msg.incomingDepth++ ;
  
  if ( guDebugFlags & DEBUG_DIAGNOSTICS )
    gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
			 "diag : incoming reply depth++ = %d",
			 pAI->msg.incomingDepth );
  
  if ( pAI->msg.incomingDepth >= MAX_REPLY_DEPTH ) {
    gHyp_util_logError ( "Incoming message overflow error at depth %d", 
			 pAI->msg.incomingDepth ) ;
    longjmp ( gsJmpStack[0], COND_FATAL ) ;
  }   
  pAI->msg.incomingReply[pAI->msg.incomingDepth]->secs.id = NULL_DEVICEID ;
  pAI->msg.incomingReply[pAI->msg.incomingDepth]->secs.stream = -1 ;
  pAI->msg.incomingReply[pAI->msg.incomingDepth]->secs.function = -1 ;
  pAI->msg.incomingReply[pAI->msg.incomingDepth]->secs.TID = -1 ;
  pAI->msg.incomingReply[pAI->msg.incomingDepth]->secs.SID = -1 ;
  pAI->msg.incomingReply[pAI->msg.incomingDepth]->frameDepth = -1 ;
  pAI->msg.incomingReply[pAI->msg.incomingDepth]->pStack = NULL ;

}

void gHyp_instance_decIncomingDepth ( sInstance *pAI )
{
  int n ;
  if ( pAI->msg.incomingDepth > 0 ) pAI->msg.incomingDepth-- ;

  /* If the reply message is still there, throw it away */
  n = pAI->msg.incomingDepth ;
  if ( pAI->msg.incomingReply[n]->msg != NULL ) {
    gHyp_aimsg_delete ( pAI->msg.incomingReply[n]->msg ) ;
    pAI->msg.incomingReply[n]->msg = NULL ;
  }

  if ( guDebugFlags & DEBUG_DIAGNOSTICS )
    gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
			 "diag : incoming reply depth-- = %d",
			 pAI->msg.incomingDepth);
  
}

void gHyp_instance_incOutgoingDepth ( sInstance *pAI )
{
  pAI->msg.outgoingDepth++ ;
  
  if ( guDebugFlags & DEBUG_DIAGNOSTICS )
    gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
			 "diag : outgoing reply depth++ = %d",
			 pAI->msg.outgoingDepth);
    
  if ( pAI->msg.outgoingDepth >= MAX_REPLY_DEPTH ) {
    gHyp_util_logError ( "Outgoing message overflow error at depth %d", 
			 pAI->msg.outgoingDepth ) ;
    longjmp ( gsJmpStack[0], COND_FATAL ) ;
  }  
}

void gHyp_instance_decOutgoingDepth ( sInstance *pAI )
{
  if ( pAI->msg.outgoingDepth > 0 ) pAI->msg.outgoingDepth-- ;
  
  if ( guDebugFlags & DEBUG_DIAGNOSTICS )
    gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
			 "diag : outgoing reply depth-- = %d",
			 pAI->msg.outgoingDepth);
  
}

sLOGICAL gHyp_instance_atCorrectDepth ( sInstance *pAI, char *pMethodStr, int frameDepth )
{
  /* Checks to see if gHyp_instance_replyMessage can be called to decrement the call-method level */

  int outgoingDepth = pAI->msg.outgoingDepth - 1 ;
  char *pMethodStr2 ;
  char *NULLstring = "NULL" ;
  int frameDepth2 ;

  if ( outgoingDepth >= 0 &&
       pAI->msg.outgoingReply[outgoingDepth]->msg != NULL ) {

    /* Get the name of the method stored at the current depth */
    pMethodStr2 = gHyp_aimsg_method ( pAI->msg.outgoingReply[outgoingDepth]->msg ) ;

    /* Get the frame depth saved with the method we are looking to send out a reply. */
    frameDepth2 = pAI->msg.outgoingReply[outgoingDepth]->frameDepth ;
  }
  else {
    /* Make up dummy values */
    pMethodStr2 = NULLstring ;
    frameDepth2 = -1 ;
  } 

  /*gHyp_util_debug("Evaluating depth %d, %s==%s, frame %d==%d", 
  outgoingDepth, pMethodStr, pMethodStr2, frameDepth, frameDepth2 ) ;*/

  /* If we are already at depth 0, then there is no reply to send. */ 
  if ( outgoingDepth < 0 ) return FALSE ;

  /* If the frame depth saved in the reply message is higher than the
   * current frame depth, then its ok to send out a reply for the current depth
   */
  if ( pAI->msg.outgoingReply[outgoingDepth]->frameDepth > frameDepth ) return TRUE ;

  /* If the frame depth saved in the reply message is lower than the
   * current frame depth, then its never ok to send out the reply
   */
  if ( pAI->msg.outgoingReply[outgoingDepth]->frameDepth < frameDepth ) return FALSE ;

  /* The frame depths are equal. */

  if ( pAI->msg.outgoingReply[outgoingDepth]->msg == NULL ) {
    /* The method was an event message.  There is no reply to send.  
     * Return TRUE  so that the outgoingDepth is decremented 
     */
    /*gHyp_util_debug("Event message, no reply", pMethodStr, pMethodStr2 ) ;*/
    return TRUE ;
  }
  else if ( strcmp ( pMethodStr, pMethodStr2 ) == 0 ) {
    /* Methods match.  Ok to send reply */
    /*gHyp_util_debug("Sending reply for %s", pMethodStr2 ) ;*/
    return TRUE ;
  }
  else {
    /* A mistmatch - OK as well, just a different method needs a reply */
    /*gHyp_util_debug("Sending reply for %s from return of %s", pMethodStr2, pMethodStr ) ;*/
    return TRUE ;
  }

  /* Whatever this is, its FALSE */
  return FALSE ;
}

void gHyp_instance_secsReply ( sInstance *pAI, sData *pMethodData )
{
  char
    value[VALUE_SIZE+1] ;

  sData
    *pResult,
    *pData,
    *pGlobalVariable,
    *pLocalVariable ;

  int
    outgoingDepth = pAI->msg.outgoingDepth ;

  if ( pAI->msg.outgoingReply[outgoingDepth]->secs.id != NULL_DEVICEID &&
       pAI->msg.outgoingReply[outgoingDepth]->secs.function%2 == 0 ) {

    /* Primary message needs a secondary reply */
    sprintf ( value, 
	      "S%dF%d", 
	      pAI->msg.outgoingReply[outgoingDepth]->secs.stream, 
	      pAI->msg.outgoingReply[outgoingDepth]->secs.function ) ;

    if ( guDebugFlags & DEBUG_DIAGNOSTICS ) {
      gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
			   "diag : Looking for outgoing secs reply %s at depth %d",
			   value, outgoingDepth ) ;
    }

    pLocalVariable = gHyp_data_getChildByName ( pMethodData, 
						value ) ;
    if ( pLocalVariable ) {

      pGlobalVariable = gHyp_frame_findRootVariable ( pAI->exec.pFrame, value ) ;
      if ( pGlobalVariable && pLocalVariable != pGlobalVariable ) {
      
	/* Assign the data to the global variable */
	gHyp_data_deleteValues ( pGlobalVariable ) ;
	pResult = gHyp_data_new ( NULL ) ;
	gHyp_data_setReference ( pResult, value, pGlobalVariable ) ;
	if ( guDebugFlags & DEBUG_PROTOCOL )
	  gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_PROTOCOL,
			       "Assigning secs local return value '%'s",
			       gHyp_data_print ( pLocalVariable ) ) ;
	pData = gHyp_type_assign ( pAI,
				   pAI->exec.pFrame,
				   pResult,
				   pLocalVariable,
				   gHyp_data_dataType ( pLocalVariable ),
				   FALSE, FALSE  ) ;
	gHyp_data_delete ( pData ) ;
	gHyp_data_delete ( pResult ) ;
      }
    }
  }
}

sLOGICAL gHyp_instance_replyMessage ( sInstance *pAI, sData *pMethodData )
{
  /* Description:
   *
   *	Sends a REPLY message when a QUERY method returns.
   *
   * Arguments:
   *
   *	pAI						[R]
   *	- pointer to instance
   *
   * Return value:
   *
   *	none
   *
   */
   
  
  sData
    *pTV,
    *pTV2,
    *pValue,
    *pToken,
    *pVariable,
    *pLvalue,
    *pResult,
    *pMethodVariable ;

  char
    /*senderId[SENDER_SIZE+1],*/
    value[VALUE_SIZE+1],
    *pMethodStr,
    *pVariableStr ;

  sLOGICAL
    isVector,
    isOldStyle,
    resend,
    status ;

  sWORD
    id ;

  int
    outgoingDepth,
    nBytes=0,
    maxTries,
    ss,
    ss2,
    context,
    context2,
    secsFd,
    stream,
    function,
    TID,
    SID,
    contentLength,
    saveJmpRootLevel=giJmpRootLevel,
    cond;

  sBYTE
    tokenType ;

  sSecs1
    *pSecs1=NULL ;

  sHsms
    *pHsms=NULL ;

  sSecs2
    *pSecs2 ;

  
  if ( pAI->msg.outgoingDepth < 1 ) return FALSE ;

  if ( guDebugFlags & DEBUG_DIAGNOSTICS )
    gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
			 "diag : Looking for outgoing reply at depth %d",
			 pAI->msg.outgoingDepth-1 ) ;
  

  gHyp_instance_decOutgoingDepth ( pAI ) ;
  outgoingDepth =  pAI->msg.outgoingDepth ;
  
  /* If the reply is SECS, assign the reply SECS message to 
   * the variable - <SyF(x+1)> - and if it exists globally 
   * (it's up to the program to make it beforehand), then it is 
   * copied into that so that it will be found upon
   * returning.
   */
  gHyp_instance_secsReply ( pAI, pMethodData ) ;

  /* If the method completed was QUERY initiated, a REPLY is needed. */
  if ( pAI->msg.outgoingReply[outgoingDepth]->msg ) {

    if ( pAI->msg.outgoingReply[outgoingDepth]->secs.id == NULL_DEVICEID ) {

      /* For non-secs messages, set the value of the STATUS variable to 
       * that of the method value. 
       */    

      pMethodStr = gHyp_aimsg_method ( 
	pAI->msg.outgoingReply[outgoingDepth]->msg ) ;

      pMethodVariable = gHyp_data_getChildByName ( pMethodData, pMethodStr ) ;
      if ( !pMethodVariable )
        pMethodVariable = gHyp_frame_findVariable ( pAI, 
						    pAI->exec.pFrame, 
						    pMethodStr ) ; 

      /* If the method has no value, STATUS remains unchanged */ 
      if ( pMethodVariable && gHyp_data_getCount ( pMethodVariable ) > 0 ) {
	
	pLvalue = gHyp_data_new ( NULL ) ;
	gHyp_data_setReference ( 
	  pLvalue, 
	  "STATUS", 
	  gHyp_frame_findRootVariable ( pAI->exec.pFrame, "STATUS" )  ) ;
	
	/* Set STATUS to value of method variable */
	pResult = gHyp_type_assign ( pAI,
				     pAI->exec.pFrame,
				     pLvalue,
				     pMethodVariable,
				     TYPE_NULL,
				     FALSE, FALSE  ) ;
	gHyp_data_delete ( pResult ) ;
	gHyp_data_delete ( pLvalue ) ;
      }
    }

    /* Get the list of tokens in the REPLY message */
    pTV = gHyp_aimsg_tv ( pAI->msg.outgoingReply[outgoingDepth]->msg ) ;

    /* Set the token values. */
    pTV2 = gHyp_data_new ( "_tvreply_" ) ;
    pToken = NULL ;
    ss = -1 ;
    context = -1 ;
    while ( (pToken = gHyp_data_nextValue ( pTV, 
					    pToken, 
					    &context,
					    ss ))) {

 
      /* Find the variable from the instance data base */
      pVariableStr = gHyp_data_getLabel ( pToken ) ;
      pVariable = gHyp_data_getChildByName ( pMethodData, pVariableStr ) ;
      if ( !pVariable )
        pVariable = gHyp_frame_findVariable ( pAI, 
					      pAI->exec.pFrame, 
					      pVariableStr ) ;
 
      isOldStyle = FALSE ;
      if ( pVariable ) {

	/* Determine if we can send the variable in the old style |TOKEN|VALUE|VALUE|| */
	isOldStyle = TRUE ;
	isVector = (gHyp_data_getDataType( pVariable ) > TYPE_STRING ) ;

	if ( !isVector ) {
          pValue = NULL ;
          ss2 = -1 ; 
          context2 = -1 ;
          while ( isOldStyle &&
	        (pValue = gHyp_data_nextValue ( pVariable, 
					        pValue, 
					        &context2,
					        ss2 ) ) ) {
	    tokenType = gHyp_data_tokenType(pValue) ;
	    if ( tokenType == TOKEN_VARIABLE ) isOldStyle = FALSE ;
	  }
	}
	if ( isVector ) isOldStyle = FALSE ;
      }
      if ( isOldStyle || !pVariable ) {
	pValue = gHyp_data_new ( gHyp_data_getLabel(pToken) ) ;
        gHyp_data_setReference ( pValue, pVariableStr, pVariable ) ;
      }
      else {
	contentLength = 0 ;
	pValue = gHyp_fileio_describeData ( pAI, pVariable, ' ', FALSE, FALSE, &contentLength ) ;
      }
      gHyp_data_append ( pTV2, pValue ) ;
    }

    gHyp_aimsg_setTV ( pAI->msg.outgoingReply[outgoingDepth]->msg, pTV2 ) ;
    
    /* Check to see if this is a secs message reply */
    if ( pAI->msg.outgoingReply[outgoingDepth]->secs.id != NULL_DEVICEID ) {

      /* It is, setup the arguments for the reply message */
      id = pAI->msg.outgoingReply[outgoingDepth]->secs.id ;
      stream = pAI->msg.outgoingReply[outgoingDepth]->secs.stream ;
      function = pAI->msg.outgoingReply[outgoingDepth]->secs.function ;
      TID = pAI->msg.outgoingReply[outgoingDepth]->secs.TID ;
      SID = pAI->msg.outgoingReply[outgoingDepth]->secs.SID ;

      /* Initialize the secs structures. */
      secsFd = gHyp_instance_getDeviceFd ( pAI, id ) ;
      pSecs2 = gHyp_instance_getSecs2byId ( pAI, id ) ;
      if ( !pSecs2 ) 
	return gHyp_util_logError("Failed to send SECS reply message");
      
      /* What variable must be sent back */
      sprintf ( value, "S%dF%d", stream, function ) ; 
      pVariable = gHyp_frame_findVariable ( pAI, 
					    pAI->exec.pFrame, 
					    value ) ;


      /* Old code saved here:  
       *
       * - when user undefs secondary reply, send back a SxF0.
       *     
       function = 0 ;
       sprintf ( value, "S%dF0", stream ) ; 
       pVariable = gHyp_frame_createVariable ( pAI->exec.pFrame, value ) ;
       *
       *
       * New code: do not send anything. Instead use S9 functions. 
      */

      if ( pVariable ) {

	gHyp_secs2_unParseSecs ( pSecs2, pVariable, pAI, stream, function ) ;
	
	/* Get the SECS I or HSMS structure */
	pHsms  = (sHsms*) gHyp_concept_getSocketObject ( pAI->exec.pConcept, 
							 secsFd, 
							 DATA_OBJECT_HSMS ) ;

	pSecs1 = (sSecs1*) gHyp_concept_getSocketObject ( pAI->exec.pConcept, 
	 						  secsFd, 
							  DATA_OBJECT_SECS1 ) ; 
	status = FALSE ;
	
	/* Send message. */
	if ( pHsms ) {
	  
	  nBytes = gHyp_hsms_outgoing ( pHsms, 
					pSecs2, 
					pAI,
					stream,
					function,
					id,
					TID,
					SID,
					MESSAGE_REPLY ) ;
	  
	  gHyp_aimsg_delete ( pAI->msg.outgoingReply[outgoingDepth]->msg ) ;
          pAI->msg.outgoingReply[outgoingDepth]->msg = NULL ;
          pAI->msg.outgoingReply[outgoingDepth]->secs.id = NULL_DEVICEID ;
          pAI->msg.outgoingReply[outgoingDepth]->secs.stream = -1 ;
          pAI->msg.outgoingReply[outgoingDepth]->secs.function = -1 ;
          pAI->msg.outgoingReply[outgoingDepth]->secs.TID = -1 ;
          pAI->msg.outgoingReply[outgoingDepth]->secs.SID = -1 ;
	  
	  if ( nBytes < 0 )
	    return gHyp_util_logError("Failed to send SECS HSMS reply message");
	  else {
            /* Successfully sent */
	    if ( guDebugFlags & DEBUG_DIAGNOSTICS )
		gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
				     "SECS (HSMS) reply successfully sent at depth %d",
				     outgoingDepth ) ;

	  }
	}
	else if ( pSecs1 ) {
	  
	  /* If gHyp_secs1_outgoing returns 0, then the message will have been
	   * interrupted (by an ENQ/ENQ contention).  We should try resending
	   * the reply message again, after we have serviced the interrupting
           * message.
	   */
	  
	  nBytes = 0 ;
	  maxTries = MAX_RESEND ;
	  while ( nBytes == 0 && maxTries > 0 ) {
          	    
	    nBytes = gHyp_secs1_outgoing ( pSecs1, 
					   pSecs2,
					   pAI,
					   stream,
					   function,
					   id,
					   TID,
					   SID,
					   MESSAGE_REPLY) ;
	    
	    if ( nBytes < 0 ) {
	      gHyp_instance_signalPipe ( pAI, 
		gHyp_secs1_fd ( pSecs1 ),
		gHyp_secs1_port ( pSecs1 ),
		id ) ;
	      return gHyp_util_logWarning("Failed to send SECS reply message");
	    }
	    else if ( nBytes == 0 ) {

	      /* ENQ contention:  WE DIDN'T SEND THE REPLY MESSAGE!!!
               * We'll do that later.  See gHyp_frame_return 
               */

              /*return gHyp_util_logInfo("ENQ contention while sending SECS reply message, trying later");*/

	   
	      /* Since we didn't send the reply, restore the outgoing depth */
	      gHyp_instance_incOutgoingDepth ( pAI ) ;
              
              if ( guDebugFlags & DEBUG_DIAGNOSTICS )
	        gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
	        "ENQ Contention on Reply. Adjusting jmpRootLevel to %d",giJmpLevel+1);

	      if ( giJmpLevel == MAX_JMP_LEVEL ) {
                gHyp_util_logError ( "Parse jump level overflow at %d", MAX_JMP_LEVEL ) ;
                longjmp ( gsJmpStack[0], COND_FATAL ) ;
              }
	      
	      giJmpRootLevel = giJmpLevel+1 ;
                cond = gHyp_instance_ENQcontention ( pAI, pAI->exec.pFrame ) ;
	      giJmpRootLevel = saveJmpRootLevel ; 

	      resend = ( pAI->msg.outgoingReply[outgoingDepth]->msg != NULL &&
                         pAI->msg.outgoingReply[outgoingDepth]->secs.id == id && 
                         pAI->msg.outgoingReply[outgoingDepth]->secs.stream == stream &&
                         pAI->msg.outgoingReply[outgoingDepth]->secs.function == function &&
                         pAI->msg.outgoingReply[outgoingDepth]->secs.TID == TID &&
                         pAI->msg.outgoingReply[outgoingDepth]->secs.SID == SID
		       ) ;

	      pSecs1 = (sSecs1*) gHyp_concept_getSocketObject ( pAI->exec.pConcept, 
								secsFd, 
								DATA_OBJECT_SECS1 ) ;
	     
              if ( resend && pSecs1 && pAI->msg.outgoingDepth > 0 ) {              

  	        if ( outgoingDepth+1 >= pAI->msg.outgoingDepth  ) 
		  gHyp_instance_decOutgoingDepth ( pAI ) ;

		gHyp_util_logInfo (
		  "Re-sending interrupted S%dF%d reply message at depth %d",
		  stream, function,outgoingDepth  ) ;
                maxTries-- ;
	      }
              else { 
                maxTries = 0 ;
              }
	    } /* nBytes == 0 */
            else {
              /* Successfully sent */

	      if ( guDebugFlags & DEBUG_DIAGNOSTICS )
		gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
				     "SECS reply successfully sent at depth %d",
				     outgoingDepth ) ;
	      if ( pAI->msg.outgoingReply[outgoingDepth]->msg )
	        gHyp_aimsg_delete ( pAI->msg.outgoingReply[outgoingDepth]->msg ) ;
              pAI->msg.outgoingReply[outgoingDepth]->msg = NULL ;
              pAI->msg.outgoingReply[outgoingDepth]->secs.id = NULL_DEVICEID ;
              pAI->msg.outgoingReply[outgoingDepth]->secs.stream = -1 ;
              pAI->msg.outgoingReply[outgoingDepth]->secs.function = -1 ;
              pAI->msg.outgoingReply[outgoingDepth]->secs.TID = -1 ;
              pAI->msg.outgoingReply[outgoingDepth]->secs.SID = -1 ;
	      
            }
	  }  /* End while */
	}
      }
    }
    else {

      /* Not a secs reply message */
      gHyp_concept_route ( 
	pAI->exec.pConcept, 
	gHyp_aimsg_unparse(pAI->msg.outgoingReply[outgoingDepth]->msg) ) ;

      gHyp_aimsg_delete ( pAI->msg.outgoingReply[outgoingDepth]->msg ) ;
      pAI->msg.outgoingReply[outgoingDepth]->msg = NULL ;

    }  
  
  }

  return TRUE ;
}

int gHyp_instance_ENQcontention ( sInstance * pAI, sFrame * pFrame )
{
  int cond ;
  sBYTE currentState ;

  /* Save the state */
  currentState = pAI->exec.state ;
  pAI->exec.state = STATE_QUERY ;

  /* Setting FRAME_GLOBAL_TRUE allows HS to execute handlers */
  gHyp_frame_setGlobalFlag ( pFrame, FRAME_GLOBAL_TRUE ) ;

  do {
    /* Queue up the interrupting message.  
     * TRUE means don't block, which means we are not executing select(),
     * we are only fetching the incoming message from the queue and setting
     * up for the message call.
     */
    cond = gHyp_instance_read ( pAI, TRUE ) ;
    /*gHyp_util_debug("INSTANCE_READ = %d",cond);*/
    cond = gHyp_instance_parse ( pAI ) ;
    /*gHyp_util_debug("INSTANCE_PARSE = %d, state=%s",cond,gzaInstanceState[pAI->exec.state]);*/
    
  }
  while ( cond == COND_NORMAL && pAI->exec.state == STATE_PARSE );

  /* DO NOT restore the state,
   * The new state is needed by replyMessage
   *
  pAI->exec.state = currentState ) ;
  */

  return 0 ;
}

void gHyp_instance_setExpectedReply ( sInstance *pAI, 
				      char *senderPath,
				      char* method,
				      char *transactionID,
				      int timeoutTime ) 
{
  /* Description:
   *
   *	When sending a query message, remember the name of the target of 
   *	the query message and the method name, these will be expected in the 
   *	returning reply message.
   *
   * Arguments:
   *
   *	pAI						[R]
   *	- pointer to instance
   *
   *	target						[R]
   *	- pointer to target expected in reply message
   *
   *	method						[R]
   *	- pointer to method expected in reply message
   *
   *	transactionID
   *	- transaction id
   *
   *
   * Return value:
   *
   *	none
   *
   */

  int
    incomingDepth = pAI->msg.incomingDepth -1 ;

  
  if ( guDebugFlags & DEBUG_DIAGNOSTICS )
    gHyp_util_logDebug ( 
      FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
      "diag : Setting expected reply for sender:method:tid %s:%s:%s at incoming depth %d",
      senderPath, method, transactionID, incomingDepth ) ;
  
  strcpy ( pAI->msg.incomingReply[incomingDepth]->sender, senderPath ) ;
  strcpy ( pAI->msg.incomingReply[incomingDepth]->method, method ) ;
  strcpy ( pAI->msg.incomingReply[incomingDepth]->transactionID, 
	   transactionID ) ;
  pAI->msg.incomingReply[incomingDepth]->timeoutTime = timeoutTime ;
  pAI->msg.incomingReply[incomingDepth]->frameDepth = gHyp_frame_depth ( pAI->exec.pFrame ) ;

  pAI->msg.incomingReply[incomingDepth]->pStack = gHyp_frame_stack (pAI->exec.pFrame ) ;

  pAI->msg.incomingReply[incomingDepth]->secs.id = NULL_DEVICEID ;
  pAI->msg.incomingReply[incomingDepth]->secs.stream = -1 ;
  pAI->msg.incomingReply[incomingDepth]->secs.function = -1 ;
  pAI->msg.incomingReply[incomingDepth]->secs.TID = -1 ;
  pAI->msg.incomingReply[incomingDepth]->secs.SID = -1 ;
  return ;
}

void gHyp_instance_requeue ( sInstance* pAI )
{
  int 
    n ;

  if ( pAI->signal.uMSGPENDING && pAI->msg.incoming != NULL ) {

    pAI->signal.uMSGPENDING = 0 ;
    
    /* Advance to next location */
    pAI->msg.endRQ++ ;
    if ( pAI->msg.endRQ >= MAX_QUEUE_DEPTH ) pAI->msg.endRQ = 0 ;

    n = pAI->msg.endRQ ;

    if ( pAI->msg.rq[n] != NULL ) {
      gHyp_util_logWarning("Overflow re-queue at %u",MAX_QUEUE_DEPTH);
      gHyp_aimsg_delete ( pAI->msg.rq[n] ) ;
    }
    
    pAI->msg.rq[n] = pAI->msg.incoming ;
    pAI->msg.rqSecs[n] = pAI->msg.inSecs ;
    pAI->msg.inSecs.id = NULL_DEVICEID ;
    pAI->msg.inSecs.stream = -1 ;
    pAI->msg.inSecs.function = -1 ;
    pAI->msg.inSecs.TID = -1 ;
    pAI->msg.inSecs.SID = -1 ;
    pAI->msg.incoming = NULL ;

    if ( guDebugFlags & DEBUG_DIAGNOSTICS )
      gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
			   "diag : Incoming message transfered to requeue") ;
  }
  return ;
}

void gHyp_instance_setSecsMLB ( sInstance *pAI, sBYTE mlb ) 
{
  pAI->device.mlb = mlb ;
}

sBYTE gHyp_instance_getSecsMLB ( sInstance *pAI ) 
{
  return pAI->device.mlb ;
}

void gHyp_instance_setSecsXML ( sInstance *pAI, sLOGICAL isXML ) 
{
  pAI->device.xml = isXML ;
}

sLOGICAL gHyp_instance_getSecsXML ( sInstance *pAI ) 
{
  return pAI->device.xml ;
}

void gHyp_instance_setSecsMHP ( sInstance *pAI, sWORD mhp ) 
{
  pAI->device.mhp = mhp ;
}

sWORD gHyp_instance_getSecsMHP ( sInstance *pAI ) 
{
  return pAI->device.mhp ;
}

void gHyp_instance_setSecsReplyOut ( sInstance *pAI,
				     sWORD id, 
				     int stream, 
				     int function,
				     int TID,
				     int SID )
{
  char
    value[VALUE_SIZE+1] ;

  unsigned
    i=0 ;

  int
    outgoingDepth ;

  sData
    *pVariable ;
  
  outgoingDepth = pAI->msg.outgoingDepth ;

  /* Remember the id, stream, and function */
  pAI->msg.outgoingReply[outgoingDepth]->secs.id = id ;
  pAI->msg.outgoingReply[outgoingDepth]->secs.stream = stream ;
  pAI->msg.outgoingReply[outgoingDepth]->secs.function = function ;
  pAI->msg.outgoingReply[outgoingDepth]->secs.TID = TID ;
  pAI->msg.outgoingReply[outgoingDepth]->secs.SID = SID ;

  if ( id == NULL_DEVICEID ) return ;

  /* Create the SECS variable as a global, same as the method scope. */
  sprintf ( value, "S%dF%d", stream, function ) ; 
  pVariable = gHyp_frame_createRootVariable ( pAI->exec.pFrame, value ) ;

  /* Make the variable a binary with value 0 */  
  gHyp_data_newVector ( pVariable, TYPE_BINARY, 1, TRUE ) ;
  gHyp_data_setVectorRaw ( pVariable, &i, 0 ) ;

  if ( guDebugFlags & DEBUG_DIAGNOSTICS )
    gHyp_util_logDebug (FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
			"Created default secs reply variable '%s' at outgoing depth %d",
			gHyp_data_print ( pVariable ),
			outgoingDepth ) ;
}


void gHyp_instance_setSecsReplyIn ( sInstance *pAI, 
				    sWORD id, 
				    int stream, 
				    int function,
				    int TID,
				    int SID )  
{
  int
    incomingDepth = pAI->msg.incomingDepth -1 ;

  if ( guDebugFlags & DEBUG_DIAGNOSTICS )
    gHyp_util_logDebug ( 
      FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
      "diag : Setting SECS reply S%dF%d at incoming depth %d",
      stream, function, incomingDepth ) ;

  /* Remember the id, stream, and function */
  pAI->msg.incomingReply[incomingDepth]->secs.id = id ;
  pAI->msg.incomingReply[incomingDepth]->secs.stream = stream ;
  pAI->msg.incomingReply[incomingDepth]->secs.function = function ;
  pAI->msg.incomingReply[incomingDepth]->secs.TID = TID ;
  pAI->msg.incomingReply[incomingDepth]->secs.SID = SID ;
}

void gHyp_instance_secsEnable ( sInstance *pAI, 
				sWORD id, 
				int stream, 
				int function, 
				sData *pMethodVariable )
{
  char
    secsMethod[METHOD_SIZE+1] ;

  sMethod
    *pMethod ;

  /* Hash the string I*S*F* that will allow us to lookup pMethodVariable */
  sprintf ( secsMethod, "I%dS%dF%d", id, stream, function ) ;
  gHyp_hash_update ( pAI->device.pHash,
		     DEVICE_HASH_TABLE_SIZE,
		     secsMethod,
		     pMethodVariable ) ;

  /* Enable the method */
  pMethod = (sMethod*) gHyp_data_getObject ( pMethodVariable ) ;
  if ( pMethod ) gHyp_method_enable ( pMethod ) ;
}

void gHyp_instance_secsDisable ( sInstance *pAI, 
				 sWORD id, 
				 int stream, 
				 int function )
{
  char
    secsMethod[METHOD_SIZE+1] ;

  /* Hash the string I*S*F* that will allow us to lookup pMethodVariable */
  sprintf ( secsMethod, "I%dS%dF%d", id, stream, function ) ;
  gHyp_hash_remove ( pAI->device.pHash,
		     DEVICE_HASH_TABLE_SIZE,
		     secsMethod,
		     NULL ) ;
}

sData* gHyp_instance_secsMethod ( sInstance *pAI, 
				  sWORD id, 
				  int stream, 
				  int function )
{
  char
    secsMethod[METHOD_SIZE+1] ;

  /* Hash the string I*S*F* that will allow us to lookup pMethodVariable */
  sprintf ( secsMethod, "I%dS%dF%d", id, stream, function ) ;
  return (sData*) gHyp_hash_find ( pAI->device.pHash,
				   DEVICE_HASH_TABLE_SIZE,
				   secsMethod ) ;
}

sAImsg *gHyp_instance_incomingMsg ( sInstance *pAI ) 
{
  /* Get next incoming message location for instance */
  int 
    n ;

  pAI->msg.endQQ++ ;
  if ( pAI->msg.endQQ >= MAX_QUEUE_DEPTH ) pAI->msg.endQQ = 0 ;
  n = pAI->msg.endQQ ;

  if ( guDebugFlags & DEBUG_DIAGNOSTICS )
    gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
    "Queue = Start [%d:%s], End [%d:%s]",
      pAI->msg.startQQ,
      pAI->msg.qq[pAI->msg.startQQ]?gHyp_aimsg_tokensValues(pAI->msg.qq[pAI->msg.startQQ]):"empty",
      pAI->msg.endQQ,
      pAI->msg.qq[pAI->msg.endQQ]?gHyp_aimsg_tokensValues(pAI->msg.qq[pAI->msg.endQQ]):"empty" ) ;

  if ( pAI->msg.qq[n] ) {
    /* Overflow!! */
    gHyp_util_logWarning("Message queue overflow at %d",MAX_QUEUE_DEPTH);
    /*gHyp_aimsg_delete ( pAI->msg.qq[n] ) ;*/
  }
  else {
    pAI->msg.qq[n] = gHyp_aimsg_new() ;
  }
  /* We call signalMsg() here as well as in readQueue(),
   * because it sets uMSG, which is used by gHyp_instance_isSignal(),
   * which in turn makes sure the select() timeout is zero.
   * Thus, uMSG is important to set as soon as the message has been received.
   * It also must be cleared when there are no messages so that spinning
   * does not occur.
   */
  gHyp_instance_signalMsg ( pAI ) ;

  if ( guDebugFlags & DEBUG_DIAGNOSTICS )
    gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
      "Initialize new message in qq[%d]",n ) ;

  return pAI->msg.qq[n] ;
}

void  gHyp_instance_secsIncoming (  sInstance *pAI, 
				    sWORD id, 
				    int stream, 
				    int function,
				    int TID,
				    int SID )
{
  int
    n = pAI->msg.endQQ ;

  /*gHyp_util_logInfo("Incoming qqSecs[%d] S%dF%d",n,stream,function);*/

  pAI->msg.qqSecs[n].id = id ;
  pAI->msg.qqSecs[n].stream = stream ;
  pAI->msg.qqSecs[n].function = function ;
  pAI->msg.qqSecs[n].TID = TID ;
  pAI->msg.qqSecs[n].SID = SID ;
  return ;
}

sSecs2 *gHyp_instance_getSecs2byId ( sInstance *pAI, sWORD id )
{
  sWORD i ;
  for ( i=0; i<pAI->device.numDevices; i++ )
    if ( pAI->device.id[i] == id ) 
      return pAI->device.pSecs2[i] ;

  return NULL ;
}

SOCKET gHyp_instance_getDeviceFd ( sInstance *pAI, sWORD id )
{
  sWORD i ;
  for ( i=0; i<pAI->device.numDevices; i++ )
    if ( pAI->device.id[i] == id ) 
      return pAI->device.fd[i] ;

  return INVALID_SOCKET ;
}

sLOGICAL gHyp_instance_hasDeviceId ( sInstance *pAI, SOCKET fd )
{
  sWORD i ;
  for ( i=0; i<pAI->device.numDevices; i++ )
    if ( pAI->device.fd[i] == fd ) return TRUE ;

  return FALSE ;
}



sWORD gHyp_instance_getDeviceId ( sInstance *pAI, SOCKET fd )
{
  sWORD i ;
  for ( i=0; i<pAI->device.numDevices; i++ )
    if ( pAI->device.fd[i] == fd ) 
      return pAI->device.id[i] ;

  return NULL_DEVICEID ;
}

sLOGICAL gHyp_instance_updateFd ( sInstance *pAI, 
				  SOCKET fd,
				  sWORD id,
				  sSecs2 *pSecs2, 
				  sLOGICAL rBit ) 
{
  /* 'id' is the device id, it is a unique key.
   * 'fd' is the socket.
   *
   * Fd to Id has a one to many relationship.
   * 
   * When the argument 'id' in not NULL_DEVICEID, then
   * the id must match. We give a new 'fd' to an
   * existing 'id'
   *
   * When argument 'id' is NULL_DEVICEID, the the
   * fd must match.  This could be used to update
   * the pSecs pointer for a SECS handle that did
   * not have any id's assigned to it yet.
   * But, this function is never called (as of 12/28/10)
   * with id == NULL_DEVICE
   */
  int
    i ;
  
  if ( guDebugFlags & DEBUG_DIAGNOSTICS )
    gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
      "Requesting update id %d with socket %d",id,fd ) ;

  for ( i=0; i<pAI->device.numDevices; i++ ) {

    if ( ( id == NULL_DEVICEID && fd == pAI->device.fd[i] )

		      ||

         ( id != NULL_DEVICEID && id == pAI->device.id[i] ) ) {

       /* Found a socket with a the key 'id' */

	if ( guDebugFlags & DEBUG_DIAGNOSTICS ) {
          if ( pAI->device.fd[i] != fd ) 
  	    gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
	      "Updating id %d from socket %d to socket %d",
		      pAI->device.id[i],
		      pAI->device.fd[i],
		      fd ) ;
	}

      /* Replace everything */
      if ( pSecs2 ) {

	if ( pAI->device.pSecs2[i] != pSecs2 ) {
	  gHyp_secs2_delete ( pAI->device.pSecs2[i] ) ;
	  pAI->device.pSecs2[i] = pSecs2 ;
	}
	pAI->device.rBit[i] = rBit ;
      }
      pAI->device.fd[i] = fd ;
      return TRUE ;
    }
  }

  /* Otherwise, add a new connection. 
   * Look for the first available slot.
   */
  for ( i=0; i<pAI->device.numDevices; i++ ) {
    if ( pAI->device.fd[i] == INVALID_SOCKET ) {
      /* Found an empty spot, use it */
      if ( guDebugFlags & DEBUG_DIAGNOSTICS )
	gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
	    "Inserting id %d with socket %d",id,fd ) ;
      pAI->device.pSecs2[i] = pSecs2 ;
      pAI->device.id[i] = id ;
      pAI->device.fd[i] = fd ;
      pAI->device.rBit[i] = rBit ;
      return TRUE ;
    }
  }

  /* Add a new connection */
  if ( i < MAX_DEVICES ) {
    /* A new spot, use it */
    if ( guDebugFlags & DEBUG_DIAGNOSTICS )
      gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
	 "Inserting id %d with socket %d",id,fd ) ;
    pAI->device.pSecs2[i] = pSecs2 ;
    pAI->device.id[i] = id ;
    pAI->device.fd[i] = fd ;
    pAI->device.rBit[i] = rBit ;
    pAI->device.numDevices++ ;
    return TRUE ;
  }
  else {
    gHyp_util_logError ( "More than the maximum of %d devices requested",
			 MAX_DEVICES ) ;
    longjmp ( gsJmpStack[0], COND_FATAL ) ;
  }    
  return FALSE ; 
}

void gHyp_instance_deleteFd ( sInstance *pAI, SOCKET fd ) 
{
  int i ;

  for ( i=0; i<pAI->device.numDevices; i++ ) {

    if ( fd == pAI->device.fd[i] ) {

      if ( pAI->device.pSecs2[i] ) {
	  gHyp_secs2_delete ( pAI->device.pSecs2[i] ) ;
	  pAI->device.pSecs2[i] = NULL ;
      }
      pAI->device.fd[i] = INVALID_SOCKET ;
      pAI->device.id[i] = NULL_DEVICEID ;
    }
  } 
}



sLOGICAL gHyp_instance_isRbitSet ( sInstance * pAI, sWORD id ) 
{
  int i ;

  for ( i=0; i<pAI->device.numDevices; i++ ) {
    if ( pAI->device.id[i] == id ) return pAI->device.rBit[i] ;
  }
  return FALSE ;
}

void gHyp_instance_portEnable ( sInstance *pAI, 
				sWORD id,  
				sData *pMethodVariable )
{
  char
    portMethod[METHOD_SIZE+1] ;
  
  sMethod
    *pMethod ;

  /* Hash the string that will allow us to lookup pMethodVariable */
  sprintf ( portMethod, "%d", id ) ;
  gHyp_hash_update ( pAI->device.pHash,
		     DEVICE_HASH_TABLE_SIZE,
		     portMethod,
		     pMethodVariable ) ;

  /* Enable the method */
  pMethod = (sMethod*) gHyp_data_getObject ( pMethodVariable ) ;
  if ( pMethod ) gHyp_method_enable ( pMethod ) ;
}

void gHyp_instance_portDisable ( sInstance *pAI, 
				 sWORD id )
{
  char
    portMethod[METHOD_SIZE+1] ;

  /* Hash the string that will allow us to lookup pMethodVariable */
  sprintf ( portMethod, "%d", id ) ;
  gHyp_hash_remove ( pAI->device.pHash,
		     DEVICE_HASH_TABLE_SIZE,
		     portMethod,
		     NULL ) ;
}

sData* gHyp_instance_portMethod ( sInstance *pAI, 
				  sWORD id )
{
  char
    portMethod[METHOD_SIZE+1] ;

  /* Hash the string that will allow us to lookup pMethodVariable */
  sprintf ( portMethod, "%d", id ) ;
  return (sData*) gHyp_hash_find ( pAI->device.pHash,
				   DEVICE_HASH_TABLE_SIZE,
				   portMethod ) ;
}

void gHyp_instance_reset ( sInstance *pAI, char state, sLOGICAL enable )
{
  /* Description:
   *
   *	Flush the stack and traceback the frame.
   *	Set the STATE of the instance.
   *	Enable or disable further execution.
   *
   *	Called when:
   *	   
   *	1. When compiling only, the HyperScript program is put into a PARSE
   *	   state with execution disabled. (hs.c)
   *
   *	2. When the program end-of-file is received, the program will go to
   *	   IDLE state to wait for a message if the "-e" switch was used when
   *	   starting HyperScript. (hs.c)
   *
   *    3. Instantiate a new instance.
   *
   *    4. After an interactive error.
   *
   * Arguments:
   *
   *	pAI						[R]
   *	- pointer to instance
   *
   *	state
   *    - new state of instance
   *
   *    enabled
   *    - is execution enabled on frame
   *
   * Return value:
   *
   *	none
   *
   */
  int
    hypIndex ;

  sHyp
    *pHyp ;

  gHyp_frame_reset ( pAI->exec.pFrame ) ;
  gHyp_stack_flush ( gHyp_frame_stack ( pAI->exec.pFrame ) ) ;

  if ( enable )
    gHyp_frame_setGlobalFlag ( pAI->exec.pFrame, FRAME_GLOBAL_TRUE ) ;
  else
    gHyp_frame_clearGlobalFlag ( pAI->exec.pFrame, FRAME_GLOBAL_TRUE ) ;

  pAI->exec.state = state ;

  pHyp = gHyp_frame_getHyp ( pAI->exec.pFrame ) ;
  hypIndex = gHyp_hyp_getHypCount ( pHyp ) ;
  gHyp_frame_setHypIndex ( pAI->exec.pFrame, hypIndex ) ;
  gHyp_frame_setStatementIndex ( pAI->exec.pFrame, hypIndex ) ;

}

sBYTE gHyp_instance_getState ( sInstance *pAI )
{
  /* Description:
   *
   *	Returns state of the instance.
   *	State can be STATE_IDLE, STATE_PARSE, STATE_EXECUTE, STATE_QUEURY
   *
   * Arguments:
   *
   *	pAI						[R]
   *	- pointer to instance
   *
   * Return value:
   *
   *	Returns instance state.
   *
   */
  return pAI->exec.state ;
}

void gHyp_instance_setState ( sInstance *pAI, sBYTE state  )
{
  /* Description:
   *
   *	Sets the state of the instance.
   *	State can be STATE_IDLE, STATE_PARSE, STATE_EXECUTE
   *
   * Arguments:
   *
   *	pAI						[R]
   *	- pointer to instance
   *
   * Return value:
   *
   *	none
   *
   */
  pAI->exec.state = state ;
  return ;
}

char* gHyp_instance_getTargetId ( sInstance *pAI )
{
  return pAI->msg.targetId ;
}

char* gHyp_instance_getTargetPath ( sInstance *pAI )
{
  return pAI->msg.targetPath ;
}

void gHyp_instance_setTargetPath ( sInstance *pAI, char *targetPath )
{
  strcpy ( pAI->msg.targetPath, targetPath )  ;
}

void gHyp_instance_newTargetId ( sInstance *pAI )
{
  gHyp_aimsg_getTargetId ( pAI->msg.current, pAI->msg.targetId ) ;
  return ;
}

sAImsg *gHyp_instance_outgoingMsg ( sInstance *pAI ) 
{
  if ( pAI->msg.outgoing == NULL ) pAI->msg.outgoing = gHyp_aimsg_new();
  return pAI->msg.outgoing ;
}

sAImsg *gHyp_instance_currentMsg ( sInstance *pAI ) 
{
  /* Description:
   *
   *	Returns a pointer to the current message.  This routine is used
   *	by such functions as method(), mode(), sender(), and self().
   *	
   *	Note that when pAI->signal.uMSGPENDING is true when executing a handler, the
   *	current message becomes the pending (incoming) message.  
   *	Thus, if a new message comes in while waiting at a query() function,
   *	the on_message handler will refer to the new message when using
   *	the method(), mode(), sender(), and self() functions.
   *
   * Arguments:
   *
   *	pAI						[R]
   *	- pointer to instance
   *
   * Return value:
   *
   *	none
   */
   
  if ( pAI->signal.uMSGPENDING && pAI->msg.incoming )
    return pAI->msg.incoming ;
  else
    return pAI->msg.current ;
}

void gHyp_instance_setAlarm ( sInstance *pAI, int seconds )
{
  gsCurTime = time ( NULL ) ; 
  if ( seconds == 0 )
    pAI->exec.alarmTime = 0 ;
  else
    pAI->exec.alarmTime = gsCurTime + seconds ;
    
  gHyp_instance_nextEvent ( pAI ) ;
}

void gHyp_instance_setAlarmHandler ( sInstance *pAI, int hypIndex, sHyp *pHyp )
{                  
  pAI->exec.handler[HANDLER_ALARM].hypIndex = hypIndex ;
  pAI->exec.handler[HANDLER_ALARM].pHyp = pHyp ;
  return ;
}

void gHyp_instance_setDeath ( sInstance *pAI, int seconds )
{                  
  gsCurTime = time ( NULL ) ; 
  if ( seconds == 0 )
    pAI->exec.deathTime = 0 ;
  else
    pAI->exec.deathTime = gsCurTime + seconds ;
    
  gHyp_instance_nextEvent ( pAI ) ;
}

void gHyp_instance_setDeathHandler ( sInstance *pAI, int hypIndex, sHyp *pHyp )
{                  
  pAI->exec.handler[HANDLER_DEATH].hypIndex = hypIndex ;
  pAI->exec.handler[HANDLER_DEATH].pHyp = pHyp ;
  return ;
}

void gHyp_instance_setErrorHandler ( sInstance *pAI, int hypIndex, sHyp *pHyp )
{                  
  pAI->exec.handler[HANDLER_ERROR].hypIndex = hypIndex ;
  pAI->exec.handler[HANDLER_ERROR].pHyp = pHyp ;
  return ;
}

void gHyp_instance_setIntHandler ( sInstance *pAI, int hypIndex, sHyp *pHyp )
{                  
  pAI->exec.handler[HANDLER_INTERRUPT].hypIndex = hypIndex ;
  pAI->exec.handler[HANDLER_INTERRUPT].pHyp = pHyp ;
  return ;
}

void gHyp_instance_setHangupHandler( sInstance *pAI, int hypIndex, sHyp *pHyp )
{                  
  pAI->exec.handler[HANDLER_HANGUP].hypIndex = hypIndex ;
  pAI->exec.handler[HANDLER_HANGUP].pHyp = pHyp ;
  return ;
}

void gHyp_instance_setConnectHandler( sInstance *pAI, int hypIndex, sHyp *pHyp )
{                  
  pAI->exec.handler[HANDLER_CONNECT].hypIndex = hypIndex ;
  pAI->exec.handler[HANDLER_CONNECT].pHyp = pHyp ;
  return ;
}

void gHyp_instance_setPipeHandler ( sInstance *pAI, int hypIndex, sHyp *pHyp )
{                  
  pAI->exec.handler[HANDLER_PIPE].hypIndex = hypIndex ;
  pAI->exec.handler[HANDLER_PIPE].pHyp = pHyp ;
  return ;
}

void gHyp_instance_setMessageHandler ( sInstance *pAI, 
				       int hypIndex, 
				       sHyp *pHyp )
{
  pAI->exec.handler[HANDLER_MESSAGE].hypIndex = hypIndex ;
  pAI->exec.handler[HANDLER_MESSAGE].pHyp = pHyp ;
  return ;
}

void gHyp_instance_setMethodCall ( sInstance* pAI )
{
  pAI->signal.uMETHODCALL = 1 ;
  return ;
}

void gHyp_instance_setDerefHandler ( sInstance* pAI, 
				     int hypIndex, 
				     sHyp *pHyp )
{
  pAI->signal.uDEREFERENCE = 1 ;
  pAI->exec.handler[HANDLER_DEREFERENCE].hypIndex = hypIndex ;
  pAI->exec.handler[HANDLER_DEREFERENCE].pHyp = pHyp ;
  return ;
}

void gHyp_instance_initTimeOut ( sInstance *pAI, int seconds )
{                  
  gsCurTime = time ( NULL ) ; 
  pAI->exec.timeOut = seconds ;
}

void gHyp_instance_cancelTimeOut ( sInstance *pAI, int depth )
{ 
  pAI->msg.incomingReply[depth]->timeoutTime  = 0 ;

  /*gHyp_util_debug("Cancelling timeout for %s S%dF%d",
	  pAI->msg.incomingReply[depth]->method,
	  pAI->msg.incomingReply[depth]->secs.stream,
	  pAI->msg.incomingReply[depth]->secs.function );
  */
  pAI->exec.timeOutTime = 0 ;
  gHyp_instance_nextEvent ( pAI ) ;
}

time_t gHyp_instance_getTimeOutTime ( sInstance *pAI )
{
  return pAI->exec.timeOutTime ;
}

void gHyp_instance_setTimeOut ( sInstance *pAI )
{        
  int n = pAI->msg.incomingDepth-1;

  gsCurTime = time ( NULL ) ; 
  if ( pAI->exec.timeOut == 0 ) {
    pAI->exec.timeOutTime = 0 ;
    pAI->msg.incomingReply[n]->timeoutTime  = 0 ;
  }
  else {
    pAI->exec.timeOutTime = gsCurTime + pAI->exec.timeOut ;
    pAI->msg.incomingReply[n]->timeoutTime  = (int) pAI->exec.timeOutTime ;
  }
  /*gHyp_util_debug("setting timeout for %s S%dF%d",
	  pAI->msg.incomingReply[n]->method,
	  pAI->msg.incomingReply[n]->secs.stream,
	  pAI->msg.incomingReply[n]->secs.function );
  */
  gHyp_instance_nextEvent ( pAI ) ;
}


void gHyp_instance_restoreTimeOut ( sInstance *pAI )
{        
  int n = pAI->msg.incomingDepth-1;

  gsCurTime = time ( NULL ) ; 
  if ( pAI->exec.timeOut == 0 ) {
    pAI->exec.timeOutTime = 0 ;
    pAI->msg.incomingReply[n]->timeoutTime  = 0 ;
  }
  else {
    pAI->exec.timeOutTime = pAI->msg.incomingReply[n]->timeoutTime ;

  }
  gHyp_instance_nextEvent ( pAI ) ;
}

void gHyp_instance_setTimeOutHandler ( sInstance *pAI, 
				       int hypIndex, 
				       sHyp *pHyp )
{
  pAI->exec.handler[HANDLER_TIMEOUT].hypIndex = hypIndex ;
  pAI->exec.handler[HANDLER_TIMEOUT].pHyp = pHyp ;
  return ;
}

sData* gHyp_instance_setStatus ( sInstance *pAI, char *pStatus )
{
  sData	
    *pVariable,
    *pValue ;
  
  int
    n ;

  char
    value[VALUE_SIZE+1] ;

  pVariable = gHyp_frame_findRootVariable ( pAI->exec.pFrame, "STATUS" ) ;
  
  if ( guDebugFlags & DEBUG_STATE ) {
    n = gHyp_data_getStr ( pVariable,
			   value,
			   VALUE_SIZE,
			   0,
			   TRUE ) ;
    if ( n == -1 || strcmp ( pStatus, value ) != 0 )
      gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_STATE,
			   "state: STATUS = '%s'",pStatus) ;
  }

  pValue = gHyp_data_new ( NULL ) ;
  gHyp_data_deleteValues ( pVariable ) ;

  gHyp_data_setToken ( pValue, pStatus ) ;
  gHyp_data_append ( pVariable, pValue ) ;
  return pVariable ;
}

void gHyp_instance_warning ( sInstance * pAI, char *status, char *fmt, ... )
{
  char		buf[MAX_OUTPUT_LENGTH+1] ;
  va_list	ap ;
  
  gHyp_instance_setStatus ( pAI, status ) ;
  
  pAI->signal.uWARNING = 1 ;

  if ( guRunFlags & RUN_QUIET ) return ;

  /* Create and display the warning message */
  sprintf ( buf, "%s: warning: ", status ) ; 
  va_start ( ap, fmt ) ;
  vsprintf ( buf+strlen(buf), fmt, ap ) ;
  va_end ( ap ) ;
  strcat ( buf, "\n" ) ;
  gHyp_util_output ( buf ) ;
}

void gHyp_instance_error ( sInstance *pAI, char *status, char *fmt, ... )
{
  /* Process an error */
  
  char
    buf[MAX_OUTPUT_LENGTH+1] ;
  
  va_list
    ap ;
  
  int
    hypIndex,
    maxIndex ;
  
  sHyp
    *pHyp ;

  sCode
    *pCode ;


  if ( !pAI->signal.uERROR ) {

    /* Update the STATUS variable */
    if ( gHyp_frame_testGlobalFlag ( pAI->exec.pFrame, FRAME_GLOBAL_TRUE ) )
      gHyp_instance_setStatus ( pAI, status ) ;

    /* Create and display the error message */
    sprintf ( buf, "%s: error: ", status ) ; 
    va_start ( ap, fmt ) ;
    vsprintf ( buf+strlen(buf), fmt, ap ) ;
    va_end ( ap ) ;
    gHyp_util_log ( buf ) ;
    
    /* Output the trace of the token stream being parsed */
    pHyp = gHyp_frame_getHyp ( pAI->exec.pFrame ) ;
    hypIndex = gHyp_frame_getHypIndex ( pAI->exec.pFrame ) ;
    maxIndex = gHyp_hyp_getHypCount ( pHyp ) ;
    if ( maxIndex > 0 ) {
      pCode = gHyp_hyp_code ( pHyp, MIN ( hypIndex, maxIndex-1 ) ) ;
      gHyp_hyp_traceReport ( gHyp_hyp_line ( pCode ),gHyp_hyp_method ( pHyp ) ) ; 
    }
    pAI->signal.uERROR = 1 ;

    /* Keep track of the total number of unrelated fatal errors */
    guErrorCount++ ;
  }
 
  /* Release any data that would otherwise be leaked */
  if ( gpsTempData ) { 
    gHyp_data_delete ( gpsTempData ) ; 
    gpsTempData = NULL ; 
  }
  if ( gpsTempData2 ) { 
    gHyp_data_delete ( gpsTempData2 ) ; 
    gpsTempData2 = NULL ; 
  }

  /* Exit the current frame. */
  gHyp_frame_nextState ( pAI->exec.pFrame, G_STMT_EOS ) ;
  gHyp_frame_clearGlobalFlag ( pAI->exec.pFrame, FRAME_GLOBAL_TRUE ) ;

  /* Retreat */  
  if ( guDebugFlags & DEBUG_FRAME )
    gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_FRAME,
			 "frame: ERROR (longjmp to %d from frame %d)",
			 giJmpLevel, gHyp_frame_depth(pAI->exec.pFrame) );

  if ( giJmpLevel >= 0 ) longjmp ( gsJmpStack[giJmpLevel], COND_NORMAL ) ;

  return ;		    
}

void gHyp_instance_clearError ( sInstance *pAI )
{
  /* Process an error */
  pAI->signal.uERROR = 0 ;
}
  
static sLOGICAL lHyp_instance_handleError ( sInstance * pAI )
{
  sData
    *pMethodData ;

  if ( !pAI->signal.uERROR ) return FALSE ;

  pAI->signal.uERROR = 0  ;
  
  /* If no error handler, then terminate */
  if ( pAI->exec.handler[HANDLER_ERROR].hypIndex == -1 ) 
    longjmp ( gsJmpStack[0], COND_ERROR );

  if ( guDebugFlags & DEBUG_DIAGNOSTICS )
    gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
			 "diag : Handling error" ) ;

  /* Setup the error handler */
  gHyp_frame_newStmt (	pAI->exec.pFrame,
			STATEMENT_CALL,
			G_STMT_METHODCALL,
			G_PROGRAM_STMT ) ;
  pAI->exec.state = STATE_PARSE ;
  gHyp_frame_setGlobalFlag ( pAI->exec.pFrame, FRAME_GLOBAL_TRUE ) ;

  /* The STATUS variable receives the handler's return status */
  gHyp_frame_setMethodVariable ( 
    pAI->exec.pFrame,
    gHyp_frame_findRootVariable ( pAI->exec.pFrame, "STATUS" ) ) ;

  /* If local is set, then create local data */
  if ( !gHyp_frame_isGlobalScope ( pAI->exec.pFrame ) ) {
    pMethodData = gHyp_data_new ( "_error_handler_" ) ;
    gHyp_data_setHashed ( pMethodData, TRUE ) ;
    gHyp_frame_setMethodData ( pAI->exec.pFrame, pMethodData );
  }

  /* Set program counter to begin executing at the handler statement. */
  gHyp_frame_setHyp( pAI->exec.pFrame, 
		     pAI->exec.handler[HANDLER_ERROR].pHyp ) ;
  gHyp_frame_setHypIndex ( pAI->exec.pFrame, 
			   pAI->exec.handler[HANDLER_ERROR].hypIndex ) ;  
  gHyp_frame_setStatementIndex ( pAI->exec.pFrame, 
				 pAI->exec.handler[HANDLER_ERROR].hypIndex );  
  gHyp_frame_setGlobalFlag ( pAI->exec.pFrame, FRAME_GLOBAL_HANDLER ) ;

  /* Reset handler */
  pAI->exec.handler[HANDLER_ERROR].hypIndex = -1 ;
  
  return TRUE ;		    
}

static sLOGICAL lHyp_instance_handleMethod ( sInstance * pAI )
{
  sStack	
    *pStack = gHyp_frame_stack ( pAI->exec.pFrame ) ;
  
  sData
    *pData,
    *pArgs,
    *pMethodArgs,
    *pMethodData,
    *pMethodVariable,
    *pLvalue,
    *pValue,
    *pVariable,
    *pResult ;
  
  int
    lineCount,
    hypIndex=0,
    ss,
    n,
    context ;

  sHyp
    *pHyp ;

  sBYTE
    tokenType ;

  sLOGICAL
    isVector ;

  char
    value[VALUE_SIZE+1],
    *pStr,
    *pStream,
    *pMethodToken,
    *pVariableStr ;

  sMethod
    *pMethod = NULL ;

  if ( !pAI->signal.uMETHODCALL ) return FALSE ;
  pAI->signal.uMETHODCALL = 0  ;

  /* Pop the method arguments off the stack */
  pArgs = gHyp_stack_popRdata ( pStack, pAI ) ;

  /* Get the method name off the stack. */
  pMethodVariable = pData = gHyp_stack_popLvalue ( pStack, pAI )  ;
  pMethodToken = gHyp_data_getLabel ( pMethodVariable ) ;

  if ( guDebugFlags & DEBUG_FRAME )
    gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_FRAME,
			 "frame: Handling method call to '%s'", pMethodToken);
  
  pMethod = (sMethod*) gHyp_data_getObject ( pMethodVariable ) ;
  if ( !pMethod ) {

    /* Suppose it is ok to try and find a root method definition */
    pMethodVariable = gHyp_frame_findMethodVariable ( pAI->exec.pFrame,
						      pMethodToken,
						      pAI ) ;
    if ( !pMethodVariable ) {
      pAI->exec.state = STATE_PARSE ;
      gHyp_instance_error ( pAI, STATUS_METHOD, 
			    "Method was not found for variable '%s'",
			    pMethodToken ) ;
    }
    pMethod = (sMethod*) gHyp_data_getObject ( pMethodVariable ) ;
				
    if ( !pMethod ) {
      pAI->exec.state = STATE_PARSE ;
      gHyp_instance_error ( pAI, STATUS_UNDEFINED, 
			    "Method was not defined for variable '%s'",
			    pMethodToken ) ;
    }
  }

  /* Setup the method call */
  gHyp_frame_newStmt (	pAI->exec.pFrame,
			STATEMENT_CALL,
			G_STMT_METHODCALL,
			G_PROGRAM_STMT ) ;

  pAI->exec.state = STATE_PARSE ;
  gHyp_frame_setGlobalFlag ( pAI->exec.pFrame, FRAME_GLOBAL_TRUE ) ;
  
  /* Set the data method to the variable that holds the pointer
   * to the method code. 
   */
  pData = gHyp_data_getReference ( pMethodVariable ) ;
  if ( pData ) pMethodVariable = pData  ;
  gHyp_frame_setMethodVariable ( pAI->exec.pFrame, pMethodVariable ) ;

  /* If local is set, then create local data */
  if ( !gHyp_frame_isGlobalScope ( pAI->exec.pFrame ) ) {
    pMethodData = gHyp_data_new ( pMethodToken ) ;
    /*
    gHyp_util_logInfo("%p(%s)new-methoddata",pMethodData,pMethodToken);
    */
    gHyp_data_setHashed ( pMethodData, TRUE ) ;
    gHyp_frame_setMethodData ( pAI->exec.pFrame, pMethodData );
  }
 
  /* Set program counter to begin executing at the method statement. */
  gHyp_frame_clearGlobalFlag ( pAI->exec.pFrame, FRAME_GLOBAL_HANDLER ) ;  
  gHyp_frame_clearGlobalFlag ( pAI->exec.pFrame, FRAME_GLOBAL_MESSAGE ) ;
  
  pHyp = gHyp_method_getHyp ( pMethod ) ;    
  gHyp_frame_setHyp ( pAI->exec.pFrame, pHyp ) ;
  gHyp_frame_setHypIndex ( pAI->exec.pFrame, 0 ) ;
  gHyp_frame_setStatementIndex ( pAI->exec.pFrame, 0 ) ; 

  /* Initialize the arguments. For each argument defined, create
   * the variable and insert the passed value.
   */
  pMethodArgs = gHyp_method_getArgs ( pMethod ) ;

  /* Merge the arguments into the defined method args */
  pResult = gHyp_env_mergeData ( pMethodArgs, pArgs, pAI, 0, FALSE, FALSE, FALSE, FALSE, NULL, 0 ) ;

  /* The merged arguments in 'pResult' can be integrated into the method */
  pValue = NULL ;
  ss = -1 ;
  context = -1 ;
  pLvalue = gHyp_data_new ( NULL ) ;
  isVector = (gHyp_data_getDataType( pResult ) > TYPE_STRING ) ;
  lineCount = 0 ;

  /* Create or retrieve the "args" variable, and clear it of any values. */
  pArgs = gHyp_frame_createVariable ( pAI, pAI->exec.pFrame, "args" ) ;
  gHyp_data_deleteValues ( pArgs ) ;

  /* Get next defined method argument */
  while ((pValue = gHyp_data_nextValue ( pResult, 
					 pValue, 
					 &context,
					 ss ) ) ) {

    tokenType = gHyp_data_tokenType ( pValue ) ;

    if ( tokenType == TOKEN_VARIABLE ) {

      /* Create a new variable or overwrite an existing variable. */
      pVariableStr = gHyp_data_getLabel ( pValue ) ;

      if ( strcmp ( pVariableStr, "STATUS" ) == 0 )
	/* Special case: STATUS must be set as a global variable */
	pVariable = gHyp_frame_findRootVariable ( pAI->exec.pFrame, 
						  "STATUS" ) ;
      else 
	pVariable = gHyp_frame_createVariable ( pAI, pAI->exec.pFrame, 
						pVariableStr ) ;

      gHyp_data_setReference ( pLvalue, pVariableStr, pVariable);
      pData = gHyp_type_assign ( pAI,
				 pAI->exec.pFrame,
				 pLvalue,
				 pValue,
				 gHyp_data_dataType ( pValue ),
				 FALSE, FALSE  ) ;
      gHyp_data_delete ( pData ) ;

      /* Store a reference to the variable in the "args" variable */
      pData = gHyp_data_new ( NULL ) ;
      gHyp_data_setReference ( pData, pVariableStr, NULL ) ;
      gHyp_data_append ( pArgs, pData ) ;
    }
    else {

      /* Get the end of the hyp area for the method */
      hypIndex = gHyp_hyp_getHypCount ( pHyp ) ;

      if ( lineCount == 0 ) {

	/* Prefix a "{" on the end of the data to dereference */
	pStream = gHyp_load_fromStream ( pAI, pHyp, "{", lineCount++ ) ;
	if ( !pStream || *pStream ) {
	  gHyp_hyp_setHypCount ( pHyp, hypIndex ) ;
	  pAI->exec.state = STATE_PARSE ;
	  gHyp_instance_error ( pAI,
				STATUS_UNDEFINED,
				"Failed to load HyperScript segment (method) '{'" ) ;
	}
      }

      /* Get the value, as a string */
      n = gHyp_data_getStr ( pValue, 
	 		     value, 
			     VALUE_SIZE, 
			     context, 
			     isVector ) ;
      pStr = value ;

      /* Add the code segment to the stream */
      pStream = gHyp_load_fromStream ( pAI, pHyp, pStr, lineCount++ ) ;
      if ( !pStream || *pStream ) {
	gHyp_hyp_setHypCount ( pHyp, hypIndex ) ;
	pAI->exec.state = STATE_PARSE ;
	gHyp_instance_error ( pAI,
			      STATUS_UNDEFINED,
			      "Failed to load HyperScript segment (method) '%s'",
			      pStr ) ;
      }

      /* Always add an extra ";" */
      pStream = gHyp_load_fromStream ( pAI, pHyp, ";", lineCount++ ) ;
      if ( !pStream || *pStream ) {
	gHyp_hyp_setHypCount ( pHyp, hypIndex ) ;
	pAI->exec.state = STATE_PARSE ;
	gHyp_instance_error ( pAI,
			      STATUS_UNDEFINED,
			      "Failed to load HyperScript segment (method) ';'" ) ;
      }
    }
  }
  if ( context== -2 && ss != -1 ) {
    gHyp_data_delete ( pResult ) ; 
    pAI->exec.state = STATE_PARSE ;
    gHyp_instance_error ( pAI, STATUS_BOUNDS, 
			  "Subscript '%d' is out of bounds in dereference argument",ss) ;
  }

  if ( lineCount > 0 ) {
    /* Add the final "}" */
    pStream = gHyp_load_fromStream ( pAI, pHyp, "}", lineCount++ ) ;
    if ( !pStream || *pStream ) {
      gHyp_hyp_setHypCount ( pHyp, hypIndex ) ;
      pAI->exec.state = STATE_PARSE ;
      gHyp_instance_error ( pAI,
			    STATUS_UNDEFINED,
			    "Failed to load HyperScript segment (method) '}'" ) ;
    }
    gHyp_frame_setGlobalFlag ( pAI->exec.pFrame, FRAME_GLOBAL_MSGARGS ) ;
    /*gHyp_util_debug("Deref from method");*/
    gHyp_instance_setDerefHandler ( pAI, 
				    hypIndex, 
				    pHyp ) ;
  }

  /* Delete the merged data and the working variable */
  gHyp_data_delete ( pResult ) ;
  gHyp_data_delete ( pLvalue ) ;
  
  return TRUE ;		    
}

static sLOGICAL lHyp_instance_handleDereference ( sInstance * pAI )
{    
  char segment[41] ;

  if ( !pAI->signal.uDEREFERENCE ) return FALSE ;
  pAI->signal.uDEREFERENCE = 0  ;

  /* Setup the dereference call */
  gHyp_frame_newStmt (	pAI->exec.pFrame,
			STATEMENT_DEREFERENCE,
			G_STMT_DEREFERENCE,
			G_PROGRAM_STMT ) ;

  gHyp_frame_setGlobalFlag ( pAI->exec.pFrame, FRAME_GLOBAL_DEREFERENCE ) ;
  pAI->exec.state = STATE_PARSE ;
  gHyp_frame_setGlobalFlag ( pAI->exec.pFrame, FRAME_GLOBAL_TRUE ) ;

 
  if ( guDebugFlags & DEBUG_DIAGNOSTICS ) {
    gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
			 "diag : Handling dereference : %s",
			  gHyp_hyp_describe2 ( 
			  pAI->exec.handler[HANDLER_DEREFERENCE].pHyp,
			  pAI->exec.handler[HANDLER_DEREFERENCE].hypIndex,
			  segment, 40 ) ) ;
  }
  

  /* Set program counter to begin executing at the dereference statement. */
  gHyp_frame_setHyp ( pAI->exec.pFrame, 
		      pAI->exec.handler[HANDLER_DEREFERENCE].pHyp ) ;
  gHyp_frame_setHypIndex ( pAI->exec.pFrame, 
			   pAI->exec.handler[HANDLER_DEREFERENCE].hypIndex ) ;
  gHyp_frame_setStatementIndex ( 
    pAI->exec.pFrame, 
    pAI->exec.handler[HANDLER_DEREFERENCE].hypIndex ) ;
 
  /* Reset handler */
  pAI->exec.handler[HANDLER_DEREFERENCE].hypIndex = -1 ;
 
  return TRUE ;		    
}

static sLOGICAL lHyp_instance_handleAlarm ( sInstance *pAI ) 
{
  sData
    *pMethodData ;

  sConcept
    *pConcept ;

  /* Determine what event type triggered the alarm signal.
   * Set the status variable to the event type.
   * If a handler was defined for the event type, call it.
   */
  
  if ( pAI->signal.uALRM ) {

    pAI->signal.uALRM = 0 ;
#ifdef SIGALRM
    /* Since longjmp is used in lHyp_instance_intHandler rather than
     * than siglongjmp, the signal mask for the process may still have 
     * SIGALRM blocked.  Unblock it so we can re-use the handler.
     */
    alarm(0) ;
    gHyp_signal_unblock ( SIGALRM ) ;
 #endif
  }
  
  gsCurTime = time ( NULL ) ;
  if ( pAI->exec.eventTime > gsCurTime ) return FALSE ;
  
  /*gHyp_util_debug("NEXT EVENT is past %d seconds",gsCurTime-pAI->exec.eventTime );*/

  if ( guDebugFlags & DEBUG_DIAGNOSTICS )
    gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
			 "diag : Handling alarm");
  
  if ( pAI->exec.eventType & EVENT_DEATH ) {
    
    gHyp_instance_warning ( pAI, STATUS_DEATH, "Death condition at %s",gHyp_util_timeStamp ( gsCurTime ) ) ;
    
    if ( pAI->exec.handler[HANDLER_DEATH].hypIndex == -1 ) {
      if ( guDebugFlags & DEBUG_FRAME )
	gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_FRAME,
			     "frame: DEATH (longjmp to 0 from frame %d)",
			     gHyp_frame_depth(pAI->exec.pFrame) ); 
      longjmp ( gsJmpStack[0], COND_ERROR );
    }
    
    /* Allow a few more seconds - enough to get through the handler. */
    gsCurTime = time ( NULL ) ; 
    pAI->exec.deathTime = gsCurTime + DEATH_GRACE_PERIOD ;
    gHyp_instance_nextEvent ( pAI ) ;
    
    gHyp_frame_newStmt ( pAI->exec.pFrame,
			 STATEMENT_CALL,
			 G_STMT_METHODCALL,
			 G_PROGRAM_STMT ) ;
    
    pAI->exec.state = STATE_PARSE ;
    gHyp_frame_setGlobalFlag ( pAI->exec.pFrame, FRAME_GLOBAL_TRUE ) ;
    
    /* The STATUS variable receives the handler's return status */
    gHyp_frame_setMethodVariable (pAI->exec.pFrame,
				  gHyp_frame_findRootVariable ( pAI->exec.pFrame, "STATUS" ) ) ;
    
    /* If local is set, then create local data */
    if ( !gHyp_frame_isGlobalScope ( pAI->exec.pFrame ) ) {
      pMethodData = gHyp_data_new ( "_death_handler_" ) ;
      gHyp_data_setHashed ( pMethodData, TRUE ) ;
      gHyp_frame_setMethodData ( pAI->exec.pFrame, pMethodData );
    }
    
    /* Set program counter to begin executing at the handler statement. */
    gHyp_frame_setHyp ( pAI->exec.pFrame, 
			pAI->exec.handler[HANDLER_DEATH].pHyp ) ;
    gHyp_frame_setHypIndex ( pAI->exec.pFrame, 
			     pAI->exec.handler[HANDLER_DEATH].hypIndex ) ;
    
    gHyp_frame_setStatementIndex ( pAI->exec.pFrame, 
				   pAI->exec.handler[HANDLER_DEATH].hypIndex ) ;
    
    gHyp_frame_setGlobalFlag ( pAI->exec.pFrame, FRAME_GLOBAL_HANDLER ) ;
    
    /* Reset handler */
    pAI->exec.handler[HANDLER_DEATH].hypIndex = -1 ;

  }
  else if ( pAI->exec.eventType & EVENT_TIMEOUT ) {
    
    gHyp_instance_warning ( pAI, STATUS_TIMEDOUT,"Timeout condition at %s",gHyp_util_timeStamp ( gsCurTime )  ) ;
    
    /* Re-establish timer. */
    gsCurTime = time ( NULL ) ;
    gHyp_instance_setTimeOut ( pAI ) ;
    
    /* If handler is not defined then kill instance */
    if ( pAI->exec.handler[HANDLER_TIMEOUT].hypIndex == -1 ) {
      if ( guDebugFlags & DEBUG_FRAME )
	gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_FRAME,
			     "frame: TIMEOUT (longjmp to 0 from frame %d)",
			     gHyp_frame_depth(pAI->exec.pFrame ));
      longjmp ( gsJmpStack[0],COND_ERROR);
    }
    
    gHyp_frame_newStmt ( pAI->exec.pFrame,
			 STATEMENT_CALL,
			 G_STMT_METHODCALL,
			 G_PROGRAM_STMT ) ;
    
    pAI->exec.state = STATE_PARSE ;
    gHyp_frame_setGlobalFlag ( pAI->exec.pFrame, FRAME_GLOBAL_TRUE ) ;
    
    /* The STATUS variable receives the handler's return status */
    gHyp_frame_setMethodVariable ( pAI->exec.pFrame,
				  gHyp_frame_findRootVariable ( pAI->exec.pFrame, "STATUS" ) ) ;
    
    /* If local is set, then create local data */
    if ( !gHyp_frame_isGlobalScope ( pAI->exec.pFrame ) ) {
      pMethodData = gHyp_data_new ( "_handler_" ) ;
      gHyp_data_setHashed ( pMethodData, TRUE ) ;
      gHyp_frame_setMethodData ( pAI->exec.pFrame, pMethodData );
    }
    
    /* Set program counter to begin executing at the handler statement. */
    gHyp_frame_setHyp ( pAI->exec.pFrame, 
			pAI->exec.handler[HANDLER_TIMEOUT].pHyp ) ;
    gHyp_frame_setHypIndex ( pAI->exec.pFrame, 
			     pAI->exec.handler[HANDLER_TIMEOUT].hypIndex ) ;
    gHyp_frame_setStatementIndex ( pAI->exec.pFrame,  
				   pAI->exec.handler[HANDLER_TIMEOUT].hypIndex ) ;
    
    gHyp_frame_setGlobalFlag ( pAI->exec.pFrame, FRAME_GLOBAL_HANDLER ) ;
    
    /* Reset handler */
    pAI->exec.handler[HANDLER_TIMEOUT].hypIndex = -1 ;
    
  }
  else if ( pAI->exec.eventType & EVENT_ALARM ) {
    
    gHyp_instance_warning ( pAI, STATUS_ALARM, "Alarm condition at %s",gHyp_util_timeStamp ( gsCurTime )  ) ;

    pAI->exec.alarmTime = 0 ;
    
    /* Re-establish timer. */
    gsCurTime = time ( NULL ) ; 
    gHyp_instance_nextEvent ( pAI ) ;
    
    /* If interactive mode, let alarm pass, otherwise kill instance */
    if ( pAI->exec.handler[HANDLER_ALARM].hypIndex == -1 ) {
      if ( guDebugFlags & DEBUG_FRAME )
	gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_FRAME,
			     "frame: ALARM (longjmp to 0 from frame %d)",
			     gHyp_frame_depth(pAI->exec.pFrame) );
      longjmp ( gsJmpStack[0], COND_ERROR );
    }
    /*gHyp_frame_dumpLevel ( pAI->exec.pFrame ) ;*/
    /*gHyp_util_debug("Frame statement %d is %s/%s",
		    gHyp_frame_statementType(pAI->exec.pFrame),
		    gHyp_frame_expectedStateStr ( pAI->exec.pFrame ),
		    gHyp_frame_currentStateStr ( pAI->exec.pFrame ) ) ;
    */
    gHyp_frame_newStmt (	pAI->exec.pFrame,
				STATEMENT_CALL,
				G_STMT_METHODCALL,
				G_PROGRAM_STMT ) ;
    
    pAI->exec.state = STATE_PARSE ;
    gHyp_frame_setGlobalFlag ( pAI->exec.pFrame, FRAME_GLOBAL_TRUE ) ;
    
    /* The STATUS variable receives the handler's return status */
    gHyp_frame_setMethodVariable ( 
				  pAI->exec.pFrame,
				  gHyp_frame_findRootVariable ( pAI->exec.pFrame, "STATUS" ) ) ;
    
    /* If local is set, then create local data */
    if ( !gHyp_frame_isGlobalScope ( pAI->exec.pFrame ) ) {
      pMethodData = gHyp_data_new ( "_alarm_handler_" ) ;
      gHyp_data_setHashed ( pMethodData, TRUE ) ;
      gHyp_frame_setMethodData ( pAI->exec.pFrame, pMethodData );
    }
    
    /* Set program counter to begin executing at the handler statement. */
    gHyp_frame_setHyp ( pAI->exec.pFrame, 
			pAI->exec.handler[HANDLER_ALARM].pHyp ) ;
    gHyp_frame_setHypIndex ( pAI->exec.pFrame, 
			     pAI->exec.handler[HANDLER_ALARM].hypIndex ) ;
    
    gHyp_frame_setStatementIndex ( pAI->exec.pFrame, 
				   pAI->exec.handler[HANDLER_ALARM].hypIndex ) ; 
    
    gHyp_frame_setGlobalFlag ( pAI->exec.pFrame, FRAME_GLOBAL_HANDLER ) ;
    
    /* Reset handler */
    pAI->exec.handler[HANDLER_ALARM].hypIndex = -1 ;
    
  }
  else if ( pAI->exec.eventType & EVENT_WAKEUP ) {

    if ( guDebugFlags & DEBUG_DIAGNOSTICS )
      gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
			   "Event: Wakeup, at level %d", gHyp_frame_depth ( pAI->exec.pFrame ) ) ;
    gsCurTime = time ( NULL ) ;
    pAI->exec.wakeTime = 0 ;
    gHyp_instance_nextEvent ( pAI ) ;
    gHyp_instance_setStatus ( pAI, STATUS_ACKNOWLEDGE ) ;

    /*gHyp_util_debug( "WAKEUP" );*/

    /* Fall out of sleep() */
    if ( pAI->exec.state == STATE_SLEEP ) {
      if ( guDebugFlags & DEBUG_DIAGNOSTICS )
        gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,"Fall out of sleep" );
      pAI->exec.state = STATE_PARSE ;
      gHyp_instance_pushSTATUS ( pAI, gHyp_frame_stack ( pAI->exec.pFrame ) ) ;
      pConcept = gHyp_instance_getConcept(pAI) ;
      if ( gHyp_concept_getConceptInstance ( pConcept ) == pAI && gHyp_instance_isEND ( pAI ) ) 
        gHyp_concept_setReturnToStdIn ( pConcept,TRUE ) ;
    }

    /* Not really a condition, it doesn't execute a handler */
    return FALSE ;

  }
  else if ( pAI->exec.eventType & EVENT_HEARTBEAT ) {

    if ( guDebugFlags & DEBUG_DIAGNOSTICS )
      gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
			   "Event: Heartbeat" ) ;
    gsCurTime = time ( NULL ) ;
    pAI->exec.beatTime = gsCurTime + HEARTBEAT_INTERVAL ;
    gHyp_instance_nextEvent ( pAI ) ;
    gHyp_instance_setStatus ( pAI, STATUS_ACKNOWLEDGE ) ;

    /* Not really a condition, it doesn't execute a handler */
    return FALSE ;
  }
    
  return TRUE ;  
}  

static sLOGICAL lHyp_instance_handleInterrupt ( sInstance *pAI )
{
  sData
    *pMethodData ;

  if ( pAI->signal.uINT ) {

    pAI->signal.uINT = 0 ;
#ifdef SIGINT
    /* Since longjmp is used in lHyp_instance_intHandler rather than
     * than siglongjmp, the signal mask for the process may still have 
     * SIGINT blocked.  Unblock it so we can re-use the handler.
     */
    gHyp_signal_unblock ( SIGINT ) ;
#endif
  }
  else 
    return FALSE ;

  if ( pAI->exec.handler[HANDLER_INTERRUPT].hypIndex == -1 ) {
    if ( guDebugFlags & DEBUG_FRAME )
      gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_FRAME,
			   "frame: INTERRUPT (longjmp to 0 from frame %d)",
			   gHyp_frame_depth(pAI->exec.pFrame) );
    longjmp ( gsJmpStack[0], COND_FATAL);
  }

  if ( guDebugFlags & DEBUG_DIAGNOSTICS )
    gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
			 "diag : Handling interrupt");

  gHyp_frame_newStmt (	pAI->exec.pFrame,
			STATEMENT_CALL,
			G_STMT_METHODCALL,
			G_PROGRAM_STMT ) ;
      
  pAI->exec.state = STATE_PARSE ;
  gHyp_frame_setGlobalFlag ( pAI->exec.pFrame, FRAME_GLOBAL_TRUE ) ;
  
  /* The STATUS variable receives the handler's return status */
  gHyp_frame_setMethodVariable ( 
    pAI->exec.pFrame,
    gHyp_frame_findRootVariable ( pAI->exec.pFrame, "STATUS" ) ) ;

  /* If local is set, then create local data */
  if ( !gHyp_frame_isGlobalScope ( pAI->exec.pFrame ) ) {
    pMethodData = gHyp_data_new ( "_interrupt_handler_" ) ;
    gHyp_data_setHashed ( pMethodData, TRUE ) ;
    gHyp_frame_setMethodData ( pAI->exec.pFrame, pMethodData );
  }

  /* Set program counter to begin executing at the handler statement. */
  gHyp_frame_setGlobalFlag ( pAI->exec.pFrame, FRAME_GLOBAL_HANDLER ) ;
  gHyp_frame_setHyp ( pAI->exec.pFrame, 
		      pAI->exec.handler[HANDLER_INTERRUPT].pHyp ) ;
  gHyp_frame_setHypIndex ( pAI->exec.pFrame, 
			   pAI->exec.handler[HANDLER_INTERRUPT].hypIndex ) ;
  gHyp_frame_setStatementIndex ( 
    pAI->exec.pFrame, 
    pAI->exec.handler[HANDLER_INTERRUPT].hypIndex ) ;   

  /* Reset handler */
  pAI->exec.handler[HANDLER_INTERRUPT].hypIndex = -1 ;

      
  return TRUE ;
}

static sLOGICAL lHyp_instance_handleHangup ( sInstance *pAI )
{
  sData
    *pData,
    *pValue,
    *pMethodData ;

  char
    status[VALUE_SIZE+1] ;

  int
    i,
    n ;

  if ( pAI->signal.uHUP ) {

    pAI->signal.numHup-- ;
    n = pAI->signal.numHup ;
    if ( n < 0 ) return FALSE ;
    /*
    *gHyp_util_debug("HUP at [%d] s=%d x=%d id=%d",n,pAI->signal.hupArg[0], 
	      pAI->signal.hupArg2[0], 
	      pAI->signal.hupArg3[0] ) ;
    */
    sprintf ( status, "%s %d %d %d", STATUS_HANGUP, 
	      pAI->signal.hupArg[0], 
	      pAI->signal.hupArg2[0], 
	      pAI->signal.hupArg3[0] ) ; 
    gHyp_instance_warning ( pAI,status,"Hangup signal for %s", pAI->msg.targetId );

    /* If no signals left, turn off the flag */
    if ( n == 0 ) pAI->signal.uHUP = 0 ;

    for ( i=0; i<n; i++ ) {
      pAI->signal.hupArg[i] = pAI->signal.hupArg[i+1] ;
      pAI->signal.hupArg2[i] = pAI->signal.hupArg2[i+1] ;
      pAI->signal.hupArg3[i] = pAI->signal.hupArg3[i+1] ;
    }

    pData = gHyp_frame_createRootVariable ( gHyp_instance_frame ( pAI ), "_hangup_status_" ) ;
    gHyp_data_deleteValues ( pData ) ;
    pValue = gHyp_data_new ( NULL ) ;
    gHyp_data_setStr ( pValue, status ) ;
    gHyp_data_append ( pData, pValue ) ;

#ifdef SIGHUP
    /* Since longjmp is used in lHyp_instance_intHandler rather than
     * than siglongjmp, the signal mask for the process may still have 
     * SIGHUP blocked.  Unblock it so we can re-use the handler.
     */
    gHyp_signal_unblock ( SIGHUP ) ;
#endif
  }
  else 
    return FALSE ;

  if ( pAI->exec.handler[HANDLER_HANGUP].hypIndex == -1 ) {
    if ( guDebugFlags & DEBUG_FRAME )
      gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_FRAME,
			   "frame: HANGUP (longjmp to 0 from frame %d)",
			   gHyp_frame_depth(pAI->exec.pFrame) );
    longjmp ( gsJmpStack[0], COND_FATAL);
  }

  if ( guDebugFlags & DEBUG_DIAGNOSTICS )
    gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
			 "diag : Handling hangup");

  gHyp_frame_newStmt (	pAI->exec.pFrame,
			STATEMENT_CALL,
			G_STMT_METHODCALL,
			G_PROGRAM_STMT ) ;
      
  pAI->exec.state = STATE_PARSE ;
  gHyp_frame_setGlobalFlag ( pAI->exec.pFrame, FRAME_GLOBAL_TRUE ) ;
  
  /* The STATUS variable receives the handler's return status */
  gHyp_frame_setMethodVariable ( 
    pAI->exec.pFrame,
    gHyp_frame_findRootVariable ( pAI->exec.pFrame, "STATUS" ) ) ;

  /* If local is set, then create local data */
  if ( !gHyp_frame_isGlobalScope ( pAI->exec.pFrame ) ) {
    pMethodData = gHyp_data_new ( "_hangup_handler_" ) ;
    gHyp_data_setHashed ( pMethodData, TRUE ) ;
    gHyp_frame_setMethodData ( pAI->exec.pFrame, pMethodData );
  }

  /* Set program counter to begin executing at the handler statement. */
  gHyp_frame_setGlobalFlag ( pAI->exec.pFrame, FRAME_GLOBAL_HANDLER ) ;
  gHyp_frame_setHyp ( pAI->exec.pFrame, 
		      pAI->exec.handler[HANDLER_HANGUP].pHyp ) ;
  gHyp_frame_setHypIndex ( pAI->exec.pFrame, 
			   pAI->exec.handler[HANDLER_HANGUP].hypIndex ) ;
  gHyp_frame_setStatementIndex ( 
    pAI->exec.pFrame, 
    pAI->exec.handler[HANDLER_HANGUP].hypIndex ) ;   

  /* Reset handler - but only if there are no more hangup signals stacked */
  if ( pAI->signal.uHUP == 0 ) pAI->exec.handler[HANDLER_HANGUP].hypIndex = -1 ;

      
  return TRUE ;
}

static sLOGICAL lHyp_instance_handleConnect ( sInstance *pAI )
{

  sData
    *pData,
    *pValue,
    *pMethodData ;

  char
    status[VALUE_SIZE+1] ;

  int
    i,
    n ;

  if ( pAI->signal.uCONNECT ) {

    pAI->signal.numCon-- ;
    n = pAI->signal.numCon ;
    if ( n < 0 ) return FALSE ;
    /*
    *gHyp_util_debug("CON at [%d] s=%d x=%d id=%d",
              n,
              pAI->signal.conArg[0], 
	      pAI->signal.conArg2[0], 
	      pAI->signal.conArg3[0] ) ;
    */
    sprintf ( status, "%s %d %d %d", STATUS_CONNECT, 
	      pAI->signal.conArg[0], 
	      pAI->signal.conArg2[0], 
	      pAI->signal.conArg3[0] ) ; 
    gHyp_instance_warning ( pAI,status,"Incoming connection signal for %s", pAI->msg.targetId );
    /* If no signals left, turn off the flag */
    if ( n == 0 ) pAI->signal.uCONNECT = 0 ;

    for ( i=0; i<n; i++ ) {
      pAI->signal.conArg[i] = pAI->signal.conArg[i+1] ;
      pAI->signal.conArg2[i] = pAI->signal.conArg2[i+1] ;
      pAI->signal.conArg3[i] = pAI->signal.conArg3[i+1] ;
    }
    pData = gHyp_frame_createRootVariable ( gHyp_instance_frame ( pAI ), "_connect_status_" ) ;
    gHyp_data_deleteValues ( pData ) ;
    pValue = gHyp_data_new ( NULL ) ;
    gHyp_data_setStr ( pValue, status ) ;
    gHyp_data_append ( pData, pValue ) ;

  }
  else 
    return FALSE ;

  if ( pAI->exec.handler[HANDLER_CONNECT].hypIndex == -1 )  return FALSE ;

  if ( guDebugFlags & DEBUG_DIAGNOSTICS )
    gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
			 "diag : Handling incoming connection");

  gHyp_frame_newStmt (	pAI->exec.pFrame,
			STATEMENT_CALL,
			G_STMT_METHODCALL,
			G_PROGRAM_STMT ) ;
      
  pAI->exec.state = STATE_PARSE ;
  gHyp_frame_setGlobalFlag ( pAI->exec.pFrame, FRAME_GLOBAL_TRUE ) ;
  
  /* The STATUS variable receives the handler's return status */
  gHyp_frame_setMethodVariable ( 
    pAI->exec.pFrame,
    gHyp_frame_findRootVariable ( pAI->exec.pFrame, "STATUS" ) ) ;

  /* If local is set, then create local data */
  if ( !gHyp_frame_isGlobalScope ( pAI->exec.pFrame ) ) {
    pMethodData = gHyp_data_new ( "_connect_handler_" ) ;
    gHyp_data_setHashed ( pMethodData, TRUE ) ;
    gHyp_frame_setMethodData ( pAI->exec.pFrame, pMethodData );
  }

  /* Set program counter to begin executing at the handler statement. */
  gHyp_frame_setGlobalFlag ( pAI->exec.pFrame, FRAME_GLOBAL_HANDLER ) ;
  gHyp_frame_setHyp ( pAI->exec.pFrame, 
		      pAI->exec.handler[HANDLER_CONNECT].pHyp ) ;
  gHyp_frame_setHypIndex ( pAI->exec.pFrame, 
			   pAI->exec.handler[HANDLER_CONNECT].hypIndex ) ;
  gHyp_frame_setStatementIndex ( 
    pAI->exec.pFrame, 
    pAI->exec.handler[HANDLER_CONNECT].hypIndex ) ;   

  /* Reset handler */
  pAI->exec.handler[HANDLER_CONNECT].hypIndex = -1 ;

      
  return TRUE ;
}

static sLOGICAL lHyp_instance_handlePipe ( sInstance *pAI )
{
  sData
    *pData,
    *pValue,
    *pMethodData ;

  char
    status[VALUE_SIZE+1] ;

  int
    i,
    n ;

  if ( pAI->signal.uPIPE ) {
    /* SIGPIPE was received. */


    /* If already in a handler, cannot invoke another one 
    if ( gHyp_frame_testGlobalFlag ( pAI->exec.pFrame, FRAME_GLOBAL_HANDLER ) ) return FALSE ;
    */

    pAI->signal.numPipe-- ;
    n = pAI->signal.numPipe ;
    if ( n < 0 ) return FALSE ;

    sprintf ( status, "%s %d %d %d", STATUS_PIPE, 
	      pAI->signal.pipeArg[0], 
	      pAI->signal.pipeArg2[0], 
	      pAI->signal.pipeArg3[0] ) ; 
    gHyp_instance_warning ( pAI,status,"Pipe signal for %s", pAI->msg.targetId );
    /* If no signals left, turn off the flag */
    if ( n == 0 ) pAI->signal.uPIPE = 0 ;
    for ( i=0; i<n; i++ ) {
      pAI->signal.pipeArg[i] = pAI->signal.pipeArg[i+1] ;
      pAI->signal.pipeArg2[i] = pAI->signal.pipeArg2[i+1] ;
      pAI->signal.pipeArg3[i] = pAI->signal.pipeArg3[i+1] ;
    }
    pData = gHyp_frame_createRootVariable ( gHyp_instance_frame ( pAI ), "_pipe_status_" ) ;
    gHyp_data_deleteValues ( pData ) ;
    pValue = gHyp_data_new ( NULL ) ;
    gHyp_data_setStr ( pValue, status ) ;
    gHyp_data_append ( pData, pValue ) ;

#ifdef SIGPIPE
    /* Since longjmp is used in lHyp_instance_pipeHandler rather than
     * than siglongjmp, the signal mask for the process may still have 
     * SIGPIPE blocked.  Unblock it so we can re-use the handler.
     */
    gHyp_signal_unblock ( SIGPIPE ) ;
#endif
  }
  else 
    return FALSE ;

  if ( pAI->exec.handler[HANDLER_PIPE].hypIndex == -1 ) { 
    if ( guDebugFlags & DEBUG_FRAME )
      gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_FRAME,
			   "frame: PIPE (longjmp to 0 from frame %d)",
			   gHyp_frame_depth(pAI->exec.pFrame) );

    longjmp ( gsJmpStack[0], COND_FATAL);
  }

  if ( guDebugFlags & DEBUG_DIAGNOSTICS )
    gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
			 "diag : Handling pipe");

  gHyp_frame_newStmt (	pAI->exec.pFrame,
			STATEMENT_CALL,
			G_STMT_METHODCALL,
			G_PROGRAM_STMT ) ;
      
  pAI->exec.state = STATE_PARSE ;
  gHyp_frame_setGlobalFlag ( pAI->exec.pFrame, FRAME_GLOBAL_TRUE ) ;
  
  /* The STATUS variable receives the handler's return status */
  gHyp_frame_setMethodVariable ( 
    pAI->exec.pFrame,
    gHyp_frame_findRootVariable ( pAI->exec.pFrame, "STATUS" ) ) ;

  /* If local is set, then create local data */
  if ( !gHyp_frame_isGlobalScope ( pAI->exec.pFrame ) ) {
    pMethodData = gHyp_data_new ( "_pipe_handler_" ) ;
    gHyp_data_setHashed ( pMethodData, TRUE ) ;
    gHyp_frame_setMethodData ( pAI->exec.pFrame, pMethodData );
  }

  /* Set program counter to begin executing at the handler statement. */
  gHyp_frame_setGlobalFlag ( pAI->exec.pFrame, FRAME_GLOBAL_HANDLER ) ;
  gHyp_frame_setHyp ( pAI->exec.pFrame,
		      pAI->exec.handler[HANDLER_PIPE].pHyp ) ;
  gHyp_frame_setHypIndex ( pAI->exec.pFrame,
			   pAI->exec.handler[HANDLER_PIPE].hypIndex ) ;

  gHyp_frame_setStatementIndex ( pAI->exec.pFrame, 
				 pAI->exec.handler[HANDLER_PIPE].hypIndex ) ;

  /* Reset handler - but only if there are no more pipe signals stacked */
  if ( pAI->signal.uPIPE == 0 ) pAI->exec.handler[HANDLER_PIPE].hypIndex = -1 ;
      
  return TRUE ;
}

static sLOGICAL lHyp_instance_handleMessageInt ( sInstance *pAI )
{
  /* Handle a interrupting message.  The handler can decide to 
   * keep the message and invoke the corresponding method or it can
   * throw the message away.  A TRUE return value in STATUS keeps the
   * message.  A FALSE return value in STATUS throws the message away.
   */
  sData
    *pMethodData;
  /*gHyp_util_debug("Checking %d interrupt %d",pAI->signal.uMSGINTERRUPT,pAI->exec.handler[HANDLER_MESSAGE].hypIndex);*/

  if ( !pAI->signal.uMSGINTERRUPT ) return FALSE ;

  pAI->signal.uMSGINTERRUPT = 0  ;

  if ( pAI->exec.handler[HANDLER_MESSAGE].hypIndex == -1 ) return FALSE ;

  if ( guDebugFlags & DEBUG_DIAGNOSTICS )
    gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
			 "diag : Handling msg interrupt");

  gHyp_frame_newStmt (	pAI->exec.pFrame,
			STATEMENT_CALL,
			G_STMT_METHODCALL,
			G_PROGRAM_STMT ) ;
  
  pAI->exec.state = STATE_PARSE ;
  gHyp_frame_setGlobalFlag ( pAI->exec.pFrame, FRAME_GLOBAL_TRUE ) ;
  
  /* The STATUS variable receives the handler's return status,
   * ( see gHyp_stmt_return ).
   */
  gHyp_frame_setMethodVariable ( 
    pAI->exec.pFrame,
    gHyp_frame_findRootVariable ( pAI->exec.pFrame, "STATUS" ) ) ;

  /* If local is set, then create local data */
   if ( !gHyp_frame_isGlobalScope ( pAI->exec.pFrame ) ) {
    pMethodData = gHyp_data_new ( "_message_handler_" ) ;
    gHyp_data_setHashed ( pMethodData, TRUE ) ;
    gHyp_frame_setMethodData ( pAI->exec.pFrame, pMethodData );
  }

  /* Set program counter to begin executing at the handler statement. */
  gHyp_frame_setGlobalFlag ( pAI->exec.pFrame, FRAME_GLOBAL_HANDLER ) ;
  gHyp_frame_setHyp ( pAI->exec.pFrame, 
		      pAI->exec.handler[HANDLER_MESSAGE].pHyp ) ;
  gHyp_frame_setHypIndex ( pAI->exec.pFrame, 
			   pAI->exec.handler[HANDLER_MESSAGE].hypIndex ) ;

  gHyp_frame_setStatementIndex ( pAI->exec.pFrame, 
				 pAI->exec.handler[HANDLER_MESSAGE].hypIndex);

  /* Reset handler */
  pAI->exec.handler[HANDLER_MESSAGE].hypIndex = -1 ;
  
  return TRUE ;
}

static sLOGICAL lHyp_instance_handleMessageCall ( sInstance *pAI )
{  
  char
    *methodName ;
  
  sData
    *pSTATUS,
    *pMethodVariable,
    *pMethodData ;

  sHyp
    *pHyp ;

  sMethod
    *pMethod = NULL ;

  sLOGICAL
     isLocalMethod ;

 if ( !pAI->signal.uMSGPENDING ) return FALSE ;
  
  /* Only allow when coming from an IDLE or SLEEP or QUERY state */
  if ( pAI->exec.state != STATE_QUERY && 
       pAI->exec.state != STATE_SLEEP &&
       pAI->exec.state != STATE_IDLE ) return FALSE ;

  /* There's a method to invoke. */
  pAI->signal.uMSGPENDING = 0 ;

  /* If this is handler invoked message, i.e. from a query, then the
   * STATUS variable may have been set FALSE by a message handler.
   */

  pSTATUS = gHyp_frame_findRootVariable ( pAI->exec.pFrame, "STATUS" ) ;
  if ( guDebugFlags & DEBUG_DIAGNOSTICS )
    gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
			   "...checking %s",gHyp_data_print(pSTATUS)) ;

  if ( !gHyp_data_getBool ( pSTATUS, 0, TRUE ) ) {
    gHyp_util_logWarning("Aborting message call - %s",gHyp_data_print(pSTATUS));
    gHyp_aimsg_delete ( pAI->msg.incoming ) ;
    pAI->msg.incoming = NULL ;
    return FALSE ;
  }

  /* Lookup method name. */
  methodName = gHyp_aimsg_method ( pAI->msg.incoming ) ;

  if ( guDebugFlags & DEBUG_DIAGNOSTICS )
    gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
			 "diag : Handling msg call for '%s'",methodName);

  pMethodVariable = gHyp_frame_findMethodVariable ( pAI->exec.pFrame, 
						    methodName,
						    pAI ) ; 
  if ( pMethodVariable )
    pMethod = (sMethod*) gHyp_data_getObject ( pMethodVariable ) ;

  if ( !pMethodVariable || !pMethod || !gHyp_method_isEnabled ( pMethod ) ) {
    gHyp_util_logWarning ( "Method %s is not enabled.",methodName );
    return FALSE ;
  }  

  isLocalMethod = ( pMethodVariable == 
		    gHyp_frame_findLocalVariable ( pAI->exec.pFrame, methodName ) ) ;

  /* Setup the call frame */
  gHyp_frame_newStmt (	pAI->exec.pFrame,
			STATEMENT_CALL,
			G_STMT_METHODCALL,
			G_PROGRAM_STMT ) ;
  
  pAI->exec.state = STATE_PARSE ;
  gHyp_frame_setGlobalFlag ( pAI->exec.pFrame, FRAME_GLOBAL_TRUE ) ;
  gHyp_frame_setGlobalFlag ( pAI->exec.pFrame, FRAME_GLOBAL_MESSAGE ) ;

  /* If local is set, then create local data */
  if ( !gHyp_frame_isGlobalScope ( pAI->exec.pFrame ) ) {
    pMethodData = gHyp_data_new ( methodName ) ;
    gHyp_data_setHashed ( pMethodData, TRUE ) ;

    if ( isLocalMethod )
      /* Create new local variable to retain return value of method */
      /*gHyp_data_detach ( pMethodVariable ) ;*/
      gHyp_data_append( pMethodData, gHyp_data_new ( methodName ) );

    gHyp_frame_setMethodData ( pAI->exec.pFrame, pMethodData );
  }
 
  /*
  if ( isLocalMethod ) 
    gHyp_data_append( gHyp_frame_getMethodData(pAI->exec.pFrame),
		      pMethodVariable);
  */

  /* Set the data method to the variable that holds the pointer
   * to the method code. 
   */
  gHyp_frame_setMethodVariable ( pAI->exec.pFrame, pMethodVariable ) ;

  /* Set program counter to begin executing at the method statement. */
  gHyp_frame_clearGlobalFlag ( pAI->exec.pFrame, FRAME_GLOBAL_HANDLER ) ;
  pHyp = gHyp_method_getHyp ( pMethod ) ;    
  gHyp_frame_setHyp ( pAI->exec.pFrame, pHyp ) ;
  gHyp_frame_setHypIndex ( pAI->exec.pFrame, 0 ) ;
  gHyp_frame_setStatementIndex ( pAI->exec.pFrame, 0 ) ; 

  /* Consume tokens and values from message. 
   * The incoming message becomes the current message.
   */
  lHyp_instance_consumeMessage ( pAI, pAI->msg.outgoingDepth ) ;

  if ( pAI->msg.current ) gHyp_aimsg_delete ( pAI->msg.current ) ;
  pAI->msg.current = pAI->msg.incoming ;

  if ( gHyp_aimsg_isQuery ( pAI->msg.incoming ) ) 
    gHyp_instance_setSecsReplyOut (pAI,
				     pAI->msg.inSecs.id,
				     pAI->msg.inSecs.stream,
				     pAI->msg.inSecs.function+1,
				     pAI->msg.inSecs.TID,
				     pAI->msg.inSecs.SID ) ;

  pAI->msg.incoming = NULL ;
  pAI->msg.inSecs.id = NULL_DEVICEID ;
  pAI->msg.inSecs.stream = -1 ;
  pAI->msg.inSecs.function = -1 ;
  pAI->msg.inSecs.TID = -1 ;
  pAI->msg.inSecs.SID = -1 ;

  /* Remember the frame depth from which we will send the reply */ 
  pAI->msg.outgoingReply[pAI->msg.outgoingDepth]->frameDepth = gHyp_frame_depth( pAI->exec.pFrame) ;

  /* Increment the outgoingdepth */
  gHyp_instance_incOutgoingDepth ( pAI ) ;

  return TRUE ;    
}

int gHyp_instance_parse ( sInstance * pAI )
{
  sCode 	
    *pCode ;

  int
    hypIndex ;

  sHyp
    *pHyp ;

  int
    condition = COND_SILENT ;
  
  while ( pAI->signal.tokenSignal ||
          ( pAI->exec.state == STATE_PARSE && !gHyp_instance_isEND (pAI) ) ||
	  ( gHyp_frame_testGlobalFlag ( pAI->exec.pFrame, FRAME_GLOBAL_TRUE ) && 
	    gHyp_instance_handleCondition(pAI) ) ) {

    /* If a condition was detected in gHyp_instance_run after calling
     * gHyp_instance_handleCondition, then tokenSignal was set and its 
     * purpose is to prevent a second call to gHyp_instance_handleCondition 
     * in cases when multiple signals are queued up.  
     * We want to execute the signal's methods one at a time 
     * in the order in they were received
     */
    pAI->signal.tokenSignal = 0 ;

    /* Get next instruction index and code */
    hypIndex = gHyp_frame_getHypIndex ( pAI->exec.pFrame ) ;
    pHyp = gHyp_frame_getHyp ( pAI->exec.pFrame ) ;
    pCode = gHyp_hyp_code ( pHyp, hypIndex ) ;

    /* PARSE */
    if ( giJmpLevel == MAX_JMP_LEVEL ) {
      gHyp_util_logError ( "Parse jump level overflow at %d", MAX_JMP_LEVEL ) ;
      longjmp ( gsJmpStack[0], COND_FATAL ) ;
    }

    giJmpLevel++ ;
    condition = gHyp_hyp_parse ( pAI,
				 pCode,
				 pHyp,
				 pAI->exec.pFrame ) ;
    giJmpLevel-- ;
    
    /* If gHyp_hyp_parse returns COND_NORMAL, then keep on parsing.
     * If gHyp_hyp_parse returns another condition, then its because of a longjmp,
     */ 
    if ( condition != COND_NORMAL ) break ;
  }  

  return condition ;
}

sLOGICAL gHyp_instance_isEND ( sInstance *pAI ) 
{
  /* Test's whether HyperScript is at the EndOfProgram */
  int 
    hypIndex,
    hypCount ;

  sHyp
    *pHyp ;

  hypIndex = gHyp_frame_getHypIndex ( pAI->exec.pFrame ) ;
  pHyp = gHyp_frame_getHyp ( pAI->exec.pFrame ) ;
  hypCount = gHyp_hyp_getHypCount ( pHyp ) ;

  if ( hypIndex < hypCount ) return FALSE ;

  if ( hypIndex < gHyp_hyp_getHighWaterCount ( pHyp ) ) {
    if ( gHyp_frame_testGlobalFlag ( pAI->exec.pFrame, FRAME_GLOBAL_HANDLER ) )
      return FALSE ;
  }
  /* gHyp_util_logInfo("AT END FOR %s",pAI->msg.targetId ) ;*/

  return TRUE ;
}

static sLOGICAL hasCondition = 0 ;
sLOGICAL gHyp_instance_handleCondition ( sInstance * pAI )
{
  /*if ( ! gHyp_frame_testGlobalFlag ( pAI->exec.pFrame, FRAME_GLOBAL_TRUE ) )
    return FALSE ;
  else
  */

  /* If a TRUE condition is waiting, then take it */
  if ( hasCondition ) {
    hasCondition = 0 ;
    return TRUE ;
  }
  
  /* Look for another condition */
  hasCondition = (
      lHyp_instance_handleMessageInt  ( pAI ) ||
      lHyp_instance_handleMethod      ( pAI ) ||
      lHyp_instance_handleMessageCall ( pAI ) ||
      lHyp_instance_handleError       ( pAI ) ||
      lHyp_instance_handleDereference ( pAI ) ||
      lHyp_instance_handlePipe        ( pAI ) ||
      lHyp_instance_handleConnect     ( pAI ) ||
      lHyp_instance_handleHangup      ( pAI ) ||
      lHyp_instance_handleInterrupt   ( pAI ) ||
      lHyp_instance_handleAlarm       ( pAI ) 
    ) ;

  if ( !hasCondition ) return FALSE ;

  /* Remember it! */
  pAI->signal.tokenSignal = 1 ;

  /* Clear condition */
  hasCondition = 0 ;

  return TRUE ;
}

int  gHyp_instance_run ( sInstance * pAIarg )
{
  /* longjmp protection */ 
  sInstance *pAI = pAIarg ;

  int
    jmpVal,
    cond = COND_NORMAL ;

  sLOGICAL 
    hasRun=FALSE,
    hasCondition=TRUE,
    isEnd=FALSE ;

  /* Execute HyperScript tokens */
   
  /* When a error occurs we always jump to gsJmpStack[0] */
  giJmpLevel = 0 ;
  giJmpRootLevel = 1 ;
  giJmpEnabled = TRUE ;
  if ( (jmpVal = setjmp ( gsJmpStack[giJmpLevel] )) ) return jmpVal ;

  if ( guDebugFlags & DEBUG_FRAME )
    gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_FRAME,
			 "Running instance %s, STATE = %s", 
			 pAI->msg.targetId,
			 gzaInstanceState[pAI->exec.state] ) ;
  
  /* Interpret the pre-loaded HyperScript code - (parse and execute) */
  while ( pAI->signal.tokenSignal ||
          ( (hasCondition = gHyp_instance_handleCondition(pAI)) ) ||
          !(isEnd = gHyp_instance_isEND (pAI) ) ||
          ( pAI->exec.state == STATE_IDLE ||
            pAI->exec.state == STATE_QUERY ||
	    pAI->exec.state == STATE_SLEEP ) ) {
  
    /* Parse (and possibly execute) the next token.
     * Returns immediately if not STATE_PARSE 
     */

    hasRun = TRUE ;
    cond = gHyp_instance_parse ( pAI ) ;
    /*gHyp_util_debug ( "Parsed. STATE=%s COND=%d",gzaInstanceState[pAI->exec.state],cond);*/
    if ( cond < 0 ) break ;

   /* Returning from gHyp_instance_parse, the state of the instance will be:
     *
     *	1. STATE_PARSE. This is usually because we are at the end-of-input stream.
     *     It could also be after an syntax error occurred in the input stream.
     *
     *	2. STATE_IDLE when the instance executes a longjmp to gsJmpStack[1]
     *	   after executing an idle() function or returning from a METHOD 
     *     or handler which was called from an idle state.
     *
     *	3. STATE_EXECUTE when the instance executes a longjmp to gsJmpStack[1]
     *     after executing the pexec() function or context switching between
     *     running instance
     *
     *  4. STATE_QUERY when the instance executes a longjmp to gsJmpStack[1]
     *     after sending a query message and now needs to get a reply.
     *
     *  5. STATE_SLEEP after the instance executes the sleep() function..
     *
     *  6. STATE_DEREFERENCE after a dereference returns

     */
    if ( pAI->exec.state == STATE_IDLE || 
	 pAI->exec.state == STATE_QUERY ||
	 pAI->exec.state == STATE_SLEEP ) {
   
      if ( guDebugFlags & DEBUG_DIAGNOSTICS ) {
	  gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
			       "%s returning to %s state",
			       pAI->msg.targetId,
			       gzaInstanceState[pAI->exec.state]) ;
      }

      /* If an instance that could go back to a SLEEP, IDLE, or QUERY state
       * still has an outstanding condition to handle, then don't to the
       * gHyp_instance_read until the conditions are fully handled
       */
      if (  pAI->signal.tokenSignal ||
            (hasCondition = gHyp_instance_handleCondition(pAI))
	    ) {
        /* Loop around and execute the condition */
        gHyp_instance_setState ( pAI, STATE_PARSE ) ;
      }
      else {

        /* Look for a signal or an incoming message */
        cond = gHyp_instance_read ( pAI, FALSE ) ;

        /* For COND_SILENT, COND_ERROR, or COND_FATAL, we stop parsing, 
         * we exit out, then maybe load another instance to parse/run
         */
        if ( cond <= 0 ) break ;

        /* If we are awaking from a sleep, break out and maybe let
         * another instance run.
         */
        if ( pAI->exec.state == STATE_SLEEP &&
	     pAI->exec.eventType & EVENT_WAKEUP &&
	     pAI->exec.wakeTime == 0 ) {
	  gHyp_instance_nextEvent ( pAI ) ;
          if ( guDebugFlags & DEBUG_DIAGNOSTICS ) 
	     gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
			       "%s awakening from SLEEP state",pAI->msg.targetId ) ;
	  gHyp_instance_pushSTATUS ( pAI, gHyp_frame_stack ( pAI->exec.pFrame ) ) ;
	  gHyp_instance_setState ( pAI, STATE_PARSE ) ;
          break ;
        }
        
      }
    }
    else if ( pAI->exec.state == STATE_DEREFERENCE ) {

      /* After a dereference ends, set the state back to PARSE. */
      pAI->exec.state = STATE_PARSE ;
    }
    else if ( pAI->exec.state == STATE_EXECUTE ) {
      
      /* CURRENT USE:
       *  1. pexec() to PROMIS.   
       *  2. context switching (after '}' and ';' )
       */
      gHyp_util_log ( " " ) ;
      cond = COND_NORMAL ;
      break ;
    }
    else if ( pAI->exec.state == STATE_PARSE ) {

      /* Probably the end of the input stream but, 
       * if there was a parse error and a handler is defined, then
       * turn back on execution so the handler can execute, otherwise
       * exit back to the calling function.
       */
      if ( pAI->signal.uERROR == 1 ) {

	if ( pAI->exec.handler[HANDLER_ERROR].hypIndex != -1 )

	  /* A handler was defined, turn execution back on. */
	  gHyp_frame_setGlobalFlag ( pAI->exec.pFrame, FRAME_GLOBAL_TRUE ) ;

	else

	  /* No handler.  If we are still executing, continue, otherwise exit */
	  if ( !gHyp_frame_testGlobalFlag ( pAI->exec.pFrame,
					    FRAME_GLOBAL_TRUE ) ) {
	    cond = COND_ERROR ;
	    break ;
	  }
      }
    }
  }
  
  /* Prevent spinning here...
   *
   * Spinning occurs when:
   *
   *	- the cond is NORMAL
   *    - the instance is in STATE_PARSE
   *    - the instance has no pending conditions.
   *    - the instance is at its end-of-file
   *    - the instance didn't just run
   *
   * To fix, we need to put the instance back into IDLE state.
   */
  if (	cond == COND_NORMAL &&
	!hasCondition && 
	isEnd && 
	!hasRun && 
	pAI->exec.state == STATE_PARSE ) {
    if ( gHyp_concept_getConceptInstance ( gHyp_instance_getConcept(pAI) ) == pAI /*&&
	    gHyp_frame_depth ( pAI->exec.pFrame) <= 1 */ ) {
	    /*gHyp_util_debug("Can return to stdin");*/
      gHyp_concept_setReturnToStdIn ( gHyp_instance_getConcept(pAI),TRUE ) ;
    }
    else {
	    /*gHyp_util_debug("Back to idle");*/
	    gHyp_instance_setState ( pAI, STATE_IDLE ) ;
    }
  }
  
  /** pAI can invalidate.  See renameto() *
  if ( guDebugFlags & DEBUG_STATE )
    gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_STATE,
			 "%s returning from running, cond=%d, state=%s",
			 pAI->msg.targetId,
			 cond,
			 gzaInstanceState[pAI->exec.state] ) ;
  */

  return cond ;

}

void gHyp_instance_pushLocalSTATUS ( sInstance *pAI, sStack *pStack )
{
  sData 
    *pRootStatus,
    *pResult,
    *pValue ;

  char
      value[VALUE_SIZE+1],
      strBuf[MAX_INPUT_LENGTH+1],
      *pStr,
      *pBuf,
      *pBufEnd ;    

  int
      valueLen,
      ss,
      context ;
    
  sLOGICAL
      isVector ;

  pRootStatus = gHyp_frame_findRootVariable ( pAI->exec.pFrame, "STATUS" ) ; 
  if ( pRootStatus ) {

    /* Construct the string from the elements of the list */
    pBuf = strBuf ;
    pBufEnd = pBuf + MAX_INPUT_LENGTH ;    
    pResult = NULL ;
    isVector = (gHyp_data_getDataType(pRootStatus) > TYPE_STRING ) ;
    ss = gHyp_data_getSubScript ( pRootStatus ) ; 
    context = -1 ;
    while ( (pResult = gHyp_data_nextValue ( pRootStatus, 
					     pResult,
					     &context,
					     ss ))) {
      valueLen = gHyp_data_getStr ( pResult, 
				    value, 
				    VALUE_SIZE, 
				    context, 
				    isVector ) ;
      pStr = value ;
      
      if ( (pBuf + valueLen) > pBufEnd ) 
	gHyp_instance_error ( pAI, STATUS_IO,
			      "Input string longer than %d characters \n",
			      MAX_INPUT_LENGTH ) ;
      sprintf ( pBuf, "%s", pStr ) ;
      pBuf += valueLen ;   
    }
    if ( context== -2 && ss != -1 ) 
      gHyp_instance_error ( pAI, STATUS_BOUNDS, 
			    "Subscript '%d' is out of bounds in strtok()",ss);
    *pBuf = '\0' ;
  }
  else
    strcpy ( strBuf, "%NOSTATUS" ) ;

  pValue = gHyp_data_new ( NULL ) ;
  gHyp_data_setToken ( pValue, strBuf ) ;
  gHyp_stack_push ( pStack, pValue ) ;
}

void gHyp_instance_pushSTATUS ( sInstance *pAI, sStack *pStack )
{
  sData *pResult ;
  pResult = gHyp_data_new ( NULL ) ;
  gHyp_data_setReference ( pResult,
                           "STATUS",
			   gHyp_frame_findRootVariable ( pAI->exec.pFrame, "STATUS" )
			  ) ;
  gHyp_stack_push ( pStack, pResult ) ;
}

void gHyp_instance_setgpAI( sInstance *pAI )
{
  gpAI = pAI ;
}

void gHyp_instance_setgpAImain ( sInstance *pAI )
{
  gpAImain = pAI ;
}
