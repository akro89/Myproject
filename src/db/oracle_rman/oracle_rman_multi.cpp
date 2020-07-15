#include "require.h"
#include "abio.h"
#include "lib.h"
#include "vendor.h"
#include "oracle_rman_multi.h"


// start of variables for abio library comparability
char ABIOMASTER_CK_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_MASTER_SERVER_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_DATA_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_CATALOG_DB_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_VOLUME_DB_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_FILE_LIST_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_LOG_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_CLIENT_FOLDER_SCRIPT[MAX_PATH_LENGTH];
char ORACLE_RMAN_QUERY_FILE_GET_TABLESPACE_LIST[MAX_PATH_LENGTH * 2];
char ORACLE_RMAN_QUERY_FILE_GET_TABLESPACE_LIST12[MAX_PATH_LENGTH * 2];
char ORACLE_RMAN_QUERY_FILE_GET_CDB_LIST[MAX_PATH_LENGTH * 2];
char ORACLE_RMAN_QUERY_FILE_GET_ARCHIVELOG_LIST[MAX_PATH_LENGTH * 2];
char ORACLE_RMAN_QUERY_FILE_GET_COMMAND_ARCHIVELOG_LIST[MAX_PATH_LENGTH * 2];
char ORACLE_RMAN_QUERY_FILE_GET_THREAD_LIST[MAX_PATH_LENGTH * 2];

struct portRule * portRule;
int portRuleNumber;

int tapeDriveTimeOut;
int tapeDriveDelayTime;
// end of variables for abio library comparability


char ABIOMASTER_CLIENT_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_CLIENT_CATALOG_DB_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_CLIENT_FILE_LIST_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_CLIENT_LOG_FOLDER[MAX_PATH_LENGTH];
char ORACLE_RMAN_LOG_FOLDER[MAX_PATH_LENGTH];

char ORACLE_RMAN_QUERY_FILE_BACKUP[MAX_PATH_LENGTH * 2];
char ORACLE_RMAN_QUERY_FILE_SYSTEMBACKUP[MAX_PATH_LENGTH * 2];
char ORACLE_RMAN_QUERY_FILE_RESTORE[MAX_PATH_LENGTH * 2];
char ORACLE_RMAN_QUERY_FILE_EXPIRE[MAX_PATH_LENGTH * 2];

char ORACLE_RMAN_QUERY_VERSION[MAX_PATH_LENGTH];
char ORACLE_RMAN_QUERY_CDB[MAX_PATH_LENGTH];

char ORACLE_RMAN_COPY_TO_ASM_FOLDER[MAX_PATH_LENGTH *2];
char ORACLE_ACCOUNT_NAME[ORACLE_ACCOUNT_LENGTH];
char ORACLE_RMAN_ASM_SID[MAX_PATH_LENGTH * 2];
char ORACLE_RMAN_ASM_HOME[MAX_PATH_LENGTH * 2];
char ORACLE_RMAN_QUERY_FILE_ASM_CONTROL_BACKUP_SOURCE_FILE_PATH_CTL[MAX_PATH_LENGTH * 2];
char ORACLE_RMAN_QUERY_FILE_ASM_CONTROL_BACKUP_SOURCE_FILE_PATH_TRC[MAX_PATH_LENGTH * 2];
//char ORACLE_RMAN_QUERY_FILE_ASM_CONTROL_BACKUP_DESTINATION_FILE_PATH[MAX_PATH_LENGTH * 2];


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

int debug_level;
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
// variables for rman qeury connection
va_sock_t rmanSock;
char rmanPort[PORT_LENGTH];

char RmanScriptFileName[MAX_PATH_LENGTH];
char RmanExecuteFileName[MAX_PATH_LENGTH];
char RmanSystemBackupFileName[MAX_PATH_LENGTH];
char RmanDataBackupFileName[MAX_PATH_LENGTH];
char RmanDataRestoreFileName[MAX_PATH_LENGTH];
char RmanArchiveBackupFileName[MAX_PATH_LENGTH];
char RmanArchiveRestoreFileName[MAX_PATH_LENGTH];
char RmanExpireFileName[MAX_PATH_LENGTH];
char RmanASMControlBackupFileNameCTL[MAX_PATH_LENGTH];
char RmanASMControlBackupFileNameTRC[MAX_PATH_LENGTH];
char RmanRemoveASMControlBackupFile[MAX_PATH_LENGTH];


//////////////////////////////////////////////////////////////////////////////////////////////////
// backup/restore buffer variables
int readDiskBufferSize;			// backup read buffer의 크기
int writeDiskBufferSize;		// restore write buffer의 크기


//////////////////////////////////////////////////////////////////////////////////////////////////
// variables for oracle connection
char username[ORACLE_RMAN_USERNAME_LENGTH];
char account[ORACLE_RMAN_ACCOUNT_LENGTH];
char passwd[ORACLE_RMAN_PASSWORD_LENGTH];
char sid[ORACLE_RMAN_SID_LENGTH];
char home[ORACLE_RMAN_HOME_LENGTH];
int deleteArchiveLog;
int remainArchiveLogCount;		// archive log 남길 갯수
char remainArchiveLog[DSIZ];

char environmentValueSid[MAX_PATH_LENGTH];
char environmentValueHome[MAX_PATH_LENGTH];

char sqlcmdUtil[MAX_PATH_LENGTH];
char queryFile[MAX_PATH_LENGTH];
char outputFile[MAX_PATH_LENGTH];


//////////////////////////////////////////////////////////////////////////////////////////////////
// variables for sql query
va_sock_t sybaseSock;
char sybasePort[PORT_LENGTH];

//////////////////////////////////////////////////////////////////////////////////////////////////
// backup/restore log
struct _backupLogDescription * backupLogDescription;
int backupLogDescriptionNumber;

static void SendCommand(int request, int reply, int execute, char * requestCommand, char * subCommand, char * executeCommand, char * sourceIP, char * sourcePort, char * sourceNodename, char * destinationIP, char * destinationPort, char * destinationNodename);
static void Error();
static void Exit();
static void PrintDeteteResult(char * file, int result, char * thread);


int DivideSeperator(char * dest, char * output1, char * output2);

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
		
		
		
		Exit();
		
		return 0;
	}

int InitProcess(char * filename)
{
	#ifdef __ABIO_WIN
		WSADATA wsaData;
		HANDLE token;
	#endif
	
	struct oracleRmanDB oracle;
	
	int i;
	
	
	
	///////////////////////////////////////////////////////////////////////////////////////////////////
	// 1. 전역 변수들을 초기화한다.
	
	memset(ABIOMASTER_CLIENT_FOLDER, 0, sizeof(ABIOMASTER_CLIENT_FOLDER));
	memset(ABIOMASTER_CLIENT_CATALOG_DB_FOLDER, 0, sizeof(ABIOMASTER_CLIENT_CATALOG_DB_FOLDER));
	memset(ABIOMASTER_CLIENT_FILE_LIST_FOLDER, 0, sizeof(ABIOMASTER_CLIENT_FILE_LIST_FOLDER));
	memset(ABIOMASTER_CLIENT_LOG_FOLDER, 0, sizeof(ABIOMASTER_CLIENT_LOG_FOLDER));
	memset(ORACLE_RMAN_LOG_FOLDER, 0, sizeof(ORACLE_RMAN_LOG_FOLDER));

	memset(ORACLE_RMAN_QUERY_FILE_BACKUP, 0, sizeof(ORACLE_RMAN_QUERY_FILE_BACKUP));
	memset(ORACLE_RMAN_QUERY_FILE_SYSTEMBACKUP, 0, sizeof(ORACLE_RMAN_QUERY_FILE_SYSTEMBACKUP));
	memset(ORACLE_RMAN_QUERY_FILE_RESTORE, 0, sizeof(ORACLE_RMAN_QUERY_FILE_RESTORE));
	memset(ORACLE_RMAN_QUERY_FILE_EXPIRE, 0, sizeof(ORACLE_RMAN_QUERY_FILE_EXPIRE));
	memset(ABIOMASTER_CLIENT_FOLDER_SCRIPT, 0 ,sizeof(ABIOMASTER_CLIENT_FOLDER_SCRIPT));
	memset(ORACLE_RMAN_QUERY_FILE_GET_TABLESPACE_LIST, 0, sizeof(ORACLE_RMAN_QUERY_FILE_GET_TABLESPACE_LIST));
	memset(ORACLE_RMAN_QUERY_FILE_GET_TABLESPACE_LIST12, 0, sizeof(ORACLE_RMAN_QUERY_FILE_GET_TABLESPACE_LIST12));
	memset(ORACLE_RMAN_QUERY_FILE_GET_CDB_LIST, 0, sizeof(ORACLE_RMAN_QUERY_FILE_GET_CDB_LIST));
	memset(ORACLE_RMAN_QUERY_FILE_GET_ARCHIVELOG_LIST, 0, sizeof(ORACLE_RMAN_QUERY_FILE_GET_ARCHIVELOG_LIST));
	memset(ORACLE_RMAN_QUERY_FILE_GET_COMMAND_ARCHIVELOG_LIST, 0 , sizeof(ORACLE_RMAN_QUERY_FILE_GET_COMMAND_ARCHIVELOG_LIST));
	memset(ORACLE_RMAN_QUERY_FILE_GET_THREAD_LIST, 0 , sizeof(ORACLE_RMAN_QUERY_FILE_GET_THREAD_LIST));
	memset(ORACLE_RMAN_QUERY_VERSION, 0, sizeof(ORACLE_RMAN_QUERY_VERSION));
	memset(ORACLE_RMAN_QUERY_CDB, 0, sizeof(ORACLE_RMAN_QUERY_CDB));
	memset(ORACLE_RMAN_COPY_TO_ASM_FOLDER, 0, sizeof(ORACLE_RMAN_COPY_TO_ASM_FOLDER));
	memset(ORACLE_ACCOUNT_NAME, 0, sizeof(ORACLE_ACCOUNT_NAME));
	memset(ORACLE_RMAN_ASM_SID, 0, sizeof(ORACLE_RMAN_ASM_SID));
	memset(ORACLE_RMAN_ASM_HOME, 0, sizeof(ORACLE_RMAN_ASM_HOME));
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

	debug_level = 0;
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// variables for rman qeury connection
	rmanSock = -1;
	memset(rmanPort, 0, sizeof(rmanPort));


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
	remainArchiveLogCount = 10;
	memset(remainArchiveLog, 0, sizeof(remainArchiveLog));

	memset(environmentValueSid, 0, sizeof(environmentValueSid));
	memset(environmentValueHome, 0, sizeof(environmentValueHome));
	
	memset(sqlcmdUtil, 0, sizeof(sqlcmdUtil));
	memset(queryFile, 0, sizeof(queryFile));
	memset(outputFile, 0, sizeof(outputFile));
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// backup/restore log
	backupLogDescription = NULL;
	backupLogDescriptionNumber = 0;
	
	//////////////////////////////////////////////////////////////////////////////////////////////////

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
	sprintf(ABIOMASTER_CLIENT_FOLDER_SCRIPT,"%s%c%s%c",ABIOMASTER_CLIENT_FOLDER,FILE_PATH_DELIMITER,"scripts",FILE_PATH_DELIMITER);
	// set job id and module number for debug log of ABIO common library
	strcpy(logJobID, commandData.jobID);
	logModuleNumber = MODULE_DB_ORACLE_RMAN_ORACLE_RMAN;
	
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
	memcpy(&oracle, commandData.db.dbData, sizeof(struct oracleRmanDB));
	strcpy(username, oracle.username);
	strcpy(account, oracle.account);
	strcpy(passwd, oracle.passwd);
	strcpy(sid, oracle.sid);
	strcpy(home, oracle.home);
	deleteArchiveLog = oracle.deleteArchive;
	
	strcpy(remainArchiveLog,ORACLE_RMAN_REMAIN_ARCHIVE_LOG_DEFAULT);
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
			va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE_RMAN_ORACLE_RMAN, FUNCTION_InitProcess, ERROR_WIN_INIT_WIN_SOCK, &commandData);
			
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
				va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE_RMAN_ORACLE_RMAN, FUNCTION_InitProcess, ERROR_WIN_GET_BACKUP_PRIVILEGE, &commandData);
				
				return -1;
	  		}
			
	  		if (SetPrivilege(token, ABIO_NT_PRIVILEGE_SE_RESTORE_NAME, 1) == FALSE)
	  		{
				va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE_RMAN_ORACLE_RMAN, FUNCTION_InitProcess, ERROR_WIN_GET_BACKUP_PRIVILEGE, &commandData);
				
				return -1;
	  		}
			
	  		if (SetPrivilege(token, ABIO_NT_PRIVILEGE_SE_SECURITY_NAME, 1) == FALSE)
			{
				va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE_RMAN_ORACLE_RMAN, FUNCTION_InitProcess, ERROR_WIN_GET_BACKUP_PRIVILEGE, &commandData);
				
				return -1;
	  		}
		}
		else
		{
			va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE_RMAN_ORACLE_RMAN, FUNCTION_InitProcess, ERROR_WIN_GET_BACKUP_PRIVILEGE, &commandData);
			
			return -1;
		}
	#endif
	

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 5. sybase archive device로부터 접속을 받을 서버를 만든다.
	
	if ((rmanSock = va_make_server_socket_iptable(NULL, NULL, NULL, 0, rmanPort)) == -1)
	{
		va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_SYBASE_BACKUP, FUNCTION_InitProcess, ERROR_NETWORK_PORT_BIND, &commandData);
		
		return -1;
	}
	
	return 0;
}

