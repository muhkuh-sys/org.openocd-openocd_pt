#include "muhkuh_openocd.h"

#include <unistd.h>

/*-------------------------------------*/
/* openocd includes */

#include "openocd.c"

#include "target/target_type.h"
#include "target/target_request.h"


#define MUHKUH_OUTPUT_BUFFER_MAGIC 0x686f6f4d

typedef struct MUHKUH_OUTPUT_BUFFER_STRUCT
{
	unsigned long ulMagic;
	PFN_MUHKUH_OPENOCD_OUTPUT_HANDLER_T pfnOutput;
	void *pvUser;
	size_t sizUsed;
	size_t sizMax;
	char *pcBuffer;
} MUHKUH_OUTPUT_BUFFER_T;


int muhkuh_output_handler(struct command_context *ptContext, const char *pcLine)
{
	MUHKUH_OUTPUT_BUFFER_T *ptHandle;
	size_t sizUsed;
	size_t sizMax;
	size_t sizLine;
	size_t sizBufferNew;
	char *pcBuffer;
	char *pcBufferNew;
	PFN_MUHKUH_OPENOCD_OUTPUT_HANDLER_T pfnOutput;


	if( pcLine!=NULL )
	{
		/* Is the handle valid? */
		ptHandle = (MUHKUH_OUTPUT_BUFFER_T*)(ptContext->output_handler_priv);
		if( ptHandle!=NULL && ptHandle->ulMagic==MUHKUH_OUTPUT_BUFFER_MAGIC && ptHandle->pfnOutput!=NULL )
		{
			pfnOutput = ptHandle->pfnOutput;
			sizUsed = ptHandle->sizUsed;
			sizMax = ptHandle->sizMax;
			pcBuffer = ptHandle->pcBuffer;

			/* Is data in the line? */
			sizLine = strlen(pcLine);
			if( sizLine>0 )
			{
				/* Can the line be printed as it is?
				 * This is possible if...
				 *  1) the line ends with a newline and
				 *  2) the buffer is empty.
				 */
				if( pcLine[sizLine-1]=='\n' && sizUsed==0 )
				{
					/* Just pass the line without the newline to the handler. */
					pfnOutput(ptHandle->pvUser, pcLine, sizLine-1);
				}
				else
				{
					/* Append the line to the buffer. */
					sizBufferNew = sizUsed + sizLine;
					/* Does a buffer already exist? */
					if( sizMax==0 )
					{
						/* No -> allocate a new buffer. */
						pcBufferNew = (char*)malloc(sizBufferNew);
					}
					else
					{
						/* Does the combined data fit into the existing buffer? */
						if( sizBufferNew<=sizMax )
						{
							/* Yes -> keep the current buffer. */
							pcBufferNew = pcBuffer;
							sizBufferNew = sizMax;
						}
						else
						{
							/* No -> reallocate the buffer. */
							pcBufferNew = (char*)realloc(pcBuffer, sizBufferNew);
						}
					}
					if( pcBufferNew!=NULL )
					{
						/* Use the new buffer. */
						pcBuffer = pcBufferNew;
						sizMax = sizBufferNew;

						/* Append the new data to the end of the buffer. */
						memcpy(pcBuffer+sizUsed, pcLine, sizLine);
						sizUsed += sizLine;

						/* Does the buffer hold a complete line now? */
						if( pcLine[sizUsed-1]=='\n' )
						{
							pfnOutput(ptHandle->pvUser, pcBuffer, sizUsed-1);

							free(pcBuffer);
							pcBuffer = NULL;
							sizUsed = 0;
							sizMax = 0;
						}

						/* Update the handle. */
						ptHandle->sizUsed = sizUsed;
						ptHandle->sizMax = sizMax;
						ptHandle->pcBuffer = pcBuffer;
					}
				}
			}
		}
		else
		{
			/* No valid output handler found. Just print the data. */
			LOG_USER_N("%s", pcLine);
		}
	}

	return ERROR_OK;
}


