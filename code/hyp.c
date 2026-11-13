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
/********************** HYPERSCRIPT INTERFACE ********************************/

#include "auto.h"	/* System Interface and Function Prototypes */

/**********************	EXTERNAL GLOBAL VARIABLES ****************************/

/**********************	INTERNAL GLOBAL VARIABLES ****************************/

/********************** INTERNAL OBJECT STRUCTURES ***************************/

static char	gzTraceBuf[MAX_TRACE_LENGTH+1] ;	
static char	gzTraceBufPrev[MAX_TRACE_LENGTH+1] ;	
static char	gzTraceBufPrevPrev[MAX_TRACE_LENGTH+1] ;	

/* The code_t structure holds a single execution unit of program code.
 * The entire HyperScript program is a set of sHyp blocks, consisting
 * of arrays of sCode.
 */
 
struct code_t {

  /* Code function */
  void	(*function)(sInstance *pAI,sCode *pCode,sLOGICAL isPARSE) ;
  char	*token ;	       	/* HyperScript token */
  int  	index ;		       	/* Program position */
  short	line ;		       	/* Program line # */
  sBYTE tokenType ;		/* Token type */	
  sBYTE	precedence ;	       	/* Token precedence */	
  sBYTE	rank ;		       	/* Token rank */	
} ;

/* All of the tokens extracted from the HyperScript program source are
 * stored in a linked list inside of a hash table array.
 */

/* The hyp_t structure holds the entire HyperScript program */
struct	hyp_t
{
  unsigned 	size ;			/* Size of pHyp memory allocated */
  unsigned 	count ;			/* Count of HyperScript commands */
  unsigned	highWaterCount ;	/* Dereference area */
  char		method[METHOD_SIZE+1] ; /* Name of method which owns code */
  sCode		*pCode ;		/* Compiled HyperScript	*/
} ;

/**********************	FUNCTION DECLARATIONS ********************************/

sHyp* gHyp_hyp_new ( char *method )
{
  /* Description:
   *
   *	Create a new program space. 
   *
   * Arguments:
   *
   *	method	[R]
   *	- name of method
   *
   * Return value:
   *
   *	Pointer to program's sHyp structure
   *
   */
  sHyp *pHyp = (sHyp*) AllocMemory ( sizeof ( sHyp ) ) ;
  assert ( pHyp ) ;
  strcpy ( pHyp->method, method ) ;
  pHyp->pCode = NULL ;
  pHyp->size = 0 ;
  pHyp->count = 0 ;
  pHyp->highWaterCount = pHyp->count ;

  gzTraceBufPrev[0] = '\0' ;
  gzTraceBufPrevPrev[0] = '\0' ;
  gzTraceBuf[0]     = '\0' ;

  /*  gHyp_util_logInfo("Creating Hyp %s",method ) ; */
  return pHyp ;
}

void gHyp_hyp_delete ( sHyp * pHyp ) 
{
  /* Description:
   *
   *	Delete the HyperScript program. 
   *
   * Arguments:
   *
   *	pHyp							[R]
   *	- pointer to sHyp program object
   *
   * Return value:
   *
   *	none
   *
   */

  /* Free the block of code */
  ReleaseMemory ( pHyp->pCode ) ; 
  
  /* Free the program */
  ReleaseMemory ( pHyp ) ; 

  return ;
}


void gHyp_hyp_traceReport ( int lineNo, char *method ) 
{
  /* Description:
   *
   *	Output a line indicating where HyperScript is currently executing.
   *
   * Arguments:
   *
   *	lineNo						[R]
   *	- line number of last token executed
   *
   * Return value:
   *
   *	none
   *
   */
  char *pTrace = strchr(gzTraceBuf,':') ;
  if ( !pTrace ) 
    pTrace = gzTraceBuf ;
  else
    pTrace++ ;
  pTrace = pTrace + MAX ( 0, (int) (strlen ( pTrace ) - 40 ) ) ;
  gHyp_util_logError ( "-->%s<-- in line %d, in method %s", pTrace, lineNo, method ) ;
  gHyp_util_logError ( "%s", gzTraceBufPrevPrev ) ;
  gHyp_util_logError ( "%s", gzTraceBufPrev ) ;
  gHyp_util_logError ( "%s", gzTraceBuf ) ;
}