int GetModuleConfiguration()
{
	struct confOption * moduleConfig;
	int moduleConfigNumber;
	
	int i;
	
	
	
	if (va_load_conf_file(ABIOMASTER_CLIENT_FOLDER, ABIOMASTER_ORACLE_RMAN_CONFIG, &moduleConfig, &moduleConfigNumber) > 0)
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
			else if (!strcmp(moduleConfig[i].optionName, ORACLE_RMAN_LOG_FOLDER_OPTION_NAME))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(ORACLE_RMAN_LOG_FOLDER) - 1)
				{
					strcpy(ORACLE_RMAN_LOG_FOLDER, moduleConfig[i].optionValue);
				}
			}
			else if (!strcmp(moduleConfig[i].optionName, ORACLE_RMAN_QUERY_FILE_GET_TABLESPACE_LIST_OPTION_NAME))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(ORACLE_RMAN_QUERY_FILE_GET_TABLESPACE_LIST) - 1)
				{
					strcpy(ORACLE_RMAN_QUERY_FILE_GET_TABLESPACE_LIST, moduleConfig[i].optionValue);
				}
			}
			//blanc
			else if (!strcmp(moduleConfig[i].optionName, ORACLE_RMAN_QUERY_FILE_GET_TABLESPACE_LIST12_OPTION_NAME))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(ORACLE_RMAN_QUERY_FILE_GET_TABLESPACE_LIST12) - 1)
				{
					strcpy(ORACLE_RMAN_QUERY_FILE_GET_TABLESPACE_LIST12, moduleConfig[i].optionValue);
				}
			}
			else if (!strcmp(moduleConfig[i].optionName, ORACLE_RMAN_QUERY_FILE_GET_ARCHIVELOG_LIST_OPTION_NAME))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(ORACLE_RMAN_QUERY_FILE_GET_ARCHIVELOG_LIST) - 1)
				{
					strcpy(ORACLE_RMAN_QUERY_FILE_GET_ARCHIVELOG_LIST, moduleConfig[i].optionValue);
				}
			}
			else if (!strcmp(moduleConfig[i].optionName, ORACLE_RMAN_QUERY_FILE_GET_COMMAND_ARCHIVELOG_LIST_OPTION_NAME))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(ORACLE_RMAN_QUERY_FILE_GET_COMMAND_ARCHIVELOG_LIST) - 1)
				{
					strcpy(ORACLE_RMAN_QUERY_FILE_GET_COMMAND_ARCHIVELOG_LIST, moduleConfig[i].optionValue);
				}
			}
			else if (!strcmp(moduleConfig[i].optionName, ORACLE_RMAN_QUERY_FILE_GET_THREAD_LIST_OPTION_NAME))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(ORACLE_RMAN_QUERY_FILE_GET_THREAD_LIST) - 1)
				{
					strcpy(ORACLE_RMAN_QUERY_FILE_GET_THREAD_LIST, moduleConfig[i].optionValue);
				}
			}

			else if (!strcmp(moduleConfig[i].optionName, ORACLE_RMAN_QUERY_FILE_BACKUP_OPTION_NAME))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(ORACLE_RMAN_QUERY_FILE_BACKUP) - 1)
				{
					strcpy(ORACLE_RMAN_QUERY_FILE_BACKUP, moduleConfig[i].optionValue);
				}
			}
			else if (!strcmp(moduleConfig[i].optionName, ORACLE_RMAN_QUERY_FILE_SYSTEMBACKUP_OPTION_NAME))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(ORACLE_RMAN_QUERY_FILE_SYSTEMBACKUP) - 1)
				{
					strcpy(ORACLE_RMAN_QUERY_FILE_SYSTEMBACKUP, moduleConfig[i].optionValue);
				}
			}
			else if (!strcmp(moduleConfig[i].optionName, ORACLE_RMAN_QUERY_FILE_RESTORE_OPTION_NAME))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(ORACLE_RMAN_QUERY_FILE_RESTORE) - 1)
				{
					strcpy(ORACLE_RMAN_QUERY_FILE_RESTORE, moduleConfig[i].optionValue);
				}
			}		
			else if (!strcmp(moduleConfig[i].optionName, ORACLE_RMAN_QUERY_FILE_EXPIRE_OPTION_NAME))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(ORACLE_RMAN_QUERY_FILE_EXPIRE) - 1)
				{
					strcpy(ORACLE_RMAN_QUERY_FILE_EXPIRE, moduleConfig[i].optionValue);
				}
			}
			//blanc
			else if (!strcmp(moduleConfig[i].optionName, ORACLE_RMAN_QUERY_FILE_GET_VERSION_OPTION_NAME))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(ORACLE_RMAN_QUERY_VERSION) - 1)
				{
					strcpy(ORACLE_RMAN_QUERY_VERSION, moduleConfig[i].optionValue);
				}
			}
			//blanc
			else if (!strcmp(moduleConfig[i].optionName, ORACLE_RMAN_QUERY_FILE_GET_CDB_LIST_OPTION_NAME))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(ORACLE_RMAN_QUERY_FILE_GET_CDB_LIST) - 1)
				{
					strcpy(ORACLE_RMAN_QUERY_FILE_GET_CDB_LIST, moduleConfig[i].optionValue);
				}
			}
			else if (!strcmp(moduleConfig[i].optionName, CLIENT_REMAIN_ARCHIVE_LOG_DAY))
			{
				strcpy(remainArchiveLog, moduleConfig[i].optionValue);

				//ramainArchiveLogCount = atoi(moduleConfig[i].optionValue);
			}
			else if (!strcmp(moduleConfig[i].optionName, CLIENT_REMAIN_ARCHIVE_LOG_COUNT))
			{				
				remainArchiveLogCount = atoi(moduleConfig[i].optionValue);
			}
			else if (!strcmp(moduleConfig[i].optionName, DEBUG_LEVEL))
			{				
				debug_level = atoi(moduleConfig[i].optionValue);
			}
			else if (!strcmp(moduleConfig[i].optionName, ORACLE_RMAN_COPY_TO_ASM_FOLDER_OPTION_NAME))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(ORACLE_RMAN_COPY_TO_ASM_FOLDER) - 1)
				{
					strcpy(ORACLE_RMAN_COPY_TO_ASM_FOLDER, moduleConfig[i].optionValue);
				}
			}
			else if(!strcmp(moduleConfig[i].optionName, ORACLE_RMAN_ACCOUNT_OPTION_NAME))
			{
				if((int)strlen(moduleConfig[i].optionValue) < sizeof(ORACLE_ACCOUNT_NAME) -1)
				{
					strcpy(ORACLE_ACCOUNT_NAME, moduleConfig[i].optionValue);
				}
			}
			else if(!strcmp(moduleConfig[i].optionName, ORACLE_RMAN_BACKUP_ASM_SID_OPTION_NAME))
			{
				if((int)strlen(moduleConfig[i].optionValue) < sizeof(ORACLE_RMAN_ASM_SID) -1)
				{
					strcpy(ORACLE_RMAN_ASM_SID, moduleConfig[i].optionValue);
				}
			}
			else if(!strcmp(moduleConfig[i].optionName, ORACLE_RMAN_BACKUP_ASM_HOME_OPTION_NAME))
			{
				if((int)strlen(moduleConfig[i].optionValue) < sizeof(ORACLE_RMAN_ASM_HOME) -1)
				{
					strcpy(ORACLE_RMAN_ASM_HOME, moduleConfig[i].optionValue);
				}
			}
			/*else if(!strcmp(moduleConfig[i].optionName, ORACLE_RMAN_QUERY_FILE_ASM_CONTROL_BACKUP_SOURCE_OPTION_NAME_CTL))
			{
				if((int)strlen(moduleConfig[i].optionValue) < sizeof(ORACLE_RMAN_QUERY_FILE_ASM_CONTROL_BACKUP_SOURCE_FILE_PATH_CTL) -1)
				{
					strcpy(ORACLE_RMAN_QUERY_FILE_ASM_CONTROL_BACKUP_SOURCE_FILE_PATH_CTL, moduleConfig[i].optionValue);
				}
			}
			else if(!strcmp(moduleConfig[i].optionName, ORACLE_RMAN_QUERY_FILE_ASM_CONTROL_BACKUP_SOURCE_OPTION_NAME_TRC))
			{
				if((int)strlen(moduleConfig[i].optionValue) < sizeof(ORACLE_RMAN_QUERY_FILE_ASM_CONTROL_BACKUP_SOURCE_FILE_PATH_TRC) -1)
				{
					strcpy(ORACLE_RMAN_QUERY_FILE_ASM_CONTROL_BACKUP_SOURCE_FILE_PATH_TRC, moduleConfig[i].optionValue);
				}
			}*/
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
		va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE_RMAN_ORACLE_RMAN, FUNCTION_GetModuleConfiguration, ERROR_CONFIG_OPEN_CLIENT_CONFIG_FILE, &commandData);
		
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