void *muhkuh_openocd_init(const char *pcScriptSearchDir, PFN_MUHKUH_OPENOCD_OUTPUT_HANDLER_T pfnOutputHandler, void *pvOutputHanderData)
{
	/* Initialize command line interface. */
	struct command_context *ptCmdCtx;
	void *pvResult;
	int iResult;
	MUHKUH_OUTPUT_BUFFER_T *ptBuffer;


	pvResult = NULL;

	/* Allocate a new output buffer. */
	ptBuffer = (MUHKUH_OUTPUT_BUFFER_T*)malloc(sizeof(MUHKUH_OUTPUT_BUFFER_T));
	if( ptBuffer!=NULL )
	{
		ptBuffer->ulMagic = MUHKUH_OUTPUT_BUFFER_MAGIC;
		ptBuffer->pfnOutput = pfnOutputHandler;
		ptBuffer->pvUser = pvOutputHanderData;
		ptBuffer->sizUsed = 0;
		ptBuffer->sizMax = 0;
		ptBuffer->pcBuffer = NULL;

		ptCmdCtx = setup_command_handler(NULL);
		if( ptCmdCtx!=NULL )
		{
			iResult = util_init(ptCmdCtx);
			if( iResult==ERROR_OK )
			{
				iResult = ioutil_init(ptCmdCtx);
				if( iResult==ERROR_OK )
				{
					iResult = rtt_init();
					if( iResult==ERROR_OK )
					{
						command_context_mode(ptCmdCtx, COMMAND_CONFIG);
						command_set_output_handler(ptCmdCtx, muhkuh_output_handler, ptBuffer);

						server_host_os_entry();

						/* Add a search path to the interpreter. */
						if( pcScriptSearchDir!=NULL )
						{
							add_script_search_dir(pcScriptSearchDir);
						}

						iResult = server_preinit();
						if( iResult==ERROR_OK )
						{
							/* NOTE: do not call server_init. */
							pvResult = ptCmdCtx;
						}
					}
				}
			}
		}
	}

	return pvResult;
}



int muhkuh_openocd_get_result(void *pvContext, char *pcBuffer, size_t sizBufferMax)
{
	struct command_context *ptCmdCtx;
	Jim_Interp *ptInterp;
	Jim_Obj *ptResultObj;
	const char *pcResult;
	int iResLen;
	int iResult;


	/* Be pessimistic. */
	iResult = ERROR_FAIL;

	ptCmdCtx = (struct command_context *)pvContext;
	if( ptCmdCtx!=NULL )
	{
		ptInterp = ptCmdCtx->interp;

		ptResultObj = Jim_GetResult(ptInterp);
		if( ptResultObj!=NULL )
		{
			iResLen = 0;
			pcResult = Jim_GetString(ptResultObj, &iResLen);

			/* Does the result still exist? */
			if( pcResult!=NULL && iResLen>0 )
			{
				/* The size of the result must be smaller and not equal because it must be terminated with a 0 byte. */
				if( iResLen<sizBufferMax )
				{
					strncpy(pcBuffer, pcResult, iResLen);
					pcBuffer[iResLen] = '\0';
					iResult = ERROR_OK;
				}
			}
		}
	}

	return iResult;
}



int muhkuh_openocd_get_result_alloc(void *pvContext, char **ppcBuffer, size_t *psizBuffer)
{
	struct command_context *ptCmdCtx;
	Jim_Interp *ptInterp;
	Jim_Obj *ptResultObj;
	const char *pcResult;
	char *pcBuffer;
	int iResLen;
	int iResult;


	/* Be pessimistic. */
	iResult = ERROR_FAIL;
	pcBuffer = NULL;
	iResLen = 0;

	ptCmdCtx = (struct command_context *)pvContext;
	if( ptCmdCtx!=NULL )
	{
		ptInterp = ptCmdCtx->interp;

		ptResultObj = Jim_GetResult(ptInterp);
		if( ptResultObj!=NULL )
		{
			pcResult = Jim_GetString(ptResultObj, &iResLen);

			/* Does the result still exist? */
			if( pcResult!=NULL && iResLen>0 )
			{
				/* Allocate a new buffer for the result. Add 1 more byte for the terminating 0. */
				pcBuffer = (char*)malloc(iResLen + 1);
				if( pcBuffer!=NULL )
				{
					memcpy(pcBuffer, pcResult, iResLen);
					pcBuffer[iResLen] = '\0';

					*ppcBuffer = pcBuffer;
					*psizBuffer = iResLen;

					iResult = ERROR_OK;
				}
			}
		}
	}

	return iResult;
}



