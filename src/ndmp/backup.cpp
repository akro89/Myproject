#include "require.h"
#include "abio.h"
#include "lib.h"
#include "vendor.h"
#include "ndmpc.h"
#include "backup.h"


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

extern int DebugLev;
extern char ABIOMASTER_NDMP_DEBUG_FOLDER[MAX_PATH_LENGTH];

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
// #issue148 
int waitTapeMediaTime;

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

int includeNumber;
//////////////////////////////////////////////////////////////////////////////////////////////////
// ndmp variables
ndmp_mover_listen_reply connmoveraddr;

int nMoverPauseState;
int nMoverHaltState;
int nDataHaltState;

int nMoverPauseReason;
int nMoverHaltReason;
int nDataHaltReason;

extern int findRoot;
//////////////////////////////////////////////////////////////////////////////////////////////////
// variables for backup files
char ** includeList;
int includeListNumber;

char ** excludeList;
int excludeListNumber;



//////////////////////////////////////////////////////////////////////////////////////////////////
// variables for the catalog db and volume db
char backupsetVolume[VOLUME_NAME_LENGTH];	// �̹��� ����ϴ� backupset�� ���۵� media

__uint64 totalWriteSize;					// ���� mount�Ǿ��ִ� media�� ������ �� ��� �������� ũ��.

__uint64 backupsetSize;						// ���� mount�Ǿ��ִ� media�� ������ ��� �������� ũ��
int backupsetBlockNumber;					// ���� mount�Ǿ��ִ� media�� ������ block�� ����


//////////////////////////////////////////////////////////////////////////////////////////////////
// variables for backed up files list
extern dirinfoarray * ndmpBackupFileArray;
extern __uint64 ndmpBackupFileArrayNumber;

extern dirinfo * ndmpBackupFileInfo;
extern __uint64 ndmpBackupFileNumber;

extern inodeindexarray * inodeIndexArray;
extern unsigned long inodeIndexArrayNumber;


//////////////////////////////////////////////////////////////////////////////////////////////////
// variable for the tape drive
int volumeBlockSize;	// volume block size

__uint64 backupsetPosition;						// backupset position in a media. it means a filemark number in case of the tape media and a media size in case of the disk media.
__uint64 catalogBackupsetPosition;
char mountedVolume[VOLUME_NAME_LENGTH];			// ���� mount�Ǿ��ִ� media
char previousMountedVolume[VOLUME_NAME_LENGTH];	// ������ mount�Ǿ����� media
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

char jobBackupset[BACKUPSET_ID_LENGTH];		// current backupset
char currentBackupset[BACKUPSET_ID_LENGTH];	// current backupset
int preNDMPBackupFileNumber;

struct portRule * portRule;					// VIRBAK ABIO IP Table ���
int portRuleNumber;							// VIRBAK ABIO IP Table ����

//DebugLevel
int DebugLevel=0;

//////////////////////////////////////////////////////////////////////////////////////////////////
// error control
// ��� ���߿� ������ �߻����� �� ������ �߻��� ��Ҹ� ����Ѵ�. 0�� abio ndmp client (nas ndmp server�� �ƴ�) error, 1�� master server error�� �ǹ��Ѵ�.
enum {ABIO_NDMP_CLIENT, ABIO_MASTER_SERVER} errorLocation;

int jobCanceled;
int errorOccured;
int jobComplete;


