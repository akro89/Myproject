#include "require.h"
#include "abio.h"
#include "lib.h"
#include "vendor.h"
#include "ndmpc.h"
#include "restore.h"


// start of variables for abio library comparability
char ABIOMASTER_CK_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_MASTER_SERVER_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_DATA_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_CATALOG_DB_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_VOLUME_DB_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_FILE_LIST_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_LOG_FOLDER[MAX_PATH_LENGTH];
// end of variables for abio library comparability


char ABIOMASTER_NDMP_CLIENT_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_NDMP_CATALOG_DB_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_NDMP_FILE_LIST_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_NDMP_LOG_FOLDER[MAX_PATH_LENGTH];


struct ck command;
struct ckBackup commandData;


//////////////////////////////////////////////////////////////////////////////////////////////////
// ABIO log option
int requestLogLevel;
char logJobID[JOB_ID_LENGTH];
int logModuleNumber;


//////////////////////////////////////////////////////////////////////////////////////////////////
// slave server configuration
int tapeDriveTimeOut;
int tapeDriveDelayTime;


//////////////////////////////////////////////////////////////////////////////////////////////////
// message transfer variables
va_trans_t midCK;			// message transfer id to communicate between backup server and communication kernel
int mtypeCK;

va_trans_t midClient;		// message transfer id to receive a message from the master server
int mtypeClient;

va_trans_t midMedia;		// message transfer id to receive a message related media
int mtypeMedia;


//////////////////////////////////////////////////////////////////////////////////////////////////
// ndmp connect infomation
char ndmpAccount[NDMP_ACCOUNT_LENGTH];
char ndmpPasswd[NDMP_PASSWORD_LENGTH];
char ndmpIP[IP_LENGTH];
char ndmpPort[PORT_LENGTH];


//////////////////////////////////////////////////////////////////////////////////////////////////
// ndmp variables
ndmp_mover_listen_reply connmoveraddr;

int nMoverPauseState;
int nMoverHaltState;
int nDataHaltState;

int nMoverPauseReason;
int nMoverHaltReason;
int nDataHaltReason;

int DebugLevel;

//////////////////////////////////////////////////////////////////////////////////////////////////
// ndmp backupset file list
struct backupsetFile * backupsetFileList;
int backupsetFileListNumber;


//////////////////////////////////////////////////////////////////////////////////////////////////
// variables for the catalog db and volume db
char currentBackupset[BACKUPSET_ID_LENGTH];
char serverFilelistFile[MAX_PATH_LENGTH];


//////////////////////////////////////////////////////////////////////////////////////////////////
// variable for the tape drive
int volumeBlockSize;	// volume block size

char mountedVolume[VOLUME_NAME_LENGTH];	// 현재 mount되어있는 media
enum {DRIVE_CLOSED, DRIVE_OPENING} driveStatus;
char * tapeHeader;


//////////////////////////////////////////////////////////////////////////////////////////////////
// ip and port number
char masterIP[IP_LENGTH];					// master server ip address
char masterPort[PORT_LENGTH];				// master server ck port
char masterNodename[NODE_NAME_LENGTH];		// master server node name
char masterLogPort[PORT_LENGTH];			// master server httpd logger port
char serverIP[IP_LENGTH];					// backup server ip address
char serverPort[PORT_LENGTH];				// backup server ck port
char serverNodename[NODE_NAME_LENGTH];		// backup server node name

struct portRule * portRule;					// VIRBAK ABIO IP Table 목록
int portRuleNumber;							// VIRBAK ABIO IP Table 개수

int restoreNumber;							//restore 반복
//////////////////////////////////////////////////////////////////////////////////////////////////
// error control
// 리스토어 도중에 에러가 발생했을 때 에러가 발생한 장소를 기록한다. 0은 backup server error, 1은 master server error, 2는 client error를 의미한다.
// backup server나 master server에서 에러가 발생한 경우, 에러가 발생했다고 client에게 알려준다.
enum {ABIO_NDMP_CLIENT, ABIO_MASTER_SERVER} errorLocation;

int jobCanceled;
int errorOccured;
int jobComplete;


//////////////////////////////////////////////////////////////////////////////////////////////////
// job thread
va_thread_t tidNdmpRestore;
va_thread_t tidMessageTransfer;
va_thread_t tidLog;


//////////////////////////////////////////////////////////////////////////////////////////////////
// ndmp log
struct _backupLogDescription * backupLogDescription;
int backupLogDescriptionNumber;
extern SendJobLogFunc_t SendNdmpLogToMaster;


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
		
		
		if (InitNdmpServer() < 0)
		{
			Exit();
			
			return 2;
		}
		
		
		// 리스토어를 시작한다.
		if (Restore() < 0)
		{
			Exit();
			
			return 2;
		}
		
		
		Exit();
		
		return 0;
	}

int va_string_indexOf(char * str , char ch )
{
	int i;
	int length;

	if ( str == NULL)
	{
		return -1;
	}

	length = (int)strlen(str);

	for(i = 0 ; i < length ; i++)
	{
		//WriteDebugData(ABIOMASTER_CLIENT_LOG_FOLDER,ABIOMASTER_VMWARE_DEBUG,"ch = %c - %c",str[i],ch);
		if(str[i] == ch)
		{
			break;
		}
	}

	if(length <= i)
	{
		return -1;
	}

	return i;
}

int va_string_lastIndexOf(char * str , char ch )
{	
	int idx;
	int foundIdx;

	idx = -1;
	foundIdx = -1;

	while(0 < (idx = va_string_indexOf(str + (idx + 1), ch)))
	{
		foundIdx = idx;			
	}

	return foundIdx;
}

int InitProcess(char * filename)
{
	#ifdef __ABIO_WIN
		WSADATA wsaData;
	#endif
	
	int i;
	
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 1. 전역 변수들을 초기화한다.
	
	memset(ABIOMASTER_NDMP_CLIENT_FOLDER, 0, sizeof(ABIOMASTER_NDMP_CLIENT_FOLDER));
	memset(ABIOMASTER_NDMP_CATALOG_DB_FOLDER, 0, sizeof(ABIOMASTER_NDMP_CATALOG_DB_FOLDER));
	memset(ABIOMASTER_NDMP_FILE_LIST_FOLDER, 0, sizeof(ABIOMASTER_NDMP_FILE_LIST_FOLDER));
	memset(ABIOMASTER_NDMP_LOG_FOLDER, 0, sizeof(ABIOMASTER_NDMP_LOG_FOLDER));
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// ABIO log option
	requestLogLevel = LOG_LEVEL_JOB;
	memset(logJobID, 0, sizeof(logJobID));
	logModuleNumber = MODULE_UNKNOWN;
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// slave server configuration
	tapeDriveTimeOut = 0;
	tapeDriveDelayTime = 0;
	
	DebugLevel = 0;
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// message transfer variables
	midClient = (va_trans_t)-1;
	midMedia = (va_trans_t)-1;
	
	mtypeClient = 0;
	mtypeMedia = 0;
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// ndmp connect infomation
	memset(ndmpAccount, 0, sizeof(ndmpAccount));
	memset(ndmpPasswd, 0, sizeof(ndmpPasswd));;
	memset(ndmpIP, 0, sizeof(ndmpIP));
	memset(ndmpPort, 0, sizeof(ndmpPort));
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// ndmp variables
	memset(&connmoveraddr, 0, sizeof(ndmp_mover_listen_reply));
	
	nMoverPauseState = 0;
	nMoverHaltState = 0;
	nDataHaltState = 0;
	
	nMoverPauseReason = 0;
	nMoverHaltReason = 0;
	nDataHaltReason = 0;
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// ndmp backupset file list
	backupsetFileList = NULL;
	backupsetFileListNumber = 0;
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// variables for the catalog db and volume db
	memset(currentBackupset, 0, sizeof(currentBackupset));
	memset(serverFilelistFile, 0, sizeof(serverFilelistFile));
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// variable for the tape drive and the disk media
	volumeBlockSize = 0;
	
	memset(mountedVolume, 0, sizeof(mountedVolume));
	driveStatus = DRIVE_CLOSED;
	tapeHeader = NULL;
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// ip and port number
	memset(masterIP, 0, sizeof(masterIP));
	memset(masterPort, 0, sizeof(masterPort));
	memset(masterNodename, 0, sizeof(masterNodename));
	memset(masterLogPort, 0, sizeof(masterLogPort));
	memset(serverIP, 0, sizeof(serverIP));
	memset(serverPort, 0, sizeof(serverPort));
	memset(serverNodename, 0, sizeof(serverNodename));
	
	portRule = NULL;
	portRuleNumber = 0;
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// error control
	errorLocation = ABIO_NDMP_CLIENT;
	
	jobCanceled = 0;
	errorOccured = 0;
	jobComplete = 0;
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// job thread
	tidNdmpRestore = 0;
	tidMessageTransfer = 0;
	tidLog = 0;
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// ndmp log
	backupLogDescription = NULL;
	backupLogDescriptionNumber = 0;
	SendNdmpLogToMaster = SendJobLog;
	
	
	// restore Number
	restoreNumber = 0;

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 2. 각 시스템에 맞게 process를 초기화한다.
	
	// set a resource limit
	va_setrlimit();
	
	// set the ndmp client working directory
	for (i = (int)strlen(filename) - 1; i > 0; i--)
	{
		if (filename[i] == FILE_PATH_DELIMITER)
		{
			strncpy(ABIOMASTER_NDMP_CLIENT_FOLDER, filename, i);
			
			break;
		}
	}
	
	// set job id and module number for debug log of ABIO common library
	strcpy(logJobID, commandData.jobID);
	logModuleNumber = MODULE_NDMP_RESTORE;
	
	// set master server ip and port
	strcpy(masterIP, command.sourceIP);
	strcpy(masterPort, command.sourcePort);
	strcpy(masterNodename, command.sourceNodename);
	strcpy(masterLogPort, commandData.client.masterLogPort);
	
	// set backup server ip and port
	strcpy(serverIP, command.destinationIP);
	strcpy(serverPort, command.destinationPort);
	strcpy(serverNodename, command.destinationNodename);

	// set ndmp node ip and port
	strcpy(ndmpIP, commandData.client.ip);
	strcpy(ndmpPort, commandData.client.port);
	strcpy(ndmpAccount, commandData.ndmp.account);
	strcpy(ndmpPasswd, commandData.ndmp.passwd);
	
	#ifdef __ABIO_WIN
		// windows socket을 open한다.
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) 
		{
			Error(FUNCTION_InitProcess, ERROR_WIN_INIT_WIN_SOCK);
			
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
	
	LoadLogDescription();
	
	
	return 0;
}

