#include "require.h"
#include "abio.h"
#include "lib.h"
#include "vendor.h"
#include "httpd.h"

//#issue164
#ifdef __ABIO_AIX
#include <sys/vfs.h>
#include <sys/vmount.h>
#endif

// start of variables for abio library comparability
char ABIOMASTER_MASTER_SERVER_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_DATA_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_CATALOG_DB_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_VOLUME_DB_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_FILE_LIST_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_LOG_FOLDER[MAX_PATH_LENGTH];

char ABIOMASTER_SLAVE_DEDUP_DB_FOLDER[ABIO_FILE_LENGTH];

struct portRule * portRule;		// VIRBAK ABIO IP Table 목록
int portRuleNumber;				// VIRBAK ABIO IP Table 개수

int tapeDriveTimeOut;
int tapeDriveDelayTime;
// end of variables for abio library comparability


char ABIOMASTER_CK_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_CLIENT_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_CLIENT_CATALOG_DB_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_CLIENT_FILE_LIST_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_CLIENT_LOG_FOLDER[MAX_PATH_LENGTH];


struct ck command;
struct ckBackup commandData;


//////////////////////////////////////////////////////////////////////////////////////////////////
// ABIO log option
int requestLogLevel;
char logJobID[JOB_ID_LENGTH];
int logModuleNumber;


//////////////////////////////////////////////////////////////////////////////////////////////////
// message transfer variables
va_trans_t mid;		// message transfer id
int mtype;			// message type number


//////////////////////////////////////////////////////////////////////////////////////////////////
// ip and port number
char masterIP[IP_LENGTH];					// master server ip address
char masterPort[PORT_LENGTH];				// master server ck port
char masterNodename[NODE_NAME_LENGTH];		// master server node name
char masterLogPort[PORT_LENGTH];			// master server httpd logger port
char clientIP[IP_LENGTH];					// client ip address
char clientPort[PORT_LENGTH];				// client ck port
char clientNodename[NODE_NAME_LENGTH];		// client node name

int DebugLevel=0;


struct checkprocessitem * checkProcess;
int checkProcessList;
char ** processPIDList;
int processPIDListCount;

#ifdef __ABIO_WIN
	struct networkDriveConfig * networkDriveArray;
	int networkDriveCount;
#endif


#ifdef __ABIO_LINUX
char PATH_FILESYSTEM[MAX_PATH_LENGTH];		//PATH_FILESYSTEM
#endif

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
	
	int i;
	
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 1. 전역 변수들을 초기화한다.
	
	memset(ABIOMASTER_CLIENT_FOLDER, 0, sizeof(ABIOMASTER_CLIENT_FOLDER));
	memset(ABIOMASTER_CLIENT_CATALOG_DB_FOLDER, 0, sizeof(ABIOMASTER_CLIENT_CATALOG_DB_FOLDER));
	memset(ABIOMASTER_CLIENT_FILE_LIST_FOLDER, 0, sizeof(ABIOMASTER_CLIENT_FILE_LIST_FOLDER));
	memset(ABIOMASTER_CLIENT_LOG_FOLDER, 0, sizeof(ABIOMASTER_CLIENT_LOG_FOLDER));
	
	memset(ABIOMASTER_SLAVE_DEDUP_DB_FOLDER, 0, sizeof(ABIOMASTER_SLAVE_DEDUP_DB_FOLDER));
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// ABIO log option
	requestLogLevel = LOG_LEVEL_JOB;
	memset(logJobID, 0, sizeof(logJobID));
	logModuleNumber = MODULE_UNKNOWN;
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// ip and port number
	memset(masterIP, 0, sizeof(masterIP));
	memset(masterPort, 0, sizeof(masterPort));
	memset(masterNodename, 0, sizeof(masterNodename));
	memset(masterLogPort, 0, sizeof(masterLogPort));

	memset(clientIP, 0, sizeof(clientIP));
	memset(clientPort, 0, sizeof(clientPort));
	memset(clientNodename, 0, sizeof(clientNodename));
	
	#ifdef __ABIO_LINUX
	memset(PATH_FILESYSTEM, 0, sizeof(PATH_FILESYSTEM));
	strcpy(PATH_FILESYSTEM,_PATH_FSTAB);
	#endif
	
	
	#ifdef __ABIO_WIN
		networkDriveArray = NULL;
		networkDriveCount = 0;
	#endif
	
	checkProcess = NULL;
	checkProcessList = 0;
	
	processPIDList = NULL;
	processPIDListCount = 0;
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
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
	
	// set the ck directory
	for (i = (int)strlen(ABIOMASTER_CLIENT_FOLDER) - 1; i > 0; i--)
	{
		if (ABIOMASTER_CLIENT_FOLDER[i] == FILE_PATH_DELIMITER)
		{
			strncpy(ABIOMASTER_CK_FOLDER, ABIOMASTER_CLIENT_FOLDER, i + 1);
			strcat(ABIOMASTER_CK_FOLDER, "ck");
			
			break;
		}
	}
	
	// set module number for debug log of ABIO common library
	logModuleNumber = MODULE_CLIENT_HTTPD;
	
	// set master server ip and port
	strcpy(masterIP, command.sourceIP);
	strcpy(masterPort, command.sourcePort);
	strcpy(masterNodename, command.sourceNodename);
	strcpy(masterLogPort, commandData.client.masterLogPort);
	
	// set client ip and port
	strcpy(clientIP, commandData.client.ip);
	strcpy(clientPort, commandData.client.port);
	strcpy(clientNodename, commandData.client.nodename);


	#ifdef __ABIO_WIN
		// windows socket을 open한다.
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) 
		{
			return -1;
		}
	#endif
	
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
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
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 5. Network drive 연결
	
	#ifdef __ABIO_WIN
		for (i = 0; i < networkDriveCount; i++)
		{
			if(0 < DebugLevel)
			{
				WriteDebugData(ABIOMASTER_CLIENT_LOG_FOLDER,"netdrive.log","result = %d",va_add_network_drive_connection(networkDriveArray + i,DebugLevel,ABIOMASTER_CLIENT_LOG_FOLDER));
			}
			else
			{
				va_add_network_drive_connection(networkDriveArray + i,DebugLevel,ABIOMASTER_CLIENT_LOG_FOLDER);
			}
		}
	#endif

	return 0;
}

int GetModuleConfiguration()
{
	struct confOption * moduleConfig;
	int moduleConfigNumber;
	
	int i;

	#ifdef __ABIO_WIN
		int len;
		int j;
		int k;
		int l;
	#endif
	
	
	
	if (va_load_conf_file(ABIOMASTER_CLIENT_FOLDER, ABIOMASTER_CLIENT_CONFIG, &moduleConfig, &moduleConfigNumber) > 0)
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
			else if (!strcmp(moduleConfig[i].optionName, SLAVE_DEDUP_DB_FOLDER_OPTION_NAME))
			{
				strcpy(ABIOMASTER_SLAVE_DEDUP_DB_FOLDER, moduleConfig[i].optionValue);
			}
			else if (!strcmp(moduleConfig[i].optionName, ABIOMASTER_PORT_RULE_OPTION_NAME))
			{
				va_parse_port_rule(moduleConfig[i].optionValue, &portRule, &portRuleNumber);
			}
			else if (!strcmp(moduleConfig[i].optionName, DEBUG_LEVEL))
			{
				DebugLevel=atoi(moduleConfig[i].optionValue);
			}
			#ifdef __ABIO_LINUX
			else if (!strcmp(moduleConfig[i].optionName, "PATH_FILESYSTEM"))
			{
				strcpy(PATH_FILESYSTEM, moduleConfig[i].optionValue);
			}
			#endif
			
			#ifdef __ABIO_WIN
				else if (!strcmp(moduleConfig[i].optionName, CLIENT_NETWORK_DRIVE_OPTION_NAME))
				{
					len = (int)strlen(moduleConfig[i].optionValue);
					
					for (j = 0; j < len; j++)
					{
						if (moduleConfig[i].optionValue[j] == ':')
						{
							break;
						}
					}
					
					for (k = j + 1; k < len; k++)
					{
						if (moduleConfig[i].optionValue[k] == ':')
						{
							break;
						}
					}
					
					for (l = k + 1; l < len; l++)
					{
						if (moduleConfig[i].optionValue[l] == ':')
						{
							break;
						}
					}
					
					if (j + 1 < sizeof(networkDriveArray->localName) - 1 && 
						k - j - 1 < sizeof(networkDriveArray->remoteName) - 1 && 
						l - k - 1 < sizeof(networkDriveArray->username) - 1 && 
						len - l - 1 < sizeof(networkDriveArray->password) - 1 && 
						l < len)
					{
						if (networkDriveCount % DEFAULT_MEMORY_FRAGMENT == 0)
						{
							networkDriveArray = (struct networkDriveConfig *)realloc(networkDriveArray, sizeof(struct networkDriveConfig) * (networkDriveCount + DEFAULT_MEMORY_FRAGMENT));
							memset(networkDriveArray + networkDriveCount, 0, sizeof(struct networkDriveConfig) * DEFAULT_MEMORY_FRAGMENT);
						}
						
						strncpy(networkDriveArray[networkDriveCount].localName, moduleConfig[i].optionValue, j);
						networkDriveArray[networkDriveCount].localName[j] = ':';
						
						strncpy(networkDriveArray[networkDriveCount].remoteName, moduleConfig[i].optionValue + j + 1, k - j - 1);
						strncpy(networkDriveArray[networkDriveCount].username, moduleConfig[i].optionValue + k + 1, l - k - 1);
						strncpy(networkDriveArray[networkDriveCount].password, moduleConfig[i].optionValue + l + 1, len - l - 1);
						
						networkDriveCount++;
					}
				}
			#endif
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

void ResponseRequest()
{
	if(DebugLevel>0)
	{
		WriteDebugData(ABIOMASTER_CLIENT_LOG_FOLDER, ABIOMASTER_CLIENT_DEBUG, DEBUG_CODE_NORMAL " Command : %s " ,MODULE_NAME_CLIENT_HTTPD,__FUNCTION__ ,command.subCommand);
	}

	if (!strcmp(command.subCommand, "<GET_FILE_LIST>"))
	{
		GetFileList();
	}
	else if (!strcmp(command.subCommand, "<GET_DIRECTORY_LIST>"))
	{
		GetDirectoryList();
	}
	else if (!strcmp(command.subCommand, "<GET_FILESYSTEM_LIST>"))
	{
		GetFilesystemList();
	}
	else if (!strcmp(command.subCommand, "<PUT_FILE_LIST>"))
	{
		PutFileList();
	}
	else if (!strcmp(command.subCommand, "<MAKE_DIRECTORY>"))
	{
		MakeDirectory();
	}
	else if (!strcmp(command.subCommand, "<CREATE_FILE>"))
	{
		MakeFile();
	}
	else if (!strcmp(command.subCommand, "<REMOVE_FILE>"))
	{
		RemoveFile();
	}
	else if (!strcmp(command.subCommand, "<GET_DISK_INVENTORY_VOLUME_LIST>"))
	{
		GetDiskInventoryVolumeList();
	}
	else if (!strcmp(command.subCommand, "<GET_TAPE_LIBRARY_INVENTORY>"))
	{
		GetTapeLibraryInventory();
	}
	else if (!strcmp(command.subCommand, "<MOVE_MEDIUM>"))
	{
		MoveMedium();
	}
	else if (!strcmp(command.subCommand, "<CLEANING_MOVE_MEDIUM>"))
	{
		CleaningMoveMedium();
	}
	else if (!strcmp(command.subCommand, "<GET_NODE_MODULE>"))
	{
		GetNodeModule();
	}
	else if (!strcmp(command.subCommand, "<GET_MODULE_CONF>"))
	{		
		GetModuleConf();
	}
	else if (!strcmp(command.subCommand, "<UPDATE_MODULE_CONF>")
			|| !strcmp(command.subCommand, "<ADD_MODULE_CONF>")
			|| !strcmp(command.subCommand, "<DELETE_MODULE_CONF>"))
	{	
		SetModuleConf();
	}
	else if (!strcmp(command.subCommand, "<SET_SERVICE_ACTIVE>"))
	{		
		ServiceStart();
	}
	else if (!strcmp(command.subCommand, "<SET_SERVICE_INACTIVE>"))
	{		
		ServiceStop();
	}
	else if(!strcmp(command.subCommand, "<GET_CATALOG_INFO>"))
	{
		GetCatalogInfo();
	}
	else if (!strcmp(command.subCommand, "<GET_NODE_INFORMATION>"))
	{
		GetNodeInformation();
	}
	else if (!strcmp(command.subCommand, "<EXPIRE_BACKUPSET>"))
	{
		ExpireBackupset();
	}
	else if (!strcmp(command.subCommand, "<SHRINK_CLIENT_CATALOG>"))
	{
		ShrinkClientCatalog();
	}
	else if (!strcmp(command.subCommand, "<DELETE_SLAVE_CATALOG>"))
	{
		DeleteSlaveCatalog();
	}
	else if (!strcmp(command.subCommand, "<DELETE_SLAVE_LOG>"))
	{
		DeleteSlaveLog();
	}
	//ky88
	else if (!strcmp(command.subCommand, "<EXPIRE_NOTES_BACKUPSET>"))
	{
		ExpireNotesBackupset();
	}
	else if (!strcmp(command.subCommand, "<GET_ENTIRE_FILELIST>"))
	{
		GetEntireFileList();
	}
	else if (!strcmp(command.subCommand, "<GET_CHECK_PROCESS_USAGE>"))
	{
		GetCheckProcessUsage();
	}	
}


//ky88
void ExpireNotesBackupset()
{
	va_sock_t commandSock;
	
	char backupset[BACKUPSET_ID_LENGTH];
	char ** expiredBackupsetList;
	int expiredBackupsetListNumber;
	
	va_fd_t fdCatalogLatest;
	struct catalogDB catalogDB;
	int isValidBackupset;
	
	int i;
	
	
	
	// initialize variables
	expiredBackupsetList = NULL;
	expiredBackupsetListNumber = 0;
	
	
	if ((commandSock = va_make_client_socket_iptable(clientIP, masterIP, portRule, portRuleNumber, commandData.catalogPort,1)) != -1)
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
		
		// expire backup files in the expired backupset
		if ((fdCatalogLatest = va_open(ABIOMASTER_CLIENT_CATALOG_DB_FOLDER, NOTES_CATALOG_DB_LATEST, O_RDWR, 1, 0)) != (va_fd_t)-1)
		{
			while (va_read(fdCatalogLatest, &catalogDB, sizeof(struct catalogDB), DATA_TYPE_CATALOG_DB) > 0)
			{
				for (i = 0; i < expiredBackupsetListNumber; i++)
				{
					if (!strcmp(catalogDB.backupset, expiredBackupsetList[i]))
					{
						catalogDB.expire = 1;
						
						va_lseek(fdCatalogLatest, -(va_offset_t)sizeof(struct catalogDB), SEEK_CUR);
						va_write(fdCatalogLatest, &catalogDB, sizeof(struct catalogDB), DATA_TYPE_CATALOG_DB);
						
						break;
					}
				}
				
				va_lseek(fdCatalogLatest, (va_offset_t)(catalogDB.length + catalogDB.length2 + catalogDB.length3), SEEK_CUR);
			}
			
			
			// check whether the latest catalog db is expired or not.
			// the latest catalog db에 있는 모든 file이 expire되었으면 the latest catalog db가 expire되었다고 판단한다.
			isValidBackupset = 0;
			
			va_lseek(fdCatalogLatest, 0, SEEK_SET);
			while (va_read(fdCatalogLatest, &catalogDB, sizeof(struct catalogDB), DATA_TYPE_CATALOG_DB) > 0)
			{
				if (catalogDB.expire == 0)
				{
					isValidBackupset = 1;
					
					break;
				}
				
				va_lseek(fdCatalogLatest, (va_offset_t)(catalogDB.length + catalogDB.length2 + catalogDB.length3), SEEK_CUR);
			}
			
			va_close(fdCatalogLatest);
			
			
			// if the latest catalog db is expired, remove it
			if (isValidBackupset == 0)
			{
				va_remove(ABIOMASTER_CLIENT_CATALOG_DB_FOLDER, NOTES_CATALOG_DB_LATEST);
			}
		}
		
		
		for (i = 0; i < expiredBackupsetListNumber; i++)
		{
			va_free(expiredBackupsetList[i]);
		}
		va_free(expiredBackupsetList);
	}
}
int sendEntireFileList(va_sock_t commandSock,char* backupFolder, int multiSessionNumber)
{
	char folder[MAX_PATH_LENGTH];
	char ** filelist;
	int fileNumber;
	
	struct abio_file_stat file_stat;
	
	int filetype;
	
	int fileListNumber;

	int i;
	char tmpbuf[MAX_PATH_LENGTH];
	
	memset(tmpbuf,0,sizeof(tmpbuf));

	// initialize variables
	memset(folder, 0, sizeof(folder));
	filelist = NULL;
	fileNumber = 0;

	fileListNumber = multiSessionNumber;

	strcpy(folder, backupFolder);
		
	// windows의 경우 root file system을 의미하는 '/' 폴더가 없기 때문에 
	// root file system의 정보를 보내라고 요청하면 drive 정보를 읽어서 보내준다.
	#ifdef __ABIO_WIN
		if (!strcmp(folder, "/"))
		{
			GetRootFileList(commandSock);
			
			va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
			
			return -1;
		}
	#endif
	
	
	// get a sub file list in the directory
	#ifdef __ABIO_WIN
		va_slash_to_backslash(folder);
	#endif

	if ((fileNumber = va_scandir(folder, &filelist)) > 0)
	{
		
		fileListNumber += fileNumber;
		for (i = 0; i < fileNumber; i++)
		{
			va_lstat(folder, filelist[i], &file_stat);
							
			// set a file type by number
			filetype = GetFileTypeAbio(&file_stat);
							
			#ifdef __ABIO_WIN
				va_backslash_to_slash(folder);
			#endif

			memset(tmpbuf,0,sizeof(tmpbuf));
			if(filetype == 0)
			{
				//폴더.
				if(commandData.multiSession < fileListNumber)
				{
					sprintf(tmpbuf,"INCLUDE %s%s/\n",folder,filelist[i]);
				}
				else
				{				
					fileListNumber = sendEntireFileList(commandSock,va_strcpy("%s%s/", folder,filelist[i]),fileListNumber -1);
				}
			}
			else
			{
				sprintf(tmpbuf,"INCLUDE %s%s\n",folder,filelist[i]);
			}

			if (va_send(commandSock, tmpbuf, 1024, 0, DATA_TYPE_NOT_CHANGE) <= 0)
			{								
				break;
			}
		}
		
		
		for (i = 0; i < fileNumber; i++)
		{
			va_free(filelist[i]);
		}
		va_free(filelist);
	}

	return fileListNumber;
}
void GetEntireFileList()
{
	va_sock_t commandSock;	
	
	if ((commandSock = va_make_client_socket_iptable(clientIP, masterIP, portRule, portRuleNumber, commandData.catalogPort,1)) != -1)
	{		
		sendEntireFileList(commandSock,commandData.backupFile,0);
		
		va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
	}
}

