#include "require.h"
#include "abio.h"
#include "lib.h"
#include "vendor.h"
#include "httpd.h"


extern char ABIOMASTER_CK_FOLDER[MAX_PATH_LENGTH];
extern char ABIOMASTER_MASTER_SERVER_FOLDER[MAX_PATH_LENGTH];
extern char ABIOMASTER_CLIENT_FOLDER[MAX_PATH_LENGTH];
extern char ABIOMASTER_DATA_FOLDER[MAX_PATH_LENGTH];
extern char ABIOMASTER_CATALOG_DB_FOLDER[MAX_PATH_LENGTH];
extern char ABIOMASTER_FILE_LIST_FOLDER[MAX_PATH_LENGTH];
extern char ABIOMASTER_LOG_FOLDER[MAX_PATH_LENGTH];
extern char ABIOMASTER_VOLUME_DB_FOLDER[MAX_PATH_LENGTH];
extern char ABIOMASTER_REPORT_FOLDER[MAX_PATH_LENGTH];
extern char ABIOMASTER_CLIENT_LOG_FOLDER[MAX_PATH_LENGTH];

extern char masterNodename[NODE_NAME_LENGTH];		// master server node name
extern char masterIP[IP_LENGTH];					// master server ip address
extern char masterPort[PORT_LENGTH];				// master server communication kernel port
extern char masterLogPort[PORT_LENGTH];				// master server httpd logger port
extern char lcPort[PORT_LENGTH];					// master server library controler port
extern char scPort[PORT_LENGTH];					// master server scheduler port
extern char hdPort[PORT_LENGTH];					// master server httpd port

extern struct portRule * portRule;					// VIRBAK ABIO IP Table 목록
extern int portRuleNumber;							// VIRBAK ABIO IP Table 개수

extern __int64 validationDate;

struct oracleArchiveLog
{
	char filepath[ABIO_FILE_LENGTH * 2];
	char backupset[BACKUPSET_ID_LENGTH];
	__uint64 size;
	__int64 mtime;
	__int64 backupTime;
};

struct oracleControlFile
{
	char backupset[BACKUPSET_ID_LENGTH];
	__uint64 size;
	__int64 backupTime;
};

struct oracleTablespace
{
	char backupset[BACKUPSET_ID_LENGTH];
	char filepath[ABIO_FILE_LENGTH * 2];
	__uint64 size;
	__int64 backupTime;
};

static int ComparatorOracleArchiveLogDescending(const void * a1, const void * a2);
static int ComparatorOracleControlFileDescending(const void * a1, const void * a2);
static int ComparatorOracleTablespaceDescending(const void * a1, const void * a2);


static int CheckDataValidation_GET_ORACLE12C_FILE_LIST(char value[DSIZ][DSIZ]);
static int CheckDataValidation_GET_ORACLE12C_BACKUP_TABLESPACE_LIST(char value[DSIZ][DSIZ]);
static int CheckDataValidation_GET_ORACLE12C_CATALOG_DB(char value[DSIZ][DSIZ]);
static int CheckDataValidation_GET_ORACLE12C_BACKUP_SET_MODE(char value[DSIZ][DSIZ]);
int GetOracle12cFileList(char * cmd, va_sock_t sock)
{
	char value[DSIZ][DSIZ];
	int valueNumber;
	
	char msg[DSIZ * DSIZ];
	
	struct ck command;
	struct ckBackup commandData;
	
	va_sock_t commandSock;
	va_sock_t masterSock;
	va_sock_t clientSock;
	
	int recvSize;
	
	int i;
	
	
	valueNumber = va_parser(cmd, value);
	
	if (CheckDataValidation_GET_ORACLE12C_FILE_LIST(value) < 0)
	{
		memset(msg, 0, sizeof(msg));
		sprintf(msg, "%s\\invalid_format\\", value[0]);
		va_send(sock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE);
		
		return 0;
	}
	
	memset(msg, 0, sizeof(msg));
	sprintf(msg, "ORACLE12C_FILE_LIST_START\\start\\");
	if (va_send(sock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE) < 0)
	{
		return -1;
	}
	memset(&command, 0, sizeof(struct ck));
	memset(&commandData, 0, sizeof(struct ckBackup));
	
	// client setting
	if (SetNodeInfo(value[110], &commandData.client, &commandData.ndmp) == 0)
	{
		// client가 살아있는지 확인한다.
		if (CheckNodeAlive(commandData.client.ip, commandData.client.port) < 0)
		{
			va_write_error_code(ERROR_LEVEL_ERROR, 0, 0, 0, &commandData);
		}
	}
	else
	{
		va_write_error_code(ERROR_LEVEL_ERROR, 0, 0, 0, &commandData);
	}
	
	if (!GET_ERROR(commandData.errorCode))
	{
		command.request = 1;
		command.reply = 0;
		strcpy(command.requestCommand, MODULE_NAME_DB_ORACLE12C_HTTPD);
		strcpy(command.subCommand, "<GET_ORACLE12C_FILE_LIST>");
		strcpy(command.executeCommand, MODULE_NAME_MASTER_HTTPD);
		strcpy(command.sourceIP, masterIP);
		strcpy(command.sourcePort, masterPort);
		strcpy(command.sourceNodename, masterNodename);
		strcpy(command.destinationIP, commandData.client.ip);
		strcpy(command.destinationPort, commandData.client.port);
		strcpy(command.destinationNodename, commandData.client.nodename);
		
		strcpy(((struct oracle12cDB *)commandData.db.dbData)->username, value[201]);
		strcpy(((struct oracle12cDB *)commandData.db.dbData)->account, value[202]);
		strcpy(((struct oracle12cDB *)commandData.db.dbData)->passwd, value[203]);
		strcpy(((struct oracle12cDB *)commandData.db.dbData)->sid, value[204]);
		strcpy(((struct oracle12cDB *)commandData.db.dbData)->home, value[205]);
		
		// client로부터 접속을 받을 server socket을 만든다.
		if ((masterSock = va_make_server_socket_iptable(command.destinationIP, command.sourceIP, portRule, portRuleNumber, commandData.catalogPort)) != -1)
		{
			// 명령을 client로 보낸다.
			if ((commandSock = va_connect("127.0.0.1", masterPort, 1)) != -1)
			{
				memcpy(command.dataBlock, &commandData, sizeof(struct ckBackup));
				
				va_send(commandSock, &command, sizeof(struct ck), 0, DATA_TYPE_CK);
				va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
				
				
				// client로부터 접속을 기다린다.
				if ((clientSock = va_wait_connection(masterSock, TIME_OUT_HTTPD_REQUEST, NULL)) >= 0)
				{
					// client로부터 결과를 받는다.
					while ((recvSize = va_recv(clientSock, msg, sizeof(msg), 0, DATA_TYPE_NOT_CHANGE)) > 0)
					{
						for (i = 0; i < recvSize; i++)
						{
							if (msg[i] != '\0')
							{
								if (va_send(sock, msg + i, (int)strlen(msg + i) + 1, 0, DATA_TYPE_NOT_CHANGE) < 0)
								{
									va_write_error_code(ERROR_LEVEL_ERROR, 0, 0, 0, &commandData);
									
									break;
								}
								
								i += (int)strlen(msg + i);
							}
						}
						
						if (GET_ERROR(commandData.errorCode))
						{
							break;
						}
					}
					
					va_close_socket(clientSock, ABIO_SOCKET_CLOSE_SERVER);
				}
			}
			
			va_close_socket(masterSock, 0);
		}
	}
	
	memset(msg, 0, sizeof(msg));
	sprintf(msg, "ORACLE12C_FILE_LIST_END\\end\\");
	va_send(sock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE);
	
	
	return 0;
}

