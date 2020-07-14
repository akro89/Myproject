#include "require.h"
#include "abio.h"
#include "lib.h"
#include "vendor.h"
#include "oracle12c.h"
#include "oracle12c_lib.h"


extern char ABIOMASTER_CLIENT_FOLDER[MAX_PATH_LENGTH];
extern char ABIOMASTER_CLIENT_CATALOG_DB_FOLDER[MAX_PATH_LENGTH];
extern char ABIOMASTER_CLIENT_FILE_LIST_FOLDER[MAX_PATH_LENGTH];
extern char ABIOMASTER_CLIENT_LOG_FOLDER[MAX_PATH_LENGTH];
extern char ORACLE12C_LOG_FOLDER[MAX_PATH_LENGTH];


extern struct ck command;				// communication kernel command
extern struct ckBackup commandData;		// communication kernel command data


//////////////////////////////////////////////////////////////////////////////////////////////////
// message transfer variables
extern va_trans_t midCK;		// message transfer id
extern int mtypeCK;				// message type number


//////////////////////////////////////////////////////////////////////////////////////////////////
// ip and port number
extern char masterIP[IP_LENGTH];					// master server ip address
extern char masterPort[PORT_LENGTH];				// master server ck port
extern char masterNodename[NODE_NAME_LENGTH];		// master server node name
extern char masterLogPort[PORT_LENGTH];				// master server httpd logger port
extern char serverIP[IP_LENGTH];					// backup server ip address
extern char serverPort[PORT_LENGTH];				// backup server ck port
extern char serverNodename[NODE_NAME_LENGTH];		// backup server node name
extern char clientIP[IP_LENGTH];					// client ip address
extern char clientPort[PORT_LENGTH];				// client ck port
extern char clientNodename[NODE_NAME_LENGTH];		// client node name

extern int maxOutboundBandwidth;					// Outbound �ִ� Bandwidth


//////////////////////////////////////////////////////////////////////////////////////////////////
// variables for oracle connection
extern char username[ORACLE_USERNAME_LENGTH];
extern char account[ORACLE_ACCOUNT_LENGTH];
extern char passwd[ORACLE_PASSWORD_LENGTH];
extern char sid[ORACLE_SID_LENGTH];
extern char home[ORACLE_HOME_LENGTH];
extern char sqlcmdUtil[MAX_PATH_LENGTH];
extern int deleteArchiveLog;

extern char environmentValueSid[MAX_PATH_LENGTH];
extern char environmentValueHome[MAX_PATH_LENGTH];

extern char queryFile[MAX_PATH_LENGTH];
extern char outputFile[MAX_PATH_LENGTH];

extern int ramainArchiveLogCount;
//////////////////////////////////////////////////////////////////////////////////////////////////
// variables for oracle file list
static struct tableSpaceItem * tableSpaceList;
static int tableSpaceNumber;

static struct archiveLogItem * archiveLogList;
static int archiveLogNumber;

static struct archiveLogItem * archiveLogFileList;
static int archiveLogFileNumber;


static char archiveLogFolder[MAX_PATH_LENGTH];
static char archiveLogMode[MAX_PATH_LENGTH];
static char databaseOpenMode[MAX_PATH_LENGTH];

//////////////////////////////////////////////////////////////////////////////////////////////////
// message transfer variables
static va_trans_t midClient;		// message transfer id to receive a message from the parallel server
static int mtypeClient;


//////////////////////////////////////////////////////////////////////////////////////////////////
// backup buffer variables
#ifdef __ABIO_LINUX
	static char * unalignedBuffer[CLIENT_BACKUP_BUFFER_NUMBER];
#endif
static char * backupBuffer[CLIENT_BACKUP_BUFFER_NUMBER];		// backup buffer
static int backupBufferSize[CLIENT_BACKUP_BUFFER_NUMBER];		// backup buffer�� ����ִ� ������ ��
extern int readDiskBufferSize;							// backup buffer�� ũ��

static va_sem_t semid[CLIENT_BACKUP_BUFFER_NUMBER];
static int bufferIndex;


//////////////////////////////////////////////////////////////////////////////////////////////////
// variables for backup tcp/ip socket
static va_sock_t dataSock;


//////////////////////////////////////////////////////////////////////////////////////////////////
// variables for backup files
static char ** includeList;
static int includeListNumber;


static char ** archiveList;
int archiveFileCount;

	
//////////////////////////////////////////////////////////////////////////////////////////////////
// error control
// ��� ���߿� ������ �߻����� �� ������ �߻��� ��Ҹ� ����Ѵ�. 0�� client error, 1�� server error�� �ǹ��Ѵ�.
// client���� ������ �߻��� ���, ������ �߻��ߴٰ� server���� �˷��ش�.
static enum {ABIO_CLIENT, ABIO_BACKUP_SERVER,ABIO_STOP} errorLocation;
///��� �۾� ����
int backupJobState;

static int errorOccured;
static int jobComplete;

static int serverConnected;
static int recvErrorFromServer;


//////////////////////////////////////////////////////////////////////////////////////////////////
// job thread
static va_thread_t tidSender;
static va_thread_t tidMessageTransfer;
static va_thread_t tidKeepSocketAlive;


//////////////////////////////////////////////////////////////////////////////////////////////////
// backup log
extern struct _backupLogDescription * backupLogDescription;
extern int backupLogDescriptionNumber;


//////////////////////////////////////////////////////////////////////////////////////////////////
// function prototype*/
static void InitBackup();
static va_thread_return_t InnerMessageTransfer(void * arg);
int CheckNodeAlive(char * ip, char * port);
int CheckNode(struct ckBackup recvCommandData);
static int StartBackup();
static int BackupTableSpace(char * backupTableSpaceName, char * databaseName);
static int BackupArchiveLog();
static int BackupControlFileBinary();
static int BackupControlFileTrace();
static void DeleteArchiveLog();

static int BackupFile(char * file, struct abio_file_stat * file_stat, enum fileType filetype, char * backupTableSpaceName, char * databaseName);
static int FileBackup(char * file, struct abio_file_stat * file_stat, enum fileType filetype, char * backupTableSpaceName, char * databaseName);
#ifdef __ABIO_AIX
	static int BackupRawDeviceAIX(char * file, struct abio_file_stat * file_stat, char * backupTableSpaceName, char * databaseName);
#endif
#if defined(__ABIO_AIX) || defined(__ABIO_SOLARIS) || defined(__ABIO_HP) || defined(__ABIO_LINUX)
	static int RawDeviceBackup(char * file, struct abio_file_stat * file_stat, __uint64 fsSize, char * backupTableSpaceName, char * databaseName);
#endif
static void MakeFileHeader(char * file, struct abio_file_stat * file_stat, enum fileType filetype, char * databaseName, char * backupTableSpaceName, struct fileHeader * returnFileHeader);
static void MakeAbioStat(char * file, struct abio_file_stat * file_stat, struct abio_stat * ast);
static int SendFileHeader(struct fileHeader * fileHeader, enum fileHeaderType headerType);
static int SendFileContents(char * file, struct fileHeader * fileHeader);
static int SendFileContentsCompress(char * file, struct fileHeader * fileHeader);
#ifdef __ABIO_AIX
	static int SendFileContentsRawDeviceAIX(char * file, struct fileHeader * fileHeader, __uint64 fsSize);
	static int SendFileContentsRawDeviceAIXCompress(char * file, struct fileHeader * fileHeader, __uint64 fsSize);
#endif
static int SendBackupData(void * dataBuffer, unsigned int dataSize);
static int SendBackupBuffer(int flushBackupBuffer);
static va_thread_return_t SendBackupFile(void * arg);
static va_thread_return_t KeepSocketAlive(void * arg);
static int SendEndOfBackup();

static int GetTableSpaceList();
static int GetArchiveLogDir();
static int GetOracleLogMode();
static int GetDatabaseOpenMode(char * databaseName);
static int GetArchiveLogFile(char* filename);
static int BeginBackup(char * tableSpace, char * database);
static int EndBackup(char * tableSpace, char* database);
static int SwitchLogFile();
static int AlterBackupControlFileBinary(char * controlFilePath);
static int AlterBackupControlFileTrace(char * controlFilePath);
static int ArchiveLogComparator(const void * a1, const void * a2);

static void ServerError(struct ckBackup * recvCommandData);
static void WaitForServerError();
static void Error();
static void Exit();

static void PrintBackupTableSpaceResult(char * tableSpaceName, char* databaseName, int result, enum backupFailCause failCause);
static void PrintBackupFileResult(char * file, enum fileType filetype, int result, enum backupFailCause failCause);
static void CompareArchiveList();

//////////////////////////////////////////////////////////////////////////////////////////////////
// etc

void CheckAdvancedOption();


int Backup()
{
	InitBackup();
	
	if (StartBackup() < 0)
	{
		Error();
		
		Exit();
		
		return -1;
	}
	
	
	Exit();
	
	return 0;
}


static void InitBackup()
{
	int i;
	
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// variables for oracle file list
	tableSpaceList = NULL;
	tableSpaceNumber = 0;
	
	archiveLogList = NULL;
	archiveLogNumber = 0;
	

	archiveLogFileList = NULL;
	archiveLogFileNumber = 0;


	memset(archiveLogFolder, 0, sizeof(archiveLogFolder));
	memset(archiveLogMode, 0, sizeof(archiveLogMode));
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// message transfer variables
	midClient = (va_trans_t)-1;
	mtypeClient = 0;
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// backup buffer variables
	for (i = 0; i < CLIENT_BACKUP_BUFFER_NUMBER; i++)
	{
		#ifdef __ABIO_LINUX
			unalignedBuffer[i] = NULL;
		#endif
		
		backupBuffer[i] = NULL;
		backupBufferSize[i] = 0;
		
		semid[i] = (va_sem_t)-1;
	}
	
	bufferIndex = 0;
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// variables for backup tcp/ip socket
	dataSock = -1;
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// variables for backup files
	includeList = NULL;
	includeListNumber = 0;
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// error control
	errorLocation = ABIO_CLIENT;
	
	errorOccured = 0;
	jobComplete = 0;
	
	serverConnected = 0;
	recvErrorFromServer = 0;
	
	backupJobState=0;
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// job thread
	tidSender = 0;
	tidMessageTransfer = 0;

		archiveList = NULL;
	archiveFileCount = 0;
}


static void Error()
{
	if (errorOccured == 0)
	{
		errorOccured = 1;
		
		//////////////////////////////////////////////////////////////////////////////////////////////////
		// 1. ���� �޼����� client�� backup server ���̿��� �ְ�޾Ƽ� ���ʿ��� ��� ������ �ν��� �� �ֵ��� �Ѵ�.
		
		// client���� �߻��� ������ ��� backup server�� ������ �߻������� �˸���, backup server���� ������ �ν��� ������ ��ٸ���.
		if (errorLocation == ABIO_CLIENT)
		{
			// backup server���� client���� ������ �߻������� �˷��ش�.
			SendCommand(1, 0, 1, MODULE_NAME_SLAVE_BACKUP, "ERROR", MODULE_NAME_DB_ORACLE12C, clientIP, clientPort, clientNodename, serverIP, serverPort, serverNodename);
			
			// �������� ������ �޾Ҵٴ� �޼����� �ö����� ��ٸ���.
			WaitForServerError();
		}
		else if (errorLocation == ABIO_STOP)
		{	
		}
		// server���� �߻��� ������ ��� backup server���� ������ �ν������� �˸���.
		else
		{
			// backup server���� client���� ������ �޾Ҵٰ� �˷��ش�.
			SendCommand(1, 0, 1, MODULE_NAME_SLAVE_BACKUP, "ERROR", MODULE_NAME_DB_ORACLE12C, clientIP, clientPort, clientNodename, serverIP, serverPort, serverNodename);
		}
		
		//��� state ���������� ����.
		backupJobState=0;
		// �۾��� �������� ǥ���Ѵ�.
		jobComplete = 1;
		
		
		//////////////////////////////////////////////////////////////////////////////////////////////////
		// 2. backup server���� ������ �ν��� �Ͱ� ������� client���� �۾��� ������ �� �ֵ��� �Ѵ�.
		
		// �������� ���¿��� release�Ǿ���ϴ� Ƚ������ �ѹ��� release�ؼ� reader�� ����� �� �ֵ��� �Ѵ�.
		va_release_semaphore(semid[1]);
	}
}

static void Exit()
{
	struct ck closeCommand;
	
	int i;
	int j;
	
	
	
	// �۾��� �������� ǥ���Ѵ�.
	jobComplete = 1;
	
	// �������� ���¿��� release�Ǿ���ϴ� Ƚ������ �ѹ��� release�ؼ� sender�� ����� �� �ֵ��� �Ѵ�.
	va_release_semaphore(semid[0]);
	va_wait_thread(tidSender);
	
	// InnerMessageTransfer thread ����
	if (tidMessageTransfer != 0)
	{
		memset(&closeCommand, 0, sizeof(struct ck));
		strcpy(closeCommand.subCommand, "EXIT_INNER_MESSAGE_TRANSFER");
		va_msgsnd(midClient, mtypeClient, &closeCommand, sizeof(struct ck), 0);
		
		va_wait_thread(tidMessageTransfer);
	}
	
	// backup file list ����
	va_remove(ABIOMASTER_CLIENT_FILE_LIST_FOLDER, commandData.filelist);
	
	// system �ڿ� �ݳ�
	#ifdef __ABIO_WIN
		va_remove_message_transfer(midClient);
	#endif
	
	for (i = 0; i < CLIENT_BACKUP_BUFFER_NUMBER; i++)
	{
		va_remove_semaphore(semid[i]);
	}
	
	// memory ����
	for (i = 0; i < CLIENT_BACKUP_BUFFER_NUMBER; i++)
	{
		#ifdef __ABIO_LINUX
			va_free(unalignedBuffer[i]);
		#else
			va_free(backupBuffer[i]);
		#endif
	}
	
	for (i = 0; i < includeListNumber; i++)
	{
		va_free(includeList[i]);
	}
	va_free(includeList);
	
	for (i = 0; i < tableSpaceNumber; i++)
	{
		for (j = 0; j < tableSpaceList[i].fileNumber; j++)
		{
			va_free(tableSpaceList[i].dataFile[j]);
		}
		va_free(tableSpaceList[i].dataFile);
	}
	va_free(tableSpaceList);
	
	if (archiveLogList != NULL)
	{
		for (i = 0; i < archiveLogNumber; i++)
		{
			va_free(archiveLogList[i].name);
		}
		va_free(archiveLogList);
	}

	for (i = 0 ; i < archiveFileCount; i++)
	{
		va_free(archiveList[i]);
	}
	va_free(archiveList);

	for (i = 0; i < archiveLogFileNumber; i++)
	{	
		va_free(archiveLogFileList[i].name);
	}
	va_free(archiveLogFileList);
}