void GetFileList()
{
	char reply[DSIZ][DSIZ];
	char msg[DSIZ * DSIZ];
	
	va_sock_t commandSock;
	
	char folder[MAX_PATH_LENGTH];
	char ** filelist;
	int fileNumber;
	
	struct abio_file_stat file_stat;
	
	char permission[PERMISSION_STRING_LENGTH];
	char mtime[TIME_STAMP_LENGTH];
	int filetype;
	
	int i;
	
	
	
	// initialize variables
	memset(folder, 0, sizeof(folder));
	filelist = NULL;
	fileNumber = 0;
	
	
	if ((commandSock = va_make_client_socket_iptable(clientIP, masterIP, portRule, portRuleNumber, commandData.catalogPort,1)) != -1)
	{
		strcpy(folder, commandData.backupFile);
		
		// windows의 경우 root file system을 의미하는 '/' 폴더가 없기 때문에 
		// root file system의 정보를 보내라고 요청하면 drive 정보를 읽어서 보내준다.
		#ifdef __ABIO_WIN
			if (!strcmp(folder, "/"))
			{
				GetRootFileList(commandSock);
				
				va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
				
				return;
			}
		#endif
		
		
		// get a sub file list in the directory
		#ifdef __ABIO_WIN
			va_slash_to_backslash(folder);
		#endif
		if ((fileNumber = va_scandir(folder, &filelist)) > 0)
		{
			for (i = 0; i < fileNumber; i++)
			{
				va_lstat(folder, filelist[i], &file_stat);
				
				// set a file permission
				memset(permission, 0, sizeof(permission));
				va_make_permission(file_stat.mode, file_stat.windowsAttribute, permission);
				
				// set the last modification time
				memset(mtime, 0, sizeof(mtime));
				va_make_time_stamp((time_t)file_stat.mtime, mtime, TIME_STAMP_TYPE_INTERNAL);
				
				// set a file type by number
				filetype = GetFileTypeAbio(&file_stat);
				
				// make a reply message
				va_init_reply_buf(reply);
				
				strcpy(reply[0], "FILE_LIST");
				va_itoa(filetype, reply[90]);
				strcpy(reply[91], filelist[i]);
				strcpy(reply[92], permission);
				va_ulltoa(file_stat.size, reply[96]);
				strcpy(reply[97], mtime);
				
				va_make_reply_msg(reply, msg);
				if (va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE) < 0)
				{
					break;
				}
			}
			
			
			for (i = 0; i < fileNumber; i++)
			{
				va_free(filelist[i]);
			}
			va_free(filelist);
		}
		
		va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
	}
}



void GetDirectoryList()
{
	char reply[DSIZ][DSIZ];
	char msg[DSIZ * DSIZ];
	
	va_sock_t commandSock;
	
	char folder[MAX_PATH_LENGTH];
	char ** filelist;
	int fileNumber;		// number of files in directory
	
	int i;
	
	
	
	// initialize variables
	memset(folder, 0, sizeof(folder));
	filelist = NULL;
	fileNumber = 0;
	
	
	if ((commandSock = va_make_client_socket_iptable(clientIP, masterIP, portRule, portRuleNumber, commandData.catalogPort,1)) != -1)
	{
		strcpy(folder, commandData.backupFile);
		
		// windows의 경우 root file system을 의미하는 '/' 폴더가 없기 때문에 
		// root file system의 정보를 보내라고 요청하면 drive 정보를 읽어서 보내준다.
		#ifdef __ABIO_WIN
			if (!strcmp(folder, "/"))
			{
				GetRootDirectoryList(commandSock);
				
				va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
				
				return;
			}
		#endif
		
		
		// get a sub folder list in the directory
		#ifdef __ABIO_WIN
			va_slash_to_backslash(folder);
		#endif
		if ((fileNumber = va_scandir_folder(folder, &filelist)) > 0)
		{
			for (i = 0; i < fileNumber; i++)
			{
				// make a reply message
				va_init_reply_buf(reply);
				
				strcpy(reply[0], "DIRECTORY_LIST");
				strcpy(reply[91], filelist[i]);
				
				va_make_reply_msg(reply, msg);
				if (va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE) < 0)
				{
					break;
				}
			}
			
			for (i = 0; i < fileNumber; i++)
			{
				va_free(filelist[i]);
			}
			va_free(filelist);
		}
		
		va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
	}
}

void GetFilesystemList()
{
	va_sock_t commandSock;
	
	
	
	if ((commandSock = va_make_client_socket_iptable(clientIP, masterIP, portRule, portRuleNumber, commandData.catalogPort,1)) != -1)
	{
		//#issue 64
		//RawDevice 윈도우 추가.
		#ifdef __ABIO_WIN
			GetFilesystemListWin(commandSock);
		#elif __ABIO_AIX
			GetFilesystemListAIX(commandSock);
		#elif __ABIO_SOLARIS
			GetFilesystemListSolaris(commandSock);
		#elif __ABIO_HP
			GetFilesystemListHP(commandSock);
		#elif __ABIO_LINUX
			GetFilesystemListLinux(commandSock);
		#else
			// 이외의 시스템에서는 raw device backup을 지원하지 않는다.
		#endif
		
		va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
	}
}

#ifdef __ABIO_WIN
	int GetFilesystemListWin(va_sock_t commandSock)
	{
		char reply[DSIZ][DSIZ];
		char msg[DSIZ * DSIZ];
		
		DWORD drives;
		int driveIndex;
		int driveType;
		
		char driveName[MAX_PATH_LENGTH];
		__int64 diskSize;
		
		
		
		driveIndex = 0;
		
		drives = GetLogicalDrives();
		
		while (drives)
		{
			if ((drives & 1) == 1)
			{
				driveType = va_get_windows_drive_type('A' + driveIndex);
				
				if (driveType != DRIVE_REMOVABLE && driveType != DRIVE_CDROM)
				{
					memset(driveName, 0, sizeof(driveName));
					//sprintf(driveName, "%c:\\", 'A' + driveIndex);
					sprintf(driveName, "%c:", 'A' + driveIndex);
					//diskSize = va_get_disk_free_space(driveName);
					//#issue 64
					//window 드라이브 전체용량 구하기
					//GetDiskFreeSpaceExA(드라이브명,남은용량,전체용량,남은용량)
					 if( ::GetDiskFreeSpaceExA(driveName,NULL,(PULARGE_INTEGER)&diskSize,NULL ) == FALSE )
					 {						 
						diskSize=0;
					 }
					
					// make a reply message
					va_init_reply_buf(reply);
					
					strcpy(reply[0], "FILESYSTEM_LIST");
					strcpy(reply[90], "0");
					//reply[91][0] = 'A' + driveIndex;
					strcpy(reply[91], driveName);
					strcpy(reply[98], "/");
					
					if (diskSize > 0)
					{
						va_lltoa(diskSize, reply[99]);
					}
					else
					{
						strcpy(reply[99], "0");
					}
					
					va_make_reply_msg(reply, msg);
					if (va_send(commandSock, msg, (int)strlen(msg) + 1 , 0, DATA_TYPE_NOT_CHANGE) < 0)
					{
						break;
					}
				}
			}
			
			driveIndex++;
			drives >>= 1;
		}
		
		memset(msg, 0, sizeof(msg));
		sprintf(msg, "FILE_LIST_END\\end\\");
		va_send(commandSock, msg, (int)strlen(msg) + 1 , 0, DATA_TYPE_NOT_CHANGE);
		
		
		return 0;
	}
#elif  __ABIO_AIX
	int GetFilesystemListAIX(va_sock_t commandSock)
	{
		//#issue 164
		// get vxfs file system

		char reply[DSIZ][DSIZ];
		char msg[DSIZ * DSIZ];
		struct fstab *fs;
		struct vfs_ent *fs_type;

		int size;
		char * buffer;
		char * mnt;
		struct vmount * vmount;

		time_t mountTime;
		char mtime[TIME_STAMP_LENGTH];

		struct statfs fsStatus;

		char fsname[40][10];

		memset(fsname, 0, sizeof(fsname));

		//fs type name setting
		setvfsent();
		while (fs_type = getvfsent())
		{
			strcpy(fsname[fs_type->vfsent_type], fs_type->vfsent_name);
		}
		endvfsent();

		if (mntctl(MCTL_QUERY, sizeof(size), (char *)&size) >= 0)
		{
			buffer = (char *)malloc(sizeof(char) * size);
			if (mntctl(MCTL_QUERY, size, (char *)buffer) >= 0)
			{
				for (mnt = buffer; mnt < buffer+size; mnt+=((struct vmount *)mnt)->vmt_length)
				{

					vmount = (struct vmount *)mnt;
					setfsent();
					while(fs = getfsent())
					{
						if(!strcmp(vmt2dataptr(vmount, VMT_STUB), fs->fs_file))
						{
							memset(&fsStatus, 0, sizeof(struct statfs));
							statfs(fs->fs_file, &fsStatus);
							
							mountTime = (time_t)vmount->vmt_time;
							memset(mtime, 0, sizeof(mtime));
							va_make_time_stamp(mountTime, mtime, TIME_STAMP_TYPE_INTERNAL);

							// make a reply message
							va_init_reply_buf(reply);
					
							strcpy(reply[0], "FILESYSTEM_LIST");
							strcpy(reply[91], fs->fs_spec);
							strcpy(reply[98], fs->fs_file);
							strcpy(reply[372], fsname[vmount->vmt_gfstype]);
				
							strcpy(reply[103], mtime);
									
							if (mountTime > 0)
							{
								va_itoa(fsStatus.f_bfree * (fsStatus.f_bsize/DSIZ), reply[100]);
							}
							else
							{
								strcpy(reply[100], "0");
							}

							va_make_reply_msg(reply, msg);
		
							if (va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE) < 0)
							{
								return -1;
							}
		
							break;
						}
					}
				}
			}
		}
		char reply[DSIZ][DSIZ];
		char msg[DSIZ * DSIZ];
		
		time_t mountTime;
		char mtime[TIME_STAMP_LENGTH];
		
		__uint64 fsSize;
		
		struct fstab * fs;
		struct statfs fsStatus;
		
		int size;
		char * buffer;
		char * mnt;
		struct vmount * vmount;
		
		struct lv_id lid;
		struct queryvgs * vgs;
		struct queryvg * vg;
		struct querylv * lv;
		struct querylv * lvList;
		int lvNumber;
		
		int i;
		int j;
		int k;
		
		
	
		// get number of logical volume
		lvNumber = 0;
		
		// get number of online volume group
		if (lvm_queryvgs(&vgs, 0) == 0)
		{
			// get volume group information
			for (i = 0; i < vgs->num_vgs; i++)
			{
				if (lvm_queryvg(&vgs->vgs[i].vg_id, &vg, NULL) == 0)
				{
					lvNumber += vg->num_lvs;
				}
			}
		}
		
		if (lvNumber == 0)
		{
			return 0;
		}
		
		
		if (lvNumber > 0)
		{
			// get logical volume list
			if (lvm_queryvgs(&vgs, 0) == 0)
			{
				lvList = (struct querylv *)malloc(sizeof(struct querylv) * (lvNumber));
				k = 0;
				
				// get volume group information
				for (i = 0; i < vgs->num_vgs; i++)
				{
					if (lvm_queryvg(&vgs->vgs[i].vg_id, &vg, NULL) == 0)
					{
						// get logical volume information
						lid.vg_id.word1 = vgs->vgs[i].vg_id.word1;
						lid.vg_id.word2 = vgs->vgs[i].vg_id.word2;
						lid.vg_id.word3 = vgs->vgs[i].vg_id.word3;
						lid.vg_id.word4 = vgs->vgs[i].vg_id.word4;
						
						for (j = 0; j < vg->num_lvs; j++)
						{
							lid.minor_num = vg->lvs[j].lv_id.minor_num;
							
							if (lvm_querylv(&lid, &lv, NULL) == 0)
							{
								memcpy(lvList + k, lv, sizeof(struct querylv));
								k++;
							}
						}
					}
				}
				
				// get mounted file system
				if (mntctl(MCTL_QUERY, sizeof(size), (char *)&size) >= 0)
				{
					buffer = (char *)malloc(sizeof(char) * size);
					if (mntctl(MCTL_QUERY, size, (char *)buffer) >= 0)
					{
						setfsent();
						while ((fs = getfsent()) != NULL)
						{
							for (i = 0; i < lvNumber; i++)
							{
								if (!strcmp(lvList[i].lvname, fs->fs_spec + 5))
								{
									memset(&fsStatus, 0, sizeof(struct statfs));
									statfs(fs->fs_file, &fsStatus);
									mountTime = 0;
									
									for (mnt = buffer; mnt < buffer+size; mnt+=((struct vmount *)mnt)->vmt_length)
									{
										vmount = (struct vmount *)mnt;
										
										if (!strcmp(fs->fs_file, vmt2dataptr(vmount, VMT_STUB)))
										{
											mountTime = (time_t)vmount->vmt_time;
											
											break;
										}
									}
									
									memset(mtime, 0, sizeof(mtime));
									va_make_time_stamp(mountTime, mtime, TIME_STAMP_TYPE_INTERNAL);
									
									
									// make a reply message
									va_init_reply_buf(reply);
									
									strcpy(reply[0], "FILESYSTEM_LIST");
									strcpy(reply[91], fs->fs_spec);
									strcpy(reply[98], fs->fs_file);
									
									fsSize = 1;
									for (j = 0; j < lvList[i].ppsize; j++)
									{
										fsSize *= 2;
									}
									fsSize *= lvList[i].currentsize;
									va_ulltoa(fsSize, reply[99]);
									
									strcpy(reply[103], mtime);
									
									if (mountTime > 0)
									{
										va_itoa(fsStatus.f_bfree * (fsStatus.f_bsize/DSIZ), reply[100]);
									}
									else
									{
										strcpy(reply[100], "0");
									}
									
									va_make_reply_msg(reply, msg);
									if (va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE) < 0)
									{
										va_free(buffer);
										va_free(lvList);
										
										return -1;
									}
									
									break;
								}
							}
						}
					}
					
					va_free(buffer);
					buffer = NULL;
				}
				
				va_free(lvList);
				lvList = NULL;
			}
		}
		*/
		
		return 0;
	}
#elif __ABIO_SOLARIS
	int GetFilesystemListSolaris(va_sock_t commandSock)
	{
		char reply[DSIZ][DSIZ];
		char msg[DSIZ * DSIZ];
		
		time_t mountTime;
		char mtime[TIME_STAMP_LENGTH];
		
		__uint64 fsSize;
		
		FILE * vfp;
		struct vfstab vfs;
		
		FILE * mfp;
		struct mnttab mnt;
		
		va_fd_t fd;
		struct fs fs;
		
		struct statvfs vfsStatus;
		
		int i;
		
		
		
		// get a file system list
		if ((vfp = va_fopen(NULL, VFSTAB, "r", 1)) != NULL)
		{
			while ((getvfsent(vfp, &vfs)) != -1)
			{
				if (vfs.vfs_fsckdev != NULL)
				{
					// read superblock
					fsSize = 0;
					if ((fd = va_open(NULL, vfs.vfs_special, O_RDONLY, 0, 1)) != (va_fd_t)-1)
					{
						va_lseek(fd, (va_offset_t)(1024 * 16), SEEK_SET);
						
						va_read(fd, &fs, sizeof(struct fs), DATA_TYPE_NOT_CHANGE);
						
						fsSize = fs.fs_size;
						
						// solaris의 경우 file system type이 ufs일 경우에만 이 값이 제대로 표시된다.
						// veritas file system이 설치되어서 vxfs일 경우에는 이 값이 표시가 되지 않는다.
						if (fs.fs_fsize != 0)
						{
							fsSize *= fs.fs_fsize;
						}
						
						va_close(fd);
					}
					
					// get a mounted file system list
					mountTime = 0;
				    if ((mfp = va_fopen(NULL, MNTTAB, "r", 1)) != NULL)
				    {
					    while (getmntent(mfp, &mnt) != -1)
					 	{	
						 	if (mnt.mnt_special != NULL && vfs.vfs_special != NULL)
						 	{
								if (!strcmp(mnt.mnt_special, vfs.vfs_special))
								{
									mountTime = (time_t)atol(mnt.mnt_time);
									
									break;
								}
							}
						}
						
						va_fclose(mfp);
					}
					
					memset(mtime, 0, sizeof(mtime));
					va_make_time_stamp(mountTime, mtime, TIME_STAMP_TYPE_INTERNAL);
					
					
					// make a reply message
					va_init_reply_buf(reply);
					
					strcpy(reply[0], "FILESYSTEM_LIST");
					strcpy(reply[91], vfs.vfs_special);
					if (vfs.vfs_mountp != NULL)
					{
						strcpy(reply[98], vfs.vfs_mountp);
					}
					va_ulltoa(fsSize, reply[99]);
					strcpy(reply[103], mtime);
					
					if (mountTime > 0 && vfs.vfs_mountp != NULL)
					{
						memset(&vfsStatus, 0, sizeof(struct statvfs));
						statvfs(vfs.vfs_mountp, &vfsStatus);
						
						strcpy(reply[100], "0");
					}
					else
					{
						strcpy(reply[100], "0");
					}
					
					va_make_reply_msg(reply, msg);
					if (va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE) < 0)
					{
						break;
					}
				}
			}
			
			va_fclose(vfp);
		}
		
		
		return 0;
	}