//////////////////////////////////////////////////////////////////////////////////////////////////
// job thread
va_thread_t tidNdmpBackup;
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


	// ����� �����Ѵ�.
	if (Backup() < 0)
	{
		Exit();

		return 2;
	}


	Exit();

	return 0;
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


	DebugLev = 0;
	memset(ABIOMASTER_NDMP_DEBUG_FOLDER, 0, sizeof(ABIOMASTER_NDMP_DEBUG_FOLDER));

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// ABIO log option
	requestLogLevel = LOG_LEVEL_JOB;
	memset(logJobID, 0, sizeof(logJobID));
	logModuleNumber = MODULE_UNKNOWN;


	//////////////////////////////////////////////////////////////////////////////////////////////////
	// slave server configuration
	tapeDriveTimeOut = 0;
	tapeDriveDelayTime = 0;
	waitTapeMediaTime = 0;

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// message transfer variables
	midClient = (va_trans_t)-1;
	midMedia = (va_trans_t)-1;

	mtypeClient = 0;
	mtypeMedia = 0;

	includeNumber = 0;
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
	// variables for backup files
	// backup file list
	includeList = NULL;
	includeListNumber = 0;

	excludeList = NULL;
	excludeListNumber = 0;


	//////////////////////////////////////////////////////////////////////////////////////////////////
	// variables for the catalog db and volume db
	memset(backupsetVolume, 0, sizeof(backupsetVolume));

	totalWriteSize = 0;

	backupsetSize = 0;
	backupsetBlockNumber = 0;


	//////////////////////////////////////////////////////////////////////////////////////////////////
	// variables for backed up files list
	ndmpBackupFileArray = NULL;
	ndmpBackupFileArrayNumber = 0;

	ndmpBackupFileInfo = NULL;
	ndmpBackupFileNumber = 0;

	inodeIndexArray = NULL;
	inodeIndexArrayNumber = 0;


	//////////////////////////////////////////////////////////////////////////////////////////////////
	// variable for the tape drive
	volumeBlockSize = 0;

	backupsetPosition = 0;
	catalogBackupsetPosition = 0;
	memset(mountedVolume, 0, sizeof(mountedVolume));
	memset(previousMountedVolume, 0, sizeof(previousMountedVolume));
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

	memset(currentBackupset,0,sizeof(currentBackupset));
	memset(jobBackupset,0,sizeof(jobBackupset));

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
	tidNdmpBackup = 0;
	tidMessageTransfer = 0;
	tidLog = 0;


	//////////////////////////////////////////////////////////////////////////////////////////////////
	// ndmp log
	backupLogDescription = NULL;
	backupLogDescriptionNumber = 0;
	SendNdmpLogToMaster = SendJobLog;



	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 2. �� �ý��ۿ� �°� process�� �ʱ�ȭ�Ѵ�.

	// set a resource limit
	va_setrlimit();

	// set the slave server working directory
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
	logModuleNumber = MODULE_NDMP_BACKUP;

	// set master server ip and port
	strcpy(masterIP, command.sourceIP);
	strcpy(masterPort, command.sourcePort);
	strcpy(masterNodename, command.sourceNodename);
	strcpy(masterLogPort, commandData.client.masterLogPort);

	// set ndmp agent (backup server) ip and port
	strcpy(serverIP, command.destinationIP);
	strcpy(serverPort, command.destinationPort);
	strcpy(serverNodename, command.destinationNodename);

	// set backup server ip and port
	strcpy(ndmpIP, commandData.client.ip);
	strcpy(ndmpPort, commandData.client.port);
	strcpy(ndmpAccount, commandData.ndmp.account);
	strcpy(ndmpPasswd, commandData.ndmp.passwd);

	strcpy(currentBackupset,commandData.backupset);
	strcpy(jobBackupset,commandData.backupset);

	if(strlen(commandData.volumeDB.parentBackupset) <= 0)
	{
		strcpy(commandData.volumeDB.parentBackupset,commandData.backupset);
	}

	
	

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
					strcpy(ABIOMASTER_NDMP_DEBUG_FOLDER, moduleConfig[i].optionValue);						
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
			else if (!strcmp(moduleConfig[i].optionName, NDMP_WAIT_TAPE_MEDIA_TIME))
			{
				waitTapeMediaTime = atoi(moduleConfig[i].optionValue);
			}
			else if (!strcmp(moduleConfig[i].optionName, DEBUG_LEVEL))
			{
				DebugLevel=atoi(moduleConfig[i].optionValue);
				DebugLev = DebugLevel;
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


	// set a backupset media
	strcpy(backupsetVolume, commandData.mountStatus.volume);

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

void SendCommand_changeBackupset(int request, int reply, int execute, char * requestCommand, char * subCommand, char * executeCommand, char * sourceIP, char * sourcePort, char * sourceNodename, char * destinationIP, char * destinationPort, char * destinationNodename , char * changedBackupset)
{
	struct ck sendCommand;

	struct ckBackup changedCommandData;


	memset(&sendCommand, 0, sizeof(struct ck));
	memset(&changedCommandData, 0, sizeof(struct ckBackup));

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

	
	memcpy(&changedCommandData, &commandData, sizeof(struct ckBackup));

	memset(changedCommandData.backupset,0,sizeof(changedCommandData.backupset));
	strcpy(changedCommandData.backupset,changedBackupset);
	


	memcpy(sendCommand.dataBlock, &changedCommandData, sizeof(struct ckBackup));

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

		if(0 < DebugLevel)
		{
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "recvCommand.subCommand = %s", recvCommand.subCommand);
		}

		// send the message to appropriate thread
		if (!strcmp(recvCommand.subCommand, "EXIT_INNER_MESSAGE_TRANSFER"))
		{
			break;
		}
		else
		{
			if (!strcmp(recvCommandData.jobID, commandData.jobID))
			{
				if (!strcmp(recvCommand.subCommand, "GET_MOUNT_STATUS"))
				{
					va_msgsnd(midMedia, mtypeMedia, &recvCommand, sizeof(struct ck), 0);
				}
				else if (!strcmp(recvCommand.subCommand, "MOUNT_NEW_VOLUME"))
				{
					va_msgsnd(midMedia, mtypeMedia, &recvCommand, sizeof(struct ck), 0);
				}
				else if (!strcmp(recvCommand.subCommand, "WRITE_VOLUME_DB"))
				{
					va_msgsnd(midMedia, mtypeMedia, &recvCommand, sizeof(struct ck), 0);
				}
				else if (!strcmp(recvCommand.subCommand, "UPDATE_VOLUME_DB_NEXT_VOLUME"))
				{
					va_msgsnd(midMedia, mtypeMedia, &recvCommand, sizeof(struct ck), 0);
				}
				else if (!strcmp(recvCommand.subCommand, "GET_BACKUPSET_ID"))
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

int first;

int Backup()
{
	NdmpConnection connection;

	char ** lines;
	int * linesLength;
	int lineNumber;

	int i;

	

	ndmpFileSystemList fslist;
	int j;

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 1. make backup file list

	if ((lineNumber = va_load_text_file_lines(ABIOMASTER_NDMP_FILE_LIST_FOLDER, commandData.filelist, &lines, &linesLength)) > 0)
	{
		for (i = 0; i < lineNumber; i++)
		{
			if (!strncmp(lines[i], BACKUP_FILE_LIST_OPTION_INCLUDE, strlen(BACKUP_FILE_LIST_OPTION_INCLUDE)))
			{
				if (includeListNumber % DEFAULT_MEMORY_FRAGMENT == 0)
				{
					includeList = (char **)realloc(includeList, sizeof(char *) * (includeListNumber + DEFAULT_MEMORY_FRAGMENT));
					memset(includeList + includeListNumber, 0, sizeof(char *) * DEFAULT_MEMORY_FRAGMENT);
				}
				includeList[includeListNumber] = (char *)malloc(sizeof(char) * (linesLength[i] - (strlen(BACKUP_FILE_LIST_OPTION_INCLUDE) + 1) + 1));
				memset(includeList[includeListNumber], 0, sizeof(char) * (linesLength[i] - (strlen(BACKUP_FILE_LIST_OPTION_INCLUDE) + 1) + 1));
				strcpy(includeList[includeListNumber], lines[i] + (strlen(BACKUP_FILE_LIST_OPTION_INCLUDE) + 1));

				// include list�� ��� ������ϰ�� �ڿ� ���丮 ǥ�ð� �پ������� �̸� �����Ѵ�.
				if (includeList[includeListNumber][strlen(includeList[includeListNumber]) - 1] == DIRECTORY_IDENTIFIER && strlen(includeList[includeListNumber]) > 1)
				{
					includeList[includeListNumber][strlen(includeList[includeListNumber]) - 1] = '\0';
				}

				includeListNumber++;
			}
			else if (!strncmp(lines[i], BACKUP_FILE_LIST_OPTION_EXCLUDE, strlen(BACKUP_FILE_LIST_OPTION_EXCLUDE)))
			{
				if (excludeListNumber % DEFAULT_MEMORY_FRAGMENT == 0)
				{
					excludeList = (char **)realloc(excludeList, sizeof(char *) * (excludeListNumber + DEFAULT_MEMORY_FRAGMENT));
					memset(excludeList + excludeListNumber, 0, sizeof(char *) * DEFAULT_MEMORY_FRAGMENT);
				}
				excludeList[excludeListNumber] = (char *)malloc(sizeof(char) * (linesLength[i] - (strlen(BACKUP_FILE_LIST_OPTION_EXCLUDE) + 1) + 1));
				memset(excludeList[excludeListNumber], 0, linesLength[i] - (strlen(BACKUP_FILE_LIST_OPTION_EXCLUDE) + 1) + 1);
				strcpy(excludeList[excludeListNumber], lines[i] + (strlen(BACKUP_FILE_LIST_OPTION_EXCLUDE) + 1));

				// exclude list�� ��� ��� �̸����� directory�� file�� �����ؾ��ϱ� ������ ��� �ڿ� �پ��ִ� ���丮 ǥ�ø� �������� �ʴ´�.

#ifdef __ABIO_WIN
				// windows client������ exclude list�� windows file format���� ��ȯ�ؼ� �����Ѵ�.
				va_slash_to_backslash(excludeList[excludeListNumber]);
#endif

				excludeListNumber++;
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
		Error(FUNCTION_Backup, ERROR_JOB_OPEN_BACKUP_FILE_LIST);

		return -1;
	}

	// #issue148 ������� ����Ʈ�� ���� ���������̸� ����
	// ���������� ����� �Ѱ��� �ۿ� ���� ���ؼ� �ٷ� ����ó���� �ǳ�, ���߿� 2���̻� ����� ���������� ������ �ʿ���.

	// initialize variables
	memset(&fslist, 0, sizeof(ndmpFileSystemList));
	j = 0;

	if (va_ndmp_connect_ndmp_server(ndmpIP, (unsigned long)atol(ndmpPort), ndmpAccount, ndmpPasswd, &connection) == 0)
	{
		// nas�� volume ����� �о�´�.
		if (va_ndmp_config_get_fs_info(connection, &fslist) == 0)
		{
			for (i = 0; i < fslist.fsnum; i++)
			{
				// #issue148 size�� 0 �ϰ�쿡�� offline���� �Ǵ��ؼ� ����Ʈ�� �������� �ʴ´�.
				if(fslist.fs[i].size == 0)
				{
					for(j=0; j<includeListNumber; j++)
					{
						if(!strcmp(fslist.fs[i].filesystem, includeList[j]))
						{
							// �������� ����
							Error(FUNCTION_Backup, ERROR_NDMP_VOLUME_OFFLINE);

							return -1;
						}
					}
				}
			}			
			va_free(fslist.fs);
		}
		va_ndmp_connect_close(connection);
	}
	//

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 2. make a connection with ndmp server

	if ((connection = MakeNewConnection()) == NULL)
	{
		return -1;
	}


	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 3. run threads

	// run the inner message transfer
	if ((tidMessageTransfer = va_create_thread(InnerMessageTransfer, NULL)) == 0)
	{
		Error(FUNCTION_Backup, ERROR_RESOURCE_MAKE_THREAD);
		va_ndmp_connect_close(connection);

		return -1;
	}

	// volume�� load�Ѵ�.
	if (LoadNdmpTapeMedia(connection) == 0)
	{
		// set a tape block size
		if (va_ndmp_library_set_tape_block_size(connection, volumeBlockSize) != 0)
		{
			Error(FUNCTION_NdmpBackup, ERROR_VOLUME_TAPE_BLOCK_SIZE);
		}
	}

	first = 1;
	preNDMPBackupFileNumber = 0;
	
	for(includeNumber = 0; includeNumber < includeListNumber; includeNumber++)
	{
		if(jobComplete == 1)
		{
			break;
		}

		if(0 < DebugLevel)
		{
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "Backup() Start");
		}

		if(includeNumber != 0)
		{		
			if(0 < DebugLevel)
			{
				WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "currentBackupset = %s",currentBackupset);
			}
			//va_lltoa(va_atoll(currentBackupset) + 1,currentBackupset);
			//WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "currentBackupset = %s",currentBackupset);
			
			memset(previousMountedVolume,0,sizeof(previousMountedVolume));

			memset(backupsetVolume,0,sizeof(backupsetVolume));
			strcpy(backupsetVolume,mountedVolume);
			
			catalogBackupsetPosition = backupsetPosition;

		}

		if(0 < DebugLevel)
		{
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "!! 2. backupsetPosition : %lld", backupsetPosition);
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "!! 2. catalogBackupsetPosition : %lld", catalogBackupsetPosition);
		}

		// run the writer
		if ((tidNdmpBackup = va_create_thread(NdmpBackup, (void *)connection)) == 0)
		{
			Error(FUNCTION_Backup, ERROR_RESOURCE_MAKE_THREAD);
			va_ndmp_connect_close(connection);
			
			return -1;
		}

		//////////////////////////////////////////////////////////////////////////////////////////////////
		// 4. backup�� �����⸦ ��ٸ���

		// backup�� �����⸦ ��ٸ���.
		va_wait_thread(tidNdmpBackup);

		//////////////////////////////////////////////////////////////////////////////////////////////////
		// 5. backup�� ���� ���� ���� ó��

		if(0 < DebugLevel)
		{
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "commandData.fileNumber = %d",commandData.fileNumber);
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "preNDMPBackupFileNumber = %d",preNDMPBackupFileNumber);
		}

		if (!GET_ERROR(commandData.errorCode))
		{
			UpdateJobState(STATE_NDMP_MAKE_CATALOG_DB);

			// backup�� ������ ����� �����.
			qsort(ndmpBackupFileArray, (unsigned long)ndmpBackupFileArrayNumber, sizeof(dirinfoarray), va_ndmp_file_compare_ndmpbackupfilearray);
			if(0 < DebugLevel)
			{
				WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "dBackup() Start");

				if(ndmpBackupFileArray != NULL)
				{
					WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "ndmpBackupFileArray ==> parent : %lld", ndmpBackupFileArray->parent);
					WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "ndmpBackupFileArray ==> current : %d", ndmpBackupFileArray->current);
					WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "ndmpBackupFileArray ==> arraynum : %d", ndmpBackupFileArray->arraynum);
					WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "ndmpBackupFileArray ==> array->name : %s", ndmpBackupFileArray->array->name);
					WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "ndmpBackupFileArray ==> array->fullpath : %s", ndmpBackupFileArray->array->fullpath);
					WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "ndmpBackupFileArray ==> array->node : %lld", ndmpBackupFileArray->array->node);
					WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "ndmpBackupFileArray ==> array->parent : %lld", ndmpBackupFileArray->array->parent);
					WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "ndmpBackupFileArray ==> array->fh_info : %lld", ndmpBackupFileArray->array->fh_info);
					WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "ndmpBackupFileArray ==> array->mtime : %lld", ndmpBackupFileArray->array->mtime);
					WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "ndmpBackupFileArray ==> array->size : %lld", ndmpBackupFileArray->array->size);
					WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "ndmpBackupFileArray ==> array->filetype : %lld \n", ndmpBackupFileArray->array->filetype);
				}

				WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "ndmpBackupFileArrayNumber : %lld\n", ndmpBackupFileArrayNumber);

				if ((ndmpBackupFileInfo = va_ndmp_file_make_dir_info_array_to_dir_info_debug(NULL, ndmpBackupFileArray, ndmpBackupFileArrayNumber)) == NULL)
				{				
					Error(FUNCTION_Backup, ERROR_NDMP_HISTORY_MAKE_DIR_INFO_ARRAY);

					//return -1;
					continue;
				}
				WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "Backup() End\n");
			}
			else
			{
				if ((ndmpBackupFileInfo = va_ndmp_file_make_dir_info_array_to_dir_info(NULL, ndmpBackupFileArray, ndmpBackupFileArrayNumber)) == NULL)
				{
					Error(FUNCTION_Backup, ERROR_NDMP_HISTORY_MAKE_DIR_INFO_ARRAY);

					//return -1;
					continue;
				}
			}

			// ����� ������ ���� ������ ����Ѵ�.
			commandData.fileNumber = (int)ndmpBackupFileNumber;			

			// set a backupset media of the job
			strcpy(commandData.backupsetVolume, backupsetVolume);

			

			if (0 < commandData.fileNumber - preNDMPBackupFileNumber)
			{
				// master server���� volume db�� �����ϱ� ���� communication kernel data�� volume db�� �Ǿ �����ش�.
				MakeVolumeDB(VOLUME_USAGE_FILLING, &commandData.volumeDB);
				
				// backup�� ������ ����� �������� backupset catalog db�� �����.
				if (MakeCatalogDB() != 0)
				{
					va_ndmp_file_free_dir_info(ndmpBackupFileInfo);

					return -1;
				}
			}
			else
			{
				commandData.objectNumber++;
			}

			va_ndmp_file_free_dir_info(ndmpBackupFileInfo);			

			//////////////////////////////////////////////////////////////////////////////////////////////
			// 6. backup�� �����⸦ ��ٸ���
			if (SendCatalogToMaster() < 0)
			{
				return -1;
			}

			UpdateBackupsetList();

			if(0 < DebugLevel)
			{
				WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "SendENDCommand");
			}

			//////////////////////////////////////////////////////////////////////////////////////////////////
			// 6. write filemark
			if (driveStatus == DRIVE_OPENING)
			{
				// ���� mount�Ǿ� �ִ� volume�� ������ �����Ͱ� ������ filemark�� ����Ѵ�.
				if (backupsetSize > 0)
				{
					if (va_ndmp_library_write_filemark(connection, 1) != 0)
					{
						Error(FUNCTION_NdmpBackup, ERROR_VOLUME_WRITE_FILEMARK);
					}
					backupsetPosition++;
				}
			}		
			
			preNDMPBackupFileNumber = commandData.fileNumber;
			
			if(0 < DebugLevel)
			{
				WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "ABIOMASTER_NDMP_CATALOG_DB_FOLDER = %s",ABIOMASTER_NDMP_CATALOG_DB_FOLDER);
				WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "currentBackupset = %s",currentBackupset);
			}

			// backupset catalog db ����			
			va_remove(ABIOMASTER_NDMP_CATALOG_DB_FOLDER, currentBackupset);

			
			if(includeNumber  < (includeListNumber - 1))
			{
				if(SendENDCommand() < 0)
				{

				}
			}
		}
		else
		{
			commandData.objectNumber++;
			//break;
		}
	}

	jobComplete = 1;
	

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 7. unload the tape media and close the tape drive
	if (driveStatus == DRIVE_OPENING)
	{
		UnloadNdmpTapeMedia(connection);
	}

	// ndmp server���� ������ �����Ѵ�.
	va_ndmp_connect_close(connection);



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
int end;