int GetModuleConfiguration()
{
	struct confOption * moduleConfig;
	int moduleConfigNumber;
	
	int i;
	
	
	
	if (va_load_conf_file(ABIOMASTER_NDMP_CLIENT_FOLDER, ABIOMASTER_NDMP_CONFIG, &moduleConfig, &moduleConfigNumber) > 0)
	{
		for (i = 0; i < moduleConfigNumber; i++)
		{
			if (!strcmp(moduleConfig[i].optionName, NDMP_CATALOG_DB_FOLDER_OPTION_NAME))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(ABIOMASTER_NDMP_CATALOG_DB_FOLDER) - 1)
				{
					strcpy(ABIOMASTER_NDMP_CATALOG_DB_FOLDER, moduleConfig[i].optionValue);
				}
			}
			else if (!strcmp(moduleConfig[i].optionName, NDMP_FILE_LIST_FOLDER_OPTION_NAME))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(ABIOMASTER_NDMP_FILE_LIST_FOLDER) - 1)
				{
					strcpy(ABIOMASTER_NDMP_FILE_LIST_FOLDER, moduleConfig[i].optionValue);
				}
			}
			else if (!strcmp(moduleConfig[i].optionName, NDMP_LOG_FOLDER_OPTION_NAME))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(ABIOMASTER_NDMP_LOG_FOLDER) - 1)
				{
					strcpy(ABIOMASTER_NDMP_LOG_FOLDER, moduleConfig[i].optionValue);
				}
			}
			else if (!strcmp(moduleConfig[i].optionName, NDMP_TAPE_DRIVE_TIME_OUT_OPTION_NAME))
			{
				tapeDriveTimeOut = atoi(moduleConfig[i].optionValue);
			}
			else if (!strcmp(moduleConfig[i].optionName, NDMP_TAPE_DRIVE_DELAY_TIME_OPTION_NAME))
			{
				tapeDriveDelayTime = atoi(moduleConfig[i].optionValue);
			}
			else if (!strcmp(moduleConfig[i].optionName, ABIOMASTER_PORT_RULE_OPTION_NAME))
			{
				va_parse_port_rule(moduleConfig[i].optionValue, &portRule, &portRuleNumber);
			}
			else if (!strcmp(moduleConfig[i].optionName, DEBUG_LEVEL))
			{
				DebugLevel=atoi(moduleConfig[i].optionValue);
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
		Error(FUNCTION_GetModuleConfiguration, ERROR_CONFIG_OPEN_NDMP_CONFIG_FILE);

		return -1;
	}
	
	
	if (tapeDriveTimeOut < 60)
	{
		tapeDriveTimeOut = 60;
	}
	
	
	if (tapeDriveDelayTime == 0)
	{
		tapeDriveDelayTime = 1;
	}
	
	
	return 0;
}

void LoadLogDescription()
{
	struct confOption * moduleConfig;
	int moduleConfigNumber;
	
	int i;
	
	
	
	// abio log description file을 로드한다.
	if (va_load_conf_file(ABIOMASTER_NDMP_CLIENT_FOLDER, ABIOMASTER_BACKUP_LOG_DESCRIPTION, &moduleConfig, &moduleConfigNumber) > 0)
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

int InitNdmpServer()
{
	UpdateJobState(STATE_NDMP_RECV_REQUEST_DATA_TRANSFER);
	
	// set a media block size and data buffer size
	volumeBlockSize = commandData.mountStatus.blockSize;
	
	tapeHeader = (char *)malloc(sizeof(char) * volumeBlockSize);
	memset(tapeHeader, 0, sizeof(char) * volumeBlockSize);
	
	
	// set a server backupset block file name
	sprintf(serverFilelistFile, "%s.ndmp.list", commandData.jobID);
	
	
	// set the message transfer id to receive a message from the master server
	mtypeClient = commandData.mountStatus.mtype;
	#ifdef __ABIO_UNIX
		midClient = midCK;
	#elif __ABIO_WIN
		if ((midClient = va_make_message_transfer(mtypeClient)) == (va_trans_t)-1)
		{
			Error(FUNCTION_InitNdmpServer, ERROR_RESOURCE_MAKE_MESSAGE_TRANSFER);

			return -1;
		}
	#endif
	
	
	// set the message transfer id to receive a message related media
	mtypeMedia = mtypeClient * 10 + 1;
	#ifdef __ABIO_UNIX
		if ((midMedia = va_make_message_transfer(0)) == (va_trans_t)-1)
		{
			Error(FUNCTION_InitNdmpServer, ERROR_RESOURCE_MAKE_MESSAGE_TRANSFER);

			return -1;
		}
	#elif __ABIO_WIN
		if ((midMedia = va_make_message_transfer(mtypeMedia)) == (va_trans_t)-1)
		{
			Error(FUNCTION_InitNdmpServer, ERROR_RESOURCE_MAKE_MESSAGE_TRANSFER);

			return -1;
		}
	#endif
	
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

va_thread_return_t InnerMessageTransfer(void * arg)
{
	struct ck recvCommand;
	struct ckBackup recvCommandData;
	
	
	
	va_sleep(1);
	
	while (1)
	{
		// receive a message from master server
		if (va_msgrcv(midClient, mtypeClient, &recvCommand, sizeof(struct ck), 0) <= 0)
		{
			break;
		}
		memcpy(&recvCommandData, recvCommand.dataBlock, sizeof(struct ckBackup));
		
		
		// send the message to appropriate thread
		if (!strcmp(recvCommand.subCommand, "EXIT_INNER_MESSAGE_TRANSFER"))
		{
			break;
		}
		else
		{
			if (!strcmp(recvCommandData.jobID, commandData.jobID))
			{
				if (!strcmp(recvCommand.subCommand, "MOUNT_NEXT_VOLUME"))
				{
					va_msgsnd(midMedia, mtypeMedia, &recvCommand, sizeof(struct ck), 0);
				}
				else if (!strcmp(recvCommand.subCommand, "CANCEL_JOB"))
				{
					CancelJob(&recvCommandData);
				}
			}
		}
	}
	
	
	#ifdef __ABIO_UNIX
		return NULL;
	#else
		_endthreadex(0);
		return 0;
	#endif
}

int Restore()
{
	NdmpConnection connection;
	if(0 < DebugLevel)
	{
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_RESTORE_LOG, "Restore() Start");
	}
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 1. get restore file list
	
	if (GetBackupsetFileList() < 0)
	{
		return -1;
	}
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 2. making a connection with ndmp server
	
	if ((connection = MakeNewConnection()) == NULL)
	{
		return -1;
	}
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 3. run threads
	
	// run the inner message transfer
	if ((tidMessageTransfer = va_create_thread(InnerMessageTransfer, NULL)) == 0)
	{
		Error(FUNCTION_Restore, ERROR_RESOURCE_MAKE_THREAD);
		va_ndmp_connect_close(connection);
		return -1;
	}

	if ((tidNdmpRestore = va_create_thread(NdmpRestore, connection)) == 0)
	{
		Error(FUNCTION_Restore, ERROR_RESOURCE_MAKE_THREAD);
		va_ndmp_connect_close(connection);
		return -1;
	}
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 4. restore가 끝나기를 기다린다
	
	// restore가 끝나기를 기다린다.
	va_wait_thread(tidNdmpRestore);
	
	// ndmp server와의 접속을 종료한다.
	va_ndmp_connect_close(connection);
	if(0 < DebugLevel)
	{
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_RESTORE_LOG, "Restore() End");
	}
	
	return 0;
}