static int StartBackup()
{
	char backupTableSpaceName[MAX_PATH_LENGTH];
	char backupDatabaseName[MAX_PATH_LENGTH];

	int i;

	int archive_flag;
	archive_flag = 0;
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 1. �۾��� �����ϱ� ���� �����ؾߵǴ� �������� �����Ѵ�.
	
	// END_OF_BACKUP file header�� backup server�� ������ �� data socket�� �������� �ʾƵ� backup server���� END_OF_BACKUP file header�� ��� data block�� ���� �� �ֵ��� �ϱ� ����
	// backup buffer�� ũ�⸦ ������� �ɼ��� �����ϰ� media block�� data buffer size��ŭ �����Ѵ�.
	readDiskBufferSize = commandData.mountStatus.blockSize - VOLUME_BLOCK_HEADER_SIZE;
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 2. �۾��� �ʿ��� �ڿ����� �����´�.
	
	// set the message transfer id to receive a message from the master server and backup server
	mtypeClient = commandData.mountStatus.mtype2;
	#ifdef __ABIO_UNIX
		midClient = midCK;
	#elif __ABIO_WIN
		if ((midClient = va_make_message_transfer(mtypeClient)) == (va_trans_t)-1)
		{
			va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_BACKUP, FUNCTION_BACKUP_StartBackup, ERROR_RESOURCE_MAKE_MESSAGE_TRANSFER, &commandData);
			
			return -1;
		}
	#endif
	
	// run the inner message transfer
	if ((tidMessageTransfer = va_create_thread(InnerMessageTransfer, NULL)) == 0)
	{
		va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_BACKUP, FUNCTION_BACKUP_StartBackup, ERROR_RESOURCE_MAKE_THREAD, &commandData);
		
		return -1;
	}
	
	// backup buffer�� �����.
	for (i = 0; i < CLIENT_BACKUP_BUFFER_NUMBER; i++)
	{
		#ifdef __ABIO_LINUX
			unalignedBuffer[i] = (char *)malloc(sizeof(char) * (readDiskBufferSize * 2));
			memset(unalignedBuffer[i], 0, sizeof(char) * (readDiskBufferSize * 2));
			
			backupBuffer[i] = va_ptr_align(unalignedBuffer[i], readDiskBufferSize);
		#else
			backupBuffer[i] = (char *)malloc(sizeof(char) * readDiskBufferSize);
			memset(backupBuffer[i], 0, sizeof(char) * readDiskBufferSize);
		#endif
	}
	
	// backup buffer ����ȭ�� �ʿ��� ������� �����.
	for (i = 0; i < CLIENT_BACKUP_BUFFER_NUMBER; i++)
	{
		if ((semid[i] = va_make_semaphore(0)) == (va_sem_t)-1)
		{
			va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_BACKUP, FUNCTION_BACKUP_StartBackup, ERROR_RESOURCE_MAKE_SEMAPHORE, &commandData);
			
			return -1;
		}
	}
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 3. backup server�� �����Ѵ�.
	
	// backup server�� �����Ѵ�.
	if ((dataSock = va_connect(serverIP, commandData.client.dataPort, 1)) == -1)
	{
		va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_BACKUP, FUNCTION_BACKUP_StartBackup, ERROR_NETWORK_DATA_SOCKET_DOWN, &commandData);
		
		return -1;
	}
	
	// backup server�� ����Ǿ����� ǥ���Ѵ�.
	serverConnected = 1;
	
	// backup server�� client�� ������ �˷��ش�.
	strcpy(backupBuffer[0], ABIO_CHECK_CODE);
	strcpy(backupBuffer[0] + ABIO_CHECK_CODE_SIZE, commandData.backupset);
	strcpy(backupBuffer[0] + ABIO_CHECK_CODE_SIZE + BACKUPSET_ID_LENGTH, ABIO_VERSION_TAG);
	
	commandData.version = (enum version)htonl(commandData.version);		// host to network
	memcpy(backupBuffer[0] + ABIO_CHECK_CODE_SIZE + BACKUPSET_ID_LENGTH + ABIO_VERSION_TAG_SIZE, &commandData.version, sizeof(commandData.version));
	commandData.version = (enum version)htonl(commandData.version);		// network to host
	
	if (va_send(dataSock, backupBuffer[0], readDiskBufferSize, 0, DATA_TYPE_NOT_CHANGE) != readDiskBufferSize)
	{
		serverConnected = 0;
		
		va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_BACKUP, FUNCTION_BACKUP_StartBackup, ERROR_NETWORK_DATA_SOCKET_DOWN, &commandData);
		
		return -1;
	}
	
	// server�� ������ ������ thread�� �����.
	if ((tidKeepSocketAlive = va_create_thread(KeepSocketAlive, NULL)) == 0)
	{
		va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_BACKUP, FUNCTION_BACKUP_StartBackup, ERROR_RESOURCE_MAKE_THREAD, &commandData);
		
		return -1;
	}
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 5. backup file ����� �������� ��� ���� �α׸� ����Ѵ�.
	
	if ((includeListNumber = va_load_text_file_lines(ABIOMASTER_CLIENT_FILE_LIST_FOLDER, commandData.filelist, &includeList, NULL)) == 0)
	{
		va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_BACKUP, FUNCTION_BACKUP_StartBackup, ERROR_JOB_OPEN_BACKUP_FILE_LIST, &commandData);
		
		return -1;
	}
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 6. �ʿ��� thread�� �����Ŵ
	
	// run the backup file sender
	if ((tidSender = va_create_thread(SendBackupFile, NULL)) == 0)
	{
		va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_BACKUP, FUNCTION_BACKUP_StartBackup, ERROR_RESOURCE_MAKE_THREAD, &commandData);
		
		return -1;
	}
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 7. ORACLE ȯ���� �����ϰ� DB ������ backup server�� ����
	// check archive log mode

	CheckAdvancedOption();

	if (GetOracleLogMode() < 0)
	{
		va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_BACKUP, FUNCTION_BACKUP_StartBackup, ERROR_ORACLE_CHECK_ARCHIVE_LOG_MODE, &commandData);
		
		return -1;
	}
	if (!strcmp(archiveLogMode, "NOARCHIVELOG"))
	{
		SendJobLog(LOG_MSG_ID_ORACLE_NO_ARCHIVE_MODE, "");
		va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_BACKUP, FUNCTION_BACKUP_StartBackup, ERROR_ORACLE_NO_ARCHIVE_LOG_MODE, &commandData);
		
		return -1;
	}
	
	// table space ����� �о�´�.
	if (GetTableSpaceList() < 0)
	{
		va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_BACKUP, FUNCTION_BACKUP_StartBackup, ERROR_ORACLE_GET_TABLESPACE_LIST, &commandData);
		
		return -1;
	}

	
	// archive log folder�� �о�´�.
	if (GetArchiveLogDir() < 0)
	{
		va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_BACKUP, FUNCTION_BACKUP_StartBackup, ERROR_ORACLE_GET_ARCHIVE_LOG_FOLDER, &commandData);
		
		return -1;
	}
	
	
	// table space�� ����Ѵ�.
	if (commandData.jobMethod == ABIO_DB_BACKUP_METHOD_ORACLE12C_FULL)
	{
		// ��ü table space�� ����ϵ��� �����Ǿ����� Ȯ���ؼ� ����Ѵ�.
		for (i = 0; i < includeListNumber; i++)
		{
			if (!strcmp(includeList[i], ORACLE_BACKUP_FILE_LIST_OPTION_OBJECT_ENTIRE_DATABASES))
			{
				archive_flag = 1;
				if (BackupTableSpace(NULL,NULL) < 0)
				{
					return -1;
				}
				
				break;
			}
		}

		// ��ü table space�� ����ϵ��� �������� �ʾ����� ������ ���� table space�� ã�Ƽ� ����Ѵ�.
		if (i == includeListNumber)
		{
			for (i = 0; i < includeListNumber; i++)
			{
				if (!strncmp(includeList[i], ORACLE_BACKUP_FILE_LIST_OPTION_OBJECT_TABLESPACE, strlen(ORACLE_BACKUP_FILE_LIST_OPTION_OBJECT_TABLESPACE)))
				{
					memset(backupTableSpaceName, 0, sizeof(backupTableSpaceName));
					strcpy(backupTableSpaceName, includeList[i] + strlen(ORACLE_BACKUP_FILE_LIST_OPTION_OBJECT_TABLESPACE) + 1);					
				}
				else if(!strncmp(includeList[i], ORACLE_BACKUP_FILE_LIST_OPTION_OBJECT_DATABASE, strlen(ORACLE_BACKUP_FILE_LIST_OPTION_OBJECT_DATABASE)))
				{
					memset(backupDatabaseName, 0, sizeof(backupDatabaseName));
					strcpy(backupDatabaseName, includeList[i] + strlen(ORACLE_BACKUP_FILE_LIST_OPTION_OBJECT_DATABASE) + 1);
					if (BackupTableSpace(backupTableSpaceName, backupDatabaseName) < 0)
					{
						return -1;
					}

				}
				else if(!strcmp(includeList[i],"ARCHIVE"))
				{
					archive_flag = 1;
				}

			}
		}
	}
	else
	{
		archive_flag = 1;
	}

	if (archive_flag == 1)
	{
		// archive log�� ����Ѵ�.
		if (BackupArchiveLog() < 0)
		{
			return -1;
		}
		
		// control file�� binary format���� ����Ѵ�.
		if (BackupControlFileBinary() < 0)
		{
			return -1;
		}
		
		// control file�� user readable form���� ����Ѵ�.
		if (BackupControlFileTrace() < 0)
		{
			return -1;
		}
	}
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 8. backup server�� ����� �������� �˸���.
	
	if (SendEndOfBackup() < 0)
	{
		return -1;
	}
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 9. archive log�� �����Ѵ�.
	
	// multi-session ����� �� ��� archive log�� �����ϰ� �Ǹ� ���ÿ� ����Ǵ� �ٸ� ��� �۾��� ������ �ְ� �Ǳ� ������, archive log�� �������� �ʴ´�.
	if (archive_flag == 1)
	{
		DeleteArchiveLog();
	}
	

	return 0;
}

static int BackupControlFileBinary()
{
	char controlFilePath[MAX_PATH_LENGTH];
	
	struct abio_file_stat file_stat;
	
	
	
	// control file�� binary format���� ����Ѵ�.
	memset(controlFilePath, 0, sizeof(controlFilePath));
	sprintf(controlFilePath, "%s%c%s_%s", ORACLE12C_LOG_FOLDER, FILE_PATH_DELIMITER, commandData.backupset, ORACLE_CONTROL_FILE_BINARY_BACKUP_NAME);
	va_remove(NULL, controlFilePath);
	
	// control file name�� ABIO_FILE_LENGTH * 2���� ��� ������� �ʴ´�.
	if (strlen(controlFilePath) > ABIO_FILE_LENGTH * 2)
	{
		PrintBackupFileResult(controlFilePath, ORACLE12C_CONTROL_FILE, -1, FILE_LENGTH_ERROR);
		
		return 0;
	}
	
	// control file�� abio oracle log folder�� ����Ѵ�.
	if (AlterBackupControlFileBinary(controlFilePath) < 0)
	{
		va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_BACKUP, FUNCTION_BACKUP_BackupControlFileBinary, ERROR_ORACLE_GET_CONTROL_FILE, &commandData);
		
		va_remove(NULL, controlFilePath);
		
		return -1;
	}
	
	// control file�� ������ ����Ѵ�.
	if (va_lstat(NULL, controlFilePath, &file_stat) == 0)
	{
		if (BackupFile(controlFilePath, &file_stat, ORACLE12C_CONTROL_FILE, NULL, NULL) < 0)
		{
			va_remove(NULL, controlFilePath);
			
			return -1;
		}
	}
	else
	{
		PrintBackupFileResult(controlFilePath, ORACLE12C_CONTROL_FILE, -1, OPEN_FILE_ERROR);
	}
	
	// ����� control file�� �����Ѵ�.
	va_remove(NULL, controlFilePath);
	
	
	return 0;
}