va_thread_return_t NdmpBackup(void * arg)
{
	NdmpConnection connection;

	time_t writeStartTime;
	time_t writeEndTime;

	int currentBlockNumber;

	int r;


	end = 0;

	errorOccured = 0;

	nMoverPauseState = 0;
	nMoverHaltState = 0;
	nDataHaltState = 0;

	nMoverPauseReason = 0;
	nMoverHaltReason = 0;
	nDataHaltReason = 0;

	ndmpBackupFileArray = NULL;
	ndmpBackupFileArrayNumber = 0;
	
	//ndmpBackupFileNumber = 0;

	ndmpBackupFileInfo = NULL;

	inodeIndexArray = NULL;
	inodeIndexArrayNumber = 0;

	findRoot = 0;


	va_sleep(1);

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 1. initialize variables

	// ndmp server���� connection�� �����Ѵ�.
	connection = arg;


	// ����� nas volume�� ������� ���Ϸ� ǥ���Ѵ�.
	strcpy(commandData.backupFile, includeList[includeNumber]);


	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 2. load the tape media and move to backup position

	//////////////////////////////////////////////////////////////////////////////////////////////////
	SendJobLog(LOG_MSG_ID_NDMP_LOG, "NORMAL", "0.", includeList[includeNumber], "");
	va_ndmp_data_clear_env(connection);
	// 3. start ndmp backup
	// ��� ������ �����Ѵ�.
	if (jobComplete == 0)
	{
		// backup �� file system ����. ����� ���� 1���� file systm���� single backup �����ϹǷ� ���� include list�� �ִ� �Ͽ��� �̷��� �۾��Ѵ�.
		if (va_ndmp_data_add_env(connection, "FILESYSTEM", includeList[includeNumber]) != 0)
		{
			Error(FUNCTION_NdmpBackup, ERROR_NDMP_ENV_ADD_FILESYSTEM);
		}		
		// backup type�� dump�� ����
		else if (va_ndmp_data_add_env(connection, "TYPE", "dump") != 0)
		{
			Error(FUNCTION_NdmpBackup, ERROR_NDMP_ENV_ADD_TYPE);
		}
		// history (catalog db ������ ���Ͽ�) true ����
		else if (va_ndmp_data_add_env(connection, "HIST", "Y") != 0)
		{
			Error(FUNCTION_NdmpBackup, ERROR_NDMP_ENV_ADD_HISTORY);
		}
		// incremental backup�� base_date�� dump_date�� ����ؼ� �ϱ� ������ ndmp server�� dumpdates ������ ������Ʈ�� �ʿ����.
		else if (va_ndmp_data_add_env(connection, "UPDATE", "N") != 0)
		{
			Error(FUNCTION_NdmpBackup, ERROR_NDMP_ENV_ADD_UPDATE);
		}
		// �۾��� ����� ũ�⸦ �����ش�.
		else if (commandData.writeDetailedLog == 1 && va_ndmp_data_add_env(connection, "VERBOSE", "Y") != 0)
		{
			Error(FUNCTION_NdmpBackup, ERROR_NDMP_ENV_ADD_VERBOSE);
		}
		else if ((excludeList != NULL) && (strlen(excludeList[0]) > 0))
		{
			if(va_ndmp_data_add_env(connection, "EXCLUDE", excludeList[0]) !=0)
			{
				Error(FUNCTION_NdmpBackup, ERROR_NDMP_ENV_ADD_EXCLUDE);
			}
		}
	}

	// ��� ��Ŀ� ���� �ɼ��� �����Ѵ�.
	if (jobComplete == 0)
	{
		// full backup
		if (commandData.jobMethod == ABIO_NDMP_BACKUP_METHOD_NDMP_FULL)
		{
			if (va_ndmp_data_add_env(connection, "BASE_DATE", "0") != 0)
			{
				Error(FUNCTION_NdmpBackup, ERROR_NDMP_ENV_ADD_BASE_DATE);
			}
			else if (va_ndmp_data_add_env(connection, "LEVEL", "0") != 0)
			{
				Error(FUNCTION_NdmpBackup, ERROR_NDMP_ENV_ADD_LEVEL);
			}
		}
		// incremental backup
		else if (commandData.jobMethod == ABIO_NDMP_BACKUP_METHOD_NDMP_INCREMENTAL)
		{
			SetNdmpDumpDate();

			if (va_ndmp_data_add_env(connection, "BASE_DATE", commandData.ndmpDumpDate) != 0)
			{
				Error(FUNCTION_NdmpBackup, ERROR_NDMP_ENV_ADD_BASE_DATE);
			}
			else if (va_ndmp_data_add_env(connection, "LEVEL", "1") != 0)
			{
				Error(FUNCTION_NdmpBackup, ERROR_NDMP_ENV_ADD_LEVEL);
			}
		}
	}

#ifdef __ABIO_DEBUG
	if (jobComplete == 0)
	{
		/*
		if (va_ndmp_data_add_env(connection, "DEBUG", "Y") != 0)
		{
		Error(FUNCTION_NdmpBackup, ERROR_NDMP_ENV_ADD_DEBUG);
		}
		*/
	}
#endif

	// ��� �۾��� �����Ѵ�.
	if (jobComplete == 0)
	{
		// mover service (data service���� data�� �о� tape�� ���� service) listen for data service
		if (va_ndmp_mover_listen(connection, "read", "tcp") != 0)
		{
			Error(FUNCTION_NdmpBackup, ERROR_NDMP_MOVER_LISTEN);
		}
		// data service (disk���� data�� �о� mover service�� ������ service) connect to mover service
		else if (va_ndmp_data_connect(connection, "tcp", NULL, NULL) != 0)
		{
			Error(FUNCTION_NdmpBackup, ERROR_NDMP_DATA_CONNECT);
		}
		// backup ����
		else if (va_ndmp_data_start_backup(connection, "dump") != 0)
		{
			Error(FUNCTION_NdmpBackup, ERROR_NDMP_DATA_START_BACKUP);
		}
	}


	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 4. write data to the tape media
	UpdateJobState(STATE_NDMP_WRITING);

	writeStartTime = time(NULL);


	while (end == 0 && jobComplete == 0)
	{
		// ndmp server�κ��� mover halt state�� ���� ����, ���̾� data halt state�� ���� ��� ���� �� mover halt state�� ó���ϰ� ndmp state�� �˻��ϴ� ��쿡, ������ �߻��� ������ ó���ȴ�.
		// ��ó�� ndmp server�κ��� �����ؼ� �޼����� ���� ��쿡 �޼��� ������ Ÿ�̹��� ������ ���� ������ �߻��� ������ ó���� ���ɼ��� �ִ�.
		// �׷��� ndmp polling interval ���� ndmp server�κ��� ���� �޼����� ��� ó���� �ڿ� ndmp state�� �˻��ϵ��� �Ѵ�.
		while ((r = va_ndmp_util_poll_ndmp_request(connection, NDMP_POLLING_INTERVAL)) == 1 && jobComplete == 0)
		{
			if (va_ndmp_comm_process_request(connection) < 0)
			{
				Error(FUNCTION_NdmpBackup, ERROR_NDMP_REQUEST_PROCESS);

				break;
			}

			// ��� ���� ��Ȳ�� ������Ʈ�Ѵ�.
			if (va_ndmp_tape_get_current_block_number(connection, &currentBlockNumber) != 0)
			{
				Error(FUNCTION_NdmpBackup, ERROR_NDMP_TAPE_GET_CURRENT_BLOCK_NUMBER);

				break;
			}

			if (currentBlockNumber < 0)
			{
				Error(FUNCTION_NdmpBackup, ERROR_NDMP_TAPE_GET_CURRENT_BLOCK_NUMBER_INVALID);

				break;
			}

			backupsetSize = (__uint64)currentBlockNumber * volumeBlockSize;
			backupsetBlockNumber = currentBlockNumber;
			commandData.capacity = totalWriteSize + backupsetSize;
			commandData.fileNumber = (int)ndmpBackupFileNumber;
			
			writeEndTime = time(NULL);
			commandData.writeTime = writeEndTime - writeStartTime;


		}

		if (r < 0)
		{
			Error(FUNCTION_NdmpBackup, ERROR_NDMP_REQUEST_POLL);

			break;
		}


		// ndmp state�� �˻��Ѵ�.
		if (end == 0)
		{
			CheckNdmpState(connection);
		}

		// ��� ���� ��Ȳ�� ������Ʈ�Ѵ�.
		if (va_ndmp_tape_get_current_block_number(connection, &currentBlockNumber) != 0)
		{
			Error(FUNCTION_NdmpBackup, ERROR_NDMP_TAPE_GET_CURRENT_BLOCK_NUMBER);

			break;
		}

		if (currentBlockNumber < 0)
		{
			Error(FUNCTION_NdmpBackup, ERROR_NDMP_TAPE_GET_CURRENT_BLOCK_NUMBER_INVALID);

			break;
		}

		backupsetSize = (__uint64)currentBlockNumber * volumeBlockSize;
		backupsetBlockNumber = currentBlockNumber;
		commandData.capacity = totalWriteSize + backupsetSize;
		commandData.fileNumber = (int)ndmpBackupFileNumber;

		writeEndTime = time(NULL);
		commandData.writeTime = writeEndTime - writeStartTime;
	}

	// ��� ���� ��Ȳ�� ������Ʈ�Ѵ�.
	if (va_ndmp_tape_get_current_block_number(connection, &currentBlockNumber) != 0)
	{
		Error(FUNCTION_NdmpBackup, ERROR_NDMP_TAPE_GET_CURRENT_BLOCK_NUMBER);
	}

	if (currentBlockNumber < 0)
	{
		Error(FUNCTION_NdmpBackup, ERROR_NDMP_TAPE_GET_CURRENT_BLOCK_NUMBER_INVALID);
	}


	backupsetSize = (__uint64)currentBlockNumber * volumeBlockSize;
	backupsetBlockNumber = currentBlockNumber;
	totalWriteSize += backupsetSize;
	commandData.capacity = totalWriteSize;
	commandData.fileNumber = (int)ndmpBackupFileNumber;

	writeEndTime = time(NULL);
	commandData.writeTime = writeEndTime - writeStartTime;

	if (va_ndmp_data_get_env(connection, "DUMP_DATE", commandData.ndmpDumpDate) != 0)
	{ 
		Error(FUNCTION_NdmpBackup, ERROR_NDMP_DATA_GET_DUMP_DATE);
	}


	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 5. NDMP Server���� ������ �����Ѵ�.
	va_ndmp_mover_abort(connection);
	va_ndmp_data_abort(connection);
	va_ndmp_mover_stop(connection);
	va_ndmp_data_stop(connection);

#ifdef __ABIO_UNIX
	return NULL;
#else
	_endthreadex(0);
	return 0;
#endif
}