void CreateCommandFile(char* fileType,char* filename, char* tablespace, char* database, char* tag, char * thread)
{
	char commandmsg[DSIZ*2];
	char jobtype[DSIZ];
	char databaseName[DSIZ];

	memset(jobtype, 0, sizeof(jobtype));
	memset(commandmsg, 0 , sizeof(commandmsg));
	memset(databaseName, 0 , sizeof(databaseName));

	va_fd_t fd;	

	//blanc
	if(database == NULL)
	{
		sprintf(databaseName, "%s", "");
	}
	else
	{
		if(strlen(database) == 0)
			sprintf(databaseName, "%s", "");
		else
			sprintf(databaseName, "'%s':", database);
	}
	


	fd = va_open(ABIOMASTER_CLIENT_FOLDER_SCRIPT,filename, O_CREAT|O_RDWR,0,0);	
	if (commandData.jobMethod == ABIO_DB_BACKUP_METHOD_ORACLE_RMAN_FULL)
	{
		strcpy(jobtype, "INCREMENTAL Level=0");
	}
	else if (commandData.jobMethod == ABIO_DB_BACKUP_METHOD_ORACLE_RMAN_CUMULATIVE)
	{
		strcpy(jobtype, "INCREMENTAL Level=1 CUMULATIVE");
	}
	else if (commandData.jobMethod == ABIO_DB_BACKUP_METHOD_ORACLE_RMAN_INCREMENTAL)
	{
		strcpy(jobtype, "INCREMENTAL Level=1");
	}	

	if(strcmp(fileType,"tablespace.sql") == 0)
	{
		sprintf(commandmsg,"spool &1;\nSELECT\n		Total.name || '^' ||\n	nvl(total_space-free_space, 0) || '^' \n 	FROM\n	(\n	SELECT\n		tablespace_name,\n		SUM(bytes) free_space\n	FROM\n		dba_free_space\n	GROUP BY\n		tablespace_name\n	) Free,\n	(\n	SELECT\n		b.name,\n		SUM(bytes) total_space\n	FROM\n		v$datafile a,\n		v$tablespace b\n	WHERE\n		a.ts# = b.ts#\n	GROUP BY\n		b.name\n	) Total\nWHERE\n	Free.tablespace_name(+)=Total.name;\nspool off;\nexit;\n");
	}
	else if(strcmp(fileType,"archivelog.sql") == 0)
	{
		sprintf(commandmsg,"spool &1;\n	select '^' || name || '^' from v$archived_log where name is not NULL;\n spool off;\nexit;\n");		
	}
	else if(strcmp(fileType,"threadarchivelog.sql") == 0)
	{
		sprintf(commandmsg,"spool &1;\n	select '^' || sequence# || '^' from v$archived_log where name is not NULL and thread# = '%s' and status != 'D';\n spool off;\nexit;\n", tablespace);		
	}
	else if(strcmp(fileType,"getthread.sql") == 0)
	{
		sprintf(commandmsg,"spool &1;\n select '^' || thread# || '^' from v$thread; \n spool off;\nexit;\n");
	}
	else if(strcmp(fileType,"version.sql") == 0)
	{
		sprintf(commandmsg,"spool &1;\nSELECT\n		version\n 	FROM\n	v$instance;\nspool off;\nexit;\n");
	}
	//cdb
	else if(strcmp(fileType,"cdb.sql") == 0)
	{
		sprintf(commandmsg,"spool &1;\nSELECT\n		cdb\n 	FROM\n	v$database;\nspool off;\nexit;\n");
	}
	#ifdef __ABIO_WIN	

	else if(strcmp(fileType,"Systembackup.cmd") == 0)
	{
		sprintf(commandmsg,"RUN {\n CONFIGURE CONTROLFILE AUTOBACKUP OFF;\n ALLOCATE CHANNEL ch00 TYPE 'SBT_TAPE' PARMS='SBT_LIBRARY=%s%corasbt.dll';\n SEND CHANNEL 'ch00' \"JOB_TYPE=BACKUP\";\n SEND CHANNEL 'ch00' \"PORT_NUM=%s\";\n BACKUP\n      %s\n       TABLESPACE %s%s\n       TAG '%s';\n RELEASE CHANNEL ch00;\n }",ABIOMASTER_CLIENT_FOLDER,FILE_PATH_DELIMITER,rmanPort,jobtype,databaseName,tablespace,tag);
		//sprintf(commandmsg,"@setlocal ENABLEEXTENSIONS\n@set NLS_LANG=American\n@set NLS_DATE_FORMAT=YYYY-MM-DD:hh24:mi:ss\n@set ORACLE_HOME=%%1\n@set RMAN=%%ORACLE_HOME%%\\bin\\rman.exe\n@set ORACLE_SID=%%2\n@set ORACLE_ACCOUNT=%%3\n@set ORACLE_PASSWORD=%%4\n@set BACKUP_PORT_NUM=%%5\n@if %%6 EQU 1 @set BACKUP_METHOD=INCREMENTAL Level=0\n@if %%6 EQU 2 @set BACKUP_METHOD=INCREMENTAL Level=1 CUMULATIVE\n@if %%6 EQU 3 @set BACKUP_METHOD=INCREMENTAL Level=1\n@if NOT DEFINED BACKUP_METHOD @set BACKUP_METHOD=INCREMENTAL Level=0\n@set BACKUP_TABLESPACE_NAME=%%7\n@set BACKUPSET_ID=%%8\n@set LOG_PATH=%%9\n@(\necho RUN {\necho ALLOCATE CHANNEL ch00 TYPE 'SBT_TAPE' PARMS='SBT_LIBRARY=%s%corasbt.dll';\necho SEND CHANNEL 'ch00' \"JOB_TYPE=BACKUP\";\necho SEND CHANNEL 'ch00' \"PORT_NUM=%%BACKUP_PORT_NUM%%\";\necho ALLOCATE CHANNEL ch01 TYPE disk FORMAT '%%ORACLE_HOME%%\\%%ORACLE_SID%%_%%BACKUPSET_ID%%_control';\necho ALLOCATE CHANNEL ch02 TYPE disk FORMAT '%%ORACLE_HOME%%\\%%ORACLE_SID%%_%%BACKUPSET_ID%%_sp';\necho BACKUP\necho       %%BACKUP_METHOD%%\necho       TABLESPACE %%BACKUP_TABLESPACE_NAME%%\necho       TAG '%%BACKUPSET_ID%%';\necho RELEASE CHANNEL ch00;\necho RELEASE CHANNEL ch01;\necho RELEASE CHANNEL ch02;\necho }\n) | %%RMAN%% target %%ORACLE_ACCOUNT%%/%%ORACLE_PASSWORD%%@%%ORACLE_SID%% nocatalog log \"%%LOG_PATH%%\"\n",ABIOMASTER_CLIENT_FOLDER,FILE_PATH_DELIMITER);
	}
	else if(strcmp(fileType,"backup.cmd") == 0)
	{
		sprintf(commandmsg,"RUN {\n ALLOCATE CHANNEL ch00 TYPE 'SBT_TAPE' PARMS='SBT_LIBRARY=%s%corasbt.dll';\n SEND CHANNEL 'ch00' \"JOB_TYPE=BACKUP\";\n SEND CHANNEL 'ch00' \"PORT_NUM=%s\";\n BACKUP\n      %s\n       TABLESPACE %s%s\n       TAG '%s';\n RELEASE CHANNEL ch00;\n }",ABIOMASTER_CLIENT_FOLDER,FILE_PATH_DELIMITER,rmanPort,jobtype,databaseName,tablespace,tag);
		//sprintf(commandmsg,"@setlocal ENABLEEXTENSIONS\n@set NLS_LANG=American\n@set NLS_DATE_FORMAT=YYYY-MM-DD:hh24:mi:ss\n@set ORACLE_HOME=%%1\n@set RMAN=%%ORACLE_HOME%%\\bin\\rman.exe\n@set ORACLE_SID=%%2\n@set ORACLE_ACCOUNT=%%3\n@set ORACLE_PASSWORD=%%4\n@set BACKUP_PORT_NUM=%%5\n@if %%6 EQU 1 @set BACKUP_METHOD=INCREMENTAL Level=0\n@if %%6 EQU 2 @set BACKUP_METHOD=INCREMENTAL Level=1 CUMULATIVE\n@if %%6 EQU 3 @set BACKUP_METHOD=INCREMENTAL Level=1\n@if NOT DEFINED BACKUP_METHOD @set BACKUP_METHOD=INCREMENTAL Level=0\n@set BACKUP_TABLESPACE_NAME=%%7\n@set BACKUPSET_ID=%%8\n@set LOG_PATH=%%9\n@(\necho RUN {\necho ALLOCATE CHANNEL ch00 TYPE 'SBT_TAPE' PARMS='SBT_LIBRARY=%s%corasbt.dll';\necho SEND CHANNEL 'ch00' \"JOB_TYPE=BACKUP\";\necho SEND CHANNEL 'ch00' \"PORT_NUM=%%BACKUP_PORT_NUM%%\";\necho BACKUP\necho       %%BACKUP_METHOD%%\necho       TABLESPACE %%BACKUP_TABLESPACE_NAME%%\necho       TAG '%%BACKUPSET_ID%%';\necho RELEASE CHANNEL ch00;\necho }\n) | %%RMAN%% target %%ORACLE_ACCOUNT%%/%%ORACLE_PASSWORD%%@%%ORACLE_SID%% nocatalog log \"%%LOG_PATH%%\"\n",ABIOMASTER_CLIENT_FOLDER,FILE_PATH_DELIMITER);
	}
	else if(strcmp(fileType,"restore.cmd") == 0)
	{
		sprintf(commandmsg,"RUN {\n ALLOCATE CHANNEL ch00 TYPE 'SBT_TAPE' PARMS='SBT_LIBRARY=%s%corasbt.dll';\n SEND CHANNEL 'ch00' \"PORT_NUM=%s\";\n RESTORE TABLESPACE %s%s from tag='%s';\n RECOVER TABLESPACE %s%s;\n RELEASE CHANNEL ch00;\n }",ABIOMASTER_CLIENT_FOLDER,FILE_PATH_DELIMITER,rmanPort,databaseName,tablespace,tag,databaseName,tablespace);
		//sprintf(commandmsg,"@setlocal ENABLEEXTENSIONS\n@set NLS_LANG=American\n@set NLS_DATE_FORMAT=YYYY-MM-DD:hh24:mi:ss\n@set ORACLE_HOME=%%1\n@set RMAN=%%ORACLE_HOME%%\\bin\\rman.exe\n@set ORACLE_SID=%%2\n@set ORACLE_ACCOUNT=%%3\n@set ORACLE_PASSWORD=%%4\n@set BACKUP_PORT_NUM=%%5\n@set RESTORE_TABLESPACE_NAME=%%6\n@set RESTORE_TAG=%%7\n@set LOG_PATH=%%8\n@(\necho RUN {\necho ALLOCATE CHANNEL ch00 TYPE 'SBT_TAPE' PARMS='SBT_LIBRARY=%s%corasbt.dll';\necho SEND CHANNEL 'ch00' \"PORT_NUM=%%BACKUP_PORT_NUM%%\";\necho RESTORE TABLESPACE %%RESTORE_TABLESPACE_NAME%% from tag='%%RESTORE_TAG%%';\necho RECOVER TABLESPACE %%RESTORE_TABLESPACE_NAME%%;\necho RELEASE CHANNEL ch00;\necho }\n) | %%RMAN%% target %%ORACLE_ACCOUNT%%/%%ORACLE_PASSWORD%%@%%ORACLE_SID%% nocatalog log \"%%LOG_PATH%%\"\n",ABIOMASTER_CLIENT_FOLDER,FILE_PATH_DELIMITER);
	}	
	else if(strcmp(fileType,"archivebackup.cmd") == 0)
	{
		sprintf(commandmsg,"RUN {\n ALLOCATE CHANNEL ch00 TYPE 'SBT_TAPE' PARMS='SBT_LIBRARY=%s%corasbt.dll';\n SEND CHANNEL 'ch00' \"JOB_TYPE=BACKUP\";\n SEND CHANNEL 'ch00' \"PORT_NUM=%s\";\n BACKUP\n      ARCHIVELOG sequence %s%s thread %s\n        TAG '%s';\n RELEASE CHANNEL ch00;\n }",ABIOMASTER_CLIENT_FOLDER,FILE_PATH_DELIMITER,rmanPort,databaseName,tablespace, thread, tag);		
	}
	else if(strcmp(fileType,"deletearchivelog.cmd") == 0)
	{
		sprintf(commandmsg,"DELETE ARCHIVELOG sequence between %s and %s thread %s;", tablespace, tablespace, thread);
	}
	else if(strcmp(fileType,"archiverestore.cmd") == 0)
	{
		sprintf(commandmsg,"RUN {\n ALLOCATE CHANNEL ch00 TYPE 'SBT_TAPE' PARMS='SBT_LIBRARY=%s%corasbt.dll';\n SEND CHANNEL 'ch00' \"PORT_NUM=%s\";\n RESTORE ARCHIVELOG sequence %s%s thread %s;\n RELEASE CHANNEL ch00;\n }",ABIOMASTER_CLIENT_FOLDER,FILE_PATH_DELIMITER,rmanPort,databaseName,tablespace,thread);
	}
	else if(strcmp(fileType,"expire.cmd") == 0)
	{
		strcpy(commandmsg,"RUN {\n crosscheck archivelog all;\n }");
	}
	
	#else
	else if(strcmp(fileType,"Systembackup.sh") == 0)
	{
		
		sprintf(commandmsg,"RUN {\n CONFIGURE CONTROLFILE AUTOBACKUP OFF;\n ALLOCATE CHANNEL ch00 TYPE 'SBT_TAPE' PARMS='SBT_LIBRARY=%s%clibobk.so';\n SEND CHANNEL 'ch00' \"JOB_TYPE=BACKUP\";\n SEND CHANNEL 'ch00' \"PORT_NUM=%s\";\n BACKUP\n      %s\n       TABLESPACE %s%s\n       TAG '%s';\n RELEASE CHANNEL ch00;\n }",ABIOMASTER_CLIENT_FOLDER,FILE_PATH_DELIMITER,rmanPort,jobtype,databaseName,tablespace,tag);
		//sprintf(commandmsg,"RUN {\n CONFIGURE CONTROLFILE AUTOBACKUP ON;\n CONFIGURE CONTROLFILE AUTOBACKUP FORMAT FOR DEVICE TYPE DISK TO '/oracle/ora11/product/ora11sw/test%%F';\nALLOCATE CHANNEL ch00 TYPE 'SBT_TAPE' PARMS='SBT_LIBRARY=%s%clibobk.so';\n SEND CHANNEL 'ch00' \"JOB_TYPE=BACKUP\";\n SEND CHANNEL 'ch00' \"PORT_NUM=%s\";\n  BACKUP\n      %s\n       TABLESPACE %s \n       TAG '%s';\n RELEASE CHANNEL ch00;\n}",ABIOMASTER_CLIENT_FOLDER,FILE_PATH_DELIMITER,rmanPort,jobtype,tablespace,tag);
		//sprintf(commandmsg,"#!/bin/sh\nORACLE_HOME=$1\nRMAN=$ORACLE_HOME/bin/rman\nORACLE_SID=$2\nORACLE_ACCOUNT=$3\nORACLE_PASSWORD=$4\nBACKUP_PORT_NUM=$5\ncase $6 in\n	1)	BACKUP_METHOD='INCREMENTAL Level=0';;\n	2)	BACKUP_METHOD='INCREMENTAL Level=1 CUMULATIVE';;\n	3)	BACKUP_METHOD='INCREMENTAL Level=1';;\n	*)	BACKUP_METHOD='INCREMENTAL Level=0';;\nesac\nBACKUP_TABLESPACE_NAME=$7\nBACKUPSET_ID=$8\nLOG_PATH=$9\nUNDERBAR=_\nCONTROL=control\nSP=sp\necho \"RUN {\nALLOCATE CHANNEL ch00 TYPE 'SBT_TAPE' PARMS='SBT_LIBRARY=%s/libobk.so';\nSEND CHANNEL 'ch00' \\\"JOB_TYPE=BACKUP\\\";\nSEND CHANNEL 'ch00' \\\"PORT_NUM=$BACKUP_PORT_NUM\\\";\nALLOCATE CHANNEL ch01 TYPE disk FORMAT '$ORACLE_HOME/$ORACLE_SID$UNDERBAR$BACKUPSET_ID$UNDERBAR$CONTROL';\nALLOCATE CHANNEL ch02 TYPE disk FORMAT '$ORACLE_HOME/$ORACLE_SID$UNDERBAR$BACKUPSET_ID$UNDERBAR$SP';\nBACKUP\n	$BACKUP_METHOD	\n	TABLESPACE $BACKUP_TABLESPACE_NAME\n	TAG \\\"$BACKUPSET_ID\\\";\nRELEASE CHANNEL ch00;\nRELEASE CHANNEL ch01;\nRELEASE CHANNEL ch02;\n}\" | $RMAN target $ORACLE_ACCOUNT/$ORACLE_PASSWORD@$ORACLE_SID nocatalog log \"$LOG_PATH\"\n",ABIOMASTER_CLIENT_FOLDER);
	}
	//blanc
	else if(strcmp(fileType,"backup.sh") == 0)	
	{
		sprintf(commandmsg,"RUN {\n ALLOCATE CHANNEL ch00 TYPE 'SBT_TAPE' PARMS='SBT_LIBRARY=%s%clibobk.so';\n SEND CHANNEL 'ch00' \"JOB_TYPE=BACKUP\";\n SEND CHANNEL 'ch00' \"PORT_NUM=%s\";\n BACKUP\n      %s\n       TABLESPACE %s%s\n       TAG '%s';\n RELEASE CHANNEL ch00;\n }",ABIOMASTER_CLIENT_FOLDER,FILE_PATH_DELIMITER,rmanPort,jobtype,databaseName,tablespace,tag);
		//sprintf(commandmsg,"#!/bin/sh\nORACLE_HOME=$1\nRMAN=$ORACLE_HOME/bin/rman\nORACLE_SID=$2\nORACLE_ACCOUNT=$3\nORACLE_PASSWORD=$4\nBACKUP_PORT_NUM=$5\ncase $6 in\n	1)	BACKUP_METHOD='INCREMENTAL Level=0';;\n	2)	BACKUP_METHOD='INCREMENTAL Level=1 CUMULATIVE';;\n	3)	BACKUP_METHOD='INCREMENTAL Level=1';;\n	*)	BACKUP_METHOD='INCREMENTAL Level=0';;\nesac\nBACKUP_TABLESPACE_NAME=$7\nBACKUPSET_ID=$8\nLOG_PATH=$9\necho \"RUN {\nALLOCATE CHANNEL ch00 TYPE 'SBT_TAPE' PARMS='SBT_LIBRARY=%s/libobk.so';\nSEND CHANNEL 'ch00' \\\"JOB_TYPE=BACKUP\\\";\nSEND CHANNEL 'ch00' \\\"PORT_NUM=$BACKUP_PORT_NUM\\\";\nBACKUP\n	$BACKUP_METHOD	\n	TABLESPACE $BACKUP_TABLESPACE_NAME\n	TAG \\\"$BACKUPSET_ID\\\";\nRELEASE CHANNEL ch00;\n}\" | $RMAN target $ORACLE_ACCOUNT/$ORACLE_PASSWORD@$ORACLE_SID nocatalog log \"$LOG_PATH\"\n",ABIOMASTER_CLIENT_FOLDER);			
	}
	else if(strcmp(fileType,"restore.sh") == 0)
	{
		sprintf(commandmsg,"RUN {\n ALLOCATE CHANNEL ch00 TYPE 'SBT_TAPE' PARMS='SBT_LIBRARY=%s%clibobk.so';\n SEND CHANNEL 'ch00' \"PORT_NUM=%s\";\n RESTORE TABLESPACE %s%s from tag='%s';\n RECOVER TABLESPACE %s%s;\n RELEASE CHANNEL ch00;\n }",ABIOMASTER_CLIENT_FOLDER,FILE_PATH_DELIMITER,rmanPort,databaseName,tablespace,tag,databaseName,tablespace);
		//sprintf(commandmsg,"#!/bin/sh\nORACLE_HOME=$1\nRMAN=$ORACLE_HOME/bin/rman\nORACLE_SID=$2\nORACLE_ACCOUNT=$3\nORACLE_PASSWORD=$4\nRESTORE_PORT_NUM=$5\nRESTORE_TABLESPACE_NAME=$6\nRESTORE_TAG=$7\nLOG_PATH=$8\necho \"RUN {\nALLOCATE CHANNEL ch00 TYPE 'SBT_TAPE' PARMS='SBT_LIBRARY=%s/libobk.so';\nSEND CHANNEL 'ch00' \\\"PORT_NUM=$RESTORE_PORT_NUM\\\";\nRESTORE TABLESPACE $RESTORE_TABLESPACE_NAME from tag=\\\"$RESTORE_TAG\\\";\nRECOVER TABLESPACE $RESTORE_TABLESPACE_NAME;\nRELEASE CHANNEL ch00;\n}\" | $RMAN target $ORACLE_ACCOUNT/$ORACLE_PASSWORD@$ORACLE_SID nocatalog log \"$LOG_PATH\"\n",ABIOMASTER_CLIENT_FOLDER);			
	}	
	else if(strcmp(fileType,"archivebackup.sh") == 0)
	{
		sprintf(commandmsg,"RUN {\n ALLOCATE CHANNEL ch00 TYPE 'SBT_TAPE' PARMS='SBT_LIBRARY=%s%clibobk.so';\n SEND CHANNEL 'ch00' \"JOB_TYPE=BACKUP\";\n SEND CHANNEL 'ch00' \"PORT_NUM=%s\";\n BACKUP\n      ARCHIVELOG sequence %s%s thread %s\n       TAG '%s';\n RELEASE CHANNEL ch00;\n }",ABIOMASTER_CLIENT_FOLDER,FILE_PATH_DELIMITER,rmanPort,databaseName,tablespace, thread, tag);		
	}
	else if(strcmp(fileType,"deletearchivelog.sh") == 0)
	{
		sprintf(commandmsg,"DELETE ARCHIVELOG sequence between %s%s and %s%s thread %s;", databaseName,tablespace, databaseName,tablespace, thread);
	}
	else if(strcmp(fileType,"archiverestore.sh") == 0)
	{
		sprintf(commandmsg,"RUN {\n ALLOCATE CHANNEL ch00 TYPE 'SBT_TAPE' PARMS='SBT_LIBRARY=%s%clibobk.so';\n SEND CHANNEL 'ch00' \"PORT_NUM=%s\";\n RESTORE ARCHIVELOG sequence %s%s thread %s;\n RELEASE CHANNEL ch00;\n }",ABIOMASTER_CLIENT_FOLDER,FILE_PATH_DELIMITER,rmanPort,databaseName,tablespace,thread);
	}
	else if(strcmp(fileType,"expire.sh") == 0)
	{
		strcpy(commandmsg,"RUN {\n crosscheck archivelog all;\n }");
	}
	#endif
	va_write(fd,commandmsg,(int)strlen(commandmsg),DATA_TYPE_NOT_CHANGE);	
	va_close(fd);
}

