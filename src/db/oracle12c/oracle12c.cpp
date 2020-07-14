#include "require.h"
#include "abio.h"
#include "lib.h"
#include "vendor.h"
#include "oracle12c.h"
#include "oracle12c_lib.h"




// start of variables for abio library comparability
char ABIOMASTER_CK_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_MASTER_SERVER_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_DATA_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_CATALOG_DB_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_VOLUME_DB_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_FILE_LIST_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_LOG_FOLDER[MAX_PATH_LENGTH];


struct portRule * portRule;
int portRuleNumber;

int tapeDriveTimeOut;
int tapeDriveDelayTime;
// end of variables for abio library comparability


char ABIOMASTER_CLIENT_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_CLIENT_CATALOG_DB_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_CLIENT_FILE_LIST_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_CLIENT_LOG_FOLDER[MAX_PATH_LENGTH];
char ORACLE12C_LOG_FOLDER[MAX_PATH_LENGTH];


struct ck command;				// communication kernel command
struct ckBackup commandData;	// communication kernel command data


//////////////////////////////////////////////////////////////////////////////////////////////////
// ABIO log option
int requestLogLevel;
char logJobID[JOB_ID_LENGTH];
int logModuleNumber;


///////////////////////////////////////////////////////////////////////////////////////////////////
// message transfer variables
va_trans_t midCK;			// message transfer id
int mtypeCK;				// message type number


///////////////////////////////////////////////////////////////////////////////////////////////////
// ip and port number
char masterIP[IP_LENGTH];					// master server ip address
char masterPort[PORT_LENGTH];				// master server ck port
char masterNodename[NODE_NAME_LENGTH];		// master server node name
char masterLogPort[PORT_LENGTH];			// master server httpd logger port
char serverIP[IP_LENGTH];					// backup server ip address
char serverPort[PORT_LENGTH];				// backup server ck port
char serverNodename[NODE_NAME_LENGTH];		// backup server node name
char clientIP[IP_LENGTH];					// client ip address
char clientPort[PORT_LENGTH];				// client ck port
char clientNodename[NODE_NAME_LENGTH];		// client node name

int maxOutboundBandwidth;					// Outbound 최대 Bandwidth


//////////////////////////////////////////////////////////////////////////////////////////////////
// backup/restore buffer variables
int readDiskBufferSize;			// backup read buffer의 크기
int writeDiskBufferSize;		// restore write buffer의 크기


//////////////////////////////////////////////////////////////////////////////////////////////////
// variables for oracle connection
char username[ORACLE_USERNAME_LENGTH];
char account[ORACLE_ACCOUNT_LENGTH];
char passwd[ORACLE_PASSWORD_LENGTH];
char sid[ORACLE_SID_LENGTH];
char home[ORACLE_HOME_LENGTH];
int deleteArchiveLog;
int ramainArchiveLogCount;


char environmentValueSid[MAX_PATH_LENGTH];
char environmentValueHome[MAX_PATH_LENGTH];

char sqlcmdUtil[MAX_PATH_LENGTH];
char queryFile[MAX_PATH_LENGTH];
char outputFile[MAX_PATH_LENGTH];


//////////////////////////////////////////////////////////////////////////////////////////////////
// backup/restore log
struct _backupLogDescription * backupLogDescription;
int backupLogDescriptionNumber;


static void Error();
static void Exit();


#ifdef __ABIO_UNIX
	int main(int argc, char ** argv)
#elif __ABIO_WIN
	int wmain(int argc, wchar_t ** argv)