void CheckNdmpState(NdmpConnection connection)
{
	int currentBlockNumber;

	// data read �� ����Ǿ���. ������?
	if (nDataHaltState == 1)
	{
		switch (nDataHaltReason)
		{
		case NDMP_DATA_HALT_SUCCESSFUL:			// �����ߴ�.
			// backup �������� ���� ó��.
			//jobComplete = 1;
			end = 1;
			break;

		case NDMP_DATA_HALT_ABORTED:			// ��ҵǾ���.
			// backup �� ��ҵǾ���.
			Error(FUNCTION_CheckNdmpState, ERROR_NDMP_DATA_HALT_ABORTED);
			break;

		case NDMP_DATA_HALT_INTERNAL_ERROR:		// NDMP���� ������ ������ ����Ǿ���.
			Error(FUNCTION_CheckNdmpState, ERROR_NDMP_DATA_HALT_INTERNAL_ERROR);
			break;

		case NDMP_DATA_HALT_CONNECT_ERROR:		// data connect�� ������ ����Ǿ���.
			Error(FUNCTION_CheckNdmpState, ERROR_NDMP_DATA_HALT_CONNECT_ERROR);
			break;

		case NDMP_DATA_HALT_NA:					// data halt �� �ƴϴ�! �̷����� ���� ���̴�. �ִٸ� ������ ó���ؾ���.
		default:								//�� �� ���� ������.
			Error(FUNCTION_CheckNdmpState, ERROR_NDMP_DATA_HALT_UNKNOWN);
			break;
		}
	}

	// tape write�� ����Ǿ���. ������?
	if (nMoverHaltState == 1)
	{
		switch (nMoverHaltReason)
		{
		case NDMP_MOVER_HALT_CONNECT_CLOSED:	// connect�� ������ ����Ǿ���.
			// connect �� ����Ǿ���. backup �� ������ ���� �ƴ϶�� ��ҵ� ���̴�.
			if (nDataHaltState == 0)
			{
				Error(FUNCTION_CheckNdmpState, ERROR_NDMP_MOVER_HALT_CONNECT_CLOSED);
			}
			break;

		case NDMP_MOVER_HALT_ABORTED:			// ��ҵǾ���.
			Error(FUNCTION_CheckNdmpState, ERROR_NDMP_MOVER_HALT_ABORTED);
			break;

		case NDMP_MOVER_HALT_INTERNAL_ERROR:	// NDMP���� ������ ������ ����Ǿ���.
			Error(FUNCTION_CheckNdmpState, ERROR_NDMP_MOVER_HALT_INTERNAL_ERROR);
			break;

		case NDMP_MOVER_HALT_CONNECT_ERROR:		// ���� ������ ����Ǿ���.
			Error(FUNCTION_CheckNdmpState, ERROR_NDMP_MOVER_HALT_CONNECT_ERROR);
			break;

		case NDMP_MOVER_HALT_NA:				// mover�� halt���� �ʾҴ�! �̷����� ���� ���̴�. �ִٸ� ������ ó���ؾ���.
		default:								//�� �� ���� ������.
			Error(FUNCTION_CheckNdmpState, ERROR_NDMP_MOVER_HALT_UNKNOWN);
			break;
		}
	}

	// tape write�� �����. ������?
	if (nMoverPauseState == 1)
	{
		//nMoverPauseReason
		switch (nMoverPauseReason)
		{
		case NDMP_MOVER_PAUSE_EOM:				// mover �� tape�� ���� �����ؼ� pause �Ǿ���.
			nMoverPauseState = 0;
			nMoverPauseReason = 0;

			// ��� ���� ��Ȳ�� ������Ʈ�Ѵ�.
			if (va_ndmp_tape_get_current_block_number(connection, &currentBlockNumber) != 0)
			{
				Error(FUNCTION_CheckNdmpState, ERROR_NDMP_TAPE_GET_CURRENT_BLOCK_NUMBER);

				break;
			}

			if (currentBlockNumber < 0)
			{
				Error(FUNCTION_CheckNdmpState, ERROR_NDMP_TAPE_GET_CURRENT_BLOCK_NUMBER_INVALID);

				break;
			}

			backupsetSize = (__uint64)currentBlockNumber * volumeBlockSize;
			backupsetBlockNumber = currentBlockNumber;
			totalWriteSize += backupsetSize;
			commandData.capacity = totalWriteSize;

			// ���ο� media�� mount�Ѵ�.
			if (MountNdmpNewVolume(connection) < 0)
			{
				break;
			}

			// mover continue
			if (va_ndmp_mover_continue(connection) != 0)
			{
				Error(FUNCTION_CheckNdmpState, ERROR_NDMP_MOVER_CONTINUE);
				break;
			}

			UpdateJobState(STATE_NDMP_WRITING);
			backupsetSize = 0;
			backupsetBlockNumber = 0;

			break;

		case NDMP_MOVER_PAUSE_EOF:				// mover �� tape���� file mark�� �����ؼ� pause �Ǿ���.
			Error(FUNCTION_CheckNdmpState, ERROR_NDMP_MOVER_PAUSE_EOF);
			break;

		case NDMP_MOVER_PAUSE_SEEK:				// ã�� ���� data window�� �ٱ����̾ pause �Ǿ���.
			Error(FUNCTION_CheckNdmpState, ERROR_NDMP_MOVER_PAUSE_SEEK);
			break;

		case NDMP_MOVER_PAUSE_MEDIA_ERROR:		// tape�� read/write ������ �߻��ؼ� pause �Ǿ���.
			Error(FUNCTION_CheckNdmpState, ERROR_NDMP_MOVER_PAUSE_MEDIA_ERROR);
			break;

		case NDMP_MOVER_PAUSE_EOW:				// �۾��� mover window�� ���� �����Ͽ��� pause �Ǿ���.
			Error(FUNCTION_CheckNdmpState, ERROR_NDMP_MOVER_PAUSE_EOW);
			break;

		case NDMP_MOVER_PAUSE_NA:				// mover �� pause ���� �ʾҴ�. �̷����� ���� ���̴�.
		default:								//�� �� ���� ������.
			Error(FUNCTION_CheckNdmpState, ERROR_NDMP_MOVER_PAUSE_UNKNOWN);
			break;
		}
	}
}