int muhkuh_openocd_command_run_line(void *pvContext, char *pcLine)
{
	struct command_context *ptCmdCtx;
	int iResult;


	/* Be pessimistic... */
	iResult = ERROR_FAIL;

	if( pvContext!=NULL )
	{
		ptCmdCtx = (struct command_context *)pvContext;

		iResult = command_run_line(ptCmdCtx, pcLine);
	}

	return iResult;
}



void muhkuh_openocd_uninit(void *pvContext)
{
	struct command_context *ptCmdCtx;
	MUHKUH_OUTPUT_BUFFER_T *ptHandle;


	if( pvContext!=NULL )
	{
		ptCmdCtx = (struct command_context *)pvContext;

		unregister_all_commands(ptCmdCtx, NULL);

		/* free all DAP and CTI objects */
		dap_cleanup_all();
		arm_cti_cleanup_all();

		adapter_quit();

		server_host_os_close();

		/* Shutdown commandline interface */
		command_exit(ptCmdCtx);

		rtt_exit();
		free_config();

		/* Close the output handler. */
		ptHandle = (MUHKUH_OUTPUT_BUFFER_T*)(ptCmdCtx->output_handler_priv);
		command_set_output_handler(ptCmdCtx, NULL, NULL);
		if( ptHandle!=NULL && ptHandle->ulMagic==MUHKUH_OUTPUT_BUFFER_MAGIC )
		{
			if( ptHandle->pcBuffer!=NULL )
			{
				/* Send any waiting data. */
				if( ptHandle->pfnOutput!=NULL && ptHandle->sizUsed!=0 )
				{
					ptHandle->pfnOutput(ptHandle->pvUser, ptHandle->pcBuffer, ptHandle->sizUsed);
				}
				/* Free the buffer. */
				free(ptHandle->pcBuffer);
			}
			free(ptHandle);
		}
	}
}



/* Read a byte (8bit) from the netX. */
int muhkuh_openocd_read_data08(void *pvContext, uint32_t ulNetxAddress, uint8_t *pucData)
{
	struct command_context *ptCmdCtx;
	struct target *ptTarget;
	int iResult;


	/* Cast the handle to the command context. */
	ptCmdCtx = (struct command_context*)pvContext;

	/* Get the target from the command context. */
	ptTarget = get_current_target(ptCmdCtx);

	/* Read the data from the netX. */
	iResult = target_read_u8(ptTarget, ulNetxAddress, pucData);
	if( iResult==ERROR_OK )
	{
		iResult = 0;
	}
	else
	{
		iResult = 1;
	}

	/* FIXME: is this really necessary? */
//	usleep(1000);

	return iResult;
}


/* Read a word (16bit) from the netX. */
int muhkuh_openocd_read_data16(void *pvContext, uint32_t ulNetxAddress, uint16_t *pusData)
{
	struct command_context *ptCmdCtx;
	struct target *ptTarget;
	int iResult;


	/* cast the handle to the command context */
	ptCmdCtx = (struct command_context*)pvContext;

	/* get the target from the command context */
	ptTarget = get_current_target(ptCmdCtx);

	/* read the data from the netX */
	iResult = target_read_u16(ptTarget, ulNetxAddress, pusData);
	if( iResult==ERROR_OK )
	{
		iResult = 0;
	}
	else
	{
		iResult = 1;
	}

	/* FIXME: is this really necessary? */
//	usleep(1000);

	return iResult;
}


