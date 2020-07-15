#include "require.h"
#include "abio.h"
#include "lib.h"
#include "vendor.h"
#include "httpd.h"


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
char ABIOMASTER_CLIENT_LOG_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_CLIENT_FOLDER_SCRIPT[MAX_PATH_LENGTH];

char ORACLE_RMAN_QUERY_FILE_GET_TABLESPACE_LIST[MAX_PATH_LENGTH * 2];
char ORACLE_RMAN_QUERY_FILE_GET_TABLESPACE_LIST12[MAX_PATH_LENGTH * 2];
char ORACLE_RMAN_QUERY_FILE_LIST_BACKUP[MAX_PATH_LENGTH];
char ORACLE_RMAN_QUERY_FILE_RESTORE_PREVIEW[MAX_PATH_LENGTH];
char ORACLE_RMAN_QUERY_FILE_EXPIRE[MAX_PATH_LENGTH];
char ORACLE_RMAN_QUERY_VERSION[MAX_PATH_LENGTH];

struct ck command;				// communication kernel command
struct ckBackup commandData;	// communication kernel command data


//////////////////////////////////////////////////////////////////////////////////////////////////
// ABIO log option
int requestLogLevel;
char logJobID[JOB_ID_LENGTH];
int logModuleNumber;


///////////////////////////////////////////////////////////////////////////////////////////////////
// message transfer variables
va_trans_t mid;			// message transfer id
int mtype;				// message type number


///////////////////////////////////////////////////////////////////////////////////////////////////
// variables for oracle rman connection
char username[ORACLE_RMAN_USERNAME_LENGTH];
char account[ORACLE_RMAN_ACCOUNT_LENGTH];
char passwd[ORACLE_RMAN_PASSWORD_LENGTH];
char sid[ORACLE_RMAN_SID_LENGTH];
char home[ORACLE_RMAN_HOME_LENGTH];

char environmentValueSid[MAX_PATH_LENGTH];
char environmentValueHome[MAX_PATH_LENGTH];

char sqlcmdUtil[MAX_PATH_LENGTH];

char outputFile[MAX_PATH_LENGTH];


///////////////////////////////////////////////////////////////////////////////////////////////////
// variables for oracle rman file list
struct tableSpaceItem * tableSpaceList;
int tableSpaceNumber;

__uint64 archiveLogSize;
__uint64 controlFileSize;


///////////////////////////////////////////////////////////////////////////////////////////////////
// ip and port number
char masterIP[IP_LENGTH];					// master server ip address
char masterPort[PORT_LENGTH];				// master server ck port
char masterNodename[NODE_NAME_LENGTH];		// master server node name
char masterLogPort[PORT_LENGTH];			// master server httpd logger port

char jobId[JOB_ID_LENGTH];
//////////////////////////////////////////////////////////////////////////////////////////////////
// backup/restore log
struct _backupLogDescription * backupLogDescription;
int backupLogDescriptionNumber;

int debug_level;

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
			mid = (va_trans_t)atoi(argv[1]);
			mtype = atoi(argv[2]);
		#elif __ABIO_WIN
			#ifdef __ABIO_64
				mid = (va_trans_t)_wtoi64(argv[1]);
			#else
				mid = (va_trans_t)_wtoi(argv[1]);
			#endif
			mtype = _wtoi(argv[2]);
		#endif
		
		va_msgrcv(mid, mtype, &command, sizeof(struct ck), 0);
		memcpy(&commandData, command.dataBlock, sizeof(struct ckBackup));
		
		
		// initialize a process
		#ifdef __ABIO_UNIX
			if (va_convert_string_to_utf8(ENCODING_UNKNOWN, argv[0], (int)strlen(argv[0]), (void **)&convertFilename, &convertFilenameSize) == 0)
			{
				Exit();
				
				return 2;
			}
		#elif __ABIO_WIN
			if (va_convert_string_to_utf8(ENCODING_UTF16_LITTLE, (char *)argv[0], (int)wcslen(argv[0]) * sizeof(wchar_t), (void **)&convertFilename, &convertFilenameSize) == 0)
			{
				Exit();
				
				return 2;
			}
		#endif
		
		if (InitProcess(convertFilename) < 0)
		{
			va_free(convertFilename);
			
			Exit();
			
			return 2;
		}
		
		va_free(convertFilename);
		
		
		ResponseRequest();
		
		
		Exit();
		
		return 0;
	}