void gHyp_hyp_traceReset ( char* pMethod ) 
{
  /* Description:
   *
   *	Reset the trace buffer. Done after every EOS.
   *
   * Arguments:
   *
   *	none
   *
   * Return value:
   *
   *	none
   *
   */
  strcpy ( gzTraceBufPrevPrev, gzTraceBufPrev ) ;
  strcpy ( gzTraceBufPrev, gzTraceBuf ) ;
  sprintf ( gzTraceBuf, "%s: ", pMethod ) ;
}

static void lHyp_hyp_trace ( sCode *pCode, sLOGICAL isExecuting ) 
{
  /* Description:
   *
   *	Add tokens just parsed to the trace buffer.
   *
   * Arguments:
   *
   *	pCode						[R]
   *	- pointer to the code just parsed
   *
   *	isExecuting
   *	- if true, the code is also being executed
   *
   * Return value:
   *
   *	none
   *
   */
  /*char		tokenStr[MAX_INPUT_LENGTH+1] ;*/
  char	tokenStr[MAX_TRACE_LENGTH+1] ;
  int		nextTokenLen,
  		lastTokensLen ;

  /* Trace the most recent tokens being parsed. */
  if ( pCode->tokenType == TOKEN_LITERAL )
    sprintf ( tokenStr, "%c%.*s%c ", '"', MAX_TRACE_LENGTH-3, pCode->token, '"' ) ; 

  else if ( pCode->tokenType == TOKEN_UNIDENTIFIED &&
	    !gHyp_util_isIdentifier( pCode->token ) )
    
    sprintf ( tokenStr, "'%.*s' ", MAX_TRACE_LENGTH-3, pCode->token ) ; 
      
  else

    sprintf ( tokenStr, "%.*s ", MAX_TRACE_LENGTH-3, pCode->token ) ; 
  
  nextTokenLen = strlen ( tokenStr ) ;
  lastTokensLen = strlen ( gzTraceBuf ) ;
  
  if ( nextTokenLen > MAX_TRACE_LENGTH )
    
    strcpy ( gzTraceBuf,  tokenStr + nextTokenLen - MAX_TRACE_LENGTH ) ;
  
  else {
  
    if ( nextTokenLen > ( MAX_TRACE_LENGTH - lastTokensLen ) )
    
    {
      /* Cannot append without first trimming from the front of the buffer.
       * The source and destination overlap, so memmove, never strcpy
       * (undefined for overlapping strings; AddressSanitizer flags it). */
      int trim = MAX ( MAX_TRACE_LENGTH/2, nextTokenLen ) ;
      if ( trim > lastTokensLen ) trim = lastTokensLen ;
      memmove ( gzTraceBuf, gzTraceBuf + trim, lastTokensLen - trim + 1 ) ;
    }
    
    /* Append the token */
    strcat ( gzTraceBuf, tokenStr ) ;
   }
      
  /* Output a trace that tracks which tokens were parsed and executed. */
  if ( (guDebugFlags & DEBUG_TRACE ) && isExecuting ) 
    gHyp_util_trace ( tokenStr ) ;
    
  return ;
}
      
void gHyp_hyp_describe ( sHyp *pHyp )
{
  unsigned 
    i ;

  sCode
    *pCode ;

  /*gHyp_util_debug("DESCRIBE %s",pHyp->method);*/
  for ( i=0; i<pHyp->count; i++ ) {
    
    pCode = (sCode*) (pHyp->pCode + i) ;
    gHyp_util_logInfo( "%d: %s", pCode->index, pCode->token ) ;

  }
}
    	