#elif __ABIO_HP
	int GetFilesystemListHP(va_sock_t commandSock)
	{
		char reply[DSIZ][DSIZ];
		char msg[DSIZ * DSIZ];
		
		time_t mountTime;
		char mtime[TIME_STAMP_LENGTH];
		
		__uint64 fsSize;
		
		FILE * fsp;
		struct mntent * fsentBuf;
		struct mntent fsent;
		
		FILE * mfp;
		struct mntent * mntent;
		
		va_fd_t fd;
		struct fs fs;
		
		struct statvfs vfsStatus;
		
		int i;
		
		
		
		fsent.mnt_fsname = (char *)malloc(sizeof(char) * (MAX_PATH_LENGTH));
		fsent.mnt_dir = (char *)malloc(sizeof(char) * (MAX_PATH_LENGTH));
		fsent.mnt_type = (char *)malloc(sizeof(char) * (MAX_PATH_LENGTH));
		fsent.mnt_opts = (char *)malloc(sizeof(char) * (MAX_PATH_LENGTH));
		
		// get filesystem information from filesystem entry
		if ((fsp = setmntent(MNT_CHECKLIST, "r")) != NULL)
		{
			while ((fsentBuf = getmntent(fsp)) != NULL)
			{
				CopyMntent(&fsent, fsentBuf);
				
				// read superblock
				fsSize = 0;
				if ((fd = va_open(NULL, fsent.mnt_fsname, O_RDONLY, 0, 1)) != (va_fd_t)-1)
				{
					va_lseek(fd, (va_offset_t)8192, SEEK_SET);
					
					va_read(fd, &fs, sizeof(struct fs), DATA_TYPE_NOT_CHANGE);
					
					fsSize = fs.fs_size;
					
					// hp의 경우 file system type이 hfs일 경우에만 이 값이 제대로 표시된다.
					// veritas file system이 설치되어서 vxfs일 경우에는 이 값이 표시가 되지 않는다.
					if (fs.fs_fsize != 0)
					{
						fsSize *= fs.fs_fsize;
					}
					
					va_close(fd);
				}
				
				// get a mounted file system list
				mountTime = 0;
				if ((mfp = setmntent(MNT_MNTTAB, "r")) != NULL)
				{
					while ((mntent = getmntent(mfp)) != NULL)
					{
						if (!strcmp(mntent->mnt_fsname, fsent.mnt_fsname))
						{
							mountTime = (time_t)mntent->mnt_time;
							
							break;
						}
					}
					
					endmntent(mfp);
				}
				
				memset(mtime, 0, sizeof(mtime));
				va_make_time_stamp(mountTime, mtime, TIME_STAMP_TYPE_INTERNAL);
				
				
				// make a reply message
				va_init_reply_buf(reply);
				
				strcpy(reply[0], "FILESYSTEM_LIST");
				strcpy(reply[91], fsent.mnt_fsname);
				strcpy(reply[98], fsent.mnt_dir);
				va_ulltoa(fsSize, reply[99]);
				strcpy(reply[103], mtime);
				
				if (mountTime > 0)
				{
					memset(&vfsStatus, 0, sizeof(struct statvfs));
					statvfs(fsent.mnt_dir, &vfsStatus);
					
					strcpy(reply[100], "0");
				}
				else
				{
					strcpy(reply[100], "0");
				}
				
				va_make_reply_msg(reply, msg);
				if (va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE) < 0)
				{
					break;
				}
			}
			
			endmntent(fsp);
		}
		
		
		va_free(fsent.mnt_fsname);
		va_free(fsent.mnt_dir);
		va_free(fsent.mnt_type);
		va_free(fsent.mnt_opts);
		
		
		return 0;
	}
	
	void CopyMntent(struct mntent * dest, struct mntent * src)
	{
		memset(dest->mnt_fsname, 0, MAX_PATH_LENGTH);
		memset(dest->mnt_dir, 0, MAX_PATH_LENGTH);
		memset(dest->mnt_type, 0, MAX_PATH_LENGTH);
		memset(dest->mnt_opts, 0, MAX_PATH_LENGTH);
		
		strcpy(dest->mnt_fsname, src->mnt_fsname);
		strcpy(dest->mnt_dir, src->mnt_dir);
		strcpy(dest->mnt_type, src->mnt_type);
		strcpy(dest->mnt_opts, src->mnt_opts);
		dest->mnt_freq = src->mnt_freq;
		dest->mnt_passno = src->mnt_passno;
		dest->mnt_time = src->mnt_time;
	}
#elif __ABIO_LINUX
	int GetFilesystemListLinux(va_sock_t commandSock)
	{
		char reply[DSIZ][DSIZ];
		char msg[DSIZ * DSIZ];
		
		time_t mountTime;
		char mtime[TIME_STAMP_LENGTH];
		
		__uint64 fsSize;
		
		FILE * fsp;
		struct mntent * fsentBuf;
		struct mntent fsent;
		
		FILE * mfp;
		struct mntent * mntent;
		
		int i;
		
		
		
		fsent.mnt_fsname = (char *)malloc(sizeof(char) * (MAX_PATH_LENGTH));
		fsent.mnt_dir = (char *)malloc(sizeof(char) * (MAX_PATH_LENGTH));
		fsent.mnt_type = (char *)malloc(sizeof(char) * (MAX_PATH_LENGTH));
		fsent.mnt_opts = (char *)malloc(sizeof(char) * (MAX_PATH_LENGTH));
		
		// get filesystem information from filesystem entry
		if ((fsp = setmntent(PATH_FILESYSTEM, "r")) != NULL)
		{
			while ((fsentBuf = getmntent(fsp)) != NULL)
			{
				fsSize = 0;
				
				CopyMntent(&fsent, fsentBuf);
				
				// get a mounted file system list
				mountTime = 0;
				if ((mfp = setmntent(_PATH_MOUNTED, "r")) != NULL)
				{
					while ((mntent = getmntent(mfp)) != NULL)
					{
						if (!strcmp(mntent->mnt_fsname, fsent.mnt_fsname))
						{
							break;
						}
					}
					
					endmntent(mfp);
				}
				
				memset(mtime, 0, sizeof(mtime));
				va_make_time_stamp(mountTime, mtime, TIME_STAMP_TYPE_INTERNAL);
				
				
				// make a reply message
				va_init_reply_buf(reply);
				
				strcpy(reply[0], "FILESYSTEM_LIST");
				strcpy(reply[91], fsent.mnt_fsname);
				strcpy(reply[98], fsent.mnt_dir);
				va_ulltoa(fsSize, reply[99]);
				strcpy(reply[103], mtime);
				
				strcpy(reply[100], "0");
				
				va_make_reply_msg(reply, msg);
				if (va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE) < 0)
				{
					break;
				}
			}
			
			endmntent(fsp);
		}
		
		
		va_free(fsent.mnt_fsname);
		va_free(fsent.mnt_dir);
		va_free(fsent.mnt_type);
		va_free(fsent.mnt_opts);
		
		
		return 0;
	}
	
	void CopyMntent(struct mntent * dest, struct mntent * src)
	{
		memset(dest->mnt_fsname, 0, MAX_PATH_LENGTH);
		memset(dest->mnt_dir, 0, MAX_PATH_LENGTH);
		memset(dest->mnt_type, 0, MAX_PATH_LENGTH);
		memset(dest->mnt_opts, 0, MAX_PATH_LENGTH);
		
		strcpy(dest->mnt_fsname, src->mnt_fsname);
		strcpy(dest->mnt_dir, src->mnt_dir);
		strcpy(dest->mnt_type, src->mnt_type);
		strcpy(dest->mnt_opts, src->mnt_opts);
		dest->mnt_freq = src->mnt_freq;
		dest->mnt_passno = src->mnt_passno;
	}
#endif

void PutFileList()
{
	va_sock_t commandSock;
	
	va_fd_t fdFilelist;
	
	char tmpbuf[DATA_BLOCK_SIZE];
	int recvSize;
	
	
	
	if ((commandSock = va_make_client_socket_iptable(clientIP, masterIP, portRule, portRuleNumber, commandData.catalogPort,1)) != -1)
	{
		// server에서 file list를 받는다.
		if ((fdFilelist = va_open(ABIOMASTER_CLIENT_FILE_LIST_FOLDER, commandData.filelist, O_CREAT|O_RDWR, 1, 1)) != (va_fd_t)-1)
		{
			while ((recvSize = va_recv(commandSock, tmpbuf, sizeof(tmpbuf), 0, DATA_TYPE_NOT_CHANGE)) > 0)
			{
				if (va_write(fdFilelist, tmpbuf, recvSize, DATA_TYPE_NOT_CHANGE) < 0)
				{
					break;
				}
			}
			
			va_close(fdFilelist);
		}
		
		va_close_socket(commandSock, ABIO_SOCKET_CLOSE_SERVER);
	}
}
void GetCheckProcessUsage()
{
	char reply[DSIZ][DSIZ];
	char msg[DSIZ * DSIZ];
	
	va_sock_t commandSock;
	
	char ** lines;
	int * linesLength;
	int lineNumber;
	int firstBlank;
	int secondBlank;
	char actionCommand[MAX_ACTION_COMMAND_VALUE_LENGTH];
	char commandName[MAX_ACTION_COMMAND_VALUE_LENGTH];
	

	int i;
	int j;
	int k;
	if ((lineNumber = va_load_text_file_lines(ABIOMASTER_CK_FOLDER, ACTION_LIST, &lines, &linesLength)) > 0)
	{
		for (i = 0; i < lineNumber; i++)
		{
			if (lines[i][0] == '\0')
			{
				continue;
			}

			memset(actionCommand, 0, sizeof(actionCommand));

			for (j = 0; j < linesLength[i]; j++)
			{
				if (lines[i][j] == ' ' || lines[i][j] == '\t')
				{
					firstBlank = j;
					break;
				}
			}

			for (j = linesLength[i] - 1; j > 0; j--)
			{
				if (lines[i][j] == ' ' || lines[i][j] == '\t')
				{
					secondBlank = j;
					break;
				}
			}
			strncpy(actionCommand, lines[i] + firstBlank + 1, secondBlank - firstBlank - 1);
			
			memset(commandName, 0, sizeof(commandName));
			
			for (k = j; k > 0; k--)
			{
				if (actionCommand[k] == FILE_PATH_DELIMITER)
				{
					strncpy(commandName, actionCommand + k + 1, j - k - 1);
					break;
				}
			}
			#if defined(__ABIO_AIX) || defined(__ABIO_SOLARIS) || defined(__ABIO_HP)
			if(GetProcessPID(commandName) == 0)
			{
				
			}	
			
			#elif defined(__ABIO_LINUX) || defined(__ABIO_WIN) 
			ExecuteGetProcessUasge(commandName);
			
			#endif
		}
		for (i = 0; i < lineNumber; i++)
		{
			va_free(lines[i]);
		}
		va_free(lines);
		va_free(linesLength);		
	}
	#if defined(__ABIO_AIX) || defined(__ABIO_SOLARIS)  || defined(__ABIO_HP)
		ExcuteGetProcessUsageAIX();
	#endif

	if ((commandSock = va_make_client_socket_iptable(clientIP, masterIP, portRule, portRuleNumber, commandData.catalogPort,1)) != -1)
	{
		for (i = 0; i < checkProcessList; i++)
		{
			va_init_reply_buf(reply);			
			strcpy(reply[0], "CHECK_PROCESS_USAGE");
			strcpy(reply[120], checkProcess[i].cpuusage);
			strcpy(reply[121], checkProcess[i].memusage);
			strcpy(reply[122], checkProcess[i].processname);
			memset(msg, 0, sizeof(msg));
			va_make_reply_msg(reply, msg);
			va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE);
		}
		va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
	}

	
}
#ifdef __ABIO_UNIX 
int GetProcessPID(char* abioCommand)
{
	char ** lines;
	int * linesLength;
	int lineNumber;

	char processpid[PROCESS_USAGE];

	char argQueryFile[MAX_PATH_LENGTH];
	char outputFilePath[MAX_PATH_LENGTH];


	int i;
	int j;
	int r;
	int firstBlank;
	int secondBlank;
	r = 0;

	if((int)va_open(ABIOMASTER_CLIENT_FOLDER, GETPROCESSPID, O_RDONLY,0,0) == -1)
	{
		CreateCheckProcess(GETPROCESSPID);
	}
	
	memset(argQueryFile, 0, sizeof(argQueryFile));
	memset(outputFilePath, 0, sizeof(outputFilePath));


	sprintf(argQueryFile, "%s%c%s", ABIOMASTER_CLIENT_FOLDER, FILE_PATH_DELIMITER, GETPROCESSPID);	
	sprintf(outputFilePath, "%s%c%s", ABIOMASTER_CLIENT_FOLDER, FILE_PATH_DELIMITER, CheckProcessOutPutFile);

	r = va_spawn_process(WAIT_PROCESS,"root", "sh", argQueryFile, abioCommand, outputFilePath, NULL);
	
	if ( r != 0 )
	{
		va_remove(ABIOMASTER_CLIENT_FOLDER, CheckProcessOutPutFile);
		return -1;
	}


	if ((lineNumber = va_load_text_file_lines(ABIOMASTER_CLIENT_FOLDER, CheckProcessOutPutFile, &lines, &linesLength)) > 0)
	{
		for (i = 0; i < lineNumber; i++)
		{
			if (lines[i][0] == '\0')
			{
				continue;
			}
			memset(processpid, 0, sizeof(processpid));
			for (j = 0; j < linesLength[i]; j++)
			{
				if (lines[i][j] == ' ' || lines[i][j] == '\t')					
				{
					continue;						
				}
				else
				{
					firstBlank = j;
					break;
				}
			}
			for (j = j + 1; j < linesLength[i]; j++)
			{
				if (lines[i][j] == ' ' || lines[i][j] == '\t')
				{
					secondBlank = j;
					break;
				}
			}
			strncpy(processpid, lines[i] + firstBlank , secondBlank - firstBlank );
			processPIDList = (char **)realloc(processPIDList, sizeof(char *) * (processPIDListCount + 1));
			processPIDList[processPIDListCount] = (char *)malloc(sizeof(char) * (strlen(processpid) + 1));
			memset(processPIDList[processPIDListCount], 0, sizeof(char) * (strlen(processpid) + 1));
			strcpy(processPIDList[processPIDListCount], processpid);
			
			processPIDListCount++;
		}
		for (i = 0; i < lineNumber; i++)
		{
			va_free(lines[i]);
		}
		va_free(lines);
		va_free(linesLength);	
	}

	va_remove(ABIOMASTER_CLIENT_FOLDER, CheckProcessOutPutFile);
	return 0;
}
void ExcuteGetProcessUsageAIX()
{
	char argQueryFile[MAX_PATH_LENGTH];
	char outputFilePath[MAX_PATH_LENGTH];
	int r;
	int i;

	for(i = 0; i < processPIDListCount; i++)
	{
		memset(argQueryFile, 0, sizeof(argQueryFile));
		memset(outputFilePath, 0, sizeof(outputFilePath));

		if((int)va_open(ABIOMASTER_CLIENT_FOLDER, CheckProcessUsage, O_RDONLY,0,0) == -1)
		{
			CreateCheckProcess(CheckProcessUsage);
		}

		sprintf(argQueryFile, "%s%c%s", ABIOMASTER_CLIENT_FOLDER, FILE_PATH_DELIMITER, CheckProcessUsage);	
		sprintf(outputFilePath, "%s%c%s", ABIOMASTER_CLIENT_FOLDER, FILE_PATH_DELIMITER, CheckProcessOutPutFile);
					
		r = va_spawn_process(WAIT_PROCESS,"root", "sh", argQueryFile, processPIDList[i], outputFilePath, NULL);
			
				
		if ( r == 0 )
		{
			LoadOutPutFile();
			
		}
		va_remove(ABIOMASTER_CLIENT_FOLDER, CheckProcessOutPutFile);
	}
		
}
#endif
void LoadOutPutFile()
{

	char ** lines;
	int * linesLength;
	int lineNumber;

	char cpuResult[PROCESS_USAGE];
	char memResult[ABIO_FILE_LENGTH];
	char processname[ABIO_FILE_LENGTH];

	int firstBlank;
	int secondBlank;
	int thirdBlank;
	int fourthBlank;
	int fifthBlank;
	int i;
	int j;
	int k;

	#ifdef __ABIO_WIN
		i = 0;
	#else
		i = 1;
	#endif


	if ((lineNumber = va_load_text_file_lines(ABIOMASTER_CLIENT_FOLDER, CheckProcessOutPutFile, &lines, &linesLength)) > 0)
	{
		for (i = i; i < lineNumber; i++)
		{
			if (lines[i][0] == '\0')
			{
				continue;
			}

			memset(cpuResult, 0, sizeof(cpuResult));
			memset(memResult, 0, sizeof(memResult));
			memset(processname, 0, sizeof(processname));

			for (j = 0; j < linesLength[i]; j++)
			{
				if (lines[i][j] == ' ' || lines[i][j] == '\t')					
				{
					continue;						
				}
				else
				{
					firstBlank = j;
					break;
				}				
			}
			for (j = j + 1; j < linesLength[i]; j++)
			{
				if (lines[i][j] == ' ' || lines[i][j] == '\t')					
				{
					secondBlank = j;
					break;
				}				
			}
			for (j = j + 1; j < linesLength[i]; j++)
			{
				if (lines[i][j] == ' ' || lines[i][j] == '\t')					
				{
					continue;						
				}
				else
				{
					thirdBlank = j;
					break;
				}
			}
			for (j = j + 1; j < linesLength[i]; j++)
			{
				if (lines[i][j] == ' ' || lines[i][j] == '\t')					
				{
					fourthBlank = j;
					break;
				}				
			}
			
			for (j = j + 1; j < linesLength[i]; j++)
			{
				if (lines[i][j] == ' ' || lines[i][j] == '\t')					
				{
					continue;						
				}
				else
				{
					fifthBlank = j;
					break;
				}
			}
			
			#ifdef __ABIO_WIN
				strncpy(processname, lines[i] + firstBlank, secondBlank - firstBlank);
				strncpy(cpuResult, lines[i] + thirdBlank, fourthBlank - thirdBlank);
				strcpy(memResult, lines[i] + fifthBlank);
			#else
				strncpy(cpuResult, lines[i] + firstBlank, secondBlank - firstBlank);
				strncpy(memResult, lines[i] + thirdBlank, fourthBlank - thirdBlank);
				strcpy(processname, lines[i] + fifthBlank);
			#endif

			
			
			for (k = 0; k < checkProcessList; k++)
			{
				if (!strcmp(checkProcess[k].processname, processname))
				{
					break;
				}
			}
			if (k == checkProcessList)
			{
				if (checkProcessList % DEFAULT_MEMORY_FRAGMENT == 0)
				{
					checkProcess = (struct checkprocessitem *)realloc(checkProcess, sizeof(struct checkprocessitem) * (checkProcessList + DEFAULT_MEMORY_FRAGMENT));
					memset(checkProcess + checkProcessList, 0, sizeof(struct checkprocessitem) * DEFAULT_MEMORY_FRAGMENT);
				}
				strcpy(checkProcess[checkProcessList].cpuusage, cpuResult);
				strcpy(checkProcess[checkProcessList].memusage, memResult);
				strcpy(checkProcess[checkProcessList].processname, processname);			
	
				checkProcessList++;
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
void ExecuteGetProcessUasge(char *commandName)
{
	char argQueryFile[MAX_PATH_LENGTH];
	char outputFilePath[MAX_PATH_LENGTH];
	int r;

	char serviceName[MAX_ACTION_COMMAND_VALUE_LENGTH];
	int findServiceIndex;
	memset(argQueryFile, 0, sizeof(argQueryFile));
	memset(outputFilePath, 0, sizeof(outputFilePath));

	if((int)va_open(ABIOMASTER_CLIENT_FOLDER, CheckProcessUsage, O_RDONLY,0,0) == -1)
	{
		CreateCheckProcess(CheckProcessUsage);
	}

	sprintf(argQueryFile, "%s%c%s", ABIOMASTER_CLIENT_FOLDER, FILE_PATH_DELIMITER, CheckProcessUsage);	
	sprintf(outputFilePath, "%s%c%s", ABIOMASTER_CLIENT_FOLDER, FILE_PATH_DELIMITER, CheckProcessOutPutFile);
	#ifdef __ABIO_WIN
		memset(serviceName, 0 ,sizeof(serviceName));
		
		for(findServiceIndex = 0; findServiceIndex < (int)strlen(commandName); findServiceIndex++)
		{			
			
			if(commandName[findServiceIndex] == '.')
			{
				strncpy(serviceName, commandName, findServiceIndex);

				break;
			}
		}
		r = va_spawn_process(WAIT_PROCESS, "cmd", "/C", argQueryFile, serviceName, outputFilePath, NULL);

	#else
		r = va_spawn_process(WAIT_PROCESS,"root", "sh", argQueryFile, commandName, outputFilePath, NULL);
	#endif
	if ( r == 0 )
	{
		LoadOutPutFile();
		
	}
	va_remove(ABIOMASTER_CLIENT_FOLDER, CheckProcessOutPutFile);		
	
}
void CreateCheckProcess(char * fileName)
{
	char commandmsg[DSIZ*2];
	va_fd_t fd;	

	memset(commandmsg, 0 , sizeof(commandmsg));

	fd = va_open(ABIOMASTER_CLIENT_FOLDER,fileName, O_CREAT|O_RDWR,0,0);	

	if (strcmp(fileName, CheckProcessUsage) == 0)
	{

		#if defined(__ABIO_AIX) || defined(__ABIO_SOLARIS)  || defined(__ABIO_HP)
			strcpy(commandmsg, "ps -p $1 -o pcpu,pmem,args > $2");		
		#elif __ABIO_LINUX
			strcpy(commandmsg, "ps -C $1 -o %cpu,%mem,cmd > $2");
		#elif __ABIO_WIN
			strcpy(commandmsg, "wmic path Win32_PerfFormattedData_PerfProc_Process get Name,PercentProcessorTime,WorkingSet | findstr /i %1 > %2");
		#endif
	}
	#ifdef __ABIO_UNIX
	else if(strcmp(fileName, GETPROCESSPID) == 0)
	{
		strcpy(commandmsg, "ps -e|grep $1 > $2");
	}
	#endif
	
	va_write(fd,commandmsg,(int)strlen(commandmsg),DATA_TYPE_NOT_CHANGE);	
	va_close(fd);


}	
void GetDiskInventoryVolumeList()
{
	char reply[DSIZ][DSIZ];
	char msg[DSIZ * DSIZ];
	
	va_sock_t commandSock;
	
	char ** filelist;
	int fileNumber;
	
	va_fd_t fdDiskVolume;
	char * tapeHeader;
	struct volumeDB volumeDB;
	
	int i;
	
	
	
	// initialize variables
	filelist = NULL;
	fileNumber = 0;
	
	tapeHeader = (char *)malloc(sizeof(char) * commandData.mountStatus.blockSize);
	memset(tapeHeader, 0, sizeof(char) * commandData.mountStatus.blockSize);
	
	
	if ((commandSock = va_make_client_socket_iptable(clientIP, masterIP, portRule, portRuleNumber, commandData.catalogPort,1)) != -1)
	{
		// repository의 파일들을 읽어와서 ABIO에서 만든 disk volume인지 확인하고, 맞으면 정보를 master server로 전송한다.
		if ((fileNumber = va_scandir(commandData.mountStatus.repository, &filelist)) > 0)
		{
			for (i = 0; i < fileNumber; i++)
			{
				// 파일 이름이 ABIO의 disk volume name prefix인지 확인한다.
				if (!strncmp(filelist[i], commandData.mountStatus.diskPrefix, strlen(ABIOMASTER_DISK_VOLUME_NAME_PREFIX)))
				{
					// disk volume의 volume header를 읽어서 ABIO에서 기록한 disk volume이 맞는지 확인한다.
					if ((fdDiskVolume = va_open(commandData.mountStatus.repository, filelist[i], O_RDONLY, 1, 1)) != (va_fd_t)-1)
					{
						memset(&volumeDB, 0, sizeof(struct volumeDB));
						
						if (va_read(fdDiskVolume, tapeHeader, commandData.mountStatus.blockSize, DATA_TYPE_NOT_CHANGE) == commandData.mountStatus.blockSize)
						{
							memcpy(&volumeDB, tapeHeader, sizeof(struct volumeDB));
							va_change_byte_order_volumeDB(&volumeDB);		// network to host
						}
						
						va_close(fdDiskVolume);
						
						
						// ABIO에서 기록한 disk volume이 맞으면 정보를 master server로 전송한다.
						if (va_is_abio_volume(&volumeDB) == 1 && !strcmp(volumeDB.volume, filelist[i]))
						{
							// make a reply message
							va_init_reply_buf(reply);
							
							strcpy(reply[0], "DISK_INVENTORY_VOLUME_LIST");
							strcpy(reply[40], volumeDB.volume);
							
							va_make_reply_msg(reply, msg);
							if (va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE) < 0)
							{
								break;
							}
						}
					}
				}
			}
			
			for (i = 0; i < fileNumber; i++)
			{
				va_free(filelist[i]);
			}
			va_free(filelist);
		}
		
		va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
	}
	
	
	va_free(tapeHeader);
}

void MakeDirectory()
{
	va_sock_t commandSock;
	
	int result;
	
	
	
	if ((commandSock = va_make_client_socket_iptable(clientIP, masterIP, portRule, portRuleNumber, commandData.catalogPort,1)) != -1)
	{
		// directory를 만든다.
		result = va_make_directory(commandData.backupFile, 0, &commandData.mountStatus.networkDrive);
		
		if (result == -1)
		{
			va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_MakeDirectory, ERROR_FILE_INVALID_FORMAT, &commandData);
		}
		else if (result == -2)
		{
			va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_MakeDirectory, ERROR_FILE_MAKE_DIRECTORY, &commandData);
		}
		else if (result == -3)
		{
			va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_MakeDirectory, ERROR_VOLUME_DISK_CONNECT_NETWORK_DRIVE, &commandData);
		}
		
		// directory를 만든 결과를 전달한다.
		va_send(commandSock, &commandData, sizeof(struct ckBackup), 0, DATA_TYPE_CK_BACKUP);
		va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
	}
}

