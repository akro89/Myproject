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

char mountedVolume[VOLUME_NAME_LENGTH];	// ���� mount�Ǿ��ִ� media
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

struct portRule * portRule;					// VIRBAK ABIO IP Table ���
int portRuleNumber;							// VIRBAK ABIO IP Table ����

int restoreNumber;							//restore �ݺ�
//////////////////////////////////////////////////////////////////////////////////////////////////
// error control
// ������� ���߿� ������ �߻����� �� ������ �߻��� ��Ҹ� ����Ѵ�. 0�� backup server error, 1�� master server error, 2�� client error�� �ǹ��Ѵ�.
// backup server�� master server���� ������ �߻��� ���, ������ �߻��ߴٰ� client���� �˷��ش�.
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
		
		
		// ������ �����Ѵ�.
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
	// 1. ���� �������� �ʱ�ȭ�Ѵ�.
	
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
	// 2. �� �ý��ۿ� �°� process�� �ʱ�ȭ�Ѵ�.
	
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
		// windows socket�� open�Ѵ�.
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) 
		{
			Error(FUNCTION_InitProcess, ERROR_WIN_INIT_WIN_SOCK);
			
			return -1;
		}
	#endif
	
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 3. process ���࿡ �ʿ��� ���� ������ �о�´�.
	
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
	
	
	
	// abio log description file�� �ε��Ѵ�.
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
	// 4. restore�� �����⸦ ��ٸ���
	
	// restore�� �����⸦ ��ٸ���.
	va_wait_thread(tidNdmpRestore);
	
	// ndmp server���� ������ �����Ѵ�.
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
	
	
	
	// client�� ����ִ��� Ȯ���Ѵ�.
	if ((clientSock = va_connect(ndmpIP, ndmpPort, 1)) != -1)
	{
		va_close_socket(clientSock, ABIO_SOCKET_CLOSE_CLIENT);
	}
	else
	{
		Error(FUNCTION_MakeNewConnection, ERROR_NETWORK_CLIENT_DOWN);
		
		return NULL;
	}
	
	// ndmp server�� �����Ѵ�.
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
	
	// job log�� master server�� ���� thread�� �����.
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
	
	// ndmp server���� connection�� �����Ѵ�.
	connection = arg;
	
	totalBlockNumber = 0;
	currentBlockNumber = 0;
	
	
	// volume�� load�Ѵ�.
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
		
		// ��������� nas volume�� ����������� ���Ϸ� ǥ���Ѵ�.
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
			// backupset position���� �̵��Ѵ�.
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
		
		// ������� ������ �����ϰ�, ������� ����� ����Ѵ�.
		if (jobComplete == 0)
		{
			// dump level ���� value "0" �� full/incremental�� ���� �ٲ�����.
			if (va_ndmp_data_add_env(connection, "LEVEL", "0") != 0)
			{
				Error(FUNCTION_NdmpRestore, ERROR_NDMP_ENV_ADD_LEVEL);
			}
			// ��������� ������ �����ִ� volume�� ����Ѵ�.
			else if (va_ndmp_data_add_env(connection, "FILESYSTEM", commandData.backupFile) != 0)
			{
				Error(FUNCTION_NdmpRestore, ERROR_NDMP_ENV_ADD_FILESYSTEM);
			}
			// ��������ؾߵ� ����� ����Ѵ�.
			else if (AddRestoreList(connection, i) != 0)
			{
				Error(FUNCTION_NdmpRestore, ERROR_NDMP_ADD_RESTORE_LIST);
			}
			// �۾��� ����� ũ�⸦ �����ش�.
			else if (commandData.writeDetailedLog == 1 && va_ndmp_data_add_env(connection, "VERBOSE", "Y") != 0)
			{
				Error(FUNCTION_NdmpRestore, ERROR_NDMP_ENV_ADD_VERBOSE);
			}
		}
		
		// DAR �۾��� ���� �ɼ��� �����Ѵ�.
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
		
		// ������� �۾��� �����Ѵ�.
		if (jobComplete == 0)
		{
			// mover service (tape���� �����͸� �о data service�� �����ִ� service) listen for data service
			if (va_ndmp_mover_listen(connection, "write", "tcp") != 0)
			{
				Error(FUNCTION_NdmpRestore, ERROR_NDMP_MOVER_LISTEN);
			}
			// DAR �ɼ��� ����� ��� mover���� �о���� �����Ͱ� ���۵� ��ġ�� �˷��ش�.
			else if (commandData.ndmpDAR == 1 && va_ndmp_mover_set_window(connection, 0, (__uint64)-1) != 0)
			{
				Error(FUNCTION_NdmpRestore, ERROR_NDMP_MOVER_SET_WINDOW);
			}
			// data service (mover service���� �����͸� �޾Ƽ� disk�� ���� service) connect to mover service
			else if (va_ndmp_data_connect(connection, "tcp", NULL, NULL) != 0)
			{
				Error(FUNCTION_NdmpRestore, ERROR_NDMP_DATA_CONNECT);
			}
			// data service���� ������ �����϶�� �˷��ش�.
			else if (va_ndmp_data_start_recover(connection, "dump") != 0)
			{
				Error(FUNCTION_NdmpRestore, ERROR_NDMP_DATA_START_RECOVER);
			}
			// mover service���� tape���� �����͸� �б� �����϶�� �˷��ش�.
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
			// ndmp server�κ��� mover halt state�� ���� ����, ���̾� data halt state�� ���� ��� ���� �� mover halt state�� ó���ϰ� ndmp state�� �˻��ϴ� ��쿡, ������ �߻��� ������ ó���ȴ�.
			// ��ó�� ndmp server�κ��� �����ؼ� �޼����� ���� ��쿡 �޼��� ������ Ÿ�̹��� ������ ���� ������ �߻��� ������ ó���� ���ɼ��� �ִ�.
			// �׷��� ndmp polling interval ���� ndmp server�κ��� ���� �޼����� ��� ó���� �ڿ� ndmp state�� �˻��ϵ��� �Ѵ�.
			while ((r = va_ndmp_util_poll_ndmp_request(connection, NDMP_POLLING_INTERVAL)) == 1 && jobComplete == 0)
			{
				if (va_ndmp_comm_process_request(connection) < 0)
				{
					Error(FUNCTION_NdmpRestore, ERROR_NDMP_REQUEST_PROCESS);
					ndmpRestoreFail = 1;
					break;
				}
				
				// ������� ���� ��Ȳ�� ������Ʈ�Ѵ�.
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
			
			// ������� ���� ��Ȳ�� ������Ʈ�Ѵ�.
			if (va_ndmp_tape_get_current_block_number(connection, &currentBlockNumber) != 0 || currentBlockNumber < 0)
			{
				currentBlockNumber = 0;
			}
			
			commandData.capacity = (__uint64)(totalBlockNumber + currentBlockNumber) * volumeBlockSize;
			
			readEndTime = time(NULL);
			commandData.writeTime = readEndTime - readStartTime;
			
			// ndmp state�� �˻��Ѵ�.
			if (ndmpRestoreSuccess == 0 && ndmpRestoreFail == 0 && jobComplete == 0)
			{
				// data write �� ����Ǿ���. ������?
				if (nDataHaltState == 1)
				{
					switch (nDataHaltReason)
					{
						case NDMP_DATA_HALT_SUCCESSFUL:			// �����ߴ�.
							// ������� �������� ���� ó��.
							ndmpRestoreSuccess = 1;
							break;
						
						case NDMP_DATA_HALT_ABORTED:			// ��ҵǾ���.
							// ������ ��ҵǾ���.
							Error(FUNCTION_NdmpRestore, ERROR_NDMP_DATA_HALT_ABORTED);
							ndmpRestoreFail = 1;
							break;
						
						case NDMP_DATA_HALT_INTERNAL_ERROR:		// NDMP���� ������ ������ ����Ǿ���.
							Error(FUNCTION_NdmpRestore, ERROR_NDMP_DATA_HALT_INTERNAL_ERROR);
							ndmpRestoreFail = 1;
							break;
						
						case NDMP_DATA_HALT_CONNECT_ERROR:		// data connect�� ������ ����Ǿ���.
							Error(FUNCTION_NdmpRestore, ERROR_NDMP_DATA_HALT_CONNECT_ERROR);
							ndmpRestoreFail = 1;
							break;
						
						case NDMP_DATA_HALT_NA:					// data halt �� �ƴϴ�! �̷����� ���� ���̴�. �ִٸ� ������ ó���ؾ���.
						default:								//�� �� ���� ������.
							Error(FUNCTION_NdmpRestore, ERROR_NDMP_DATA_HALT_UNKNOWN);
							ndmpRestoreFail = 1;
							break;
					}
				}
				
				// tape read�� ����Ǿ���. ������?
				if (nMoverHaltState == 1)
				{
					switch (nMoverHaltReason)
					{
						case NDMP_MOVER_HALT_CONNECT_CLOSED:	// connect�� ������ ����Ǿ���.
							// connect �� ����Ǿ���. restore �� ������ ���� �ƴ϶�� ��ҵ� ���̴�.
							if (nDataHaltState == 0)
							{
								Error(FUNCTION_NdmpRestore, ERROR_NDMP_MOVER_HALT_CONNECT_CLOSED);
								ndmpRestoreFail = 1;
							}
							break;
						
						case NDMP_MOVER_HALT_ABORTED:			// ��ҵǾ���.
							// ���ó��.
							Error(FUNCTION_NdmpRestore, ERROR_NDMP_MOVER_HALT_ABORTED);
							ndmpRestoreFail = 1;
							break;
						
						case NDMP_MOVER_HALT_INTERNAL_ERROR:	// NDMP���� ������ ������ ����Ǿ���.
							Error(FUNCTION_NdmpRestore, ERROR_NDMP_MOVER_HALT_INTERNAL_ERROR);
							ndmpRestoreFail = 1;
							break;
						
						case NDMP_MOVER_HALT_CONNECT_ERROR:		// ���� ������ ����Ǿ���.
							Error(FUNCTION_NdmpRestore, ERROR_NDMP_MOVER_HALT_CONNECT_ERROR);
							ndmpRestoreFail = 1;
							break;
						
						case NDMP_MOVER_HALT_NA:				// mover�� halt���� �ʾҴ�! �̷����� ���� ���̴�. �ִٸ� ������ ó���ؾ���.
						default:								//�� �� ���� ������.
							Error(FUNCTION_NdmpRestore, ERROR_NDMP_MOVER_HALT_UNKNOWN);
							ndmpRestoreFail = 1;
							break;
					}
				}
				
				// tape read�� �����. ������?
				if (nMoverPauseState == 1)
				{
					//nMoverPauseReason
					switch (nMoverPauseReason)
					{
						case NDMP_MOVER_PAUSE_SEEK:				// ã�� ���� data window�� �ٱ����̾ pause �Ǿ���.
						case NDMP_MOVER_PAUSE_EOM:				// mover �� tape�� ���� �����ؼ� pause �Ǿ���.
							nMoverPauseState = 0;
							nMoverPauseReason = 0;
							
							// ������� ���� ��Ȳ�� ������Ʈ�Ѵ�.
							if (va_ndmp_tape_get_current_block_number(connection, &currentBlockNumber) != 0 || currentBlockNumber < 0)
							{
								currentBlockNumber = 0;
							}
							
							totalBlockNumber += currentBlockNumber;
							commandData.capacity = (__uint64)totalBlockNumber * volumeBlockSize;
							
							// ���� media�� mount�Ѵ�.
							j++;
							
							// media�� ���� �����ߴµ� ���� media�� ���ٴ� ���� ���� �߸��Ǿ��ٴ� ���̴�.
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
							
							// DAR �ɼ��� ����� ��� mover���� �о���� �����Ͱ� ���۵� ��ġ�� �˷��ش�.
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
						
						case NDMP_MOVER_PAUSE_EOF:				// mover �� tape���� file mark�� �����ؼ� pause �Ǿ���. �� ��� file restore�� �����ߴٰ� �����Ѵ�.
							nMoverPauseState = 0;
							nMoverPauseReason = 0;
							ndmpRestoreSuccess = 1;
							break;
						
						case NDMP_MOVER_PAUSE_MEDIA_ERROR:		// tape�� read/write ������ �߻��ؼ� pause �Ǿ���.
							Error(FUNCTION_NdmpRestore, ERROR_NDMP_MOVER_PAUSE_MEDIA_ERROR);
							ndmpRestoreFail = 1;
							break;
						
						case NDMP_MOVER_PAUSE_EOW:				// �۾��� mover window�� ���� �����Ͽ��� pause �Ǿ���.
							Error(FUNCTION_NdmpRestore, ERROR_NDMP_MOVER_PAUSE_EOW);
							ndmpRestoreFail = 1;
							break;
						
						case NDMP_MOVER_PAUSE_NA:				// mover �� pause ���� �ʾҴ�. �̷����� ���� ���̴�.
						default:								//�� �� ���� ������.
							Error(FUNCTION_NdmpRestore, ERROR_NDMP_MOVER_PAUSE_UNKNOWN);
							ndmpRestoreFail = 1;
							break;
					}
				}
			}
			
			// ������� ���� ��Ȳ�� ������Ʈ�Ѵ�.
			if (va_ndmp_tape_get_current_block_number(connection, &currentBlockNumber) != 0 || currentBlockNumber < 0)
			{
				currentBlockNumber = 0;
			}
			
			commandData.capacity = (__uint64)(totalBlockNumber + currentBlockNumber) * volumeBlockSize;
			
			readEndTime = time(NULL);
			commandData.writeTime = readEndTime - readStartTime;
		}
		
		// ������� ���� ��Ȳ�� ������Ʈ�Ѵ�.
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
		// 5. NDMP Server���� ������ �����Ѵ�.
		
		// backupset���� �����ؾߵ� ���� ����� �����Ѵ�.
		va_ndmp_data_clear_nlist(connection);
		
		// NDMP Server���� ������ �����Ѵ�.
		va_ndmp_mover_abort(connection);
		va_ndmp_data_abort(connection);
		va_ndmp_mover_stop(connection);
		va_ndmp_data_stop(connection);
		
		
		//////////////////////////////////////////////////////////////////////////////////////////////////
		// 6. NDMP Server���� ������ �����Ѵ�.
		
		// ������� �۾��� �������� �ʾҰ�, ���� ��������� backupset�� ����������, ���� ��������� backupset�� volume�� mount�Ѵ�.
		if (i + 1 < backupsetFileListNumber && ndmpRestoreFail == 0 && jobComplete == 0)
		{
			if (MountNdmpNextVolume(connection, backupsetFileList[i + 1].mediaName[0]) < 0)
			{
				break;
			}
		}

		// �۾��� �Ϸ�/��� �Ǿ��ų� ����Ǿ����� loop ���� �������´�.
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
			// �ֽ� ���� �������� DAR ������ �� ���� ���� ������ �����Ѵ�.
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
			// mount�� media�� �ʿ��� media�� �´��� �˻��Ѵ�.
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
	
	
	// media���� media header�� �о�´�.
	if (va_ndmp_library_read(connection, tapeHeader, volumeBlockSize, DATA_TYPE_NOT_CHANGE) == volumeBlockSize)
	{
		// media header�� �ִ� label ������ �д´�.
		memcpy(&volumeDB, tapeHeader, sizeof(struct volumeDB));
		va_change_byte_order_volumeDB(&volumeDB);		// network to host
		
		// media header�� label�� ��ϵǾ��ִ��� Ȯ���Ѵ�.
		if (va_is_abio_volume(&volumeDB) == 1)
		{
			// mount�� media�� �ʿ��� media�� �´��� �˻��Ѵ�.
			if (strcmp(volumeDB.volume, mountedVolume) != 0)
			{
				Error(FUNCTION_CheckNdmpLabeling, ERROR_VOLUME_LABEL_DISCORDANCE);
				loadResult = -1;
			}
		}
		else
		{
			// label�� ��ϵǾ����� ���� ��� ������ ó���Ѵ�.
			Error(FUNCTION_CheckNdmpLabeling, ERROR_VOLUME_NO_LABEL);
			loadResult = -1;
		}
	}
	else
	{
		// media header�� ��ϵǾ����� ���� ��� ������ ó���Ѵ�.
		// disk media�� labeling�Ǿ����� ���� ��� �߻��Ѵ�.
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
	
	// ���� mount�ؾߵǴ� volume�� ���� mount�Ǿ� �ִ� volume�� ������, volume�� rewind�� �Ѵ�.
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
	// ���� mount�ؾߵǴ� volume�� ���� mount�Ǿ� �ִ� volume�� �ƴϸ�, ���� mount�Ǿ� �ִ� volume�� unmount�ϱ� ���� volume�� unload�Ѵ�.
	else
	{
		UnloadNdmpTapeMedia(connection);
	}
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 2. send a mount next media request to master server
	// mount�� next media�� �����Ѵ�.
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
			// mount�� media�� ������ �����Ѵ�.
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
		// 1�ʿ� �ѹ��� �۾��� �������� Ȯ���ϸ鼭 ���� �α� ���� �ð����� ����Ѵ�.
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
	
	
	// �۾��� ���¸� �����Ѵ�.
	memcpy(&sendCommandData, &commandData, sizeof(struct ckBackup));
	
	// master server�� ���� �α� �޼����� �����.
	strcpy(cmd, "<UPDATE_JOB_STATUS>");
	
	va_change_byte_order_ckBackup(&sendCommandData);		// host to network
	memcpy(cmd + strlen("<UPDATE_JOB_STATUS>") + 1, &sendCommandData, sizeof(struct ckBackup));
	va_change_byte_order_ckBackup(&sendCommandData);		// network to host
	
	// master server�� �α� �޼����� ������.
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
	
	
	// �۾��� ���¸� �����Ѵ�.
	commandData.state = state;
	memcpy(&sendCommandData, &commandData, sizeof(struct ckBackup));
	moduleNumber = MODULE_NDMP_RESTORE;
	
	// master server�� ���� �α� �޼����� �����.
	strcpy(cmd, "<UPDATE_JOB_STATE>");
	
	moduleNumber = htonl(moduleNumber);		// host to network
	memcpy(cmd + strlen("<UPDATE_JOB_STATE>") + 1, &moduleNumber, sizeof(int));
	moduleNumber = htonl(moduleNumber);		// network to host

	va_change_byte_order_ckBackup(&sendCommandData);		// host to network
	memcpy(cmd + strlen("<UPDATE_JOB_STATE>") + 1 + sizeof(int) + 1, &sendCommandData, sizeof(struct ckBackup));
	va_change_byte_order_ckBackup(&sendCommandData);		// network to host
	
	// master server�� �α� �޼����� ������.
	if ((masterSock = va_connect(masterIP, masterLogPort, 1)) != -1)
	{
		va_send(masterSock, cmd, (int)strlen("<UPDATE_JOB_STATE>") + 1 + sizeof(int) + 1 + sizeof(struct ckBackup), 0, DATA_TYPE_NOT_CHANGE);
		va_close_socket(masterSock, ABIO_SOCKET_CLOSE_CLIENT);
	}
}

void CancelJob(struct ckBackup * recvCommandData)
{
	va_sock_t serverSock;
	
	
	
	// cancel job ��û�� ���� ������ ������.
	recvCommandData->success = 1;
	
	if ((serverSock = va_connect(masterIP, recvCommandData->catalogPort, 1)) != -1)
	{
		va_send(serverSock, recvCommandData, sizeof(struct ckBackup), 0, DATA_TYPE_CK_BACKUP);
		va_close_socket(serverSock, ABIO_SOCKET_CLOSE_CLIENT);
	}
	
	
	// ������� �۾��� ������.
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
		
		// ������ ����Ѵ�.
		if (functionNumber != 0 && errorNumber != 0)
		{
			va_write_error_code(ERROR_LEVEL_ERROR, MODULE_NDMP_RESTORE, functionNumber, errorNumber, &commandData);
		}
		
		// �۾��� �������� ǥ���Ѵ�.
		jobComplete = 1;
	}
}