char* gHyp_hyp_describe2 ( sHyp *pHyp, int hypIndex, char *buf, int buflen )
{
  unsigned 
    i ;

  sCode
    *pCode ;

  char
    *pBuf = buf,
    *pEndBuf = buf + buflen ;

  /*gHyp_util_debug("DESCRIBE %s",pHyp->method);*/
  for ( i=hypIndex; i<pHyp->count; i++ ) {
    
    pCode = (sCode*) (pHyp->pCode + i) ;
    if ( pBuf + strlen ( pCode->token ) >= pEndBuf ) break ;
    
    pBuf += sprintf ( pBuf, "%s ", pCode->token ) ;

  }

  *pBuf = '\0' ;
  return buf ;
}
    	

void gHyp_hyp_setHypCount ( sHyp * pHyp, int hypCount )
{
  /* Description:
   *
   *	Set the total count of the number of program codes (instructions).
   *
   * Arguments:
   *
   *	pHyp					[R]
   *	- pointer to sHyp code
   *
   *	hypCount				[R]
   *	- number of sCode's
   *
   * Return value:
   *
   *	none
   *
   */
  pHyp->count = hypCount ;
  return ;
}

int gHyp_hyp_getHypCount ( sHyp * pHyp )
{
  /* Description:
   *
   *	Get the total count of the number of sCode's (instructions).
   *
   * Arguments:
   *
   *	pHyp					[R]
   *	- pointer to sHyp code
   *
   * Return value:
   *
   *	Total number of sCode's.
   *
   */
  return pHyp->count ;
}

int gHyp_hyp_getHighWaterCount ( sHyp *pHyp )
{
  return pHyp->highWaterCount ;
}

char *gHyp_hyp_method ( sHyp *pHyp )
{
  return pHyp->method ;
}

void gHyp_hyp_setRank ( sCode * pCode, sBYTE rank )
{
  pCode->rank = rank ;
}

sBYTE gHyp_hyp_rank ( sCode * pCode )
{
  /* Description:
   *
   *	Returns the expression rank of a sCode.
   *	See HYPDEF.H for RANK_* defines.
   *
   * Arguments:
   *
   *	pCode					[R]
   *	- pointer to code
   *
   * Return value:
   *
   *	Expression RANK_* value
   *
   */
  return pCode->rank ;
}

sBYTE gHyp_hyp_precedence ( sCode * pCode )
{
  /* Description:
   *
   *	Get the precedence of the  sCode.
   *	See HYPDEF.H for PRECEDENCE_* defines.
   *
   * Arguments:
   *
   *	pCode					[R]
   *	- pointer to  code
   *
   * Return value:
   *
   *	PRECEDENCE_* value
   *
   */
  return pCode->precedence ;
}

void gHyp_hyp_setPrecedence ( sCode* pCode, sBYTE precedence ) 
{
  pCode->precedence = precedence ;
}

void gHyp_hyp_setTokenType ( sCode* pCode, sBYTE tokenType ) 
{
  pCode->tokenType = tokenType ;
}

sBYTE gHyp_hyp_tokenType ( sCode * pCode )
{
  /* Description:
   *
   *	Get the token type of the sCode.
   *	See HYPDEF.H for TOKEN_* defines.
   *
   * Arguments:
   *
   *	pCode					[R]
   *	- pointer to sHyp code
   *
   * Return value:
   *
   *	TOKEN_* value
   */
  return pCode->tokenType ;
}

int gHyp_hyp_index ( sCode * pCode )
{
  /* Description:
   *
   *	Get the program counter of the sCode.
   *
   * Arguments:
   *
   *	pCode					[R]
   *	- pointer to sHyp code
   *
   * Return value:
   *
   *	Program counter.
   *
   */
  return pCode->index ;
}