void CreateCommandFileDataFile(char* filename, char* tablespace, char * database, char * backupset, char * dataFileFrom, char * dataFileTo)
{

	char commandmsg[DSIZ*2];
	char jobtype[DSIZ];
	char databaseName[DSIZ];

	va_fd_t fd;	

	memset(jobtype, 0, sizeof(jobtype));
	memset(commandmsg, 0 , sizeof(commandmsg));
	memset(databaseName, 0 , sizeof(databaseName));

	//blanc
	if(database == NULL)
	{
		sprintf(databaseName, "%s", "");
	}
	else
	{
		if(strlen(database) == 0)
			sprintf(databaseName, "%s", "");
		else
			sprintf(databaseName, "'%s':", database);
	}
	

	fd = va_open(ABIOMASTER_CLIENT_FOLDER_SCRIPT,filename, O_CREAT|O_RDWR,0,0);	

	
	#ifdef __ABIO_WIN	
	if(strcmp(filename,"moverestore.cmd") == 0)
	{
		sprintf(commandmsg,"RUN {\n ALLOCATE CHANNEL ch00 TYPE 'SBT_TAPE' PARMS='SBT_LIBRARY=%s%corasbt.dll';\n SEND CHANNEL 'ch00' \"PORT_NUM=%s\";\n set newname for datafile '%s' to '%s';\n RESTORE TABLESPACE %s%s from tag='%s';\n RELEASE CHANNEL ch00;\n }",ABIOMASTER_CLIENT_FOLDER,FILE_PATH_DELIMITER,rmanPort, dataFileFrom, dataFileTo, databaseName, tablespace,backupset);
	}
	else if(strcmp(filename,"movearchiverestore.cmd") == 0)
	{
		sprintf(commandmsg,"RUN {\n ALLOCATE CHANNEL ch00 TYPE 'SBT_TAPE' PARMS='SBT_LIBRARY=%s%corasbt.dll';\n SEND CHANNEL 'ch00' \"PORT_NUM=%s\";\n set archivelog destination to '%s';\n RESTORE ARCHIVELOG sequence %s thread %s;\nRELEASE CHANNEL ch00;\n }",ABIOMASTER_CLIENT_FOLDER,FILE_PATH_DELIMITER,rmanPort, dataFileTo, tablespace, dataFileFrom);
	}
	#else
	if(strcmp(filename,"moverestore.sh") == 0)
	{
		sprintf(commandmsg,"RUN {\n ALLOCATE CHANNEL ch00 TYPE 'SBT_TAPE' PARMS='SBT_LIBRARY=%s%clibobk.so';\n SEND CHANNEL 'ch00' \"PORT_NUM=%s\";\n set newname for datafile '%s' to '%s';\n RESTORE TABLESPACE %s%s from tag='%s';\n RELEASE CHANNEL ch00;\n }",ABIOMASTER_CLIENT_FOLDER,FILE_PATH_DELIMITER,rmanPort,dataFileFrom, dataFileTo, databaseName, tablespace,backupset);
	}
	else if(strcmp(filename,"movearchiverestore.sh") == 0)
	{
		sprintf(commandmsg,"RUN {\n ALLOCATE CHANNEL ch00 TYPE 'SBT_TAPE' PARMS='SBT_LIBRARY=%s%clibobk.so';\n SEND CHANNEL 'ch00' \"PORT_NUM=%s\";\n set archivelog destination to '%s';\n RESTORE ARCHIVELOG sequence %s thread %s;\nRELEASE CHANNEL ch00;\n }",ABIOMASTER_CLIENT_FOLDER,FILE_PATH_DELIMITER,rmanPort, dataFileTo, tablespace, dataFileFrom);
	}

	#endif
	va_write(fd,commandmsg,(int)strlen(commandmsg),DATA_TYPE_NOT_CHANGE);	
	va_close(fd);
}

void CreateASMControlBackupFileCTL(char* file)
{
	char commandmsg[DSIZ*2];
	char destination[MAX_PATH_LENGTH];
	char sourcefilepath[MAX_PATH_LENGTH];
	memset(commandmsg, 0, sizeof(commandmsg));
	memset(sourcefilepath, 0, sizeof(sourcefilepath));
	memset(destination, 0, sizeof(destination));
	sprintf(sourcefilepath, "%s%c%s_%s", ORACLE_RMAN_COPY_TO_ASM_FOLDER, FILE_PATH_DELIMITER, commandData.backupset, ORACLE_CONTROL_FILE_BINARY_BACKUP_NAME);
	sprintf(destination, "%s%c%s_%s", ORACLE_RMAN_LOG_FOLDER, FILE_PATH_DELIMITER, commandData.backupset, ORACLE_CONTROL_FILE_BINARY_BACKUP_NAME);
	
	va_remove(NULL,	sourcefilepath);
	va_remove(NULL, destination);
	va_fd_t fd;
	fd = va_open(ABIOMASTER_CLIENT_FOLDER_SCRIPT,file, O_CREAT|O_RDWR,0,0);	
	#ifdef __ABIO_WIN
	sprintf(commandmsg, "@set ORACLE_SID=%s\n@set ORACLE_HOME=%s\nasmcmd cp %s %s", ORACLE_RMAN_ASM_SID, ORACLE_RMAN_ASM_HOME, sourcefilepath, destination);
	#else
	sprintf(commandmsg,"#!/bin/sh\nsu - %s -c \"export ORACLE_SID=%s; export ORACLE_HOME=%s; asmcmd cp %s %s\"", ORACLE_ACCOUNT_NAME, ORACLE_RMAN_ASM_SID, ORACLE_RMAN_ASM_HOME,sourcefilepath, destination);
	
#endif
	va_write(fd,commandmsg,(int)strlen(commandmsg),DATA_TYPE_NOT_CHANGE);
	va_close(fd);
}