static int BackupControlFileTrace()
{
	char controlFilePath[MAX_PATH_LENGTH];
	
	struct abio_file_stat file_stat;
	
	
	
	// control file�� user readable form���� ����Ѵ�.
	memset(controlFilePath, 0, sizeof(controlFilePath));
	sprintf(controlFilePath, "%s%c%s_%s", ORACLE12C_LOG_FOLDER, FILE_PATH_DELIMITER, commandData.backupset, ORACLE_CONTROL_FILE_TRACE_BACKUP_NAME);
	va_remove(NULL, controlFilePath);
	
	// control file name�� ABIO_FILE_LENGTH * 2���� ��� ������� �ʴ´�.
	if (strlen(controlFilePath) > ABIO_FILE_LENGTH * 2)
	{
		// user readable form�� control file ����� ����� �����ϴ��� ������ ó������ �ʴ´�.
		
		return 0;
	}
	
	// control file�� abio oracle log folder�� ����Ѵ�.
	if (AlterBackupControlFileTrace(controlFilePath) == 0)
	{
		// control file�� ������ ����Ѵ�.
		if (va_lstat(NULL, controlFilePath, &file_stat) == 0)
		{
			if (BackupFile(controlFilePath, &file_stat, ORACLE12C_CONTROL_FILE, NULL, NULL) < 0)
			{
				// user readable form�� control file ����̶� ����� �ߴ��ؾ��� ������ �߻��ϸ� ������ ó���Ѵ�.
				
				va_remove(NULL, controlFilePath);
				
				return -1;
			}
		}
		else
		{
			// user readable form�� control file ����� ����� �����ϴ��� ������ ó������ �ʴ´�.
		}
	}
	else
	{
		// user readable form�� control file ����� ����� �����ϴ��� ������ ó������ �ʴ´�.
	}
	
	// ����� control file�� �����Ѵ�.
	va_remove(NULL, controlFilePath);
	
	
	return 0;
}

static int BeginBackup(char * tableSpace,char * database)
{
	char query[1024];
	char query2[1024];
	
	
	memset(query, 0, sizeof(query));
	memset(query2, 0, sizeof(query2));


	
	sprintf(query, SET_SESSION_CONTAINER, database);
	sprintf(query2, BEGIN_BACKUP_QUERY, tableSpace);
	
	if (ExecuteSql(query,query2) != 0)
	{
		va_remove(ABIOMASTER_CLIENT_FOLDER, queryFile);
		va_remove(ABIOMASTER_CLIENT_FOLDER, outputFile);
		
		return -1;
	}
	
	
	va_remove(ABIOMASTER_CLIENT_FOLDER, queryFile);
	va_remove(ABIOMASTER_CLIENT_FOLDER, outputFile);
	
	
	return 0;
}

static int EndBackup(char * tableSpace,char * database)
{
	char query[1024];
	char query2[1024];
		
	
	memset(query, 0, sizeof(query));
	memset(query2, 0, sizeof(query2));
	
	sprintf(query, SET_SESSION_CONTAINER, database);
	sprintf(query2, END_BACKUP_QUERY, tableSpace);

	if (ExecuteSql(query,query2) != 0)
	{
		va_remove(ABIOMASTER_CLIENT_FOLDER, queryFile);
		va_remove(ABIOMASTER_CLIENT_FOLDER, outputFile);
		
		return -1;
	}
	
	
	va_remove(ABIOMASTER_CLIENT_FOLDER, queryFile);
	va_remove(ABIOMASTER_CLIENT_FOLDER, outputFile);
	
	
	return 0;
}

static int BackupTableSpace(char * backupTableSpaceName, char * databaseName)
{
	struct abio_file_stat file_stat;
	int openmode;
	int i;
	int j;
	int r;
	
	openmode = 0;

	for (i = 0; i < tableSpaceNumber; i++)
	{
		// Ư�� table space�� ����ϵ��� ������ ��쿡 ������ table space�� �´��� Ȯ���Ѵ�.*/
		if (backupTableSpaceName != NULL && backupTableSpaceName[0] != '\0' && ((strcmp(tableSpaceList[i].tableSpace, backupTableSpaceName) != 0) || (strcmp(tableSpaceList[i].database, databaseName) != 0) ))
		{
			continue;
		}
		
		// table space �̸��� ABIO_FILE_LENGTH ���� ��� ������� �ʴ´�.
		if (strlen(tableSpaceList[i].tableSpace) > ABIO_FILE_LENGTH)
		{
			PrintBackupTableSpaceResult(tableSpaceList[i].tableSpace, tableSpaceList[i].database, -1, FILE_LENGTH_ERROR);
			
			continue;
		}
		
		// data file �̸��� ABIO_FILE_LENGTH ���� ��� ������� �ʴ´�.
		for (j = 0; j < tableSpaceList[i].fileNumber; j++)
		{
			if (strlen(tableSpaceList[i].dataFile[j]) > ABIO_FILE_LENGTH)
			{
				PrintBackupFileResult(tableSpaceList[i].dataFile[j], ORACLE12C_REGULAR_DATA_FILE, -1, FILE_LENGTH_ERROR);
				
				break;
			}
		}
		
		if (j < tableSpaceList[i].fileNumber)
		{
			continue;
		}
		if (GetDatabaseOpenMode(tableSpaceList[i].database) < 0)
		{
			PrintBackupTableSpaceResult(tableSpaceList[i].tableSpace, tableSpaceList[i].database, -1, (enum backupFailCause)0);

		}		
		if (!strcmp(databaseOpenMode, "READ WRITE"))
		{		
			openmode = 1;
		}

		if (openmode == 1)
		{
			// execute begin backup
			if (BeginBackup(tableSpaceList[i].tableSpace, tableSpaceList[i].database) < 0)
			{
				// table space�� begin backup�� �����ϴµ� ���������� end backup�� �����ϰ� �ٽ� begin backup�� �õ��Ѵ�.
				// begin backup�� �����ϴµ� �����ϴ� ���� ������ ���� ���� data file�� �սǵ� ���, end backup�� ������ �ȵ� ��찡 �ִ�.
				// ���⼭�� end backup�� ������ �ȵ� ��츸 ó���� �� �ִ�.
				
				// execute end backup
				EndBackup(tableSpaceList[i].tableSpace, tableSpaceList[i].database);
				
				// execute begin backup
				if (BeginBackup(tableSpaceList[i].tableSpace, tableSpaceList[i].database) < 0)
				{
					PrintBackupTableSpaceResult(tableSpaceList[i].tableSpace, tableSpaceList[i].database, -1, (enum backupFailCause)0);
					
					continue;
				}
			}			
		}

		// backup data file
		for (j = 0; j < tableSpaceList[i].fileNumber; j++)
		{
			if (va_lstat(NULL, tableSpaceList[i].dataFile[j], &file_stat) == 0)
			{
				if ((r = BackupFile(tableSpaceList[i].dataFile[j], &file_stat, ORACLE12C_REGULAR_DATA_FILE, tableSpaceList[i].tableSpace, tableSpaceList[i].database)) != 0)
				{
					break;
				}
			}
			else
			{
				PrintBackupFileResult(tableSpaceList[i].dataFile[j], ORACLE12C_REGULAR_DATA_FILE, -1, OPEN_FILE_ERROR);
			}
		}
		
		if (openmode == 1)
		{
			// execute end backup
			EndBackup(tableSpaceList[i].tableSpace,  tableSpaceList[i].database);	
			openmode = 0;
		}
		PrintBackupTableSpaceResult(tableSpaceList[i].tableSpace, tableSpaceList[i].database, r, (enum backupFailCause)0);
		
		if (r < 0)
		{
			return -1;
		}
	}
	
	
	return 0;
}

static int BackupArchiveLog()
{
	char ** filelist;
	struct abio_file_stat fileStatus;
	
	int i;
	
	char temp[1024];
	
	// initialize variables
	filelist = NULL;
	
	
	// switch archive log file to next sequence
	if (SwitchLogFile() < 0)
	{
		va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_BACKUP, FUNCTION_BACKUP_BackupArchiveLog, ERROR_ORACLE_SWITCH_ARCHIVE_LOG, &commandData);
		
		return -1;
	}
	
	
	// get archive log file name
	if ((archiveLogNumber = va_scandir(archiveLogFolder, &filelist)) > 0)
	{
		archiveLogList = (struct archiveLogItem *)malloc(sizeof(struct archiveLogItem) * archiveLogNumber);
		memset(archiveLogList, 0, sizeof(struct archiveLogItem) * archiveLogNumber);
		
		// archive log file ����� �����.
		for (i = 0; i < archiveLogNumber; i++)
		{
			memset(temp,0,sizeof(temp));
			// copy the archive log file name
			archiveLogList[i].name = (char *)malloc(sizeof(char) * (strlen(archiveLogFolder) + 1 + strlen(filelist[i]) + 1));
			memset(archiveLogList[i].name, 0, sizeof(char) * (strlen(archiveLogFolder) + 1 + strlen(filelist[i]) + 1));
			sprintf(archiveLogList[i].name, "%s",  filelist[i]);		
			sprintf(temp,"%s%c%s",archiveLogFolder,FILE_PATH_DELIMITER, filelist[i]);
			va_lstat(NULL, temp, &archiveLogList[i].st);
			va_free(filelist[i]);
		}
		
		va_free(filelist);
	}
	else
	{
		// archive log file�� ���� ��� archive log folder�� �����ϴ��� Ȯ���ؼ� archive log folder�� ���� ��� �α׸� ����� �۾� ����� partially completed�� ó���ϵ��� �Ѵ�.
		if (va_lstat(NULL, archiveLogFolder, &fileStatus) != 0 || va_is_directory(fileStatus.mode, fileStatus.windowsAttribute) != 1)
		{
			SendJobLog(LOG_MSG_ID_ORACLE12C_BACKUP_ARCHIVE_LOG_FOLDER, archiveLogFolder, "");
			commandData.objectNumber++;
		}
	}
	
	qsort(archiveLogList, archiveLogNumber, sizeof(struct archiveLogItem), ArchiveLogComparator);
		// backup archive log file
	for (i = 0; i < archiveLogNumber; i++)
	{
		if (GetArchiveLogFile(archiveLogList[i].name) == 0)
		{
			break;
		}

	}
	
	CompareArchiveList();
	for (i = 0; i < archiveLogFileNumber; i++)
	{
		if (strlen(archiveLogFileList[i].name) > ABIO_FILE_LENGTH * 2)
		{
			PrintBackupFileResult(archiveLogFileList[i].name, ORACLE12C_ARCHIVE_LOG, -1, FILE_LENGTH_ERROR);
			
			continue;
		}
		
		if (archiveLogFileList[i].st.size > 0)
		{
			if (BackupFile(archiveLogFileList[i].name, &archiveLogFileList[i].st, ORACLE12C_ARCHIVE_LOG, NULL, NULL) < 0)
			{
				return -1;
			}
		}
		else
		{
			PrintBackupFileResult(archiveLogFileList[i].name, ORACLE12C_ARCHIVE_LOG, -1, OPEN_FILE_ERROR);
		}
	}
	
	return 0;
}


static int SwitchLogFile()
{
	if (ExecuteSql(SWITCH_LOG_FILE_QUERY, NULL) != 0)
	{
		va_remove(ABIOMASTER_CLIENT_FOLDER, queryFile);
		va_remove(ABIOMASTER_CLIENT_FOLDER, outputFile);
		
		return -1;
	}
	
	// archive log�� switch�� �Ŀ� �ٷ� folder�� �����ؼ� ����� ������
	// switch�ؼ� ���� ������ ����� ������ �ʰų� ������ ������ ����� �ȳ����� ��찡 �ִ�.
	// �׷��� ������ ������ i-node�� file system�� ������ ������Ʈ�ɶ����� ��� ���μ��� ������ �ߴ��Ѵ�.
	va_sleep(1);
	
	
	va_remove(ABIOMASTER_CLIENT_FOLDER, queryFile);
	va_remove(ABIOMASTER_CLIENT_FOLDER, outputFile);
	
	
	return 0;
}


static int ArchiveLogComparator(const void * a1, const void * a2)
{
	struct archiveLogItem * l1;
	struct archiveLogItem * l2;
	
	
	
	l1 = (struct archiveLogItem *)a1;
	l2 = (struct archiveLogItem *)a2;
	
	if (l1->st.mtime > l2->st.mtime)
	{
		return 1;
	}
	else if (l1->st.mtime < l2->st.mtime)
	{
		return -1;
	}
	else
	{
		return 0;
	}
}


static int AlterBackupControlFileBinary(char * controlFilePath)
{
	char query[1024];
	
	
	
	memset(query, 0, sizeof(query));
	sprintf(query, ALTER_CONTROL_FILE_BINARY_QUERY, controlFilePath);
	
	if (ExecuteSql(query,NULL) != 0)
	{
		va_remove(ABIOMASTER_CLIENT_FOLDER, queryFile);
		va_remove(ABIOMASTER_CLIENT_FOLDER, outputFile);
		
		return -1;
	}
	
	
	va_remove(ABIOMASTER_CLIENT_FOLDER, queryFile);
	va_remove(ABIOMASTER_CLIENT_FOLDER, outputFile);
	
	
	return 0;
}

static int GetArchiveLogFile(char* filename)
{
		
	char ** lines;
	int * linesLength;
	int lineNumber;
	
	char archiveFilename[1024];
	char Query[1024];
	
	int i;
	int j;
	
	memset(Query, 0, sizeof(Query));

	sprintf(Query,"select name||'^' from v$archived_log where recid >= (select recid from v$archived_log where name like '%%%%%s');",filename);

	if (ExecuteSql(Query,NULL) != 0)
	{
		va_remove(ABIOMASTER_CLIENT_FOLDER, queryFile);
		va_remove(ABIOMASTER_CLIENT_FOLDER, outputFile);
		
		return -1;
	}
	
	// sql query ����� �д´�.
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

					memset(archiveFilename, 0, sizeof(archiveFilename));

					// query ������� tablespace �̸��� ã�´�.
					for (j = 0; j < linesLength[i]; j++)
					{
						if (lines[i][j] == '^')
						{
							strncpy(archiveFilename, lines[i], j);
							break;
						}
					}					
					archiveList = (char **)realloc(archiveList, sizeof(char *) * (archiveFileCount + 1));
					archiveList[archiveFileCount] = (char *)malloc(sizeof(char) * (strlen(archiveFilename) + 1));
					memset(archiveList[archiveFileCount], 0, sizeof(char) * (strlen(archiveFilename) + 1));
					// table space name�� ����Ѵ�.
					strcpy(archiveList[archiveFileCount], archiveFilename);
					// table space list ������ ������Ų��.
					archiveFileCount++;
				
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

	if (archiveFileCount ==0)
	{
		return -1;
	}

	va_remove(ABIOMASTER_CLIENT_FOLDER, queryFile);
	va_remove(ABIOMASTER_CLIENT_FOLDER, outputFile);
	
	return 0;
}