#endif
	{
			
		char * convertFilename;
		int convertFilenameSize;
		
		
		
		// receive a message from ck
		#ifdef __ABIO_UNIX
			midCK = (va_trans_t)atoi(argv[1]);
			mtypeCK = atoi(argv[2]);
		#elif __ABIO_WIN
			#ifdef __ABIO_64
				midCK = (va_trans_t)_wtoi64(argv[1]);
			#else
				midCK = (va_trans_t)_wtoi(argv[1]);
			#endif
			mtypeCK = _wtoi(argv[2]);
		#endif
		
		va_msgrcv(midCK, mtypeCK, &command, sizeof(struct ck), 0);
		memcpy(&commandData, command.dataBlock, sizeof(struct ckBackup));
		
		
		// initialize a process
		#ifdef __ABIO_UNIX
			if (va_convert_string_to_utf8(ENCODING_UNKNOWN, argv[0], (int)strlen(argv[0]), (void **)&convertFilename, &convertFilenameSize) == 0)
			{
				Error();
				Exit();
				
				return 2;
			}
		#elif __ABIO_WIN
			if (va_convert_string_to_utf8(ENCODING_UTF16_LITTLE, (char *)argv[0], (int)wcslen(argv[0]) * sizeof(wchar_t), (void **)&convertFilename, &convertFilenameSize) == 0)
			{
				Error();
				Exit();
				
				return 2;
			}
		#endif
		
		if (InitProcess(convertFilename) < 0)
		{
			va_free(convertFilename);
			
			Error();
			Exit();
			
			return 2;
		}
		
		va_free(convertFilename);
		
		
		// 작업을 시작한다.
		if (commandData.jobType == BACKUP_JOB)
		{
			if (Backup() < 0)
			{
				Exit();
				
				return 2;
			}
		}
		else if (commandData.jobType == RESTORE_JOB)
		{
			if (Restore() < 0)
			{
				Exit();
				
				return 2;
			}
		}
		
		
		Exit();
		
		return 0;
	}