int InitProcess(char * filename)
{
	#ifdef __ABIO_WIN
		WSADATA wsaData;
		HANDLE token;
	#endif
	
	struct oracleRmanDB oracleRman;
	
	int i;
	
	
	
	///////////////////////////////////////////////////////////////////////////////////////////////////
	// 1. 전역 변수들을 초기화한다.
	
	memset(ABIOMASTER_CLIENT_FOLDER, 0, sizeof(ABIOMASTER_CLIENT_FOLDER));
	memset(ABIOMASTER_CLIENT_LOG_FOLDER, 0, sizeof(ABIOMASTER_CLIENT_LOG_FOLDER));
	memset(ORACLE_RMAN_QUERY_FILE_GET_TABLESPACE_LIST, 0, sizeof(ORACLE_RMAN_QUERY_FILE_GET_TABLESPACE_LIST));
	memset(ORACLE_RMAN_QUERY_FILE_GET_TABLESPACE_LIST12, 0, sizeof(ORACLE_RMAN_QUERY_FILE_GET_TABLESPACE_LIST12));
	memset(ORACLE_RMAN_QUERY_FILE_EXPIRE, 0, sizeof(ORACLE_RMAN_QUERY_FILE_EXPIRE));
	memset(ORACLE_RMAN_QUERY_FILE_LIST_BACKUP, 0, sizeof(ORACLE_RMAN_QUERY_FILE_LIST_BACKUP));
	memset(ORACLE_RMAN_QUERY_FILE_RESTORE_PREVIEW, 0, sizeof(ORACLE_RMAN_QUERY_FILE_RESTORE_PREVIEW));
	memset(ABIOMASTER_CLIENT_FOLDER_SCRIPT, 0 ,sizeof(ABIOMASTER_CLIENT_FOLDER_SCRIPT));
	memset(ORACLE_RMAN_QUERY_VERSION, 0 ,sizeof(ORACLE_RMAN_QUERY_VERSION));
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// ABIO log option
	requestLogLevel = LOG_LEVEL_JOB;
	memset(logJobID, 0, sizeof(logJobID));
	logModuleNumber = MODULE_UNKNOWN;
	
	debug_level = 0;

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// variables for oracle rman connection
	memset(username, 0, sizeof(username));
	memset(account, 0, sizeof(account));
	memset(passwd, 0, sizeof(passwd));
	memset(sid, 0, sizeof(sid));
	memset(home, 0, sizeof(home));
	
	memset(environmentValueSid, 0, sizeof(environmentValueSid));
	memset(environmentValueHome, 0, sizeof(environmentValueHome));
	
	memset(sqlcmdUtil, 0, sizeof(sqlcmdUtil));
	memset(outputFile, 0, sizeof(outputFile));
	
	
	///////////////////////////////////////////////////////////////////////////////////////////////////
	// variables for oracle rman file list
	tableSpaceList = NULL;
	tableSpaceNumber = 0;
	
	archiveLogSize = 0;
	controlFileSize = 0;
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// ip and port number
	memset(masterIP, 0, sizeof(masterIP));
	memset(masterPort, 0, sizeof(masterPort));
	memset(masterNodename, 0, sizeof(masterNodename));
	memset(masterLogPort, 0, sizeof(masterLogPort));
	
	
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
	sprintf(ABIOMASTER_CLIENT_FOLDER_SCRIPT,"%s%c%s%c",ABIOMASTER_CLIENT_FOLDER,FILE_PATH_DELIMITER,"scripts",FILE_PATH_DELIMITER);
	// set module number for debug log of ABIO common library
	logModuleNumber = MODULE_DB_ORACLE_RMAN_HTTPD;
	
	// set master server ip and port
	strcpy(masterIP, command.sourceIP);
	strcpy(masterPort, command.sourcePort);
	strcpy(masterNodename, command.sourceNodename);
	strcpy(masterLogPort, commandData.client.masterLogPort);
	
	// set account and password
	memcpy(&oracleRman, commandData.db.dbData, sizeof(struct oracleRmanDB));
	strcpy(username, oracleRman.username);
	strcpy(account, oracleRman.account);
	strcpy(passwd, oracleRman.passwd);
	strcpy(sid, oracleRman.sid);
	strcpy(home, oracleRman.home);
	
	#ifdef __ABIO_WIN
		va_slash_to_backslash(home);
	#endif
	
	// set the sqlcmd utility
	sprintf(sqlcmdUtil, "%s%cbin%c%s", home, FILE_PATH_DELIMITER, FILE_PATH_DELIMITER, ORACLE_RMAN_SQLCMD_UTILITY);
	sprintf(outputFile, "oracle_rman_%d.out", va_getpid());
	
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
	
	
	#ifdef __ABIO_WIN
		// windows socket을 open한다.
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) 
		{
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
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 4. process 보안 권한 변경을 시도한다.
	
	#ifdef __ABIO_WIN
		if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &token) == TRUE)
		{
			if (SetPrivilege(token, ABIO_NT_PRIVILEGE_SE_BACKUP_NAME, 1) == FALSE)
			{
				return -1;
	  		}
			
	  		if (SetPrivilege(token, ABIO_NT_PRIVILEGE_SE_RESTORE_NAME, 1) == FALSE)
	  		{
				return -1;
	  		}
			
	  		if (SetPrivilege(token, ABIO_NT_PRIVILEGE_SE_SECURITY_NAME, 1) == FALSE)
			{
				return -1;
	  		}
		}
		else
		{
			return -1;
		}
	#endif
	
	
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
			if (!strcmp(moduleConfig[i].optionName, CLIENT_LOG_FOLDER_OPTION_NAME))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(ABIOMASTER_CLIENT_LOG_FOLDER) - 1)
				{
					strcpy(ABIOMASTER_CLIENT_LOG_FOLDER, moduleConfig[i].optionValue);
				}
			}
			else if (!strcmp(moduleConfig[i].optionName, ORACLE_RMAN_QUERY_FILE_GET_TABLESPACE_LIST_OPTION_NAME))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(ORACLE_RMAN_QUERY_FILE_GET_TABLESPACE_LIST) - 1)
				{
					strcpy(ORACLE_RMAN_QUERY_FILE_GET_TABLESPACE_LIST, moduleConfig[i].optionValue);
				}
			}
			else if (!strcmp(moduleConfig[i].optionName, ORACLE_RMAN_QUERY_FILE_GET_TABLESPACE_LIST12_OPTION_NAME))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(ORACLE_RMAN_QUERY_FILE_GET_TABLESPACE_LIST12) - 1)
				{
					strcpy(ORACLE_RMAN_QUERY_FILE_GET_TABLESPACE_LIST12, moduleConfig[i].optionValue);
				}
			}
			else if (!strcmp(moduleConfig[i].optionName, ORACLE_RMAN_QUERY_FILE_LIST_BACKUP_OPTION_NAME))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(ORACLE_RMAN_QUERY_FILE_LIST_BACKUP) - 1)
				{
					strcpy(ORACLE_RMAN_QUERY_FILE_LIST_BACKUP, moduleConfig[i].optionValue);
				}
			}
			else if (!strcmp(moduleConfig[i].optionName, ORACLE_RMAN_QUERY_FILE_RESTORE_PREVIEW_OPTION_NAME))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(ORACLE_RMAN_QUERY_FILE_RESTORE_PREVIEW) - 1)
				{
					strcpy(ORACLE_RMAN_QUERY_FILE_RESTORE_PREVIEW, moduleConfig[i].optionValue);
				}
			}
			else if (!strcmp(moduleConfig[i].optionName, DEBUG_LEVEL))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(DEBUG_LEVEL) - 1)
				{
					debug_level = atoi(moduleConfig[i].optionValue);					
				}
			}
			else if (!strcmp(moduleConfig[i].optionName, ORACLE_RMAN_QUERY_FILE_EXPIRE_OPTION_NAME))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(ORACLE_RMAN_QUERY_FILE_EXPIRE) - 1)
				{
					strcpy(ORACLE_RMAN_QUERY_FILE_EXPIRE, moduleConfig[i].optionValue);
				}
			}
			else if (!strcmp(moduleConfig[i].optionName, ORACLE_RMAN_QUERY_FILE_GET_VERSION_OPTION_NAME))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(ORACLE_RMAN_QUERY_VERSION) - 1)
				{
					strcpy(ORACLE_RMAN_QUERY_VERSION, moduleConfig[i].optionValue);
				}
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
		return -1;
	}

	if (ORACLE_RMAN_QUERY_FILE_GET_TABLESPACE_LIST12[0] == '\0')
	{
		return -1;
	}
	
	if (ORACLE_RMAN_QUERY_FILE_GET_TABLESPACE_LIST[0] == '\0')
	{
		return -1;
	}
	
	if (ORACLE_RMAN_QUERY_FILE_LIST_BACKUP[0] == '\0')
	{
		return -1;
	}
	
	if (ORACLE_RMAN_QUERY_FILE_RESTORE_PREVIEW[0] == '\0')
	{
		return -1;
	}

	if (ORACLE_RMAN_QUERY_FILE_EXPIRE[0] == '\0')
	{
		return -1;
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
	va_send_abio_log(commandData.client.masterIP, masterLogPort, jobId, NULL, LOG_LEVEL_JOB, MODULE_DB_ORACLE_RMAN_ORACLE_RMAN, logmsg);
}
void PrintError()
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
int RestoreArchivePreview()
{
	char ** lines;
	int * linesLength;
	int lineNumber;

	va_sock_t commandSock;
	char msg[DSIZ * DSIZ];
	char tablespacesName[ABIO_FILE_LENGTH * 2];
	char backupset[BACKUPSET_ID_LENGTH];

	char tablespace[ABIO_FILE_LENGTH * 2];
	char container[ABIO_FILE_LENGTH * 2];

	memset(tablespacesName,0,sizeof(tablespacesName));
	memset(tablespace,0,sizeof(tablespace));
	memset(container,0,sizeof(container));
	memset(backupset,0,sizeof(backupset));
	char sendmsg[10];
	int i;
	int j;

	if ((commandSock = va_connect(commandData.client.masterIP, commandData.catalogPort, 1)) != -1)
	{
		memset(msg, 0, sizeof(msg));
		va_recv(commandSock, msg, 512, 0 ,DATA_TYPE_NOT_CHANGE);
		strcpy(tablespacesName,msg);
		memset(msg, 0, sizeof(msg));
		va_recv(commandSock, msg, 16, 0 ,DATA_TYPE_NOT_CHANGE);
		strcpy(backupset,msg);
		memset(jobId, 0, sizeof(jobId));
		va_recv(commandSock, jobId, 64, 0, DATA_TYPE_NOT_CHANGE);

		DivideSeperator(tablespacesName,tablespace,container );

		//printf("[rman-httpd]tb : %s\n", tablespace);

		if(10 <= getDBVersion())
		{
			if(ExcuteRmanArchivePreview(ORACLE_RMAN_QUERY_FILE_RESTORE_PREVIEW,tablespace,container,backupset)==0)
			{
				if ((lineNumber = va_load_text_file_lines(ABIOMASTER_CLIENT_FOLDER, outputFile, &lines, &linesLength)) > 0)
				{
					for(i = 0; i < lineNumber; i++)
					{
						if(lines[i][0]=='n' && lines[i][1]=='o')
						{
							memset(sendmsg,0,sizeof(sendmsg));
							va_send(commandSock, sendmsg, sizeof(sendmsg), 0 ,DATA_TYPE_NOT_CHANGE);
							va_close_socket(commandSock,ABIO_SOCKET_CLOSE_CLIENT);
							PrintError();
							for (j = 0; j < lineNumber; j++)
							{
								va_free(lines[j]);
							}
							va_free(lines);
							va_free(linesLength);
							va_remove(ABIOMASTER_CLIENT_FOLDER,outputFile);
							return 0;
						}
					}		
					for (i = 0; i < lineNumber; i++)
					{
						va_free(lines[i]);
					}
					va_free(lines);
					va_free(linesLength);
				}	
			}
			else
			{
				memset(sendmsg,0,sizeof(sendmsg));
				strcpy(sendmsg,"fail");
				va_send(commandSock, sendmsg, sizeof(sendmsg), 0 ,DATA_TYPE_NOT_CHANGE);
				va_close_socket(commandSock,ABIO_SOCKET_CLOSE_CLIENT);
			}
		}
		else
		{
		
		}
	}
	memset(sendmsg,0,sizeof(sendmsg));
	strcpy(sendmsg,"ok");
	va_send(commandSock, sendmsg, sizeof(sendmsg), 0 ,DATA_TYPE_NOT_CHANGE);
	va_close_socket(commandSock,ABIO_SOCKET_CLOSE_CLIENT);
	va_remove(ABIOMASTER_CLIENT_FOLDER,outputFile);
	return 0;
	
}
void ResponseRequest()
{	
	
	if (!strcmp(command.subCommand, "<GET_ORACLE_RMAN_FILE_LIST>"))
	{
		GetOracleRmanFileList();
	}
	else if (!strcmp(command.subCommand, "<RESTORE_ARCHIVE_PREVIEW>"))
	{
		RestoreArchivePreview();
	}
	else if (!strcmp(command.subCommand, "<GET_ORACLE_RMAN_RESTORE_BACKUPSET_LIST>"))
	{
		GetOracleRmanRestoreBackupsetList();
	}
	/*else if (!strcmp(command.subCommand, "<SYNC_ORACLE_RMAN_BACKUPSET_LIST>"))
	{
		SyncOracleRmanBackupsetList();
	}*/
	else if (!strcmp(command.subCommand, "<EXPIRE_ORACLE_RMAN_BACKUPSET>"))
	{
		ExpireOracleRmanBackupset();
	}
	else if (!strcmp(command.subCommand, "<EXPIRE_ORACLE_RMAN_BACKUPSET_SILENT>"))
	{
		ExpireOracleRmanBackupsetSilent();
	}
	else if (!strcmp(command.subCommand, "<EXPIRE_BACKUPSET>"))
	{
		ExpireBackupset();
	}
	else if (!strcmp(command.subCommand, "<GET_ENTIRE_DATABASES>"))
	{
		GetEntireDatabases();		
	}
}

void ExpireBackupset()
{
	va_sock_t commandSock;
	
	char backupset[BACKUPSET_ID_LENGTH];
	char ** expiredBackupsetList;
	int expiredBackupsetListNumber;
		
	int i;
	
	
	
	// initialize variables
	expiredBackupsetList = NULL;
	expiredBackupsetListNumber = 0;
	
	
	if ((commandSock = va_connect(masterIP, commandData.catalogPort, 1)) != -1)
	{
		memset(backupset, 0, sizeof(backupset));
		while (va_recv(commandSock, backupset, BACKUPSET_ID_LENGTH, 0, DATA_TYPE_NOT_CHANGE) > 0)
		{
			if (expiredBackupsetListNumber % DEFAULT_MEMORY_FRAGMENT == 0)
			{
				expiredBackupsetList = (char **)realloc(expiredBackupsetList, sizeof(char *) * (expiredBackupsetListNumber + DEFAULT_MEMORY_FRAGMENT));
				memset(expiredBackupsetList + expiredBackupsetListNumber, 0, sizeof(char *) * DEFAULT_MEMORY_FRAGMENT);
			}
			
			expiredBackupsetList[expiredBackupsetListNumber] = (char *)malloc(sizeof(char) * (strlen(backupset) + 1));
			memset(expiredBackupsetList[expiredBackupsetListNumber], 0, sizeof(char) * (strlen(backupset) + 1));
			strcpy(expiredBackupsetList[expiredBackupsetListNumber], backupset);
			expiredBackupsetListNumber++;
			
			memset(backupset, 0, sizeof(backupset));
		}
		
		va_close_socket(commandSock, ABIO_SOCKET_CLOSE_SERVER);

		char argOutputFile[MAX_PATH_LENGTH];

		memset(argOutputFile, 0, sizeof(argOutputFile));


		sprintf(argOutputFile, "%s%c%s", ABIOMASTER_CLIENT_FOLDER, FILE_PATH_DELIMITER, outputFile);

		if (ExcuteRmanExpireBackupset(ORACLE_RMAN_QUERY_FILE_EXPIRE, backupset) == 0)
		{


		}

		for (i = 0; i < expiredBackupsetListNumber; i++)
		{
			va_free(expiredBackupsetList[i]);
		}
		va_free(expiredBackupsetList);
	}
}