static void CompareArchiveList()
{
	int i;
	int j;
	int k;
	char temp[1024];
	char archivetemp[1024];
	for(i = 0; i < archiveLogNumber; i++)
	{
		memset(temp, 0 , sizeof(temp));
		memset(archivetemp, 0 , sizeof(archivetemp));
		sprintf(temp,"%s%c%s",archiveLogFolder,FILE_PATH_DELIMITER,archiveLogList[i].name);
	
		for(j = 0; j< archiveFileCount; j++)
		{						
			for(k = 0; k < (int)strlen(temp); k++)
			{
				temp[k] =(char) towupper(temp[k]);
			}
			for(k = 0; k < (int)strlen(archiveList[j]); k++)
			{
				archivetemp[k] =(char) towupper(archiveList[j][k]);
			}
			if(strcmp(temp, archivetemp) == 0)
			{
				//strcpy(archiveLogList[i].name, archiveList[j]);
				if (archiveLogFileNumber % DEFAULT_MEMORY_FRAGMENT == 0)
				{				
					archiveLogFileList = (struct archiveLogItem *)realloc(archiveLogFileList, sizeof(struct archiveLogItem) * (archiveLogFileNumber + DEFAULT_MEMORY_FRAGMENT));
					memset(archiveLogFileList + archiveLogFileNumber, 0, sizeof(struct archiveLogItem) * DEFAULT_MEMORY_FRAGMENT);
				}

				archiveLogFileList[archiveLogFileNumber].name = (char *)malloc(sizeof(char) * (strlen(archiveList[j]) + 1));
				memset(archiveLogFileList[archiveLogFileNumber].name, 0, sizeof(char) * (strlen(archiveList[j]) + 1));
				strcpy(archiveLogFileList[archiveLogFileNumber].name, archiveList[j]);
				va_lstat(NULL, archiveLogFileList[archiveLogFileNumber].name, &archiveLogFileList[archiveLogFileNumber].st);
				archiveLogFileNumber++;
				break;
			}	
		}
	}
}



static int AlterBackupControlFileTrace(char * controlFilePath)
{
	char query[1024];
	
	
	
	memset(query, 0, sizeof(query));
	sprintf(query, ALTER_CONTROL_FILE_TRACE_QUERY, controlFilePath);
	
	if (ExecuteSql(query,NULL) != 0)
	{
		va_remove(ABIOMASTER_CLIENT_FOLDER, queryFile);
		va_remove(ABIOMASTER_CLIENT_FOLDER, outputFile);
		
		return -1;
	}
	
	
	va_remove(ABIOMASTER_CLIENT_FOLDER, queryFile);
	va_remove(ABIOMASTER_CLIENT_FOLDER, outputFile);
	
	
	return 0;
}


static int GetDatabaseOpenMode(char * databaseName)
{
	char ** lines;
	int * linesLength;
	int lineNumber;
	
	int i;
	int j;
	
	char query[1024];

	memset(query, 0, sizeof(query));
	sprintf(query, GET_DATABASE_OPEN_MODE_QUERY, databaseName);

	memset(databaseOpenMode, 0, sizeof(databaseOpenMode));
	
	if (ExecuteSql(query,NULL) != 0)
	{
		va_remove(ABIOMASTER_CLIENT_FOLDER, queryFile);
		va_remove(ABIOMASTER_CLIENT_FOLDER, outputFile);
		
		return -1;
	}
	
	// sql query ����� �д´�.
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
						strncpy(databaseOpenMode, lines[i], j);
						
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
		
		SendJobLog(LOG_MSG_ID_ORACLE_QUERY_ERROR_OPEN_QUERY_OUTPUT_FILE, "");
		
		return -1;
	}
	
	
	va_remove(ABIOMASTER_CLIENT_FOLDER, queryFile);
	va_remove(ABIOMASTER_CLIENT_FOLDER, outputFile);
	
	
	return 0;
}


static void PrintBackupTableSpaceResult(char * tableSpaceName, char* databaseName, int result, enum backupFailCause failCause)
{
	char lengthStr[NUMBER_STRING_LENGTH];
	
	
	
	if (result == 0)
	{
		SendJobLog(LOG_MSG_ID_ORACLE12C_BACKUP_TABLESPACE_SUCCESS, tableSpaceName, databaseName, "");
	}
	else
	{
		commandData.objectNumber++;
		
		if (failCause == FILE_LENGTH_ERROR)
		{
			memset(lengthStr, 0, sizeof(lengthStr));
			va_itoa(ABIO_FILE_LENGTH, lengthStr);
			SendJobLog(LOG_MSG_ID_ORACLE12C_BACKUP_TABLESPACE_FAIL_LENGTH, tableSpaceName, databaseName, lengthStr, "");
		}
		else
		{
			SendJobLog(LOG_MSG_ID_ORACLE12C_BACKUP_TABLESPACE_FAIL, tableSpaceName, databaseName,"");
		}
	}
}

static int GetArchiveLogDir()
{
	char ** lines;
	int * linesLength;
	int lineNumber;
	
	int i;
	int j;
	
	
	
	if (ExecuteSql(GET_ARCHIVE_LOG_DIR_QUERY,NULL) != 0)
	{
		va_remove(ABIOMASTER_CLIENT_FOLDER, queryFile);
		va_remove(ABIOMASTER_CLIENT_FOLDER, outputFile);
		
		return -1;
	}
	
	// sql query ����� �д´�.
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
		
		SendJobLog(LOG_MSG_ID_ORACLE_QUERY_ERROR_OPEN_QUERY_OUTPUT_FILE, "");
		
		return -1;
	}
	
	
	va_remove(ABIOMASTER_CLIENT_FOLDER, queryFile);
	va_remove(ABIOMASTER_CLIENT_FOLDER, outputFile);
	
	
	return 0;
}

// send backup file header & file to ABIO MASTER server and send backup file information to Catalog DB
static int BackupFile(char * file, struct abio_file_stat * file_stat, enum fileType filetype, char * backupTableSpaceName, char * databaseName)
{
	int r;
	#ifdef __ABIO_UNIX
		struct abio_file_stat linkStat;
	#endif
	
	
	
	if (va_is_regular(file_stat->mode, file_stat->windowsAttribute))
	{
		if ((r = FileBackup(file, file_stat, filetype, backupTableSpaceName, databaseName)) < 0)
		{
			
		}
		
		return r;
	}
	else
	{
		#ifdef __ABIO_UNIX
			// file�� symbolic link�̸� link�� ���󰡼� ������ ������ ��´�.
			if (va_is_symlink(file_stat->mode, file_stat->windowsAttribute))
			{
				if (va_stat(NULL, file, &linkStat) != 0)
				{
					return 2;
				}
			}
			else
			{
				memcpy(&linkStat, file_stat, sizeof(struct abio_file_stat));
			}
			
			if (S_ISBLK(linkStat.mode) || S_ISCHR(linkStat.mode))
			{
				#ifdef __ABIO_AIX
					return BackupRawDeviceAIX(file, file_stat, backupTableSpaceName, databaseName);
				#elif defined(__ABIO_SOLARIS) || defined(__ABIO_HP) || defined(__ABIO_LINUX)
					return RawDeviceBackup(file, file_stat, 0, backupTableSpaceName, databaseName);
				#else
					// �̿��� �ý��ۿ����� raw device file ����� �������� �ʴ´�.
					return 2;
				#endif
			}
		#elif __ABIO_WIN
			// windows������ raw device file ����� �������� �ʴ´�.
			return 2;
		#endif
	}
	
	
	return 0;
}


static int GetTableSpaceList()
{
	char ** lines;
	int * linesLength;
	int lineNumber;
	
	char databaseName[1024];
	char tableSpaceName[1024];
	char dataFileName[1024];
	
	int i;
	int j;
	int n;
	int k;
	
	
	
	if (ExecuteSql(GET_TABLE_SPACE_QUERY,NULL) != 0)
	{
		va_remove(ABIOMASTER_CLIENT_FOLDER, queryFile);
		va_remove(ABIOMASTER_CLIENT_FOLDER, outputFile);
		
		return -1;
	}
	
	// sql query ����� �д´�.
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
					
					// query ������� tablespace �̸��� ã�´�.
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

					// query ������� datafile �̸��� ã�´�.
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

					// query ����� delimiter�� ������(����) �����Ѵ�.
					if (tableSpaceName[0] == '\0')
					{
						break;
					}
					
					// query ����� datafile�� ������ ���� ���� �˻��Ѵ�.
					if (dataFileName[0] == '\0')
					{
						continue;
					}
					
					
					// find the table space in the table space list
					for (k = 0; k < tableSpaceNumber; k++)
					{
						if (!strcmp(tableSpaceList[k].tableSpace, tableSpaceName) && !strcmp(tableSpaceList[k].database, databaseName))
						{
							break;
						}
					}
					
					// table space�� table space list�� ������ table space list�� �߰��Ѵ�.
					if (k == tableSpaceNumber)
					{
						// table space list�� �ϳ� �߰��Ѵ�.
						if (k % DEFAULT_MEMORY_FRAGMENT == 0)
						{
							tableSpaceList = (struct tableSpaceItem *)realloc(tableSpaceList, sizeof(struct tableSpaceItem) * (k + DEFAULT_MEMORY_FRAGMENT));
							memset(tableSpaceList + k, 0, sizeof(struct tableSpaceItem) * DEFAULT_MEMORY_FRAGMENT);
						}
						
						// table space name�� ����Ѵ�.
						strcpy(tableSpaceList[k].database, databaseName);
						strcpy(tableSpaceList[k].tableSpace, tableSpaceName);
						
						// table space list ������ ������Ų��.
						tableSpaceNumber++;
					}
					
					// table space�� data file�� �߰��Ѵ�.
					if (tableSpaceList[k].fileNumber % DEFAULT_MEMORY_FRAGMENT == 0)
					{
						tableSpaceList[k].dataFile = (char **)realloc(tableSpaceList[k].dataFile, sizeof(char *) * (tableSpaceList[k].fileNumber + DEFAULT_MEMORY_FRAGMENT));
						memset(tableSpaceList[k].dataFile + tableSpaceList[k].fileNumber, 0, sizeof(char *) * DEFAULT_MEMORY_FRAGMENT);
					}
					tableSpaceList[k].dataFile[tableSpaceList[k].fileNumber] = (char *)malloc(sizeof(char) * (strlen(dataFileName) + 1));
					memset(tableSpaceList[k].dataFile[tableSpaceList[k].fileNumber], 0, sizeof(char) * (strlen(dataFileName) + 1));
					strcpy(tableSpaceList[k].dataFile[tableSpaceList[k].fileNumber], dataFileName);
					
					// table space�� data file ������ ������Ų��.
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
		
		SendJobLog(LOG_MSG_ID_ORACLE_QUERY_ERROR_OPEN_QUERY_OUTPUT_FILE, "");
		
		return -1;
	}
	
	
	va_remove(ABIOMASTER_CLIENT_FOLDER, queryFile);
	va_remove(ABIOMASTER_CLIENT_FOLDER, outputFile);
	
	
	return 0;
}


int CheckNode(struct ckBackup recvCommandData)
{
		int result=0;
	
		va_sock_t serverSock;

		if (CheckNodeAlive(serverIP,serverPort) < 0)
		{
			result=0;
		}
		else
		{
			result=1;
		}

		recvCommandData.success=result;
		if ((serverSock = va_connect(masterIP, recvCommandData.catalogPort, 1)) != -1)
		{
			va_send(serverSock, &recvCommandData, sizeof(struct ckBackup), 0, DATA_TYPE_CK_BACKUP);
			va_close_socket(serverSock, ABIO_SOCKET_CLOSE_CLIENT);
		}

		return result;
}
int CheckNodeAlive(char * ip, char * port)
{
	va_sock_t sock;	
	
	if ((sock = va_connect(ip, port, 0)) != -1)
	{
		va_close_socket(sock, ABIO_SOCKET_CLOSE_CLIENT);
	}
	else
	{
		return -1;
	}	
	
	return 0;
}

static void WaitForServerError()
{
	int count;
	
	
	
	// initialize variables
	count = 0;
	
	// server�κ��� ������ ���� ������ ����Ѵ�.
	while (recvErrorFromServer == 0 && serverConnected == 1)
	{
		va_sleep(1);
		
		count++;
		
		// client���� ������ �߻��� �Ŀ� ���� �ð����� ������ ���� ������ �����Ѵ�.
		if (count == TIME_OUT_JOB_CONNECTION)
		{
			break;
		}
	}
}

