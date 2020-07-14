#include "require.h"
#include "abio.h"
#include "lib.h"
#include "vendor.h"
#include "httpd.h"
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
char ABIOMASTER_CLIENT_LOG_FOLDER[MAX_PATH_LENGTH];


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
// variables for oracle connection
char username[ORACLE_USERNAME_LENGTH];
char account[ORACLE_ACCOUNT_LENGTH];
char passwd[ORACLE_PASSWORD_LENGTH];
char sid[ORACLE_SID_LENGTH];
char home[ORACLE_HOME_LENGTH];

char environmentValueSid[MAX_PATH_LENGTH];
char environmentValueHome[MAX_PATH_LENGTH];

char sqlcmdUtil[MAX_PATH_LENGTH];
char queryFile[MAX_PATH_LENGTH];
char outputFile[MAX_PATH_LENGTH];



///////////////////////////////////////////////////////////////////////////////////////////////////
// variables for oracle file list
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


//////////////////////////////////////////////////////////////////////////////////////////////////
// backup/restore log
struct _backupLogDescription * backupLogDescription;
int backupLogDescriptionNumber;



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
	
	struct oracle12cDB oracle12c;
	
	int i;
	
	
	
	///////////////////////////////////////////////////////////////////////////////////////////////////
	// 1. 전역 변수들을 초기화한다.
	
	memset(ABIOMASTER_CLIENT_FOLDER, 0, sizeof(ABIOMASTER_CLIENT_FOLDER));
	memset(ABIOMASTER_CLIENT_LOG_FOLDER, 0, sizeof(ABIOMASTER_CLIENT_LOG_FOLDER));
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// ABIO log option
	requestLogLevel = LOG_LEVEL_JOB;
	memset(logJobID, 0, sizeof(logJobID));
	logModuleNumber = MODULE_UNKNOWN;
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// variables for oracle connection
	memset(username, 0, sizeof(username));
	memset(account, 0, sizeof(account));
	memset(passwd, 0, sizeof(passwd));
	memset(sid, 0, sizeof(sid));
	memset(home, 0, sizeof(home));
	
	memset(environmentValueSid, 0, sizeof(environmentValueSid));
	memset(environmentValueHome, 0, sizeof(environmentValueHome));
	
	memset(sqlcmdUtil, 0, sizeof(sqlcmdUtil));
	memset(queryFile, 0, sizeof(queryFile));
	memset(outputFile, 0, sizeof(outputFile));
	
	
	///////////////////////////////////////////////////////////////////////////////////////////////////
	// variables for oracle12c file list
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
	
	// set module number for debug log of ABIO common library
	logModuleNumber = MODULE_DB_ORACLE12C_SCRIPT_HTTPD;
	
	// set master server ip and port
	strcpy(masterIP, command.sourceIP);
	strcpy(masterPort, command.sourcePort);
	strcpy(masterNodename, command.sourceNodename);
	strcpy(masterLogPort, commandData.client.masterLogPort);
	
	// set account and password
	memcpy(&oracle12c, commandData.db.dbData, sizeof(struct oracle12cDB));
	strcpy(username, oracle12c.username);
	strcpy(account, oracle12c.account);
	strcpy(passwd, oracle12c.passwd);
	strcpy(sid, oracle12c.sid);
	strcpy(home, oracle12c.home);
	
	#ifdef __ABIO_WIN
		va_slash_to_backslash(home);
	#endif
	
	// set the sqlcmd utility
	sprintf(sqlcmdUtil, "%s%cbin%c%s", home, FILE_PATH_DELIMITER, FILE_PATH_DELIMITER, ORACLE_SQLCMD_UTILITY);
	sprintf(queryFile, "oracle_%d.sql", va_getpid());
	sprintf(outputFile, "oracle_%d.out", va_getpid());
	
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

void ResponseRequest()
{
	if (!strcmp(command.subCommand, "<GET_ORACLE12C_FILE_LIST>"))
	{
		GetOracle12cFileList();
	}
	/*else if (!strcmp(command.subCommand, "<GET_ENTIRE_DATABASES>"))
	{
		GetEntireDatabases();		
	}*/
}