int gHyp_hyp_line ( sCode * pCode )
{
  /* Description:
   *
   *	Get the line number of the sCode.
   *
   * Arguments:
   *
   *	pCode					[R]
   *	- pointer to sHyp code
   *
   * Return value:
   *
   *	Line number for code.
   *
   */
  return pCode->line ;
}

char* gHyp_hyp_token ( sCode * pCode )
{
  return pCode->token ;
}

sCode* gHyp_hyp_code ( sHyp * pHyp, int index )
{
  /* Description:
   *
   *	Get the code object from the program, given a program counter.
   *
   * Arguments:
   *
   *	pHyp					[R]
   *	- pointer to sHyp code
   *
   *	index					[R]
   *	- program counter
   *
   * Return value:
   *
   *	Pointer to code corresponding to index
   *
   */
  if ( index >= 0 && index < (int) pHyp->highWaterCount ) 
    return ( pHyp->pCode + index ) ;
  else
    return NULL ;
}

sLOGICAL gHyp_hyp_okUnaryOperator ( sHyp *pHyp )
{
  /* Description:
   *
   *	Check to see if the previous token can be followed by a unary operator.
   *
   * Arguments:
   *
   *	pHyp					[R]
   *	- pointer to hyp object
   *
   * Return value:
   *
   *	TRUE if YES, otherwise FALSE.
   *
   *
   */

  sCode
    *pCode ;

  /* If the operator begins the program, its unary. */
  if ( pHyp->count == 0 ) return TRUE ;

  /* If the previous token is unary, binary, or (,{,[, then its unary. */
  pCode = pHyp->pCode + pHyp->count - 1 ;
  if ( pCode->rank == RANK_UNARY ||
       pCode->rank == RANK_BINARY ||
       pCode->rank == RANK_OPEN )	
    return TRUE ;
  
  /* All other cases, its binary */
  return FALSE ;
} 

int gHyp_hyp_source ( sInstance *pAIarg,
		      sHyp *pHypArg, 
		      FILE* pp,
		      char *ppName,
		      sLOGICAL allowLongJmp )
{
  /* Description:
   *
   *	Read the next input line from 'pp' and load it into the current
   *	HyperScript program.
   *
   * Arguments:
   *
   *	pAI				[R]
   *	- instance
   *
   *	pHyp				[R]
   *	- pointer to HyperScript program
   *
   *	pp
   *	- pointer to file (or stdin) for HyperScript source code
   *
   * Return value:
   *
   */

  static char		
    stream[MAX_STREAM_LENGTH+1] ;

  char
    *pStream,
    terminator ;

  /* longjmp protection */
  sInstance *pAI = pAIarg ;
  sHyp *pHyp = pHypArg ;

  int 
    jmpVal ;

  /* Establish return from signal longjmp. 
   *
   * Under VMS, a longjmp from a handler (such as a SIGALRM handler) causes 
   * the file status to go bad, resulting in a failure the next time a read 
   * is attempted.  Under VMS, the solution is to call sys$cancel on the file's
   * channel rather than longjmp, causing the read to complete gracefully
   * upon returning from the handler.
   */

  if ( allowLongJmp ) {
    giJmpLevel = 0 ;
    giJmpEnabled = TRUE ;
    if ( (jmpVal = setjmp ( gsJmpStack[giJmpLevel] )) ) return jmpVal ;
  }
  else {
    giJmpEnabled = FALSE ;
  }

  /* Loop to handle read of string literals over multiple lines */ 
  while ( 1 ) {

    /* Read next line of input, adding to previous input. */
#ifdef AS_WINDOWS
    gsSocketToCancel = _fileno ( pp ) ;
#else
    gsSocketToCancel = fileno ( pp ) ;
#endif
    pStream = fgets ( stream, MAX_STREAM_LENGTH, pp ) ;

    /* Test for end-of-file */
    if ( pStream == NULL ) return COND_ERROR ;
    
    /* giLineCount is for the current METHOD being parsed or executed, and 
     * is set to zero at the start of each new method.  Its purpose
     * is for debugging, when errors occurs it helps to find the spot.
     * On the other hand, giProgramCount is for all that has been loaded
     * thus far.
     */
    giLineCount++ ;
    giProgramCount++ ;
    /* Test for verify flag */
    if ( (guRunFlags & RUN_VERIFY) )
      gHyp_util_logInfo ( "%4d: %s", giProgramCount, pStream ) ;
    
    /* Load tokens into HyperScript program. 
     * Under normal conditions, pStream will point to the null character
     * at the end of the stream.  If pStream is NULL, then there was a 
     * load error such as an illegal character.  If pStream points to
     * a non-null character, then this means a string literal did not
     * have a matching end delimiter, instead the line ended  with a '\' 
     * character and the line continues.
    */
    pStream = gHyp_load_fromStream ( pAI, pHyp, stream, giLineCount ) ;

    /* Quit if load was fatal */
    if ( pStream == NULL ) {
      guErrorCount++ ;
      gHyp_util_logError ( "Failed to load HyperScript from %s",ppName ) ;
      return COND_FATAL ;
    }
    
    /* If the stream ends is a NULL terminated string, then we're done */
    if ( !*pStream ) break ;
     
    /* Some of the stream was not loaded. 
     * This is a nonterminated "string" or 'variable' on the line.
     * Strings and variables cannot span multiple lines. 
     * Terminate the string or variable and issue a warning.
     */
    terminator = *pStream ;
      
    if ( terminator == '"' )
      gHyp_util_logWarning ( 
	"Nonterminated string constant on line %d, (line %d of %s)", giProgramCount, giLineCount, ppName ) ;
    else if ( terminator == '\'' )
      gHyp_util_logWarning ( 
	 "Nonterminated variable on line %d, (line %d of %s)", giProgramCount, giLineCount, ppName ) ;
    else 
      gHyp_util_logWarning ( 
	 "Nonterminated character array on line %d, (line %d of %s)", giProgramCount, giLineCount, ppName ) ;

    break ;
  }

  return COND_NORMAL ;
}