static va_thread_return_t InnerMessageTransfer(void * arg)
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
				if (!strcmp(recvCommand.subCommand, "ERROR"))
				{
					ServerError(&recvCommandData);
				}
				else if (!strcmp(recvCommand.subCommand, "STOP_JOB"))
				{
					if(1==CheckNode(recvCommandData))
					{
						backupJobState=2;
						errorLocation=ABIO_STOP;
						SendCommand(1, 0, 1, MODULE_NAME_SLAVE_BACKUP, "STOP_JOB", MODULE_NAME_CLIENT_BACKUP, clientIP, clientPort, clientNodename, serverIP, serverPort, serverNodename);
					}
				}
				else if (!strcmp(recvCommand.subCommand, "PAUSE_JOB"))
				{
					if(1==CheckNode(recvCommandData))
					{
						backupJobState=1;					
						SendCommand(1, 0, 1, MODULE_NAME_SLAVE_BACKUP, "PAUSE_JOB", MODULE_NAME_CLIENT_BACKUP, clientIP, clientPort, clientNodename, serverIP, serverPort, serverNodename);
					}					
				}
				else if (!strcmp(recvCommand.subCommand, "RESUME_JOB"))
				{
					if(1==CheckNode(recvCommandData))
					{
						SendCommand(1, 0, 1, MODULE_NAME_SLAVE_BACKUP, "RESUME_JOB", MODULE_NAME_CLIENT_BACKUP, clientIP, clientPort, clientNodename, serverIP, serverPort, serverNodename);
					}
				}
				else if (!strcmp(recvCommand.subCommand, "RESUME_JOB_OK"))
				{
					backupJobState=0;
					errorLocation = ABIO_CLIENT;
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

static va_thread_return_t KeepSocketAlive(void * arg)
{
	char tmpbuf[8];
	
	
	
	va_sleep(1);
	
	while (jobComplete == 0 && dataSock != -1)
	{
		// backup server�κ��� ���� ���ڸ� �޴´�.
		if (va_recv(dataSock, tmpbuf, 1, 0, DATA_TYPE_NOT_CHANGE) == 1)
		{
			// backup server�κ��� END_OF_BACKUP file header�� �޾Ҵٴ� �޼����� �����ϸ� ��� ���¸� data socket�� close�� �� �ֵ��� ����� thread�� �����Ѵ�.
			if (tmpbuf[0] == 'E')
			{
				break;
			}
		}
		else
		{
			if(backupJobState==0)
			{
				break;
			}
		}
	}
	
	va_close_socket(dataSock, ABIO_SOCKET_CLOSE_CLIENT);
	dataSock = -1;
	
	
	#ifdef __ABIO_UNIX
		return NULL;
	#else
		_endthreadex(0);
		return 0;
	#endif
}


// backup file sender
static va_thread_return_t SendBackupFile(void * arg)
{
	int senderIndex;
	
	
	
	va_sleep(1);
	
	senderIndex = 0;
	
	while (jobComplete == 0)
	{
		// ���� ����ϰ� �ִ� backup buffer�� �����Ͱ� ������ ������ �� ���������� ��ٸ���.
		if (va_grab_semaphore(semid[0]) < 0)
		{
			va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_BACKUP, FUNCTION_BACKUP_SendBackupFile, ERROR_RESOURCE_SEMAPHORE_REMOVED, &commandData);
			Error();
			
			break;
		}
		
		if (jobComplete == 1)
		{
			break;
		}
		
		// ���� ����ϰ� �ִ� backupset buffer�� ������ �����ϱ� ����������, ���� backup buffer�� �����͸� ä���� �˷��ش�.
		if (va_release_semaphore(semid[1]) < 0)
		{
			va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_BACKUP, FUNCTION_BACKUP_SendBackupFile, ERROR_RESOURCE_SEMAPHORE_REMOVED, &commandData);
			Error();
			
			break;
		}
		
		// ���� ����ϰ� �ִ� backup buffer�� ������ �����Ѵ�.
		if (backupBufferSize[senderIndex] > 0)
		{
			// maximum bandwidth�� ������ ��� ������ bandwidth�� ���� �ʵ��� �ϱ� ���� ��Ʈ��ũ�� �����͸� ������ ���� bandwidth ��뷮�� �˻��Ѵ�.
			// �� �˻縦 ��Ʈ��ũ�� �����͸� ���� �Ŀ� �ϸ� ������ bandwidth�� �ѱ� �׷����� ���� ������ ȿ���� �ְ�, 
			// �� �˻縦 ��Ʈ��ũ�� �����͸� ������ ���� �ϸ� ������ bandwidth�� �ѱ��� �ʵ��� �׷����� ������ ȿ���� �ִ�.
			if (maxOutboundBandwidth > 0)
			{
				va_limit_network_bandwidth(maxOutboundBandwidth * 1024, backupBufferSize[senderIndex]);
			}
			
			// backup buffer�� ������ �����Ѵ�.
			if (va_send(dataSock, backupBuffer[senderIndex], backupBufferSize[senderIndex], 0, DATA_TYPE_NOT_CHANGE) < 0)
			{
				if (jobComplete == 0)
				{
					va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_BACKUP, FUNCTION_BACKUP_SendBackupFile, ERROR_NETWORK_DATA_SOCKET_DOWN, &commandData);
					Error();
				}
			}
			
			// backup buffer�� �ִ� �����͸� �����.
			backupBufferSize[senderIndex] = 0;
			memset(backupBuffer[senderIndex], 0, readDiskBufferSize);
		}
		
		// backup buffer�� index�� ���� buffer�� �����Ѵ�.
		senderIndex++;
		senderIndex = senderIndex % CLIENT_BACKUP_BUFFER_NUMBER;
	}
	
	// backup server���� ������ �����Ѵ�.
	va_close_socket(dataSock, ABIO_SOCKET_CLOSE_CLIENT);
	dataSock = -1;
	
	
	#ifdef __ABIO_UNIX
		return NULL;
	#else
		_endthreadex(0);
		return 0;
	#endif
}


static void ServerError(struct ckBackup * recvCommandData)
{
	if (jobComplete == 0)
	{
		// server���� �߻��� ������ ����Ѵ�.
		errorLocation = ABIO_BACKUP_SERVER;
		memcpy(commandData.errorCode, recvCommandData->errorCode, sizeof(struct errorCode) * ERROR_CODE_NUMBER);
		
		// �۾��� �����Ѵ�.
		Error();
	}
	
	// server�κ��� ������ �޾����� ����Ѵ�.
	recvErrorFromServer = 1;
}

static int GetOracleLogMode()
{
	char ** lines;
	int * linesLength;
	int lineNumber;
	
	int i;
	int j;
	
	
	
	if (ExecuteSql(GET_ARCHIVE_LOG_MODE_QUERY,NULL) != 0)
	{
		va_remove(ABIOMASTER_CLIENT_FOLDER, queryFile);
		va_remove(ABIOMASTER_CLIENT_FOLDER, outputFile);
		
		return -1;
	}
	
	// sql query ����� �д´�.
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
						strncpy(archiveLogMode, lines[i], j);
						
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
		
		SendJobLog(LOG_MSG_ID_ORACLE_QUERY_ERROR_OPEN_QUERY_OUTPUT_FILE, "");
		
		return -1;
	}
	
	
	va_remove(ABIOMASTER_CLIENT_FOLDER, queryFile);
	va_remove(ABIOMASTER_CLIENT_FOLDER, outputFile);
	
	
	return 0;
}

#ifdef __ABIO_AIX
	static int BackupRawDeviceAIX(char * file, struct abio_file_stat * file_stat, char * backupTableSpaceName, char * databaseName)
	{
		struct lv_id lid;
		struct queryvgs * vgs;
		struct queryvg * vg;
		struct querylv * lv;
		
		static struct querylv * lvList;
		static int lvNumber;
		
		char logicalVolumeFile[MAX_PATH_LENGTH];
		__uint64 fsSize;
		
		char linkPath[MAX_PATH_LENGTH];
		struct abio_file_stat linkStat;
		
		int i;
		int j;
		int k;
		
		int r;
		
		
		
		fsSize = 0;
		memset(logicalVolumeFile, 0, sizeof(logicalVolumeFile));
		memset(linkPath, 0, sizeof(linkPath));
		
		// get an information of logical volume
		if (lvNumber == 0)
		{
			// get number of online volume group
			if (lvm_queryvgs(&vgs, 0) == 0)
			{
				// get volume group list
				for (i = 0; i < vgs->num_vgs; i++)
				{
					if (lvm_queryvg(&vgs->vgs[i].vg_id, &vg, NULL) == 0)
					{
						lvNumber += vg->num_lvs;
					}
				}
			}
			
			if (lvNumber > 0)
			{
				// get logical volume list
				if (lvm_queryvgs(&vgs, 0) == 0)
				{
					lvList = (struct querylv *)malloc(sizeof(struct querylv) * lvNumber);
					memset(lvList, 0, sizeof(struct querylv) * lvNumber);
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
				}
			}
		}
		
		// get a raw device file size
		if (lvNumber > 0)
		{
			if (va_is_symlink(file_stat->mode, file_stat->windowsAttribute))
			{
				// link ������ status�� �����´�.
				if (va_stat(NULL, file, &linkStat) != 0)
				{
					return 2;
				}
				
				// link ������ ��θ� �о fstab�� ��ϵǾ� �ִ� ��θ� ã�Ƴ���.
				if (va_read_symlink(file, linkPath, sizeof(linkPath)) == 0)
				{
					if (S_ISBLK(linkStat.mode))
					{
						strcpy(logicalVolumeFile, linkPath);
					}
					else
					{
						sprintf(logicalVolumeFile, "/dev/%s", linkPath + strlen("/dev/r"));
					}
				}
				else
				{
					return 2;
				}
			}
			else
			{
				if (S_ISBLK(file_stat->mode))
				{
					strcpy(logicalVolumeFile, file);
				}
				else
				{
					sprintf(logicalVolumeFile, "/dev/%s", file + strlen("/dev/r"));
				}
			}
			
			
			// find a raw device file in logical volume list
			for (i = 0; i < lvNumber; i++)
			{
				if (!strcmp(lvList[i].lvname, logicalVolumeFile + strlen("/dev/")))
				{
					// get a volume size
					fsSize = 1;
					for (j = 0; j < lvList[i].ppsize; j++)
					{
						fsSize *= 2;
					}
					fsSize *= lvList[i].currentsize;
					
					break;
				}
			}
		}
		if ((r = RawDeviceBackup(file, file_stat, fsSize, backupTableSpaceName)) < 0)
		{
			
		}
		
		return r;

		/*
		if (fsSize > 0)
		{
			if ((r = RawDeviceBackup(file, file_stat, fsSize, backupTableSpaceName)) < 0)
			{
				
			}
			
			return r;
		}
		else
		{
			va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_BACKUP, FUNCTION_BACKUP_BackupRawDeviceAIX, ERROR_RAW_DEVICE_GET_LOGICAL_VOLUME_LIST, &commandData);
			
			return -1;
		}
		*/
	}
#endif
	
#if defined(__ABIO_AIX) || defined(__ABIO_SOLARIS) || defined(__ABIO_HP) || defined(__ABIO_LINUX)
	static int RawDeviceBackup(char * file, struct abio_file_stat * file_stat, __uint64 fsSize, char * backupTableSpaceName, char * databaseName)
	{
		struct fileHeader fileHeader;
		int backupResult;
		
		char filesystemPath[MAX_PATH_LENGTH];
		
		char linkPath[MAX_PATH_LENGTH];
		struct abio_file_stat linkStat;
		
		int i;
		
		
		
		backupResult = 0;
		memset(filesystemPath, 0, sizeof(filesystemPath));
		memset(linkPath, 0, sizeof(linkPath));
		
		// make a file header
		memset(&fileHeader, 0, sizeof(struct fileHeader));
		MakeFileHeader(file, file_stat, ORACLE12C_RAW_DEVICE_DATA_FILE, databaseName,backupTableSpaceName, &fileHeader);
		
		
		#ifdef __ABIO_AIX
			// link ������ ��� link�� ���󰡼� character special flie ��θ� ���Ѵ�.
			if (va_is_symlink(file_stat->mode, file_stat->windowsAttribute))
			{
				// link ������ status�� �����´�.
				if (va_stat(NULL, file, &linkStat) != 0)
				{
					return 2;
				}
				
				// link ������ ��θ� �о file system ��θ� ã�Ƴ���.
				if (va_read_symlink(file, linkPath, sizeof(linkPath)) == 0)
				{
					// block special file�̸� character special file ��θ� ���Ѵ�.
					if (S_ISBLK(linkStat.mode))
					{
						for (i = strlen(linkPath) - 1; i > -1; i--)
						{
							if (linkPath[i] == FILE_PATH_DELIMITER)
							{
								break;
							}
						}
						
						strncpy(filesystemPath, linkPath, i+1);
						strcat(filesystemPath, "r");
						strcat(filesystemPath, linkPath+i+1);
					}
					// character special file�̸� �״�� ����Ѵ�.
					else
					{
						strcpy(filesystemPath, linkPath);
					}
				}
				else
				{
					return 2;
				}
			}
			else
			{
				// block special file�̸� character special file ��θ� ���Ѵ�.
				if (S_ISBLK(file_stat->mode))
				{
					for (i = strlen(file) - 1; i > -1; i--)
					{
						if (file[i] == FILE_PATH_DELIMITER)
						{
							break;
						}
					}
					
					strncpy(filesystemPath, file, i+1);
					strcat(filesystemPath, "r");
					strcat(filesystemPath, file+i+1);
				}
				// character special file�̸� �״�� ����Ѵ�.
				else
				{
					strcpy(filesystemPath, file);
				}
			}
			
			
			if (commandData.compress == 0)
			{
				backupResult = SendFileContentsRawDeviceAIX(filesystemPath, &fileHeader, fsSize);
			}
			else
			{
				backupResult = SendFileContentsRawDeviceAIXCompress(filesystemPath, &fileHeader, fsSize);
			}
		#else
			if (commandData.compress == 0)
			{
				backupResult = SendFileContents(file, &fileHeader);
			}
			else
			{
				backupResult = SendFileContentsCompress(file, &fileHeader);
			}
		#endif
		
		
		return backupResult;
	}
#endif

static int FileBackup(char * file, struct abio_file_stat * file_stat, enum fileType filetype, char * backupTableSpaceName,char * databaseName)
{
	struct fileHeader fileHeader;
	int backupResult;
	
	
	
	backupResult = 0;
	
	// make a file header
	memset(&fileHeader, 0, sizeof(struct fileHeader));
	MakeFileHeader(file, file_stat, filetype, databaseName, backupTableSpaceName, &fileHeader);
	
	
	// send the file header and file contents to the backup server
	if (fileHeader.st.size > 0)
	{
		if (commandData.compress == 0)
		{
			if ((backupResult = SendFileContents(file, &fileHeader)) < 0)
			{
				
			}
		}
		else
		{
			if ((backupResult = SendFileContentsCompress(file, &fileHeader)) < 0)
			{
				
			}
		}
	}
	
	
	return backupResult;
}