void GetOracle12cFileList()
{
	char reply[DSIZ][DSIZ];
	char msg[DSIZ * DSIZ];
	
	va_sock_t commandSock;
	
	char mtime[TIME_STAMP_LENGTH];
	
	int i;
	int j;
	
	if ((commandSock = va_connect(masterIP, commandData.catalogPort, 1)) != -1)
	{
		// get a oracle file list
		if (GetTableSpaceList() < 0)
		{
			va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
			
			return;
		}
		if (GetArchiveLogSize() < 0)
		{
			va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
			
			return;
		}
		if (GetControlFileSize() < 0)
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
			
			strcpy(reply[0], "ORACLE12C_FILE_LIST");
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
		
		strcpy(reply[0], "ORACLE12C_FILE_LIST");
		strcpy(reply[91], ORACLE_BACKUP_FILE_LIST_OPTION_OBJECT_ARCHIVE_LOG);
		va_ulltoa(archiveLogSize, reply[96]);
		
		memset(msg, 0, sizeof(msg));
		va_make_reply_msg(reply, msg);
		va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE);
		
		// send the control file
		va_init_reply_buf(reply);
		
		strcpy(reply[0], "ORACLE12C_FILE_LIST");
		strcpy(reply[91], ORACLE_BACKUP_FILE_LIST_OPTION_OBJECT_CONTROL_FILE);
		va_ulltoa(controlFileSize, reply[96]);
		
		memset(msg, 0, sizeof(msg));
		va_make_reply_msg(reply, msg);
		va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE);
		
		
		va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
	}
}