void GetEntireDatabases()
{

	char msg[DSIZ];
	va_fd_t fd;
	va_fd_t fdFilelist;
	va_sock_t commandSock;
	
	char tmpbuf[MAX_PATH_LENGTH];
	int readSize;
	int i;	
	
	readSize = 0;
	memset(tmpbuf,0,sizeof(tmpbuf));
	if ((commandSock = va_connect(masterIP, commandData.catalogPort, 1)) != -1)
	{
		if (GetTableSpaceList() < 0)
		{
			va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
			
			return;
		}	
		if ((fd = va_open(ABIOMASTER_CLIENT_FOLDER, "temp.txt",O_CREAT|O_RDWR|O_APPEND,0,0)) != (va_fd_t)-1)
		{
			for (i = 0; i < tableSpaceNumber; i++)
			{
				memset(msg,0,sizeof(msg));
				// table space 이름이 ABIO_FILE_LENGTH 보다 길면 백업하지 않는다.
				if (strlen(tableSpaceList[i].tableSpace) > ABIO_FILE_LENGTH)
				{
					continue;
				}
				sprintf(msg,"%s %s\n",ORACLE_RMAN_BACKUP_FILE_LIST_OPTION_OBJECT_TABLESPACE, tableSpaceList[i].tableSpace);
				va_write(fd,msg,(int)strlen(msg),DATA_TYPE_NOT_CHANGE);

				if(i == tableSpaceNumber -1)
				{
					va_write(fd,"ARCHIVE\n",9,DATA_TYPE_NOT_CHANGE);	

				}
			}
		}		
		
		va_close(fd);		
		if ((fdFilelist = va_open(ABIOMASTER_CLIENT_FOLDER, "temp.txt", O_RDONLY, 0, 0)) != (va_fd_t)-1)
		{
			while ((readSize = va_read(fdFilelist, tmpbuf, sizeof(tmpbuf), DATA_TYPE_NOT_CHANGE)) > 0)
			{
				if (va_send(commandSock, tmpbuf, readSize, 0, DATA_TYPE_NOT_CHANGE) <= 0)
				{								
					break;
				}
			}			
		}
		va_close(fdFilelist);
		va_remove(ABIOMASTER_CLIENT_FOLDER,"temp.txt");
		va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
	}
}

void GetOracleRmanFileList()
{
	char reply[DSIZ][DSIZ];
	char msg[DSIZ * DSIZ];
	
	va_sock_t commandSock;
	
	char mtime[TIME_STAMP_LENGTH];
		
	int i;
	int j;	
	
	if ((commandSock = va_connect(masterIP, commandData.catalogPort, 1)) != -1)
	{
		// get a oracle rman file list
		if (GetTableSpaceList() < 0)
		{
			va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
			
			return;
		}
	
		// send the table space list
		for (i = 0; i < tableSpaceNumber; i++)
		{
			// table space 이름이 ABIO_FILE_LENGTH 보다 길면 백업하지 않는다.
			if (strlen(tableSpaceList[i].tableSpace) > ABIO_FILE_LENGTH)
			{
				continue;
			}
			// data file 이름이 ABIO_FILE_LENGTH 보다 길면 백업하지 않는다.
			for (j = 0; j < tableSpaceList[i].fileNumber; j++)
			{
				if (strlen(tableSpaceList[i].dataFile[j]) > ABIO_FILE_LENGTH)
				{
					break;
				}
			}
			if (j < tableSpaceList[i].fileNumber)
			{
				continue;
			}
			memset(mtime, 0, sizeof(mtime));
			va_make_time_stamp((time_t)tableSpaceList[i].mtime, mtime, TIME_STAMP_TYPE_INTERNAL);
			
			// make reply message
			va_init_reply_buf(reply);
			
			strcpy(reply[0], "ORACLE_RMAN_FILE_LIST");
			strcpy(reply[91], tableSpaceList[i].tableSpace);
			strcpy(reply[92], tableSpaceList[i].database);
			va_ulltoa(tableSpaceList[i].size, reply[96]);	
			strcpy(reply[97], mtime);	
			
			memset(msg, 0, sizeof(msg));
			va_make_reply_msg(reply, msg);
			if (va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE) < 0)
			{
				break;
			}
		}		
		
		
		// send the archive log
		va_init_reply_buf(reply);
		
		strcpy(reply[0], "ORACLE_RMAN_FILE_LIST");
		strcpy(reply[91], ORACLE_BACKUP_FILE_LIST_OPTION_OBJECT_ARCHIVE_LOG);
		//va_ulltoa(archiveLogSize, reply[96]);
		
		memset(msg, 0, sizeof(msg));
		va_make_reply_msg(reply, msg);
		va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE);
		
		// send the control file
		va_init_reply_buf(reply);
		
		strcpy(reply[0], "ORACLE_RMAN_FILE_LIST");
		strcpy(reply[91], ORACLE_BACKUP_FILE_LIST_OPTION_OBJECT_CONTROL_FILE);
		//va_ulltoa(controlFileSize, reply[96]);
		
		memset(msg, 0, sizeof(msg));
		va_make_reply_msg(reply, msg);
		va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE);

		va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
	}
}

int GetTableSpaceList()
{
	char ** lines;
	int * linesLength;
	int lineNumber;
	
	char tableSpaceName[1024];
	char tableSpaceSize[1024];
	char dataFileNumber[1024];

	char dataFileName[1024];
	char databaseName[1024];
	
	int i;
	int j;
	int k;
	int l;

	int version;

	version = getDBVersion();
	
	if (ExecuteTablespaceSql(ORACLE_RMAN_QUERY_FILE_GET_TABLESPACE_LIST, version) != 0)
	{			
		if(0 == debug_level)
		{
			va_remove(ABIOMASTER_CLIENT_FOLDER, outputFile);
		}
		
		return -1;
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
				
				for ( ; i < lineNumber; i++)
				{
					if (lines[i][0] == '\0')
					{
						break;
					}
					
					memset(tableSpaceName, 0, sizeof(tableSpaceName));
					memset(tableSpaceSize, 0, sizeof(tableSpaceSize));
					memset(dataFileNumber, 0, sizeof(dataFileNumber));

					memset(databaseName, 0, sizeof(databaseName));

					// query 결과에서 tablespace 이름을 찾는다.
					if (12 > version)
					{
						for (j = 0; j < linesLength[i]; j++)
						{
							if (lines[i][j] == '^')
							{
								strncpy(tableSpaceName, lines[i], j);							
								break;
							}
						}
					
						// query 결과에서 tablespace size를 찾는다.
						for (k = linesLength[i] - 1; k > j; k--)
						{
							if (lines[i][k] == '^')
							{
								strncpy(tableSpaceSize, lines[i] + j + 1, k - (j + 1));
								break;
							}
						}
						for (l = linesLength[i] - 1; l > k ; l--)
						{
							if (lines[i][l] == '&')
							{
								strncpy(dataFileNumber, lines[i] + k + 1, l - (k + 1)); 
								break;
							}
						}
						// query 결과에 delimiter가 없으면(빈줄) 종료한다.
						if (tableSpaceName[0] == '\0')
						{
							strncpy(dataFileNumber, lines[i] + k + 1, l - (k + 1)); 
							break;
						}
						
						// query 결과에 tablespace size가 없으면 다음 줄을 검사한다.
						if (tableSpaceSize[0] == '\0')
						{
							continue;
						}
						
						
						// find the table space in the table space list
						for (k = 0; k < tableSpaceNumber; k++)
						{
							if (!strcmp(tableSpaceList[k].tableSpace, tableSpaceName))
							{
								break;
							}
						}
						// table space가 table space list에 없으면 추가한다.
						if (k == tableSpaceNumber)
						{
							// table space item을 하나 추가한다.
							if (k % DEFAULT_MEMORY_FRAGMENT == 0)
							{
								tableSpaceList = (struct tableSpaceItem *)realloc(tableSpaceList, sizeof(struct tableSpaceItem) * (k + DEFAULT_MEMORY_FRAGMENT));
								memset(tableSpaceList + k, 0, sizeof(struct tableSpaceItem) * DEFAULT_MEMORY_FRAGMENT);
							}
							
							// table space name을 기록한다.
							strcpy(tableSpaceList[k].tableSpace, tableSpaceName);
							// table space size를 기록한다.
							tableSpaceList[k].size = 0;
							for (j = 0; tableSpaceSize[j] >= '0'&& tableSpaceSize[j]<='9'; j++)
							{
								tableSpaceList[k].size = tableSpaceList[k].size * 10 + (tableSpaceSize[j] - '0');
							}	
							tableSpaceList[k].fileNumber2 = 0;
							for (l = 0; dataFileNumber[l] >= '0'&& dataFileNumber[l]<='9'; l++)
							{
								tableSpaceList[k].fileNumber2 = tableSpaceList[k].fileNumber2 * 10 + (dataFileNumber[l] - '0');
							}
							// table space list 개수를 증가시킨다.
							tableSpaceNumber++;

						}
					} //end less than 12 version
					else
					{
						memset(databaseName, 0, sizeof(databaseName));
						memset(dataFileName, 0, sizeof(dataFileName));
						
						// query 결과에서 tablespace 이름을 찾는다.
						for (j = 0; j < linesLength[i]; j++)
						{
							if (lines[i][j] == '^')
							{
								strncpy(databaseName, lines[i], j);	
								break;
							}
						}
						for (l = j + 1; l < linesLength[i]; l++)
						{
							if (lines[i][l] == '^')
							{
								strncpy(tableSpaceName, lines[i] + j + 1, l - (j + 1));
								break;
							}
						}
						// query 결과에서 datafile 이름을 찾는다.
						for (k = linesLength[i] - 1; k > l; k--)
						{
							if (lines[i][k] == '^')
							{
								strncpy(tableSpaceSize, lines[i] + l + 1, k - (l + 1));
								break;
							}
						}
					
						// query 결과에서 tablespace size를 찾는다.
					/*
						for (k = linesLength[i] - 1; k > j; k--)
						{
							if (lines[i][k] == '^')
							{
								strncpy(tableSpaceName, lines[i] + j + 1, k - (j + 1));
								break;
							}
						}
						for (l = linesLength[i] - 1; l > k ; l--)
						{
							if (lines[i][l] == '&')
							{
								strncpy(dataFileName, lines[i] + k + 1, l - (k + 1)); 
								break;
							}
						}
						*/
						// query 결과에 delimiter가 없으면(빈줄) 종료한다.
						if (tableSpaceName[0] == '\0')
						{
							break;
						}
						
						// query 결과에 tablespace size가 없으면 다음 줄을 검사한다.
						/*if (dataFileName[0] == '\0')
						{
							continue;
						}
						*/
						// find the table space in the table space list
						for (k = 0; k < tableSpaceNumber; k++)
						{
							if (!strcmp(tableSpaceList[k].tableSpace, tableSpaceName)&& !strcmp(tableSpaceList[k].database, databaseName))
							{
								break;
							}
						}
						// table space가 table space list에 없으면 추가한다.
						if (k == tableSpaceNumber)
						{
							// table space item을 하나 추가한다.
							if (k % DEFAULT_MEMORY_FRAGMENT == 0)
							{
								tableSpaceList = (struct tableSpaceItem *)realloc(tableSpaceList, sizeof(struct tableSpaceItem) * (k + DEFAULT_MEMORY_FRAGMENT));
								memset(tableSpaceList + k, 0, sizeof(struct tableSpaceItem) * DEFAULT_MEMORY_FRAGMENT);
							}

							// table space name을 기록한다.
							strcpy(tableSpaceList[k].database, databaseName);
							
							// database name을 기록한다.
							strcpy(tableSpaceList[k].tableSpace, tableSpaceName);

							// table space size를 기록한다.
							tableSpaceList[k].size = 0;
							for (j = 0; tableSpaceSize[j] >= '0'&& tableSpaceSize[j]<='9'; j++)
							{
								tableSpaceList[k].size = tableSpaceList[k].size * 10 + (tableSpaceSize[j] - '0');
							}
														
							// table space list 개수를 증가시킨다.
							tableSpaceNumber++;
						}
						
					}//end more than 12 version
					
				}
			}
		}
		
		for (i = 0; i < lineNumber; i++)
		{
			va_free(lines[i]);
		}
		va_free(lines);
		va_free(linesLength);
	}
	else
	{
		if(0 == debug_level)
		{
			va_remove(ABIOMASTER_CLIENT_FOLDER, outputFile);
		}
		
		return -1;
	}
	
	if(0 == debug_level)
	{
		va_remove(ABIOMASTER_CLIENT_FOLDER, outputFile);
	}


	
	return 0;
}