static void DeleteArchiveLog()
{
	int i;
	
	// ��� �Ŀ� archive log�� �����ϴ� �ɼ��� �����Ǿ� ������, �ֱ� 3���� archive log�� ����� �����Ѵ�.
	if (deleteArchiveLog != 0)
	{
		for (i = 0; i < archiveLogFileNumber - ramainArchiveLogCount; i++)
		{
			//va_remove(NULL, archiveLogFileList[i].name);
			if (va_remove(NULL, archiveLogFileList[i].name) < 0)
			{
				PrintBackupFileResult(archiveLogFileList[i].name,DELETE_ARCHIVE_LOG, -1, (enum backupFailCause)0);
			}
			else
			{
				PrintBackupFileResult(archiveLogFileList[i].name,DELETE_ARCHIVE_LOG, 0, (enum backupFailCause)0);
			}
		}
	}
}

static void MakeFileHeader(char * file, struct abio_file_stat * file_stat, enum fileType filetype, char * databaseName, char * backupTableSpaceName, struct fileHeader * returnFileHeader)
{
	struct fileHeader fileHeader;
	
	if(0 < backupJobState)
	{
		while(backupJobState)
		{
			va_sleep(2);
			
			 if(backupJobState==2)
			{
				SendEndOfBackup();
				DeleteArchiveLog();
				Error();
				return;
			}
		}
	}

	memset(&fileHeader, 0, sizeof(struct fileHeader));
	
	strcpy(fileHeader.abio_check_code, ABIO_CHECK_CODE);
	strcpy(fileHeader.backupset, commandData.backupset);
	fileHeader.version = CURRENT_VERSION_INTERNAL;
	
	// file name ����
	strcpy(((struct oracleFilePath *)fileHeader.filepath)->name, file);
	#ifdef __ABIO_WIN
		va_backslash_to_slash(((struct oracleFilePath *)fileHeader.filepath)->name);
	#endif
	
	// file status�� �����Ѵ�.
	MakeAbioStat(file, file_stat, &fileHeader.st);
	
	// file type�� �����Ѵ�.
	fileHeader.filetype = filetype;
	fileHeader.st.filetype = filetype;
	fileHeader.dbBackupMethod = commandData.jobMethod;
	
	// if a backup file is a data file of table space, write a table space name in file header
	if (filetype == ORACLE12C_REGULAR_DATA_FILE || filetype == ORACLE12C_RAW_DEVICE_DATA_FILE)
	{
		char temp[256];
		memset(temp, 0 , sizeof(temp));
		sprintf(temp,"%s_%s",databaseName, backupTableSpaceName);
		strcpy(((struct oracleFilePath *)fileHeader.filepath)->tableSpaceName, temp);

	}
	
	fileHeader.compress = commandData.compress;
	
	
	memcpy(returnFileHeader, &fileHeader, sizeof(struct fileHeader));
}


// file status�� ABIO MASTER file status�� ��ȯ
static void MakeAbioStat(char * file, struct abio_file_stat * file_stat, struct abio_stat * ast)
{
	// set basic information
	ast->version	= CURRENT_VERSION_INTERNAL;
	ast->mode		= file_stat->mode;
	ast->mtime 		= file_stat->mtime;
	ast->ctime   	= file_stat->ctime;
	ast->size 		= file_stat->size;
	ast->uid	 	= file_stat->uid;
	ast->gid 		= file_stat->gid;
	ast->backupTime = commandData.startTime;
	va_make_permission(file_stat->mode, file_stat->windowsAttribute, ast->permission);
	
	#ifdef __ABIO_WIN
		ast->windowsAttribute = file_stat->windowsAttribute;
	#endif
}


static void PrintBackupFileResult(char * file, enum fileType filetype, int result, enum backupFailCause failCause)
{
	char lengthStr[NUMBER_STRING_LENGTH];
	
	
	
	#ifdef __ABIO_WIN
		va_backslash_to_slash(file);
	#endif
	
	if (result == 0)
	{
		if (filetype == ORACLE12C_REGULAR_DATA_FILE || filetype == ORACLE12C_RAW_DEVICE_DATA_FILE)
		{
			SendJobLog(LOG_MSG_ID_ORACLE12C_BACKUP_DATA_FILE_SUCCESS, file, "");
		}
		else if (filetype == ORACLE12C_ARCHIVE_LOG)
		{
			SendJobLog(LOG_MSG_ID_ORACLE12C_BACKUP_ARCHIVE_LOG_SUCCESS, file, "");
		}
		else if (filetype == ORACLE12C_CONTROL_FILE)
		{
			SendJobLog(LOG_MSG_ID_ORACLE12C_BACKUP_CONTROL_FILE_SUCCESS, file, "");
		}
	}
	else
	{
		commandData.objectNumber++;
		
		if (filetype == ORACLE12C_REGULAR_DATA_FILE || filetype == ORACLE12C_RAW_DEVICE_DATA_FILE)
		{
			if (failCause == FILE_LENGTH_ERROR)
			{
				memset(lengthStr, 0, sizeof(lengthStr));
				va_itoa(ABIO_FILE_LENGTH, lengthStr);
				SendJobLog(LOG_MSG_ID_ORACLE12C_BACKUP_DATA_FILE_FAIL_LENGTH, file, lengthStr, "");
			}
			else if (failCause == OPEN_FILE_ERROR)
			{
				SendJobLog(LOG_MSG_ID_ORACLE12C_BACKUP_DATA_FILE_FAIL_OPEN, file, "");
			}
			else if (failCause == READ_FILE_ERROR)
			{
				SendJobLog(LOG_MSG_ID_ORACLE12C_BACKUP_DATA_FILE_FAIL_READ, file, "");
			}
		}
		else if (filetype == ORACLE12C_ARCHIVE_LOG)
		{
			if (failCause == FILE_LENGTH_ERROR)
			{
				memset(lengthStr, 0, sizeof(lengthStr));
				va_itoa(ABIO_FILE_LENGTH * 2, lengthStr);
				SendJobLog(LOG_MSG_ID_ORACLE12C_BACKUP_ARCHIVE_LOG_FAIL_LENGTH, file, lengthStr, "");
			}
			else if (failCause == OPEN_FILE_ERROR)
			{
				SendJobLog(LOG_MSG_ID_ORACLE12C_BACKUP_ARCHIVE_LOG_FAIL_OPEN, file, "");
			}
			else if (failCause == READ_FILE_ERROR)
			{
				SendJobLog(LOG_MSG_ID_ORACLE12C_BACKUP_ARCHIVE_LOG_FAIL_READ, file, "");
			}
		}
		else if (filetype == ORACLE12C_CONTROL_FILE)
		{
			if (failCause == FILE_LENGTH_ERROR)
			{
				memset(lengthStr, 0, sizeof(lengthStr));
				va_itoa(ABIO_FILE_LENGTH * 2, lengthStr);
				SendJobLog(LOG_MSG_ID_ORACLE12C_BACKUP_CONTROL_FILE_FAIL_LENGTH, file, lengthStr, "");
			}
			else if (failCause == OPEN_FILE_ERROR)
			{
				SendJobLog(LOG_MSG_ID_ORACLE12C_BACKUP_CONTROL_FILE_FAIL_OPEN, file, "");
			}
			else if (failCause == READ_FILE_ERROR)
			{
				SendJobLog(LOG_MSG_ID_ORACLE12C_BACKUP_CONTROL_FILE_FAIL_READ, file, "");
			}
		}
	}
	
	#ifdef __ABIO_WIN
		va_slash_to_backslash(file);
	#endif
}
static int SendFileContentsCompress(char * file, struct fileHeader * fileHeader)
{
	va_fd_t fdBackupFile;
	
	int compressedDiskBufferSize;
	
	#ifdef __ABIO_LINUX
		unsigned char * unalignedDataBuf;
	#endif
	unsigned char * dataBuf;
	unsigned zlib_long_t dataBufSize;
	int readSize;
	
	unsigned char * compressedBuf;
	unsigned zlib_long_t compressedSize;
	
	unsigned char * sendBuf;
	int sendSize;
	
	int r;
	
	
	
	compressedDiskBufferSize = readDiskBufferSize;
	fileHeader->st.compressedDiskBufferSize = compressedDiskBufferSize;
	
	#ifdef __ABIO_LINUX
		unalignedDataBuf = (unsigned char *)malloc(sizeof(char) * (compressedDiskBufferSize * 2));
		memset(unalignedDataBuf, 0, sizeof(char) * (compressedDiskBufferSize * 2));
		
		dataBuf = va_ptr_align(unalignedDataBuf, compressedDiskBufferSize);
	#else
		dataBuf = (unsigned char *)malloc(sizeof(char) * compressedDiskBufferSize);
		memset(dataBuf, 0, sizeof(char) * compressedDiskBufferSize);
	#endif
	
	compressedBuf = (unsigned char *)malloc(sizeof(char) * (compressedDiskBufferSize * 2));
	memset(compressedBuf, 0, sizeof(char) * (compressedDiskBufferSize * 2));
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 1. open the backup file to send the file to VIRBAK ABIO server
	
	if ((fdBackupFile = va_open_backup_file(NULL, file)) != (va_fd_t)-1)
	{
		//////////////////////////////////////////////////////////////////////////////////////////////////
		// 2. send the head file header
		
		if (SendFileHeader(fileHeader, FILE_HEADER_HEAD) < 0)
		{
			va_close(fdBackupFile);
			
			#ifdef __ABIO_LINUX
				va_free(unalignedDataBuf);
			#else
				va_free(dataBuf);
			#endif
			va_free(compressedBuf);
			
			return -1;
		}
		
		
		//////////////////////////////////////////////////////////////////////////////////////////////////
		// 3. send the file contents
		
		fileHeader->st.size = 0;
		
		while ((readSize = va_read(fdBackupFile, dataBuf, compressedDiskBufferSize, DATA_TYPE_NOT_CHANGE)) > 0)
		{
			// ������ ũ�⸦ ����Ѵ�.
			fileHeader->st.size += readSize;
			
			// �о���� �����͸� �����Ѵ�.
			compressedSize = compressedDiskBufferSize * 2;
			dataBufSize = (unsigned zlib_long_t)readSize;
			r = compress2(compressedBuf, &compressedSize, dataBuf, dataBufSize, commandData.compress);
			
			// ���࿡ �������� ��� ����� ����� ����ϰ�, ����� �����͸� �����Ѵ�.
			if (r == 0)
			{
				// encryption
				if (fileHeader->encryption == 1)
				{
					aes_encrypt(compressedBuf,compressedBuf,(int)compressedSize);
				}
				sendBuf = compressedBuf;
				sendSize = (int)compressedSize;
			}
			// ���࿡ �������� ��� �о���� ���� �����͸� �״�� �����Ѵ�.
			else
			{
				// encryption
				if (fileHeader->encryption == 1)
				{
					aes_encrypt(compressedBuf,compressedBuf,(int)compressedSize);
				}
				sendBuf = dataBuf;
				sendSize = readSize;
			}
			
			// ����� ������ ���� ũ�⸦ ����Ѵ�.
			fileHeader->st.compressedSize += sizeof(int) + sizeof(int) + sendSize;
			
			// ���� ���ο� ����� �������� ũ��� ����� �����͸� �����Ѵ�.
			r = htonl(r);					// host to network
			sendSize = htonl(sendSize);		// host to network
			if (SendBackupData(&r, sizeof(int)) < 0 || SendBackupData(&sendSize, sizeof(int)) < 0 || SendBackupData(sendBuf, htonl(sendSize)) < 0)
			{
				va_close(fdBackupFile);
				
				#ifdef __ABIO_LINUX
					va_free(unalignedDataBuf);
				#else
					va_free(dataBuf);
				#endif
				va_free(compressedBuf);
				
				return -1;
			}
		}
		
		if (readSize < 0)
		{
			va_close(fdBackupFile);
			
			PrintBackupFileResult(file, fileHeader->filetype, -1, READ_FILE_ERROR);
			
			#ifdef __ABIO_LINUX
				va_free(unalignedDataBuf);
			#else
				va_free(dataBuf);
			#endif
			va_free(compressedBuf);
			
			return 2;
		}
		
		
		//////////////////////////////////////////////////////////////////////////////////////////////////
		// 4. close the file
		
		va_close(fdBackupFile);
		
		
		//////////////////////////////////////////////////////////////////////////////////////////////////
		// 5. ����� ������ ���� ���� �����Ѵ�.
		
		r = 1;
		sendSize = 0;
		
		// ����� ������ ���� ũ�⸦ ����Ѵ�.
		fileHeader->st.compressedSize += sizeof(int) + sizeof(int);
		
		// ���� ���ο� ����� �������� ũ�⸦ �����Ѵ�.
		r = htonl(r);					// host to network
		sendSize = htonl(sendSize);		// host to network
		if (SendBackupData(&r, sizeof(int)) < 0 || SendBackupData(&sendSize, sizeof(int)) < 0)
		{
			#ifdef __ABIO_LINUX
				va_free(unalignedDataBuf);
			#else
				va_free(dataBuf);
			#endif
			va_free(compressedBuf);
			
			return -1;
		}
		
		
		//////////////////////////////////////////////////////////////////////////////////////////////////
		// 6. send the tail file header
		
		if (SendFileHeader(fileHeader, FILE_HEADER_TAIL) < 0)
		{
			#ifdef __ABIO_LINUX
				va_free(unalignedDataBuf);
			#else
				va_free(dataBuf);
			#endif
			va_free(compressedBuf);
			
			return -1;
		}
	}
	else
	{
		PrintBackupFileResult(file, fileHeader->filetype, -1, OPEN_FILE_ERROR);
		
		#ifdef __ABIO_LINUX
			va_free(unalignedDataBuf);
		#else
			va_free(dataBuf);
		#endif
		va_free(compressedBuf);
		
		return 2;
	}
	
	
	#ifdef __ABIO_LINUX
		va_free(unalignedDataBuf);
	#else
		va_free(dataBuf);
	#endif
	va_free(compressedBuf);
	
	PrintBackupFileResult(file, fileHeader->filetype, 0, (enum backupFailCause)0);
	
	return 0;
}
static int SendFileHeader(struct fileHeader * fileHeader, enum fileHeaderType headerType)
{
	int r;
	
	
	
	// backup buffer�� ���� ����� 1K�� ����� ��ġ�� �����ؾ� �Ѵ�.
	// �׷��� backup buffer�� ũ�Ⱑ 1K�� ����� �ƴ� �� backup buffer�� ũ�⸦ 1K�� ����� �µ��� �������ش�.
	if (backupBufferSize[bufferIndex] % DSIZ != 0)
	{
		backupBufferSize[bufferIndex] += DSIZ - backupBufferSize[bufferIndex] % DSIZ;
	}
	
	if (SendBackupBuffer(0) < 0)
	{
		return -1;
	}
	
	// encryption
	if (commandData.encryption == 1)
	{
		fileHeader->encryption = 1;
	}
	
	// send the file header
	fileHeader->headerType = headerType;
	
	// copy a file header to backup buffer
	va_change_byte_order_fileHeader(fileHeader);		// host to network
	memcpy(backupBuffer[bufferIndex] + backupBufferSize[bufferIndex], fileHeader, sizeof(struct fileHeader));
	backupBufferSize[bufferIndex] += DSIZ;
	va_change_byte_order_fileHeader(fileHeader);		// network to host
	
	// head type�� file header�� �׻� backup buffer�� ù �κп� ������ �ϱ� ����
	// file header�� tail�� ��� backup buffer�� ������ dummy data�� ������.
	if (headerType == FILE_HEADER_TAIL)
	{
		backupBufferSize[bufferIndex] = readDiskBufferSize;
	}
	
	
	if ((r = SendBackupBuffer(0)) < 0)
	{
		
	}
	
	return r;
}