int GetOracle12cCatalogDB(char * cmd, va_sock_t sock)
{
	char value[DSIZ][DSIZ];
	int valueNumber;
	
	char reply[DSIZ][DSIZ];
	char msg[DSIZ * DSIZ];
	
	va_fd_t fdBackupsetList;
	char backupsetListFile[MAX_PATH_LENGTH];
	struct backupsetList backupsetList;
	
	va_fd_t fdCatalog;
	struct catalogDB catalogDB;
	char backupFilePath[ABIO_FILE_LENGTH * 2];
	char tableSpaceName[ABIO_FILE_LENGTH];
	
	struct oracleArchiveLog * archiveLogList;
	int archiveLogNumber;
	int catalogNumber;
	
	struct oracleTablespace * tablespaceList;
	int tablespaceNumber;
	
	struct oracleControlFile * controlFileList;
	int controlFileNumber;
	
	char backupTime[TIME_STAMP_LENGTH];
	
	int i;
	int j;
	
	
	
	valueNumber = va_parser(cmd, value);
	
	if (CheckDataValidation_GET_ORACLE12C_CATALOG_DB(value) < 0)
	{
		memset(msg, 0, sizeof(msg));
		sprintf(msg, "%s\\invalid_format\\", value[0]);
		va_send(sock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE);
		
		return 0;
	}
	
	
	// initialize variables
	memset(backupsetListFile, 0, sizeof(backupsetListFile));
	
	archiveLogList = NULL;
	archiveLogNumber = 0;
	
	tablespaceList = NULL;
	tablespaceNumber = 0;
	
	controlFileList = NULL;
	controlFileNumber = 0;
	
	
	memset(msg, 0, sizeof(msg));
	sprintf(msg, "ORACLE12C_CATALOG_DB_START\\start\\");
	if (va_send(sock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE) < 0)
	{
		return -1;
	}
	
	
	va_get_backupset_list_file_name(value[110], ABIO_BACKUP_DATA_ORACLE12C, backupsetListFile);
	
	if ((fdBackupsetList = va_open(ABIOMASTER_CATALOG_DB_FOLDER, backupsetListFile, O_RDONLY, 1, 1)) != (va_fd_t)-1)
	{
		while (va_read(fdBackupsetList, &backupsetList, sizeof(struct backupsetList), DATA_TYPE_BACKUPSET_LIST) > 0)
		{
			if (!strcmp(backupsetList.instance, value[204]))
			{
				/*#issue 223 버전호환성
				// evaluation key가 있는 상태(validationDate가 0이 아닌 경우)에서는 evaluation key가 등록되기 이전의 백업셋들은 복구할 수 없도록 UI 로 전송하지 않는다.
				if (validationDate > 0 && backupsetList.backupTime < validationDate)
				{
					continue;
				}
				*/
				
				if ((fdCatalog = va_open(ABIOMASTER_CATALOG_DB_FOLDER, backupsetList.backupset, O_RDONLY, 1, 1)) != (va_fd_t)-1)
				{
					catalogNumber = 0;
					
					while (va_read_catalog_db(fdCatalog, &catalogDB, backupFilePath, sizeof(backupFilePath), tableSpaceName, sizeof(tableSpaceName), NULL, 0) == 0)
					{
						if (catalogDB.expire == 1)
						{
							continue;
						}

						if (!strcmp(value[91], ORACLE_RESTORE_FILE_LIST_OPTION_OBJECT_ARCHIVE_LOG))
						{
							if (catalogDB.st.filetype == ORACLE12C_ARCHIVE_LOG)
							{
								// archive log의 file name만을 얻는다.
								for (j = (int)strlen(backupFilePath); j >=0; j--)
								{
									if (backupFilePath[j] == DIRECTORY_IDENTIFIER)
									{
										break;
									}
								}
								
								// backup된 archive log를 archive log list에서 찾는다.
								for (i = 0; i < archiveLogNumber; i++)
								{
									if (!strcmp(archiveLogList[i].filepath, backupFilePath + j + 1))
									{
										break;
									}
								}
								
								// 기존 archive log list에 없는 것이면 추가한다.
								if (i == archiveLogNumber)
								{
									if ((i + catalogNumber) % DEFAULT_MEMORY_FRAGMENT == 0)
									{
										archiveLogList = (struct oracleArchiveLog *)realloc(archiveLogList, sizeof(struct oracleArchiveLog) * (i + catalogNumber + DEFAULT_MEMORY_FRAGMENT));
										memset(archiveLogList + i + catalogNumber, 0, sizeof(struct oracleArchiveLog) * DEFAULT_MEMORY_FRAGMENT);
									}
									strcpy(archiveLogList[i + catalogNumber].filepath, backupFilePath + j + 1);
									strcpy(archiveLogList[i + catalogNumber].backupset, catalogDB.backupset);
									archiveLogList[i + catalogNumber].size = catalogDB.st.size;
									archiveLogList[i + catalogNumber].mtime = catalogDB.st.mtime;
									archiveLogList[i + catalogNumber].backupTime = catalogDB.st.backupTime;
									catalogNumber++;
								}
								// 기존 archive log list에 있는 것이면 backupset을 교체한다.
								else
								{	
									memset(archiveLogList[i].backupset, 0, sizeof(archiveLogList[i].backupset));
									strcpy(archiveLogList[i].backupset, catalogDB.backupset);
								}
							}
						}
						else if (!strcmp(value[91], ORACLE_RESTORE_FILE_LIST_OPTION_OBJECT_CONTROL_FILE))
						{
							if (catalogDB.st.filetype == ORACLE12C_CONTROL_FILE)
							{
								// 한번의 백업 작업에 여러개의 control file이 백업될 수 있기 때문에, 기존 control file list에 있는 것인지 확인한다.
								for (i = 0; i < controlFileNumber; i++)
								{
									if (controlFileNumber != 0)
									{
										break;
									}
								/*	if (!strcmp(controlFileList[i].backupset, catalogDB.backupset))
									{
										break;
									}*/
								}
								
								// 기존 control file list에 없는 것이면 추가한다.
								if (i == controlFileNumber)
								{
									if (controlFileNumber % DEFAULT_MEMORY_FRAGMENT == 0)
									{
										controlFileList = (struct oracleControlFile *)realloc(controlFileList, sizeof(struct oracleControlFile) * (controlFileNumber + DEFAULT_MEMORY_FRAGMENT));
										memset(controlFileList + controlFileNumber, 0, sizeof(struct oracleControlFile) * DEFAULT_MEMORY_FRAGMENT);
									}
									strcpy(controlFileList[controlFileNumber].backupset, catalogDB.backupset);
									controlFileList[controlFileNumber].size = catalogDB.st.size;
									controlFileList[controlFileNumber].backupTime = catalogDB.st.backupTime;
									controlFileNumber++;
								}
								// 기존 control file list에 있는 것이면 control file의 크기를 증기시킨다.
								else
								{
									memset(controlFileList[i].backupset, 0 , sizeof(controlFileList[i].backupset));
									strcpy(controlFileList[i].backupset, catalogDB.backupset);
									controlFileList[i].size = catalogDB.st.size;
									controlFileList[i].backupTime = catalogDB.st.backupTime;
									//controlFileList[i].size += catalogDB.st.size;
								}
							}
						}
						else if (!strcmp(value[91], tableSpaceName))
						{
							if (catalogDB.st.filetype == ORACLE12C_REGULAR_DATA_FILE || catalogDB.st.filetype == ORACLE12C_RAW_DEVICE_DATA_FILE)
							{
								//// 기존 tablespace list에 있는 것인지 확인한다.
								//for (i = 0; i < tablespaceNumber; i++)
								//{
								//	if (!strcmp(tablespaceList[i].backupset, catalogDB.backupset))
								//	{
								//		break;
								//	}
								//}
								//
								// 기존 tablespace list에 없는 것이면 추가한다.
								for (i = 0; i < tablespaceNumber; i++)
								{
									if (!strcmp(tablespaceList[i].filepath, backupFilePath))
									{
										break;
									}
								}		

								if (i == tablespaceNumber)
								{
									if (tablespaceNumber % DEFAULT_MEMORY_FRAGMENT == 0)
									{
										tablespaceList = (struct oracleTablespace *)realloc(tablespaceList, sizeof(struct oracleTablespace) * (tablespaceNumber + DEFAULT_MEMORY_FRAGMENT));
										memset(tablespaceList + tablespaceNumber, 0, sizeof(struct oracleTablespace) * DEFAULT_MEMORY_FRAGMENT);
									}
									strcpy(tablespaceList[tablespaceNumber].backupset, catalogDB.backupset);
									strcpy(tablespaceList[tablespaceNumber].filepath, backupFilePath);	
									tablespaceList[tablespaceNumber].size = catalogDB.st.size;
									tablespaceList[tablespaceNumber].backupTime = catalogDB.st.backupTime;
									tablespaceNumber++;
								}
								// 기존 tablespace list에 있는 것이면 tablespace의 크기를 증가시킨다.
								else
								{
									memset(tablespaceList[i].backupset, 0, sizeof(tablespaceList[i].backupset));									
									strcpy(tablespaceList[i].backupset, catalogDB.backupset);
									tablespaceList[i].size = catalogDB.st.size;									
									tablespaceList[i].backupTime = catalogDB.st.backupTime;
								}
							}
						}
					}
					
					va_close(fdCatalog);
					
					
					archiveLogNumber += catalogNumber;
				}
			}
		}
		
		va_close(fdBackupsetList);
		
		
		if (!strcmp(value[91], ORACLE_RESTORE_FILE_LIST_OPTION_OBJECT_ARCHIVE_LOG))
		{
			qsort(archiveLogList, archiveLogNumber, sizeof(struct oracleArchiveLog), ComparatorOracleArchiveLogDescending);
			
			for (i = 0; i < archiveLogNumber; i++)
			{
				// set backup time
				memset(backupTime, 0, sizeof(backupTime));
				va_make_time_stamp((time_t)archiveLogList[i].backupTime, backupTime, TIME_STAMP_TYPE_INTERNAL);
				
				
				// make reply message
				va_init_reply_buf(reply);
				
				strcpy(reply[0], "ORACLE12C_CATALOG_DB");
				
				strcpy(reply[91], archiveLogList[i].filepath);
				va_ulltoa(archiveLogList[i].size, reply[96]);
				strcpy(reply[150], archiveLogList[i].backupset);
				strcpy(reply[153], backupTime);
				
				va_make_reply_msg(reply, msg);
				if (va_send(sock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE) < 0)
				{
					break;
				}
			}
			
			va_free(archiveLogList);
		}
		else if (!strcmp(value[91], ORACLE_RESTORE_FILE_LIST_OPTION_OBJECT_CONTROL_FILE))
		{
			qsort(controlFileList, controlFileNumber, sizeof(struct oracleControlFile), ComparatorOracleControlFileDescending);
			
			for (i = 0; i < controlFileNumber; i++)
			{
				// set backup time
				memset(backupTime, 0, sizeof(backupTime));
				va_make_time_stamp((time_t)controlFileList[i].backupTime, backupTime, TIME_STAMP_TYPE_INTERNAL);
				
				
				// make reply message
				va_init_reply_buf(reply);
				
				strcpy(reply[0], "ORACLE12C_CATALOG_DB");
				
				strcpy(reply[91], ORACLE_RESTORE_FILE_LIST_OPTION_OBJECT_FULL_BACKUP);
				va_ulltoa(controlFileList[i].size, reply[96]);
				strcpy(reply[150], controlFileList[i].backupset);
				strcpy(reply[153], backupTime);
				
				va_make_reply_msg(reply, msg);
				if (va_send(sock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE) < 0)
				{
					break;
				}
			}
			
			va_free(controlFileList);
		}
		else
		{
			qsort(tablespaceList, tablespaceNumber, sizeof(struct oracleTablespace), ComparatorOracleTablespaceDescending);
			
			for (i = 0; i < tablespaceNumber; i++)
			{
				// set backup time
				memset(backupTime, 0, sizeof(backupTime));
				va_make_time_stamp((time_t)tablespaceList[i].backupTime, backupTime, TIME_STAMP_TYPE_INTERNAL);
				
				
				// make reply message
				va_init_reply_buf(reply);
				
				strcpy(reply[0], "ORACLE12C_CATALOG_DB");
				
				strcpy(reply[91], tablespaceList[i].filepath);
				va_ulltoa(tablespaceList[i].size, reply[96]);
				strcpy(reply[150], tablespaceList[i].backupset);
				strcpy(reply[153], backupTime);
				
				va_make_reply_msg(reply, msg);
				if (va_send(sock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE) < 0)
				{
					break;
				}
			}
			
			va_free(tablespaceList);
		}
	}
	
	
	memset(msg, 0, sizeof(msg));
	sprintf(msg, "ORACLE12C_CATALOG_DB_END\\end\\");
	va_send(sock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE);
	
	
	return 0;
}