int LoadNdmpTapeMedia(NdmpConnection connection)
{
	struct volumeDB volumeDB;

	int loadResult;

	time_t loadStartTime;
	time_t loadEndTime;



	UpdateJobState(STATE_NDMP_POSITIONING);


	// initialize variables;
	loadResult = 0;

	loadStartTime = time(NULL);
	loadEndTime = time(NULL);


	// set a mounted media
	memset(mountedVolume, 0, sizeof(mountedVolume));
	strcpy(mountedVolume, commandData.mountStatus.volume);

	printf("commandData.mountStatus.volume = %s\n",commandData.mountStatus.volume);

	// #issue148 disk�� open�ɶ����� �ݺ��Ѵ�. (ndmp.conf : "waitTapeMediaTime")
	if(waitTapeMediaTime == 0)
	{
		waitTapeMediaTime = 300;
	}
	va_sleep(waitTapeMediaTime);

	// open the tape drive
	if (va_ndmp_library_open_tape_drive(connection, commandData.mountStatus.device, O_RDWR, 1) == 0)
	{
		driveStatus = DRIVE_OPENING;

		// load the tape media
		if (va_ndmp_library_load_medium(connection) != 0)
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
	//

	// positioning
	if (loadResult == 0)
	{
		// media�� ����ִ� ��� labeling�� �ϰ�, �׷��� ������ �����͸� �� ��ġ���� tape�� ���´�.
		backupsetPosition = commandData.mountStatus.backupsetPosition;

		// media�� ����ִ� ��� media header�� ����. (labeling)
		if (backupsetPosition == 0)
		{
			printf("mountedVolume = %s\n",mountedVolume);

			// tape media�� media header�� ����.
			if (WriteMediaHeader(connection, mountedVolume) == 0)
			{
				// media header ������ filemark�� ����.
				if (va_ndmp_library_write_filemark(connection, 1) == 0)
				{
					backupsetPosition = 1;
				}
				else
				{
					Error(FUNCTION_LoadNdmpTapeMedia, ERROR_VOLUME_WRITE_FILEMARK);

					loadResult = -1;
				}
			}
		}
		// media�� ������� ���� ��쿡�� mount�� tape media�� �ʿ��� tape media�� �´��� �˻��ϰ�, �����͸� �� ��ġ���� tape�� ���´�.
		else
		{
			// media���� media header�� �о�´�.
			if (va_ndmp_library_read(connection, tapeHeader, volumeBlockSize, DATA_TYPE_NOT_CHANGE) == volumeBlockSize)
			{
				// media header�� �ִ� label ������ �д´�.
				memcpy(&volumeDB, tapeHeader, sizeof(struct volumeDB));
				va_change_byte_order_volumeDB(&volumeDB);		// network to host

				// media header�� label�� ��ϵǾ��ִ��� Ȯ���Ѵ�.
				if (va_is_abio_volume(&volumeDB) == 1)
				{
					// mount�� tape media�� �ʿ��� tape media�� �´��� �˻��Ѵ�.
					if (!strcmp(volumeDB.volume, commandData.mountStatus.volume))
					{
						// �����͸� �� ��ġ���� tape�� ���´�.
						if (va_ndmp_library_forward_filemark(connection, (int)backupsetPosition) != 0)
						{
							Error(FUNCTION_LoadNdmpTapeMedia, ERROR_VOLUME_MOVE_FILEMARK);

							loadResult = -1;
						}
					}
					else
					{
						Error(FUNCTION_LoadNdmpTapeMedia, ERROR_VOLUME_LABEL_DISCORDANCE);
						loadResult = -1;
					}
				}
				else
				{
					// label�� ��ϵǾ����� ���� ��� ������ ó���Ѵ�.
					// standalone tape drive�� ����ִ� tape media�� labeling�Ǿ����� ���� ��� �߻��Ѵ�.
					Error(FUNCTION_LoadNdmpTapeMedia, ERROR_VOLUME_NO_LABEL);
					loadResult = -1;
				}
			}
			else
			{
				// media header�� ��ϵǾ����� ���� ��� ������ ó���Ѵ�.
				// tape media�� labeling�Ǿ����� ���� ��� �߻��Ѵ�.
				Error(FUNCTION_LoadNdmpTapeMedia, ERROR_VOLUME_READ_DATA);

				loadResult = -1;
			}
		}
	}

	if (catalogBackupsetPosition == 0)
	{
		catalogBackupsetPosition = backupsetPosition;
	}

	loadEndTime = time(NULL);
	commandData.loadTime += loadEndTime - loadStartTime;


	return loadResult;
}

int WriteMediaHeader(NdmpConnection connection, char * mediaName)
{
	struct volumeDB volumeDB;



	memset(&volumeDB, 0, sizeof(struct volumeDB));
	memset(tapeHeader, 0, volumeBlockSize);

	// make a tape header
	volumeDB.version = CURRENT_VERSION_INTERNAL;
	strcpy(volumeDB.volume, mediaName);

	// write a media header to the media
	va_change_byte_order_volumeDB(&volumeDB);		// host to network
	memcpy(tapeHeader, &volumeDB, sizeof(struct volumeDB));
	va_change_byte_order_volumeDB(&volumeDB);		// network to host

	if (va_ndmp_library_write(connection, tapeHeader, volumeBlockSize, DATA_TYPE_NOT_CHANGE) != volumeBlockSize)
	{
		Error(FUNCTION_WriteMediaHeader, ERROR_VOLUME_WRITE_DATA);

		return -1;
	}


	return 0;
}

void UnloadNdmpTapeMedia(NdmpConnection connection)
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

int MakeCatalogDB()
{
	va_fd_t fdCatalog;
	char * catalogBuf;
	int catalogBufSize;



	catalogBuf = NULL;
	catalogBufSize = 0;

	if ((catalogBuf = (char *)malloc(DSIZ * DSIZ)) == NULL)
	{
		Error(FUNCTION_MakeCatalogDB, ERROR_RESOURCE_NOT_ENOUGH_MEMORY);

		return -1;
	}

	memset(catalogBuf, 0, DSIZ * DSIZ);

	// make catalog db
	//if ((fdCatalog = va_open(ABIOMASTER_NDMP_CATALOG_DB_FOLDER, commandData.backupset, O_CREAT|O_RDWR|O_APPEND, 1, 1)) != (va_fd_t)-1)
	if ((fdCatalog = va_open(ABIOMASTER_NDMP_CATALOG_DB_FOLDER, currentBackupset, O_CREAT|O_RDWR|O_APPEND, 1, 1)) != (va_fd_t)-1)
	{
		if (WriteCatalogDB(fdCatalog, ndmpBackupFileInfo, catalogBuf, &catalogBufSize) < 0)
		{
			va_close(fdCatalog);
			va_free(catalogBuf);

			Error(FUNCTION_MakeCatalogDB, ERROR_RESOURCE_WRITE_DATA);

			return -1;
		}

		if (catalogBufSize > 0)
		{	
			if (va_write(fdCatalog, catalogBuf, catalogBufSize, DATA_TYPE_NOT_CHANGE) < catalogBufSize)
			{
				Error(FUNCTION_MakeCatalogDB, ERROR_RESOURCE_WRITE_DATA);

				return -1;
			}
			else
			{
				catalogBufSize = 0;
			}
		}
		va_close(fdCatalog);
		va_free(catalogBuf);
	}
	else
	{
		Error(FUNCTION_MakeCatalogDB, ERROR_CATALOG_DB_MAKE_FILE);

		return -1;
	}


	return 0;
}

int WriteCatalogDB(va_fd_t fdCatalog, dirinfo * dir, char * catalogBuf, int * catalogBufSize)
{
	struct catalogDB catalogDB;

	static __int64 catalogOffset = 0;		// catalog db file�� ũ��

	int i;



	// initialize variables
	memset(&catalogDB, 0, sizeof(struct catalogDB));

	// make a catalog db
	catalogDB.st.version = CURRENT_VERSION_INTERNAL;
	if (dir->filetype < VIRBAK_NDMP_DIRECTORY || VIRBAK_NDMP_FILE_OTHER < dir->filetype)
	{
		if (dir->fullpath[strlen(dir->fullpath) - 1] == DIRECTORY_IDENTIFIER)
		{
			catalogDB.st.filetype = VIRBAK_NDMP_DIRECTORY;
		}
		else
		{
			catalogDB.st.filetype = VIRBAK_NDMP_FILE_REG;
		}
	}
	else
	{
		catalogDB.st.filetype = dir->filetype;
	}
	catalogDB.st.inode = dir->node;
	catalogDB.st.fh_info = dir->fh_info;
	catalogDB.st.mtime = dir->mtime;
	catalogDB.st.backupTime = commandData.startTime;
	catalogDB.st.size = dir->size;

	catalogDB.version = CURRENT_VERSION_INTERNAL;
	catalogDB.backupsetCopyNumber = 1;
	catalogDB.expire = 0;
	catalogDB.compress = 0;
	catalogDB.offset = catalogOffset;
	//strcpy(catalogDB.backupset, commandData.backupset);
	strcpy(catalogDB.backupset, currentBackupset);
	strcpy(catalogDB.volume, backupsetVolume);
	catalogDB.backupsetPosition = catalogBackupsetPosition;		// ndmp�� image ����̹Ƿ� ù��° backup media�� backupset position�� ����صд�.
	catalogDB.startBlockNumber = 0;
	catalogDB.endBlockNumber = 0;
	strcpy(catalogDB.endVolume, mountedVolume);

	catalogDB.length = (int)strlen(includeList[includeNumber]) + 1 + (int)strlen(dir->fullpath);
	catalogDB.length2 = 0;
	catalogDB.length3 = 0;


	// ���� ����� ũ�⸦ �����Ѵ�.
	commandData.filesSize += catalogDB.st.size;


	// catalog buffer�� ���� ũ�Ⱑ �߰��� ��� ���� ������ ũ�⺸�� ������ catalog buffer�� ���Ͽ� ����Ѵ�.
	if (DSIZ * DSIZ - *catalogBufSize < (int)sizeof(catalogDB) + catalogDB.length + catalogDB.length2 + catalogDB.length3)
	{	
		if (va_write(fdCatalog, catalogBuf, *catalogBufSize, DATA_TYPE_NOT_CHANGE) != *catalogBufSize)
		{
			Error(FUNCTION_WriteCatalogDB, ERROR_RESOURCE_WRITE_DATA);

			return -1;
		}
		else
		{
			*catalogBufSize = 0;
		}
	}


	// write a catalog db information
	va_change_byte_order_catalogDB(&catalogDB);		// host to network
	memcpy(catalogBuf + *catalogBufSize, &catalogDB, sizeof(struct catalogDB));
	*catalogBufSize += sizeof(struct catalogDB);
	va_change_byte_order_catalogDB(&catalogDB);		// network to host


	sprintf(catalogBuf + *catalogBufSize, "%s:%s", includeList[includeNumber], dir->fullpath);
	*catalogBufSize += catalogDB.length;

	catalogOffset += sizeof(struct catalogDB) + catalogDB.length + catalogDB.length2 + catalogDB.length3;

	// make catalog db
	for (i = 0; i < dir->listnum; i++)
	{  
		if (WriteCatalogDB(fdCatalog, &(dir->list[i]), catalogBuf, catalogBufSize) < 0)
		{
			return -1;
		}
	}

	return 0;
}

int SendENDCommand()
{
	struct ck recvCommand;
	struct ckBackup recvCommandData;
		

	SendCommand_changeBackupset(0, 1, 0, MODULE_NAME_NDMP_BACKUP, "END", MODULE_NAME_MASTER_BACKUP, serverIP, serverPort, serverNodename, masterIP, masterPort, masterNodename,currentBackupset);	

	if (va_msgrcv(midMedia, mtypeMedia, &recvCommand, sizeof(struct ck), 0) > 0)
	{	
		memcpy(&recvCommandData, recvCommand.dataBlock, sizeof(struct ckBackup));

		if (GET_ERROR(recvCommandData.errorCode))
		{
			errorLocation = ABIO_MASTER_SERVER;
			memcpy(commandData.errorCode, recvCommandData.errorCode, sizeof(struct errorCode) * ERROR_CODE_NUMBER);
			
			return -1;
		}

		memset(commandData.backupset,0,sizeof(commandData.backupset));

		strcpy(currentBackupset,recvCommandData.backupset);

	}
	else
	{
		Error(FUNCTION_MountNdmpNewVolume, ERROR_RESOURCE_MESSAGE_TRANSFER_REMOVED);

		return -1;
	}

	return 0;
}

int SendCatalogToMaster()
{
	va_sock_t serverSock;				// backup server wait connection from master server through this socket
	va_sock_t masterSock;				// backup server receive a port number that master server receive catalog db from backup server through this socket

	va_fd_t fdCatalog;

	struct ckBackup tmpCmdData;
	char tmpbuf[DATA_BLOCK_SIZE];
	int readSize;

	int result;
	int timeoutCnt;



	// initialize variables
	result = 0;
	timeoutCnt = 0;


	// backup server���� ������� backupset catalog db�� ���ռ��� �˻��Ѵ�.
	if (va_check_coordination_catalogdb(ABIOMASTER_NDMP_CATALOG_DB_FOLDER, currentBackupset) < 0)
	{
		Error(FUNCTION_SendCatalogToMaster, ERROR_CATALOG_DB_BREAK_COORDINATION);
		result = -1;
	}
	else
	{
		//#ISSUE144 ERROR_NETWORK_TIMEOUT�� ��õ�
		for(timeoutCnt = 0 ; timeoutCnt < NETWORK_TIME_OUT ; timeoutCnt++)
		{
			// master server���� catalog db�� ���� ������ ����.
			if ((serverSock = va_make_server_socket_iptable(masterIP, serverIP, portRule, portRuleNumber, commandData.catalogPort)) != -1)
			{
				// master server���� catalog db�� ������� �Ѵ�.
				UpdateJobState(STATE_NDMP_SEND_REQUEST_SAVING_CATALOG_DB);
				SendCommand_changeBackupset(0, 1, 0, MODULE_NAME_NDMP_BACKUP, "CATALOG", MODULE_NAME_MASTER_BACKUP, serverIP, serverPort, serverNodename, masterIP, masterPort, masterNodename,currentBackupset);

				// master server�κ����� ������ ��ٸ�.
				if ((masterSock = va_wait_connection(serverSock, TIME_OUT_JOB_CONNECTION, NULL)) >= 0)
				{
					UpdateJobState(STATE_NDMP_SEND_CATALOG_DB);

					// master server���� catalog db�� ����
					//if ((fdCatalog = va_open(ABIOMASTER_NDMP_CATALOG_DB_FOLDER, commandData.backupset, O_RDONLY, 1, 1)) != (va_fd_t)-1)
					if ((fdCatalog = va_open(ABIOMASTER_NDMP_CATALOG_DB_FOLDER, currentBackupset, O_RDONLY, 1, 1)) != (va_fd_t)-1)
					{
						while ((readSize = va_read(fdCatalog, tmpbuf, sizeof(tmpbuf), DATA_TYPE_NOT_CHANGE)) > 0)
						{
							if (va_send(masterSock, tmpbuf, readSize, 0, DATA_TYPE_NOT_CHANGE) <= 0)
							{
								Error(FUNCTION_SendCatalogToMaster, ERROR_NETWORK_MASTER_DOWN);
								result = -1;

								break;
							}
						}

						va_close(fdCatalog);
					}

					va_close_socket(masterSock, ABIO_SOCKET_CLOSE_CLIENT);
				}
				else
				{
					//va_write_error_code�Լ� ���ο��� CommandData�� errorCode�� Error ������ �����ϱ� ������
					//��õ��ÿ� Module���� �߻��ߴ� ������ ���ؼ� ó���ϰ� �ǹǷ� �ӽ� CommandData�� ����� �������� �۾��� �������� ����ϵ��� �Ѵ�.
					memcpy(&tmpCmdData, &commandData, sizeof(struct ckBackup));

					if(timeoutCnt < NETWORK_TIME_OUT -1)
					{
						//ERROR_NETWORK_TIMEOUT�� Log�� ����� ��õ�
						va_write_error_code(ERROR_LEVEL_ERROR, MODULE_NDMP_BACKUP, FUNCTION_SendCatalogToMaster, ERROR_NETWORK_TIMEOUT, &tmpCmdData);
						va_close_socket(serverSock, 0);
						continue;
					}
					else
					{
						Error(FUNCTION_SendCatalogToMaster, ERROR_NETWORK_TIMEOUT);
						result = -1;
					}				
				}

				va_close_socket(serverSock, 0);
			}
			else
			{
				Error(FUNCTION_SendCatalogToMaster, ERROR_NETWORK_PORT_BIND);
				result = -1;
			}
			break;
		}
	}


	return result;
}

void MakeVolumeDB(enum volumeUsage usage, struct volumeDB * returnVolumeDB)
{	
	struct volumeDB volumeDB;

	int currentNDMPBackupFileNumber = commandData.fileNumber - preNDMPBackupFileNumber;

	memset(&volumeDB, 0, sizeof(struct volumeDB));

	volumeDB.version = CURRENT_VERSION_INTERNAL;
	volumeDB.backupMethod = commandData.jobMethod;
	volumeDB.backupData = commandData.backupData;
	volumeDB.compress = commandData.compress;
	volumeDB.backupsetCopyNumber = 1;
	volumeDB.retention[0] = commandData.retention[0];
	volumeDB.retention[1] = commandData.retention[1];
	volumeDB.retention[2] = commandData.retention[2];
	volumeDB.backupsetStatus = ABIO_BACKUPSET_STATUS_CREATING;
	//volumeDB.fileNumber = commandData.fileNumber;
	volumeDB.fileNumber = currentNDMPBackupFileNumber;
	volumeDB.totalBlockNumber = backupsetBlockNumber;

	volumeDB.expirationTime = 0;
	volumeDB.fileSize = backupsetSize;
	volumeDB.backupsetPosition = backupsetPosition;
	volumeDB.locatedTime = commandData.startTime;
	volumeDB.backupTime = commandData.startTime;

	strcpy(volumeDB.volume, mountedVolume);
	strcpy(volumeDB.previousVolume, previousMountedVolume);
	strcpy(volumeDB.parentBackupset, commandData.volumeDB.parentBackupset);
	strcpy(volumeDB.backupset, currentBackupset);

	strcpy(volumeDB.jobBackupset, jobBackupset);


	strcpy(volumeDB.pool, commandData.pool);
	strcpy(volumeDB.client, commandData.client.nodename);
	strcpy(volumeDB.server, serverNodename);
	strcpy(volumeDB.jobID, commandData.jobID);
	strcpy(volumeDB.policy, commandData.policy);
	strcpy(volumeDB.schedule, commandData.schedule);

	
	if (usage == VOLUME_USAGE_FULL)
	{
		volumeDB.backupsetEndPosition = 0;
	}
	else
	{
		volumeDB.backupsetEndPosition = backupsetPosition + 1;
	}

	memcpy(returnVolumeDB, &volumeDB, sizeof(struct volumeDB));
}

int MountNdmpNewVolume(NdmpConnection connection)
{
	struct ck recvCommand;
	struct ckBackup recvCommandData;



	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 1. unload media

	UnloadNdmpTapeMedia(connection);


	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 2. make volume db and send the volume db to master server 
	MakeVolumeDB(VOLUME_USAGE_FULL, &commandData.volumeDB);
	UpdateJobState(STATE_NDMP_SEND_REQUEST_SAVING_VOLUME_DB);
	SendCommand(0, 1, 0, MODULE_NAME_NDMP_BACKUP, "WRITE_VOLUME_DB", MODULE_NAME_MASTER_BACKUP, serverIP, serverPort, serverNodename, masterIP, masterPort, masterNodename);

	if (va_msgrcv(midMedia, mtypeMedia, &recvCommand, sizeof(struct ck), 0) > 0)
	{
		UpdateJobState(STATE_NDMP_RECV_RESULT_SAVING_VOLUME_DB);
		memcpy(&recvCommandData, recvCommand.dataBlock, sizeof(struct ckBackup));

		if (GET_ERROR(recvCommandData.errorCode))
		{
			errorLocation = ABIO_MASTER_SERVER;
			memcpy(commandData.errorCode, recvCommandData.errorCode, sizeof(struct errorCode) * ERROR_CODE_NUMBER);

			Error(0, 0);
			return -1;
		}
	}
	else
	{
		Error(FUNCTION_MountNdmpNewVolume, ERROR_RESOURCE_MESSAGE_TRANSFER_REMOVED);

		return -1;
	}


	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 3. send a mount new media request to master server
	UpdateJobState(STATE_NDMP_SEND_REQUEST_MOUNT_NEW_VOLUME);
	SendCommand(0, 1, 0, MODULE_NAME_NDMP_BACKUP, "MOUNT_NEW_VOLUME", MODULE_NAME_MASTER_BACKUP, serverIP, serverPort, serverNodename, masterIP, masterPort, masterNodename);

	if (va_msgrcv(midMedia, mtypeMedia, &recvCommand, sizeof(struct ck), 0) > 0)
	{
		UpdateJobState(STATE_NDMP_RECV_RESULT_MOUNT_NEW_VOLUME);
		memcpy(&recvCommandData, recvCommand.dataBlock, sizeof(struct ckBackup));

		if (!GET_ERROR(recvCommandData.errorCode))
		{
			// new volume mount ����� Ȯ���Ѵ�.
			if (!strcmp(mountedVolume, recvCommandData.mountStatus.volume))
			{
				Error(FUNCTION_MountNdmpNewVolume, ERROR_VOLUME_DB_SAME_SPAN_VOLUME);

				return -1;
			}

			// ������ mount�Ǿ����� media�� �̸��� �����Ѵ�.
			memset(previousMountedVolume, 0, sizeof(previousMountedVolume));
			strcpy(previousMountedVolume, mountedVolume);

			// ���� mount�� media�� ������ �����Ѵ�.
			commandData.mountStatus.backupsetPosition = recvCommandData.mountStatus.backupsetPosition;
			commandData.mountStatus.capacity = recvCommandData.mountStatus.capacity;

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
		Error(FUNCTION_MountNdmpNewVolume, ERROR_RESOURCE_MESSAGE_TRANSFER_REMOVED);

		return -1;
	}


	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 4. send the old volume db to master server
	// ���� media�� volume db�� next media�� ���� mount�� media�� �����ϰ� master server�� �����Ѵ�.
	strcpy(commandData.volumeDB.nextVolume, recvCommandData.mountStatus.volume);
	UpdateJobState(STATE_NDMP_SEND_REQUEST_SAVING_NEXT_VOLUME_DB);
	SendCommand(0, 1, 0, MODULE_NAME_NDMP_BACKUP, "UPDATE_VOLUME_DB_NEXT_VOLUME", MODULE_NAME_MASTER_BACKUP, serverIP, serverPort, serverNodename, masterIP, masterPort, masterNodename);

	if (va_msgrcv(midMedia, mtypeMedia, &recvCommand, sizeof(struct ck), 0) > 0)
	{
		UpdateJobState(STATE_NDMP_RECV_RESULT_SAVING_NEXT_VOLUME_DB);
		memcpy(&recvCommandData, recvCommand.dataBlock, sizeof(struct ckBackup));

		if (GET_ERROR(recvCommandData.errorCode))
		{
			errorLocation = ABIO_MASTER_SERVER;
			memcpy(commandData.errorCode, recvCommandData.errorCode, sizeof(struct errorCode) * ERROR_CODE_NUMBER);

			Error(0, 0);
			return -1;
		}
	}
	else
	{
		Error(FUNCTION_MountNdmpNewVolume, ERROR_RESOURCE_MESSAGE_TRANSFER_REMOVED);

		return -1;
	}


	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 5. load and position the media
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
	moduleNumber = MODULE_NDMP_BACKUP;

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


	// ��� �۾��� ������.
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

	if(0 < DebugLevel)
	{
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "errorOccured : %d", errorOccured);
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "functionNumber : %d", functionNumber);
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "errorNumber : %d", errorNumber);
	}

	if (errorOccured == 0)
	{
		errorOccured = 1;

		// ������ ����Ѵ�.
		if (functionNumber != 0 && errorNumber != 0)
		{
			if(functionNumber == FUNCTION_NdmpBackup
				|| functionNumber == FUNCTION_CheckNdmpState 
				|| errorNumber == ERROR_NDMP_HISTORY_MAKE_DIR_INFO_ARRAY)
			{
				va_write_error_code(ERROR_LEVEL_WARNING, MODULE_NDMP_BACKUP, functionNumber, errorNumber, &commandData);
			}
			else
			{
				va_write_error_code(ERROR_LEVEL_ERROR, MODULE_NDMP_BACKUP, functionNumber, errorNumber, &commandData);
				jobComplete = 1;
			}
		}

		end = 1;
		// �۾��� �������� ǥ���Ѵ�.
		//jobComplete = 1;
	}

	if(0 < DebugLevel)
	{
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "end : %d", end);
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "jobComplete : %d", jobComplete);
	}
}