/* Read a long (32bit) from the netX. */
int muhkuh_openocd_read_data32(void *pvContext, uint32_t ulNetxAddress, uint32_t *pulData)
{
	struct command_context *ptCmdCtx;
	struct target *ptTarget;
	int iResult;


	/* cast the handle to the command context */
	ptCmdCtx = (struct command_context*)pvContext;

	/* get the target from the command context */
	ptTarget = get_current_target(ptCmdCtx);

	/* read the data from the netX */
	iResult = target_read_u32(ptTarget, ulNetxAddress, pulData);
	if( iResult==ERROR_OK )
	{
		iResult = 0;
	}
	else
	{
		iResult = 1;
	}

	/* FIXME: is this really necessary? */
//	usleep(1000);

	return iResult;
}


/* Read a byte array from the netX. */
int muhkuh_openocd_read_image(void *pvContext, uint32_t ulNetxAddress, uint8_t *pucData, uint32_t ulSize)
{
	struct command_context *ptCmdCtx;
	struct target *ptTarget;
	int iResult;


	/* cast the handle to the command context */
	ptCmdCtx = (struct command_context*)pvContext;

	/* get the target from the command context */
	ptTarget = get_current_target(ptCmdCtx);

	/* read the data from the netX */
	iResult = target_read_buffer(ptTarget, ulNetxAddress, ulSize, pucData);
	if( iResult==ERROR_OK )
	{
		iResult = 0;
	}
	else
	{
		iResult = 1;
	}

	/* FIXME: is this really necessary? */
//	usleep(1000);

	return iResult;
}


/* Write a byte (8bit) to the netX. */
int muhkuh_openocd_write_data08(void *pvContext, uint32_t ulNetxAddress, uint8_t ucData)
{
	struct command_context *ptCmdCtx;
	struct target *ptTarget;
	int iResult;


	/* cast the handle to the command context */
	ptCmdCtx = (struct command_context*)pvContext;

	/* get the target from the command context */
	ptTarget = get_current_target(ptCmdCtx);

	/* read the data from the netX */
	iResult = target_write_u8(ptTarget, ulNetxAddress, ucData);
	if( iResult==ERROR_OK )
	{
		iResult = 0;
	}
	else
	{
		iResult = 1;
	}

	/* FIXME: is this really necessary? */
//	usleep(1000);

	return iResult;
}


/* Write a word (16bit) to the netX. */
int muhkuh_openocd_write_data16(void *pvContext, uint32_t ulNetxAddress, uint16_t usData)
{
	struct command_context *ptCmdCtx;
	struct target *ptTarget;
	int iResult;


	/* cast the handle to the command context */
	ptCmdCtx = (struct command_context*)pvContext;

	/* get the target from the command context */
	ptTarget = get_current_target(ptCmdCtx);

	/* read the data from the netX */
	iResult = target_write_u16(ptTarget, ulNetxAddress, usData);
	if( iResult==ERROR_OK )
	{
		iResult = 0;
	}
	else
	{
		iResult = 1;
	}

	/* FIXME: is this really necessary? */
//	usleep(1000);

	return iResult;
}


/* Write a long (32bit) to the netX. */
int muhkuh_openocd_write_data32(void *pvContext, uint32_t ulNetxAddress, uint32_t ulData)
{
	struct command_context *ptCmdCtx;
	struct target *ptTarget;
	int iResult;


	/* cast the handle to the command context */
	ptCmdCtx = (struct command_context*)pvContext;

	/* get the target from the command context */
	ptTarget = get_current_target(ptCmdCtx);

	/* read the data from the netX */
	iResult = target_write_u32(ptTarget, ulNetxAddress, ulData);
	if( iResult==ERROR_OK )
	{
		iResult = 0;
	}
	else
	{
		iResult = 1;
	}

	/* FIXME: is this really necessary? */
//	usleep(1000);

	return iResult;
}