int GetOracle12cInstance(char * cmd, va_sock_t sock)
{
	char value[DSIZ][DSIZ];
	int valueNumber;
	
	char reply[DSIZ][DSIZ];
	char msg[DSIZ * DSIZ];
	
	va_fd_t fdBackupsetList;
	char backupsetListFile[MAX_PATH_LENGTH];
	struct backupsetList backupsetList;

	
	valueNumber = va_parser(cmd, value);

	if (CheckDataValidation_GET_ORACLE12C_BACKUP_SET_MODE(value) < 0)
	{
		memset(msg, 0, sizeof(msg));
		sprintf(msg, "%s\\invalid_format\\", value[0]);
		va_send(sock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE);
		
		return 0;
	}

	
	// initialize variables
	memset(backupsetListFile, 0, sizeof(backupsetListFile));
	
	
	memset(msg, 0, sizeof(msg));
	sprintf(msg, "ORACLE12C_INSTANCE_START\\start\\");
	if (va_send(sock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE) < 0)
	{
		return -1;
	}
	va_get_backupset_list_file_name(value[110], ABIO_BACKUP_DATA_ORACLE12C, backupsetListFile);

	if ((fdBackupsetList = va_open(ABIOMASTER_CATALOG_DB_FOLDER, backupsetListFile, O_RDONLY, 1, 0)) != (va_fd_t)-1)
	{
		while (va_read(fdBackupsetList, &backupsetList, sizeof(struct backupsetList), DATA_TYPE_BACKUPSET_LIST) > 0)
		{
			if (!strcmp(backupsetList.backupset, value[150]))
			{		
				/*#issue 223 버전호환성
				// evaluation key가 있는 상태(validationDate가 0이 아닌 경우)에서는 evaluation key가 등록되기 이전의 백업셋들은 복구할 수 없도록 UI 로 전송하지 않는다.
				if (validationDate > 0 && backupsetList.backupTime < validationDate)
				{
					continue;
				}
				*/

				va_init_reply_buf(reply);
			
				strcpy(reply[0], "ORACLE12C_INSTANCE");
				strcpy(reply[201], backupsetList.instance);

				va_make_reply_msg(reply, msg);
				if (va_send(sock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE) < 0)
				{
					break;
				}				
			}
			
		}
	}
	va_close(fdBackupsetList);

	memset(msg, 0, sizeof(msg));
	sprintf(msg, "ORACLE12C_INSTANCE_END\\end\\");
	va_send(sock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE);

	return 0;
}
static int ComparatorOracleArchiveLogDescending(const void * a1, const void * a2)
{
	struct oracleArchiveLog * l1;
	struct oracleArchiveLog * l2;
	
	
	
	l1 = (struct oracleArchiveLog *)a1;
	l2 = (struct oracleArchiveLog *)a2;
	
	if (l1->mtime > l2->mtime)
	{
		return -1;
	}
	else if (l1->mtime < l2->mtime)
	{
		return 1;
	}
	else
	{
		if (strcmp(l1->backupset, l2->backupset) > 0)
		{
			return -1;
		}
		else if (strcmp(l1->backupset, l2->backupset) < 0)
		{
			return 1;
		}
		else
		{
			return strcmp(l2->filepath, l1->filepath);
		}
	}
}