void MakeFile()
{
	va_sock_t commandSock;
	
	va_fd_t fdFile;
	char directory[MAX_PATH_LENGTH];
	char filename[MAX_PATH_LENGTH];
	struct abio_file_stat fileStatus;
	
	__int64 fsFreeSize;
	
	char dedupIndexFileName[MAX_PATH_LENGTH];
	char dedupDbFileName[MAX_PATH_LENGTH];
	
	// initialize variables
	memset(directory, 0, sizeof(directory));
	memset(filename, 0, sizeof(filename));

	if ((commandSock = va_make_client_socket_iptable(clientIP, masterIP, portRule, portRuleNumber, commandData.catalogPort,1)) != -1)
	{
		va_split_path(commandData.backupFile, directory, filename);
		
		
		if (directory[0] == '\0' || filename[0] == '\0')
		{
			va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_CreateFile, ERROR_FILE_INVALID_FORMAT, &commandData);
		}
		else
		{
			// 만들려는 파일이 시스템에 없으면 새로 만든다.
			if (va_lstat(directory, filename, &fileStatus) != 0)
			{
				#ifdef __ABIO_WIN
					// directory가 network drive에 있으면 network drive connection을 먼저 한다.
					if (commandData.mountStatus.networkDrive.localName[0] != '\0')
					{
						if (va_add_network_drive_connection(&commandData.mountStatus.networkDrive,DebugLevel,ABIOMASTER_CLIENT_LOG_FOLDER) != 0)
						{
							va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_CreateFile, ERROR_VOLUME_DISK_CONNECT_NETWORK_DRIVE, &commandData);
						}
					}
				#endif
				
				// directory가 있는지 확인
				if (va_lstat(NULL, directory, &fileStatus) != 0)
				{
					va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_CreateFile, ERROR_FILE_NO_DIRECTORY, &commandData);
				}
				
				// 만들려는 파일의 크기를 지정한 경우 disk에 파일을 만들 공간이 있는지 확인한다.
				if (!GET_ERROR(commandData.errorCode))
				{
					if (commandData.capacity > 0)
					{
						if ((fsFreeSize = va_get_disk_free_space(directory)) >= 0)
						{
							if ((__uint64)fsFreeSize < commandData.capacity)
							{
								va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_CreateFile, ERROR_RESOURCE_NOT_ENOUGH_DISK_SPACE, &commandData);
							}
						}
						else
						{
							if (fsFreeSize == -1)
							{
								va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_CreateFile, ERROR_FILE_INVALID_FORMAT, &commandData);
							}
							else if (fsFreeSize == -2)
							{
								va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_CreateFile, ERROR_RESOURCE_GET_DISK_INFORMAION, &commandData);
							}
						}
					}
				}

				// 파일을 만든다
				//#issue88 ~ dedup disk 일때 index파일도 만든다, 두 파일모두 capacity 크기로 생성되는게 아닌, 0byte부터 증가된다.
				if(commandData.dedup != 0)
				{
					if (!GET_ERROR(commandData.errorCode))
					{
						if ((fdFile = va_open(directory, filename, O_CREAT|O_RDWR, 0, 1)) != (va_fd_t)-1)
						{
							va_close(fdFile);
							
							if (GET_ERROR(commandData.errorCode))
							{
								va_remove(directory, filename);
							}
						}
						else
						{
							va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_CreateFile, ERROR_FILE_CREATE_FILE, &commandData);
						}
					}

					memset(dedupIndexFileName, 0, MAX_PATH_LENGTH);
					sprintf(dedupIndexFileName, "%s%s", filename, ".deidx");
					if (!GET_ERROR(commandData.errorCode))
					{
						if ((fdFile = va_open(directory, dedupIndexFileName, O_CREAT|O_RDWR, 0, 1)) != (va_fd_t)-1)
						{							
							va_close(fdFile);
							
							if (GET_ERROR(commandData.errorCode))
							{
								va_remove(directory, dedupIndexFileName);
							}
						}
						else
						{
							va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_CreateFile, ERROR_FILE_CREATE_FILE, &commandData);
						}
					}

					//ABIOMASTER_SLAVE_DEDUP_DB_FOLDER
					if(!strlen(ABIOMASTER_SLAVE_DEDUP_DB_FOLDER))
					{
						strcpy(ABIOMASTER_SLAVE_DEDUP_DB_FOLDER, directory);
					}

					memset(dedupDbFileName, 0, MAX_PATH_LENGTH);
					sprintf(dedupDbFileName, "%s%s", filename, ".db");
					if (!GET_ERROR(commandData.errorCode))
					{
						if(commandData.dedup == 1)
						{
							if ((fdFile = va_open(ABIOMASTER_SLAVE_DEDUP_DB_FOLDER, dedupDbFileName, O_CREAT|O_RDWR, 0, 1)) != (va_fd_t)-1)
							{							
								va_close(fdFile);
								
								if (GET_ERROR(commandData.errorCode))
								{
									va_remove(ABIOMASTER_SLAVE_DEDUP_DB_FOLDER, dedupDbFileName);
								}
							}
							else
							{
								va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_CreateFile, ERROR_FILE_CREATE_FILE, &commandData);
							}
						}
					}
				}
				else
				{
					if (!GET_ERROR(commandData.errorCode))
					{
						if ((fdFile = va_open(directory, filename, O_CREAT|O_RDWR, 0, 1)) != (va_fd_t)-1)
						{
							if (commandData.capacity > 0)
							{
								if (va_ftruncate(fdFile, (va_offset_t)commandData.capacity) != 0)
								{
									//s3fs 정책변경된걸로 추측. truncate로 1기가 티스크를 생성하지 못함. 0byte부터 백업 진행.
									va_write_error_code(ERROR_LEVEL_WARNING, MODULE_CLIENT_HTTPD, FUNCTION_CreateFile, ERROR_FILE_TRUNCATE, &commandData);
								}
							}
							
							va_close(fdFile);
							
							if (GET_ERROR(commandData.errorCode))
							{
								va_remove(directory, filename);
							}
						}
						else
						{
							va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_CreateFile, ERROR_FILE_CREATE_FILE, &commandData);
						}
					}
				}
				
				#ifdef __ABIO_WIN
					// network drive에 연결되어 있으면 network drive connection을 끊는다.
					if (commandData.mountStatus.networkDrive.localName[0] != '\0')
					{
						va_cancel_network_drive_connection(&commandData.mountStatus.networkDrive);
					}
				#endif
			}
		}
		
		// 파일을 만든 결과를 전달한다.
		va_send(commandSock, &commandData, sizeof(struct ckBackup), 0, DATA_TYPE_CK_BACKUP);
		va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
	}
	
}

void RemoveFile()
{
	struct abio_file_stat fileStatus;

	va_sock_t commandSock;
	
	char directory[MAX_PATH_LENGTH];
	char filename[MAX_PATH_LENGTH];

	va_fd_t fdFile;
	char dedupIndexFileName[MAX_PATH_LENGTH];
	char dedupDbFileName[MAX_PATH_LENGTH];
	
	
	
	// initialize variables
	memset(directory, 0, sizeof(directory));
	memset(filename, 0, sizeof(filename));
	
	
	if ((commandSock = va_make_client_socket_iptable(clientIP, masterIP, portRule, portRuleNumber, commandData.catalogPort,1)) != -1)
	{
		if(!strncmp(commandData.backupFile, "DEDUP_DATABASE", strlen("DEDUP_DATABASE")))
		{
			strcpy(directory, ABIOMASTER_CLIENT_CATALOG_DB_FOLDER);
			strcpy(filename, commandData.backupFile+strlen("DEDUP_DATABASE")+1);
			sprintf(filename, "%s%s", filename, ".db");
		}
		else
		{
			va_split_path(commandData.backupFile, directory, filename);
		}
		// 파일을 삭제한다.
		if (directory[0] != '\0' && filename[0] != '\0')
		{
			#ifdef __ABIO_UNIX
			// backup file list에 지정된 파일이 실제 존재하는지 확인한다.
 			if (va_lstat(directory, filename, &fileStatus) == 0)
			{
				if (va_is_directory(fileStatus.mode, fileStatus.windowsAttribute))
				{
					va_remove_all_directory(directory, filename);
				}
				else
				{
					va_remove(directory, filename);
				}
			}
			#elif __ABIO_WIN
				// directory가 network drive에 있으면 network drive connection을 먼저 한다.
				if (commandData.mountStatus.networkDrive.localName[0] != '\0')
				{
					if (va_add_network_drive_connection(&commandData.mountStatus.networkDrive,DebugLevel,ABIOMASTER_CLIENT_LOG_FOLDER) == 0)
					{
						va_remove(directory, filename);
						
						// network drive connection을 끊는다.
						va_cancel_network_drive_connection(&commandData.mountStatus.networkDrive);
					}
				}
				else
				{
					if (va_lstat(directory, filename, &fileStatus) == 0)
					{
						if (va_is_directory(fileStatus.mode, fileStatus.windowsAttribute))
						{
							va_remove_all_directory(directory, filename);
						}
						else
						{
							va_remove(directory, filename);
						}
					}
				}
			#endif
		}
		
		//#issue88 ~ dedup disk 일때 deidx 파일 삭제		
		memset(dedupIndexFileName, 0, MAX_PATH_LENGTH);
		memset(dedupDbFileName, 0, MAX_PATH_LENGTH);
		sprintf(dedupIndexFileName, "%s%s", filename, ".deidx");
		sprintf(dedupDbFileName, "%s%s", filename, ".db");
		if ((fdFile = va_open(directory, dedupIndexFileName, O_RDONLY, 0, 1)) != (va_fd_t)-1)
		{
			va_close(fdFile);	
			va_remove(directory, dedupIndexFileName);
			//ABIOMASTER_SLAVE_DEDUP_DB_FOLDER
			if(!strlen(ABIOMASTER_SLAVE_DEDUP_DB_FOLDER))
			{
				strcpy(ABIOMASTER_SLAVE_DEDUP_DB_FOLDER, directory);
			}
			va_remove(ABIOMASTER_SLAVE_DEDUP_DB_FOLDER, dedupDbFileName);
			

		}
		//
				
		va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
	}
	
}

void GetTapeLibraryInventory()
{
	char reply[DSIZ][DSIZ];
	char msg[DSIZ * DSIZ];
	
	va_sock_t commandSock;
	
	struct tapeInventory inventory;
	int elementNumber;
	
	int i;
	
	
	
	if ((commandSock = va_make_client_socket_iptable(clientIP, masterIP, portRule, portRuleNumber, commandData.catalogPort,1)) != -1)
	{
		// tape library의 inventory 정보를 읽어온다.
		memset(&inventory, 0, sizeof(struct tapeInventory));
		if ((elementNumber = va_make_inventory(commandData.mountStatus.device, ELEMENT_ALL, &inventory, 0)) > 0)
		{
			for (i = 0; i < inventory.robotNumber; i++)
			{
				// make a reply message
				va_init_reply_buf(reply);
				
				strcpy(reply[0], "TAPE_LIBRARY_INVENTORY");
				strcpy(reply[280], "1");
				va_itoa(inventory.robot[i].address, reply[281]);
				va_itoa(inventory.robot[i].full, reply[282]);
				strcpy(reply[283], inventory.robot[i].volume);
				
				va_make_reply_msg(reply, msg);
				if (va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE) < 0)
				{
					break;
				}
			}
			
			for (i = 0; i < inventory.storageNumber; i++)
			{
				// make a reply message
				va_init_reply_buf(reply);
				
				strcpy(reply[0], "TAPE_LIBRARY_INVENTORY");
				strcpy(reply[280], "2");
				va_itoa(inventory.storage[i].address, reply[281]);
				va_itoa(inventory.storage[i].full, reply[282]);
				strcpy(reply[283], inventory.storage[i].volume);
				
				va_make_reply_msg(reply, msg);
				if (va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE) < 0)
				{
					break;
				}
			}
			
			for (i = 0; i < inventory.ioNumber; i++)
			{
				// make a reply message
				va_init_reply_buf(reply);
				
				strcpy(reply[0], "TAPE_LIBRARY_INVENTORY");
				strcpy(reply[280], "3");
				va_itoa(inventory.io[i].address, reply[281]);
				va_itoa(inventory.io[i].full, reply[282]);
				strcpy(reply[283], inventory.io[i].volume);
				
				va_make_reply_msg(reply, msg);
				if (va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE) < 0)
				{
					break;
				}
			}
			
			for (i = 0; i < inventory.driveNumber; i++)
			{
				// make a reply message
				va_init_reply_buf(reply);
				
				strcpy(reply[0], "TAPE_LIBRARY_INVENTORY");
				strcpy(reply[280], "4");
				va_itoa(inventory.drive[i].address, reply[281]);
				va_itoa(inventory.drive[i].full, reply[282]);
				strcpy(reply[283], inventory.drive[i].volume);
				strcpy(reply[284], inventory.drive[i].serial);
				
				va_make_reply_msg(reply, msg);
				if (va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE) < 0)
				{
					break;
				}
			}
			
			va_free_inventory(&inventory);
		}
		// tape library의 inventory 정보를 읽어오지 못한 경우 element type을 0으로 넘겨서 inventory 정보를 읽어오지 못했다는 것을 서버에서 알 수 있도록 한다.
		else if (elementNumber < 0)
		{
			// make a reply message
			va_init_reply_buf(reply);
			
			strcpy(reply[0], "TAPE_LIBRARY_INVENTORY");
			strcpy(reply[280], "0");
			
			va_make_reply_msg(reply, msg);
			va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE);
		}
		
		va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
	}
}