sLOGICAL gHyp_hyp_load ( sInstance *pAI,
			 sConcept *pConcept,
			 sHyp   *pHyp,
			 char   *token, 
			 sBYTE 	tokenType, 
			 sBYTE 	precedence,
			 int   	lineNo,
			 void  	(*pf)(sInstance*,sCode*,sLOGICAL) )
{
  /* Description:
   *
   *	Create a code object from a HyperScript token. 
   *
   * Arguments:
   *
   *	pHyp							[R]
   *	- pointer to hyp object
   *
   *	token							[R]
   *	- pointer to token in a program
   *
   *	tokentype						[R]
   *	- see HYPDEF.H, TOKEN_* defines
   *
   *	precedence						[R]
   *	- see HYPDEF.H, PRECEDENCE_* defines
   *
   *	lineNo							[R]
   *	- line number in which token was found
   *
   *	pf							[R]
   *	- a pointer to a function returning void
   *	- this is the function that executes the token
   *
   * Return value:
   *
   *	TRUE if a new program code is appended to the program
   *
   *
   */
  sCode
    *pCode ;

  int
    hypIndex ;

  if ( pHyp->count >= pHyp->size ) {

    if ( !pHyp->pCode ) {

      /* Allocate the MINIMUM amount for the hyperscript code */
      pHyp->size = DEFAULT_HYPER_CODE ;
      pHyp->pCode = (sCode*) AllocMemory ( pHyp->size * sizeof ( sCode ) ) ;
      assert ( pHyp->pCode ) ;
    }
    else {

      /* Out of room. Allocate twice as much as before */
      pHyp->size <<= 1 ;
      if ( pHyp->size > MAX_HYPER_CODE )
    	return gHyp_util_logError ( 
    		"HyperScript Code size of %d exceeds maximum size of %d",
		pHyp->size, MAX_HYPER_CODE ) ;
      pCode = (sCode*) ReAllocMemory ( pHyp->pCode, pHyp->size * sizeof ( sCode ) ) ;
      assert ( pCode ) ;
      pHyp->pCode = pCode ;
    }	
  }	

  /* Initialize HyperScript entry */
  hypIndex = pHyp->count++ ;
  pHyp->highWaterCount = pHyp->count ;

  pCode = gHyp_hyp_code ( pHyp, hypIndex ) ;

  /* Token needs to be allocated and put in hash table. */
  pCode->token = gHyp_concept_createToken ( pConcept, token ) ;

  pCode->function = pf ;
  pCode->index = hypIndex ; 
  pCode->line = lineNo ; 
  pCode->tokenType = tokenType ;
  pCode->precedence = precedence ;

  /* See HYPDEF.H for understanding next statement */
  if ( tokenType < TOKEN_KEYWORD ) 
    pCode->rank = RANK_OPERAND ;
  else if ( tokenType < TOKEN_POSTINC ) 
    pCode->rank = RANK_UNARY ;
  else if ( tokenType < TOKEN_EEXP ) 
    pCode->rank = RANK_POSTUNARY ;
  else if ( tokenType < TOKEN_BEXP ) 
    pCode->rank = RANK_CLOSE ;
  else if ( tokenType < TOKEN_LIST ) 
    pCode->rank = RANK_OPEN ;
  else
    pCode->rank = RANK_BINARY ;

  /*
  gHyp_util_logInfo("Loading %s into %s at %d",
		    token,
		    pHyp->method,
		    hypIndex );
  */
  return TRUE ;
}