void RemoveASMControlBackupFile(char *file)
{
	char commandmsg[DSIZ*2];
	
	char ASMFilePathCTL[MAX_PATH_LENGTH];
	char LocalFilePathCTL[MAX_PATH_LENGTH];
	//char ASMFilePathTRC[MAX_PATH_LENGTH];
	//char LocalFilePathTRC[MAX_PATH_LENGTH];

	memset(commandmsg, 0, sizeof(commandmsg));
	memset(ASMFilePathCTL, 0, sizeof(ASMFilePathCTL));
	memset(LocalFilePathCTL, 0, sizeof(LocalFilePathCTL));
	
    
	sprintf(ASMFilePathCTL, "%s%c%s_%s", ORACLE_RMAN_COPY_TO_ASM_FOLDER, FILE_PATH_DELIMITER, commandData.backupset, ORACLE_CONTROL_FILE_BINARY_BACKUP_NAME);
	sprintf(LocalFilePathCTL, "%s%c%s_%s", ORACLE_RMAN_LOG_FOLDER, FILE_PATH_DELIMITER, commandData.backupset, ORACLE_CONTROL_FILE_BINARY_BACKUP_NAME);
	
	
	va_fd_t fd;
	fd = va_open(ABIOMASTER_CLIENT_FOLDER_SCRIPT,file, O_CREAT|O_RDWR,0,0);	
	#ifdef __ABIO_WIN
	#else
	sprintf(commandmsg, "#!/bin/sh\nrm -rf %s\nsu - %s -c \"export ORACLE_SID=%s; export ORACLE_HOME=%s; asmcmd rm -rf %s\"", LocalFilePathCTL, ORACLE_ACCOUNT_NAME, ORACLE_RMAN_ASM_SID, ORACLE_RMAN_ASM_HOME, ASMFilePathCTL);
	#endif
	va_write(fd, commandmsg, (int)strlen(commandmsg), DATA_TYPE_NOT_CHANGE);
	va_close(fd);
}
void CreateASMControlBackupFileTRC(char* file)
{
	char commandmsg[DSIZ*2];
	char destination[MAX_PATH_LENGTH];
	char sourcefilepath[MAX_PATH_LENGTH];
	memset(commandmsg, 0, sizeof(commandmsg));
	memset(sourcefilepath, 0, sizeof(sourcefilepath));
	memset(destination, 0, sizeof(destination));
	sprintf(sourcefilepath, "%s%c%s_%s", ORACLE_RMAN_COPY_TO_ASM_FOLDER, FILE_PATH_DELIMITER, commandData.backupset, ORACLE_CONTROL_FILE_TRACE_BACKUP_NAME);
	sprintf(destination, "%s%c%s_%s", ORACLE_RMAN_LOG_FOLDER, FILE_PATH_DELIMITER, commandData.backupset, ORACLE_CONTROL_FILE_TRACE_BACKUP_NAME);
	va_remove(NULL,	sourcefilepath);
	va_remove(NULL, destination);

	va_fd_t fd;
	fd = va_open(ABIOMASTER_CLIENT_FOLDER_SCRIPT,file, O_CREAT|O_RDWR,0,0);	
	#ifdef __ABIO_WIN
	sprintf(commandmsg, "@set ORACLE_SID=%s\n@set ORACLE_HOME=%s\nasmcmd cp %s %s", ORACLE_RMAN_ASM_SID, ORACLE_RMAN_ASM_HOME, sourcefilepath, destination);
	#else
	sprintf(commandmsg,"#!/bin/sh\nsu - %s -c \"export ORACLE_SID=%s; export ORACLE_HOME=%s; asmcmd cp %s %s\"", ORACLE_ACCOUNT_NAME, ORACLE_RMAN_ASM_SID, ORACLE_RMAN_ASM_HOME,sourcefilepath, destination);	
#endif
	va_write(fd,commandmsg,(int)strlen(commandmsg),DATA_TYPE_NOT_CHANGE);
	va_close(fd);
}

void CreateExecuteFile(char* file)
{
	char commandmsg[DSIZ*2];
	memset(commandmsg, 0 , sizeof(commandmsg));
	va_fd_t fd;	
	fd = va_open(ABIOMASTER_CLIENT_FOLDER_SCRIPT,file, O_CREAT|O_RDWR,0,0);	
	#ifdef __ABIO_WIN 
	sprintf(commandmsg,"@setlocal ENABLEEXTENSIONS\n@set NLS_LANG=American\n@set NLS_DATE_FORMAT=YYYY-MM-DD:hh24:mi:ss\n@set ORACLE_HOME=%%1\n@set RMAN=%%ORACLE_HOME%%\\bin\\rman.exe\n@set ORACLE_SID=%%2\n@set ORACLE_ACCOUNT=%%3\n@set ORACLE_PASSWORD=%%4\n@set ORACLE_TYPE=%%5\n@set LOG_PATH=%%6\n%%RMAN%% target %%ORACLE_ACCOUNT%%/%%ORACLE_PASSWORD%%@%%ORACLE_SID%% nocatalog cmdfile %%ORACLE_TYPE%% log %%LOG_PATH%%\n");
	#else
	sprintf(commandmsg,"#!/bin/sh\nNLS_LANG=ENGLISH\nexport NLS_LANG\nNLS_DATE_FORMAT=YYYY-MM-DD:hh24:mi:ss\nexport NLS_DATE_FORMAT\nORACLE_HOME=$1\nRMAN=$ORACLE_HOME/bin/rman\nORACLE_SID=$2\nORACLE_ACCOUNT=$3\nORACLE_PASSWORD=$4\nORACLE_TYPE=$5\nLOG_PATH=$6\n$RMAN target $ORACLE_ACCOUNT/$ORACLE_PASSWORD@$ORACLE_SID nocatalog cmdfile \"$ORACLE_TYPE\" log \"$LOG_PATH\"");
	#endif
	va_write(fd,commandmsg,(int)strlen(commandmsg),DATA_TYPE_NOT_CHANGE);	
	va_close(fd);
}
int ConnectOracleRman(rman_fd_t * fdOracleRman, char * backupTargetName, char * backupTargetDatabase, char * backupset, char * restoreFileListPath)
{
	char jobMethod[NUMBER_STRING_LENGTH];
	char argOutputFile[MAX_PATH_LENGTH];
	char db_home[ORACLE_RMAN_HOME_LENGTH];
	char command_path[MAX_PATH_LENGTH];
	char rmanconnect[DSIZ];
	char log_command[MAX_PATH_LENGTH];
	char excutecommand[MAX_PATH_LENGTH];
	
	int pid;
	va_sock_t sock;
	
	#ifdef __ABIO_WIN
		BOOL opt = TRUE;
	#else
		int opt = 1;
	#endif

	// initialize variables
	memset(jobMethod, 0, sizeof(jobMethod));
	memset(argOutputFile, 0, sizeof(argOutputFile));
	memset(command_path, 0, sizeof(command_path));
	memset(db_home, 0, sizeof(db_home));
	memset(rmanconnect, 0, sizeof(rmanconnect));
	memset(log_command, 0, sizeof(log_command));
	memset(excutecommand, 0, sizeof(excutecommand));

	memset(RmanExecuteFileName, 0, sizeof(RmanExecuteFileName));
	memset(RmanScriptFileName, 0, sizeof(RmanScriptFileName));
	memset(RmanDataBackupFileName, 0, sizeof(RmanDataBackupFileName));
	memset(RmanDataRestoreFileName, 0, sizeof(RmanDataRestoreFileName));
	memset(RmanArchiveBackupFileName, 0, sizeof(RmanArchiveBackupFileName));
	memset(RmanArchiveRestoreFileName, 0, sizeof(RmanArchiveRestoreFileName));

	#ifdef __ABIO_WIN		
	sprintf(RmanExecuteFileName,"execute_%s.cmd",commandData.backupset);
	va_remove(ABIOMASTER_CLIENT_FOLDER_SCRIPT,RmanExecuteFileName);

	sprintf(db_home,"%s%cbin%crman.exe",home,FILE_PATH_DELIMITER,FILE_PATH_DELIMITER);	
	if((int)va_open(ABIOMASTER_CLIENT_FOLDER_SCRIPT,RmanExecuteFileName,O_RDONLY,0,0) == -1)
	{
		CreateExecuteFile(RmanExecuteFileName);
	}

	#else
	sprintf(RmanExecuteFileName,"execute_%s.sh",commandData.backupset);
	va_remove(ABIOMASTER_CLIENT_FOLDER_SCRIPT,RmanExecuteFileName);

	sprintf(db_home,"%s%cbin%crman",home,FILE_PATH_DELIMITER,FILE_PATH_DELIMITER);
	if((int)va_open(ABIOMASTER_CLIENT_FOLDER_SCRIPT,RmanExecuteFileName,O_RDONLY,0,0) == -1)
	{
		CreateExecuteFile(RmanExecuteFileName);
	}
	
	#endif
	sprintf(argOutputFile, "%s%c%s", ABIOMASTER_CLIENT_FOLDER, FILE_PATH_DELIMITER, outputFile);
	sprintf(rmanconnect,"%s target %s/%s@%s nocatalog cmdfile ",db_home,account,passwd,sid);
	sprintf(log_command," log %s",argOutputFile);
	if (fdOracleRman->connectType == ORACLE_RMAN_CONNECT_BACKUP_TABLESPACE)
	{
		#ifdef __ABIO_WIN	
		if(strcmp(backupTargetName,"SYSTEM")==0)
		{			
			
			sprintf(RmanSystemBackupFileName,"Systembackup_%s.cmd",commandData.backupset);
			va_remove(ABIOMASTER_CLIENT_FOLDER_SCRIPT,RmanSystemBackupFileName);
			
			CreateCommandFile("Systembackup.cmd",RmanSystemBackupFileName,backupTargetName,backupTargetDatabase,commandData.backupset,NULL);	
			
			
			sprintf(command_path,"%s%s",ABIOMASTER_CLIENT_FOLDER_SCRIPT,RmanSystemBackupFileName);
			sprintf(excutecommand,"%s%s",ABIOMASTER_CLIENT_FOLDER_SCRIPT,RmanExecuteFileName);
			pid = va_spawn_process(NOWAIT_PROCESS, ORACLE_RMAN_QUERY_FILE_SHELL,"/C",excutecommand, home, sid, account, passwd, command_path, argOutputFile, NULL);
		
		}
		else
		{	
			sprintf(RmanDataBackupFileName,"backup_%s.cmd",commandData.backupset);
			va_remove(ABIOMASTER_CLIENT_FOLDER_SCRIPT,RmanDataBackupFileName);
			//va_remove(ABIOMASTER_CLIENT_FOLDER_SCRIPT,"backup.cmd");
			CreateCommandFile("backup.cmd",RmanDataBackupFileName,backupTargetName,backupTargetDatabase,commandData.backupset,NULL);				
			sprintf(command_path,"%s%s",ABIOMASTER_CLIENT_FOLDER_SCRIPT,RmanDataBackupFileName);
			sprintf(excutecommand,"%s%s",ABIOMASTER_CLIENT_FOLDER_SCRIPT,RmanExecuteFileName);
			pid = va_spawn_process(NOWAIT_PROCESS, ORACLE_RMAN_QUERY_FILE_SHELL,"/C",excutecommand, home, sid, account, passwd, command_path, argOutputFile, NULL);
			
		}	
		#else
		if(strcmp(backupTargetName,"SYSTEM")==0)
		{	
			sprintf(RmanSystemBackupFileName,"Systembackup_%s.sh",commandData.backupset);			
			va_remove(ABIOMASTER_CLIENT_FOLDER_SCRIPT,RmanSystemBackupFileName);
			CreateCommandFile("Systembackup.sh",RmanSystemBackupFileName,backupTargetName,backupTargetDatabase,commandData.backupset,NULL);	
			sprintf(command_path,"%s%s",ABIOMASTER_CLIENT_FOLDER_SCRIPT,RmanSystemBackupFileName);			
			sprintf(excutecommand,"%s%s",ABIOMASTER_CLIENT_FOLDER_SCRIPT,RmanExecuteFileName);
			pid = va_spawn_process(NOWAIT_PROCESS, username, ORACLE_RMAN_QUERY_FILE_SHELL,excutecommand, home, sid, account, passwd, command_path, argOutputFile, NULL);				
		}
		else
		{			
			sprintf(RmanDataBackupFileName,"backup_%s.sh",commandData.backupset);			
			va_remove(ABIOMASTER_CLIENT_FOLDER_SCRIPT,RmanDataBackupFileName);
			
			
			CreateCommandFile("backup.sh",RmanDataBackupFileName,backupTargetName,backupTargetDatabase,commandData.backupset,NULL);	
			sprintf(command_path,"%s%s",ABIOMASTER_CLIENT_FOLDER_SCRIPT,RmanDataBackupFileName);			
			sprintf(excutecommand,"%s%s",ABIOMASTER_CLIENT_FOLDER_SCRIPT,RmanExecuteFileName);
			pid = va_spawn_process(NOWAIT_PROCESS, username, ORACLE_RMAN_QUERY_FILE_SHELL,excutecommand, home, sid, account, passwd, command_path, argOutputFile, NULL);				
		}
		#endif
	}	
	else if (fdOracleRman->connectType == ORACLE_RMAN_CONNECT_RESTORE_TABLESPACE)
	{
		#ifdef __ABIO_WIN			
			sprintf(RmanDataRestoreFileName,"restore.cmd");			
			va_remove(ABIOMASTER_CLIENT_FOLDER_SCRIPT,RmanDataRestoreFileName);
			CreateCommandFile("restore.cmd",RmanDataRestoreFileName,backupTargetName,backupTargetDatabase,backupset,NULL);	
			sprintf(command_path,"%s%s",ABIOMASTER_CLIENT_FOLDER_SCRIPT,RmanDataRestoreFileName);			
			sprintf(excutecommand,"%s%s",ABIOMASTER_CLIENT_FOLDER_SCRIPT,RmanExecuteFileName);
			pid = va_spawn_process(NOWAIT_PROCESS, ORACLE_RMAN_QUERY_FILE_SHELL,"/C",excutecommand, home, sid, account, passwd, command_path, argOutputFile, NULL);
			
		#else	
			sprintf(RmanDataRestoreFileName,"restore.sh");
			va_remove(ABIOMASTER_CLIENT_FOLDER_SCRIPT,RmanDataRestoreFileName);
			CreateCommandFile("restore.sh",RmanDataRestoreFileName,backupTargetName,backupTargetDatabase,backupset,NULL);	
			sprintf(command_path,"%s%s",ABIOMASTER_CLIENT_FOLDER_SCRIPT,RmanDataRestoreFileName);			
			sprintf(excutecommand,"%s%s",ABIOMASTER_CLIENT_FOLDER_SCRIPT,RmanExecuteFileName);
			pid = va_spawn_process(NOWAIT_PROCESS, username, ORACLE_RMAN_QUERY_FILE_SHELL,excutecommand, home, sid, account, passwd, command_path, argOutputFile, NULL);	

		#endif
	}
	else if (fdOracleRman->connectType == ORACLE_RMAN_CONNECT_BACKUP_COMMAND_ARCHIVELOG)
	{
		#ifdef __ABIO_WIN	
			sprintf(RmanArchiveBackupFileName,"archivebackup_%s.cmd",commandData.backupset);
			va_remove(ABIOMASTER_CLIENT_FOLDER_SCRIPT,RmanArchiveBackupFileName);
			CreateCommandFile("archivebackup.cmd",RmanArchiveBackupFileName,backupTargetName,backupTargetDatabase,commandData.backupset,backupset);	
			sprintf(command_path,"%s%s",ABIOMASTER_CLIENT_FOLDER_SCRIPT,RmanArchiveBackupFileName);			
			sprintf(excutecommand,"%s%s",ABIOMASTER_CLIENT_FOLDER_SCRIPT,RmanExecuteFileName);
			pid = va_spawn_process(NOWAIT_PROCESS, ORACLE_RMAN_QUERY_FILE_SHELL,"/C",excutecommand, home, sid, account, passwd, command_path, argOutputFile, NULL);

		#else
			sprintf(RmanArchiveBackupFileName,"archivebackup_%s.sh",commandData.backupset);
			va_remove(ABIOMASTER_CLIENT_FOLDER_SCRIPT,RmanArchiveBackupFileName);
			CreateCommandFile("archivebackup.sh",RmanArchiveBackupFileName,backupTargetName,backupTargetDatabase,commandData.backupset,backupset);	
			sprintf(command_path,"%s%s",ABIOMASTER_CLIENT_FOLDER_SCRIPT,RmanArchiveBackupFileName);			
			sprintf(excutecommand,"%s%s",ABIOMASTER_CLIENT_FOLDER_SCRIPT,RmanExecuteFileName);
			pid = va_spawn_process(NOWAIT_PROCESS, username, ORACLE_RMAN_QUERY_FILE_SHELL,excutecommand, home, sid, account, passwd, command_path, argOutputFile, NULL);	
		#endif
	}
	else if (fdOracleRman->connectType == ORACLE_RMAN_CONNECT_RESTORE_COMMAND_ARCHIVELOG)
	{

		#ifdef __ABIO_WIN		
			sprintf(RmanArchiveRestoreFileName,"archiverestore.cmd");
			va_remove(ABIOMASTER_CLIENT_FOLDER_SCRIPT,RmanArchiveRestoreFileName);
			CreateCommandFile("archiverestore.cmd",RmanArchiveRestoreFileName,backupTargetName,backupTargetDatabase,commandData.backupset,restoreFileListPath);	
			sprintf(command_path,"%s%s",ABIOMASTER_CLIENT_FOLDER_SCRIPT,RmanArchiveRestoreFileName);			
			sprintf(excutecommand,"%s%s",ABIOMASTER_CLIENT_FOLDER_SCRIPT,RmanExecuteFileName);
			pid = va_spawn_process(NOWAIT_PROCESS, ORACLE_RMAN_QUERY_FILE_SHELL,"/C",excutecommand, home, sid, account, passwd, command_path, argOutputFile, NULL);

		#else
			sprintf(RmanArchiveRestoreFileName,"archiverestore.sh");
			va_remove(ABIOMASTER_CLIENT_FOLDER_SCRIPT,RmanArchiveRestoreFileName);
			CreateCommandFile("archiverestore.sh",RmanArchiveRestoreFileName,backupTargetName,backupTargetDatabase,commandData.backupset,restoreFileListPath);	
			sprintf(command_path,"%s%s",ABIOMASTER_CLIENT_FOLDER_SCRIPT,RmanArchiveRestoreFileName);			
			sprintf(excutecommand,"%s%s",ABIOMASTER_CLIENT_FOLDER_SCRIPT,RmanExecuteFileName);
			pid = va_spawn_process(NOWAIT_PROCESS, username, ORACLE_RMAN_QUERY_FILE_SHELL,excutecommand, home, sid, account, passwd, command_path, argOutputFile, NULL);	
		#endif

	}
	if (pid < 0)
	{
		if (fdOracleRman->connectType == ORACLE_RMAN_CONNECT_BACKUP_TABLESPACE)
		{				
			SendJobLog(LOG_MSG_ID_ORACLE_RMAN_QUERY_ERROR_EXECUTE_SHELL, ORACLE_RMAN_CONNECT_BACKUP_TABLESPACE, "");
		}
		else if (fdOracleRman->connectType == ORACLE_RMAN_CONNECT_BACKUP_SPFILE)
		{
			SendJobLog(LOG_MSG_ID_ORACLE_RMAN_QUERY_ERROR_EXECUTE_SHELL, ORACLE_RMAN_CONNECT_BACKUP_SPFILE, "");
		}
		else if (fdOracleRman->connectType == ORACLE_RMAN_CONNECT_RESTORE_TABLESPACE)
		{
			SendJobLog(LOG_MSG_ID_ORACLE_RMAN_QUERY_ERROR_EXECUTE_SHELL, ORACLE_RMAN_CONNECT_RESTORE_TABLESPACE, "");
		}
		else if (fdOracleRman->connectType == ORACLE_RMAN_CONNECT_RESTORE_SPFILE)
		{
			SendJobLog(LOG_MSG_ID_ORACLE_RMAN_QUERY_ERROR_EXECUTE_SHELL, ORACLE_RMAN_CONNECT_RESTORE_SPFILE, "");
		}
		
		return 2;
	}
	else
	{
		fdOracleRman->pid = pid;
	}
	
	setsockopt(rmanSock , IPPROTO_TCP, TCP_NODELAY, (const char*)&opt, sizeof(opt));

	// oracle rman backup server에서 접속을 기다린다.
	if ((sock = va_wait_connection(rmanSock, 60, NULL)) != -1)
	{
		fdOracleRman->sock = sock;
	}
	else			
	{
		CheckQueryResult(fdOracleRman);
		return 2;
	}
	
	return 0;
}