int InitProcess(char * filename)
{
	#ifdef __ABIO_WIN
		WSADATA wsaData;
		HANDLE token;
	#endif
	
	struct oracle12cDB oracle12c;
	
	int i;
	
	
	
	///////////////////////////////////////////////////////////////////////////////////////////////////
	// 1. 전역 변수들을 초기화한다.
	
	memset(ABIOMASTER_CLIENT_FOLDER, 0, sizeof(ABIOMASTER_CLIENT_FOLDER));
	memset(ABIOMASTER_CLIENT_CATALOG_DB_FOLDER, 0, sizeof(ABIOMASTER_CLIENT_CATALOG_DB_FOLDER));
	memset(ABIOMASTER_CLIENT_FILE_LIST_FOLDER, 0, sizeof(ABIOMASTER_CLIENT_FILE_LIST_FOLDER));
	memset(ABIOMASTER_CLIENT_LOG_FOLDER, 0, sizeof(ABIOMASTER_CLIENT_LOG_FOLDER));
	memset(ORACLE12C_LOG_FOLDER, 0, sizeof(ORACLE12C_LOG_FOLDER));
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// ABIO log option
	requestLogLevel = LOG_LEVEL_JOB;
	memset(logJobID, 0, sizeof(logJobID));
	logModuleNumber = MODULE_UNKNOWN;
	
	
	///////////////////////////////////////////////////////////////////////////////////////////////////
	// ip and port number
	memset(masterIP, 0, sizeof(masterIP));
	memset(masterPort, 0, sizeof(masterPort));
	memset(masterNodename, 0, sizeof(masterNodename));
	memset(masterLogPort, 0, sizeof(masterLogPort));
	memset(serverIP, 0, sizeof(serverIP));
	memset(serverPort, 0, sizeof(serverPort));
	memset(serverNodename, 0, sizeof(serverNodename));
	memset(clientIP, 0, sizeof(clientIP));
	memset(clientPort, 0, sizeof(clientPort));
	memset(clientNodename, 0, sizeof(clientNodename));
	
	maxOutboundBandwidth = 0;
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// backup/restore buffer variables
	readDiskBufferSize = DEFAULT_READ_DISK_BUFFER_SIZE;
	writeDiskBufferSize = DEFAULT_WRITE_DISK_BUFFER_SIZE;
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// variables for oracle connection
	memset(username, 0, sizeof(username));
	memset(account, 0, sizeof(account));
	memset(passwd, 0, sizeof(passwd));
	memset(sid, 0, sizeof(sid));
	memset(home, 0, sizeof(home));
	deleteArchiveLog = 0;
	ramainArchiveLogCount = ORACLE_DELETE_ARCHIVE_LOG_COUNT_DEFAULT;
	memset(environmentValueSid, 0, sizeof(environmentValueSid));
	memset(environmentValueHome, 0, sizeof(environmentValueHome));
	
	memset(sqlcmdUtil, 0, sizeof(sqlcmdUtil));
	memset(queryFile, 0, sizeof(queryFile));
	memset(outputFile, 0, sizeof(outputFile));
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// backup/restore log
	backupLogDescription = NULL;
	backupLogDescriptionNumber = 0;
	
	
	
	///////////////////////////////////////////////////////////////////////////////////////////////////
	// 2. 각 시스템에 맞게 process를 초기화한다.
	
	// set a resource limit
	va_setrlimit();
	
	// set the client working directory
	for (i = (int)strlen(filename) - 1; i > 0; i--)
	{
		if (filename[i] == FILE_PATH_DELIMITER)
		{
			strncpy(ABIOMASTER_CLIENT_FOLDER, filename, i);
			
			break;
		}
	}
	
	// set job id and module number for debug log of ABIO common library
	strcpy(logJobID, commandData.jobID);
	logModuleNumber = MODULE_DB_ORACLE12C_SCRIPT_ORACLE;
	
	// set master server ip and port
	strcpy(masterIP, commandData.client.masterIP);
	strcpy(masterPort, commandData.client.masterPort);
	strcpy(masterLogPort, commandData.client.masterLogPort);
	
	// set backup server ip and port
	strcpy(serverIP, command.sourceIP);
	strcpy(serverPort, command.sourcePort);
	strcpy(serverNodename, command.sourceNodename);
	
	// set client ip and port
	strcpy(clientIP, commandData.client.ip);
	strcpy(clientPort, commandData.client.port);
	strcpy(clientNodename, commandData.client.nodename);
	
	// set account and password
	memcpy(&oracle12c, commandData.db.dbData, sizeof(struct oracle12cDB));
	printf("%s\n",oracle12c.username);
	printf("%s\n",oracle12c.account);
	printf("%s\n",oracle12c.passwd);
	printf("%s\n",oracle12c.sid);
	printf("%s\n",oracle12c.home);
	printf("%d\n",oracle12c.deleteArchive);

	strcpy(username, oracle12c.username);
	strcpy(account, oracle12c.account);
	strcpy(passwd, oracle12c.passwd);
	strcpy(sid, oracle12c.sid);
	strcpy(home, oracle12c.home);
	deleteArchiveLog = oracle12c.deleteArchive;	

	#ifdef __ABIO_WIN
		va_slash_to_backslash(home);
	#endif
	
	// set oracle environment
	sprintf(environmentValueSid, "ORACLE_SID=%s", sid);
	#ifdef __ABIO_UNIX
		putenv(environmentValueSid);
	#elif __ABIO_WIN
		_putenv(environmentValueSid);
	#endif
	
	sprintf(environmentValueHome, "ORACLE_HOME=%s", home);
	#ifdef __ABIO_UNIX
		putenv(environmentValueHome);
	#elif __ABIO_WIN
		_putenv(environmentValueHome);
	#endif
	
	
	// set the sqlcmd utility
	sprintf(sqlcmdUtil, "%s%cbin%c%s", home, FILE_PATH_DELIMITER, FILE_PATH_DELIMITER, ORACLE_SQLCMD_UTILITY);
	sprintf(queryFile, "oracle_%d.sql", va_getpid());
	sprintf(outputFile, "oracle_%d.out", va_getpid());
	
	
	#ifdef __ABIO_WIN
		// windows socket을 open한다.
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) 
		{
			va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_ORACLE, FUNCTION_InitProcess, ERROR_WIN_INIT_WIN_SOCK, &commandData);
			
			return -1;
		}
	#endif
	
	
	
	///////////////////////////////////////////////////////////////////////////////////////////////////
	// 3. process 실행에 필요한 설정 값들을 읽어온다.
	
	// get a module configuration
	if (GetModuleConfiguration() < 0)
	{
		return -1;
	}
	
	LoadLogDescription();
	
	
	
	///////////////////////////////////////////////////////////////////////////////////////////////////
	// 4. process 보안 권한 변경을 시도한다.
	
	#ifdef __ABIO_WIN
		if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &token) == TRUE)
		{
			if (SetPrivilege(token, ABIO_NT_PRIVILEGE_SE_BACKUP_NAME, 1) == FALSE)
			{
				va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_ORACLE, FUNCTION_InitProcess, ERROR_WIN_GET_BACKUP_PRIVILEGE, &commandData);
				
				return -1;
	  		}
			
	  		if (SetPrivilege(token, ABIO_NT_PRIVILEGE_SE_RESTORE_NAME, 1) == FALSE)
	  		{
				va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_ORACLE, FUNCTION_InitProcess, ERROR_WIN_GET_BACKUP_PRIVILEGE, &commandData);
				
				return -1;
	  		}
			
	  		if (SetPrivilege(token, ABIO_NT_PRIVILEGE_SE_SECURITY_NAME, 1) == FALSE)
			{
				va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_ORACLE, FUNCTION_InitProcess, ERROR_WIN_GET_BACKUP_PRIVILEGE, &commandData);
				
				return -1;
	  		}
		}
		else
		{
			va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_ORACLE, FUNCTION_InitProcess, ERROR_WIN_GET_BACKUP_PRIVILEGE, &commandData);
			
			return -1;
		}
	#endif
	
	
	return 0;
}