static int ComparatorOracleControlFileDescending(const void * a1, const void * a2)
{
	struct oracleControlFile * l1;
	struct oracleControlFile * l2;
	
	
	
	l1 = (struct oracleControlFile *)a1;
	l2 = (struct oracleControlFile *)a2;
	
	return strcmp(l2->backupset, l1->backupset);
}

static int ComparatorOracleTablespaceDescending(const void * a1, const void * a2)
{
	struct oracleTablespace * l1;
	struct oracleTablespace * l2;
	
	
	
	l1 = (struct oracleTablespace *)a1;
	l2 = (struct oracleTablespace *)a2;
	
	return strcmp(l2->backupset, l1->backupset);
}

int GetOracle12cBackupTablespaceList(char * cmd, va_sock_t sock)
{
	char value[DSIZ][DSIZ];
	int valueNumber;
	
	char reply[DSIZ][DSIZ];
	char msg[DSIZ * DSIZ];
	
	va_fd_t fdBackupsetList;
	char backupsetListFile[MAX_PATH_LENGTH];
	struct backupsetList backupsetList;
	
	va_fd_t fdCatalog;
	struct catalogDB catalogDB;
	char backupFilePath[ABIO_FILE_LENGTH * 2];
	char tableSpaceName[ABIO_FILE_LENGTH];
	
	char ** tableSpaceList;
	int tableSpaceNumber;
	
	int i;
	
	
	
	valueNumber = va_parser(cmd, value);
	
	if (CheckDataValidation_GET_ORACLE12C_BACKUP_TABLESPACE_LIST(value) < 0)
	{
		memset(msg, 0, sizeof(msg));
		sprintf(msg, "%s\\invalid_format\\", value[0]);
		va_send(sock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE);
		
		return 0;
	}
	
	
	// initialize variables
	memset(backupsetListFile, 0, sizeof(backupsetListFile));
	
	tableSpaceList = NULL;
	tableSpaceNumber = 0;
	
	
	memset(msg, 0, sizeof(msg));
	sprintf(msg, "ORACLE12C_BACKUP_TABLESPACE_START\\start\\");
	if (va_send(sock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE) < 0)
	{
		return -1;
	}
	
	
	va_get_backupset_list_file_name(value[110], ABIO_BACKUP_DATA_ORACLE12C, backupsetListFile);
	
	if ((fdBackupsetList = va_open(ABIOMASTER_CATALOG_DB_FOLDER, backupsetListFile, O_RDONLY, 1, 1)) != (va_fd_t)-1)
	{
		while (va_read(fdBackupsetList, &backupsetList, sizeof(struct backupsetList), DATA_TYPE_BACKUPSET_LIST) > 0)
		{
			if (!strcmp(backupsetList.instance, value[204]))
			{
				/*#issue 223 버전호환성
				// evaluation key가 있는 상태(validationDate가 0이 아닌 경우)에서는 evaluation key가 등록되기 이전의 백업셋들은 복구할 수 없도록 UI 로 전송하지 않는다.
				if (validationDate > 0 && backupsetList.backupTime < validationDate)
				{
					continue;
				}
				*/
				
				if ((fdCatalog = va_open(ABIOMASTER_CATALOG_DB_FOLDER, backupsetList.backupset, O_RDONLY, 1, 1)) != (va_fd_t)-1)
				{
					while (va_read_catalog_db(fdCatalog, &catalogDB, backupFilePath, sizeof(backupFilePath), tableSpaceName, sizeof(tableSpaceName), NULL, 0) == 0)
					{
						if (catalogDB.st.filetype == ORACLE12C_REGULAR_DATA_FILE || catalogDB.st.filetype == ORACLE12C_RAW_DEVICE_DATA_FILE)
						{
							for (i = 0; i < tableSpaceNumber; i++)
							{
								if (!strcmp(tableSpaceList[i], tableSpaceName))
								{
									break;
								}
							}
							
							if (i == tableSpaceNumber)
							{
								if (tableSpaceNumber % DEFAULT_MEMORY_FRAGMENT == 0)
								{
									tableSpaceList = (char **)realloc(tableSpaceList, sizeof(char *) * (tableSpaceNumber + DEFAULT_MEMORY_FRAGMENT));
									memset(tableSpaceList + tableSpaceNumber, 0, sizeof(char *) * DEFAULT_MEMORY_FRAGMENT);
								}
								tableSpaceList[tableSpaceNumber] = (char *)malloc(sizeof(char) * (strlen(tableSpaceName) + 1));
								memset(tableSpaceList[tableSpaceNumber], 0, sizeof(char) * (strlen(tableSpaceName) + 1));
								strcpy(tableSpaceList[tableSpaceNumber], tableSpaceName);
								tableSpaceNumber++;
							}
						}
					}
					
					va_close(fdCatalog);
				}
			}
		}
		
		va_close(fdBackupsetList);
		
		
		qsort(tableSpaceList, tableSpaceNumber, sizeof(char *), ComparatorString);
		
		for (i = 0; i < tableSpaceNumber; i++)
		{
			// make reply message
			va_init_reply_buf(reply);
			
			strcpy(reply[0], "ORACLE12C_BACKUP_TABLESPACE");
			strcpy(reply[91], tableSpaceList[i]);
			
			va_make_reply_msg(reply, msg);
			if (va_send(sock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE) < 0)
			{
				break;
			}
		}
		
		// make reply message
		va_init_reply_buf(reply);
		
		strcpy(reply[0], "ORACLE12C_BACKUP_TABLESPACE");
		strcpy(reply[91], ORACLE_RESTORE_FILE_LIST_OPTION_OBJECT_ARCHIVE_LOG);
		
		va_make_reply_msg(reply, msg);
		va_send(sock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE);
		
		// make reply message
		va_init_reply_buf(reply);
		
		strcpy(reply[0], "ORACLE12C_BACKUP_TABLESPACE");
		strcpy(reply[91], ORACLE_RESTORE_FILE_LIST_OPTION_OBJECT_CONTROL_FILE);
		
		va_make_reply_msg(reply, msg);
		va_send(sock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE);
		
		
		for (i = 0; i < tableSpaceNumber; i++)
		{
			va_free(tableSpaceList[i]);
		}
		va_free(tableSpaceList);
	}
	
	
	memset(msg, 0, sizeof(msg));
	sprintf(msg, "ORACLE12C_BACKUP_TABLESPACE_END\\end\\");
	va_send(sock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE);
	
	
	return 0;
}
int GetOracle12cTableSpace(char * cmd, va_sock_t sock)
{
	char value[DSIZ][DSIZ];
	int valueNumber;
	
	char reply[DSIZ][DSIZ];
	char msg[DSIZ * DSIZ];
	
	char catalogDBFile[MAX_PATH_LENGTH];

	va_fd_t fdCatalog;
	struct catalogDB catalogDB;
	char backupFilePath[ABIO_FILE_LENGTH * 2];
	char dbSpaceName[ABIO_FILE_LENGTH];
	
	char ** dbSpaceList;
	int dbSpaceNumber;
	
	int i;
	
	
	valueNumber = va_parser(cmd, value);
	
	if (CheckDataValidation_GET_ORACLE12C_BACKUP_SET_MODE(value) < 0)
	{
		memset(msg, 0, sizeof(msg));
		sprintf(msg, "%s\\invalid_format\\", value[0]);
		va_send(sock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE);
		
		return 0;
	}
	
	
	// initialize variables
	memset(catalogDBFile, 0, sizeof(catalogDBFile));

	
	dbSpaceList = NULL;
	dbSpaceNumber = 0;
	
	
	memset(msg, 0, sizeof(msg));
	sprintf(msg, "ORACLE12C_TABLESPACE_START\\start\\");
	if (va_send(sock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE) < 0)
	{
		return -1;
	}
	strcpy(catalogDBFile, value[150]);
	if ((fdCatalog = va_open(ABIOMASTER_CATALOG_DB_FOLDER, catalogDBFile , O_RDONLY, 1, 1)) != (va_fd_t)-1)
	{
		while (va_read_catalog_db(fdCatalog, &catalogDB, backupFilePath, sizeof(backupFilePath), dbSpaceName, sizeof(dbSpaceName), NULL, 0) == 0)
		{
			if (catalogDB.st.filetype == ORACLE12C_REGULAR_DATA_FILE || catalogDB.st.filetype == ORACLE12C_RAW_DEVICE_DATA_FILE)
			{
				for (i = 0; i < dbSpaceNumber; i++)
				{
					if (!strcmp(dbSpaceList[i], dbSpaceName))
					{
						break;
					}
				}
				if (i == dbSpaceNumber)
				{
					if (dbSpaceNumber % DEFAULT_MEMORY_FRAGMENT == 0)
					{
						dbSpaceList = (char **)realloc(dbSpaceList, sizeof(char *) * (dbSpaceNumber + DEFAULT_MEMORY_FRAGMENT));
						memset(dbSpaceList + dbSpaceNumber, 0, sizeof(char *) * DEFAULT_MEMORY_FRAGMENT);
					}
					dbSpaceList[dbSpaceNumber] = (char *)malloc(sizeof(char) * (strlen(dbSpaceName) + 1));
					memset(dbSpaceList[dbSpaceNumber], 0, sizeof(char) * (strlen(dbSpaceName) + 1));
					strcpy(dbSpaceList[dbSpaceNumber], dbSpaceName);								
					dbSpaceNumber++;
				}
			}
		}
		
		va_close(fdCatalog);
	}
	qsort(dbSpaceList, dbSpaceNumber, sizeof(char *), ComparatorString);
	
	for (i = 0; i < dbSpaceNumber; i++)
	{
		// make reply message
		va_init_reply_buf(reply);
		
		strcpy(reply[0], "ORACLE12C_TABLESPACE");
		strcpy(reply[91], dbSpaceList[i]);
		va_make_reply_msg(reply, msg);
		if (va_send(sock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE) < 0)
		{
			break;
		}
	}

	// make reply message
		va_init_reply_buf(reply);
		
	strcpy(reply[0], "ORACLE12C_TABLESPACE");
	strcpy(reply[91], ORACLE_RESTORE_FILE_LIST_OPTION_OBJECT_ARCHIVE_LOG);
	
	va_make_reply_msg(reply, msg);
	va_send(sock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE);
	
	// make reply message
	va_init_reply_buf(reply);
	
	strcpy(reply[0], "ORACLE12C_TABLESPACE");
	strcpy(reply[91], ORACLE_RESTORE_FILE_LIST_OPTION_OBJECT_CONTROL_FILE);
	
	va_make_reply_msg(reply, msg);
	va_send(sock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE);	
	

	for (i = 0; i < dbSpaceNumber; i++)
	{
		va_free(dbSpaceList[i]);
	}
	va_free(dbSpaceList);


	memset(msg, 0, sizeof(msg));
	sprintf(msg, "ORACLE12C_TABLESPACE_END\\end\\");
	va_send(sock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE);
	
	return 0;
}