int GetBackupsetFileList()
{
	va_fd_t fdBackupsetFilelist;
	char filePathSource[ABIO_FILE_LENGTH * 2];
	char filePathTarget[ABIO_FILE_LENGTH * 2];
	
	int i;
	int j;
	
	if(0 < DebugLevel)
	{
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_RESTORE_LOG, "GetBackupsetFileList() Start");
	}
	
	if ((fdBackupsetFilelist = va_open(ABIOMASTER_NDMP_FILE_LIST_FOLDER, serverFilelistFile, O_RDONLY, 1, 0)) != (va_fd_t)-1)
	{
		// read a backupset file list number
		va_read(fdBackupsetFilelist, &backupsetFileListNumber, sizeof(backupsetFileListNumber), DATA_TYPE_NOT_CHANGE);
		backupsetFileListNumber = ntohl(backupsetFileListNumber);		// network to host
		backupsetFileList = (struct backupsetFile *)malloc(sizeof(struct backupsetFile) * backupsetFileListNumber);
		memset(backupsetFileList, 0, sizeof(struct backupsetFile) * backupsetFileListNumber);
		if(0 < DebugLevel)
		{
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_RESTORE_LOG, "backupsetFileListNumber : %d", backupsetFileListNumber);
		}
		for (i = 0; i < backupsetFileListNumber; i++)
		{
			// read a backupset id
			va_read(fdBackupsetFilelist, backupsetFileList[i].backupset, sizeof(backupsetFileList[i].backupset), DATA_TYPE_NOT_CHANGE);
			
			// read a backupset file size
			va_read(fdBackupsetFilelist, &backupsetFileList[i].fileSize, sizeof(backupsetFileList[i].fileSize), DATA_TYPE_NOT_CHANGE);
			backupsetFileList[i].fileSize = va_htonll(backupsetFileList[i].fileSize);		// network to host
			
			// read a backupset media number
			va_read(fdBackupsetFilelist, &backupsetFileList[i].mediaNumber, sizeof(backupsetFileList[i].mediaNumber), DATA_TYPE_NOT_CHANGE);
			backupsetFileList[i].mediaNumber = ntohl(backupsetFileList[i].mediaNumber);		// network to host
			

			if(0 < DebugLevel)
			{
				WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_RESTORE_LOG, "backupsetFileList[i].backupset : %s", backupsetFileList[i].backupset);
				WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_RESTORE_LOG, "backupsetFileList[i].fileSize : %lld", backupsetFileList[i].fileSize);
				WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_RESTORE_LOG, "backupsetFileList[i].mediaNumber : %d", backupsetFileList[i].mediaNumber);
			}

			backupsetFileList[i].mediaName = (char **)malloc(sizeof(char *) * backupsetFileList[i].mediaNumber);
			memset(backupsetFileList[i].mediaName, 0, sizeof(char *) * backupsetFileList[i].mediaNumber);
			
			backupsetFileList[i].backupsetPosition = (__uint64 *)malloc(sizeof(__uint64) * backupsetFileList[i].mediaNumber);
			memset(backupsetFileList[i].backupsetPosition, 0, sizeof(__uint64) * backupsetFileList[i].mediaNumber);
			
			
			for (j = 0; j < backupsetFileList[i].mediaNumber; j++)
			{
				// read a backupset media name
				backupsetFileList[i].mediaName[j] = (char *)malloc(sizeof(char) * VOLUME_NAME_LENGTH);
				va_read(fdBackupsetFilelist, backupsetFileList[i].mediaName[j], VOLUME_NAME_LENGTH, DATA_TYPE_NOT_CHANGE);
				
				// read a backupset position
				va_read(fdBackupsetFilelist, backupsetFileList[i].backupsetPosition + j, sizeof(backupsetFileList[i].backupsetPosition[0]), DATA_TYPE_NOT_CHANGE);
				backupsetFileList[i].backupsetPosition[j] = va_htonll(backupsetFileList[i].backupsetPosition[j]);		// network to host

				if(0 < DebugLevel)
				{
					WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_RESTORE_LOG, "j : %d, backupsetFileList[i].mediaName[j] : %s", j, backupsetFileList[i].mediaName[j]);
					WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_RESTORE_LOG, "backupsetFileList[i].backupsetPosition : %lld", backupsetFileList[i].backupsetPosition);
					WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_RESTORE_LOG, "backupsetFileList[i].backupsetPosition[0] : %lld", backupsetFileList[i].backupsetPosition[0]);
					//WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_RESTORE_LOG, "backupsetFileList[i].backupsetPosition[1] : %lld", backupsetFileList[i].backupsetPosition[1]);
				}
			}
			
			// read a restore file number
			va_read(fdBackupsetFilelist, &backupsetFileList[i].fileNumber, sizeof(backupsetFileList[i].fileNumber), DATA_TYPE_NOT_CHANGE);
			backupsetFileList[i].fileNumber = ntohl(backupsetFileList[i].fileNumber);
			
			backupsetFileList[i].file = (struct backupsetRestoreFile *)malloc(sizeof(struct backupsetRestoreFile) * backupsetFileList[i].fileNumber);
			memset(backupsetFileList[i].file, 0, sizeof(struct backupsetRestoreFile) * backupsetFileList[i].fileNumber);
			if(0 < DebugLevel)
			{
				WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_RESTORE_LOG, "backupsetFileList[i].fileNumber : %d", backupsetFileList[i].fileNumber);
			}
			for (j = 0; j < backupsetFileList[i].fileNumber; j++)
			{
				// read a restore source file path
				va_read(fdBackupsetFilelist, filePathSource, sizeof(filePathSource), DATA_TYPE_NOT_CHANGE);
				backupsetFileList[i].file[j].sourceFile = (char *)malloc(sizeof(char) * (strlen(filePathSource) + 1));
				memset(backupsetFileList[i].file[j].sourceFile, 0, sizeof(char) * (strlen(filePathSource) + 1));
				strcpy(backupsetFileList[i].file[j].sourceFile, filePathSource);
				
				// read a restore target file path
				va_read(fdBackupsetFilelist, filePathTarget, sizeof(filePathTarget), DATA_TYPE_NOT_CHANGE);
				backupsetFileList[i].file[j].targetFile = (char *)malloc(sizeof(char) * (strlen(filePathTarget) + 1));
				memset(backupsetFileList[i].file[j].targetFile, 0, sizeof(char) * (strlen(filePathTarget) + 1));
				strcpy(backupsetFileList[i].file[j].targetFile, filePathTarget);
				
				// read an inode
				va_read(fdBackupsetFilelist, &backupsetFileList[i].file[j].inode, sizeof(backupsetFileList[i].file[j].inode), DATA_TYPE_NOT_CHANGE);
				backupsetFileList[i].file[j].inode = va_htonll(backupsetFileList[i].file[j].inode);		// network to host
				
				// read an fh info
				va_read(fdBackupsetFilelist, &backupsetFileList[i].file[j].fh_info, sizeof(backupsetFileList[i].file[j].fh_info), DATA_TYPE_NOT_CHANGE);
				backupsetFileList[i].file[j].fh_info = va_htonll(backupsetFileList[i].file[j].fh_info);		// network to host

				if(0 < DebugLevel)
				{
					WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_RESTORE_LOG, "backupsetFileList[i].file[j].sourceFile : %s", backupsetFileList[i].file[j].sourceFile);
					WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_RESTORE_LOG, "backupsetFileList[i].file[j].targetFile : %s", backupsetFileList[i].file[j].targetFile);
					WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_RESTORE_LOG, "backupsetFileList[i].file[j].inode : %lld", backupsetFileList[i].file[j].inode);
					WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_RESTORE_LOG, "backupsetFileList[i].file[j].fh_info : %lld", backupsetFileList[i].file[j].fh_info);
				}
			}
		}
		
		va_close(fdBackupsetFilelist);
	}
	else
	{
		Error(FUNCTION_GetBackupsetFileList, ERROR_NDMP_OPEN_RESTORE_FILE_LIST);
		return -1;
	}
	if(0 < DebugLevel)
	{
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_RESTORE_LOG, "GetBackupsetFileList() End");
	}
	
	return 0;
}