void Exit()
{
	struct ck closeCommand;

	int i;



	// �۾��� �������� ǥ���Ѵ�.
	jobComplete = 1;

	// thread�� �����⸦ ��ٸ���.
	if (tidLog != 0)
	{
		va_wait_thread(tidLog);
	}

	// master server�� �۾��� �������� �˸���.
	UpdateJobState(STATE_NDMP_SEND_RESULT_DATA_TRANSFER);
	SendCommand_changeBackupset(0, 1, 0, MODULE_NAME_NDMP_BACKUP, "END UNMOUNT", MODULE_NAME_MASTER_BACKUP, serverIP, serverPort, serverNodename, masterIP, masterPort, masterNodename,currentBackupset);

	// InnerMessageTransfer thread ����
	if (tidMessageTransfer != 0)
	{
		memset(&closeCommand, 0, sizeof(struct ck));
		strcpy(closeCommand.subCommand, "EXIT_INNER_MESSAGE_TRANSFER");
		va_msgsnd(midClient, mtypeClient, &closeCommand, sizeof(struct ck), 0);

		va_wait_thread(tidMessageTransfer);
	}

	// backupset catalog db ����
	//va_remove(ABIOMASTER_NDMP_CATALOG_DB_FOLDER, commandData.backupset);
	va_remove(ABIOMASTER_NDMP_CATALOG_DB_FOLDER, currentBackupset);

	// backup file list ����
	va_remove(ABIOMASTER_NDMP_FILE_LIST_FOLDER, commandData.filelist);

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

	for (i = 0; i < backupLogDescriptionNumber; i++)
	{
		va_free(backupLogDescription[i].description);
	}
	va_free(backupLogDescription);

	for (i = 0; i < includeListNumber; i++)
	{
		va_free(includeList[i]);
	}
	va_free(includeList);

	for (i = 0; i < excludeListNumber; i++)
	{
		va_free(excludeList[i]);
	}
	va_free(excludeList);

	va_ndmp_file_free_inode_index_array(inodeIndexArray, inodeIndexArrayNumber);

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

	va_send_abio_log(masterIP, masterLogPort, commandData.jobID, NULL, LOG_LEVEL_JOB, MODULE_NDMP_BACKUP, logmsg);
}