int GetArchiveLogSize()
{
	char ** lines;
	int * linesLength;
	int lineNumber;
	
	char archiveLogFolder[1024];
	int archiveLogNumber;
	
	char ** filelist;
	
	struct abio_file_stat file_stat;
	
	int i;
	int j;
	
	
	
	// initialize variables
	memset(archiveLogFolder, 0, sizeof(archiveLogFolder));
	archiveLogNumber = 0;
	filelist = NULL;
	
	if (ExecuteSql(GET_ARCHIVE_LOG_DIR_QUERY) != 0)
	{
		va_remove(ABIOMASTER_CLIENT_FOLDER, queryFile);
		va_remove(ABIOMASTER_CLIENT_FOLDER, outputFile);
		
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
				
				if (i == lineNumber)
				{
					break;
				}
				
				for (j = 0; j < linesLength[i]; j++)
				{
					if (lines[i][j] == '^')
					{
						strncpy(archiveLogFolder, lines[i], j);
						if (archiveLogFolder[strlen(archiveLogFolder) - 1] == FILE_PATH_DELIMITER)
						{
							archiveLogFolder[strlen(archiveLogFolder) - 1] = '\0';
						}
						
						break;
					}
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
	else
	{
		va_remove(ABIOMASTER_CLIENT_FOLDER, queryFile);
		va_remove(ABIOMASTER_CLIENT_FOLDER, outputFile);
		
		return -1;
	}
	
	
	// scan the archive log folder
	if ((archiveLogNumber = va_scandir(archiveLogFolder, &filelist)) > 0)
	{
		for (i = 0; i < archiveLogNumber; i++)
		{
			// get a archive log file status
			va_lstat(archiveLogFolder, filelist[i], &file_stat);
			
			// get a file size of the archive log file
			archiveLogSize += file_stat.size;
		}
		
		for (i = 0; i < archiveLogNumber; i++)
		{
			va_free(filelist[i]);
		}
		va_free(filelist);
	}
	
	
	va_remove(ABIOMASTER_CLIENT_FOLDER, queryFile);
	va_remove(ABIOMASTER_CLIENT_FOLDER, outputFile);
	
	
	return 0;
}

int ExecuteSql(char * query)
{
	char argQueryFile[MAX_PATH_LENGTH];
	char argQueryInfo[MAX_PATH_LENGTH];	

	
	int r;
	memset(argQueryFile, 0, sizeof(argQueryFile));
	memset(argQueryInfo,0,sizeof(argQueryInfo));
	
	// 실행할 query 파일을 만든다.
	if (MakeQueryFile(query) != 0)
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
	
	if (r != 0)
	{
		if (r == -1)
		{
			PrintQueryFileError(LOG_MSG_ID_ORACLE_QUERY_ERROR_EXECUTE_SCRIPT, "");
		}
		else
		{
			PrintQueryFileError(NULL);
		}
		
		return -1;
	}
	
	// sql query를 실행하면서 에러가 발생했는지 확인한다.
	if (CheckQueryError(query) != 0)
	{
		PrintQueryFileError(NULL);
		
		return -1;
	}
	
	
	return 0;
}

int MakeQueryFile(char * query)
{
	char ** lines;	
	char spoolQuery[MAX_PATH_LENGTH];

		
	
	// initialize variables
	memset(spoolQuery, 0, sizeof(spoolQuery));
	
	
	sprintf(spoolQuery, ORACLE12C_SPOOL_ON_QUERY, ABIOMASTER_CLIENT_FOLDER, FILE_PATH_DELIMITER, outputFile);
	
	lines = (char **)malloc(sizeof(char *) * ORACLE12C_QUERY_LINE_NUMBER);
	
	lines[0] = ORACLE12C_LINESIZE_QUERY;
	lines[1] = spoolQuery;
	lines[2] = query;
	lines[3] = ORACLE12C_SPOOL_OFF_QUERY;
	lines[4] = ORACLE12C_QUIT_QUERY;
	
	if (va_write_text_file_lines(ABIOMASTER_CLIENT_FOLDER, queryFile, lines, NULL, ORACLE12C_QUERY_LINE_NUMBER) != 0)
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


int GetControlFileSize()
{
	char ** lines;
	int * linesLength;
	int lineNumber;
	
	char controlFileName[1024];
	
	struct abio_file_stat file_stat;
	
	int i;
	int j;
	
	
	
	if (ExecuteSql(GET_CONTROL_FILE_QUERY) != 0)
	{
		va_remove(ABIOMASTER_CLIENT_FOLDER, queryFile);
		va_remove(ABIOMASTER_CLIENT_FOLDER, outputFile);
		
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
					
					for (j = 0; j < linesLength[i]; j++)
					{
						if (lines[i][j] == '^')
						{
							memset(controlFileName, 0, sizeof(controlFileName));
							strncpy(controlFileName, lines[i], j);
							
							// get a file size of the control file
							va_lstat(NULL, controlFileName, &file_stat);
							
							if (va_is_regular(file_stat.mode, file_stat.windowsAttribute))
							{
								controlFileSize += file_stat.size;
							}
							else
							{
								#ifdef __ABIO_AIX
									controlFileSize += GetRawDeviceFileSizeAIX(controlFileName, &file_stat);
								#elif __ABIO_SOLARIS
									controlFileSize += GetRawDeviceFileSizeSolaris(controlFileName, &file_stat);
								#elif __ABIO_HP
									controlFileSize += GetRawDeviceFileSizeHP(controlFileName, &file_stat);
								#elif __ABIO_LINUX
									controlFileSize += GetRawDeviceFileSizeLinux(controlFileName, &file_stat);
								#else
									// 이외의 시스템에서는 raw device file 백업을 지원하지 않는다.
								#endif
							}
							
							break;
						}
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
		va_remove(ABIOMASTER_CLIENT_FOLDER, queryFile);
		va_remove(ABIOMASTER_CLIENT_FOLDER, outputFile);
		
		return -1;
	}
	
	
	va_remove(ABIOMASTER_CLIENT_FOLDER, queryFile);
	va_remove(ABIOMASTER_CLIENT_FOLDER, outputFile);
	
	
	return 0;
}

int GetTableSpaceList()
{
	char ** lines;
	int * linesLength;
	int lineNumber;
	
	char databaseName[1024];
	char tableSpaceName[1024];
	char dataFileName[1024];
	
	struct abio_file_stat file_stat;
	
	int i;
	int j;
	int n;
	int k;
	
	
	
	if (ExecuteSql(GET_TABLE_SPACE_QUERY) != 0)
	{
		va_remove(ABIOMASTER_CLIENT_FOLDER, queryFile);
		va_remove(ABIOMASTER_CLIENT_FOLDER, outputFile);
		
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
					memset(databaseName, 0 , sizeof(databaseName));
					memset(tableSpaceName, 0, sizeof(tableSpaceName));
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
					for (n = j + 1; n < linesLength[i]; n++)
					{
						if (lines[i][n] == '^')
						{
							strncpy(tableSpaceName, lines[i] + j + 1, n - (j + 1));

							break;
						}
					}
					// query 결과에서 datafile 이름을 찾는다.
					for (k = linesLength[i] - 1; k > n; k--)
					{
						if (lines[i][k] == '^')
						{
							strncpy(dataFileName, lines[i] + n + 1, k - (n + 1));
							
							break;
						}
					}
					
					if (databaseName[0] == '\0')
					{
						break;
					}
					// query 결과에 delimiter가 없으면(빈줄) 종료한다.
					if (tableSpaceName[0] == '\0')
					{
						break;
					}
					
					// query 결과에 datafile이 없으면 다음 줄을 검사한다.
					if (dataFileName[0] == '\0')
					{
						continue;
					}
					
					
					// find the table space in the table space list
					for (k = 0; k < tableSpaceNumber; k++)
					{
						if (!strcmp(tableSpaceList[k].tableSpace, tableSpaceName) && !strcmp(tableSpaceList[k].database, databaseName) )
						{
							break;
						}
					}
					
					// table space가 table space list에 없으면 table space list에 추가한다.
					if (k == tableSpaceNumber)
					{
						// table space list를 하나 추가한다.
						if (k % DEFAULT_MEMORY_FRAGMENT == 0)
						{
							tableSpaceList = (struct tableSpaceItem *)realloc(tableSpaceList, sizeof(struct tableSpaceItem) * (k + DEFAULT_MEMORY_FRAGMENT));
							memset(tableSpaceList + k, 0, sizeof(struct tableSpaceItem) * DEFAULT_MEMORY_FRAGMENT);
						}
						
						// table space name을 기록한다.
						strcpy(tableSpaceList[k].database, databaseName);
						
						strcpy(tableSpaceList[k].tableSpace, tableSpaceName);	
						// table space list 개수를 증가시킨다.
						tableSpaceNumber++;
					}
					
					// table space에 data file을 추가한다.
					if (tableSpaceList[k].fileNumber % DEFAULT_MEMORY_FRAGMENT == 0)
					{
						tableSpaceList[k].dataFile = (char **)realloc(tableSpaceList[k].dataFile, sizeof(char *) * (tableSpaceList[k].fileNumber + DEFAULT_MEMORY_FRAGMENT));
						memset(tableSpaceList[k].dataFile + tableSpaceList[k].fileNumber, 0, sizeof(char *) * DEFAULT_MEMORY_FRAGMENT);
					}
					tableSpaceList[k].dataFile[tableSpaceList[k].fileNumber] = (char *)malloc(sizeof(char) * (strlen(dataFileName) + 1));
					memset(tableSpaceList[k].dataFile[tableSpaceList[k].fileNumber], 0, sizeof(char) * (strlen(dataFileName) + 1));
					strcpy(tableSpaceList[k].dataFile[tableSpaceList[k].fileNumber], dataFileName);

					// table space의 data file 개수를 증가시킨다.
					tableSpaceList[k].fileNumber++;
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
		va_remove(ABIOMASTER_CLIENT_FOLDER, queryFile);
		va_remove(ABIOMASTER_CLIENT_FOLDER, outputFile);
		
		return -1;
	}
	
	
	// get a table space size and the last modification time
	for (i = 0; i < tableSpaceNumber; i++)
	{
		for (j = 0; j < tableSpaceList[i].fileNumber; j++)
		{
			va_lstat(NULL, tableSpaceList[i].dataFile[j], &file_stat);
			
			// get a data file size of the table space
			if (va_is_regular(file_stat.mode, file_stat.windowsAttribute))
			{
				tableSpaceList[i].size += file_stat.size;
			}
			else
			{
				#ifdef __ABIO_AIX
					tableSpaceList[i].size += GetRawDeviceFileSizeAIX(tableSpaceList[i].dataFile[j], &file_stat);
				#elif __ABIO_SOLARIS
					tableSpaceList[i].size += GetRawDeviceFileSizeSolaris(tableSpaceList[i].dataFile[j], &file_stat);
				#elif __ABIO_HP
					tableSpaceList[i].size += GetRawDeviceFileSizeHP(tableSpaceList[i].dataFile[j], &file_stat);
				#elif __ABIO_LINUX
					tableSpaceList[i].size += GetRawDeviceFileSizeLinux(tableSpaceList[i].dataFile[j], &file_stat);
				#else
					// 이외의 시스템에서는 raw device file 백업을 지원하지 않는다.
				#endif
			}
			
			// get the last modification time of the table space
			if (tableSpaceList[i].mtime < file_stat.mtime)
			{
				tableSpaceList[i].mtime = (time_t)file_stat.mtime;
			}
		}
	}
	
	
	va_remove(ABIOMASTER_CLIENT_FOLDER, queryFile);
	va_remove(ABIOMASTER_CLIENT_FOLDER, outputFile);
	
	
	return 0;
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
			if (!strcmp(moduleConfig[i].optionName, CLIENT_LOG_FOLDER_OPTION_NAME))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(ABIOMASTER_CLIENT_LOG_FOLDER) - 1)
				{
					strcpy(ABIOMASTER_CLIENT_LOG_FOLDER, moduleConfig[i].optionValue);
				}
				
				break;
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
	
	return 0;
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
	va_append_text_file_lines(ABIOMASTER_CLIENT_LOG_FOLDER, ABIOMASTER_ORACLE12C_LOG_FILE, logs, logsLength, logNumber);
	
	
	for (i = 0; i < logNumber; i++)
	{
		va_free(logs[i]);
	}
	va_free(logs);
	va_free(logsLength);
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