int ConnectOracleRmanRestore(rman_fd_t * fdOracleRman, char * backupTargetName, char * backupset, char * restoreFileListPath, char * dataFileFrom, char * dataFileTo)
{

	char jobMethod[NUMBER_STRING_LENGTH];
	char argOutputFile[MAX_PATH_LENGTH];
	char db_home[ORACLE_RMAN_HOME_LENGTH];
	char command_path[MAX_PATH_LENGTH];
	char excutecommand[MAX_PATH_LENGTH];

	//blanc
	char tablespaceName[256];
	char containerName[256];

	int pid;
	int fromLeng;
	int toLeng;
	va_sock_t sock;
	
	// initialize variables
	memset(jobMethod, 0, sizeof(jobMethod));
	memset(argOutputFile, 0, sizeof(argOutputFile));
	memset(command_path, 0, sizeof(command_path));
	memset(db_home, 0, sizeof(db_home));
	memset(excutecommand, 0, sizeof(excutecommand));

	//blanc
	memset(tablespaceName, 0, sizeof(tablespaceName));
	memset(containerName, 0, sizeof(containerName));

	
	fromLeng = 0;
	toLeng = 0;
	#ifdef __ABIO_WIN		
	sprintf(db_home,"%s%cbin%crman.exe",home,FILE_PATH_DELIMITER,FILE_PATH_DELIMITER);	
	if((int)va_open(ABIOMASTER_CLIENT_FOLDER_SCRIPT,"execute.cmd",O_RDONLY,0,0) == -1)
	{
		CreateExecuteFile("execute.cmd");
	}

	#else
	sprintf(db_home,"%s%cbin%crman",home,FILE_PATH_DELIMITER,FILE_PATH_DELIMITER);
	if((int)va_open(ABIOMASTER_CLIENT_FOLDER_SCRIPT,"execute.sh",O_RDONLY,0,0) == -1)
	{
		CreateExecuteFile("execute.sh");
	}
	
	#endif
	sprintf(argOutputFile, "%s%c%s", ABIOMASTER_CLIENT_FOLDER, FILE_PATH_DELIMITER, outputFile);

	if(fdOracleRman->connectType == ORACLE_RMAN_CONNECT_RESTORE_TABLESPACE)
	{
		fromLeng = (int)strlen(dataFileFrom);
		toLeng = (int)strlen(dataFileTo);

		//blanc
		DivideSeperator(backupTargetName, tablespaceName, containerName);

		if (fromLeng == 0 && toLeng == 0)
		{
			#ifdef __ABIO_WIN		
				va_remove(ABIOMASTER_CLIENT_FOLDER_SCRIPT,"restore.cmd");
				CreateCommandFile("restore.cmd","restore.cmd",tablespaceName,containerName,backupset,NULL);	
				sprintf(command_path,"%srestore.cmd",ABIOMASTER_CLIENT_FOLDER_SCRIPT);			
				sprintf(excutecommand,"%sexecute.cmd",ABIOMASTER_CLIENT_FOLDER_SCRIPT);

				if(0 < debug_level)
				{
					WriteDebugData(ABIOMASTER_CLIENT_FOLDER_SCRIPT,"para.log","cmd /C %s %s %s %s %s %s %s",excutecommand,home,sid,account,passwd,command_path,argOutputFile);			
				}

				pid = va_spawn_process(NOWAIT_PROCESS, ORACLE_RMAN_QUERY_FILE_SHELL,"/C",excutecommand, home, sid, account, passwd, command_path, argOutputFile, NULL);
				
			#else	
				va_remove(ABIOMASTER_CLIENT_FOLDER_SCRIPT,"restore.sh");
				CreateCommandFile("restore.sh","restore.sh",tablespaceName,containerName,backupset,NULL);	
				sprintf(command_path,"%srestore.sh",ABIOMASTER_CLIENT_FOLDER_SCRIPT);			
				sprintf(excutecommand,"%sexecute.sh",ABIOMASTER_CLIENT_FOLDER_SCRIPT);
				pid = va_spawn_process(NOWAIT_PROCESS, username, ORACLE_RMAN_QUERY_FILE_SHELL,excutecommand, home, sid, account, passwd, command_path, argOutputFile, NULL);	

			#endif

		}
		else
		{
			#ifdef __ABIO_WIN		
				va_remove(ABIOMASTER_CLIENT_FOLDER_SCRIPT,"moverestore.cmd");
				CreateCommandFileDataFile("moverestore.cmd",tablespaceName, containerName, backupset, dataFileFrom, dataFileTo);	
				sprintf(command_path,"%smoverestore.cmd",ABIOMASTER_CLIENT_FOLDER_SCRIPT);			
				sprintf(excutecommand,"%sexecute.cmd",ABIOMASTER_CLIENT_FOLDER_SCRIPT);

				if(0 < debug_level)
				{
					WriteDebugData(ABIOMASTER_CLIENT_FOLDER_SCRIPT,"para.log","cmd /C %s %s %s %s %s %s %s",excutecommand,home,sid,account,passwd,command_path,argOutputFile);			
				}

				pid = va_spawn_process(NOWAIT_PROCESS, ORACLE_RMAN_QUERY_FILE_SHELL,"/C",excutecommand, home, sid, account, passwd, command_path, argOutputFile, NULL);
				
			#else	
				va_remove(ABIOMASTER_CLIENT_FOLDER_SCRIPT,"moverestore.sh");
				CreateCommandFileDataFile("moverestore.sh", tablespaceName, containerName, backupset, dataFileFrom, dataFileTo);	
				sprintf(command_path,"%smoverestore.sh",ABIOMASTER_CLIENT_FOLDER_SCRIPT);			
				sprintf(excutecommand,"%sexecute.sh",ABIOMASTER_CLIENT_FOLDER_SCRIPT);
				pid = va_spawn_process(NOWAIT_PROCESS, username, ORACLE_RMAN_QUERY_FILE_SHELL,excutecommand, home, sid, account, passwd, command_path, argOutputFile, NULL);	

			#endif
		}
	}
	else if (fdOracleRman->connectType == ORACLE_RMAN_CONNECT_RESTORE_COMMAND_ARCHIVELOG)
	{		
		#ifdef __ABIO_WIN
		va_slash_to_backslash(dataFileTo);
		#endif

		#ifdef __ABIO_WIN		
			va_remove(ABIOMASTER_CLIENT_FOLDER_SCRIPT,"movearchiverestore.cmd");
			CreateCommandFileDataFile("movearchiverestore.cmd",backupTargetName, NULL, backupset, restoreFileListPath, dataFileTo);	
			sprintf(command_path,"%smovearchiverestore.cmd",ABIOMASTER_CLIENT_FOLDER_SCRIPT);			
			sprintf(excutecommand,"%sexecute.cmd",ABIOMASTER_CLIENT_FOLDER_SCRIPT);
			pid = va_spawn_process(NOWAIT_PROCESS, ORACLE_RMAN_QUERY_FILE_SHELL,"/C",excutecommand, home, sid, account, passwd, command_path, argOutputFile, NULL);
			
		#else	
			va_remove(ABIOMASTER_CLIENT_FOLDER_SCRIPT,"movearchiverestore.sh");
			CreateCommandFileDataFile("movearchiverestore.sh", backupTargetName, NULL, backupset, restoreFileListPath, dataFileTo);	
			sprintf(command_path,"%smovearchiverestore.sh",ABIOMASTER_CLIENT_FOLDER_SCRIPT);			
			sprintf(excutecommand,"%sexecute.sh",ABIOMASTER_CLIENT_FOLDER_SCRIPT);
			pid = va_spawn_process(NOWAIT_PROCESS, username, ORACLE_RMAN_QUERY_FILE_SHELL,excutecommand, home, sid, account, passwd, command_path, argOutputFile, NULL);	
		#endif
	}

	if (pid < 0)
	{
		SendJobLog(LOG_MSG_ID_ORACLE_RMAN_QUERY_ERROR_EXECUTE_SHELL, ORACLE_RMAN_CONNECT_RESTORE_TABLESPACE, "");
	}
	else
	{
		fdOracleRman->pid = pid;
	}
	
	// oracle rman backup server에서 접속을 기다린다.
	if ((sock = va_wait_connection(rmanSock, 60, NULL)) != -1)
	{
		fdOracleRman->sock = sock;
	}
	else			
	{
		CheckQueryResult(fdOracleRman);
		return 2;
	}

	return 0;
}