dirinfo * va_ndmp_file_make_dir_info_array_to_dir_info_debug(dirinfo * dir, dirinfoarray * dirarray, __uint64 maxPosition)
{
	dirinfo * rdir;

	int i;

	WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "va_ndmp_file_make_dir_info_array_to_dir_info_debug() Start");	

	if (dirarray == NULL)
	{
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "2. dirarray == NULL");		
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "return NULL");
		return NULL;
	}

	if (dir == NULL)
	{
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "3. dir == NULL");		

		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "3. dirarray ==> parent : %lld", dirarray->parent);
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "3. dirarray ==> arraynum : %d", dirarray->arraynum);
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "3. dirarray ==> current : %d", dirarray->current);
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "3. dirarray ==> array->name : %s", dirarray->array->name);
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "3. dirarray ==> array->fullpath : %s", dirarray->array->fullpath);
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "3. dirarray ==> array->node : %lld", dirarray->array->node);
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "3. dirarray ==> array->parent : %lld", dirarray->array->parent);
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "3. dirarray ==> array->fh_info : %lld", dirarray->array->fh_info);
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "3. dirarray ==> array->mtime : %lld", dirarray->array->mtime);
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "3. dirarray ==> array->size : %lld", dirarray->array->size);
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "3. dirarray ==> array->filetype : %lld \n", dirarray->array->filetype);

		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "3. maxPosition : %lld\n", maxPosition);

		if ((i = va_ndmp_file_stat_index_ndmp_backup_file_array_search_debug(dirarray, maxPosition, 0)) == -1)
		{	
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "4. i : -1");

			return NULL;
		}
		else
		{
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "4. i : %d", i);
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "*. dirarray ==> parent : %lld", dirarray->parent);



			rdir = dirarray[i].array;
			dirarray[i].array = (dirinfo*)NULL;
			va_ndmp_file_make_dir_info_array_to_dir_info_debug(rdir, dirarray, maxPosition);
			va_free(dirarray);
			dirarray = NULL;

			return rdir;
		}
	}
	else
	{
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "5. dir != NULL");



		if ((i = va_ndmp_file_stat_index_ndmp_backup_file_array_search_debug(dirarray, maxPosition, dir->node)) == -1)
		{
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "6. i : -1");
			return NULL;
		}
		else
		{
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "6. i : %d", i);

			dir->listnum = dirarray[i].arraynum;
			dir->list = dirarray[i].array;
			dirarray[i].array = (dirinfo*)NULL;

			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "7. dir ==> name : %s", dir->name);
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "7. dir ==> fullpath : %s", dir->fullpath);
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "7. dir ==> node : %lld", dir->node);
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "7. dir ==> parent : %lld", dir->parent);
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "7. dir ==> fh_info : %lld", dir->fh_info);
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "7. dir ==> mtime : %lld", dir->mtime);
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "7. dir ==> mtime : %lld", dir->size);
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "7. dir ==> filetype : %lld \n", dir->filetype);

			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "7. maxPosition : %lld\n", maxPosition);
		}

		for (i = 0; i < dir->listnum; i++)
		{
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "8. i : %d, dir->listnum : %d", i, dir->listnum);
			rdir = dir->list + i;



			if (rdir->filetype == VIRBAK_NDMP_DIRECTORY)
			{
				rdir->fullpath = (char *)malloc(sizeof(char) * (strlen(dir->fullpath) + strlen(rdir->name) + 1 + 1));
				memset(rdir->fullpath, 0, sizeof(char) * (strlen(dir->fullpath) + strlen(rdir->name) + 1 + 1));
				sprintf(rdir->fullpath, "%s%s%c", dir->fullpath, rdir->name, DIRECTORY_IDENTIFIER);

				WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "10. rdir->fullpath : %s", rdir->fullpath);
				WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "10. dir->fullpath : %s", dir->fullpath);

				va_ndmp_file_make_dir_info_array_to_dir_info_debug(rdir, dirarray, maxPosition);

			}
			else
			{
				rdir->fullpath = (char *)malloc(sizeof(char) * (strlen(dir->fullpath) + strlen(rdir->name) + 1));
				memset(rdir->fullpath, 0, sizeof(char) * (strlen(dir->fullpath) + strlen(rdir->name) + 1));
				sprintf(rdir->fullpath, "%s%s", dir->fullpath, rdir->name);

				WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "11. rdir->fullpath : %s", rdir->fullpath);
				WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "11. dir->fullpath : %s", dir->fullpath);
				WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "11. rdir->name : %s", rdir->name);


			}
		}
	}

	WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "va_ndmp_file_make_dir_info_array_to_dir_info_debug() End\n");
	return dir;
}