void GetOracleRmanRestoreBackupsetList()
{
	char reply[DSIZ][DSIZ];
	char msg[DSIZ * DSIZ];
	char backupTime[ORACLE_RMAN_UNTIL_TIME_LENGTH];

	struct oracleRmanBackupsetItem * oracleRmanBackupsetList;
	int oracleRmanBackupsetListNumber;
	
	va_sock_t commandSock;
	
	int i;

	oracleRmanBackupsetList = NULL;
	oracleRmanBackupsetListNumber = 0;
	
	
	
	if ((commandSock = va_connect(masterIP, commandData.catalogPort, 1)) != -1)
	{		
		va_make_time_stamp((time_t)atoi(commandData.backupset), backupTime, TIME_STAMP_TYPE_ORACLE_RMAN);

		if (ExcuteRmanRestoreBackupsetList(ORACLE_RMAN_QUERY_FILE_RESTORE_PREVIEW, commandData.backupFile, backupTime) == 0)
		{
			if ((oracleRmanBackupsetListNumber = ParsingOracleRmanBackupsetList(ABIOMASTER_CLIENT_FOLDER, outputFile, &oracleRmanBackupsetList)) < 0)
			{
				va_remove(ABIOMASTER_CLIENT_FOLDER, outputFile);
				
				va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
		
				return;
			}

			// send backupset list
			for (i = 0; i < oracleRmanBackupsetListNumber; i++)
			{
				// make reply message
				va_init_reply_buf(reply);
				
				strcpy(reply[0], "ORACLE_RMAN_RESTORE_BACKUPSET");
				strcpy(reply[150], oracleRmanBackupsetList[i].backupset);
				
				
				memset(msg, 0, sizeof(msg));
				va_make_reply_msg(reply, msg);
				
				if (va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE) < 0)
				{
					break;	
				}
			}
		}
		
		va_remove(ABIOMASTER_CLIENT_FOLDER, outputFile);
		va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
	}
}
void CreateCommandFile(char* filename, char* tablespace, char * container ,char* tag)
{
	char commandmsg[DSIZ*2];
	char databaseName[DSIZ];
	memset(commandmsg, 0 , sizeof(commandmsg));
	memset(databaseName, 0 , sizeof(databaseName));
	va_fd_t fd;	

	//blanc
	if(container == NULL)
	{
		sprintf(databaseName, "%s", "");
	}
	else
	{
		if(strlen(container) == 0)
			sprintf(databaseName, "%s", "");
		else
			sprintf(databaseName, "'%s':", container);
	}




	fd = va_open(ABIOMASTER_CLIENT_FOLDER_SCRIPT,filename, O_CREAT|O_RDWR,0,0);	
	if(strcmp(filename,"tablespace.sql") == 0)
	{
		sprintf(commandmsg,"spool &1;\nSELECT\n		Total.name || '^' ||\n	nvl(total_space-free_space, 0) || '^' \n 	FROM\n	(\n	SELECT\n		tablespace_name,\n		SUM(bytes) free_space\n	FROM\n		dba_free_space\n	GROUP BY\n		tablespace_name\n	) Free,\n	(\n	SELECT\n		b.name,\n		SUM(bytes) total_space\n	FROM\n		v$datafile a,\n		v$tablespace b\n	WHERE\n		a.ts# = b.ts#\n	GROUP BY\n		b.name\n	) Total\nWHERE\n	Free.tablespace_name(+)=Total.name;\nspool off;\nexit;\n");
	}
	//blanc
	else if(strcmp(filename,"tablespace12.sql") == 0)
	{
		sprintf(commandmsg,"spool &1;\nSELECT\n		Total.name || '^' ||\n	nvl(total_space-free_space, 0) || '^' \n 	FROM\n	(\n	SELECT\n		c.name||'^'||tablespace_name as name1,\n		SUM(bytes) free_space\n	FROM\n		cdb_free_space,\n		v$containers c\n	WHERE\n		cdb_free_space.con_id = c.con_id\n	GROUP BY\n		c.name||'^'||tablespace_name\n	) Free,\n	(\n	SELECT\n		c.name || '^' || a.name as name,\n		SUM(bytes) total_space\n	FROM\n		v$tablespace a,\n		v$datafile b,\n		v$containers c\n	WHERE\n		(c.con_id = b.con_id ) and (c.con_id = a.con_id) and (a.ts# = b.ts#)\n	GROUP BY\n		c.name || '^' || a.name\n	) Total\n	WHERE\n		Free.name1(+)=Total.name\n	ORDER BY\n		Total.name;\nspool off;\nexit;\n");
	}
	else if(strcmp(filename,"version.sql") == 0)
	{
		sprintf(commandmsg,"spool &1;\nSELECT\n		version\n 	FROM\n	v$instance;\nspool off;\nexit;\n");
	}
	#ifdef __ABIO_WIN		
	else if(strcmp(filename,"restorepreview.cmd") == 0)
	{
		sprintf(commandmsg,"RUN {\n ALLOCATE CHANNEL ch00 TYPE 'SBT_TAPE' PARMS='SBT_LIBRARY=%s%corasbt.dll';\n RESTORE TABLESPACE %s%s from TAG '%s' PREVIEW;\nRELEASE CHANNEL ch00;\n }",ABIOMASTER_CLIENT_FOLDER,FILE_PATH_DELIMITER,databaseName,tablespace,tag);
		//sprintf(commandmsg,"@setlocal ENABLEEXTENSIONS\n@set NLS_LANG=American\n@set NLS_DATE_FORMAT=YYYY-MM-DD:hh24:mi:ss\n@set ORACLE_HOME=%%1\n@set RMAN=%%ORACLE_HOME%%\\bin\\rman.exe\n@set ORACLE_SID=%%2\n@set ORACLE_ACCOUNT=%%3\n@set ORACLE_PASSWORD=%%4\n@set RESTORE_TABLESPACE_NAME=%%5\n@set RESTORE_TAG=%%6\n@set LOG_PATH=%%7\n@(\necho RUN {\necho ALLOCATE CHANNEL ch00 TYPE 'SBT_TAPE' PARMS='SBT_LIBRARY=%s%corasbt.dll';\necho RESTORE TABLESPACE %%RESTORE_TABLESPACE_NAME%% FROM TAG '%%RESTORE_TAG%%' PREVIEW;\necho RELEASE CHANNEL ch00;\necho }\n) | %%RMAN%% target %%ORACLE_ACCOUNT%%/%%ORACLE_PASSWORD%%@%%ORACLE_SID%% nocatalog log \"%%LOG_PATH%%\"\n",ABIOMASTER_CLIENT_FOLDER,FILE_PATH_DELIMITER);		
	}
	else if(strcmp(filename,"listbackup.cmd") == 0)
	{
		sprintf(commandmsg,"LIST BACKUP DEVICE TYPE 'SBT_TAPE';\n");
		//sprintf(commandmsg,"@setlocal ENABLEEXTENSIONS\n@set NLS_LANG=American\n@set NLS_DATE_FORMAT=YYYY-MM-DD:hh24:mi:ss\n@set ORACLE_HOME=%%1\n@set RMAN=%%ORACLE_HOME%%\\bin\\rman.exe\n@set ORACLE_SID=%%2\n@set ORACLE_ACCOUNT=%%3\n@set ORACLE_PASSWORD=%%4\n@set LOG_PATH=%%5\n@(\necho LIST BACKUP DEVICE TYPE 'SBT_TAPE';\n) | %%RMAN%% target %%ORACLE_ACCOUNT%%/%%ORACLE_PASSWORD%%@%%ORACLE_SID%% nocatalog log \"%%LOG_PATH%%\"\n");
	}
	else 
	{
		sprintf(commandmsg,"RUN {\n ALLOCATE CHANNEL ch00 TYPE 'SBT_TAPE' PARMS='SBT_LIBRARY=%s%corasbt.dll';\n DELETE FORCE NOPROMPT BACKUP TAG '%s';\nRELEASE CHANNEL ch00;\n }",ABIOMASTER_CLIENT_FOLDER,FILE_PATH_DELIMITER,tag);
		//sprintf(commandmsg,"@setlocal ENABLEEXTENSIONS\n@set NLS_LANG=American\n@set NLS_DATE_FORMAT=YYYY-MM-DD:hh24:mi:ss\n@set ORACLE_HOME=%%1\n@set RMAN=%%ORACLE_HOME%%\\bin\\rman.exe\n@set ORACLE_SID=%%2\n@set ORACLE_ACCOUNT=%%3\n@set ORACLE_PASSWORD=%%4\n@set BACKUPSET_ID=%%5\n@set LOG_PATH=%%6\n@(\necho RUN {\necho ALLOCATE CHANNEL ch00 TYPE 'SBT_TAPE' PARMS='SBT_LIBRARY=%s%corasbt.dll';\necho SEND CHANNEL 'ch00' \"JOB_TYPE=EXPIRE\";\necho DELETE FORCE NOPROMPT BACKUP TAG \"%%BACKUPSET_ID%%\";\necho RELEASE CHANNEL ch00;\necho }\n) | %%RMAN%% target %%ORACLE_ACCOUNT%%/%%ORACLE_PASSWORD%%@%%ORACLE_SID%% nocatalog log \"%%LOG_PATH%%\"\n",ABIOMASTER_CLIENT_FOLDER,FILE_PATH_DELIMITER);
	}	
	#else
	
	else if(strcmp(filename,"restorepreview.sh") == 0)
	{
		sprintf(commandmsg,"RUN {\n ALLOCATE CHANNEL ch00 TYPE 'SBT_TAPE' PARMS='SBT_LIBRARY=%s%clibobk.so';\n RESTORE TABLESPACE %s%s from TAG '%s' PREVIEW;\nRELEASE CHANNEL ch00;\n }",ABIOMASTER_CLIENT_FOLDER,FILE_PATH_DELIMITER,databaseName,tablespace,tag);
		//sprintf(commandmsg,"#!/bin/sh\nORACLE_HOME=$1\nRMAN=$ORACLE_HOME/bin/rman\nORACLE_SID=$2\nORACLE_ACCOUNT=$3\nORACLE_PASSWORD=$4\nRESTORE_TABLESPACE_NAME=$5\nRESTORE_TAG=$6\nLOG_PATH=$7\necho \"RUN {\nALLOCATE CHANNEL ch00 TYPE 'SBT_TAPE' PARMS='SBT_LIBRARY=%s/libobk.so';\nRESTORE TABLESPACE $RESTORE_TABLESPACE_NAME from tag \\\"$RESTORE_TAG\\\" preview;\nRELEASE CHANNEL ch00;\n}\" | $RMAN target $ORACLE_ACCOUNT/$ORACLE_PASSWORD@$ORACLE_SID nocatalog log \"$LOG_PATH\"\n",ABIOMASTER_CLIENT_FOLDER);	
	}
	else if(strcmp(filename,"listbackup.sh") == 0)
	{
		sprintf(commandmsg,"LIST BACKUP DEVICE TYPE 'SBT_TAPE';\n");
		//sprintf(commandmsg,"#!/bin/sh\nORACLE_HOME=$1\nRMAN=$ORACLE_HOME/bin/rman\nORACLE_SID=$2\nORACLE_ACCOUNT=$3\nORACLE_PASSWORD=$4\nLOG_PATH=$5\necho \"LIST BACKUP DEVICE TYPE 'SBT_TAPE' PARMS='SBT_LIBRARY=/usr/abio_bk/abio/db/oracle_rman/libobk.so';\" | $RMAN target $ORACLE_ACCOUNT/$ORACLE_PASSWORD@$ORACLE_SID nocatalog log \"$LOG_PATH\"\n",ABIOMASTER_CLIENT_FOLDER);			
	}
	else 
	{
		sprintf(commandmsg,"RUN {\n ALLOCATE CHANNEL ch00 TYPE 'SBT_TAPE' PARMS='SBT_LIBRARY=%s%clibobk.so';\n DELETE FORCE NOPROMPT BACKUP TAG '%s';\nRELEASE CHANNEL ch00;\n }",ABIOMASTER_CLIENT_FOLDER,FILE_PATH_DELIMITER,tag);
		//sprintf(commandmsg,"#!/bin/sh\nORACLE_HOME=$1\nRMAN=$ORACLE_HOME/bin/rman\nORACLE_SID=$2\nORACLE_ACCOUNT=$3\nORACLE_PASSWORD=$4\nBACKUPSET_ID=$5\nLOG_PATH=$6\necho \"RUN {\nALLOCATE CHANNEL ch00 TYPE 'SBT_TAPE' PARMS='SBT_LIBRARY=%s/libobk.so';\nSEND CHANNEL 'ch00' \\\"JOB_TYPE=EXPIRE\\\";\nDELETE FORCE NOPROMPT BACKUP TAG \\\"$BACKUPSET_ID\\\";\nRELEASE CHANNEL ch00;\n}\" | $RMAN target $ORACLE_ACCOUNT/$ORACLE_PASSWORD@$ORACLE_SID nocatalog log \"$LOG_PATH\"\n",ABIOMASTER_CLIENT_FOLDER);			
	}
		
	#endif
	va_write(fd,commandmsg,(int)strlen(commandmsg),DATA_TYPE_NOT_CHANGE);	
	va_close(fd);
}
int ExcuteRmanArchivePreview(char * queryFileName, char* tablespacename, char * containerName, char * backupsetID)
{
	char argQueryFile[MAX_PATH_LENGTH];
	char argOutputFile[MAX_PATH_LENGTH];
	char argQueryInfo[MAX_PATH_LENGTH];

	char username[ORACLE_RMAN_USERNAME_LENGTH];
	char account[ORACLE_RMAN_ACCOUNT_LENGTH];
	char passwd[ORACLE_RMAN_PASSWORD_LENGTH];
	char sid[ORACLE_RMAN_SID_LENGTH];
	char home[ORACLE_RMAN_HOME_LENGTH];

	char db_home[ORACLE_RMAN_HOME_LENGTH];
	char command_path[MAX_PATH_LENGTH];
	char rmanconnect[DSIZ];
	char log_command[MAX_PATH_LENGTH];
	char excutecommand[MAX_PATH_LENGTH];
	int r;
	
	memset(argQueryFile, 0, sizeof(argQueryFile));
	memset(argOutputFile, 0, sizeof(argOutputFile));
	memset(argQueryInfo, 0, sizeof(argQueryInfo));
	memset(command_path, 0, sizeof(command_path));
	memset(db_home, 0, sizeof(db_home));
	memset(rmanconnect, 0, sizeof(rmanconnect));
	memset(log_command, 0, sizeof(log_command));
	memset(excutecommand, 0, sizeof(excutecommand));

	strcpy(username, ((struct oracleRmanDB *)commandData.db.dbData)->username);
	strcpy(account, ((struct oracleRmanDB *)commandData.db.dbData)->account);
	strcpy(passwd, ((struct oracleRmanDB *)commandData.db.dbData)->passwd);
	strcpy(sid, ((struct oracleRmanDB *)commandData.db.dbData)->sid);
	strcpy(home, ((struct oracleRmanDB *)commandData.db.dbData)->home);
	#ifdef __ABIO_WIN		
	if((int)va_open(ABIOMASTER_CLIENT_FOLDER_SCRIPT,"execute.cmd",O_RDONLY,0,0) == -1)
	{
		CreateExecuteFile("execute.cmd");
	}

	#else
	if((int)va_open(ABIOMASTER_CLIENT_FOLDER_SCRIPT,"execute.sh",O_RDONLY,0,0) == -1)
	{
		CreateExecuteFile("execute.sh");
	}
	
	#endif
	
	// query를 실행한다.
	
	sprintf(argOutputFile, "%s%c%s", ABIOMASTER_CLIENT_FOLDER, FILE_PATH_DELIMITER, outputFile);
	

	#ifdef __ABIO_WIN		
		va_slash_to_backslash(home);	
		sprintf(db_home,"%s%cbin%crman.exe",home,FILE_PATH_DELIMITER,FILE_PATH_DELIMITER);
		sprintf(rmanconnect,"%s target %s/%s@%s nocatalog cmdfile ",db_home,account,passwd,sid);
		sprintf(log_command," log %s",argOutputFile);
		va_remove(ABIOMASTER_CLIENT_FOLDER_SCRIPT,"restorepreview.cmd");			
		CreateCommandFile("restorepreview.cmd",tablespacename,containerName,backupsetID);	
		sprintf(command_path,"%srestorepreview.cmd",ABIOMASTER_CLIENT_FOLDER_SCRIPT);			
		sprintf(excutecommand,"%sexecute.cmd",ABIOMASTER_CLIENT_FOLDER_SCRIPT);
		r = va_spawn_process(WAIT_PROCESS, ORACLE_RMAN_QUERY_FILE_SHELL,"/C",excutecommand, home, sid, account, passwd, command_path, argOutputFile, NULL);
		//r = va_spawn_process(WAIT_PROCESS, ORACLE_RMAN_QUERY_FILE_SHELL,"/C",excutecommand,NULL);	
		
	#else
		sprintf(db_home,"%s%cbin%crman",home,FILE_PATH_DELIMITER,FILE_PATH_DELIMITER);
		sprintf(rmanconnect,"%s target %s/%s@%s nocatalog cmdfile ",db_home,account,passwd,sid);
		sprintf(log_command," log %s",argOutputFile);
		va_remove(ABIOMASTER_CLIENT_FOLDER_SCRIPT,"restorepreview.sh");			
		CreateCommandFile("restorepreview.sh",tablespacename,containerName,backupsetID);	
		sprintf(command_path,"%srestorepreview.sh",ABIOMASTER_CLIENT_FOLDER_SCRIPT);		
		
		sprintf(excutecommand,"%sexecute.sh",ABIOMASTER_CLIENT_FOLDER_SCRIPT);
		r = va_spawn_process(WAIT_PROCESS, username, ORACLE_RMAN_QUERY_FILE_SHELL,excutecommand, home, sid, account, passwd, command_path, argOutputFile, NULL);				
		//r = va_spawn_process(WAIT_PROCESS, username, ORACLE_RMAN_QUERY_FILE_SHELL,excutecommand,NULL);		
		
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
int ExcuteRmanRestoreBackupsetList(char * queryFileName, char * tablespace, char * untilTime)
{
	char argQueryFile[MAX_PATH_LENGTH];
	char argOutputFile[MAX_PATH_LENGTH];
	char argQueryInfo[MAX_PATH_LENGTH];

	char username[ORACLE_RMAN_USERNAME_LENGTH];
	char account[ORACLE_RMAN_ACCOUNT_LENGTH];
	char passwd[ORACLE_RMAN_PASSWORD_LENGTH];
	char sid[ORACLE_RMAN_SID_LENGTH];
	char home[ORACLE_RMAN_HOME_LENGTH];

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
		sprintf(argQueryFile, "%s", queryFileName);
		sprintf(argQueryInfo, "%s/%s", account, passwd);
		va_slash_to_backslash(home);

		r = va_spawn_process(WAIT_PROCESS, ORACLE_RMAN_QUERY_FILE_SHELL, "/C", argQueryFile, home, sid, account, passwd, tablespace, untilTime, argOutputFile, NULL);
	#else
		sprintf(argQueryFile, "%s", queryFileName);
		sprintf(argQueryInfo, "%s/%s", account, passwd);
		
		r = va_spawn_process(WAIT_PROCESS, username, ORACLE_RMAN_QUERY_FILE_SHELL, argQueryFile, home, sid, account, passwd, tablespace, untilTime, argOutputFile, NULL);
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

void SyncOracleRmanBackupsetList()
{
	char msg[DSIZ * DSIZ];
	char replyMsg[DSIZ][DSIZ];
	char * recvMsg;

	struct oracleRmanBackupsetItem * abioBackupsetList;
	int abioBackupsetListNumber;

	struct oracleRmanBackupsetItem * oracleRmanBackupsetList;
	int oracleRmanBackupsetListNumber;

	struct oracleRmanBackupsetItem * expireBackupsetList;
	int expireBackupsetListNumber;

	struct oracleRmanBackupsetItem * alertBackupsetList;
	int alertBackupsetListNumber;
	
	va_sock_t commandSock;
	
	int recvSize;						

	int i;
	int j;
	int k;
	int l;
	int backupsetCount;

	int recvError;
	int queryError;
	int parseError;

	abioBackupsetList = NULL;
	abioBackupsetListNumber = 0;

	oracleRmanBackupsetList = NULL;
	oracleRmanBackupsetListNumber = 0;

	expireBackupsetList = NULL;
	expireBackupsetListNumber = 0;
	
	alertBackupsetList = NULL;
	alertBackupsetListNumber = 0;

	recvError = 0;
	queryError = 0;
	parseError = 0;
	backupsetCount = 0;

	if ((commandSock = va_connect(masterIP, commandData.catalogPort, 1)) != -1)
	{
		memset(msg, 0, sizeof(msg));
		sprintf(msg, "SYNC_ORACLE_RMAN_BACKUPSET_LIST_READY\\ready\\");
		va_send(commandSock, msg, sizeof(msg), 0, DATA_TYPE_NOT_CHANGE);
		memset(msg, 0x00, sizeof(msg));
		va_recv(commandSock, msg, 10, 0 ,DATA_TYPE_NOT_CHANGE);
		backupsetCount = atoi(msg);
		memset(msg, 0x00, sizeof(msg));		
		l = 0;
		while ((recvSize = va_recv(commandSock, msg,BACKUPSET_ID_LENGTH, 0, DATA_TYPE_NOT_CHANGE)) > 0)
		{
			for (i = 0; i < recvSize; i++)
			{				
				recvMsg = msg + i;	
				
				if (recvMsg[0] != '\0')
				{
					if (strncmp(recvMsg, "<SYNC_ORACLE_RMAN_BACKUPSET_LIST_END>", strlen("<SYNC_ORACLE_RMAN_BACKUPSET_LIST_END>")) == 0)
					{
						recvError = 1;
						break;
					}

					if (abioBackupsetListNumber % DEFAULT_MEMORY_FRAGMENT == 0)
					{
						abioBackupsetList = (struct oracleRmanBackupsetItem *)realloc(abioBackupsetList, sizeof(abioBackupsetList[0]) * (abioBackupsetListNumber + DEFAULT_MEMORY_FRAGMENT));
						memset(abioBackupsetList + abioBackupsetListNumber, 0, sizeof(abioBackupsetList[0]) * DEFAULT_MEMORY_FRAGMENT);
					}
					strcpy(abioBackupsetList[abioBackupsetListNumber].backupset, recvMsg);
					abioBackupsetListNumber++;

					i += (int)strlen(recvMsg);
				}
			}
			l++;
			if (recvError != 0 || GET_ERROR(commandData.errorCode))
			{
				break;
			}

			if ( l == backupsetCount)
			{
				break;
			}
		}
	}	
	else
	{
		// Error

		return;
	}
	// Server 로부터 Backupset List 를 모두 전송 받았으면 Oracle RMAN 의 Backuspet List 를 받아와 비교한다.
	if (recvError == 0)
	{		
		if (ExcuteRmanBackupsetList(ORACLE_RMAN_QUERY_FILE_LIST_BACKUP) < 0)
		{
			queryError = 1;
		}
		else if ((oracleRmanBackupsetListNumber = ParsingOracleRmanBackupsetList(ABIOMASTER_CLIENT_FOLDER, outputFile, &oracleRmanBackupsetList)) < 0)
		{
			parseError = 1;
		}
		va_remove(ABIOMASTER_CLIENT_FOLDER, outputFile);

		if (queryError == 0 && parseError == 0)
		{
			// ABIO Backupset List 와 RMAN Backupset List 를 비교한다.
			qsort(abioBackupsetList, abioBackupsetListNumber, sizeof(abioBackupsetList[0]), ComparatorRmanBackupsetDescending);
			qsort(oracleRmanBackupsetList, oracleRmanBackupsetListNumber, sizeof(oracleRmanBackupsetList[0]), ComparatorRmanBackupsetDescending);
			
			i = 0;
			j = 0;
			while (i < abioBackupsetListNumber && j < oracleRmanBackupsetListNumber)
			{
				k = strcmp(abioBackupsetList[i].backupset, oracleRmanBackupsetList[j].backupset);	
				if (k < 0)
				{
					if (alertBackupsetListNumber % DEFAULT_MEMORY_FRAGMENT == 0)
					{
						alertBackupsetList = (struct oracleRmanBackupsetItem *)realloc(alertBackupsetList, sizeof(alertBackupsetList[0]) * (alertBackupsetListNumber + DEFAULT_MEMORY_FRAGMENT));
						memset(alertBackupsetList + alertBackupsetListNumber, 0, sizeof(alertBackupsetList[0]) * DEFAULT_MEMORY_FRAGMENT);
					}
					
					strcpy(alertBackupsetList[alertBackupsetListNumber].backupset, abioBackupsetList[i].backupset);
					alertBackupsetListNumber++;
					
					i++;
				}
				else if (k > 0)
				{
					if (expireBackupsetListNumber % DEFAULT_MEMORY_FRAGMENT == 0)
					{
						expireBackupsetList = (struct oracleRmanBackupsetItem *)realloc(expireBackupsetList, sizeof(expireBackupsetList[0]) * (expireBackupsetListNumber + DEFAULT_MEMORY_FRAGMENT));
						memset(expireBackupsetList + expireBackupsetListNumber, 0, sizeof(expireBackupsetList[0]) * DEFAULT_MEMORY_FRAGMENT);
					}
					
					strcpy(expireBackupsetList[expireBackupsetListNumber].backupset, oracleRmanBackupsetList[j].backupset);
					expireBackupsetListNumber++;
					
					j++;
				}
				else
				{
					i++;
					j++;
				}
			}
			
			if (i < abioBackupsetListNumber)
			{
				while (i < abioBackupsetListNumber)
				{
					if (alertBackupsetListNumber % DEFAULT_MEMORY_FRAGMENT == 0)
					{
						alertBackupsetList = (struct oracleRmanBackupsetItem *)realloc(alertBackupsetList, sizeof(alertBackupsetList[0]) * (alertBackupsetListNumber + DEFAULT_MEMORY_FRAGMENT));
						memset(alertBackupsetList + alertBackupsetListNumber, 0, sizeof(alertBackupsetList[0]) * DEFAULT_MEMORY_FRAGMENT);
					}
					
					strcpy(alertBackupsetList[alertBackupsetListNumber].backupset, abioBackupsetList[i].backupset);
					alertBackupsetListNumber++;
					
					i++;
				}
			}
			
			if (j < oracleRmanBackupsetListNumber)
			{
				while (j < oracleRmanBackupsetListNumber)
				{
					if (expireBackupsetListNumber % DEFAULT_MEMORY_FRAGMENT == 0)
					{
						expireBackupsetList = (struct oracleRmanBackupsetItem *)realloc(expireBackupsetList, sizeof(expireBackupsetList[0]) * (expireBackupsetListNumber + DEFAULT_MEMORY_FRAGMENT));
						memset(expireBackupsetList + expireBackupsetListNumber, 0, sizeof(expireBackupsetList[0]) * DEFAULT_MEMORY_FRAGMENT);
					}
					
					strcpy(expireBackupsetList[expireBackupsetListNumber].backupset, oracleRmanBackupsetList[j].backupset);
					expireBackupsetListNumber++;
					
					j++;
				}
			}
		}
	}

	// Sync 의 결과를 처리한다.
	if (recvError == 0 && queryError == 0 && parseError == 0)
	{
		// Backupset 을 비교한 결과를 처리한다.
		memset(msg, 0, sizeof(msg));
		sprintf(msg, "SYNC_ORACLE_RMAN_BACKUPSET_LIST_START\\start\\");
		va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE);

		for (j=0; j<expireBackupsetListNumber; j++)
		{
			if (ExcuteRmanExpireBackupset(ORACLE_RMAN_QUERY_FILE_EXPIRE, expireBackupsetList[j].backupset) != 0)
			{
				va_init_reply_buf(replyMsg);

				strcpy(replyMsg[0], "SYNC_ORACLE_RMAN_BACKUPSET_LIST_END");
				strcpy(replyMsg[1], "fail");
				strcpy(replyMsg[150], expireBackupsetList[i].backupset);
				
				va_make_reply_msg(replyMsg,msg);
				va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE);

				//break;
			}
			else
			{
				memset(msg, 0, sizeof(msg));
				sprintf(msg, "SYNC_ORACLE_RMAN_BACKUPSET_LIST_END\\fail_expire\\");
				va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE);

			}
		}

		memset(msg, 0, sizeof(msg));
		sprintf(msg, "SYNC_ORACLE_RMAN_BACKUPSET_LIST_END\\ok\\");

		va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE);

	}
	else
	{
		memset(msg, 0, sizeof(msg));
		sprintf(msg, "<SYNC_ORACLE_RMAN_BACKUPSET_LIST>\\invalid_format\\");
		va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE);
	}


	va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);

	va_free(abioBackupsetList);
	va_free(oracleRmanBackupsetList);
	va_free(expireBackupsetList);
	va_free(alertBackupsetList);
}