static int SendBackupData(void * dataBuffer, unsigned int dataSize)
{
	int totalSendSize;
	int sendSize;
	
	
	
	totalSendSize = 0;
	
	while (totalSendSize < (int)dataSize)
	{
		// �����ϰ� ���� �������� ũ�Ⱑ ���� ����ϰ� �ִ� backup buffer�� ���� ũ�⺸�� ������ �����ϰ� ���� �����͸�ŭ �����Ѵ�.
		if ((int)dataSize - totalSendSize <= (int)(readDiskBufferSize - backupBufferSize[bufferIndex]))
		{
			sendSize = dataSize - totalSendSize;
		}
		// �����ϰ� ���� �������� ũ�Ⱑ ���� ����ϰ� �ִ� backup buffer�� ���� ũ�⺸�� ũ�� ���� ����ϰ� �ִ� backup buffer�� ���� ũ�⸸ŭ�� �����Ѵ�.
		else
		{
			sendSize = readDiskBufferSize - backupBufferSize[bufferIndex];
		}
		
		// ���� ����ϰ� �ִ� backup buffer�� �����͸� �����Ѵ�.
		memcpy(backupBuffer[bufferIndex] + backupBufferSize[bufferIndex], (char *)dataBuffer + totalSendSize, sendSize);
		
		// ������ �����Ϳ��� ���� ����ϰ� �ִ� backup buffer�� �Ǿ ���� �������� ũ�⸦ ����Ѵ�.
		totalSendSize += sendSize;
		
		// ���� ����ϰ� �ִ� backup buffer�� ũ�⸦ ������ ũ�⸸ŭ ������Ų��.
		backupBufferSize[bufferIndex] += sendSize;
		
		// backup buffer�� �����Ѵ�.
		if (SendBackupBuffer(0) < 0)
		{
			return -1;
		}
	}
	
	
	return totalSendSize;
}


static int SendBackupBuffer(int flushBackupBuffer)
{
	int isSendBackupBuffer;
	
	
	
	isSendBackupBuffer = 0;
	
	if (flushBackupBuffer == 0)
	{
		// backup buffer�� �� ���� buffer�� �����ϰ� index ����
		if (backupBufferSize[bufferIndex] == readDiskBufferSize)
		{
			isSendBackupBuffer = 1;
		}
	}
	else
	{
		// backup buffer�� ������ ����� ��û�� ���� backup buffer�� ������ dummy data�� ������.
		backupBufferSize[bufferIndex] = readDiskBufferSize;
		
		isSendBackupBuffer = 1;
	}
	
	if (isSendBackupBuffer == 1)
	{
		// sender���� ���� ����ϰ� �ִ� backup buffer�� �����϶�� �˷��ش�.
		if (va_release_semaphore(semid[0]) < 0)
		{
			va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_BACKUP, FUNCTION_BACKUP_SendBackupBuffer, ERROR_RESOURCE_SEMAPHORE_REMOVED, &commandData);
			
			return -1;
		}
		
		// sender�� ���� ����ϰ� �ִ� backup buffer�� ������ �����ϱ� �����ؼ� ���� backup buffer�� �����͸� ä�� �� �ְ� �ɶ����� ��ٸ���.
		if (va_grab_semaphore(semid[1]) < 0)
		{
			va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_BACKUP, FUNCTION_BACKUP_SendBackupBuffer, ERROR_RESOURCE_SEMAPHORE_REMOVED, &commandData);
			
			return -1;
		}
		
		// backup buffer�� index�� ���� buffer�� �����Ѵ�.
		bufferIndex++;
		bufferIndex = bufferIndex % CLIENT_BACKUP_BUFFER_NUMBER;
		
		if (jobComplete == 1)
		{
			return -1;
		}
	}
	
	
	// backup buffer�� ������ ����� ��û�� ���� ������ �����϶�� �˷��� ������ ������ ���������� ����Ѵ�.
	if (flushBackupBuffer == 1)
	{
		// sender���� ���� ����ϰ� �ִ� backup buffer�� �����϶�� �˷��ش�.
		if (va_release_semaphore(semid[0]) < 0)
		{
			va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_BACKUP, FUNCTION_BACKUP_SendBackupBuffer, ERROR_RESOURCE_SEMAPHORE_REMOVED, &commandData);
			
			return -1;
		}
		
		// sender�� ���� ����ϰ� �ִ� backup buffer�� ������ �����ϱ� �����ؼ� ���� backup buffer�� �����͸� ä�� �� �ְ� �ɶ����� ��ٸ���.
		if (va_grab_semaphore(semid[1]) < 0)
		{
			va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_BACKUP, FUNCTION_BACKUP_SendBackupBuffer, ERROR_RESOURCE_SEMAPHORE_REMOVED, &commandData);
			
			return -1;
		}
		
		// backup buffer�� index�� ���� buffer�� �����Ѵ�.
		bufferIndex++;
		bufferIndex = bufferIndex % CLIENT_BACKUP_BUFFER_NUMBER;
		
		if (jobComplete == 1)
		{
			return -1;
		}
	}
	
	
	return 0;
}

static int SendFileContents(char * file, struct fileHeader * fileHeader)
{
	va_fd_t fdBackupFile;
	int readSize;
	
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 1. open the backup file to send the file to VIRBAK ABIO server
	
	if ((fdBackupFile = va_open_backup_file(NULL, file)) != (va_fd_t)-1)
	{
		//////////////////////////////////////////////////////////////////////////////////////////////////
		// 2. send the head file header
		
		if (SendFileHeader(fileHeader, FILE_HEADER_HEAD) < 0)
		{
			va_close(fdBackupFile);
			
			return -1;
		}
		
		
		//////////////////////////////////////////////////////////////////////////////////////////////////
		// 3. send the file contents
		
		fileHeader->st.size = 0;
		
		// ���� ����ϰ� �ִ� backup buffer�� ���� ���� ��ŭ�� ���Ͽ��� �����͸� �д´�.
		while ((readSize = va_read(fdBackupFile, backupBuffer[bufferIndex] + backupBufferSize[bufferIndex], readDiskBufferSize - backupBufferSize[bufferIndex], DATA_TYPE_NOT_CHANGE)) > 0)
		{
			// encryption
			if(commandData.encryption == 1)
			{				
				va_aes_encrypt(
					backupBuffer[bufferIndex] + backupBufferSize[bufferIndex],
					backupBuffer[bufferIndex] + backupBufferSize[bufferIndex],
					readSize
					);					
			}	

			// ������ ũ�⸦ ����Ѵ�.
			fileHeader->st.size += readSize;
			
			// ���� ����ϰ� �ִ� backup buffer�� ũ�⸦ ���Ͽ��� ���� �������� ũ�⸸ŭ ������Ų��.
			backupBufferSize[bufferIndex] += readSize;
			
			// ���� �����͸� �����Ѵ�.
			if (SendBackupBuffer(0) < 0)
			{
				va_close(fdBackupFile);
				
				return -1;
			}
		}
		
		if (readSize < 0)
		{
			va_close(fdBackupFile);
			
			PrintBackupFileResult(file, fileHeader->filetype, -1, READ_FILE_ERROR);
			
			return 2;
		}
		
		
		//////////////////////////////////////////////////////////////////////////////////////////////////
		// 4. close the file
		
		va_close(fdBackupFile);
		
		
		//////////////////////////////////////////////////////////////////////////////////////////////////
		// 5. send the tail file header
		
		if (SendFileHeader(fileHeader, FILE_HEADER_TAIL) < 0)
		{
			return -1;
		}
	}
	else
	{
		PrintBackupFileResult(file, fileHeader->filetype, -1, OPEN_FILE_ERROR);
		
		return 2;
	}
	
	
	PrintBackupFileResult(file, fileHeader->filetype, 0, (enum backupFailCause)0);
	
	return 0;
}


// backup server�� ����� �������� �˸���.
static int SendEndOfBackup()
{
	struct fileHeader fileHeader;
	
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 1. ��� �������� ���� ����ϴ� file header�� ���� �����Ѵ�.
	
	memset(&fileHeader, 0, sizeof(struct fileHeader));
	strcpy(fileHeader.abio_check_code, ABIO_CHECK_CODE);
	strcpy(fileHeader.backupset, commandData.backupset);
	fileHeader.filetype = END_OF_BACKUP;
	fileHeader.objectNumber = commandData.objectNumber;
	
	if (SendFileHeader(&fileHeader, FILE_HEADER_HEAD) < 0)
	{
		return -1;
	}
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 2. wait until sender will be completed to send the last buffer
	
	if (SendBackupBuffer(1) < 0)
	{
		return -1;
	}
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 3. close the connection between the client and the backup server
	
	va_wait_thread(tidKeepSocketAlive);
	
	
	return 0;
}