NdmpConnection MakeNewConnection()
{
	NdmpConnection connection;
	va_sock_t clientSock;
	
	int r;
	
	
	
	// client가 살아있는지 확인한다.
	if ((clientSock = va_connect(ndmpIP, ndmpPort, 1)) != -1)
	{
		va_close_socket(clientSock, ABIO_SOCKET_CLOSE_CLIENT);
	}
	else
	{
		Error(FUNCTION_MakeNewConnection, ERROR_NETWORK_CLIENT_DOWN);
		
		return NULL;
	}
	
	// ndmp server에 접속한다.
	if ((r = va_ndmp_connect_ndmp_server(ndmpIP, (unsigned long)atol(ndmpPort), ndmpAccount, ndmpPasswd, &connection)) != 0)
	{
		if (r == -1)
		{
			Error(FUNCTION_MakeNewConnection, ERROR_NDMP_CONNECT_RPC);
		}
		else if (r == -2)
		{
			Error(FUNCTION_MakeNewConnection, ERROR_NDMP_CONNECT_CONNECTION);
		}
		else if (r == -3)
		{
			Error(FUNCTION_MakeNewConnection, ERROR_NDMP_CONNECT_NEGOTIATION);
		}
		else if (r == -4)
		{
			Error(FUNCTION_MakeNewConnection, ERROR_NDMP_CONNECT_AUTHENTICATION);
		}
		else
		{
			Error(FUNCTION_MakeNewConnection, ERROR_NDMP_CONNECT);
		}
		
		return NULL;
	}
	
	// job log를 master server에 보낼 thread를 만든다.
	if ((tidLog = va_create_thread(WriteRunningLog, NULL)) == 0)
	{
		Error(FUNCTION_MakeNewConnection, ERROR_RESOURCE_MAKE_THREAD);
		va_ndmp_connect_close(connection);
		
		return NULL;
	}
	
	
	return connection;
}