void CreateExecuteFile(char* file)
{
	char commandmsg[DSIZ*2];
	memset(commandmsg, 0 , sizeof(commandmsg));
	va_fd_t fd;	
	fd = va_open(ABIOMASTER_CLIENT_FOLDER_SCRIPT, file, O_CREAT|O_RDWR,0,0);	
	#ifdef __ABIO_WIN 
	sprintf(commandmsg,"@setlocal ENABLEEXTENSIONS\n@set NLS_LANG=American\n@set NLS_DATE_FORMAT=YYYY-MM-DD:hh24:mi:ss\n@set ORACLE_HOME=%%1\n@set RMAN=%%ORACLE_HOME%%\\bin\\rman.exe\n@set ORACLE_SID=%%2\n@set ORACLE_ACCOUNT=%%3\n@set ORACLE_PASSWORD=%%4\n@set ORACLE_TYPE=%%5\n@set LOG_PATH=%%6\n%%RMAN%% target %%ORACLE_ACCOUNT%%/%%ORACLE_PASSWORD%%@%%ORACLE_SID%% nocatalog cmdfile %%ORACLE_TYPE%% log %%LOG_PATH%%\n");
	#else
	sprintf(commandmsg,"#!/bin/sh\nNLS_LANG=ENGLISH\nexport NLS_LANG\nNLS_DATE_FORMAT=YYYY-MM-DD:hh24:mi:ss\nexport NLS_DATE_FORMAT\nORACLE_HOME=$1\nRMAN=$ORACLE_HOME/bin/rman\nORACLE_SID=$2\nORACLE_ACCOUNT=$3\nORACLE_PASSWORD=$4\nORACLE_TYPE=$5\nLOG_PATH=$6\n$RMAN target $ORACLE_ACCOUNT/$ORACLE_PASSWORD@$ORACLE_SID nocatalog cmdfile \"$ORACLE_TYPE\" log \"$LOG_PATH\"");
	#endif
	va_write(fd,commandmsg,(int)strlen(commandmsg),DATA_TYPE_NOT_CHANGE);	
	va_close(fd);
}
int ExcuteRmanBackupsetList(char * queryFileName)
{
	char argQueryFile[MAX_PATH_LENGTH];
	char argOutputFile[MAX_PATH_LENGTH];
	char argQueryInfo[MAX_PATH_LENGTH];

	char username[ORACLE_RMAN_USERNAME_LENGTH];
	char account[ORACLE_RMAN_ACCOUNT_LENGTH];
	char passwd[ORACLE_RMAN_PASSWORD_LENGTH];
	char sid[ORACLE_RMAN_SID_LENGTH];
	char home[ORACLE_RMAN_HOME_LENGTH];

	
	char db_home[ORACLE_RMAN_HOME_LENGTH];
	char command_path[MAX_PATH_LENGTH];
	char rmanconnect[DSIZ];
	char log_command[MAX_PATH_LENGTH];
	char excutecommand[MAX_PATH_LENGTH];
	int r;
	
	memset(argQueryFile, 0, sizeof(argQueryFile));
	memset(argOutputFile, 0, sizeof(argOutputFile));
	memset(argQueryInfo, 0, sizeof(argQueryInfo));
	memset(command_path, 0, sizeof(command_path));
	memset(db_home, 0, sizeof(db_home));
	memset(rmanconnect, 0, sizeof(rmanconnect));
	memset(log_command, 0, sizeof(log_command));
	memset(excutecommand, 0, sizeof(excutecommand));

	strcpy(username, ((struct oracleRmanDB *)commandData.db.dbData)->username);
	strcpy(account, ((struct oracleRmanDB *)commandData.db.dbData)->account);
	strcpy(passwd, ((struct oracleRmanDB *)commandData.db.dbData)->passwd);
	strcpy(sid, ((struct oracleRmanDB *)commandData.db.dbData)->sid);
	strcpy(home, ((struct oracleRmanDB *)commandData.db.dbData)->home);
	
	#ifdef __ABIO_WIN		
	if((int)va_open(ABIOMASTER_CLIENT_FOLDER_SCRIPT,"execute.cmd",O_RDONLY,0,0) == -1)
	{
		CreateExecuteFile("execute.cmd");
	}

	#else
	if((int)va_open(ABIOMASTER_CLIENT_FOLDER_SCRIPT,"execute.sh",O_RDONLY,0,0) == -1)
	{
		CreateExecuteFile("execute.sh");
	}
	
	#endif
	

	// query를 실행한다.
	sprintf(argOutputFile, "%s%c%s", ABIOMASTER_CLIENT_FOLDER, FILE_PATH_DELIMITER, outputFile);

	#ifdef __ABIO_WIN
		va_slash_to_backslash(home);
		sprintf(db_home,"%s%cbin%crman.exe",home,FILE_PATH_DELIMITER,FILE_PATH_DELIMITER);
		sprintf(rmanconnect,"%s target %s/%s@%s nocatalog cmdfile ",db_home,account,passwd,sid);
		sprintf(log_command," log \"%s\"",argOutputFile);
		va_remove(ABIOMASTER_CLIENT_FOLDER_SCRIPT,"listbackup.cmd");			
		CreateCommandFile("listbackup.cmd",NULL,NULL,NULL);	
		sprintf(command_path,"%slistbackup.cmd",ABIOMASTER_CLIENT_FOLDER_SCRIPT);		
		sprintf(excutecommand,"%sexecute.cmd",ABIOMASTER_CLIENT_FOLDER_SCRIPT);
		r = va_spawn_process(WAIT_PROCESS, ORACLE_RMAN_QUERY_FILE_SHELL,"/C",excutecommand, home, sid, account, passwd, command_path, argOutputFile, NULL);
		//r = va_spawn_process(WAIT_PROCESS, ORACLE_RMAN_QUERY_FILE_SHELL,"/C",excutecommand,NULL);
		
	#else
		sprintf(db_home,"%s%cbin%crman",home,FILE_PATH_DELIMITER,FILE_PATH_DELIMITER);
		sprintf(rmanconnect,"%s target %s/%s@%s nocatalog cmdfile ",db_home,account,passwd,sid);
		sprintf(log_command," log \"%s\"",argOutputFile);
		va_remove(ABIOMASTER_CLIENT_FOLDER_SCRIPT,"listbackup.sh");			
		CreateCommandFile("listbackup.sh",NULL,NULL,NULL);	
		sprintf(command_path,"%slistbackup.sh",ABIOMASTER_CLIENT_FOLDER_SCRIPT);		
		sprintf(excutecommand,"%sexecute.sh",ABIOMASTER_CLIENT_FOLDER_SCRIPT);
		r = va_spawn_process(WAIT_PROCESS, username, ORACLE_RMAN_QUERY_FILE_SHELL,excutecommand, home, sid, account, passwd, command_path, argOutputFile, NULL);				
	//	r = va_spawn_process(WAIT_PROCESS, username, ORACLE_RMAN_QUERY_FILE_SHELL,excutecommand,NULL);
	
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

int ParsingOracleRmanBackupsetList(char * outputFilePath, char * outputFileName, struct oracleRmanBackupsetItem ** ptrBackupsetList)
{
	char ** lines;
	int * linesLength;
	int lineNumber;
	
	struct oracleRmanBackupsetItem * rmanBackupsetList;
	int rmanBackupsetListNumber;
	
	int i;
	int j;
	int k;
	int l;
	
	// initialize variables
	rmanBackupsetList = NULL;
	rmanBackupsetListNumber = 0;	
	if ((lineNumber = va_load_text_file_lines(outputFilePath, outputFileName, &lines, &linesLength)) > 0)
	{
		for (i = 0; i < lineNumber; i++)
		{
			if (lines[i][0] == '-')
			{
				if (i + 3 >= lineNumber)
				{
					break;
				}
				
				if (rmanBackupsetListNumber % DEFAULT_MEMORY_FRAGMENT == 0)
				{
					rmanBackupsetList = (struct oracleRmanBackupsetItem *)realloc(rmanBackupsetList, sizeof(struct oracleRmanBackupsetItem) * (rmanBackupsetListNumber + DEFAULT_MEMORY_FRAGMENT));
					memset(rmanBackupsetList + rmanBackupsetListNumber, 0, sizeof(struct oracleRmanBackupsetItem) * DEFAULT_MEMORY_FRAGMENT);
				}
				
				for (j = 0; j < linesLength[i + 2]; j++)
				{
					char tempbackupset[256];
					int tempnum=0;
					memset(tempbackupset,0,256);
					if (lines[i + 2][j] == 'T')
					{
						j += 5;
						for (k = j; linesLength[i + 2]; k++)
						{
							if (lines[i + 2][k] == ' ')
							{	
								strncpy(tempbackupset, lines[i + 2] + j, k - j);
								for(l = 0; l < rmanBackupsetListNumber; l++)
								{
									if(strcmp(rmanBackupsetList[l].backupset,tempbackupset) == 0)
									{										
										tempnum=1;
										break;
									}
								}
								if(rmanBackupsetListNumber == 0 || tempnum == 0)
								{
									strcpy(rmanBackupsetList[rmanBackupsetListNumber].backupset, tempbackupset);				
									rmanBackupsetListNumber++;
								}
								break;
							}
						}						
						break;
					}
				}					
			}
		}
		
		
		for (i = 0; i < lineNumber; i++)
		{
			va_free(lines[i]);
		}
		va_free(lines);
		va_free(linesLength);
	}
	else
	{
		// Error
	}
	
	*ptrBackupsetList = rmanBackupsetList;
	return rmanBackupsetListNumber;
}

void ExpireOracleRmanBackupset()
{
	char msg[DSIZ * DSIZ];
	
	va_sock_t commandSock;
	
	char argOutputFile[MAX_PATH_LENGTH];

	memset(argOutputFile, 0, sizeof(argOutputFile));


	if ((commandSock = va_connect(masterIP, commandData.catalogPort, 1)) != -1)
	{
		sprintf(argOutputFile, "%s%c%s", ABIOMASTER_CLIENT_FOLDER, FILE_PATH_DELIMITER, outputFile);

		if (ExcuteRmanExpireBackupset(ORACLE_RMAN_QUERY_FILE_EXPIRE, commandData.backupset) == 0)
		{
			memset(msg, 0, sizeof(msg));
			sprintf(msg, "EXPIRE_ORACLE_RMAN_BACKUPSET_END\\ok\\");
			va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE);
		}
		else
		{
			memset(msg, 0, sizeof(msg));
			sprintf(msg, "EXPIRE_ORACLE_RMAN_BACKUPSET_END\\fail\\");
			va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE);
		}

		va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
	}
}