/* Write a byte array to the netX. */
int muhkuh_openocd_write_image(void *pvContext, uint32_t ulNetxAddress, const uint8_t *pucData, uint32_t ulSize)
{
	struct command_context *ptCmdCtx;
	struct target *ptTarget;
	int iResult;


	/* Cast the handle to the command context. */
	ptCmdCtx = (struct command_context*)pvContext;

	/* Get the target from the command context. */
	ptTarget = get_current_target(ptCmdCtx);

	/* write the data to the netX */
	iResult = target_write_buffer(ptTarget, ulNetxAddress, ulSize, pucData);
	if( iResult==ERROR_OK )
	{
		iResult = 0;
	}
	else
	{
		iResult = 1;
	}

	/* FIXME: is this really necessary? */
//	usleep(1000);

	return iResult;
}

/* Structure to hold the address of a line and its size.
   Used as output_handler_priv.
 */
typedef struct {
	uint8_t* pucDccData;
	unsigned long ulDccDataSize; 	/* data size without the terminating null byte. */
} DCC_LINE_BUFFER_T;

/* Store a 0-terminated line in the buffer. */
void dcc_line_buffer_put(DCC_LINE_BUFFER_T* ptBuffer, const char *line)
{
	unsigned long ulDccDataSize;
	uint8_t* pucDccData;

	if (line == NULL)
	{
		fprintf(stderr, "dcc_line_buffer_put: line == NULL\n");
	}
	else
	{
		ulDccDataSize = strlen(line);
		pucDccData = malloc(ulDccDataSize+1);
		if (pucDccData != NULL)
		{
			strcpy(pucDccData, line);
			ptBuffer->ulDccDataSize = ulDccDataSize;
			ptBuffer->pucDccData = pucDccData;
		}
		else
		{
			fprintf(stderr, "!Error: failed to allocate buffer for DCC debug message\n");
		}

	}

}

/* Remove the current line from the buffer, if any.
   fUsed = 1 if any line which may be in the buffer has already been used.
   fUsed = 0 if any line in the buffer has not been used (prints a warning).
 */
void dcc_line_buffer_clear(DCC_LINE_BUFFER_T* ptBuffer, int fUsed)
{
	if (ptBuffer->pucDccData != NULL)
	{
		if (fUsed == 0)
		{
			fprintf(stderr,"dropping debug message (not passed to Lua callback): %s\n", ptBuffer->pucDccData);
		}

		free(ptBuffer->pucDccData);
		ptBuffer->pucDccData = NULL;
		ptBuffer->ulDccDataSize = 0;
	}
}

int romloader_jtag_command_output_handler(struct command_context *context, const char *line)
{
	DCC_LINE_BUFFER_T* ptBuffer = (DCC_LINE_BUFFER_T*) context->output_handler_priv;
	if (line != NULL)
	{
		dcc_line_buffer_clear(ptBuffer, 0);
		dcc_line_buffer_put(ptBuffer, line);
	}

	return ERROR_OK;
}