void LoadLogDescription()
{
	struct confOption * moduleConfig;
	int moduleConfigNumber;
	
	int i;
	
	
	
	// abio log description file을 로드한다.
	if (va_load_conf_file(ABIOMASTER_CLIENT_FOLDER, ABIOMASTER_BACKUP_LOG_DESCRIPTION, &moduleConfig, &moduleConfigNumber) > 0)
	{
		backupLogDescriptionNumber = moduleConfigNumber;
		backupLogDescription = (struct _backupLogDescription *)malloc(sizeof(struct _backupLogDescription) * backupLogDescriptionNumber);
		
		for (i = 0; i < backupLogDescriptionNumber; i++)
		{
			backupLogDescription[i].code = moduleConfig[i].optionName;
			backupLogDescription[i].description = moduleConfig[i].optionValue;
		}
		
		va_free(moduleConfig);
	}
}

int GetModuleConfiguration()
{
	struct confOption * moduleConfig;
	int moduleConfigNumber;
	
	int i;
	
	
	
	if (va_load_conf_file(ABIOMASTER_CLIENT_FOLDER, ABIOMASTER_ORACLE12C_CONFIG, &moduleConfig, &moduleConfigNumber) > 0)
	{
		for (i = 0; i < moduleConfigNumber; i++)
		{
			if (!strcmp(moduleConfig[i].optionName, CLIENT_CATALOG_DB_FOLDER_OPTION_NAME))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(ABIOMASTER_CLIENT_CATALOG_DB_FOLDER) - 1)
				{
					strcpy(ABIOMASTER_CLIENT_CATALOG_DB_FOLDER, moduleConfig[i].optionValue);
				}
			}
			else if (!strcmp(moduleConfig[i].optionName, CLIENT_FILE_LIST_FOLDER_OPTION_NAME))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(ABIOMASTER_CLIENT_FILE_LIST_FOLDER) - 1)
				{
					strcpy(ABIOMASTER_CLIENT_FILE_LIST_FOLDER, moduleConfig[i].optionValue);
				}
			}
			else if (!strcmp(moduleConfig[i].optionName, CLIENT_LOG_FOLDER_OPTION_NAME))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(ABIOMASTER_CLIENT_LOG_FOLDER) - 1)
				{
					strcpy(ABIOMASTER_CLIENT_LOG_FOLDER, moduleConfig[i].optionValue);
				}
			}
			else if (!strcmp(moduleConfig[i].optionName, ABIOMASTER_MAX_OUTBOUND_BANDWIDTH_OPTION_NAME))
			{
				maxOutboundBandwidth = atoi(moduleConfig[i].optionValue);
			}
			else if (!strcmp(moduleConfig[i].optionName, CLIENT_READ_DISK_BUFFER_SIZE_OPTION_NAME))
			{
				readDiskBufferSize = atoi(moduleConfig[i].optionValue);
			}
			else if (!strcmp(moduleConfig[i].optionName, CLIENT_WRITE_DISK_BUFFER_SIZE_OPTION_NAME))
			{
				writeDiskBufferSize = atoi(moduleConfig[i].optionValue);
			}
			else if (!strcmp(moduleConfig[i].optionName, ORACLE_LOG_FOLDER_OPTION_NAME))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(ORACLE12C_LOG_FOLDER) - 1)
				{
					strcpy(ORACLE12C_LOG_FOLDER, moduleConfig[i].optionValue);
				}
			}
			else if (!strcmp(moduleConfig[i].optionName, CLIENT_REMAIN_ARCHIVE_LOG_COUNT))
			{
				ramainArchiveLogCount = atoi(moduleConfig[i].optionValue);
			}
		}
		
		for (i = 0; i < moduleConfigNumber; i++)
		{
			va_free(moduleConfig[i].optionName);
			va_free(moduleConfig[i].optionValue);
		}
		va_free(moduleConfig);
	}
	else
	{
		va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_ORACLE, FUNCTION_GetModuleConfiguration, ERROR_CONFIG_OPEN_CLIENT_CONFIG_FILE, &commandData);
		
		return -1;
	}
	
	
	// backup read disk buffer size가 올바른 값인지 확인한다.
	if (readDiskBufferSize % 1024 != 0)
	{
		readDiskBufferSize = DEFAULT_READ_DISK_BUFFER_SIZE;
	}
	
	// restore write disk buffer size가 올바른 값인지 확인한다.
	if (writeDiskBufferSize % 1024 != 0)
	{
		writeDiskBufferSize = DEFAULT_WRITE_DISK_BUFFER_SIZE;
	}
	
	
	return 0;
}