va_thread_return_t NdmpRestore(void * arg)
{
	if(0 < DebugLevel)
	{
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_RESTORE_LOG, "NdmpRestore() Start");
	}
	NdmpConnection connection;
	
	time_t readStartTime;
	time_t readEndTime;
	
	int ndmpRestoreSuccess;
	int ndmpRestoreFail;
	
	int totalBlockNumber;
	int currentBlockNumber;
	
	int len;
	int i;
	int j;
	int k;
	int r;
	
	
	
	va_sleep(1);
	
	// ndmp server와의 connection을 설정한다.
	connection = arg;
	
	totalBlockNumber = 0;
	currentBlockNumber = 0;
	
	
	// volume을 load한다.
	LoadNdmpTapeMedia(connection);
	
	for (i = 0; i < backupsetFileListNumber; i++)
	{
		if(0 < DebugLevel)
		{
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_RESTORE_LOG, "backupsetFileListNumber : %d, i : %d", backupsetFileListNumber, i);
		}

		//////////////////////////////////////////////////////////////////////////////////////////////////
		// 1. initialize variables
		ndmpRestoreSuccess = 0;
		ndmpRestoreFail = 0;
		
		nDataHaltState = 0;
		nMoverHaltState = 0;
		nMoverPauseReason = 0;
		nDataHaltReason = 0;
		nMoverHaltReason = 0;
		nMoverPauseState = 0;
		
		
		j = 0;
		
		// 리스토어할 nas volume을 리스토어중인 파일로 표시한다.
		len = (int)strlen(backupsetFileList[i].file[0].sourceFile);
		for (k = 0; k < len; k++)
		{
			if (backupsetFileList[i].file[0].sourceFile[k] == ':')
			{
				break;
			}
		}
		
		if (k < len)
		{
			strncpy(commandData.backupFile, backupsetFileList[i].file[0].sourceFile, k);
		}
		else
		{
			strcpy(commandData.backupFile, "NDMP DUMP IMAGE");
		}
		
		
		//////////////////////////////////////////////////////////////////////////////////////////////////
		// 2. load the media
		
		if (jobComplete == 0)
		{
			// backupset position으로 이동한다.
			if (va_ndmp_library_forward_filemark(connection, (int)backupsetFileList[i].backupsetPosition[0]) != 0)
			{
				Error(FUNCTION_NdmpRestore, ERROR_VOLUME_MOVE_FILEMARK);
			}
			// set a tape block size
			else if (va_ndmp_library_set_tape_block_size(connection, volumeBlockSize) != 0)
			{
				Error(FUNCTION_NdmpRestore, ERROR_VOLUME_TAPE_BLOCK_SIZE);
			}
		}
		
		
		//////////////////////////////////////////////////////////////////////////////////////////////////
		// 3. start ndmp restore
		
		// 리스토어 정보를 설정하고, 리스토어 목록을 등록한다.
		if (jobComplete == 0)
		{
			// dump level 설정 value "0" 는 full/incremental에 따라 바뀌어야함.
			if (va_ndmp_data_add_env(connection, "LEVEL", "0") != 0)
			{
				Error(FUNCTION_NdmpRestore, ERROR_NDMP_ENV_ADD_LEVEL);
			}
			// 리스토어할 파일이 속해있던 volume을 등록한다.
			else if (va_ndmp_data_add_env(connection, "FILESYSTEM", commandData.backupFile) != 0)
			{
				Error(FUNCTION_NdmpRestore, ERROR_NDMP_ENV_ADD_FILESYSTEM);
			}
			// 리스토어해야될 목록을 등록한다.
			else if (AddRestoreList(connection, i) != 0)
			{
				Error(FUNCTION_NdmpRestore, ERROR_NDMP_ADD_RESTORE_LIST);
			}
			// 작업이 진행된 크기를 보여준다.
			else if (commandData.writeDetailedLog == 1 && va_ndmp_data_add_env(connection, "VERBOSE", "Y") != 0)
			{
				Error(FUNCTION_NdmpRestore, ERROR_NDMP_ENV_ADD_VERBOSE);
			}
		}
		
		// DAR 작업을 위한 옵션을 설정한다.
		if (jobComplete == 0)
		{
			if (commandData.ndmpDAR == 1)
			{
				if (va_ndmp_data_add_env(connection, "DIRECT", "Y") != 0)
				{
					Error(FUNCTION_NdmpRestore, ERROR_NDMP_ENV_ADD_DIRECT);
				}
				else if (va_ndmp_data_add_env(connection, "ENHANCED_DAR_ENABLED", "T") != 0)
				{
					Error(FUNCTION_NdmpRestore, ERROR_NDMP_ENV_ADD_ENHANCED_DAR_ENABLED);
				}
			}
		}
		
		#ifdef __ABIO_DEBUG
			if (jobComplete == 0)
			{
				if (va_ndmp_data_add_env(connection, "DEBUG", "Y") != 0)
				{
					Error(FUNCTION_NdmpRestore, ERROR_NDMP_ENV_ADD_DEBUG);
				}
				/*
				else if (va_ndmp_data_add_env(connection, "LIST", "Y") != 0)
				{
					Error(FUNCTION_NdmpRestore, ERROR_NDMP_ENV_ADD_LIST);
				}
				else if (va_ndmp_data_add_env(connection, "LIST_QTREES", "Y") != 0)
				{
					Error(FUNCTION_NdmpRestore, ERROR_NDMP_ENV_ADD_LIST_QTREES);
				}
				*/
			}
		#endif
		
		// 리스토어 작업을 시작한다.
		if (jobComplete == 0)
		{
			// mover service (tape에서 데이터를 읽어서 data service로 보내주는 service) listen for data service
			if (va_ndmp_mover_listen(connection, "write", "tcp") != 0)
			{
				Error(FUNCTION_NdmpRestore, ERROR_NDMP_MOVER_LISTEN);
			}
			// DAR 옵션이 적용된 경우 mover에게 읽어야할 데이터가 시작된 위치를 알려준다.
			else if (commandData.ndmpDAR == 1 && va_ndmp_mover_set_window(connection, 0, (__uint64)-1) != 0)
			{
				Error(FUNCTION_NdmpRestore, ERROR_NDMP_MOVER_SET_WINDOW);
			}
			// data service (mover service에서 데이터를 받아서 disk에 쓰는 service) connect to mover service
			else if (va_ndmp_data_connect(connection, "tcp", NULL, NULL) != 0)
			{
				Error(FUNCTION_NdmpRestore, ERROR_NDMP_DATA_CONNECT);
			}
			// data service에게 리스토어를 시작하라고 알려준다.
			else if (va_ndmp_data_start_recover(connection, "dump") != 0)
			{
				Error(FUNCTION_NdmpRestore, ERROR_NDMP_DATA_START_RECOVER);
			}
			// mover service에게 tape에서 데이터를 읽기 시작하라고 알려준다.
			else if (va_ndmp_mover_read(connection, 0, backupsetFileList[i].fileSize) != 0)
			{
				Error(FUNCTION_NdmpRestore, ERROR_NDMP_MOVER_READ);
			}
		}
		
		UpdateJobState(STATE_NDMP_READING);
		
		if (i == 0)
		{
			readStartTime = time(NULL);
		}
		
		
		//////////////////////////////////////////////////////////////////////////////////////////////////
		// 4. read data from the tape media
		while (ndmpRestoreSuccess == 0 && ndmpRestoreFail == 0 && jobComplete == 0)
		{
			// ndmp server로부터 mover halt state가 먼저 오고, 뒤이어 data halt state가 오는 경우 먼저 온 mover halt state만 처리하고 ndmp state를 검사하는 경우에, 에러가 발생한 것으로 처리된다.
			// 이처럼 ndmp server로부터 연속해서 메세지가 오는 경우에 메세지 순서와 타이밍의 문제로 인해 에러가 발생한 것으로 처리될 가능성이 있다.
			// 그래서 ndmp polling interval 내에 ndmp server로부터 오는 메세지를 모두 처리한 뒤에 ndmp state를 검사하도록 한다.
			while ((r = va_ndmp_util_poll_ndmp_request(connection, NDMP_POLLING_INTERVAL)) == 1 && jobComplete == 0)
			{
				if (va_ndmp_comm_process_request(connection) < 0)
				{
					Error(FUNCTION_NdmpRestore, ERROR_NDMP_REQUEST_PROCESS);
					ndmpRestoreFail = 1;
					break;
				}
				
				// 리스토어 진행 상황을 업데이트한다.
				if (va_ndmp_tape_get_current_block_number(connection, &currentBlockNumber) != 0 || currentBlockNumber < 0)
				{
					currentBlockNumber = 0;
				}
				
				commandData.capacity = (__uint64)(totalBlockNumber + currentBlockNumber) * volumeBlockSize;
				
				readEndTime = time(NULL);
				commandData.writeTime = readEndTime - readStartTime;
			}
			
			if (r < 0)
			{
				Error(FUNCTION_NdmpRestore, ERROR_NDMP_REQUEST_POLL);
				ndmpRestoreFail = 1;
				break;
			}
			
			// 리스토어 진행 상황을 업데이트한다.
			if (va_ndmp_tape_get_current_block_number(connection, &currentBlockNumber) != 0 || currentBlockNumber < 0)
			{
				currentBlockNumber = 0;
			}
			
			commandData.capacity = (__uint64)(totalBlockNumber + currentBlockNumber) * volumeBlockSize;
			
			readEndTime = time(NULL);
			commandData.writeTime = readEndTime - readStartTime;
			
			// ndmp state를 검사한다.
			if (ndmpRestoreSuccess == 0 && ndmpRestoreFail == 0 && jobComplete == 0)
			{
				// data write 가 종료되었다. 이유는?
				if (nDataHaltState == 1)
				{
					switch (nDataHaltReason)
					{
						case NDMP_DATA_HALT_SUCCESSFUL:			// 성공했다.
							// 리스토어 성공으로 종료 처리.
							ndmpRestoreSuccess = 1;
							break;
						
						case NDMP_DATA_HALT_ABORTED:			// 취소되었다.
							// 리스토어가 취소되었다.
							Error(FUNCTION_NdmpRestore, ERROR_NDMP_DATA_HALT_ABORTED);
							ndmpRestoreFail = 1;
							break;
						
						case NDMP_DATA_HALT_INTERNAL_ERROR:		// NDMP서버 내부의 오류로 종료되었다.
							Error(FUNCTION_NdmpRestore, ERROR_NDMP_DATA_HALT_INTERNAL_ERROR);
							ndmpRestoreFail = 1;
							break;
						
						case NDMP_DATA_HALT_CONNECT_ERROR:		// data connect가 오류로 종료되었다.
							Error(FUNCTION_NdmpRestore, ERROR_NDMP_DATA_HALT_CONNECT_ERROR);
							ndmpRestoreFail = 1;
							break;
						
						case NDMP_DATA_HALT_NA:					// data halt 가 아니다! 이런일은 없을 것이다. 있다면 오류로 처리해야함.
						default:								//알 수 없는 이유다.
							Error(FUNCTION_NdmpRestore, ERROR_NDMP_DATA_HALT_UNKNOWN);
							ndmpRestoreFail = 1;
							break;
					}
				}
				
				// tape read가 종료되었다. 이유는?
				if (nMoverHaltState == 1)
				{
					switch (nMoverHaltReason)
					{
						case NDMP_MOVER_HALT_CONNECT_CLOSED:	// connect가 닫혀서 종료되었다.
							// connect 가 종료되었다. restore 이 성공한 것이 아니라면 취소된 것이다.
							if (nDataHaltState == 0)
							{
								Error(FUNCTION_NdmpRestore, ERROR_NDMP_MOVER_HALT_CONNECT_CLOSED);
								ndmpRestoreFail = 1;
							}
							break;
						
						case NDMP_MOVER_HALT_ABORTED:			// 취소되었다.
							// 취소처리.
							Error(FUNCTION_NdmpRestore, ERROR_NDMP_MOVER_HALT_ABORTED);
							ndmpRestoreFail = 1;
							break;
						
						case NDMP_MOVER_HALT_INTERNAL_ERROR:	// NDMP서버 내부의 오류로 종료되었다.
							Error(FUNCTION_NdmpRestore, ERROR_NDMP_MOVER_HALT_INTERNAL_ERROR);
							ndmpRestoreFail = 1;
							break;
						
						case NDMP_MOVER_HALT_CONNECT_ERROR:		// 접속 오류로 종료되었다.
							Error(FUNCTION_NdmpRestore, ERROR_NDMP_MOVER_HALT_CONNECT_ERROR);
							ndmpRestoreFail = 1;
							break;
						
						case NDMP_MOVER_HALT_NA:				// mover가 halt되지 않았다! 이런일은 없을 것이다. 있다면 오류로 처리해야함.
						default:								//알 수 없는 이유다.
							Error(FUNCTION_NdmpRestore, ERROR_NDMP_MOVER_HALT_UNKNOWN);
							ndmpRestoreFail = 1;
							break;
					}
				}
				
				// tape read가 멈췄다. 이유는?
				if (nMoverPauseState == 1)
				{
					//nMoverPauseReason
					switch (nMoverPauseReason)
					{
						case NDMP_MOVER_PAUSE_SEEK:				// 찾는 곳이 data window의 바깥쪽이어서 pause 되었다.
						case NDMP_MOVER_PAUSE_EOM:				// mover 가 tape의 끝에 도달해서 pause 되었다.
							nMoverPauseState = 0;
							nMoverPauseReason = 0;
							
							// 리스토어 진행 상황을 업데이트한다.
							if (va_ndmp_tape_get_current_block_number(connection, &currentBlockNumber) != 0 || currentBlockNumber < 0)
							{
								currentBlockNumber = 0;
							}
							
							totalBlockNumber += currentBlockNumber;
							commandData.capacity = (__uint64)totalBlockNumber * volumeBlockSize;
							
							// 다음 media를 mount한다.
							j++;
							
							// media의 끝에 도달했는데 다음 media가 없다는 것은 무언가 잘못되었다는 뜻이다.
							if (j == backupsetFileList[i].mediaNumber)
							{
								Error(FUNCTION_NdmpRestore, ERROR_NDMP_THERE_IS_NO_NEXT_VOLUME);
								ndmpRestoreFail = 1;
								break;
							}
							
							// unload next media load
							if (MountNdmpNextVolume(connection, backupsetFileList[i].mediaName[j]) < 0)
							{
								ndmpRestoreFail = 1;
								break;
							}
							
							if (va_ndmp_library_forward_filemark(connection, (int)backupsetFileList[i].backupsetPosition[j]) != 0)
							{
								Error(FUNCTION_NdmpRestore, ERROR_VOLUME_MOVE_FILEMARK);
								ndmpRestoreFail = 1;
								break;
							}
							
							// DAR 옵션이 적용된 경우 mover에게 읽어야할 데이터가 시작된 위치를 알려준다.
							if (commandData.ndmpDAR == 1 && va_ndmp_mover_set_window(connection, 0, (__uint64)-1) != 0)
							{
								Error(FUNCTION_NdmpRestore, ERROR_NDMP_MOVER_SET_WINDOW);
								ndmpRestoreFail = 1;
								break;
							}
							
							// mover continue
							if (va_ndmp_mover_continue(connection) != 0)
							{
								Error(FUNCTION_NdmpRestore, ERROR_NDMP_MOVER_CONTINUE);
								ndmpRestoreFail = 1;
								break;
							}
							
							UpdateJobState(STATE_NDMP_READING);
							break;
						
						case NDMP_MOVER_PAUSE_EOF:				// mover 가 tape에서 file mark에 도달해서 pause 되었다. 이 경우 file restore가 성공했다고 봐야한다.
							nMoverPauseState = 0;
							nMoverPauseReason = 0;
							ndmpRestoreSuccess = 1;
							break;
						
						case NDMP_MOVER_PAUSE_MEDIA_ERROR:		// tape의 read/write 오류가 발생해서 pause 되었다.
							Error(FUNCTION_NdmpRestore, ERROR_NDMP_MOVER_PAUSE_MEDIA_ERROR);
							ndmpRestoreFail = 1;
							break;
						
						case NDMP_MOVER_PAUSE_EOW:				// 작업이 mover window의 끝에 도달하여서 pause 되었다.
							Error(FUNCTION_NdmpRestore, ERROR_NDMP_MOVER_PAUSE_EOW);
							ndmpRestoreFail = 1;
							break;
						
						case NDMP_MOVER_PAUSE_NA:				// mover 가 pause 되지 않았다. 이런일은 없을 것이다.
						default:								//알 수 없는 이유다.
							Error(FUNCTION_NdmpRestore, ERROR_NDMP_MOVER_PAUSE_UNKNOWN);
							ndmpRestoreFail = 1;
							break;
					}
				}
			}
			
			// 리스토어 진행 상황을 업데이트한다.
			if (va_ndmp_tape_get_current_block_number(connection, &currentBlockNumber) != 0 || currentBlockNumber < 0)
			{
				currentBlockNumber = 0;
			}
			
			commandData.capacity = (__uint64)(totalBlockNumber + currentBlockNumber) * volumeBlockSize;
			
			readEndTime = time(NULL);
			commandData.writeTime = readEndTime - readStartTime;
		}
		
		// 리스토어 진행 상황을 업데이트한다.
		if (va_ndmp_tape_get_current_block_number(connection, &currentBlockNumber) != 0 || currentBlockNumber < 0)
		{
			currentBlockNumber = 0;
		}
		
		totalBlockNumber += currentBlockNumber;
		commandData.capacity = (__uint64)totalBlockNumber * volumeBlockSize;
		
		readEndTime = time(NULL);
		commandData.writeTime = readEndTime - readStartTime;
		
		if (ndmpRestoreSuccess == 1)
		{
			commandData.fileNumber += backupsetFileList[i].fileNumber;
			commandData.filesSize = commandData.capacity;
		}
		
		
		//////////////////////////////////////////////////////////////////////////////////////////////////
		// 5. NDMP Server와의 접속을 종료한다.
		
		// backupset에서 복구해야될 파일 목록을 제거한다.
		va_ndmp_data_clear_nlist(connection);
		
		// NDMP Server와의 접속을 종료한다.
		va_ndmp_mover_abort(connection);
		va_ndmp_data_abort(connection);
		va_ndmp_mover_stop(connection);
		va_ndmp_data_stop(connection);
		
		
		//////////////////////////////////////////////////////////////////////////////////////////////////
		// 6. NDMP Server와의 접속을 종료한다.
		
		// 리스토어 작업이 실패하지 않았고, 아직 리스토어할 backupset이 남아있으면, 다음 리스토어할 backupset의 volume을 mount한다.
		if (i + 1 < backupsetFileListNumber && ndmpRestoreFail == 0 && jobComplete == 0)
		{
			if (MountNdmpNextVolume(connection, backupsetFileList[i + 1].mediaName[0]) < 0)
			{
				break;
			}
		}

		// 작업이 완료/취소 되었거나 종료되었으면 loop 에서 빠져나온다.
		if (jobComplete == 1 || ndmpRestoreFail == 1)
		{
			break;
		}
	}
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 7. unload the tape media and close the tape drive
	if (driveStatus == DRIVE_OPENING)
	{
		UnloadNdmpTapeMedia(connection);
	}
	
	if(0 < DebugLevel)
	{
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_RESTORE_LOG, "NdmpRestore() End");
	}
	#ifdef __ABIO_UNIX
		return NULL;
	#else
		_endthreadex(0);
		return 0;
	#endif
}