#ifdef __ABIO_AIX
	static int SendFileContentsRawDeviceAIX(char * file, struct fileHeader * fileHeader, __uint64 fsSize)
	{
		va_fd_t fdBackupFile;
		int readSize;
		__uint64 totalReadSize;
		
		
		
		totalReadSize = 0;
		
		//////////////////////////////////////////////////////////////////////////////////////////////////
		// 1. open the backup file to send the file to VIRBAK ABIO server
		
		if ((fdBackupFile = va_open_backup_file(NULL, file)) != (va_fd_t)-1)
		{
			//////////////////////////////////////////////////////////////////////////////////////////////////
			// 2. send the head file header
			
			if (SendFileHeader(fileHeader, FILE_HEADER_HEAD) < 0)
			{
				va_close(fdBackupFile);
				
				return -1;
			}
			
			
			//////////////////////////////////////////////////////////////////////////////////////////////////
			// 3. send the file contents
			
			fileHeader->st.size = 0;
			
			// ���� ����ϰ� �ִ� backup buffer�� ���� ���� ��ŭ�� ���Ͽ��� �����͸� �д´�.
			while ((readSize = va_read(fdBackupFile, backupBuffer[bufferIndex] + backupBufferSize[bufferIndex], readDiskBufferSize - backupBufferSize[bufferIndex], DATA_TYPE_NOT_CHANGE)) > 0)
			{
				// encryption
				if(commandData.encryption == 1)
				{				
					va_aes_encrypt(
						backupBuffer[bufferIndex] + backupBufferSize[bufferIndex],
						backupBuffer[bufferIndex] + backupBufferSize[bufferIndex],
						readSize
						);					
				}	

				// ������ ũ�⸦ ����Ѵ�.
				fileHeader->st.size += readSize;
				
				// ������� �о���� ������ ũ�⸦ ����Ѵ�.
				totalReadSize += readSize;
				
				// ���� ����ϰ� �ִ� backup buffer�� ũ�⸦ ���Ͽ��� ���� �������� ũ�⸸ŭ ������Ų��.
				backupBufferSize[bufferIndex] += readSize;
				
				// ���� �����͸� �����Ѵ�.
				if (SendBackupBuffer(0) < 0)
				{
					va_close(fdBackupFile);
					
					return -1;
				}
			}
			
			// AIX raw device�� ��� �����͸� ���� ������ ���� �������� ũ�⺸�� read() function���� �о����� ������ ũ�Ⱑ �� ũ�� ������ �߻��Ѵ�.
			// �� ��� errno�� ENXOI�� ���õȴ�. �̶��� ������ ���� ũ�⸸ŭ�� ��Ȯ�� �о��־�� �Ѵ�.
			if (readSize < 0)
			{
				if (errno == ENXIO)
				{
					// ������ ���� �������� ũ�⸸ŭ�� �д´�.
					if ((readSize = va_read(fdBackupFile, backupBuffer[bufferIndex] + backupBufferSize[bufferIndex], fsSize - totalReadSize, DATA_TYPE_NOT_CHANGE)) > 0)
					{
						// encryption
						if(commandData.encryption == 1)
						{				
							va_aes_encrypt(
								backupBuffer[bufferIndex] + backupBufferSize[bufferIndex],
								backupBuffer[bufferIndex] + backupBufferSize[bufferIndex],
								readSize
								);					
						}	
						// ������ ũ�⸦ ����Ѵ�.
						fileHeader->st.size += readSize;
						
						// ���� ����ϰ� �ִ� backup buffer�� ũ�⸦ ���Ͽ��� ���� �������� ũ�⸸ŭ ������Ų��.
						backupBufferSize[bufferIndex] += readSize;
						
						// ���� �����͸� �����Ѵ�.
						if (SendBackupBuffer(0) < 0)
						{
							va_close(fdBackupFile);
							
							return -1;
						}
					}
					else
					{
						va_close(fdBackupFile);
						
						PrintBackupFileResult(file, fileHeader->filetype, -1, READ_FILE_ERROR);
						
						return 2;
					}
				}
				else
				{
					va_close(fdBackupFile);
					
					PrintBackupFileResult(file, fileHeader->filetype, -1, READ_FILE_ERROR);
					
					return 2;
				}
			}
			
			
			//////////////////////////////////////////////////////////////////////////////////////////////////
			// 4. close the file
			
			va_close(fdBackupFile);
			
			
			//////////////////////////////////////////////////////////////////////////////////////////////////
			// 5. send the tail file header
			
			if (SendFileHeader(fileHeader, FILE_HEADER_TAIL) < 0)
			{
				return -1;
			}
		}
		else
		{
			PrintBackupFileResult(file, fileHeader->filetype, -1, OPEN_FILE_ERROR);
			
			return 2;
		}
		
		
		PrintBackupFileResult(file, fileHeader->filetype, 0, (enum backupFailCause)0);
		
		return 0;
	}
	
	static int SendFileContentsRawDeviceAIXCompress(char * file, struct fileHeader * fileHeader, __uint64 fsSize)
	{
		va_fd_t fdBackupFile;
		__uint64 totalReadSize;
		
		int compressedDiskBufferSize;
		
		unsigned char * dataBuf;
		unsigned zlib_long_t dataBufSize;
		int readSize;
		
		unsigned char * compressedBuf;
		unsigned zlib_long_t compressedSize;
		
		unsigned char * sendBuf;
		int sendSize;
		
		int r;
		
		
		
		totalReadSize = 0;
		
		compressedDiskBufferSize = readDiskBufferSize;
		fileHeader->st.compressedDiskBufferSize = compressedDiskBufferSize;
		
		dataBuf = (unsigned char *)malloc(sizeof(char) * compressedDiskBufferSize);
		memset(dataBuf, 0, sizeof(char) * compressedDiskBufferSize);
		
		compressedBuf = (unsigned char *)malloc(sizeof(char) * (compressedDiskBufferSize * 2));
		memset(compressedBuf, 0, sizeof(char) * (compressedDiskBufferSize * 2));
		
		
		//////////////////////////////////////////////////////////////////////////////////////////////////
		// 1. open the backup file to send the file to VIRBAK ABIO server
		
		if ((fdBackupFile = va_open_backup_file(NULL, file)) != (va_fd_t)-1)
		{
			//////////////////////////////////////////////////////////////////////////////////////////////////
			// 2. send the head file header
			
			if (SendFileHeader(fileHeader, FILE_HEADER_HEAD) < 0)
			{
				va_close(fdBackupFile);
				
				va_free(dataBuf);
				va_free(compressedBuf);
				
				return -1;
			}
			
			
			//////////////////////////////////////////////////////////////////////////////////////////////////
			// 3. send the file contents
			
			fileHeader->st.size = 0;
			
			while ((readSize = va_read(fdBackupFile, dataBuf, compressedDiskBufferSize, DATA_TYPE_NOT_CHANGE)) > 0)
			{
				// encryption
				if(commandData.encryption == 1)
				{				
					va_aes_encrypt(
						backupBuffer[bufferIndex] + backupBufferSize[bufferIndex],
						backupBuffer[bufferIndex] + backupBufferSize[bufferIndex],
						readSize
						);					
				}	

				// ������ ũ�⸦ ����Ѵ�.
				fileHeader->st.size += readSize;
				
				// ������� �о���� ������ ũ�⸦ ����Ѵ�.
				totalReadSize += readSize;
				
				// �о���� �����͸� �����Ѵ�.
				compressedSize = compressedDiskBufferSize * 2;
				dataBufSize = (unsigned zlib_long_t)readSize;
				r = compress2(compressedBuf, &compressedSize, dataBuf, dataBufSize, commandData.compress);
				
				
				// ���࿡ �������� ��� ����� ����� ����ϰ�, ����� �����͸� �����Ѵ�.
				if (r == 0)
				{
					sendBuf = compressedBuf;
					sendSize = (int)compressedSize;
				}
				// ���࿡ �������� ��� �о���� ���� �����͸� �״�� �����Ѵ�.
				else
				{
					sendBuf = dataBuf;
					sendSize = readSize;
				}
				
				// ����� ������ ���� ũ�⸦ ����Ѵ�.
				fileHeader->st.compressedSize += sizeof(int) + sizeof(int) + sendSize;
				
				// ���� ���ο� ����� �������� ũ��� ����� �����͸� �����Ѵ�.
				r = htonl(r);					// host to network
				sendSize = htonl(sendSize);		// host to network
				if (SendBackupData(&r, sizeof(int)) < 0 || SendBackupData(&sendSize, sizeof(int)) < 0 || SendBackupData(sendBuf, htonl(sendSize)) < 0)
				{
					va_close(fdBackupFile);
					
					va_free(dataBuf);
					va_free(compressedBuf);
					
					return -1;
				}
			}
			
			// AIX raw device�� ��� �����͸� ���� ������ ���� �������� ũ�⺸�� read() function���� �о����� ������ ũ�Ⱑ �� ũ�� ������ �߻��Ѵ�.
			// �� ��� errno�� ENXOI�� ���õȴ�. �̶��� ������ ���� ũ�⸸ŭ�� ��Ȯ�� �о��־�� �Ѵ�.
			if (readSize < 0)
			{
				if (errno == ENXIO)
				{
					// ������ ���� �������� ũ�⸸ŭ�� �д´�.
					if ((readSize = va_read(fdBackupFile, dataBuf, fsSize - totalReadSize, DATA_TYPE_NOT_CHANGE)) > 0)
					{
						// encryption
						if(commandData.encryption == 1)
						{				
							va_aes_encrypt(
								backupBuffer[bufferIndex] + backupBufferSize[bufferIndex],
								backupBuffer[bufferIndex] + backupBufferSize[bufferIndex],
								readSize
								);					
						}	

						// ������ ũ�⸦ ����Ѵ�.
						fileHeader->st.size += readSize;
						
						// �о���� �����͸� �����Ѵ�.
						compressedSize = compressedDiskBufferSize * 2;
						dataBufSize = (unsigned zlib_long_t)readSize;
						r = compress2(compressedBuf, &compressedSize, dataBuf, dataBufSize, commandData.compress);
						
						
						// ���࿡ �������� ��� ����� ����� ����ϰ�, ����� �����͸� �����Ѵ�.
						if (r == 0)
						{
							sendBuf = compressedBuf;
							sendSize = (int)compressedSize;
						}
						// ���࿡ �������� ��� �о���� ���� �����͸� �״�� �����Ѵ�.
						else
						{
							sendBuf = dataBuf;
							sendSize = readSize;
						}
						
						// ����� ������ ���� ũ�⸦ ����Ѵ�.
						fileHeader->st.compressedSize += sizeof(int) + sizeof(int) + sendSize;
						
						// ���� ���ο� ����� �������� ũ��� ����� �����͸� �����Ѵ�.
						r = htonl(r);					// host to network
						sendSize = htonl(sendSize);		// host to network
						if (SendBackupData(&r, sizeof(int)) < 0 || SendBackupData(&sendSize, sizeof(int)) < 0 || SendBackupData(sendBuf, htonl(sendSize)) < 0)
						{
							va_close(fdBackupFile);
							
							va_free(dataBuf);
							va_free(compressedBuf);
							
							return -1;
						}
					}
					else
					{
						va_close(fdBackupFile);
						
						PrintBackupFileResult(file, fileHeader->filetype, -1, READ_FILE_ERROR);
						
						va_free(dataBuf);
						va_free(compressedBuf);
						
						return 2;
					}
				}
				else
				{
					va_close(fdBackupFile);
					
					PrintBackupFileResult(file, fileHeader->filetype, -1, READ_FILE_ERROR);
					
					va_free(dataBuf);
					va_free(compressedBuf);
					
					return 2;
				}
			}
			
			
			//////////////////////////////////////////////////////////////////////////////////////////////////
			// 4. close the file
			
			va_close(fdBackupFile);
			
			
			//////////////////////////////////////////////////////////////////////////////////////////////////
			// 5. ����� ������ ���� ���� �����Ѵ�.
			
			r = 1;
			sendSize = 0;
			
			// ����� ������ ���� ũ�⸦ ����Ѵ�.
			fileHeader->st.compressedSize += sizeof(int) + sizeof(int);
			
			// ���� ���ο� ����� �������� ũ�⸦ �����Ѵ�.
			r = htonl(r);					// host to network
			sendSize = htonl(sendSize);		// host to network
			if (SendBackupData(&r, sizeof(int)) < 0 || SendBackupData(&sendSize, sizeof(int)) < 0)
			{
				va_free(dataBuf);
				va_free(compressedBuf);
				
				return -1;
			}
			
			
			//////////////////////////////////////////////////////////////////////////////////////////////////
			// 6. send the tail file header
			
			if (SendFileHeader(fileHeader, FILE_HEADER_TAIL) < 0)
			{
				va_free(dataBuf);
				va_free(compressedBuf);
				
				return -1;
			}
		}
		else
		{
			PrintBackupFileResult(file, fileHeader->filetype, -1, OPEN_FILE_ERROR);
			
			va_free(dataBuf);
			va_free(compressedBuf);
			
			return 2;
		}
		
		
		va_free(dataBuf);
		va_free(compressedBuf);
		
		PrintBackupFileResult(file, fileHeader->filetype, 0, (enum backupFailCause)0);
		
		return 0;
	}
#endif

void CheckAdvancedOption()
{
	/*
	char charCompress[4];
	char charCheckPoint[16];
	char charRetryCount[16];
	char charRetryInterval[16];
	
	struct tm nextPause_tm;
	struct tm nextResume_tm;
	time_t nextPause;
	time_t nextResume;
	char charNextPauseTime[16];
	char charNextResumeTime[16];

	if(0 < commandData.writeDetailedLog)
	{
		SendJobLog(LOG_LEVEL_JOB,BACKUP_ADVANCED_OPTION, LOG_MSG_ID_ADVANCED_WRITE_DETAIL_LOG, "");
	}

	if(0 < commandData.compress)
	{
		//commandData.compress
		memset(charCompress,0,sizeof(charCompress));

		va_itoa(commandData.compress,charCompress);

		SendJobLog(LOG_LEVEL_JOB,BACKUP_ADVANCED_OPTION, LOG_MSG_ID_ADVANCED_COMPRESS_LOG,charCompress,"");	
	}

	if(0 < commandData.checkDeletedFile)
	{
		SendJobLog(LOG_LEVEL_JOB,BACKUP_ADVANCED_OPTION, LOG_MSG_ID_ADVANCED_CHECKDELETEFILE_LOG, "");
		
	}

	if(0 < commandData.saveIncompletedBackup)
	{
		//commandData.checkPoint		
		memset(charCheckPoint,0,sizeof(charCheckPoint));

		va_itoa(commandData.checkPoint,charCheckPoint);

		SendJobLog(LOG_LEVEL_JOB,BACKUP_ADVANCED_OPTION, LOG_MSG_ID_ADVANCED_SAVEINCOMPLETEDBACKUP_LOG,charCheckPoint, "");
		
	}

	if(0 < commandData.ARR)
	{
		SendJobLog(LOG_LEVEL_JOB,BACKUP_ADVANCED_OPTION, LOG_MSG_ID_ADVANCED_ARR_LOG, "");
		
	}

	if(0 < commandData.retryCount)
	{
		//commandData.retryCount
		//commandData.retryInterval
		memset(charRetryCount,0,sizeof(charRetryCount));
		memset(charRetryInterval,0,sizeof(charRetryInterval));

		va_itoa(commandData.retryCount,charRetryCount);
		va_itoa(commandData.retryInterval,charRetryInterval);

		SendJobLog(LOG_LEVEL_JOB,BACKUP_ADVANCED_OPTION, LOG_MSG_ID_ADVANCED_RETRY_LOG,charRetryCount,charRetryInterval,"");
		
	}

	if(0 < commandData.usePause)
	{
		//__int64 nextPauseTime;								
		//__int64 nextResumeTime;	
		memset(charNextPauseTime,0,sizeof(charNextPauseTime));
		memset(charNextResumeTime,0,sizeof(charNextResumeTime));

		nextPause= (time_t)commandData.nextPauseTime;
		nextResume= (time_t)commandData.nextResumeTime;

		memcpy(&nextPause_tm, localtime(&nextPause), sizeof(struct tm));
		memcpy(&nextResume_tm, localtime(&nextResume), sizeof(struct tm));


		sprintf(charNextPauseTime,"%02d:%02d",nextPause_tm.tm_hour,nextPause_tm.tm_min);
		sprintf(charNextResumeTime,"%02d:%02d",nextResume_tm.tm_hour,nextResume_tm.tm_min);

		SendJobLog(LOG_LEVEL_JOB,BACKUP_ADVANCED_OPTION, LOG_MSG_ID_ADVANCED_PAUSE_LOG,charNextPauseTime,charNextResumeTime,"");
		
	}
	*/

	//blanc find it
	if(0 < commandData.encryption)
	{
		SendJobLog2(LOG_LEVEL_JOB,BACKUP_ADVANCED_OPTION, LOG_MSG_ID_ADVANCED_ENCRYPTION_LOG, "");
	}
	
}