// send control information to communication kernel
void SendCommand(int request, int reply, int execute, char * requestCommand, char * subCommand, char * executeCommand, char * sourceIP, char * sourcePort, char * sourceNodename, char * destinationIP, char * destinationPort, char * destinationNodename)
{
	struct ck sendCommand;
	
	
	
	memset(&sendCommand, 0, sizeof(struct ck));
	
	sendCommand.request = request;
	sendCommand.reply = reply;
	sendCommand.execute = execute;
	strcpy(sendCommand.requestCommand, requestCommand);
	strcpy(sendCommand.subCommand, subCommand);
	strcpy(sendCommand.executeCommand, executeCommand);
	strcpy(sendCommand.sourceIP, sourceIP);
	strcpy(sendCommand.sourcePort, sourcePort);
	strcpy(sendCommand.sourceNodename, sourceNodename);
	strcpy(sendCommand.destinationIP, destinationIP);
	strcpy(sendCommand.destinationPort, destinationPort);
	strcpy(sendCommand.destinationNodename, destinationNodename);
	
	memcpy(sendCommand.dataBlock, &commandData, sizeof(struct ckBackup));
	
	// send control information to message transfer
	va_msgsnd(midCK, 1, &sendCommand, sizeof(struct ck), 0);
}



static void Error()
{
	///////////////////////////////////////////////////////////////////////////////////////////////////
	// 1. backup server에 에러가 발생했음을 알린다.
	
	// backup server에게 client에서 에러가 발생했음을 알려준다.
	SendCommand(1, 0, 1, MODULE_NAME_SLAVE_BACKUP, "ERROR", MODULE_NAME_DB_ORACLE12C, clientIP, clientPort, clientNodename, serverIP, serverPort, serverNodename);
}

static void Exit()
{
	int i;
	
	
	
	for (i = 0; i < backupLogDescriptionNumber; i++)
	{
		va_free(backupLogDescription[i].description);
	}
	va_free(backupLogDescription);
	
	#ifdef __ABIO_WIN
		// 윈속 제거
		WSACleanup();
	#endif
}