int AddRestoreList(void * connection, int idx)
{
	int i;
	char * destination;
	char folderpath[MAX_PATH_LENGTH];
	
	
	
	destination = NULL;
	
	if(0 < DebugLevel)
	{
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_RESTORE_LOG, "AddRestoreList() Start");
	}

	

	if (strncmp(commandData.backupFile, backupsetFileList[idx].file[0].targetFile, strlen(backupsetFileList[idx].file[0].targetFile) - 1) != 0)
	{
		destination = (char *)malloc(sizeof(char) * (strlen(backupsetFileList[idx].file[0].targetFile) + 1));
		memset(destination, 0, strlen(backupsetFileList[idx].file[0].targetFile) + 1);
		strncpy(destination, backupsetFileList[idx].file[0].targetFile, strlen(backupsetFileList[idx].file[0].targetFile) - 1);
	}
	
	for (i = 0; i < backupsetFileList[idx].fileNumber; i++)
	{
		if (backupsetFileList[idx].file[i].sourceFile[strlen(backupsetFileList[idx].file[i].sourceFile) - 1] == '/')
		{
			// 최신 파일 리스토어와 DAR 리스토어를 할 때는 파일 단위로 복구한다.
			if (commandData.jobMethod == ABIO_NDMP_RESTORE_METHOD_NDMP_BACKUPSET && commandData.ndmpDAR == 0)
			{
				strcpy(folderpath, strchr(backupsetFileList[idx].file[i].sourceFile, ':') + 1);
				
				if (strlen(folderpath) != 1)
				{
					folderpath[strlen(folderpath) - 1] = '\0';
				}
				
				if (va_ndmp_data_add_nlist(connection, folderpath, destination, backupsetFileList[idx].file[i].fh_info, backupsetFileList[idx].file[i].inode, NULL, NULL) != 0)
				{
					va_free(destination);
					return -1;
				}
			}
		}
		else
		{
			if (va_ndmp_data_add_nlist(connection, strchr(backupsetFileList[idx].file[i].sourceFile, ':') + 1, destination, backupsetFileList[idx].file[i].fh_info, backupsetFileList[idx].file[i].inode, NULL, NULL) != 0)
			{
				va_free(destination);
				return -1;
			}
		}
	}
	if(0 < DebugLevel)
	{
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_RESTORE_LOG, "AddRestoreList() End");
	}
	va_free(destination);
	
	return 0;
}