int MoveMedium()
{
	va_sock_t commandSock;
	
	struct tapeInventory inventory;
	
	int sourceElementNumber;
	struct tapeElement * sourceElement;
	int sourceStatus;
	
//	int destinationElementNumber;
//	struct tapeElement * destinationElement;
	int destinationStatus;
	
	int i;
	int moveResult;
	int inventoryResult;
	
	int retry_count = 0;
#ifdef __ABIO_AIX
	int libReturnCode;
	
	libReturnCode = 0;
#endif

	// initialize variables
	sourceStatus = -1;
	destinationStatus = -1;

	moveResult = -1;
	inventoryResult = -1;
	
	if ((commandSock = va_make_client_socket_iptable(clientIP, masterIP, portRule, portRuleNumber, commandData.catalogPort,1)) != -1)
	{
	
		
		
		RETRY_MOVE:
		// source slot에서 destination slot으로 volume을 이동한다.

		printf("%s : from %d to %d\n",commandData.mountStatus.device,commandData.mountStatus.volumeAddress, commandData.mountStatus.driveAddress);

		if(0 < DebugLevel)
		{
			WriteDebugData(ABIOMASTER_CLIENT_LOG_FOLDER,"Device.log","%s : from %d to %d\n",commandData.mountStatus.device,commandData.mountStatus.volumeAddress, commandData.mountStatus.driveAddress);
		}
#ifdef __ABIO_AIX
		if ((moveResult = va_move_medium(commandData.mountStatus.device, commandData.mountStatus.volumeAddress, commandData.mountStatus.driveAddress,&libReturnCode)) == 0)
#else
		if ((moveResult = va_move_medium(commandData.mountStatus.device, commandData.mountStatus.volumeAddress, commandData.mountStatus.driveAddress)) == 0)
#endif
		{
			// volume 이동 결과를 확인하기 위해 tape library의 inventory를 읽어온다.
			memset(&inventory, 0, sizeof(struct tapeInventory));
			if ((inventoryResult = va_make_inventory(commandData.mountStatus.device, ELEMENT_ALL, &inventory, 0)) > 0)
			{
				// volume 이동 결과를 확인하기 위해 source slot의 상태를 확인한다.
				if (commandData.mountStatus.mtype == ELEMENT_ROBOT)
				{
					sourceElementNumber = inventory.robotNumber;
					sourceElement = inventory.robot;
				}
				else if (commandData.mountStatus.mtype == ELEMENT_STORAGE)
				{
					sourceElementNumber = inventory.storageNumber;
					sourceElement = inventory.storage;
				}
				else if (commandData.mountStatus.mtype == ELEMENT_IO)
				{
					sourceElementNumber = inventory.ioNumber;
					sourceElement = inventory.io;
				}
				else if (commandData.mountStatus.mtype == ELEMENT_DRIVE)
				{
					sourceElementNumber = inventory.driveNumber;
					sourceElement = inventory.drive;
				}
				
				for (i = 0; i < sourceElementNumber; i++)
				{
					if (sourceElement[i].address == commandData.mountStatus.volumeAddress)
					{
						sourceStatus = sourceElement[i].full;
						
						break;
					}
				}
				
				va_free_inventory(&inventory);

				if( sourceStatus != 0 )
				{
					va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_MoveMedium, ERROR_STORAGE_MOVE_MEDIUM, &commandData);

					if (retry_count < 5) {
						retry_count++;
						printf("HTTPD MoveMedium %d --> %d sourceStaus : %d...retry..%d.\n", commandData.mountStatus.volumeAddress, commandData.mountStatus.driveAddress, sourceStatus, retry_count);
						va_sleep_random_range(10,20);
						goto RETRY_MOVE;
					}
					
				}
			}
			// volume 이동 결과를 확인하지 못한 경우 결과를 저장한다.
			else
			{
				if (inventoryResult == 0)
				{
					va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_MoveMedium, ERROR_STORAGE_NO_ELEMENT, &commandData);
				}
				else if (inventoryResult == -1)
				{
					va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_MoveMedium, ERROR_STORAGE_OPEN_TAPE_LIBRARY, &commandData);
				}
				else if (inventoryResult == -2)
				{
					va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_MoveMedium, ERROR_STORAGE_CHECK_BUS_TYPE, &commandData);
				}
				else if (inventoryResult == -3)
				{
					va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_MoveMedium, ERROR_STORAGE_READ_ELEMENT, &commandData);
				}
				else if (inventoryResult == -4)
				{
					va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_MoveMedium, ERROR_STORAGE_READ_ELEMENT, &commandData);
				}
				else if (inventoryResult == -6)
				{
					va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_MoveMedium, ERROR_STORAGE_READ_ELEMENT, &commandData);
				}
				else
				{
					va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_MoveMedium, ERROR_STORAGE_MOVE_MEDIUM, &commandData);
				}
			}
		}
		else
		{
			if (moveResult == -1)
			{
				va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_MoveMedium, ERROR_STORAGE_OPEN_TAPE_LIBRARY, &commandData);
			}
			else if (moveResult == -2)
			{
				va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_MoveMedium, ERROR_STORAGE_CHECK_BUS_TYPE, &commandData);
			}
			else if (moveResult == -3)
			{
				va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_MoveMedium, ERROR_STORAGE_READ_ELEMENT, &commandData);
			}
			else if (moveResult == -4)
			{
				va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_MoveMedium, ERROR_STORAGE_NO_ELEMENT, &commandData);
			}
			else if (moveResult == -5)
			{
				va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_MoveMedium, ERROR_STORAGE_SOURCE_ELEMENT_EMPTY, &commandData);
			}
			else if (moveResult == -6)
			{
				va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_MoveMedium, ERROR_STORAGE_DESTINATION_ELEMENT_FULL, &commandData);
			}
			else
			{
				va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_MoveMedium, ERROR_STORAGE_MOVE_MEDIUM, &commandData);
			}
		}

#ifdef __ABIO_AIX
		if(DebugLevel)
		{
			WriteDebugData(ABIOMASTER_CLIENT_LOG_FOLDER,"Device.log","moveResult = %d , libReturnCode = %d",moveResult,libReturnCode);
		}		
#endif
		// volume 이동 결과를 전달한다.
		va_send(commandSock, &commandData, sizeof(struct ckBackup), 0, DATA_TYPE_CK_BACKUP);
		va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
	}

	return moveResult;
}

void CleaningMoveMedium()
{		
	struct ck command2;
	struct ckBackup commandData2;
	
	va_sock_t commandSock;
	struct ckBackup commandDataReply;

		
	// unmount volume job을 등록한다.
	memset(&command2, 0, sizeof(struct ck));
	memset(&commandData2, 0, sizeof(struct ckBackup));
	commandData2.version = CURRENT_VERSION_INTERNAL;
	commandData2.jobType = UNMOUNT_VOLUME_JOB;
	strcpy(commandData2.client.nodename, commandData.client.nodename);
	strcpy(commandData2.policy, commandData.policy);
	strcpy(commandData2.schedule, commandData.schedule);


	strcpy(commandData2.mountStatus.volume, commandData.mountStatus.volume);
	commandData2.mountTime =  commandData.mountTime;
		

	if(MoveMedium() == 0)
	{
		va_sleep((int)commandData2.mountTime * 60);
				
		// library controller가 살아있으면 library controller로 명령을 보내고 처리 결과를 기다린다.
		memset(&command2, 0, sizeof(struct ck));
		strcpy(command2.requestCommand, "UNMOUNT_CLEANING_VOLUME");		
		
		if ((commandSock = va_connect(commandData.client.masterIP, commandData.client.masterPort, 1)) != -1)
		{
			// 명령을 library controller로 보낸다.
			memcpy(command2.dataBlock, &commandData2, sizeof(struct ckBackup));
			
			if (va_send(commandSock, &command2, sizeof(struct ck), 0, DATA_TYPE_CK) != sizeof(struct ck))
			{
				va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_MoveMedium, ERROR_NETWORK_LIBRARYD_DOWN, &commandData2);
			}
			
			// 명령 처리 결과를 기다린다.
			if (!GET_ERROR(commandData2.errorCode))
			{
				if (va_recv(commandSock, &commandDataReply, sizeof(struct ckBackup), 0, DATA_TYPE_CK_BACKUP) == sizeof(struct ckBackup))
				{
					memcpy(commandData2.errorCode, commandDataReply.errorCode, sizeof(commandData2.errorCode));
					memcpy(commandData2.errorCode, commandDataReply.errorCode, sizeof(commandData2.errorCode));
				}
				else
				{
					va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_MoveMedium, ERROR_NETWORK_LIBRARYD_DOWN, &commandData2);
				}

			}
			
			va_close_socket(commandSock, ABIO_SOCKET_CLOSE_SERVER);
		}
		// library controller가 살아있지 않으면 에러로 처리한다.
		else
		{
			va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_MoveMedium, ERROR_NETWORK_LIBRARYD_DOWN, &commandData2);
		}
	}
	else
	{		
		va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_MoveMedium, ERROR_STORAGE_MOVE_MEDIUM, &commandData2);
	}
}

void GetNodeModule()
{
	char reply[DSIZ][DSIZ];
	char msg[DSIZ * DSIZ];
	
	va_sock_t commandSock;
	
	char ** lines;
	int * linesLength;
	int lineNumber;
	
	char requestCommand[MAX_ACTION_COMMAND_NAME_LENGTH];
	char actionCommand[MAX_ACTION_COMMAND_VALUE_LENGTH];
	char commandVersion[NUMBER_STRING_LENGTH];
	char majorVersion[NUMBER_STRING_LENGTH];
	char minorVersion[NUMBER_STRING_LENGTH];
	char maintenanceLevel[NUMBER_STRING_LENGTH];
	char patchLevel[NUMBER_STRING_LENGTH];
	
	char moduleVersionCk[NUMBER_STRING_LENGTH];
	char moduleVersionMaster[NUMBER_STRING_LENGTH];
	char moduleVersionSlave[NUMBER_STRING_LENGTH];
	char moduleVersionClient[NUMBER_STRING_LENGTH];
	char moduleVersionOracle[NUMBER_STRING_LENGTH];
	char moduleVersionMssql[NUMBER_STRING_LENGTH];
	char moduleVersionExchange[NUMBER_STRING_LENGTH];
	char moduleVersionSap[NUMBER_STRING_LENGTH];
	char moduleVersionDb2[NUMBER_STRING_LENGTH];
	char moduleVersionInformix[NUMBER_STRING_LENGTH];
	char moduleVersionSybase[NUMBER_STRING_LENGTH];
	char moduleVersionSybaseIQ[NUMBER_STRING_LENGTH];
	char moduleVersionMysql[NUMBER_STRING_LENGTH];
	char moduleVersionOracleRman[NUMBER_STRING_LENGTH];
	char moduleVersionNotes[NUMBER_STRING_LENGTH];
	char moduleVersionUnisql[NUMBER_STRING_LENGTH];
	char moduleVersionAltibase[NUMBER_STRING_LENGTH];
	char moduleVersionExchangeMailbox[NUMBER_STRING_LENGTH];
	char moduleVersionNdmp[NUMBER_STRING_LENGTH];
	char moduleVersionVMware[NUMBER_STRING_LENGTH];
	char moduleVersionInformixOnbar[NUMBER_STRING_LENGTH];
	char moduleVersionTibero[NUMBER_STRING_LENGTH];
	char moduleVersionOracle12c[NUMBER_STRING_LENGTH];
	char moduleVersionSQLBackTrack[NUMBER_STRING_LENGTH];
	char moduleVersionSharePoint[NUMBER_STRING_LENGTH];
	char moduleVersionPhysical[NUMBER_STRING_LENGTH];
	char moduleVersionSpecialFolder[NUMBER_STRING_LENGTH];
	int firstBlank;
	int secondBlank;
	
	int i;
	int j;
	
	
	
	// initalize variables
	memset(moduleVersionCk, 0, sizeof(moduleVersionCk));
	memset(moduleVersionMaster, 0, sizeof(moduleVersionMaster));
	memset(moduleVersionSlave, 0, sizeof(moduleVersionSlave));
	memset(moduleVersionClient, 0, sizeof(moduleVersionClient));
	memset(moduleVersionOracle, 0, sizeof(moduleVersionOracle));
	memset(moduleVersionMssql, 0, sizeof(moduleVersionMssql));
	memset(moduleVersionExchange, 0, sizeof(moduleVersionExchange));
	memset(moduleVersionSap, 0, sizeof(moduleVersionSap));
	memset(moduleVersionDb2, 0, sizeof(moduleVersionDb2));
	memset(moduleVersionInformix, 0, sizeof(moduleVersionInformix));
	memset(moduleVersionSybase, 0, sizeof(moduleVersionSybase));
	memset(moduleVersionSybaseIQ, 0, sizeof(moduleVersionSybaseIQ));
	memset(moduleVersionMysql, 0, sizeof(moduleVersionMysql));
	memset(moduleVersionOracleRman, 0, sizeof(moduleVersionOracleRman));
	memset(moduleVersionNotes, 0, sizeof(moduleVersionNotes));
	memset(moduleVersionUnisql, 0, sizeof(moduleVersionUnisql));
	memset(moduleVersionAltibase, 0, sizeof(moduleVersionAltibase));
	memset(moduleVersionExchangeMailbox, 0, sizeof(moduleVersionExchangeMailbox));
	memset(moduleVersionNdmp, 0, sizeof(moduleVersionNdmp));
	memset(moduleVersionVMware, 0, sizeof(moduleVersionVMware));
	memset(moduleVersionInformixOnbar, 0, sizeof(moduleVersionInformixOnbar));
	memset(moduleVersionTibero, 0, sizeof(moduleVersionTibero));
	memset(moduleVersionOracle12c, 0, sizeof(moduleVersionOracle12c));
	memset(moduleVersionSQLBackTrack, 0, sizeof(moduleVersionSQLBackTrack));
	memset(moduleVersionSharePoint, 0, sizeof(moduleVersionSharePoint));
	memset(moduleVersionPhysical, 0, sizeof(moduleVersionPhysical));
	memset(moduleVersionSpecialFolder, 0, sizeof(moduleVersionSpecialFolder));

	// set a version of node module
	if ((lineNumber = va_load_text_file_lines(ABIOMASTER_CK_FOLDER, ACTION_LIST, &lines, &linesLength)) > 0)
	{
		for (i = 0; i < lineNumber; i++)
		{
			if (lines[i][0] == '\0')
			{
				continue;
			}
			
			memset(requestCommand, 0, sizeof(requestCommand));
			memset(actionCommand, 0, sizeof(actionCommand));
			memset(commandVersion, 0, sizeof(commandVersion));
			
			for (j = 0; j < linesLength[i]; j++)
			{
				if (lines[i][j] == ' ' || lines[i][j] == '\t')
				{
					firstBlank = j;
					break;
				}
			}
			
			for (j = linesLength[i] - 1; j > 0; j--)
			{
				if (lines[i][j] == ' ' || lines[i][j] == '\t')
				{
					secondBlank = j;
					break;
				}
			}
			
			strncpy(requestCommand, lines[i], firstBlank);
			strncpy(actionCommand, lines[i] + firstBlank + 1, secondBlank - firstBlank - 1);
			strcpy(commandVersion, lines[i] + secondBlank + 1);
			
			memset(majorVersion, 0, sizeof(majorVersion));
			memset(minorVersion, 0, sizeof(minorVersion));
			memset(maintenanceLevel, 0, sizeof(maintenanceLevel));
			memset(patchLevel, 0, sizeof(patchLevel));
			strncpy(majorVersion, commandVersion + 0, 2);
			strncpy(minorVersion, commandVersion + 2, 2);
			strncpy(maintenanceLevel, commandVersion + 4, 2);
			strncpy(patchLevel, commandVersion + 6, 2);
			
			if (!strcmp(requestCommand, MODULE_NAME_NEW_CK))
			{
				sprintf(moduleVersionCk, CURRENT_VERSION_DISPLAY_FORMAT, atoi(majorVersion), atoi(minorVersion), atoi(maintenanceLevel), atoi(patchLevel));
			}
			else if (!strcmp(requestCommand, MODULE_NAME_CK))
			{
				sprintf(moduleVersionCk, CURRENT_VERSION_DISPLAY_FORMAT, atoi(majorVersion), atoi(minorVersion), atoi(maintenanceLevel), atoi(patchLevel));
			}
			else if (!strcmp(requestCommand, MODULE_NAME_MASTER_BACKUP))
			{
				sprintf(moduleVersionMaster, CURRENT_VERSION_DISPLAY_FORMAT, atoi(majorVersion), atoi(minorVersion), atoi(maintenanceLevel), atoi(patchLevel));
			}
			else if (!strcmp(requestCommand, MODULE_NAME_SLAVE_BACKUP))
			{
				sprintf(moduleVersionSlave, CURRENT_VERSION_DISPLAY_FORMAT, atoi(majorVersion), atoi(minorVersion), atoi(maintenanceLevel), atoi(patchLevel));
			}
			else if (!strcmp(requestCommand, MODULE_NAME_CLIENT_BACKUP))
			{
				sprintf(moduleVersionClient, CURRENT_VERSION_DISPLAY_FORMAT, atoi(majorVersion), atoi(minorVersion), atoi(maintenanceLevel), atoi(patchLevel));
			}
			else if (!strcmp(requestCommand, MODULE_NAME_DB_ORACLE))
			{
				sprintf(moduleVersionOracle, CURRENT_VERSION_DISPLAY_FORMAT, atoi(majorVersion), atoi(minorVersion), atoi(maintenanceLevel), atoi(patchLevel));
			}
			else if (!strcmp(requestCommand, MODULE_NAME_DB_MSSQL))
			{
				sprintf(moduleVersionMssql, CURRENT_VERSION_DISPLAY_FORMAT, atoi(majorVersion), atoi(minorVersion), atoi(maintenanceLevel), atoi(patchLevel));
			}
			else if (!strcmp(requestCommand, MODULE_NAME_DB_EXCHANGE))
			{
				sprintf(moduleVersionExchange, CURRENT_VERSION_DISPLAY_FORMAT, atoi(majorVersion), atoi(minorVersion), atoi(maintenanceLevel), atoi(patchLevel));
			}
			else if (!strcmp(requestCommand, MODULE_NAME_DB_SAP))
			{
				sprintf(moduleVersionSap, CURRENT_VERSION_DISPLAY_FORMAT, atoi(majorVersion), atoi(minorVersion), atoi(maintenanceLevel), atoi(patchLevel));
			}
			else if (!strcmp(requestCommand, MODULE_NAME_DB_DB2))
			{
				sprintf(moduleVersionDb2, CURRENT_VERSION_DISPLAY_FORMAT, atoi(majorVersion), atoi(minorVersion), atoi(maintenanceLevel), atoi(patchLevel));
			}
			else if (!strcmp(requestCommand, MODULE_NAME_DB_INFORMIX))
			{
				sprintf(moduleVersionInformix, CURRENT_VERSION_DISPLAY_FORMAT, atoi(majorVersion), atoi(minorVersion), atoi(maintenanceLevel), atoi(patchLevel));
			}
			else if (!strcmp(requestCommand, MODULE_NAME_DB_SYBASE))
			{
				sprintf(moduleVersionSybase, CURRENT_VERSION_DISPLAY_FORMAT, atoi(majorVersion), atoi(minorVersion), atoi(maintenanceLevel), atoi(patchLevel));
			}
			else if (!strcmp(requestCommand, MODULE_NAME_DB_SYBASE_IQ))
			{
				sprintf(moduleVersionSybaseIQ, CURRENT_VERSION_DISPLAY_FORMAT, atoi(majorVersion), atoi(minorVersion), atoi(maintenanceLevel), atoi(patchLevel));
			}
			else if (!strcmp(requestCommand, MODULE_NAME_DB_MYSQL))
			{
				sprintf(moduleVersionMysql, CURRENT_VERSION_DISPLAY_FORMAT, atoi(majorVersion), atoi(minorVersion), atoi(maintenanceLevel), atoi(patchLevel));
			}
			else if (!strcmp(requestCommand, MODULE_NAME_DB_ORACLE_RMAN))
			{
				sprintf(moduleVersionOracleRman, CURRENT_VERSION_DISPLAY_FORMAT, atoi(majorVersion), atoi(minorVersion), atoi(maintenanceLevel), atoi(patchLevel));
			}
			else if (!strcmp(requestCommand, MODULE_NAME_DB_NOTES))
			{
				sprintf(moduleVersionNotes, CURRENT_VERSION_DISPLAY_FORMAT, atoi(majorVersion), atoi(minorVersion), atoi(maintenanceLevel), atoi(patchLevel));
			}
			else if (!strcmp(requestCommand, MODULE_NAME_DB_UNISQL))
			{
				sprintf(moduleVersionUnisql, CURRENT_VERSION_DISPLAY_FORMAT, atoi(majorVersion), atoi(minorVersion), atoi(maintenanceLevel), atoi(patchLevel));
			}
			else if (!strcmp(requestCommand, MODULE_NAME_DB_ALTIBASE))
			{
				sprintf(moduleVersionAltibase, CURRENT_VERSION_DISPLAY_FORMAT, atoi(majorVersion), atoi(minorVersion), atoi(maintenanceLevel), atoi(patchLevel));
			}
			else if (!strcmp(requestCommand, MODULE_NAME_DB_EXCHANGE_MAILBOX))
			{
				sprintf(moduleVersionExchangeMailbox, CURRENT_VERSION_DISPLAY_FORMAT, atoi(majorVersion), atoi(minorVersion), atoi(maintenanceLevel), atoi(patchLevel));
			}
			else if (!strcmp(requestCommand, MODULE_NAME_NDMP_HTTPD))
			{
				sprintf(moduleVersionNdmp, CURRENT_VERSION_DISPLAY_FORMAT, atoi(majorVersion), atoi(minorVersion), atoi(maintenanceLevel), atoi(patchLevel));
			}
			else if (!strcmp(requestCommand, MODULE_NAME_VMWARE_HTTPD))
			{
				sprintf(moduleVersionVMware, CURRENT_VERSION_DISPLAY_FORMAT, atoi(majorVersion), atoi(minorVersion), atoi(maintenanceLevel), atoi(patchLevel));
			}
			else if (!strcmp(requestCommand, MODULE_NAME_DB_INFORMIX_ONBAR))
			{
				sprintf(moduleVersionInformixOnbar, CURRENT_VERSION_DISPLAY_FORMAT, atoi(majorVersion), atoi(minorVersion), atoi(maintenanceLevel), atoi(patchLevel));
			}
			else if (!strcmp(requestCommand, MODULE_NAME_DB_TIBERO))
			{
				sprintf(moduleVersionTibero, CURRENT_VERSION_DISPLAY_FORMAT, atoi(majorVersion), atoi(minorVersion), atoi(maintenanceLevel), atoi(patchLevel));
			}
			else if (!strcmp(requestCommand, MODULE_NAME_DB_ORACLE12C))
			{
				sprintf(moduleVersionOracle12c, CURRENT_VERSION_DISPLAY_FORMAT, atoi(majorVersion), atoi(minorVersion), atoi(maintenanceLevel), atoi(patchLevel));
			}
			else if (!strcmp(requestCommand, MODULE_NAME_DB_SQL_BACKTRACK))
			{
				sprintf(moduleVersionSQLBackTrack, CURRENT_VERSION_DISPLAY_FORMAT, atoi(majorVersion), atoi(minorVersion), atoi(maintenanceLevel), atoi(patchLevel));
			}
			else if (!strcmp(requestCommand, MODULE_NAME_DB_SHAREPOINT_HTTPD))
			{
				sprintf(moduleVersionSharePoint, CURRENT_VERSION_DISPLAY_FORMAT, atoi(majorVersion), atoi(minorVersion), atoi(maintenanceLevel), atoi(patchLevel));
			}
			//issue344
			else if (!strcmp(requestCommand, MODULE_NAME_OS_HTTPD))
			{
				sprintf(moduleVersionPhysical, CURRENT_VERSION_DISPLAY_FORMAT, atoi(majorVersion), atoi(minorVersion), atoi(maintenanceLevel), atoi(patchLevel));
			}
			else if (!strcmp(requestCommand, MODULE_NAME_SF_HTTPD))
			{
				sprintf(moduleVersionSpecialFolder, CURRENT_VERSION_DISPLAY_FORMAT, atoi(majorVersion), atoi(minorVersion), atoi(maintenanceLevel), atoi(patchLevel));
			}
		}
		
		
		for (i = 0; i < lineNumber; i++)
		{
			va_free(lines[i]);
		}
		va_free(lines);
		va_free(linesLength);
	}
	
	
	if ((commandSock = va_make_client_socket_iptable(clientIP, masterIP, portRule, portRuleNumber, commandData.catalogPort,1)) != -1)
	{
		// make a reply message
		va_init_reply_buf(reply);
		
		strcpy(reply[0], "NODE_MODULE");
		strcpy(reply[290], moduleVersionCk);
		strcpy(reply[291], moduleVersionMaster);
		strcpy(reply[292], moduleVersionSlave);
		strcpy(reply[293], moduleVersionClient);
		strcpy(reply[294], moduleVersionOracle);
		strcpy(reply[295], moduleVersionMssql);
		strcpy(reply[296], moduleVersionExchange);
		strcpy(reply[297], moduleVersionSap);
		strcpy(reply[298], moduleVersionDb2);
		strcpy(reply[299], moduleVersionInformix);
		strcpy(reply[300], moduleVersionSybase);
		strcpy(reply[301], moduleVersionMysql);
		strcpy(reply[302], moduleVersionOracleRman);
		strcpy(reply[303], moduleVersionNotes);
		strcpy(reply[304], moduleVersionUnisql);
		strcpy(reply[305], moduleVersionAltibase);
		strcpy(reply[306], moduleVersionExchangeMailbox);
		strcpy(reply[307], moduleVersionNdmp);
		strcpy(reply[308], moduleVersionVMware);
		strcpy(reply[309], moduleVersionInformixOnbar);
		strcpy(reply[310], moduleVersionTibero);
		strcpy(reply[311], moduleVersionOracle12c);
		strcpy(reply[312], moduleVersionSQLBackTrack);
		strcpy(reply[313], moduleVersionSharePoint);
		//issue344
		strcpy(reply[314], moduleVersionPhysical);		
		strcpy(reply[315], moduleVersionSpecialFolder);		
		strcpy(reply[31], moduleVersionSybaseIQ);

		va_make_reply_msg(reply, msg);
		va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE);
		
		va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
	}
}