int DeleteArchiveLog(char * sequence, char * thread)
{
	char tempSqlFile[MAX_PATH_LENGTH];
	char db_home[ORACLE_RMAN_HOME_LENGTH];
	char argOutputFile[MAX_PATH_LENGTH];
	char log_command[MAX_PATH_LENGTH];
	char excutecommand[MAX_PATH_LENGTH];
	char command_path[MAX_PATH_LENGTH];
	int r;

	r = 0;
	memset(db_home, 0, sizeof(db_home));
	memset(argOutputFile, 0, sizeof(argOutputFile));
	memset(log_command, 0, sizeof(log_command));
	memset(excutecommand, 0, sizeof(excutecommand));
	memset(command_path, 0, sizeof(command_path));
	memset(tempSqlFile, 0, sizeof(tempSqlFile));

	#ifdef __ABIO_WIN		
	sprintf(db_home,"%s%cbin%crman.exe",home,FILE_PATH_DELIMITER,FILE_PATH_DELIMITER);	
	sprintf(RmanExecuteFileName,"execute_%s.cmd",commandData.backupset);
	va_remove(ABIOMASTER_CLIENT_FOLDER_SCRIPT,RmanExecuteFileName);
	if((int)va_open(ABIOMASTER_CLIENT_FOLDER_SCRIPT,RmanExecuteFileName,O_RDONLY,0,0) == -1)
	{
		CreateExecuteFile(RmanExecuteFileName);
	}

	#else
	sprintf(RmanExecuteFileName,"execute_%s.sh",commandData.backupset);
	va_remove(ABIOMASTER_CLIENT_FOLDER_SCRIPT,RmanExecuteFileName);
	sprintf(db_home,"%s%cbin%crman",home,FILE_PATH_DELIMITER,FILE_PATH_DELIMITER);
	if((int)va_open(ABIOMASTER_CLIENT_FOLDER_SCRIPT,RmanExecuteFileName,O_RDONLY,0,0) == -1)
	{
		CreateExecuteFile(RmanExecuteFileName);
	}
	#endif
	
		// query를 실행한다.
	sprintf(argOutputFile, "%s%c%s", ABIOMASTER_CLIENT_FOLDER, FILE_PATH_DELIMITER, outputFile);

	sprintf(log_command," log %s",argOutputFile);
	#ifdef __ABIO_WIN
		sprintf(tempSqlFile,"deletearchivelog_%s.cmd",commandData.backupset);
		va_remove(ABIOMASTER_CLIENT_FOLDER_SCRIPT,tempSqlFile);			
		CreateCommandFile("deletearchivelog.cmd",tempSqlFile,sequence, NULL,NULL, thread);	
		sprintf(command_path,"%s%s",ABIOMASTER_CLIENT_FOLDER_SCRIPT,tempSqlFile);		
		sprintf(excutecommand,"%s%s",ABIOMASTER_CLIENT_FOLDER_SCRIPT,RmanExecuteFileName);
		r = va_spawn_process(WAIT_PROCESS, ORACLE_RMAN_QUERY_FILE_SHELL,"/C",excutecommand, home, sid, account, passwd, command_path, argOutputFile, NULL);

	#else
		sprintf(tempSqlFile,"deletearchivelog_%s.sh",commandData.backupset);
		va_remove(ABIOMASTER_CLIENT_FOLDER_SCRIPT,tempSqlFile);			
		CreateCommandFile("deletearchivelog.sh",tempSqlFile,sequence,NULL,NULL, thread);	
		sprintf(command_path,"%s%s",ABIOMASTER_CLIENT_FOLDER_SCRIPT,tempSqlFile);		
		sprintf(excutecommand,"%s%s",ABIOMASTER_CLIENT_FOLDER_SCRIPT,RmanExecuteFileName);
		r = va_spawn_process(WAIT_PROCESS, username, ORACLE_RMAN_QUERY_FILE_SHELL,excutecommand, home, sid, account, passwd, command_path, argOutputFile, NULL);				
	
	#endif

	if (r != 0)
	{
		if (r == -1)
		{
			SendJobLog(LOG_MSG_ID_ORACLE_RMAN_QUERY_ERROR_EXECUTE_SHELL, sqlcmdUtil, "");
		}
		else
		{
			PrintQueryError();
		}
		PrintDeteteResult(sequence,  r,  thread);
		return -1;
	}
	else
	{

		PrintDeteteResult(sequence,  r,  thread);

	}
	
	#ifdef __ABIO_WIN		
	#else
	va_close(30);
	#endif
	
	return 0;

}

static void PrintDeteteResult(char * file, int result, char * thread)
{
	if (result == 0)
	{
		SendJobLog(LOG_MSG_ID_ORACLE_RMAN_BACKUP_DELETE_ARCHIVELOG_SUCCESS, file, thread, "");
	}
	else
	{
		SendJobLog(LOG_MSG_ID_ORACLE_RMAN_BACKUP_DELETE_ARCHIVELOG_FAIL, file, thread, "");
	}


}

int ExecuteVersionSql(char * queryFileName)
{
	char argQueryFile[MAX_PATH_LENGTH];
	char argOutputFile[MAX_PATH_LENGTH];
	char argQueryInfo[MAX_PATH_LENGTH];	
	int r;
	int i=0;


	memset(argQueryFile, 0, sizeof(argQueryFile));
	memset(argOutputFile, 0, sizeof(argOutputFile));
	memset(argQueryInfo, 0, sizeof(argQueryInfo));
	sprintf(argOutputFile, "\"%s%c%s\"", ABIOMASTER_CLIENT_FOLDER, FILE_PATH_DELIMITER, outputFile);
	
	sprintf(argQueryFile, "@%s", queryFileName);


	if((int)va_open(ABIOMASTER_CLIENT_FOLDER_SCRIPT,"version.sql",O_RDONLY,0,0) == -1)
	{
	 	CreateCommandFile("version.sql","version.sql",NULL,NULL,NULL,NULL);	
	}
	
	// query를 실행한다.
	#ifdef __ABIO_WIN
		if(strcmp(account,"sys")==0)
		{
			sprintf(argQueryInfo, "\"%s/%s@%s as sysdba\"", account, passwd, sid);
		}
		else
		{
			sprintf(argQueryInfo, "%s/%s@%s", account, passwd, sid);
		}

		if(0 < debug_level)
		{
			WriteDebugData(ABIOMASTER_CLIENT_FOLDER_SCRIPT,"para.log","%s -S %s %s %s",sqlcmdUtil,argQueryInfo,argQueryFile,argOutputFile);
		}

		r = va_spawn_process(WAIT_PROCESS, sqlcmdUtil, "-S", argQueryInfo, argQueryFile, argOutputFile, NULL);
	#else			
		
		if(strcmp(account,"sys")==0)
		{
			sprintf(argQueryInfo, "%s/%s@%s as sysdba", account, passwd, sid);
		}
		else
		{
			sprintf(argQueryInfo, "%s/%s@%s", account, passwd, sid);
		}

		if(0 < debug_level)
		{
			WriteDebugData(ABIOMASTER_CLIENT_FOLDER_SCRIPT,"para.log","%s -S %s %s %s",sqlcmdUtil,argQueryInfo,argQueryFile,argOutputFile);			
		}

		r = va_spawn_process(WAIT_PROCESS, username, sqlcmdUtil, "-S", argQueryInfo, argQueryFile, argOutputFile, NULL);
	#endif
	if (r != 0)
	{		
		if (r == -1)
		{
			PrintQueryFileError(LOG_MSG_ID_ORACLE_RMAN_QUERY_ERROR_EXECUTE_SHELL, "");
		}
		else
		{
			PrintQueryFileError(NULL);
		}
		
		return -1;
	}	
	
	return 0;
}



int ExecuteCdbSql(char * queryFileName)
{
	char argQueryFile[MAX_PATH_LENGTH];
	char argOutputFile[MAX_PATH_LENGTH];
	char argQueryInfo[MAX_PATH_LENGTH];	
	int r;
	int i=0;


	memset(argQueryFile, 0, sizeof(argQueryFile));
	memset(argOutputFile, 0, sizeof(argOutputFile));
	memset(argQueryInfo, 0, sizeof(argQueryInfo));
	
	sprintf(argOutputFile, "\"%s%c%s\"", ABIOMASTER_CLIENT_FOLDER, FILE_PATH_DELIMITER, outputFile);
	
	sprintf(argQueryFile, "@%s", queryFileName);
	//sprintf(argQueryFile, "@%s", "/blanc/test1/abio_bk/abio/db/oracle_rman/scripts/cdb.sql");

	if((int)va_open(ABIOMASTER_CLIENT_FOLDER_SCRIPT,"cdb.sql",O_RDONLY,0,0) == -1)
	{
	 	CreateCommandFile("cdb.sql","cdb.sql",NULL,NULL,NULL,NULL);	
	}
	
	// query를 실행한다.
	#ifdef __ABIO_WIN
		if(strcmp(account,"sys")==0)
		{
			sprintf(argQueryInfo, "\"%s/%s@%s as sysdba\"", account, passwd, sid);
		}
		else
		{
			sprintf(argQueryInfo, "%s/%s@%s", account, passwd, sid);
		}

		if(0 < debug_level)
		{
			WriteDebugData(ABIOMASTER_CLIENT_FOLDER_SCRIPT,"para.log","%s -S %s %s %s",sqlcmdUtil,argQueryInfo,argQueryFile,argOutputFile);
		}

		r = va_spawn_process(WAIT_PROCESS, sqlcmdUtil, "-S", argQueryInfo, argQueryFile, argOutputFile, NULL);
	#else			
		
		if(strcmp(account,"sys")==0)
		{
			sprintf(argQueryInfo, "%s/%s@%s as sysdba", account, passwd, sid);
		}
		else
		{
			sprintf(argQueryInfo, "%s/%s@%s", account, passwd, sid);
		}

		if(0 < debug_level)
		{
			WriteDebugData(ABIOMASTER_CLIENT_FOLDER_SCRIPT,"para.log","%s -S %s %s %s",sqlcmdUtil,argQueryInfo,argQueryFile,argOutputFile);			
		}

		r = va_spawn_process(WAIT_PROCESS, username, sqlcmdUtil, "-S", argQueryInfo, argQueryFile, argOutputFile, NULL);
	#endif
	if (r != 0)
	{		
		if (r == -1)
		{
			PrintQueryFileError(LOG_MSG_ID_ORACLE_RMAN_QUERY_ERROR_EXECUTE_SHELL, "");
		}
		else
		{
			PrintQueryFileError(NULL);
		}
		
		return -1;
	}	
	
	return 0;
}