int va_ndmp_file_stat_index_ndmp_backup_file_array_search_debug(dirinfoarray * darray, __uint64 max, __uint64 node)
{
	WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "va_ndmp_file_stat_index_ndmp_backup_file_array_search_debug() Start");	
	int low;
	int high;
	int mid;	

	if (darray == NULL || max == 0)
	{
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "1. max : %lld", max);
		return -1;
	}
	if (node == 0)
	{
		return 0;
	}
	low = 0;
	high = (int)max - 1;

	WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "3. low : %d, high : %d, max : %lld", low, high, max);

	while (low <= high)
	{
		mid = (low + high) / 2;
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "4. mid :%d", mid);
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "4. darray[mid].parent : %lld, node : %lld", darray[mid].parent, node);

		if (darray[mid].parent > node)
		{
			high = mid - 1;			
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "5. high : %d", high);
		}
		else if (darray[mid].parent < node)
		{
			low = mid + 1;	
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "5. low : %d", low);
		}
		else
		{
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "5. low : %d", mid);
			return mid;			
		}
	}
	WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "va_ndmp_file_stat_index_ndmp_backup_file_array_search_debug() End");	
	return -1;
}


int SetNdmpDumpDate()
{	
	va_fd_t fdBackupsetList;
	char backupsetListFile[MAX_PATH_LENGTH];
	struct backupsetList backupsetList;
	struct backupsetList backupsetListLast;
	

	// initialize variables
	memset(backupsetListFile, 0, sizeof(backupsetListFile));
	
	va_get_backupset_list_file_name(commandData.client.nodename, ABIO_BACKUP_DATA_NDMP, backupsetListFile);
		
	// ���� ���������� ����� ������� ã�Ƽ� �� ���Ŀ� ����� ������ ����ϵ��� dump date�� �����Ѵ�.
	memset(&backupsetListLast, 0, sizeof(struct backupsetList));
	if ((fdBackupsetList = va_open(ABIOMASTER_NDMP_CATALOG_DB_FOLDER, backupsetListFile, O_RDONLY, 1, 1)) != (va_fd_t)-1)
	{
		while (va_read(fdBackupsetList, &backupsetList, sizeof(struct backupsetList), DATA_TYPE_BACKUPSET_LIST) > 0)
		{
			if(0 < DebugLevel)
			{
				WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "backupsetList.instance = %s",backupsetList.instance);
				WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "includeList[includeNumber] = %s",includeList[includeNumber]);
				WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, " backupsetListLast.backupTime(%llu) < backupsetList.backupTime(%llu)",backupsetListLast.backupTime,backupsetList.backupTime);
			}

			if (!strncmp(backupsetList.instance, includeList[includeNumber],strlen(backupsetList.instance)))
			{
				//WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, " backupsetList.backupsetListStatus = %d",backupsetList.backupsetListStatus);

				if(backupsetList.backupsetListStatus == ABIO_LIST_BACKUPSET_STATUS_VALID)
				{
					if (backupsetListLast.backupTime < backupsetList.backupTime)
					{
						memcpy(&backupsetListLast, &backupsetList, sizeof(struct backupsetList));
					}
				}
			}
		}
		
		va_close(fdBackupsetList);
	}
	
	if (backupsetListLast.backupTime == 0)
	{
		strcpy(commandData.ndmpDumpDate, "0");
	}
	else
	{
		strcpy(commandData.ndmpDumpDate, backupsetListLast.ndmpDumpDate);
	}
	
	if(0 < DebugLevel)
	{
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "commandData.ndmpDumpDate = %s",commandData.ndmpDumpDate);
	}

	return 0;
}

int UpdateBackupsetList()
{
	int findBackupset;

	va_fd_t fdBackupsetList;
	char backupsetListFile[MAX_PATH_LENGTH];	// backup set id list file name
	struct backupsetList backupsetListNew;		// ���� �߰��� backup set
	struct backupsetList backupsetList;

	// make new backupset list item
	memset(&backupsetListNew, 0, sizeof(struct backupsetList));
	backupsetListNew.version = CURRENT_VERSION_INTERNAL;
	backupsetListNew.backupMethod = commandData.jobMethod;
	backupsetListNew.backupTime = commandData.startTime;
	strcpy(backupsetListNew.backupset, commandData.backupset);
	strcpy(backupsetListNew.parentBackupset, commandData.volumeDB.parentBackupset);
	strcpy(backupsetListNew.volume[0], commandData.backupsetVolume);
		
	// ndmp backup�� ��� volume �̸��� dump date �� �����Ѵ�.
	if (commandData.backupData == ABIO_BACKUP_DATA_NDMP)
	{
		backupsetListNew.backupsetListStatus = ABIO_LIST_BACKUPSET_STATUS_VALID;
		strncpy(backupsetListNew.instance, includeList[includeNumber], strlen(includeList[includeNumber]));
		strcpy(backupsetListNew.ndmpDumpDate, commandData.ndmpDumpDate);
	}	

	// set the backupset list file name
	memset(backupsetListFile, 0, sizeof(backupsetListFile));

	va_get_backupset_list_file_name(commandData.client.nodename, commandData.backupData, backupsetListFile);

	if(0 < DebugLevel)
	{
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "backupsetListNew.ndmpDumpDate = %s",backupsetListNew.ndmpDumpDate);	
	}

	findBackupset = 0;

	if ((fdBackupsetList = va_open(ABIOMASTER_NDMP_CATALOG_DB_FOLDER, backupsetListFile, O_CREAT|O_RDWR|O_APPEND, 1, 1)) != (va_fd_t)-1)
	{
		while (va_read(fdBackupsetList, &backupsetList, sizeof(struct backupsetList), DATA_TYPE_BACKUPSET_LIST) > 0)
		{
			if(0 < DebugLevel)
			{
				WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "backupsetList.instance = %s",backupsetList.instance);
				WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_BACKUP_LOG, "includeList[includeNumber] = %s",includeList[includeNumber]);				
			}

			if (!strncmp(backupsetList.instance, backupsetListNew.instance,strlen(backupsetListNew.instance)))
			{
				findBackupset = 1;

				va_lseek(fdBackupsetList, -(va_offset_t)sizeof(struct backupsetList), SEEK_CUR);
				va_write(fdBackupsetList, &backupsetListNew, sizeof(struct backupsetList), DATA_TYPE_BACKUPSET_LIST);

				break;
			}
		}
		
		if(findBackupset != 1)
		{
			va_write(fdBackupsetList, &backupsetListNew, sizeof(struct backupsetList), DATA_TYPE_BACKUPSET_LIST);			
		}

		va_close(fdBackupsetList);
	}
	else
	{	
		return -1;
	}
	return 0;
}