void ServiceStart()
{
	char reply[DSIZ][DSIZ];
	char msg[DSIZ * DSIZ];
	
	va_sock_t commandSock;
			
	struct ckBackup *commandData;
	
	// initalize variables
	commandData = (struct ckBackup *)malloc(sizeof(struct ckBackup));	
	memcpy(commandData,command.dataBlock , sizeof(struct ckBackup));	
	
	if(1)
	{			
		if ((commandSock = va_make_client_socket_iptable(clientIP, masterIP, portRule, portRuleNumber, commandData->catalogPort,1)) != -1)
		{
			// make a reply message
			va_init_reply_buf(reply);
			
			strcpy(reply[0], "SERVICE_ACTIVE");
						
			if(!va_execute_command("SC START \""ABIOMASTER_PRODUCT_NAME" Scheduler\""))
			{
				strcpy(reply[221],"s");
			}
			else
			{
				strcpy(reply[221],"f");
			}
			va_make_reply_msg(reply, msg);											

			va_send(commandSock, msg, 1048576, 0, DATA_TYPE_NOT_CHANGE);		
			
			va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);					
		}	
	}
	else
	{
		if ((commandSock = va_make_client_socket_iptable(clientIP, masterIP, portRule, portRuleNumber, commandData->catalogPort,1)) != -1)
		{
			// make a reply message
			va_init_reply_buf(reply);
						
			strcpy(reply[0], "SERVICE_INACTIVE");
						
			va_make_reply_msg(reply, msg);
			va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE);
			
			va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
		}
	}
}

void ServiceStop()
{
	char reply[DSIZ][DSIZ];
	char msg[DSIZ * DSIZ];
	
	va_sock_t commandSock;
			
	struct ckBackup *commandData;
	
	// initalize variables
	commandData = (struct ckBackup *)malloc(sizeof(struct ckBackup));	
	memcpy(commandData,command.dataBlock , sizeof(struct ckBackup));	
	
	if(1)
	{			
		if ((commandSock = va_make_client_socket_iptable(clientIP, masterIP, portRule, portRuleNumber, commandData->catalogPort,1)) != -1)
		{
			// make a reply message
			va_init_reply_buf(reply);
			
			strcpy(reply[0], "SERVICE_INACTIVE");
						
			if(!va_execute_command("SC STOP \""ABIOMASTER_PRODUCT_NAME" Scheduler\""))
			{
				strcpy(reply[221],"s");
			}
			else
			{
				strcpy(reply[221],"f");
			}
			va_make_reply_msg(reply, msg);											

			va_send(commandSock, msg, 1048576, 0, DATA_TYPE_NOT_CHANGE);		
			
			va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);					
		}	
	}
	else
	{
		if ((commandSock = va_make_client_socket_iptable(clientIP, masterIP, portRule, portRuleNumber, commandData->catalogPort,1)) != -1)
		{
			// make a reply message
			va_init_reply_buf(reply);
						
			strcpy(reply[0], "SERVICE_INACTIVE");
						
			va_make_reply_msg(reply, msg);
			va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE);
			
			va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
		}
	}
}

void GetModuleConf()
{
	char reply[DSIZ][DSIZ];
	char msg[DSIZ * DSIZ];
	
	va_sock_t commandSock;
	
	char ** lines;
	int * linesLength;
	int lineNumber;
	
	char requestCommand[MAX_ACTION_COMMAND_NAME_LENGTH];
	char actionCommand[MAX_ACTION_COMMAND_VALUE_LENGTH];
			
	char actionListModuleName[NUMBER_STRING_LENGTH];
	char modulePath[MAX_ACTION_COMMAND_VALUE_LENGTH];
	char moduleFileName[MAX_ACTION_COMMAND_VALUE_LENGTH];

	char* valueSize;

	int firstBlank;
	int secondBlank;
	
	int i;
	int j;
	int k;
	int length;
	int commandLength;

	struct confOption * moduleConfig;
	struct ckBackup *commandData;

	// set a version of node module
	//int moduleConfigNumber;
	
	moduleConfig = (struct confOption *)malloc(sizeof(struct confOption ));
		
	commandData = (struct ckBackup *)malloc(sizeof(struct ckBackup));	
	memcpy(commandData,command.dataBlock , sizeof(struct ckBackup));	

	// initalize variables
	memset(modulePath, 0, sizeof(modulePath));
	
	valueSize = NULL;
	
	//not matched name pattern "exchange mailbox" in actionList file("MAILBOX EXCHANGE") and conf file name("exchange mailbox").	
	va_convert_moduleName_actionList(actionListModuleName,commandData->mdouelOption.moduleName);

	if ((lineNumber = va_load_text_file_lines(ABIOMASTER_CK_FOLDER, ACTION_LIST, &lines, &linesLength)) > 0)
	{
		for (i = 0; i < lineNumber; i++)
		{
			if (lines[i][0] == '\0')
			{
				continue;
			}
			
			memset(requestCommand, 0, sizeof(requestCommand));
			memset(actionCommand, 0, sizeof(actionCommand));
						
			for (j = 0; j < linesLength[i]; j++)
			{
				if (lines[i][j] == ' ' || lines[i][j] == '\t')
				{
					firstBlank = j;
					break;
				}
			}
			
			for (j = linesLength[i] - 1; j > 0; j--)
			{
				if (lines[i][j] == ' ' || lines[i][j] == '\t')
				{
					secondBlank = j;
					break;
				}
			}		
			
			strncpy(requestCommand, lines[i], firstBlank);
			strncpy(actionCommand, lines[i] + firstBlank + 1, secondBlank - firstBlank - 1);
									
			if (strstr(requestCommand, actionListModuleName) != NULL)
			{
				length = 0;
				
				commandLength = (int)strlen(actionCommand); 

				for(k = 0; k < commandLength; k++)
				{					
					#ifdef __ABIO_WIN											
					if(actionCommand[k] == '\\')
					{
						length = k;
					}
					#else
					if(actionCommand[k] == '/')
					{
						length = k;
					}	
					#endif
					
				}				
				strncpy(modulePath,actionCommand,length);				
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
	
	va_mapping_moduleName_confFile(moduleFileName,commandData->mdouelOption.moduleName);	
	
	if(modulePath != NULL)
	{		
		if ((lineNumber = va_load_text_file_lines(modulePath, moduleFileName, &lines, &linesLength)) > 0)
		{			
			
			if(lineNumber > 0)
			{
				if ((commandSock = va_make_client_socket_iptable(clientIP, masterIP, portRule, portRuleNumber, commandData->catalogPort,1)) != -1)
				{
					for (i = 0; i < lineNumber; i++)
					{			
						if (lines[i][0] == '\0' || lines[i][1] == '\0' || lines[i][1] == ' ' || lines[i][1] == '*' || lines[i][1] == '\t')
						{
							continue;
						}
						
						memset(requestCommand, 0, sizeof(requestCommand));
						memset(actionCommand, 0, sizeof(actionCommand));
												
						firstBlank = linesLength[i];
						for (j = 0; j < linesLength[i]; j++)
						{
							if (lines[i][j] == ' ' || lines[i][j] == '\t')
							{
								firstBlank = j;
								break;
							}
						}
						
						strncpy(requestCommand, lines[i], firstBlank);

						if(firstBlank < linesLength[i])
						{
							strcpy(actionCommand, lines[i] + firstBlank + 1);						
						}
						else
						{
								
						}

						// make a reply message
						va_init_reply_buf(reply);
						
						strcpy(reply[0], "MODULE_CONF");
						
						strcpy(reply[290], requestCommand);

						
						sprintf(reply[291], "%d", strlen(actionCommand));	
						#ifdef __ABIO_WIN
							va_backslash_to_slash(actionCommand);
						#endif
						strcpy(reply[292], actionCommand);

						va_make_reply_msg(reply, msg);
												
						//va_send(commandSock, msg, (int)strlen(msg), 0, DATA_TYPE_NOT_CHANGE);		
						va_send(commandSock, msg, 1048576, 0, DATA_TYPE_NOT_CHANGE);		
												
					}										
					va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);					
				}
			}
		}		
	}
	else
	{
		if ((commandSock = va_make_client_socket_iptable(clientIP, masterIP, portRule, portRuleNumber, commandData->catalogPort,1)) != -1)
		{
			// make a reply message
			va_init_reply_buf(reply);
						
			strcpy(reply[0], "MODULE_CONF");
			strcpy(reply[290], modulePath);
			
			va_make_reply_msg(reply, msg);
			va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE);
			
			va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
		}
	}
}

void GetNodeInformation()
{
	char reply[DSIZ][DSIZ];
	char msg[DSIZ * DSIZ];
	char hostid[HOST_ID_LENGTH];
	
	va_sock_t commandSock;
	
	enum platformType platformType;
	
	
	
	if ((commandSock = va_make_client_socket_iptable(clientIP, masterIP, portRule, portRuleNumber, commandData.catalogPort,1)) != -1)
	{
		// platform type을 지정한다.
		#ifdef __ABIO_AIX
			platformType = PLATFORM_AIX;
		#elif __ABIO_HP
			platformType = PLATFORM_HP;
		#elif __ABIO_LINUX
			platformType = PLATFORM_LINUX;
		#elif __ABIO_SOLARIS
			platformType = PLATFORM_SOLARIS;
		#elif __ABIO_UNIXWARE
			platformType = PLATFORM_UNIXWARE;
		#elif __ABIO_TRU64
			platformType = PLATFORM_TRU64;
		#elif __ABIO_FREEBSD
			platformType = PLATFORM_FREEBSD;
		#elif __ABIO_WIN
			platformType = PLATFORM_WINDOWS;
		#endif
		
		
		// make a reply message
		va_init_reply_buf(reply);
		
		strcpy(reply[0], "NODE_INFORMATION");
		va_itoa(platformType, reply[112]);
		
		// mac address를 구한다.
		memset(hostid, 0, sizeof(hostid));
		if (va_get_macaddress(hostid) == 0)
		{
			strcpy(reply[119], hostid);
		}
		
		
		va_make_reply_msg(reply, msg);
		va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE);
		
		va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
	}
}

void ExpireBackupset()
{
	va_sock_t commandSock;
	
	char backupset[BACKUPSET_ID_LENGTH];
	char ** expiredBackupsetList;
	int expiredBackupsetListNumber;
	
	va_fd_t fdCatalogLatest;
	struct catalogDB catalogDB;
	int isValidBackupset;
	
	int i;
	
	
	
	// initialize variables
	expiredBackupsetList = NULL;
	expiredBackupsetListNumber = 0;
	
	
	if ((commandSock = va_make_client_socket_iptable(clientIP, masterIP, portRule, portRuleNumber, commandData.catalogPort,1)) != -1)
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
		
		
		// expire backup files in the expired backupset
		if ((fdCatalogLatest = va_open(ABIOMASTER_CLIENT_CATALOG_DB_FOLDER, ABIOMASTER_CATALOG_DB_LATEST, O_RDWR, 1, 0)) != (va_fd_t)-1)
		{
			while (va_read(fdCatalogLatest, &catalogDB, sizeof(struct catalogDB), DATA_TYPE_CATALOG_DB) > 0)
			{
				for (i = 0; i < expiredBackupsetListNumber; i++)
				{
					if (!strcmp(catalogDB.backupset, expiredBackupsetList[i]))
					{
						catalogDB.expire = 1;
						
						va_lseek(fdCatalogLatest, -(va_offset_t)sizeof(struct catalogDB), SEEK_CUR);
						va_write(fdCatalogLatest, &catalogDB, sizeof(struct catalogDB), DATA_TYPE_CATALOG_DB);
						
						break;
					}
				}
				
				va_lseek(fdCatalogLatest, (va_offset_t)(catalogDB.length + catalogDB.length2 + catalogDB.length3), SEEK_CUR);
			}
			
			
			// check whether the latest catalog db is expired or not.
			// the latest catalog db에 있는 모든 file이 expire되었으면 the latest catalog db가 expire되었다고 판단한다.
			isValidBackupset = 0;
			
			va_lseek(fdCatalogLatest, 0, SEEK_SET);
			while (va_read(fdCatalogLatest, &catalogDB, sizeof(struct catalogDB), DATA_TYPE_CATALOG_DB) > 0)
			{
				if (catalogDB.expire == 0)
				{
					isValidBackupset = 1;
					
					break;
				}
				
				va_lseek(fdCatalogLatest, (va_offset_t)(catalogDB.length + catalogDB.length2 + catalogDB.length3), SEEK_CUR);
			}
			
			va_close(fdCatalogLatest);
			
			
			// if the latest catalog db is expired, remove it
			if (isValidBackupset == 0)
			{
				va_remove(ABIOMASTER_CLIENT_CATALOG_DB_FOLDER, ABIOMASTER_CATALOG_DB_LATEST);
			}
		}
		
		
		for (i = 0; i < expiredBackupsetListNumber; i++)
		{
			va_free(expiredBackupsetList[i]);
		}
		va_free(expiredBackupsetList);
	}
}