void ExpireOracleRmanBackupsetSilent()
{
	char argOutputFile[MAX_PATH_LENGTH];

	memset(argOutputFile, 0, sizeof(argOutputFile));


	sprintf(argOutputFile, "%s%c%s", ABIOMASTER_CLIENT_FOLDER, FILE_PATH_DELIMITER, outputFile);

	if (ExcuteRmanExpireBackupset(ORACLE_RMAN_QUERY_FILE_EXPIRE, commandData.backupset) == 0)
	{


	}
	//SyncOracleRmanBackupsetList();
}

int ExcuteRmanExpireBackupset(char * queryFileName, char * backupsetID)
{
	char argQueryFile[MAX_PATH_LENGTH];
	char argOutputFile[MAX_PATH_LENGTH];
	char argQueryInfo[MAX_PATH_LENGTH];

	char username[ORACLE_RMAN_USERNAME_LENGTH];
	char account[ORACLE_RMAN_ACCOUNT_LENGTH];
	char passwd[ORACLE_RMAN_PASSWORD_LENGTH];
	char sid[ORACLE_RMAN_SID_LENGTH];
	char home[ORACLE_RMAN_HOME_LENGTH];

	char db_home[ORACLE_RMAN_HOME_LENGTH];
	char command_path[MAX_PATH_LENGTH];
	char rmanconnect[DSIZ];
	char log_command[MAX_PATH_LENGTH];
	char excutecommandpath[MAX_PATH_LENGTH];
	char excutecommandfile[MAX_PATH_LENGTH];
	char commandfile[MAX_PATH_LENGTH];
	int r;
	
	memset(argQueryFile, 0, sizeof(argQueryFile));
	memset(argOutputFile, 0, sizeof(argOutputFile));
	memset(argQueryInfo, 0, sizeof(argQueryInfo));
	memset(command_path, 0, sizeof(command_path));
	memset(db_home, 0, sizeof(db_home));
	memset(rmanconnect, 0, sizeof(rmanconnect));
	memset(log_command, 0, sizeof(log_command));
	memset(excutecommandpath, 0, sizeof(excutecommandpath));
	memset(excutecommandfile, 0, sizeof(excutecommandfile));

	memset(commandfile, 0, sizeof(commandfile));

	strcpy(username, ((struct oracleRmanDB *)commandData.db.dbData)->username);
	strcpy(account, ((struct oracleRmanDB *)commandData.db.dbData)->account);
	strcpy(passwd, ((struct oracleRmanDB *)commandData.db.dbData)->passwd);
	strcpy(sid, ((struct oracleRmanDB *)commandData.db.dbData)->sid);
	strcpy(home, ((struct oracleRmanDB *)commandData.db.dbData)->home);
	
	// query를 실행한다.
	sprintf(argOutputFile, "%s%c%s", ABIOMASTER_CLIENT_FOLDER, FILE_PATH_DELIMITER, outputFile);
	

	#ifdef __ABIO_WIN
		
		va_slash_to_backslash(home);			
		sprintf(db_home,"%s%cbin%crman.exe",home,FILE_PATH_DELIMITER,FILE_PATH_DELIMITER);
		sprintf(rmanconnect,"%s target %s/%s@%s nocatalog cmdfile ",db_home,account,passwd,sid);
		sprintf(log_command," log %s",argOutputFile);
		sprintf(commandfile,"%s.cmd",backupsetID);
		CreateCommandFile(commandfile,NULL,NULL,backupsetID);
		sprintf(command_path,"%s%s",ABIOMASTER_CLIENT_FOLDER_SCRIPT,commandfile);
		sprintf(excutecommandfile,"expire_%s.cmd",backupsetID);
		CreateExecuteFile(excutecommandfile);
		sprintf(excutecommandpath,"%s%s",ABIOMASTER_CLIENT_FOLDER_SCRIPT,excutecommandfile);
		r = va_spawn_process(WAIT_PROCESS, ORACLE_RMAN_QUERY_FILE_SHELL,"/C",excutecommandpath, home, sid, account, passwd, command_path, argOutputFile, NULL);
		//r = va_spawn_process(WAIT_PROCESS, ORACLE_RMAN_QUERY_FILE_SHELL,"/C",excutecommandpath,NULL);
	#else
		sprintf(db_home,"%s%cbin%crman",home,FILE_PATH_DELIMITER,FILE_PATH_DELIMITER);
		sprintf(rmanconnect,"%s target %s/%s@%s nocatalog cmdfile ",db_home,account,passwd,sid);
		sprintf(log_command," log %s",argOutputFile);
		sprintf(commandfile,"%s.sh",backupsetID);
		CreateCommandFile(commandfile,NULL,NULL,backupsetID);
		sprintf(command_path,"%s%s",ABIOMASTER_CLIENT_FOLDER_SCRIPT,commandfile);
		sprintf(excutecommandfile,"expire_%s.sh",backupsetID);
		CreateExecuteFile(excutecommandfile);
		sprintf(excutecommandpath,"%s%s",ABIOMASTER_CLIENT_FOLDER_SCRIPT,excutecommandfile);
		r = va_spawn_process(WAIT_PROCESS, username, ORACLE_RMAN_QUERY_FILE_SHELL,excutecommandpath, home, sid, account, passwd, command_path, argOutputFile, NULL);				
		//r = va_spawn_process(WAIT_PROCESS, username, ORACLE_RMAN_QUERY_FILE_SHELL,excutecommandpath,NULL);
		

	#endif
	va_remove(ABIOMASTER_CLIENT_FOLDER_SCRIPT,excutecommandfile);
	va_remove(ABIOMASTER_CLIENT_FOLDER_SCRIPT,commandfile);
	va_remove(ABIOMASTER_CLIENT_FOLDER, outputFile);
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

int ExecuteTablespaceSql(char * queryFileName, int version)
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
	
	//sprintf(argQueryFile, "@%s", queryFileName);

	if (12 > version)
	{
		sprintf(argQueryFile, "@%s", queryFileName);
		if((int)va_open(ABIOMASTER_CLIENT_FOLDER_SCRIPT,"tablespace.sql",O_RDONLY,0,0) == -1)
		{
	 		CreateCommandFile("tablespace.sql",NULL,NULL,NULL);	
		}
	}
	else
	{
		sprintf(argQueryFile, "@%s", ORACLE_RMAN_QUERY_FILE_GET_TABLESPACE_LIST12);
		//sprintf(argQueryFile, "@%s", "/blanc/test2/abio_bk/abio/db/oracle_rman/scripts/tablespace12.sql");
		if((int)va_open(ABIOMASTER_CLIENT_FOLDER_SCRIPT,"tablespace12.sql",O_RDONLY,0,0) == -1)
		{
	 		CreateCommandFile("tablespace12.sql",NULL,NULL,NULL);	
		}
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
	 	CreateCommandFile("version.sql",NULL,NULL,NULL);	
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

void Exit()
{
	int i;
	int j;
	
	
	
	for (i = 0; i < backupLogDescriptionNumber; i++)
	{
		va_free(backupLogDescription[i].description);
	}
	va_free(backupLogDescription);
	
	for (i = 0; i < tableSpaceNumber; i++)
	{
		for (j = 0; j < tableSpaceList[i].fileNumber; j++)
		{
			va_free(tableSpaceList[i].dataFile[j]);
		}
		va_free(tableSpaceList[i].dataFile);
	}
	va_free(tableSpaceList);
	
	
	#ifdef __ABIO_WIN
		// 윈속 제거
		WSACleanup();
	#endif
}

void PrintQueryFileError(char * logMsgID, ...)
{
	char ** logs;
	int * logsLength;
	int logNumber;
	
	char ** lines;
	int * linesLength;
	int lineNumber;
	
	time_t current_t;
	char logTime[TIME_STAMP_LENGTH];
	int logTimeLength;
	
	va_list argptr;
	char logmsg[USER_LOG_MESSAGE_LENGTH];
	
	int i;
	
	#define RESERVED_LOG_LINE_NUMBER		2
	
	
	
	// initialize variables
	logs = NULL;
	logsLength = NULL;
	logNumber = 0;
	
	memset(logTime, 0, sizeof(logTime));
	memset(logmsg, 0, sizeof(logmsg));
	
	
	// 현재 시간 저장
	current_t = time(NULL);
	va_make_time_stamp(current_t, logTime, TIME_STAMP_TYPE_EXTERNAL_EN);
	logTimeLength = (int)strlen(logTime);
	
	
	// log file split bar와 빈 라인이 들어갈 공간 할당
	logs = (char **)malloc(sizeof(char *) * RESERVED_LOG_LINE_NUMBER);
	logsLength = (int *)malloc(sizeof(int) * RESERVED_LOG_LINE_NUMBER);
	
	
	// log file split bar 삽입
	logsLength[logNumber] = logTimeLength + 1 + (int)strlen(ABIO_DB_LOG_SPLIT_BAR);
	logs[logNumber] = (char *)malloc(sizeof(char) * (logsLength[logNumber] + 1));
	memset(logs[logNumber], 0, sizeof(char) * (logsLength[logNumber] + 1));
	sprintf(logs[logNumber], "%s %s", logTime, ABIO_DB_LOG_SPLIT_BAR);
	logNumber++;
	
	
	// 에러 로그 삽입
	if (logMsgID != NULL && logMsgID[0] != '\0')
	{
		for (i = 0; i < backupLogDescriptionNumber; i++)
		{
			if (!strcmp(backupLogDescription[i].code, logMsgID))
			{
				va_start(argptr, logMsgID);
				va_format_abio_log(logmsg, sizeof(logmsg), backupLogDescription[i].description, argptr);
				va_end(argptr);
				
				logs = (char **)realloc(logs, sizeof(char *) * (RESERVED_LOG_LINE_NUMBER + 1));
				logsLength = (int *)realloc(logsLength, sizeof(int) * (RESERVED_LOG_LINE_NUMBER + 1));
				
				logsLength[logNumber] = logTimeLength + 1 + (int)strlen(ABIO_DB_LOG_QUERY_ERROR_TITLE) + 1 + (int)strlen(logmsg);
				logs[logNumber] = (char *)malloc(sizeof(char) * (logsLength[logNumber] + 1));
				memset(logs[logNumber], 0, sizeof(char) * (logsLength[logNumber] + 1));
				sprintf(logs[logNumber], "%s %s %s", logTime, ABIO_DB_LOG_QUERY_ERROR_TITLE, logmsg);
				logNumber++;
				
				break;
			}
		}
	}
	else
	{
		if ((lineNumber = va_load_text_file_lines(ABIOMASTER_CLIENT_FOLDER, outputFile, &lines, &linesLength)) > 0)
		{
			// 에러 로그가 들어갈 공간 할당
			logs = (char **)realloc(logs, sizeof(char *) * (RESERVED_LOG_LINE_NUMBER + lineNumber));
			logsLength = (int *)realloc(logsLength, sizeof(int) * (RESERVED_LOG_LINE_NUMBER + lineNumber));
			
			for (i = 0; i < lineNumber; i++)
			{
				if (lines[i][0] == '\0')
				{
					continue;
				}
				
				logsLength[logNumber] = logTimeLength + 1 + (int)strlen(ABIO_DB_LOG_QUERY_ERROR_TITLE) + 1 + linesLength[i];
				logs[logNumber] = (char *)malloc(sizeof(char) * (logsLength[logNumber] + 1));
				memset(logs[logNumber], 0, sizeof(char) * (logsLength[logNumber] + 1));
				sprintf(logs[logNumber], "%s %s %s", logTime, ABIO_DB_LOG_QUERY_ERROR_TITLE, lines[i]);
				logNumber++;
			}
			
			for (i = 0; i < lineNumber; i++)
			{
				va_free(lines[i]);
			}
			va_free(lines);
			va_free(linesLength);
		}
	}
	
	
	// 빈 라인 삽입
	logsLength[logNumber] = 0;
	logs[logNumber] = (char *)malloc(sizeof(char) * (logsLength[logNumber] + 1));
	memset(logs[logNumber], 0, sizeof(char) * (logsLength[logNumber] + 1));
	logNumber++;
	
	
	// log file에 에러 로그 추가
	va_append_text_file_lines(ABIOMASTER_CLIENT_LOG_FOLDER, ABIOMASTER_ORACLE_RMAN_LOG_FILE, logs, logsLength, logNumber);
	
	
	for (i = 0; i < logNumber; i++)
	{
		va_free(logs[i]);
	}
	va_free(logs);
	va_free(logsLength);
}

int ComparatorRmanBackupsetDescending(const void * a1, const void * a2)
{
	struct oracleRmanBackupsetItem * l1;
	struct oracleRmanBackupsetItem * l2;

	l1 = (struct oracleRmanBackupsetItem *)a1;
	l2 = (struct oracleRmanBackupsetItem *)a2;

	return strcmp(l1->backupset, l2->backupset);
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

		printf("ORACLE_RMAN_QUERY_VERSION : %s\n",ORACLE_RMAN_QUERY_VERSION);

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
	

	return version;
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