void gHyp_hyp_transfer ( sInstance *pAI,
			 sConcept *pConcept,
			 sHyp *pDstHyp,
			 sHyp *pSrcHyp,
			 int methodLabelIndex,
			 int methodStartIndex,
			 int methodEndIndex,
			 sData *pMethodData )
{
  /* Transfer the pCode elements from pSrcHyp to pDstHyp, starting at 
   * index and going to endIndex. 
   */
  int 
    i,
    j,
    lineNo=0,
    nextLineNo ;

  char
    *token ; 
		
  sBYTE
    tokenType, 
    precedence ;

  sCode
    *pCode ;

  sData
    *pData,
     *pMethodVariable ;

  void
    (*pf)(sInstance*,sCode*,sLOGICAL) ;

  pCode = gHyp_hyp_code ( pSrcHyp, methodStartIndex-1 ) ;
  pf = pCode->function ;
  lineNo = pCode->line ; 
  tokenType = pCode->tokenType ;
  precedence = pCode->precedence ;
  token = pCode->token ;

  assert ( strcmp(token,"{") == 0 ) ;

  gHyp_hyp_load ( pAI,
		    pConcept,
		    pDstHyp,
		    token, 
		    tokenType, 
		    precedence,
		    lineNo,
		    pf ) ;
    
  /* Load a "<M> ;" after the method definition.  This prevents the
   * case when a dereference is done on an argument, but the dereference
   * cannot occur until after the first statement ends after calling the method.
   * The problem occurs when the argument varaible is used as the first statement,
   * if we don't put this semicolon at this point, it'll fail.
   */
  gHyp_hyp_load ( pAI,
		   pConcept,
		   pDstHyp, 
		   gHyp_data_getLabel ( pMethodData ),
		   TOKEN_UNIDENTIFIED, 
		   PRECEDENCE_OPERAND,
		   0,
		   gHyp_operand_token ) ;

  gHyp_hyp_load ( pAI,
		   pConcept,
		   pDstHyp, 
		   ";",
		   TOKEN_EOS, 
		   PRECEDENCE_EOS,
		   0,
		   gHyp_stmt_eos ) ;
 

  for ( i=methodStartIndex ; i<methodEndIndex; i++ ) {

    /* Extract the code components from the source Hyp */
    pCode = gHyp_hyp_code ( pSrcHyp, i ) ;
    pf = pCode->function ;
    lineNo = pCode->line ; 
    tokenType = pCode->tokenType ;
    precedence = pCode->precedence ;
    token = pCode->token ;
    
    /* Re-load them into the destination Hyp */
    gHyp_hyp_load ( pAI,
		    pConcept,
		    pDstHyp,
		    token, 
		    tokenType, 
		    precedence,
		    lineNo,
		    pf ) ;
  }

  /* Truncate the source Hyp */
  j = pSrcHyp->count ;
  pSrcHyp->count = methodLabelIndex + 1  ;
  pSrcHyp->highWaterCount = pSrcHyp->count ;

  /* Check for deprecated 'method definition' - find the line number of the 
   * token following the "}".
   * A method definition should end with a ";" or and "= exp ;".  
   * The old way it did not, so the addition of a ";" if the method definition ends alone 
   * on a line with a "}" is still allowed"
   */
  if ( methodEndIndex > j ) {
    pCode = pSrcHyp->pCode + methodEndIndex ;
    nextLineNo = pCode->line ; 
  }
  else /* Must be end-of-file */ 
    nextLineNo = lineNo + 1 ;
  
  if (  nextLineNo > lineNo && 
	gHyp_parse_isMethodDeprecated (gHyp_frame_parse(gHyp_instance_frame(pAI))) &&
	!gHyp_frame_findMethodVariable ( gHyp_instance_frame(pAI), 
					 gHyp_data_getLabel (pMethodData),
					 pAI ) ) {
    
    /* Deprecated. Issue warning, urging user to correct. 
    gHyp_util_logWarning ( "Deprecated method definition for '%s', please add ';' after '}'",
    			   gHyp_data_getLabel ( pMethodData )) ;
    */
    gHyp_hyp_load ( pAI,
		    pConcept,
		    pSrcHyp, 
		    ";",
		    TOKEN_EOS, 
		    PRECEDENCE_EOS,
		    1,
		    gHyp_stmt_eos ) ;
    pMethodVariable = gHyp_frame_createVariable ( pAI,
                                                  gHyp_instance_frame(pAI), 
						  gHyp_data_getLabel ( pMethodData ) ) ;
    pData = gHyp_data_new ( NULL ) ;
    gHyp_data_setStr ( pData, STATUS_ACKNOWLEDGE ) ;
    gHyp_data_append ( pMethodVariable, pData ) ;
    gHyp_data_copyObject ( pMethodVariable, pMethodData ) ; 

  }
  
  /* Shift up any tokens found after the method back */
  for ( i=methodEndIndex; i<j; i++ ) {
    
    /* Extract the code components from the source Hyp */
    pCode = pSrcHyp->pCode + i ;
    pf = pCode->function ;
    lineNo = pCode->line ; 
    tokenType = pCode->tokenType ;
    precedence = pCode->precedence ;
    token = pCode->token ;
    
    /* Re-load them into the destination Hyp */
    /*gHyp_util_logInfo("Moving %s",token );*/
    gHyp_hyp_load ( pAI,
		    pConcept,
		    pSrcHyp,
		    token, 
		    tokenType, 
		    precedence,
		    lineNo,
		    pf ) ;
  }

}