#ifdef __ABIO_WIN
	int GetRootFileList(va_sock_t commandSock)
	{
		char reply[DSIZ][DSIZ];
		char msg[DSIZ * DSIZ];
		
		DWORD drives;
		int driveIndex;
		int driveType;
		
		char driveName[MAX_PATH_LENGTH];
		__int64 diskSize;
		
		
		
		driveIndex = 0;
		
		drives = GetLogicalDrives();
		
		if(0 < DebugLevel)
		{
			WriteDebugData(ABIOMASTER_CLIENT_LOG_FOLDER,"netdrive.log","drives = %d",drives);
		}

		while (drives)
		{
			if ((drives & 1) == 1)
			{
				driveType = va_get_windows_drive_type('A' + driveIndex);
				
				if(0 < DebugLevel)
				{
					WriteDebugData(ABIOMASTER_CLIENT_LOG_FOLDER,"netdrive.log","driveType = %d",driveType);
				}

				if (driveType != DRIVE_REMOVABLE && driveType != DRIVE_CDROM)
				{
					memset(driveName, 0, sizeof(driveName));
					sprintf(driveName, "%c:\\", 'A' + driveIndex);
					diskSize = va_get_disk_free_space(driveName);
					
					
					// make a reply message
					va_init_reply_buf(reply);
					
					strcpy(reply[0], "FILE_LIST");
					strcpy(reply[90], "0");
					reply[91][0] = 'A' + driveIndex;
					strcpy(reply[92], "d--");
					
					if (diskSize > 0)
					{
						va_lltoa(diskSize, reply[96]);
					}
					else
					{
						strcpy(reply[96], "0");
					}
					
					va_make_reply_msg(reply, msg);
					if (va_send(commandSock, msg, (int)strlen(msg) + 1 , 0, DATA_TYPE_NOT_CHANGE) < 0)
					{
						break;
					}
				}
			}
			
			driveIndex++;
			drives >>= 1;
		}
		
		memset(msg, 0, sizeof(msg));
		sprintf(msg, "FILE_LIST_END\\end\\");
		va_send(commandSock, msg, (int)strlen(msg) + 1 , 0, DATA_TYPE_NOT_CHANGE);
		
		
		return 0;
	}
	
	int GetRootDirectoryList(va_sock_t commandSock)
	{
		char reply[DSIZ][DSIZ];
		char msg[DSIZ * DSIZ];
		
		DWORD drives;
		int driveIndex;
		int driveType;
		
		
	
		driveIndex = 0;
		
		drives = GetLogicalDrives();
		
		while (drives)
		{
			if ((drives & 1) == 1)
			{
				driveType = va_get_windows_drive_type('A' + driveIndex);
				
				if (driveType != DRIVE_REMOVABLE && driveType != DRIVE_CDROM)
				{
					// make a reply message
					va_init_reply_buf(reply);
					
					strcpy(reply[0], "DIRECTORY_LIST");
					reply[91][0] = 'A' + driveIndex;
					
					va_make_reply_msg(reply, msg);
					if (va_send(commandSock, msg, (int)strlen(msg) + 1 , 0, DATA_TYPE_NOT_CHANGE) < 0)
					{
						break;
					}
				}
			}
			
			driveIndex++;
			drives >>= 1;
		}
		
		
		return 0;
	}
#endif