int ExecuteSql(char * query,char * query2)
{

	char argQueryFile[MAX_PATH_LENGTH];
	char argQueryInfo[MAX_PATH_LENGTH];	

	
	int r;
	memset(argQueryFile, 0, sizeof(argQueryFile));
	memset(argQueryInfo,0,sizeof(argQueryInfo));
	
	
	// 실행할 query 파일을 만든다.
	if (MakeQueryFile(query,query2) != 0)
	{
		return -1;
	}
	
	if(strcmp(account,"sys")==0)
	{
		sprintf(argQueryInfo, "%s/%s@%s as sysdba", account, passwd, sid);
	}
	else
	{
		sprintf(argQueryInfo, "%s/%s@%s", account, passwd, sid);
	}
	// query를 실행한다.
	//sprintf(argQueryFile, "@\"%s%c%s\"", ABIOMASTER_CLIENT_FOLDER, FILE_PATH_DELIMITER, queryFile);
	#ifdef __ABIO_WIN
		sprintf(argQueryFile, "@\"%s%c%s\"", ABIOMASTER_CLIENT_FOLDER, FILE_PATH_DELIMITER, queryFile);
		r = va_spawn_process(WAIT_PROCESS, sqlcmdUtil, "-S", argQueryInfo, argQueryFile, NULL);	
	#else
		sprintf(argQueryFile, "@%s%c%s", ABIOMASTER_CLIENT_FOLDER, FILE_PATH_DELIMITER, queryFile);
		
		r = va_spawn_process(WAIT_PROCESS, username, sqlcmdUtil, "-S", argQueryInfo, argQueryFile, NULL);
	#endif
	
	if (r  != 0)
	{
		if (r == -1)
		{
			SendJobLog(LOG_MSG_ID_ORACLE_QUERY_ERROR_EXECUTE_SCRIPT, "");
		}
		else
		{
			PrintQueryError();
		}
		
		return 2;
	}
	
	
	// sql query를 실행하면서 에러가 발생했는지 확인한다.
	if (CheckQueryError(query) != 0)
	{
		PrintQueryError();
		
		return 2;
	}
	
	
	return 0;
}
int MakeQueryFile(char * query,char * query2)
{
	char ** lines;	
	char spoolQuery[MAX_PATH_LENGTH];
	int query_line_number;

		
	
	// initialize variables
	memset(spoolQuery, 0, sizeof(spoolQuery));
	query_line_number = 0;
	
	if (query2 != NULL)
	{
		query_line_number = 6;
		sprintf(spoolQuery, ORACLE12C_SPOOL_ON_QUERY, ABIOMASTER_CLIENT_FOLDER, FILE_PATH_DELIMITER, outputFile);

		lines = (char **)malloc(sizeof(char *) * query_line_number);
		
		
		lines[0] = ORACLE12C_LINESIZE_QUERY;
		lines[1] = spoolQuery;
		lines[2] = query;		
		lines[3] = query2;
		lines[4] = ORACLE12C_SPOOL_OFF_QUERY;
		lines[5] = ORACLE12C_QUIT_QUERY;
	
	}
	else
	{
		query_line_number = 5;
		sprintf(spoolQuery, ORACLE12C_SPOOL_ON_QUERY, ABIOMASTER_CLIENT_FOLDER, FILE_PATH_DELIMITER, outputFile);

		lines = (char **)malloc(sizeof(char *) * query_line_number);		
		
		lines[0] = ORACLE12C_LINESIZE_QUERY;
		lines[1] = spoolQuery;
		lines[2] = query;			
		lines[3] = ORACLE12C_SPOOL_OFF_QUERY;
		lines[4] = ORACLE12C_QUIT_QUERY;

	}
	if (va_write_text_file_lines(ABIOMASTER_CLIENT_FOLDER, queryFile, lines, NULL, query_line_number) != 0)
	{
		va_free(lines);
		return -1;
	}
	va_free(lines);
	
	
	return 0;
}