int ExcuteRmanExpireBackupset(char * queryFileName, char * backupsetID)
{
	char argQueryFile[MAX_PATH_LENGTH];
	char argOutputFile[MAX_PATH_LENGTH];
	char argQueryInfo[MAX_PATH_LENGTH];

	char username[ORACLE_USERNAME_LENGTH];
	char account[ORACLE_ACCOUNT_LENGTH];
	char passwd[ORACLE_PASSWORD_LENGTH];
	char sid[ORACLE_SID_LENGTH];
	char home[ORACLE_HOME_LENGTH];

	int r;



	memset(argQueryFile, 0, sizeof(argQueryFile));
	memset(argOutputFile, 0, sizeof(argOutputFile));
	memset(argQueryInfo, 0, sizeof(argQueryInfo));

	strcpy(username, ((struct oracleRmanDB *)commandData.db.dbData)->username);
	strcpy(account, ((struct oracleRmanDB *)commandData.db.dbData)->account);
	strcpy(passwd, ((struct oracleRmanDB *)commandData.db.dbData)->passwd);
	strcpy(sid, ((struct oracleRmanDB *)commandData.db.dbData)->sid);
	strcpy(home, ((struct oracleRmanDB *)commandData.db.dbData)->home);



	// query를 실행한다.
	sprintf(argOutputFile, "%s%c%s", ABIOMASTER_CLIENT_FOLDER, FILE_PATH_DELIMITER, outputFile);
	#ifdef __ABIO_WIN
		sprintf(argQueryFile, "@%s", queryFileName);
		sprintf(argQueryInfo, "%s/%s", account, passwd);
		
		r = va_spawn_process(WAIT_PROCESS, ORACLE_RMAN_QUERY_FILE_SHELL, "/C", argQueryFile, home, sid, account, passwd, backupsetID, argOutputFile, NULL);
	#else
		sprintf(argQueryFile, "@%s", queryFileName);
		sprintf(argQueryInfo, "%s/%s", account, passwd);
		
		r = va_spawn_process(WAIT_PROCESS, username, ORACLE_RMAN_QUERY_FILE_SHELL, argQueryFile, home, sid, account, passwd, backupsetID, argOutputFile, NULL);
	#endif

	if (r != 0)
	{
		if (r == -1)
		{
			SendJobLog(LOG_MSG_ID_ORACLE_RMAN_QUERY_ERROR_EXECUTE_SHELL, sqlcmdUtil, "");
		}
		else
		{
			PrintQueryError();
		}
		
		return -1;
	}


	return 0;
}

int ExecuteSql(char * query)
{
	char argQueryFile[MAX_PATH_LENGTH];
	char argQueryInfo[MAX_PATH_LENGTH];	

	int r;
	
	memset(argQueryInfo, 0, sizeof(argQueryInfo));
	
	// 실행할 query 파일을 만든다.
	if (MakeQueryFile(query) != 0)
	{
		SendJobLog(LOG_MSG_ID_ORACLE_RMAN_QUERY_ERROR_MAKE_QUERY_FILE, "");
		
		return 2;
	}
	

	// query를 실행한다.
	#ifdef __ABIO_WIN

		if(strcmp(account,"sys")==0)
		{
			sprintf(argQueryInfo, "\"%s/%s@%s as sysdba\"", account, passwd, sid);
		}
		else
		{
			sprintf(argQueryInfo, "%s/%s@%s", account, passwd, sid);
		}

		sprintf(argQueryFile, "@\"%s%c%s\"", ABIOMASTER_CLIENT_FOLDER, FILE_PATH_DELIMITER, queryFile);

		if(0 < debug_level)
		{
			WriteDebugData(ABIOMASTER_CLIENT_FOLDER_SCRIPT,"para.log","%s -S %s %s",sqlcmdUtil,argQueryInfo,argQueryFile);			
		}

		r = va_spawn_process(WAIT_PROCESS, sqlcmdUtil, "-S", argQueryInfo, argQueryFile, NULL);

	#else

		if(strcmp(account,"sys")==0)
		{
			sprintf(argQueryInfo, "%s/%s@%s as sysdba", account, passwd, sid);
		}
		else
		{
			sprintf(argQueryInfo, "%s/%s@%s", account, passwd, sid);
		}

		sprintf(argQueryFile, "@%s%c%s", ABIOMASTER_CLIENT_FOLDER, FILE_PATH_DELIMITER, queryFile);

		if(0 < debug_level)
		{
			WriteDebugData(ABIOMASTER_CLIENT_FOLDER_SCRIPT,"para.log","%s -S %s %s",sqlcmdUtil,argQueryInfo,argQueryFile);			
		}

		r = va_spawn_process(WAIT_PROCESS, username, sqlcmdUtil, "-S", argQueryInfo, argQueryFile, NULL);
	#endif
	if (r != 0)
	{
		if (r == -1)
		{
			SendJobLog(LOG_MSG_ID_ORACLE_RMAN_QUERY_ERROR_EXECUTE_SHELL, sqlcmdUtil, "");
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

int MakeQueryFile(char * query)
{
	char ** lines;
	
	char spoolQuery[MAX_PATH_LENGTH];
	
	
	
	
	// initialize variables
	memset(spoolQuery, 0, sizeof(spoolQuery));
	
	
	sprintf(spoolQuery, ORACLE_SPOOL_ON_QUERY, ABIOMASTER_CLIENT_FOLDER, FILE_PATH_DELIMITER, outputFile);
	
	
	lines = (char **)malloc(sizeof(char *) * ORACLE_QUERY_LINE_NUMBER);
	
	lines[0] = ORACLE_LINESIZE_QUERY;
	lines[1] = spoolQuery;	
	lines[2] = query;
	lines[3] = ORACLE_SPOOL_OFF_QUERY;
	lines[4] = ORACLE_QUIT_QUERY;
	if (va_write_text_file_lines(ABIOMASTER_CLIENT_FOLDER, queryFile, lines, NULL, ORACLE_QUERY_LINE_NUMBER) != 0)
	{
		va_free(lines);
		
		return 2;
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

int getDBVersion()
{	
	int version;
	char tempVersion[3];

	char ** lines;
	int * linesLength;
	int lineNumber;

	int i;
	int j;
	int k;
	
	version = 0;

	i = 0;
	j = 0;
	k = 0;

	/*if(DBVersion <= 0)
	{*/
		memset(tempVersion,0,3);

		//printf("outputFile: %s\n",outputFile);

		if (ExecuteVersionSql(ORACLE_RMAN_QUERY_VERSION) != 0)
		{
			printf("not executesql\n");
			va_remove(ABIOMASTER_CLIENT_FOLDER, outputFile);
			
			return 0;
		}
		
		// sql query 결과를 읽는다.

		if ((lineNumber = va_load_text_file_lines(ABIOMASTER_CLIENT_FOLDER, outputFile, &lines, &linesLength)) > 0)
		{
			for (i = 0; i < lineNumber; i++)
			{
				if (lines[i][0] == '-')
				{
					for (i = i + 1; i < lineNumber; i++)
					{
						if (lines[i][0] != '-')
						{
							break;
						}
					}

					if (lines[i][0] == '\0')
					{
						break;
					}

					for (j = 0; j < linesLength[i]; j++)				
					{
						if (lines[i][j] == '.')
						{
							break;
						}
						tempVersion[j] = lines[i][j];
					}

					if(0 < strlen(tempVersion))
					{
						tempVersion[2] = '\0';
						version = (int)va_atoll(tempVersion);
					}

					break;
				}
			}		
			
			for (i = 0; i < lineNumber; i++)
			{
				va_free(lines[i]);
			}
			va_free(lines);
			va_free(linesLength);
		}
		
		printf("version = %d\n",version);

		WriteDebugData(ABIOMASTER_CLIENT_FOLDER,"httpd.log","version = %d",version);

		va_remove(ABIOMASTER_CLIENT_FOLDER, outputFile);	
		
		//DBVersion = version;
	//}
	/*else
	{
		printf("exist version\n");
		return DBVersion;
	}*/

	return version;
}


//cdb
int getCDB()
{	
	int cdb;

	char ** lines;
	int * linesLength;
	int lineNumber;

	int i;
	int j;
	int k;
	
	cdb = 0;

	i = 0;
	j = 0;
	k = 0;

	
		if (ExecuteCdbSql(ORACLE_RMAN_QUERY_FILE_GET_CDB_LIST) != 0)
		{
			printf("not executesql\n");
			va_remove(ABIOMASTER_CLIENT_FOLDER, outputFile);
			
			return 0;
		}
		
		// sql query 결과를 읽는다.

		if ((lineNumber = va_load_text_file_lines(ABIOMASTER_CLIENT_FOLDER, outputFile, &lines, &linesLength)) > 0)
		{
			for (i = 0; i < lineNumber; i++)
			{
				if (lines[i][0] == '-')
				{
					for (i = i + 1; i < lineNumber; i++)
					{
						if (lines[i][0] != '-')
						{
							break;
						}
					}

					if (lines[i][0] == '\0')
					{
						break;
					}

				
					if(lines[i][0] == 'Y')
					{
						cdb = 1;
					}
					else
					{
						cdb = 0;
					}

					break;
				}
			}		
			
			for (i = 0; i < lineNumber; i++)
			{
				va_free(lines[i]);
			}
			va_free(lines);
			va_free(linesLength);
		}
		
		printf("cdb = %d\n",cdb);

		WriteDebugData(ABIOMASTER_CLIENT_FOLDER,"httpd.log","cdb = %d", cdb);

		va_remove(ABIOMASTER_CLIENT_FOLDER, outputFile);	
		
	

	return cdb;
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
		else
		{
			SendJobLog(LOG_MSG_ID_ORACLE_RMAN_QUERY_ERROR_EXECUTE_SCRIPT, "");
		}
}

static void Error()
{
	///////////////////////////////////////////////////////////////////////////////////////////////////
	// 1. backup server에 에러가 발생했음을 알린다.
	
	// backup server에게 client에서 에러가 발생했음을 알려준다.
	SendCommand(1, 0, 1, MODULE_NAME_SLAVE_BACKUP, "ERROR", MODULE_NAME_DB_ORACLE_RMAN_MULTI, clientIP, clientPort, clientNodename, serverIP, serverPort, serverNodename);
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
	va_send_abio_log(masterIP, masterLogPort, commandData.jobID, NULL, LOG_LEVEL_JOB, MODULE_DB_ORACLE_RMAN_ORACLE_RMAN, logmsg);
}


int CheckQueryResult(rman_fd_t * fdOracleRman)
{
	int termCode;
	

	// isql 실행이 끝날 때 까지 기다린다.
	#ifdef __ABIO_WIN
		if (fdOracleRman->pid > 0)
		{
			if (fdOracleRman->pid == _cwait(&termCode, fdOracleRman->pid, 0))
			{
				if (termCode != 0)
				{
					PrintQueryError();
					
					return 2;
				}
			}
		}
	#else
		if (fdOracleRman->pid > 0)
		{
			if (fdOracleRman->pid == waitpid(fdOracleRman->pid, &termCode, 0))
			{
				if (termCode != 0)
				{
					PrintQueryError();
					
					return 2;
				}
			}
		}
	#endif
	
	
	return 0;
}


int DivideSeperator(char * dest, char * output1, char * output2)
{
	char * ptr;

	ptr = strtok(dest, CONTAINER_SEPARATOR);

	sprintf(output1, "%s", ptr);

	ptr = strtok(NULL, CONTAINER_SEPARATOR);

	if(ptr!=NULL)
		sprintf(output2, "%s", ptr);
	
	return 0;
}