int va_compare_restore_file_list(const void * a1, const void * a2)
{
	struct backupsetRestoreFile * c1;
	struct backupsetRestoreFile * c2;
	
	
	
	c1 = (struct backupsetRestoreFile *)a1;
	c2 = (struct backupsetRestoreFile *)a2;
	
	if (c1->fh_info > c2->fh_info)
	{
		return 1;
	}
	else if (c1->fh_info < c2->fh_info)
	{
		return -1;
	}
	else
	{
		return 0;
	}
}

int LoadNdmpTapeMedia(void * connection)
{
	int loadResult;
	
	time_t loadStartTime;
	time_t loadEndTime;
	
	
	
	UpdateJobState(STATE_NDMP_POSITIONING);
	
	
	// initialize variables
	loadResult = 0;
	
	loadStartTime = time(NULL);
	loadEndTime = time(NULL);
	
	
	// set a mounted media
	memset(mountedVolume, 0, sizeof(mountedVolume));
	strcpy(mountedVolume, commandData.mountStatus.volume);
	
	// open the tape drive
	if (va_ndmp_library_open_tape_drive(connection, commandData.mountStatus.device, O_RDWR, 1) == 0)
	{
		driveStatus = DRIVE_OPENING;
		
		// load the tape media
		if (va_ndmp_library_load_medium(connection) == 0)
		{
			// mount된 media가 필요한 media가 맞는지 검사한다.
			if (CheckNdmpLabeling(connection) < 0)
			{
				loadResult = -1;
			}
		}
		else
		{
			Error(FUNCTION_LoadNdmpTapeMedia, ERROR_VOLUME_LOAD_VOLUME);
			loadResult = -1;
		}
	}
	else
	{
		Error(FUNCTION_LoadNdmpTapeMedia, ERROR_STORAGE_OPEN_TAPE_DRIVE);
		loadResult = -1;
	}
	
	
	loadEndTime = time(NULL);
	commandData.loadTime += loadEndTime - loadStartTime;
	
	
	return loadResult;
}

void UnloadNdmpTapeMedia(void * connection)
{
	time_t unloadStartTime;
	time_t unloadEndTime;
	
	
	
	UpdateJobState(STATE_NDMP_UNLOADING);
	
	
	// initialize variables
	unloadStartTime = time(NULL);
	unloadEndTime = time(NULL);
	
	
	va_ndmp_library_unload_medium(connection);
	
	va_ndmp_tape_close(connection);
	
	
	unloadEndTime = time(NULL);
	commandData.unloadTime += unloadEndTime - unloadStartTime;
	
	
	driveStatus = DRIVE_CLOSED;
}

int CheckNdmpLabeling(void * connection)
{
	struct volumeDB volumeDB;
	
	int loadResult;
	
	
	
	// initialize variables
	loadResult = 0;
	
	
	// media에서 media header를 읽어온다.
	if (va_ndmp_library_read(connection, tapeHeader, volumeBlockSize, DATA_TYPE_NOT_CHANGE) == volumeBlockSize)
	{
		// media header에 있는 label 정보를 읽는다.
		memcpy(&volumeDB, tapeHeader, sizeof(struct volumeDB));
		va_change_byte_order_volumeDB(&volumeDB);		// network to host
		
		// media header에 label이 기록되어있는지 확인한다.
		if (va_is_abio_volume(&volumeDB) == 1)
		{
			// mount된 media가 필요한 media가 맞는지 검사한다.
			if (strcmp(volumeDB.volume, mountedVolume) != 0)
			{
				Error(FUNCTION_CheckNdmpLabeling, ERROR_VOLUME_LABEL_DISCORDANCE);
				loadResult = -1;
			}
		}
		else
		{
			// label이 기록되어있지 않은 경우 에러로 처리한다.
			Error(FUNCTION_CheckNdmpLabeling, ERROR_VOLUME_NO_LABEL);
			loadResult = -1;
		}
	}
	else
	{
		// media header가 기록되어있지 않은 경우 에러로 처리한다.
		// disk media가 labeling되어있지 않은 경우 발생한다.
		Error(FUNCTION_CheckNdmpLabeling, ERROR_VOLUME_READ_DATA);
		loadResult = -1;
	}
	
	
	return loadResult;
}

int MountNdmpNextVolume(void * connection, char * nextMedia)
{
	struct ck recvCommand;
	struct ckBackup recvCommandData;
	
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 1. unload media
	
	// 새로 mount해야되는 volume이 현재 mount되어 있는 volume과 같으면, volume을 rewind만 한다.
	if (!strcmp(mountedVolume, nextMedia))
	{
		if (va_ndmp_library_rewind_tape(connection) == 0)
		{
			return 0;
		}
		else
		{
			Error(FUNCTION_MountNdmpNextVolume, ERROR_VOLUME_LOAD_VOLUME);
			
			return -1;
		}
	}
	// 새로 mount해야되는 volume이 현재 mount되어 있는 volume이 아니면, 현재 mount되어 있는 volume을 unmount하기 위해 volume을 unload한다.
	else
	{
		UnloadNdmpTapeMedia(connection);
	}
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 2. send a mount next media request to master server
	// mount할 next media를 지정한다.
	memset(commandData.volumeDB.nextVolume, 0, sizeof(commandData.volumeDB.nextVolume));
	strcpy(commandData.volumeDB.nextVolume, nextMedia);
	
	UpdateJobState(STATE_NDMP_SEND_REQUEST_MOUNT_NEXT_VOLUME);
	SendCommand(0, 1, 0, MODULE_NAME_NDMP_RESTORE, "MOUNT_NEXT_VOLUME", MODULE_NAME_MASTER_RESTORE, serverIP, serverPort, serverNodename, masterIP, masterPort, masterNodename);
	
	if (va_msgrcv(midMedia, mtypeMedia, &recvCommand, sizeof(struct ck), 0) > 0)
	{
		UpdateJobState(STATE_NDMP_RECV_RESULT_MOUNT_NEXT_VOLUME);
		memcpy(&recvCommandData, recvCommand.dataBlock, sizeof(struct ckBackup));
		
		if (!GET_ERROR(recvCommandData.errorCode))
		{
			// mount된 media의 정보를 저장한다.
			memset(commandData.mountStatus.volume, 0, sizeof(commandData.mountStatus.volume));
			strcpy(commandData.mountStatus.volume, recvCommandData.mountStatus.volume);
		}
		else
		{
			errorLocation = ABIO_MASTER_SERVER;
			memcpy(commandData.errorCode, recvCommandData.errorCode, sizeof(struct errorCode) * ERROR_CODE_NUMBER);
			
			Error(0, 0);
			return -1;
		}
	}
	else
	{
		Error(FUNCTION_MountNdmpNextVolume, ERROR_RESOURCE_MESSAGE_TRANSFER_REMOVED);

		return -1;
	}
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 3. load and position the media
	if (LoadNdmpTapeMedia(connection) < 0)
	{
		return -1;
	}
	
	
	return 0;
}