void Exit()
{
	struct ck closeCommand;
	
	int i;
	int j;
	
	
	
	// �۾��� �������� ǥ���Ѵ�.
	jobComplete = 1;
	
	// thread�� �����⸦ ��ٸ���.
	if (tidLog != 0)
	{
		va_wait_thread(tidLog);
	}
	
	// master server�� �۾��� �������� �˸���.
	UpdateJobState(STATE_NDMP_SEND_RESULT_DATA_TRANSFER);
	SendCommand(0, 1, 0, MODULE_NAME_NDMP_RESTORE, "", MODULE_NAME_MASTER_RESTORE, serverIP, serverPort, serverNodename, masterIP, masterPort, masterNodename);
	
	// InnerMessageTransfer thread ����
	if (tidMessageTransfer != 0)
	{
		memset(&closeCommand, 0, sizeof(struct ck));
		strcpy(closeCommand.subCommand, "EXIT_INNER_MESSAGE_TRANSFER");
		va_msgsnd(midClient, mtypeClient, &closeCommand, sizeof(struct ck), 0);
		
		va_wait_thread(tidMessageTransfer);
	}
	
	// restore volume block list ����
	va_remove(ABIOMASTER_NDMP_FILE_LIST_FOLDER, serverFilelistFile);
	
	// system �ڿ� �ݳ�
	#ifdef __ABIO_UNIX
		va_remove_message_transfer(midMedia);
	#elif __ABIO_WIN
		va_remove_message_transfer(midClient);
		va_remove_message_transfer(midMedia);
	#endif
	
	// memory ����
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
		//���� ����
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