int CheckQueryError(char * query)
{
	char ** lines;
	int lineNumber;
	char executeQuery[1024];
	
	int errorOccured;
	
	int i;
	
	
	
	// 실행한 query가 output 파일에 다시 나타나면 에러로 판단한다.
	memset(executeQuery, 0, sizeof(executeQuery));
	strcpy(executeQuery, query);
	
	// 실행한 query가 output 파일에 다시 나타날 때는 query뒤의 ; 이 없다. 그래서 output 파일에서 확인할 query에서 ; 을 뺀다.
	executeQuery[strlen(executeQuery) - 1] = '\0';
	
	errorOccured = 0;
	
	if ((lineNumber = va_load_text_file_lines(ABIOMASTER_CLIENT_FOLDER, outputFile, &lines, NULL)) > 0)
	{
		for (i = 0; i < lineNumber; i++)
		{
			if (lines[i][0] == '\0')
			{
				continue;
			}
			
			if (strstr(lines[i], executeQuery) != NULL || strstr(lines[i], "ERROR:") != NULL)
			{
				errorOccured = 1;
				
				break;
			}
		}
		
		for (i = 0; i < lineNumber; i++)
		{
			va_free(lines[i]);
		}
		va_free(lines);
	}
	else
	{
		errorOccured = 1;
	}
	
	
	return errorOccured;
}

void PrintQueryError()
{
	char ** lines;
	int lineNumber;
	
	int i;
	
	
	
	if ((lineNumber = va_load_text_file_lines(ABIOMASTER_CLIENT_FOLDER, outputFile, &lines, NULL)) > 0)
	{
		for (i = 0; i < lineNumber; i++)
		{
			if (lines[i][0] == '\0')
			{
				continue;
			}
			
			SendJobLog(NULL, lines[i], "");
		}
		
		for (i = 0; i < lineNumber; i++)
		{
			va_free(lines[i]);
		}
		va_free(lines);
	}
}


void SendJobLog(char * logMsgID, ...)
{
	va_list argptr;
	char * argument;
	
	char description[USER_LOG_MESSAGE_LENGTH];
	char logmsg[USER_LOG_MESSAGE_LENGTH];
	
	int i;
	
	
	
	memset(description, 0, sizeof(description));
	memset(logmsg, 0, sizeof(logmsg));
	
	if (logMsgID != NULL && logMsgID[0] != '\0')
	{
		for (i = 0; i < backupLogDescriptionNumber; i++)
		{
			if (!strcmp(backupLogDescription[i].code, logMsgID))
			{
				strcpy(description, backupLogDescription[i].description);
				
				break;
			}
		}
		
		va_start(argptr, logMsgID);
		va_format_abio_log(logmsg, sizeof(logmsg), description, argptr);
		va_end(argptr);
	}
	else
	{
		va_start(argptr, logMsgID);
		while (1)
		{
			if ((argument = va_arg(argptr, char *)) != NULL)
			{
				if (argument[0] != '\0')
				{
					strcat(logmsg, argument);
				}
				else
				{
					break;
				}
			}
			else
			{
				break;
			}
		}
		va_end(argptr);
	}
	
	va_send_abio_log(masterIP, masterLogPort, commandData.jobID, NULL, LOG_LEVEL_JOB, MODULE_DB_ORACLE12C_SCRIPT_ORACLE, logmsg);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
void SendJobLog2(int logLevel,char * logCode, char * logMsgID, ...)
{
	va_list argptr;
	char * argument;
	
	char description[USER_LOG_MESSAGE_LENGTH];
	char logmsg[USER_LOG_MESSAGE_LENGTH];
	
	int i;
	
	
	
	memset(description, 0, sizeof(description));
	memset(logmsg, 0, sizeof(logmsg));
	
	if (logMsgID != NULL && logMsgID[0] != '\0')
	{
		for (i = 0; i < backupLogDescriptionNumber; i++)
		{
			if (!strcmp(backupLogDescription[i].code, logMsgID))
			{
				strcpy(description, backupLogDescription[i].description);
				
				break;
			}
		}
		
		va_start(argptr, logMsgID);
		va_format_abio_log(logmsg, sizeof(logmsg), description, argptr);
		va_end(argptr);
	}
	else
	{
		va_start(argptr, logMsgID);
		while (1)
		{
			if ((argument = va_arg(argptr, char *)) != NULL)
			{
				if (argument[0] != '\0')
				{
					strcat(logmsg, argument);
				}
				else
				{
					break;
				}
			}
			else
			{
				break;
			}
		}
		va_end(argptr);
	}
	va_send_abio_log(masterIP, masterLogPort, commandData.jobID, logCode, logLevel, MODULE_DB_ORACLE_RMAN_ORACLE_RMAN, logmsg);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