int muhkuh_openocd_call(void *pvContext, uint32_t ulNetxAddress, uint32_t ulR0, PFN_MUHKUH_CALL_PRINT_CALLBACK pfnCallback, void *pvCallbackUserData)
{
	struct command_context *ptCmdCtx;
	struct target *ptTarget;
	int iOocdResult;
	int iResult;
	char strCmd[32];
	int fIsRunning;
	enum target_state tState;
	const char *pcTargetTypeName;
	command_output_handler_t pfnOldOutputHandler;
	void *pvOldOutputHandler;

	DCC_LINE_BUFFER_T tDccLineBuffer = {NULL, 0};

	/* Cast the handle to the command context. */
	ptCmdCtx = (struct command_context*)pvContext;

	/* Get the target from the command context. */
	ptTarget = get_current_target(ptCmdCtx);

	/* Get the target type name */
	pcTargetTypeName = target_type_name(ptTarget);
//	fprintf(stderr, "target type: %s\n", pcTargetTypeName);

	/* Expect failure. */
	iResult = -1;

	/* Pass the pointer to parameters in register r0 on ARM, a0 on netIOL. */
	memset(strCmd, 0, sizeof(strCmd));
	
	if (0==strcmp(pcTargetTypeName, "hinetiol"))
	{
		snprintf(strCmd, sizeof(strCmd)-1, "reg a0 0x%08X", ulR0);
	}
	else
	{
		snprintf(strCmd, sizeof(strCmd)-1, "reg r0 0x%08X", ulR0);
	}

//	fprintf(stderr, "cmd: %s\n", strCmd);
	iOocdResult = command_run_line(ptCmdCtx, strCmd);
//	fprintf(stderr, "result: %d\n", iOocdResult);
	if( iOocdResult!=ERROR_OK )
	{
		fprintf(stderr, "muhkuh_openocd_call: set r0/a0 failed!\n");
	}
	else
	{

		/* Set the PC and then resume. 
		   Resume <address> does not work on target hinetiol. */
		snprintf(strCmd, sizeof(strCmd)-1, "reg pc 0x%08X", ulNetxAddress);
//		fprintf(stderr, "cmd: %s\n", strCmd);
		iOocdResult = command_run_line(ptCmdCtx, strCmd);
//		fprintf(stderr, "result: %d\n", iOocdResult);
		if( iOocdResult!=ERROR_OK )
		{
			fprintf(stderr, "muhkuh_openocd_call: set pc failed!\n");
		}
		else
		{
			memset(strCmd, 0, sizeof(strCmd));
			snprintf(strCmd, sizeof(strCmd)-1, "resume");
//			fprintf(stderr, "cmd: %s\n", strCmd);
			iOocdResult = command_run_line(ptCmdCtx, strCmd);
//			fprintf(stderr, "result: %d\n", iOocdResult);
			if( iOocdResult!=ERROR_OK )
			{
				fprintf(stderr, "muhkuh_openocd_call: resume failed!\n");
			}
			else
			{
				// redirect output handler, then grab messages, restore default output handler on halt
				pfnOldOutputHandler = ptCmdCtx->output_handler;
				pvOldOutputHandler = ptCmdCtx->output_handler_priv;
				command_set_output_handler(ptCmdCtx, &romloader_jtag_command_output_handler, &tDccLineBuffer);

				/* Wait for halt. */
				do
				{
//					usleep(1000*100);
					usleep(1000);
					ptTarget->type->poll(ptTarget);
					tState = ptTarget->state;
					//fprintf(stderr, "target state: %d\n", (unsigned long) tState);
					if( tState==TARGET_HALTED )
					{
//						fprintf(stderr, "call finished!\n");
						iResult = 0;
					}
					else
					{
						/* Execute the Lua callback. */
						fIsRunning = pfnCallback(pvCallbackUserData, tDccLineBuffer.pucDccData, tDccLineBuffer.ulDccDataSize);
						dcc_line_buffer_clear(&tDccLineBuffer, 1);

						if( fIsRunning==0 )
						{
							/* The operation was canceled, halt the target. */
							fprintf(stderr, "Call canceled by the user, stopping target...\n");
							iOocdResult = ptTarget->type->halt(ptTarget);
							if( iOocdResult!=ERROR_OK )
							{
								fprintf(stderr, "Failed to halt target: %d\n", iOocdResult);
							}
							break;
						}
						else
						{
							/* call OpenOCD timer callbacks */
							target_call_timer_callbacks();
						}
					}
				} while( tState!=TARGET_HALTED );

				/* call timer callbacks once more (to get remaining DCC output), then call the callback to consume this output */
				target_call_timer_callbacks();
				pfnCallback(pvCallbackUserData, tDccLineBuffer.pucDccData, tDccLineBuffer.ulDccDataSize);
				dcc_line_buffer_clear(&tDccLineBuffer, 1);

				command_set_output_handler(ptCmdCtx, pfnOldOutputHandler, pvOldOutputHandler);

				/* FIXME: is this really necessary? */
//				usleep(1000);
			}
		}
	}

	return iResult;
}