int GetOracle12cTableSpaceCatalog(char * cmd, va_sock_t sock)
{
	char value[DSIZ][DSIZ];
	int valueNumber;
	
	char reply[DSIZ][DSIZ];
	char msg[DSIZ * DSIZ];
	
	char catalogDBFile[MAX_PATH_LENGTH];

	va_fd_t fdCatalog;
	struct catalogDB catalogDB;
	char backupFilePath[ABIO_FILE_LENGTH * 2];
	char tableSpaceName[ABIO_FILE_LENGTH];
	char backupTime[TIME_STAMP_LENGTH];

	struct oracleArchiveLog * archiveLogList;
	int archiveLogNumber;
	
	struct oracleTablespace * tablespaceList;
	int tablespaceNumber;
	
	struct oracleControlFile * controlFileList;
	int controlFileNumber;

	int i;
	int j;
	
	valueNumber = va_parser(cmd, value);
	
	if (CheckDataValidation_GET_ORACLE12C_BACKUP_SET_MODE(value) < 0)
	{
		memset(msg, 0, sizeof(msg));
		sprintf(msg, "%s\\invalid_format\\", value[0]);
		va_send(sock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE);
		
		return 0;
	}
	
	
	// initialize variables
	memset(catalogDBFile, 0, sizeof(catalogDBFile));

	
	// initialize variables

	archiveLogList = NULL;
	archiveLogNumber = 0;
	
	tablespaceList = NULL;
	tablespaceNumber = 0;
	
	controlFileList = NULL;
	controlFileNumber = 0;


	memset(msg, 0, sizeof(msg));
	sprintf(msg, "ORACLE12C_TABLESPACE_CATALOG_START\\start\\");
	if (va_send(sock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE) < 0)
	{
		return -1;
	}
	strcpy(catalogDBFile, value[150]);
	if ((fdCatalog = va_open(ABIOMASTER_CATALOG_DB_FOLDER, catalogDBFile , O_RDONLY, 1, 1)) != (va_fd_t)-1)
	{
		while (va_read_catalog_db(fdCatalog, &catalogDB, backupFilePath, sizeof(backupFilePath), tableSpaceName, sizeof(tableSpaceName), NULL, 0) == 0)
		{				
			if (!strcmp(value[91], ORACLE_RESTORE_FILE_LIST_OPTION_OBJECT_ARCHIVE_LOG))
			{
				if (catalogDB.st.filetype == ORACLE12C_ARCHIVE_LOG)
				{
					// archive log의 file name만을 얻는다.
					for (j = (int)strlen(backupFilePath); j >=0; j--)
					{
						if (backupFilePath[j] == DIRECTORY_IDENTIFIER)
						{
							break;
						}
					}
					
					// backup된 archive log를 archive log list에서 찾는다.
					for (i = 0; i < archiveLogNumber; i++)
					{
						if (!strcmp(archiveLogList[i].filepath, backupFilePath + j + 1))
						{
							break;
						}
					}
					
					// 기존 archive log list에 없는 것이면 추가한다.
					if (i == archiveLogNumber)
					{
						if (archiveLogNumber % DEFAULT_MEMORY_FRAGMENT == 0)
						{
							archiveLogList = (struct oracleArchiveLog *)realloc(archiveLogList, sizeof(struct oracleArchiveLog) * (archiveLogNumber + DEFAULT_MEMORY_FRAGMENT));
							memset(archiveLogList + archiveLogNumber, 0, sizeof(struct oracleArchiveLog) * DEFAULT_MEMORY_FRAGMENT);
						}
						strcpy(archiveLogList[archiveLogNumber].filepath, backupFilePath + j + 1);
						strcpy(archiveLogList[archiveLogNumber].backupset, catalogDB.backupset);
						archiveLogList[archiveLogNumber].size = catalogDB.st.size;
						archiveLogList[archiveLogNumber].mtime = catalogDB.st.mtime;
						archiveLogList[archiveLogNumber].backupTime = catalogDB.st.backupTime;
						archiveLogNumber++;
					}
					// 기존 archive log list에 있는 것이면 backupset을 교체한다.
					else
					{	
						memset(archiveLogList[i].backupset, 0, sizeof(archiveLogList[i].backupset));
						strcpy(archiveLogList[i].backupset, catalogDB.backupset);
					}
				}
			}
			else if (!strcmp(value[91], ORACLE_RESTORE_FILE_LIST_OPTION_OBJECT_CONTROL_FILE))
			{
				if (catalogDB.st.filetype == ORACLE12C_CONTROL_FILE)
				{
					// 한번의 백업 작업에 여러개의 control file이 백업될 수 있기 때문에, 기존 control file list에 있는 것인지 확인한다.
					for (i = 0; i < controlFileNumber; i++)
					{
						if (controlFileNumber != 0)
						{
							break;
						}
					/*	if (!strcmp(controlFileList[i].backupset, catalogDB.backupset))
						{
							break;
						}*/
					}
					
					// 기존 control file list에 없는 것이면 추가한다.
					if (i == controlFileNumber)
					{
						if (controlFileNumber % DEFAULT_MEMORY_FRAGMENT == 0)
						{
							controlFileList = (struct oracleControlFile *)realloc(controlFileList, sizeof(struct oracleControlFile) * (controlFileNumber + DEFAULT_MEMORY_FRAGMENT));
							memset(controlFileList + controlFileNumber, 0, sizeof(struct oracleControlFile) * DEFAULT_MEMORY_FRAGMENT);
						}
						strcpy(controlFileList[controlFileNumber].backupset, catalogDB.backupset);
						controlFileList[controlFileNumber].size = catalogDB.st.size;
						controlFileList[controlFileNumber].backupTime = catalogDB.st.backupTime;
						controlFileNumber++;
					}
					// 기존 control file list에 있는 것이면 control file의 크기를 증기시킨다.
					else
					{
						memset(controlFileList[i].backupset, 0 , sizeof(controlFileList[i].backupset));
						strcpy(controlFileList[i].backupset, catalogDB.backupset);
						controlFileList[i].size = catalogDB.st.size;
						controlFileList[i].backupTime = catalogDB.st.backupTime;
						//controlFileList[i].size += catalogDB.st.size;
					}
				}
			}
			else if (!strcmp(value[91], tableSpaceName))
			{
				if (catalogDB.st.filetype == ORACLE12C_REGULAR_DATA_FILE || catalogDB.st.filetype == ORACLE12C_RAW_DEVICE_DATA_FILE)
				{
					//// 기존 tablespace list에 있는 것인지 확인한다.
					//for (i = 0; i < tablespaceNumber; i++)
					//{
					//	if (!strcmp(tablespaceList[i].backupset, catalogDB.backupset))
					//	{
					//		break;
					//	}
					//}
					//
					// 기존 tablespace list에 없는 것이면 추가한다.
					for (i = 0; i < tablespaceNumber; i++)
					{
						if (!strcmp(tablespaceList[i].filepath, backupFilePath))
						{
							break;
						}
					}		

					if (i == tablespaceNumber)
					{
						if (tablespaceNumber % DEFAULT_MEMORY_FRAGMENT == 0)
						{
							tablespaceList = (struct oracleTablespace *)realloc(tablespaceList, sizeof(struct oracleTablespace) * (tablespaceNumber + DEFAULT_MEMORY_FRAGMENT));
							memset(tablespaceList + tablespaceNumber, 0, sizeof(struct oracleTablespace) * DEFAULT_MEMORY_FRAGMENT);
						}
						strcpy(tablespaceList[tablespaceNumber].backupset, catalogDB.backupset);
						strcpy(tablespaceList[tablespaceNumber].filepath, backupFilePath);	
						tablespaceList[tablespaceNumber].size = catalogDB.st.size;
						tablespaceList[tablespaceNumber].backupTime = catalogDB.st.backupTime;
						tablespaceNumber++;
					}
					// 기존 tablespace list에 있는 것이면 tablespace의 크기를 증가시킨다.
					else
					{
						memset(tablespaceList[i].backupset, 0, sizeof(tablespaceList[i].backupset));									
						strcpy(tablespaceList[i].backupset, catalogDB.backupset);
						tablespaceList[i].size = catalogDB.st.size;									
						tablespaceList[i].backupTime = catalogDB.st.backupTime;
					}
				}
			}
		}											
		va_close(fdCatalog);

	}
	if (!strcmp(value[91], ORACLE_RESTORE_FILE_LIST_OPTION_OBJECT_ARCHIVE_LOG))
	{
		qsort(archiveLogList, archiveLogNumber, sizeof(struct oracleArchiveLog), ComparatorOracleArchiveLogDescending);
		
		for (i = 0; i < archiveLogNumber; i++)
		{
			// set backup time
			memset(backupTime, 0, sizeof(backupTime));
			va_make_time_stamp((time_t)archiveLogList[i].backupTime, backupTime, TIME_STAMP_TYPE_INTERNAL);
			
			
			// make reply message
			va_init_reply_buf(reply);
			
			strcpy(reply[0], "ORACLE12C_TABLESPACE_CATALOG");
			
			strcpy(reply[91], archiveLogList[i].filepath);
			va_ulltoa(archiveLogList[i].size, reply[96]);
			strcpy(reply[150], archiveLogList[i].backupset);
			strcpy(reply[153], backupTime);
			
			va_make_reply_msg(reply, msg);
			if (va_send(sock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE) < 0)
			{
				break;
			}
		}
		
		va_free(archiveLogList);
	}
	else if (!strcmp(value[91], ORACLE_RESTORE_FILE_LIST_OPTION_OBJECT_CONTROL_FILE))
	{
		qsort(controlFileList, controlFileNumber, sizeof(struct oracleControlFile), ComparatorOracleControlFileDescending);
		
		for (i = 0; i < controlFileNumber; i++)
		{
			// set backup time
			memset(backupTime, 0, sizeof(backupTime));
			va_make_time_stamp((time_t)controlFileList[i].backupTime, backupTime, TIME_STAMP_TYPE_INTERNAL);
			
			
			// make reply message
			va_init_reply_buf(reply);
			
			strcpy(reply[0], "ORACLE12C_TABLESPACE_CATALOG");
			
			strcpy(reply[91], ORACLE_RESTORE_FILE_LIST_OPTION_OBJECT_FULL_BACKUP);
			va_ulltoa(controlFileList[i].size, reply[96]);
			strcpy(reply[150], controlFileList[i].backupset);
			strcpy(reply[153], backupTime);
			
			va_make_reply_msg(reply, msg);
			if (va_send(sock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE) < 0)
			{
				break;
			}
		}
		
		va_free(controlFileList);
	}
	else
	{
		qsort(tablespaceList, tablespaceNumber, sizeof(struct oracleTablespace), ComparatorOracleTablespaceDescending);
		
		for (i = 0; i < tablespaceNumber; i++)
		{
			// set backup time
			memset(backupTime, 0, sizeof(backupTime));
			va_make_time_stamp((time_t)tablespaceList[i].backupTime, backupTime, TIME_STAMP_TYPE_INTERNAL);
			
			
			// make reply message
			va_init_reply_buf(reply);
			
			strcpy(reply[0], "ORACLE12C_TABLESPACE_CATALOG");
			
			strcpy(reply[91], tablespaceList[i].filepath);
			va_ulltoa(tablespaceList[i].size, reply[96]);
			strcpy(reply[150], tablespaceList[i].backupset);
			strcpy(reply[153], backupTime);
			
			va_make_reply_msg(reply, msg);
			if (va_send(sock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE) < 0)
			{
				break;
			}
		}
		
		va_free(tablespaceList);
	}


	memset(msg, 0, sizeof(msg));
	sprintf(msg, "ORACLE12C_TABLESPACE_CATALOG_END\\end\\");
	va_send(sock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE);
	
	return 0;
}