int gHyp_hyp_parse (	sInstance	*pAIarg,
			sCode 		*pCodeArg,
			sHyp 		*pHypArg,
			sFrame 		*pFrameArg )
{
  /* Description:
   *
   *	Parse the HyperScript code pointed to by pCode. 
   *
   * Arguments:
   *
   *	pAI							[R]
   *	- pointer to instance object
   *
   *	pCode							[R]
   *	- pointer to code object
   *
   *	pHyp							[R]
   *	- pointer to HyperScript program
   *
   *	pFrame							[R]
   *	- pointer to program frame
   *
   * Return value:
   *
   *	COND_*
   *
   */

  /* longjmp protection */
  sInstance* pAI = pAIarg ;
  sCode* pCode = pCodeArg ;
  sHyp* pHyp = pHypArg ;
  sFrame* pFrame = pFrameArg ;

  int jmpVal ;

  /* When a recoverable error occurs (that which we can handle), we always
   * jump back to this point and then return FALSE.
   * We always jump back to gsJmpStack[1] when going to an IDLE state.
   */
  if ( (jmpVal = setjmp ( gsJmpStack[giJmpLevel] )) ) return jmpVal ;
    
  /* Do we need to complete an expression? */
  gHyp_parse_completeExpression ( gHyp_frame_parse ( pFrame ), pAI, pHyp, pFrame ) ;

  /* Check if we were expecting 'else' but didn't get it. */
  if (	pCode->precedence != PRECEDENCE_EMPTY ) {
    while ( gHyp_frame_expectedState ( pFrame ) == G_IF_ELSE ) {
      gHyp_frame_endStmt ( pFrame, pAI ) ;
      pCode = gHyp_hyp_code ( pHyp, gHyp_frame_getHypIndex ( pFrame ) ) ;
    }
  }
    
  /* Debugging: level 5. */
  if ( guDebugFlags & DEBUG_STATE )
    gHyp_util_logDebug (	FRAME_DEPTH_NULL, DEBUG_STATE,
    				"state: %s, expecting: %s",
    				gHyp_frame_currentStateStr ( pFrame ),
				gHyp_frame_expectedStateStr ( pFrame ) ) ;
    
  /* Add to trace stream that tracks which tokens were parsed. */
  lHyp_hyp_trace ( pCode, gHyp_frame_isStmtTrue ( pFrame ) ) ;
    
  if ( guDebugFlags & DEBUG_PARSE )
    gHyp_util_logDebug (	FRAME_DEPTH_NULL, DEBUG_PARSE,
    				"parse: '%s', at index: %d ", 
				pCode->token,
				pCode->index ) ;
  
  /* When compiling-only, method definitions are tricky */
  if ( (guRunFlags & RUN_COMPILEONLY) &&
       gHyp_frame_expectedState ( pFrame ) == G_STMT_LIST &&
       pCode->tokenType != TOKEN_BLIST ) 
    gHyp_frame_setStates ( pFrame, G_STMT_EXP, G_STMT_EXP ) ;

       
  /* Advance program counter, assuming that will be the next instruction */
  gHyp_frame_incHypIndex ( pFrame ) ;
    
  /* Parse it */
  pCode->function ( pAI, pCode, TRUE ) ;
  
  return COND_NORMAL ;
}