int GetFileTypeAbio(struct abio_file_stat * file_stat)
{
	#ifdef __ABIO_UNIX
		if (file_stat->lockFile == 1)		return 8;
		if (S_ISDIR(file_stat->mode))       return 0;
		else if (S_ISREG(file_stat->mode))  return 1;
		else if (S_ISCHR(file_stat->mode))  return 2;
		else if (S_ISBLK(file_stat->mode))  return 3;
		else if (S_ISFIFO(file_stat->mode)) return 4;
		else if (S_ISLNK(file_stat->mode))  return 5;
		else if (S_ISSOCK(file_stat->mode)) return 6;
	#elif __ABIO_WIN
		#ifndef __ABIO_WIN_IA64
			if ((file_stat->mode & FILE_ATTRIBUTE_REPARSE_POINT) == FILE_ATTRIBUTE_REPARSE_POINT && 
				IsReparseTagMicrosoft(file_stat->windowsAttribute) != 0 && 
				(file_stat->windowsAttribute & IO_REPARSE_TAG_SYMLINK) == IO_REPARSE_TAG_SYMLINK)
			{
				return 5;
			}
			else
		#endif
			{
				if ((file_stat->mode & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)
				{
					return 0;
				}
				else
				{
					return 1;
				}
			}
	#endif
	
	return 0;
}

void Exit()
{

	int i;

	
	
	
	#ifdef __ABIO_WIN
		//윈속 제거
		WSACleanup();
		
		// Network drive 연결 종료
		for (i = 0; i < networkDriveCount; i++)
		{
			va_cancel_network_drive_connection(networkDriveArray + i);
		}
		va_free(networkDriveArray);

		va_free(portRule);
	#endif
	if (processPIDList != NULL)
	{
		for (i = 0; i < processPIDListCount; i++)
		{
			va_free(processPIDList[i]);
		}
		va_free(processPIDList);
	}
	if (checkProcess != NULL)
	{
		va_free(checkProcess);
	}
}

int va_mapping_moduleName_confFile(char * confFileName, char *moduleName)
{	

	int i;
	int nameLength;

	nameLength = (int)strlen(moduleName);
	for(i = 0 ; i < nameLength ; i++)
	{
		if(65 <= moduleName[i] && moduleName[i] <= 90)
		{
			moduleName[i] += 32;
		}
	}

	if(strstr(ABIOMASTER_CK_CONFIG,moduleName) != NULL)					//		CK
	{		
		strcpy(confFileName,ABIOMASTER_CK_CONFIG);		
	}
	else if (strstr(ABIOMASTER_MASTER_CONFIG,moduleName) != NULL)		//		MASTER
	{
		strcpy(confFileName,ABIOMASTER_MASTER_CONFIG);		
	}
	else if (strstr(ABIOMASTER_SLAVE_CONFIG,moduleName) != NULL)		//		SLAVE
	{
		strcpy(confFileName,ABIOMASTER_SLAVE_CONFIG);		
	}
	else if (strstr(ABIOMASTER_CLIENT_CONFIG,moduleName) != NULL)		//		CLIENT
	{
		strcpy(confFileName,ABIOMASTER_CLIENT_CONFIG);		
	}
	else if (strstr(ABIOMASTER_NDMP_CONFIG,moduleName) != NULL)			//		NDMP
	{
		strcpy(confFileName,ABIOMASTER_NDMP_CONFIG);		
	}
	else if (strstr(ABIOMASTER_ORACLE_CONFIG,moduleName) != NULL)		//		ORACLE
	{
		strcpy(confFileName,ABIOMASTER_ORACLE_CONFIG);
	}
	else if (strstr(ABIOMASTER_MSSQL_CONFIG,moduleName) != NULL)		//		MSSQL
	{
		strcpy(confFileName,ABIOMASTER_MSSQL_CONFIG);
	}
	else if (strstr(ABIOMASTER_EXCHANGE_CONFIG,moduleName) != NULL)		//		EXCHANGE
	{
		strcpy(confFileName,ABIOMASTER_EXCHANGE_CONFIG);
	}
	else if (strstr(ABIOMASTER_EXCHANGE_MAILBOX_CONFIG,moduleName) != NULL)		//		EXCHANGE_MAILBOX
	{
		strcpy(confFileName,ABIOMASTER_EXCHANGE_MAILBOX_CONFIG);
	}
	else if (strstr(ABIOMASTER_SAP_CONFIG,moduleName) != NULL)			//		SAP
	{
		strcpy(confFileName,ABIOMASTER_SAP_CONFIG);
	}
	else if (strstr(ABIOMASTER_DB2_CONFIG,moduleName) != NULL)			//		DB2
	{
		strcpy(confFileName,ABIOMASTER_DB2_CONFIG);
	}
	else if (strstr(ABIOMASTER_SYBASE_CONFIG,moduleName) != NULL)		//		SYBASE
	{
		strcpy(confFileName,ABIOMASTER_SYBASE_CONFIG);
	}
	else if (strstr(ABIOMASTER_SYBASE_IQ_CONFIG,moduleName) != NULL)		//		SYBASE IQ
	{
		strcpy(confFileName,ABIOMASTER_SYBASE_IQ_CONFIG);
	}
	else if (strstr(ABIOMASTER_MYSQL_CONFIG,moduleName) != NULL)		//		MYSQL
	{
		strcpy(confFileName,ABIOMASTER_MYSQL_CONFIG);
	}
	else if (strstr(ABIOMASTER_ORACLE_RMAN_CONFIG,moduleName) != NULL)		//		ORACLE_RMAN
	{
		strcpy(confFileName,ABIOMASTER_ORACLE_RMAN_CONFIG);
	}
	else if (strstr(ABIOMASTER_NOTES_CONFIG,moduleName) != NULL)		//		NOTES
	{
		strcpy(confFileName,ABIOMASTER_NOTES_CONFIG);
	}
	else if (strstr(ABIOMASTER_UNISQL_CONFIG,moduleName) != NULL)		//		UNISQL
	{
		strcpy(confFileName,ABIOMASTER_UNISQL_CONFIG);
	}
	else if (strstr(ABIOMASTER_ALTIBASE_CONFIG,moduleName) != NULL)		//		ALTIBASE
	{
		strcpy(confFileName,ABIOMASTER_ALTIBASE_CONFIG);
	}
	else if (strstr(ABIOMASTER_INFORMIX_CONFIG,moduleName) != NULL)		//		INFORMIX
	{
		strcpy(confFileName,ABIOMASTER_INFORMIX_CONFIG);
	}
	else if (strstr(ABIOMASTER_INFORMIX_ONBAR_CONFIG,moduleName) != NULL)		//		INFORMIX_ONBAR
	{
		strcpy(confFileName,ABIOMASTER_INFORMIX_ONBAR_CONFIG);
	}
	else if (strstr(ABIOMASTER_VMWARE_CONFIG,moduleName) != NULL)		//		VMWARE
	{
		strcpy(confFileName,ABIOMASTER_VMWARE_CONFIG);
	}

	return 0;	
}

void SetModuleConf()
{
	char reply[DSIZ][DSIZ];
	char msg[DSIZ * DSIZ];
	
	va_sock_t commandSock;
	
	char ** lines;
	int * linesLength;
	int lineNumber;
	
	char requestCommand[MAX_ACTION_COMMAND_NAME_LENGTH];
	char actionCommand[MAX_ACTION_COMMAND_VALUE_LENGTH];
		
	char actionListModuleName[NUMBER_STRING_LENGTH];
	char modulePath[MAX_ACTION_COMMAND_VALUE_LENGTH];
	char moduleFileName[MAX_ACTION_COMMAND_VALUE_LENGTH];
	char* valueSize;

	int firstBlank;
	int secondBlank;
	
	int i;
	int j;
	int k;
	int length;
	int commandLength;
	int result;
	
	struct ckBackup *commandData;


	
	// set a version of node module
		
	commandData = (struct ckBackup *)malloc(sizeof(struct ckBackup));	
	memcpy(commandData,command.dataBlock , sizeof(struct ckBackup));	
	// initalize variables

	#ifdef __ABIO_WIN
	va_slash_to_backslash(commandData->mdouelOption.optionValue);
	#endif

	memset(modulePath, 0, sizeof(modulePath));	
	valueSize = NULL;

	result = 0;
		
	//not matched name pattern "exchange mailbox" in actionList file("MAILBOX EXCHANGE") and conf file name("exchange mailbox").	
	va_convert_moduleName_actionList(actionListModuleName,commandData->mdouelOption.moduleName);
	
	if ((lineNumber = va_load_text_file_lines(ABIOMASTER_CK_FOLDER, ACTION_LIST, &lines, &linesLength)) > 0)
	{		
		for (i = 0; i < lineNumber; i++)
		{			
			if (lines[i][0] == '\0')
			{
				continue;
			}
			
			memset(requestCommand, 0, sizeof(requestCommand));
			memset(actionCommand, 0, sizeof(actionCommand));
						
			for (j = 0; j < linesLength[i]; j++)
			{
				if (lines[i][j] == ' ' || lines[i][j] == '\t')
				{
					firstBlank = j;
					break;
				}
			}
			
			for (j = linesLength[i] - 1; j > 0; j--)
			{
				if (lines[i][j] == ' ' || lines[i][j] == '\t')
				{
					secondBlank = j;
					break;
				}
			}		
			
			strncpy(requestCommand, lines[i], firstBlank);
			strncpy(actionCommand, lines[i] + firstBlank + 1, secondBlank - firstBlank - 1);			
			
			if (strstr(requestCommand,actionListModuleName) != NULL)
			{
				length = 0;
				
				commandLength = (int)strlen(actionCommand); 

				for(k = 0; k < commandLength; k++)
				{					
					#ifdef __ABIO_WIN											
					if(actionCommand[k] == '\\')
					{
						length = k;
					}
					#else
					if(actionCommand[k] == '/')
					{
						length = k;
					}	
					#endif
				}
				strncpy(modulePath,actionCommand,length);
				break;				
			}
		}
		
		for (i = 0; i < lineNumber; i++)
		{
			va_free(lines[i]);
		}
		va_free(lines);
		va_free(linesLength);

		result = 1;
	}
	else
	{
		result = 2;
	}
	va_mapping_moduleName_confFile(moduleFileName,commandData->mdouelOption.moduleName);

	lineNumber = 0;
	
	if(modulePath != NULL)
	{	
		if ((lineNumber = va_load_text_file_lines(modulePath, moduleFileName, &lines, &linesLength)) > 0)
		{	
			for (i = 0; i < lineNumber; i++)
			{			
				if (lines[i][0] == '\0' || lines[i][1] == '*' ||  lines[i][1] == ' ')
				{					
					continue;
				}
							
				for (j = 0; j < linesLength[i]; j++)
				{
					if (lines[i][j] == ' ' || lines[i][j] == '\t')
					{						
						firstBlank = j;																		
						break;
					}					
				}

				memset(requestCommand, 0, sizeof(requestCommand));
				memset(actionCommand, 0, sizeof(actionCommand));

				strncpy(requestCommand, lines[i], firstBlank);
				strcpy(actionCommand, lines[i] + firstBlank + 1);			
				
				if (strstr(requestCommand,commandData->mdouelOption.optionBeforeName) != NULL)
				{					
					break;				
				}
			}

			if( i <= lineNumber )
			{	
				//UPDATE					
				memset(lines[i],0,linesLength[i]);
				
				linesLength[i] = ((int)strlen(commandData->mdouelOption.optionAfterName) + atoi(commandData->mdouelOption.optionLength) + 1);
							
				strcpy(lines[i],commandData->mdouelOption.optionAfterName);					
				strcpy(lines[i] + strlen(commandData->mdouelOption.optionAfterName) , " ");

				if(0 < atoi(commandData->mdouelOption.optionLength))
				{
					strcpy(lines[i] + strlen(commandData->mdouelOption.optionAfterName) + 1 , commandData->mdouelOption.optionValue);									
				}
				//strcpy(lines[i] + strlen(commandData->mdouelOption.optionAfterName) + 1 + atocommandData->mdouelOption.optionLength , "\n");		
				
				if(va_write_text_file_lines(modulePath,moduleFileName,lines,linesLength,lineNumber) != 0)
				{
					result = 3;
				}
				else
				{					
					result = 1;
				}
			}
			else
			{
				result = 4;
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
			result = 5;
		}
	}
	else
	{	
		result = 6;
	}
	
	if ((commandSock = va_make_client_socket_iptable(clientIP, masterIP, portRule, portRuleNumber, commandData->catalogPort,1)) != -1)	
	{		
		// make a reply message
		va_init_reply_buf(reply);
					
		strcpy(reply[0], "UPDATE_MODULE_CONF");		
		strcpy(reply[380], commandData->mdouelOption.optionAfterName);
		sprintf(reply[383],"%d",result);
				
		va_make_reply_msg(reply, msg);
		
		va_send(commandSock, msg, (int)strlen(msg), 0, DATA_TYPE_NOT_CHANGE);
		
		va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
	}
}

int va_convert_moduleName_actionList(char* actionListModuleName , char *moduleName)
{
	if(!strcmp(moduleName,"EXCHANGE_MAILBOX"))
	{
		strcpy(actionListModuleName,"MAILBOX_EXCHANGE");
	}
	else
	{
		strcpy(actionListModuleName,moduleName);
	}
	
	return 1;
}

int GetModuleConfigurationValue(char *ckFolder,char *moduleName,char * confFileName, char* optionName , char *value)
{
	int i,j,k;
	int req = 0;

	int length = 0,commandLength = 0;

	int lineNumber;
	int * linesLength;	
	char **lines;

	int firstBlank;
	int secondBlank;

	char requestCommand[MAX_ACTION_COMMAND_NAME_LENGTH];
	char actionCommand[MAX_ACTION_COMMAND_VALUE_LENGTH];

	char modulePath[MAX_ACTION_COMMAND_VALUE_LENGTH];
	
	if(ckFolder == NULL || moduleName == NULL || optionName == NULL ||  value == NULL)
	{
		return -1;
	}		

	if ((lineNumber = va_load_text_file_lines(ckFolder, ACTION_LIST , &lines, &linesLength)) > 0)
	{
		memset(modulePath, 0, sizeof(modulePath));

		for (i = 0; i < lineNumber; i++)
		{
			if (lines[i][0] == '\0')
			{
				continue;
			}
			
			memset(requestCommand, 0, sizeof(requestCommand));
			memset(actionCommand, 0, sizeof(actionCommand));
									
			for (j = 0; j < linesLength[i]; j++)
			{
				if (lines[i][j] == ' ' || lines[i][j] == '\t')
				{
					firstBlank = j;
					break;
				}
			}
			
			for (j = linesLength[i] - 1; j > 0; j--)
			{
				if (lines[i][j] == ' ' || lines[i][j] == '\t')
				{
					secondBlank = j;
					break;
				}
			}		
			
			strncpy(requestCommand, lines[i], firstBlank);
			strncpy(actionCommand, lines[i] + firstBlank + 1, secondBlank - firstBlank - 1);
			
			if (strstr(requestCommand, moduleName) != NULL)
			{				
				length = 0;
				
				commandLength = (int)strlen(actionCommand); 

				for(k = 0; k < commandLength; k++)
				{					
					#ifdef __ABIO_WIN											
					if(actionCommand[k] == '\\')
					{
						length = k;
					}
					#else
					if(actionCommand[k] == '/')
					{
						length = k;
					}	
					#endif
					
				}			

				strncpy(modulePath,actionCommand,length);				
				req = 1;
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
	
	if ((lineNumber = va_load_text_file_lines(modulePath, confFileName , &lines, &linesLength)) > 0)
	{
		for (i = 0; i < lineNumber; i++)
		{
			if (lines[i][0] == '\0')
			{
				continue;
			}
			
			memset(requestCommand, 0, sizeof(requestCommand));
									
			for (j = 0; j < linesLength[i]; j++)
			{
				if (lines[i][j] == ' ' || lines[i][j] == '\t')
				{
					firstBlank = j;
					break;
				}
			}
			
			strncpy(requestCommand, lines[i], firstBlank);
			strcpy(actionCommand, lines[i] + firstBlank + 1);
				
			if (strstr(requestCommand, optionName) != NULL)
			{				
				strcpy(value,actionCommand);				
				req = 1;
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

	return req;
}

void GetCatalogInfo()
{
	char reply[DSIZ][DSIZ];
	char msg[DSIZ * DSIZ];
	
	va_sock_t commandSock;
	
	struct abio_file_stat *stat;

	char ** folderFileList;	
	int fileNumber;
	
	int i;		
	
	__int64 CatalogSize = 0;
	__int64 FileListSize = 0;
	__int64 LogSize = 0;

	struct ckBackup *commandData;

	char requestCommand[MAX_ACTION_COMMAND_NAME_LENGTH];
	char actionCommand[MAX_ACTION_COMMAND_VALUE_LENGTH];
	
	char modulePath[MAX_ACTION_COMMAND_VALUE_LENGTH];
	char catalogPath[MAX_ACTION_COMMAND_VALUE_LENGTH];

	char slaveCatalogPath[MAX_ACTION_COMMAND_VALUE_LENGTH];
	char slaveLogPath[MAX_ACTION_COMMAND_VALUE_LENGTH];
	char slavePath[MAX_ACTION_COMMAND_VALUE_LENGTH];
	

	// initialize variables
	folderFileList = NULL;
	fileNumber = 0;
	
	commandData = (struct ckBackup *)malloc(sizeof(struct ckBackup));	
	memcpy(commandData,command.dataBlock , sizeof(struct ckBackup));	

	memset(requestCommand,0,sizeof(requestCommand));
	memset(actionCommand,0,sizeof(actionCommand));

	memset(modulePath,0,sizeof(modulePath));
	memset(catalogPath,0,sizeof(catalogPath));

	memset(slavePath,0,sizeof(slavePath));
	memset(slaveCatalogPath,0,sizeof(slaveCatalogPath));
	memset(slaveLogPath,0,sizeof(slaveLogPath));

	stat = (struct abio_file_stat *)malloc(sizeof(struct abio_file_stat));

	if ((commandSock = va_make_client_socket_iptable(clientIP, masterIP, portRule, portRuleNumber, commandData->catalogPort,1)) != -1)
	{		
		//Slave
		if(commandData->checkPoint == 2)	
		{
			if(0 < GetModuleConfigurationValue(ABIOMASTER_CK_FOLDER, "SLAVE",ABIOMASTER_SLAVE_CONFIG,SLAVE_CATALOG_DB_FOLDER_OPTION_NAME,slaveCatalogPath))
			{	
				memset(stat,0,sizeof(struct abio_file_stat));

				// repository의 파일들을 읽어와서 ABIO에서 만든 disk volume인지 확인하고, 맞으면 정보를 master server로 전송한다.
				if ((fileNumber = va_scandir(slaveCatalogPath, &folderFileList)) > 0)
				{
					for (i = 0; i < fileNumber; i++)
					{
						if(va_stat(slaveCatalogPath,folderFileList[i],stat) == 0)
						{					
							CatalogSize += stat->size;				
						}			
					}
					
					for (i = 0; i < fileNumber; i++)
					{
						va_free(folderFileList[i]);
					}
					va_free(folderFileList);
				}		
			}

			if(0 < GetModuleConfigurationValue(ABIOMASTER_CK_FOLDER, "SLAVE",ABIOMASTER_SLAVE_CONFIG,SLAVE_LOG_FOLDER_OPTION_NAME,slaveLogPath))
			{					

				memset(stat,0,sizeof(struct abio_file_stat));

				if ((fileNumber = va_scandir(slaveLogPath, &folderFileList)) > 0)
				{
					for (i = 0; i < fileNumber; i++)
					{
						if(va_stat(slaveLogPath,folderFileList[i],stat) == 0)
						{
							LogSize += stat->size;
						}			
					}
					
					for (i = 0; i < fileNumber; i++)
					{
						va_free(folderFileList[i]);
					}
					va_free(folderFileList);
				}
			}

			//Catalog Size
			va_init_reply_buf(reply);
			strcpy(reply[0], "CATALOG_INFO");
			
			va_itoa(commandData->checkPoint,reply[118]);			
			va_ulltoa(CatalogSize, reply[381]);
			strcpy(reply[380],slaveCatalogPath);
			va_backslash_to_slash(reply[380]);

			va_ulltoa(LogSize, reply[385]);
			strcpy(reply[384],slaveLogPath);
			va_backslash_to_slash(reply[384]);
		
			va_make_reply_msg(reply, msg);

			if (va_send(commandSock, msg, 1048576, 0, DATA_TYPE_NOT_CHANGE) < 0)
			{
				
			}											
		}

		CatalogSize = 0;
		LogSize = 0;

		memset(stat,0,sizeof(struct abio_file_stat));

		// repository의 파일들을 읽어와서 ABIO에서 만든 disk volume인지 확인하고, 맞으면 정보를 master server로 전송한다.
		if ((fileNumber = va_scandir(ABIOMASTER_CLIENT_CATALOG_DB_FOLDER, &folderFileList)) > 0)
		{
			for (i = 0; i < fileNumber; i++)
			{
				if(va_stat(ABIOMASTER_CLIENT_CATALOG_DB_FOLDER,folderFileList[i],stat) == 0)
				{					
					CatalogSize += stat->size;				
				}			
			}
			
			for (i = 0; i < fileNumber; i++)
			{
				va_free(folderFileList[i]);
			}
			va_free(folderFileList);
		}
			
		if ((fileNumber = va_scandir(ABIOMASTER_CLIENT_FILE_LIST_FOLDER, &folderFileList)) > 0)
		{
		
			for (i = 0; i < fileNumber; i++)
			{
				if(va_stat(ABIOMASTER_CATALOG_DB_FOLDER,folderFileList[i],stat) == 0)
				{					
					FileListSize += stat->size;
				}			
					
			}
			
			for (i = 0; i < fileNumber; i++)
			{
				va_free(folderFileList[i]);
			}
			va_free(folderFileList);
		}
	
		if ((fileNumber = va_scandir(ABIOMASTER_CLIENT_LOG_FOLDER, &folderFileList)) > 0)
		{
			for (i = 0; i < fileNumber; i++)
			{
				if(va_stat(ABIOMASTER_CLIENT_LOG_FOLDER,folderFileList[i],stat) == 0)
				{
					LogSize += stat->size;
				}			
			}
			
			for (i = 0; i < fileNumber; i++)
			{
				va_free(folderFileList[i]);
			}
			va_free(folderFileList);
		}

		//Catalog Size
		va_init_reply_buf(reply);
		strcpy(reply[0], "CATALOG_INFO");
		strcpy(reply[118], "3");

		va_ulltoa(CatalogSize, reply[381]);
		strcpy(reply[380],ABIOMASTER_CLIENT_CATALOG_DB_FOLDER);
		va_backslash_to_slash(reply[380]);
	
		va_ulltoa(FileListSize, reply[383]);
		strcpy(reply[382],ABIOMASTER_CLIENT_FILE_LIST_FOLDER);
		va_backslash_to_slash(reply[382]);
	
		va_ulltoa(LogSize, reply[385]);
		strcpy(reply[384],ABIOMASTER_CLIENT_LOG_FOLDER);
		va_backslash_to_slash(reply[384]);
	
		va_make_reply_msg(reply, msg);
		if (va_send(commandSock, msg, 1048576, 0, DATA_TYPE_NOT_CHANGE) < 0)
		{
			
		}			

		va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
	}

}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//int shrink(int argc, char ** argv)
//{
//	/*printf("\n-------------------------------------\n");
//	printf(" Catalog Shrink tool for VIRBAK ABIO\n");
//	printf("-------------------------------------\n\n");*/
//
//	// 전역 변수 초기화
//	Init();
//
//	// 메뉴 호출
//	MenuInit();
//
//	// 종료
//}


#ifdef __ABIO_WIN
	#define ABIOMASTER_DEFATIL_CLIENT_CATALOG_DB_FOLDER		"C:\\"ABIOMASTER_PRODUCT_NAME"\\"ABIOMASTER_BRAND_NAME"\\client\\catalog"
#else
	#define ABIOMASTER_DEFATIL_CLIENT_CATALOG_DB_FOLDER		"/usr/"ABIOMASTER_INSTALL_FOLDER"/catalog/client/catalog"
#endif

#define ABIOMASTER_CATALOG_DB_NEW  "newcatalog.db"
#define ABIOMASTER_CATALOG_DB_BAK  ".bak"

void changecatalog(char * catalogName)
{
	char input[DSIZ];
		
	input[0] = 'y';
	if (input[0] == 'y' || input[0] == 'Y')
	{
		//catalog.db 삭제
		//printf("Delete File : \"%s\"\n", ABIOMASTER_CATALOG_DB_FILE);
		va_remove(ABIOMASTER_CLIENT_CATALOG_DB_FOLDER, catalogName);
		//newcatalog.db -> catalog.db 이름변경
		//printf("Rename File : \"%s\" --> \"%s\"\n", ABIOMASTER_CATALOG_DB_NEW, ABIOMASTER_CATALOG_DB_FILE);
		va_rename(ABIOMASTER_CLIENT_CATALOG_DB_FOLDER, ABIOMASTER_CATALOG_DB_NEW, ABIOMASTER_CLIENT_CATALOG_DB_FOLDER, catalogName);		
	}
	else 
	{
		char ABIOMASTER_CATALOG_DB_FILE_BAK[MAX_PATH_LENGTH];
		memset(ABIOMASTER_CATALOG_DB_FILE_BAK, 0, MAX_PATH_LENGTH);
		sprintf(ABIOMASTER_CATALOG_DB_FILE_BAK, "%s%s", catalogName, ABIOMASTER_CATALOG_DB_BAK);

		//catalog.db -> catalog.db.bak 이름변경
		//printf("Rename File : \"%s\" --> \"%s\"\n", ABIOMASTER_CATALOG_DB_FILE, ABIOMASTER_CATALOG_DB_FILE_BAK);
		va_rename(ABIOMASTER_CLIENT_CATALOG_DB_FOLDER, catalogName, ABIOMASTER_CLIENT_CATALOG_DB_FOLDER, ABIOMASTER_CATALOG_DB_FILE_BAK);
		//newcatalog.db -> catalog.db 이름변경
		//printf("Rename File : \"%s\" --> \"%s\"\n", ABIOMASTER_CATALOG_DB_NEW, ABIOMASTER_CATALOG_DB_FILE);
		va_rename(ABIOMASTER_CLIENT_CATALOG_DB_FOLDER, ABIOMASTER_CATALOG_DB_NEW, ABIOMASTER_CLIENT_CATALOG_DB_FOLDER, catalogName);		
	}	
}

void SizeToStr(__uint64 size, char * szSize)
{
	if (size / 1024 / 1024 / 1024 / 1024 != 0)
	{
		sprintf(szSize, "%.2f TB", (__int64)size / 1024.0 / 1024.0 / 1024.0 / 1024.0);
	}
	else if (size / 1024 / 1024 / 1024 != 0)
	{
		sprintf(szSize, "%.2f GB", (__int64)size / 1024.0 / 1024.0 / 1024.0);
	}
	else if (size / 1024 / 1024 != 0)
	{
		sprintf(szSize, "%.2f MB", (__int64)size / 1024.0 / 1024.0);
	}
	else if (size / 1024 != 0)
	{
		sprintf(szSize, "%.2f KB", (__int64)size / 1024.0);
	}
	else
	{
		sprintf(szSize, "%.2f KB", (__int64)size / 1024.0);
	}
}

void ShrinkClientCatalog()
{
	char reply[DSIZ][DSIZ];
	char msg[DSIZ * DSIZ];
	
	va_sock_t commandSock;
	
	int totalcnt = 0;
	int expirecnt = 0;
	int deletecnt = 0;
	__uint64 decreasefilesize = 0;

	struct ckBackup *commandData;

	commandData = (struct ckBackup *)malloc(sizeof(struct ckBackup));	
	memcpy(commandData,command.dataBlock , sizeof(struct ckBackup));	

	if ((commandSock = va_make_client_socket_iptable(clientIP, masterIP, portRule, portRuleNumber, commandData->catalogPort,1)) != -1)
	{	
		if(0 < va_Shrink_Catalog(ABIOMASTER_CLIENT_CATALOG_DB_FOLDER,ABIOMASTER_CATALOG_DB_LATEST,0,&totalcnt,&expirecnt,&deletecnt,&decreasefilesize))
		{
			//Catalog Size
			va_init_reply_buf(reply);
			strcpy(reply[0], "SHRINK_CLIENT_CATALOG");
			sprintf(reply[200],"%d",totalcnt);
			sprintf(reply[201],"%d",expirecnt);
			sprintf(reply[202],"%d",deletecnt);
			sprintf(reply[203],"%d",decreasefilesize);
			va_make_reply_msg(reply, msg);
		}
		

		if (va_send(commandSock, msg, 1048576, 0, DATA_TYPE_NOT_CHANGE) < 0)
		{
			
		}					

		va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
	}

}

void DeleteSlaveCatalog()
{
	char reply[DSIZ][DSIZ];
	char msg[DSIZ * DSIZ];
	
	va_sock_t commandSock;
	
	struct abio_file_stat *stat;

	char ** folderFileList;	
	int fileNumber;
	
	int i;			
	int failCount;

	struct ckBackup *commandData;
		
	char slaveCatalogPath[MAX_ACTION_COMMAND_VALUE_LENGTH];
	
	// initialize variables
	folderFileList = NULL;
	fileNumber = 0;
	
	commandData = (struct ckBackup *)malloc(sizeof(struct ckBackup));	
	memcpy(commandData,command.dataBlock , sizeof(struct ckBackup));	

	memset(slaveCatalogPath,0,sizeof(slaveCatalogPath));

	stat = (struct abio_file_stat *)malloc(sizeof(struct abio_file_stat));

	if ((commandSock = va_make_client_socket_iptable(clientIP, masterIP, portRule, portRuleNumber, commandData->catalogPort,1)) != -1)
	{			
		if(0 < GetModuleConfigurationValue(ABIOMASTER_CK_FOLDER, "SLAVE",ABIOMASTER_SLAVE_CONFIG,SLAVE_CATALOG_DB_FOLDER_OPTION_NAME,slaveCatalogPath))
		{		
			failCount = 0;
			memset(stat,0,sizeof(struct abio_file_stat));

			// repository의 파일들을 읽어와서 ABIO에서 만든 disk volume인지 확인하고, 맞으면 정보를 master server로 전송한다.
			if ((fileNumber = va_scandir(slaveCatalogPath, &folderFileList)) > 0)
			{
				for (i = 0; i < fileNumber; i++)
				{
					if(va_remove(slaveCatalogPath,folderFileList[i]) != 0)
					{
						// 삭제 하지 못함.
						failCount++;
					}
				}
				
				for (i = 0; i < fileNumber; i++)
				{
					va_free(folderFileList[i]);
				}
				va_free(folderFileList);
			}				

			//Catalog Size
			va_init_reply_buf(reply);
			strcpy(reply[0], "DELETE_SLAVE_CATALOG");
			sprintf(reply[200],"%d",fileNumber);
			sprintf(reply[201],"%d",fileNumber - failCount);
			sprintf(reply[202],"%d",failCount);
			
			va_make_reply_msg(reply, msg);

			if (va_send(commandSock, msg, 1048576, 0, DATA_TYPE_NOT_CHANGE) < 0)
			{
				
			}			
		}
		else
		{
			//초기화 하지 못함.
		}					

		va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
	}

}

void DeleteSlaveLog()
{
	char reply[DSIZ][DSIZ];
	char msg[DSIZ * DSIZ];
	
	va_sock_t commandSock;
	
	struct abio_file_stat *stat;

	char ** folderFileList;	
	int fileNumber;
	int failCount;
	int i;		

	struct ckBackup *commandData;

	char slaveLogPath[MAX_ACTION_COMMAND_VALUE_LENGTH];

	// initialize variables
	folderFileList = NULL;
	fileNumber = 0;
	
	commandData = (struct ckBackup *)malloc(sizeof(struct ckBackup));	
	memcpy(commandData,command.dataBlock , sizeof(struct ckBackup));	

	memset(slaveLogPath,0,sizeof(slaveLogPath));

	stat = (struct abio_file_stat *)malloc(sizeof(struct abio_file_stat));

	if ((commandSock = va_make_client_socket_iptable(clientIP, masterIP, portRule, portRuleNumber, commandData->catalogPort,1)) != -1)
	{
		if(0 < GetModuleConfigurationValue(ABIOMASTER_CK_FOLDER, "SLAVE",ABIOMASTER_SLAVE_CONFIG,SLAVE_LOG_FOLDER_OPTION_NAME,slaveLogPath))
		{				
			failCount = 0;
			memset(stat,0,sizeof(struct abio_file_stat));

			if ((fileNumber = va_scandir(slaveLogPath, &folderFileList)) > 0)
			{
				for (i = 0; i < fileNumber; i++)
				{
					if(va_remove(slaveLogPath,folderFileList[i]) != 0)
					{
						// 삭제하지 못함.
						failCount++;
					}			
				}
				
				for (i = 0; i < fileNumber; i++)
				{
					va_free(folderFileList[i]);
				}
				va_free(folderFileList);
			}
			
			va_init_reply_buf(reply);
			strcpy(reply[0], "DELETE_SLAVE_LOG");
			sprintf(reply[200],"%d",fileNumber);
			sprintf(reply[201],"%d",fileNumber - failCount);
			sprintf(reply[202],"%d",failCount);

			va_make_reply_msg(reply, msg);

			if (va_send(commandSock, msg, 1048576, 0, DATA_TYPE_NOT_CHANGE) < 0)
			{
				
			}			
		}
		else
		{
			//초기화 하지 못함.
		}					

		va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
	}

}