va_thread_return_t WriteRunningLog(void * arg)
{
	int logSendInterval;
	int count;
	
	
	
	va_sleep(1);
	
	logSendInterval = 5;
	
	// send a writing log to master server periodically
	while (1)
	{
		// 1초에 한번씩 작업이 끝났는지 확인하면서 다음 로그 전송 시간까지 대기한다.
		count = 0;
		while (jobComplete == 0 && count < logSendInterval)
		{
			va_sleep(1);
			count++;
		}
		
		if (jobComplete == 1)
		{
			break;
		}
		
		UpdateJobStatus();
	}
	
	
	#ifdef __ABIO_UNIX
		return NULL;
	#else
		_endthreadex(0);
		return 0;
	#endif
}

void UpdateJobStatus()
{
	struct ckBackup sendCommandData;
	char cmd[DSIZ * DSIZ];
	
	va_sock_t masterSock;
	
	
	
	// initialize variables
	memset(cmd, 0, sizeof(cmd));
	
	
	// 작업의 상태를 저장한다.
	memcpy(&sendCommandData, &commandData, sizeof(struct ckBackup));
	
	// master server로 보낼 로그 메세지를 만든다.
	strcpy(cmd, "<UPDATE_JOB_STATUS>");
	
	va_change_byte_order_ckBackup(&sendCommandData);		// host to network
	memcpy(cmd + strlen("<UPDATE_JOB_STATUS>") + 1, &sendCommandData, sizeof(struct ckBackup));
	va_change_byte_order_ckBackup(&sendCommandData);		// network to host
	
	// master server로 로그 메세지를 보낸다.
	if ((masterSock = va_connect(masterIP, masterLogPort, 1)) != -1)
	{
		va_send(masterSock, cmd, (int)strlen("<UPDATE_JOB_STATUS>") + 1 + sizeof(struct ckBackup), 0, DATA_TYPE_NOT_CHANGE);
		va_close_socket(masterSock, ABIO_SOCKET_CLOSE_CLIENT);
	}
}

void UpdateJobState(enum jobState state)
{
	struct ckBackup sendCommandData;
	char cmd[DSIZ * DSIZ];
	
	va_sock_t masterSock;
	int moduleNumber;
	
	
	
	// initialize variables
	memset(cmd, 0, sizeof(cmd));
	
	
	// 작업의 상태를 저장한다.
	commandData.state = state;
	memcpy(&sendCommandData, &commandData, sizeof(struct ckBackup));
	moduleNumber = MODULE_NDMP_RESTORE;
	
	// master server로 보낼 로그 메세지를 만든다.
	strcpy(cmd, "<UPDATE_JOB_STATE>");
	
	moduleNumber = htonl(moduleNumber);		// host to network
	memcpy(cmd + strlen("<UPDATE_JOB_STATE>") + 1, &moduleNumber, sizeof(int));
	moduleNumber = htonl(moduleNumber);		// network to host

	va_change_byte_order_ckBackup(&sendCommandData);		// host to network
	memcpy(cmd + strlen("<UPDATE_JOB_STATE>") + 1 + sizeof(int) + 1, &sendCommandData, sizeof(struct ckBackup));
	va_change_byte_order_ckBackup(&sendCommandData);		// network to host
	
	// master server로 로그 메세지를 보낸다.
	if ((masterSock = va_connect(masterIP, masterLogPort, 1)) != -1)
	{
		va_send(masterSock, cmd, (int)strlen("<UPDATE_JOB_STATE>") + 1 + sizeof(int) + 1 + sizeof(struct ckBackup), 0, DATA_TYPE_NOT_CHANGE);
		va_close_socket(masterSock, ABIO_SOCKET_CLOSE_CLIENT);
	}
}

void CancelJob(struct ckBackup * recvCommandData)
{
	va_sock_t serverSock;
	
	
	
	// cancel job 요청에 대한 응답을 보낸다.
	recvCommandData->success = 1;
	
	if ((serverSock = va_connect(masterIP, recvCommandData->catalogPort, 1)) != -1)
	{
		va_send(serverSock, recvCommandData, sizeof(struct ckBackup), 0, DATA_TYPE_CK_BACKUP);
		va_close_socket(serverSock, ABIO_SOCKET_CLOSE_CLIENT);
	}
	
	
	// 리스토어 작업을 끝낸다.
	if (jobCanceled == 0)
	{
		jobCanceled = 1;
		
		if (jobComplete == 0)
		{
			Error(FUNCTION_CancelJob, ERROR_JOB_CANCELED);
		}
	}
}

void Error(int functionNumber, int errorNumber)
{
	if (errorOccured == 0)
	{
		errorOccured = 1;
		
		// 에러를 기록한다.
		if (functionNumber != 0 && errorNumber != 0)
		{
			va_write_error_code(ERROR_LEVEL_ERROR, MODULE_NDMP_RESTORE, functionNumber, errorNumber, &commandData);
		}
		
		// 작업이 끝났음을 표시한다.
		jobComplete = 1;
	}
}

void Exit()
{
	struct ck closeCommand;
	
	int i;
	int j;
	
	
	
	// 작업이 끝났음을 표시한다.
	jobComplete = 1;
	
	// thread가 끝나기를 기다린다.
	if (tidLog != 0)
	{
		va_wait_thread(tidLog);
	}
	
	// master server로 작업이 끝났음을 알린다.
	UpdateJobState(STATE_NDMP_SEND_RESULT_DATA_TRANSFER);
	SendCommand(0, 1, 0, MODULE_NAME_NDMP_RESTORE, "", MODULE_NAME_MASTER_RESTORE, serverIP, serverPort, serverNodename, masterIP, masterPort, masterNodename);
	
	// InnerMessageTransfer thread 종료
	if (tidMessageTransfer != 0)
	{
		memset(&closeCommand, 0, sizeof(struct ck));
		strcpy(closeCommand.subCommand, "EXIT_INNER_MESSAGE_TRANSFER");
		va_msgsnd(midClient, mtypeClient, &closeCommand, sizeof(struct ck), 0);
		
		va_wait_thread(tidMessageTransfer);
	}
	
	// restore volume block list 삭제
	va_remove(ABIOMASTER_NDMP_FILE_LIST_FOLDER, serverFilelistFile);
	
	// system 자원 반납
	#ifdef __ABIO_UNIX
		va_remove_message_transfer(midMedia);
	#elif __ABIO_WIN
		va_remove_message_transfer(midClient);
		va_remove_message_transfer(midMedia);
	#endif
	
	// memory 해제
	va_free(portRule);
	va_free(tapeHeader);
	
	for (i = 0; i < backupsetFileListNumber; i++)
	{
		for (j = 0; j < backupsetFileList[i].mediaNumber; j++)
		{
			va_free(backupsetFileList[i].mediaName[j]);
		}
		va_free(backupsetFileList[i].mediaName);
		va_free(backupsetFileList[i].backupsetPosition);
		
		for (j = 0; j < backupsetFileList[i].fileNumber; j++)
		{
			va_free(backupsetFileList[i].file[j].sourceFile);
			va_free(backupsetFileList[i].file[j].targetFile);
		}
		va_free(backupsetFileList[i].file);
	}
	va_free(backupsetFileList);
	
	#ifdef __ABIO_WIN
		//윈속 제거
		WSACleanup();
	#endif
}

void SendJobLog(char * logMsgID, ...)
{
	va_list argptr;
	char * argument;
	
	char description[USER_LOG_MESSAGE_LENGTH];
	char logmsg[USER_LOG_MESSAGE_LENGTH];
	
	int len;
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
	
	len = (int)strlen(logmsg);
	while (len > 0)
	{
		if (logmsg[len - 1] == '\n' || logmsg[len - 1] == '\r')
		{
			logmsg[len - 1] = '\0';
			len--;
		}
		else
		{
			break;
		}
	}
	
	va_send_abio_log(masterIP, masterLogPort, commandData.jobID, NULL, LOG_LEVEL_JOB, MODULE_NDMP_RESTORE, logmsg);
}