int gHyp_hyp_execute (	sInstance	*pAIarg,
			sCode 		*pCodeArg )
{
  /* Description:
   *
   *	Execute the HyperScript code pointed to by pCode. 
   *
   * Arguments:
   *
   *	pAI							[R]
   *	- pointer to instance object
   *
   *	pCode							[R]
   *	- pointer to code object
   *
   *	pHyp							[R]
   *	- pointer to HyperScript program
   *
   *	pFrame							[R]
   *	- pointer to program frame
   *
   * Return value:
   *
   *	none
   */
   
  /* When a recoverable error occurs (that which we can handle), we always
   * jump back to this point and then return FALSE.
   * We always jump back to gsJmpStack[1] when going to an IDLE state.
   */
  /* longjmp protection */
  sInstance* pAI = pAIarg ;
  sCode* pCode = pCodeArg ;

  int jmpVal ;

  if ( (jmpVal = setjmp ( gsJmpStack[giJmpLevel] ) ) ) return jmpVal ;
  
  if ( guDebugFlags & DEBUG_POSTFIX_EXEC )
    gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_POSTFIX_EXEC,
    			"exec : '%s'", pCode->token ) ;
        
  /* Execute it */
  pCode->function ( pAI, pCode, FALSE ) ;

  return COND_NORMAL ;

}