static int CheckDataValidation_GET_ORACLE12C_CATALOG_DB(char value[DSIZ][DSIZ])
{
	// node name
	if (value[110][0] == '\0' || strlen(value[110]) > NODE_NAME_LENGTH - 1)
	{
		return -1;
	}
	
	// table space name
	if (value[91][0] == '\0')
	{
		return -1;
	}
	
	// sid
	if (value[204][0] == '\0' || strlen(value[204]) > ORACLE_SID_LENGTH - 1)
	{
		return -1;
	}
	
	return 0;
}

static int CheckDataValidation_GET_ORACLE12C_BACKUP_TABLESPACE_LIST(char value[DSIZ][DSIZ])
{
	// node name
	if (value[110][0] == '\0' || strlen(value[110]) > NODE_NAME_LENGTH - 1)
	{
		return -1;
	}
	
	// sid
	if (value[204][0] == '\0' || strlen(value[204]) > ORACLE_SID_LENGTH - 1)
	{
		return -1;
	}
	
	return 0;
}
static int CheckDataValidation_GET_ORACLE12C_FILE_LIST(char value[DSIZ][DSIZ])
{
	// node name
	if (value[110][0] == '\0' || strlen(value[110]) > NODE_NAME_LENGTH - 1)
	{
		return -1;
	}
	
	// user name
	if (value[201][0] == '\0' || strlen(value[201]) > ORACLE_USERNAME_LENGTH - 1)
	{
		return -1;
	}
	
	// account
	if (value[202][0] == '\0' || strlen(value[202]) > ORACLE_ACCOUNT_LENGTH - 1)
	{
		return -1;
	}
	
	// password
	if (value[203][0] == '\0' || strlen(value[203]) > ORACLE_PASSWORD_LENGTH - 1)
	{
		return -1;
	}
	
	// sid
	if (value[204][0] == '\0' || strlen(value[204]) > ORACLE_SID_LENGTH - 1)
	{
		return -1;
	}
	
	// home
	if (value[205][0] == '\0' || strlen(value[205]) > ORACLE_HOME_LENGTH - 1)
	{
		return -1;
	}
	
	if (value[205][0] != DIRECTORY_IDENTIFIER)
	{
		return -1;
	}
	
	if (value[205][strlen(value[205]) - 1] == DIRECTORY_IDENTIFIER)
	{
		return -1;
	}
	
	return 0;
}

static int CheckDataValidation_GET_ORACLE12C_BACKUP_SET_MODE(char value[DSIZ][DSIZ])
{
	// node name
	if (value[110][0] == '\0' || strlen(value[110]) > NODE_NAME_LENGTH - 1)
	{
		return -1;
	}
	
	// backupset name
	if (value[150][0] == '\0')
	{
		return -1;
	}	
	return 0;

}


