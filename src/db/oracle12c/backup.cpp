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

extern int maxOutboundBandwidth;					// Outbound 최대 Bandwidth


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
static int backupBufferSize[CLIENT_BACKUP_BUFFER_NUMBER];		// backup buffer에 들어있는 데이터 양
extern int readDiskBufferSize;							// backup buffer의 크기

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
// 백업 도중에 에러가 발생했을 때 에러가 발생한 장소를 기록한다. 0은 client error, 1은 server error를 의미한다.
// client에서 에러가 발생한 경우, 에러가 발생했다고 server에게 알려준다.
static enum {ABIO_CLIENT, ABIO_BACKUP_SERVER,ABIO_STOP} errorLocation;
///백업 작업 멈춤
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
		// 1. 에러 메세지를 client와 backup server 사이에서 주고받아서 양쪽에서 모두 에러를 인식할 수 있도록 한다.
		
		// client에서 발생한 에러일 경우 backup server에 에러가 발생했음을 알리고, backup server에서 에러를 인식할 때까지 기다린다.
		if (errorLocation == ABIO_CLIENT)
		{
			// backup server에게 client에서 에러가 발생했음을 알려준다.
			SendCommand(1, 0, 1, MODULE_NAME_SLAVE_BACKUP, "ERROR", MODULE_NAME_DB_ORACLE12C, clientIP, clientPort, clientNodename, serverIP, serverPort, serverNodename);
			
			// 서버에서 에러를 받았다는 메세지가 올때까지 기다린다.
			WaitForServerError();
		}
		else if (errorLocation == ABIO_STOP)
		{	
		}
		// server에서 발생한 에러일 경우 backup server에게 에러를 인식했음을 알린다.
		else
		{
			// backup server에게 client에서 에러를 받았다고 알려준다.
			SendCommand(1, 0, 1, MODULE_NAME_SLAVE_BACKUP, "ERROR", MODULE_NAME_DB_ORACLE12C, clientIP, clientPort, clientNodename, serverIP, serverPort, serverNodename);
		}
		
		//백업 state 정상적으로 변경.
		backupJobState=0;
		// 작업이 끝났음을 표시한다.
		jobComplete = 1;
		
		
		//////////////////////////////////////////////////////////////////////////////////////////////////
		// 2. backup server에서 에러를 인식한 것과 관계없이 client에서 작업을 종료할 수 있도록 한다.
		
		// 정상적인 상태에서 release되어야하는 횟수보다 한번더 release해서 reader가 종료될 수 있도록 한다.
		va_release_semaphore(semid[1]);
	}
}

static void Exit()
{
	struct ck closeCommand;
	
	int i;
	int j;
	
	
	
	// 작업이 끝났음을 표시한다.
	jobComplete = 1;
	
	// 정상적인 상태에서 release되어야하는 횟수보다 한번더 release해서 sender가 종료될 수 있도록 한다.
	va_release_semaphore(semid[0]);
	va_wait_thread(tidSender);
	
	// InnerMessageTransfer thread 종료
	if (tidMessageTransfer != 0)
	{
		memset(&closeCommand, 0, sizeof(struct ck));
		strcpy(closeCommand.subCommand, "EXIT_INNER_MESSAGE_TRANSFER");
		va_msgsnd(midClient, mtypeClient, &closeCommand, sizeof(struct ck), 0);
		
		va_wait_thread(tidMessageTransfer);
	}
	
	// backup file list 삭제
	va_remove(ABIOMASTER_CLIENT_FILE_LIST_FOLDER, commandData.filelist);
	
	// system 자원 반납
	#ifdef __ABIO_WIN
		va_remove_message_transfer(midClient);
	#endif
	
	for (i = 0; i < CLIENT_BACKUP_BUFFER_NUMBER; i++)
	{
		va_remove_semaphore(semid[i]);
	}
	
	// memory 해제
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
	// 1. 작업을 시작하기 전에 설정해야되는 정보들을 설정한다.
	
	// END_OF_BACKUP file header를 backup server로 전송한 후 data socket을 종료하지 않아도 backup server에서 END_OF_BACKUP file header가 담긴 data block을 받을 수 있도록 하기 위해
	// backup buffer의 크기를 사용자의 옵션을 무시하고 media block의 data buffer size만큼 지정한다.
	readDiskBufferSize = commandData.mountStatus.blockSize - VOLUME_BLOCK_HEADER_SIZE;
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 2. 작업에 필요한 자원들을 가져온다.
	
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
	
	// backup buffer를 만든다.
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
	
	// backup buffer 동기화에 필요한 세마포어를 만든다.
	for (i = 0; i < CLIENT_BACKUP_BUFFER_NUMBER; i++)
	{
		if ((semid[i] = va_make_semaphore(0)) == (va_sem_t)-1)
		{
			va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_BACKUP, FUNCTION_BACKUP_StartBackup, ERROR_RESOURCE_MAKE_SEMAPHORE, &commandData);
			
			return -1;
		}
	}
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 3. backup server와 연결한다.
	
	// backup server에 연결한다.
	if ((dataSock = va_connect(serverIP, commandData.client.dataPort, 1)) == -1)
	{
		va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_BACKUP, FUNCTION_BACKUP_StartBackup, ERROR_NETWORK_DATA_SOCKET_DOWN, &commandData);
		
		return -1;
	}
	
	// backup server와 연결되었음을 표시한다.
	serverConnected = 1;
	
	// backup server에 client의 버전을 알려준다.
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
	
	// server와 연결을 유지할 thread를 만든다.
	if ((tidKeepSocketAlive = va_create_thread(KeepSocketAlive, NULL)) == 0)
	{
		va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_BACKUP, FUNCTION_BACKUP_StartBackup, ERROR_RESOURCE_MAKE_THREAD, &commandData);
		
		return -1;
	}
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 5. backup file 목록을 가져오고 백업 시작 로그를 기록한다.
	
	if ((includeListNumber = va_load_text_file_lines(ABIOMASTER_CLIENT_FILE_LIST_FOLDER, commandData.filelist, &includeList, NULL)) == 0)
	{
		va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_BACKUP, FUNCTION_BACKUP_StartBackup, ERROR_JOB_OPEN_BACKUP_FILE_LIST, &commandData);
		
		return -1;
	}
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 6. 필요한 thread를 실행시킴
	
	// run the backup file sender
	if ((tidSender = va_create_thread(SendBackupFile, NULL)) == 0)
	{
		va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_BACKUP, FUNCTION_BACKUP_StartBackup, ERROR_RESOURCE_MAKE_THREAD, &commandData);
		
		return -1;
	}
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 7. ORACLE 환경을 설정하고 DB 파일을 backup server로 보냄
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
	
	// table space 목록을 읽어온다.
	if (GetTableSpaceList() < 0)
	{
		va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_BACKUP, FUNCTION_BACKUP_StartBackup, ERROR_ORACLE_GET_TABLESPACE_LIST, &commandData);
		
		return -1;
	}

	
	// archive log folder를 읽어온다.
	if (GetArchiveLogDir() < 0)
	{
		va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_BACKUP, FUNCTION_BACKUP_StartBackup, ERROR_ORACLE_GET_ARCHIVE_LOG_FOLDER, &commandData);
		
		return -1;
	}
	
	
	// table space를 백업한다.
	if (commandData.jobMethod == ABIO_DB_BACKUP_METHOD_ORACLE12C_FULL)
	{
		// 전체 table space를 백업하도록 지정되었는지 확인해서 백업한다.
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

		// 전체 table space를 백업하도록 지정되지 않았으면 지정된 개별 table space를 찾아서 백업한다.
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
		// archive log를 백업한다.
		if (BackupArchiveLog() < 0)
		{
			return -1;
		}
		
		// control file을 binary format으로 백업한다.
		if (BackupControlFileBinary() < 0)
		{
			return -1;
		}
		
		// control file을 user readable form으로 백업한다.
		if (BackupControlFileTrace() < 0)
		{
			return -1;
		}
	}
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 8. backup server에 백업이 끝났음을 알린다.
	
	if (SendEndOfBackup() < 0)
	{
		return -1;
	}
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 9. archive log를 삭제한다.
	
	// multi-session 백업을 할 경우 archive log를 삭제하게 되면 동시에 실행되는 다른 백업 작업에 영향을 주게 되기 때문에, archive log를 삭제하지 않는다.
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
	
	
	
	// control file을 binary format으로 백업한다.
	memset(controlFilePath, 0, sizeof(controlFilePath));
	sprintf(controlFilePath, "%s%c%s_%s", ORACLE12C_LOG_FOLDER, FILE_PATH_DELIMITER, commandData.backupset, ORACLE_CONTROL_FILE_BINARY_BACKUP_NAME);
	va_remove(NULL, controlFilePath);
	
	// control file name이 ABIO_FILE_LENGTH * 2보다 길면 백업하지 않는다.
	if (strlen(controlFilePath) > ABIO_FILE_LENGTH * 2)
	{
		PrintBackupFileResult(controlFilePath, ORACLE12C_CONTROL_FILE, -1, FILE_LENGTH_ERROR);
		
		return 0;
	}
	
	// control file을 abio oracle log folder로 백업한다.
	if (AlterBackupControlFileBinary(controlFilePath) < 0)
	{
		va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_BACKUP, FUNCTION_BACKUP_BackupControlFileBinary, ERROR_ORACLE_GET_CONTROL_FILE, &commandData);
		
		va_remove(NULL, controlFilePath);
		
		return -1;
	}
	
	// control file을 서버로 백업한다.
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
	
	// 백업한 control file을 삭제한다.
	va_remove(NULL, controlFilePath);
	
	
	return 0;
}

static int BackupControlFileTrace()
{
	char controlFilePath[MAX_PATH_LENGTH];
	
	struct abio_file_stat file_stat;
	
	
	
	// control file을 user readable form으로 백업한다.
	memset(controlFilePath, 0, sizeof(controlFilePath));
	sprintf(controlFilePath, "%s%c%s_%s", ORACLE12C_LOG_FOLDER, FILE_PATH_DELIMITER, commandData.backupset, ORACLE_CONTROL_FILE_TRACE_BACKUP_NAME);
	va_remove(NULL, controlFilePath);
	
	// control file name이 ABIO_FILE_LENGTH * 2보다 길면 백업하지 않는다.
	if (strlen(controlFilePath) > ABIO_FILE_LENGTH * 2)
	{
		// user readable form의 control file 백업은 백업에 실패하더라도 에러로 처리하지 않는다.
		
		return 0;
	}
	
	// control file을 abio oracle log folder로 백업한다.
	if (AlterBackupControlFileTrace(controlFilePath) == 0)
	{
		// control file을 서버로 백업한다.
		if (va_lstat(NULL, controlFilePath, &file_stat) == 0)
		{
			if (BackupFile(controlFilePath, &file_stat, ORACLE12C_CONTROL_FILE, NULL, NULL) < 0)
			{
				// user readable form의 control file 백업이라도 백업을 중단해야할 에러가 발생하면 에러로 처리한다.
				
				va_remove(NULL, controlFilePath);
				
				return -1;
			}
		}
		else
		{
			// user readable form의 control file 백업은 백업에 실패하더라도 에러로 처리하지 않는다.
		}
	}
	else
	{
		// user readable form의 control file 백업은 백업에 실패하더라도 에러로 처리하지 않는다.
	}
	
	// 백업한 control file을 삭제한다.
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
		// 특정 table space를 백업하도록 지정한 경우에 지정한 table space가 맞는지 확인한다.*/
		if (backupTableSpaceName != NULL && backupTableSpaceName[0] != '\0' && ((strcmp(tableSpaceList[i].tableSpace, backupTableSpaceName) != 0) || (strcmp(tableSpaceList[i].database, databaseName) != 0) ))
		{
			continue;
		}
		
		// table space 이름이 ABIO_FILE_LENGTH 보다 길면 백업하지 않는다.
		if (strlen(tableSpaceList[i].tableSpace) > ABIO_FILE_LENGTH)
		{
			PrintBackupTableSpaceResult(tableSpaceList[i].tableSpace, tableSpaceList[i].database, -1, FILE_LENGTH_ERROR);
			
			continue;
		}
		
		// data file 이름이 ABIO_FILE_LENGTH 보다 길면 백업하지 않는다.
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
				// table space에 begin backup을 실행하는데 실패했으면 end backup을 실행하고 다시 begin backup을 시도한다.
				// begin backup을 실행하는데 실패하는 경우는 권한이 없는 경우와 data file이 손실된 경우, end backup이 실행이 안된 경우가 있다.
				// 여기서는 end backup이 실행이 안된 경우만 처리할 수 있다.
				
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
		
		// archive log file 목록을 만든다.
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
		// archive log file이 없는 경우 archive log folder가 존재하는지 확인해서 archive log folder가 없는 경우 로그를 남기고 작업 결과를 partially completed로 처리하도록 한다.
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
	
	// archive log를 switch한 후에 바로 folder에 접근해서 목록을 얻으면
	// switch해서 생긴 파일의 목록이 읽히지 않거나 파일의 정보가 제대로 안나오는 경우가 있다.
	// 그래서 파일의 정보가 i-node나 file system에 완전히 업데이트될때까지 잠시 프로세스 실행을 중단한다.
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

					memset(archiveFilename, 0, sizeof(archiveFilename));

					// query 결과에서 tablespace 이름을 찾는다.
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
					// table space name을 기록한다.
					strcpy(archiveList[archiveFileCount], archiveFilename);
					// table space list 개수를 증가시킨다.
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
			// file이 symbolic link이면 link를 따라가서 파일의 정보를 얻는다.
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
					// 이외의 시스템에서는 raw device file 백업을 지원하지 않는다.
					return 2;
				#endif
			}
		#elif __ABIO_WIN
			// windows에서는 raw device file 백업을 지원하지 않는다.
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
						if (!strcmp(tableSpaceList[k].tableSpace, tableSpaceName) && !strcmp(tableSpaceList[k].database, databaseName))
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
	
	// server로부터 에러를 받을 때까지 대기한다.
	while (recvErrorFromServer == 0 && serverConnected == 1)
	{
		va_sleep(1);
		
		count++;
		
		// client에서 에러가 발생한 후에 일정 시간내에 에러가 오지 않으면 종료한다.
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
		// backup server로부터 제어 문자를 받는다.
		if (va_recv(dataSock, tmpbuf, 1, 0, DATA_TYPE_NOT_CHANGE) == 1)
		{
			// backup server로부터 END_OF_BACKUP file header를 받았다는 메세지가 도착하면 백업 상태를 data socket을 close할 수 있도록 만들고 thread를 종료한다.
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
		// 현재 사용하고 있는 backup buffer에 데이터가 꽉차서 전송할 수 있을때까지 기다린다.
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
		
		// 현재 사용하고 있는 backupset buffer를 서버로 전송하기 시작했으니, 다음 backup buffer에 데이터를 채우라고 알려준다.
		if (va_release_semaphore(semid[1]) < 0)
		{
			va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_BACKUP, FUNCTION_BACKUP_SendBackupFile, ERROR_RESOURCE_SEMAPHORE_REMOVED, &commandData);
			Error();
			
			break;
		}
		
		// 현재 사용하고 있는 backup buffer를 서버로 전송한다.
		if (backupBufferSize[senderIndex] > 0)
		{
			// maximum bandwidth가 지정된 경우 지정된 bandwidth를 넘지 않도록 하기 위해 네트워크로 데이터를 보내기 전에 bandwidth 사용량을 검사한다.
			// 이 검사를 네트워크로 데이터를 보낸 후에 하면 지정한 bandwidth를 넘긴 그래프를 끌어 내리는 효과가 있고, 
			// 이 검사를 네트워크로 데이터를 보내기 전에 하면 지정한 bandwidth를 넘기지 않도록 그래프를 누르는 효과가 있다.
			if (maxOutboundBandwidth > 0)
			{
				va_limit_network_bandwidth(maxOutboundBandwidth * 1024, backupBufferSize[senderIndex]);
			}
			
			// backup buffer를 서버로 전송한다.
			if (va_send(dataSock, backupBuffer[senderIndex], backupBufferSize[senderIndex], 0, DATA_TYPE_NOT_CHANGE) < 0)
			{
				if (jobComplete == 0)
				{
					va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_BACKUP, FUNCTION_BACKUP_SendBackupFile, ERROR_NETWORK_DATA_SOCKET_DOWN, &commandData);
					Error();
				}
			}
			
			// backup buffer에 있던 데이터를 지운다.
			backupBufferSize[senderIndex] = 0;
			memset(backupBuffer[senderIndex], 0, readDiskBufferSize);
		}
		
		// backup buffer의 index를 다음 buffer로 변경한다.
		senderIndex++;
		senderIndex = senderIndex % CLIENT_BACKUP_BUFFER_NUMBER;
	}
	
	// backup server와의 연결을 종료한다.
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
		// server에서 발생한 에러를 기록한다.
		errorLocation = ABIO_BACKUP_SERVER;
		memcpy(commandData.errorCode, recvCommandData->errorCode, sizeof(struct errorCode) * ERROR_CODE_NUMBER);
		
		// 작업을 종료한다.
		Error();
	}
	
	// server로부터 에러를 받았음을 기록한다.
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
				// link 파일의 status를 가져온다.
				if (va_stat(NULL, file, &linkStat) != 0)
				{
					return 2;
				}
				
				// link 파일의 경로를 읽어서 fstab에 기록되어 있는 경로를 찾아낸다.
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
			// link 파일인 경우 link를 따라가서 character special flie 경로를 구한다.
			if (va_is_symlink(file_stat->mode, file_stat->windowsAttribute))
			{
				// link 파일의 status를 가져온다.
				if (va_stat(NULL, file, &linkStat) != 0)
				{
					return 2;
				}
				
				// link 파일의 경로를 읽어서 file system 경로를 찾아낸다.
				if (va_read_symlink(file, linkPath, sizeof(linkPath)) == 0)
				{
					// block special file이면 character special file 경로를 구한다.
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
					// character special file이면 그대로 사용한다.
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
				// block special file이면 character special file 경로를 구한다.
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
				// character special file이면 그대로 사용한다.
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
	
	// 백업 후에 archive log를 삭제하는 옵션이 설정되어 있으면, 최근 3개의 archive log만 남기고 삭제한다.
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
	
	// file name 설정
	strcpy(((struct oracleFilePath *)fileHeader.filepath)->name, file);
	#ifdef __ABIO_WIN
		va_backslash_to_slash(((struct oracleFilePath *)fileHeader.filepath)->name);
	#endif
	
	// file status를 설정한다.
	MakeAbioStat(file, file_stat, &fileHeader.st);
	
	// file type을 설정한다.
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


// file status를 ABIO MASTER file status로 변환
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
			// 파일의 크기를 기록한다.
			fileHeader->st.size += readSize;
			
			// 읽어들인 데이터를 압축한다.
			compressedSize = compressedDiskBufferSize * 2;
			dataBufSize = (unsigned zlib_long_t)readSize;
			r = compress2(compressedBuf, &compressedSize, dataBuf, dataBufSize, commandData.compress);
			
			// 압축에 성공했을 경우 압축된 결과를 기록하고, 압축된 데이터를 전송한다.
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
			// 압축에 실패했을 경우 읽어들인 원본 데이터를 그대로 전송한다.
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
			
			// 압축된 데이터 블럭의 크기를 기록한다.
			fileHeader->st.compressedSize += sizeof(int) + sizeof(int) + sendSize;
			
			// 압축 여부와 압축된 데이터의 크기와 압축된 데이터를 전송한다.
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
		// 5. 압축된 데이터 블럭의 끝을 전송한다.
		
		r = 1;
		sendSize = 0;
		
		// 압축된 데이터 블럭의 크기를 기록한다.
		fileHeader->st.compressedSize += sizeof(int) + sizeof(int);
		
		// 압축 여부와 압축된 데이터의 크기를 전송한다.
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
	
	
	
	// backup buffer에 파일 헤더는 1K의 배수의 위치에 존재해야 한다.
	// 그래서 backup buffer의 크기가 1K의 배수가 아닐 때 backup buffer의 크기를 1K의 배수에 맞도록 조정해준다.
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
	
	// head type의 file header가 항상 backup buffer의 첫 부분에 오도록 하기 위해
	// file header가 tail일 경우 backup buffer의 끝까지 dummy data를 보낸다.
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
		// 전송하고 남은 데이터의 크기가 현재 사용하고 있는 backup buffer의 남은 크기보다 작으면 전송하고 남은 데이터만큼 전송한다.
		if ((int)dataSize - totalSendSize <= (int)(readDiskBufferSize - backupBufferSize[bufferIndex]))
		{
			sendSize = dataSize - totalSendSize;
		}
		// 전송하고 남은 데이터의 크기가 현재 사용하고 있는 backup buffer의 남은 크기보다 크면 현재 사용하고 있는 backup buffer의 남은 크기만큼만 전송한다.
		else
		{
			sendSize = readDiskBufferSize - backupBufferSize[bufferIndex];
		}
		
		// 현재 사용하고 있는 backup buffer에 데이터를 복사한다.
		memcpy(backupBuffer[bufferIndex] + backupBufferSize[bufferIndex], (char *)dataBuffer + totalSendSize, sendSize);
		
		// 전송할 데이터에서 현재 사용하고 있는 backup buffer에 실어서 보낸 데이터의 크기를 기록한다.
		totalSendSize += sendSize;
		
		// 현재 사용하고 있는 backup buffer의 크기를 데이터 크기만큼 증가시킨다.
		backupBufferSize[bufferIndex] += sendSize;
		
		// backup buffer를 전송한다.
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
		// backup buffer가 꽉 차면 buffer를 전송하고 index 변경
		if (backupBufferSize[bufferIndex] == readDiskBufferSize)
		{
			isSendBackupBuffer = 1;
		}
	}
	else
	{
		// backup buffer를 완전히 비우라는 요청이 오면 backup buffer의 끝까지 dummy data를 보낸다.
		backupBufferSize[bufferIndex] = readDiskBufferSize;
		
		isSendBackupBuffer = 1;
	}
	
	if (isSendBackupBuffer == 1)
	{
		// sender에게 현재 사용하고 있는 backup buffer를 전송하라고 알려준다.
		if (va_release_semaphore(semid[0]) < 0)
		{
			va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_BACKUP, FUNCTION_BACKUP_SendBackupBuffer, ERROR_RESOURCE_SEMAPHORE_REMOVED, &commandData);
			
			return -1;
		}
		
		// sender가 현재 사용하고 있는 backup buffer를 서버로 전송하기 시작해서 다음 backup buffer에 데이터를 채울 수 있게 될때까지 기다린다.
		if (va_grab_semaphore(semid[1]) < 0)
		{
			va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_BACKUP, FUNCTION_BACKUP_SendBackupBuffer, ERROR_RESOURCE_SEMAPHORE_REMOVED, &commandData);
			
			return -1;
		}
		
		// backup buffer의 index를 다음 buffer로 변경한다.
		bufferIndex++;
		bufferIndex = bufferIndex % CLIENT_BACKUP_BUFFER_NUMBER;
		
		if (jobComplete == 1)
		{
			return -1;
		}
	}
	
	
	// backup buffer를 완전히 비우라는 요청이 오면 위에서 전송하라고 알려준 버퍼의 전송이 끝날때까지 대기한다.
	if (flushBackupBuffer == 1)
	{
		// sender에게 현재 사용하고 있는 backup buffer를 전송하라고 알려준다.
		if (va_release_semaphore(semid[0]) < 0)
		{
			va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_BACKUP, FUNCTION_BACKUP_SendBackupBuffer, ERROR_RESOURCE_SEMAPHORE_REMOVED, &commandData);
			
			return -1;
		}
		
		// sender가 현재 사용하고 있는 backup buffer를 서버로 전송하기 시작해서 다음 backup buffer에 데이터를 채울 수 있게 될때까지 기다린다.
		if (va_grab_semaphore(semid[1]) < 0)
		{
			va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_BACKUP, FUNCTION_BACKUP_SendBackupBuffer, ERROR_RESOURCE_SEMAPHORE_REMOVED, &commandData);
			
			return -1;
		}
		
		// backup buffer의 index를 다음 buffer로 변경한다.
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
		
		// 현재 사용하고 있는 backup buffer에 남은 공간 만큼만 파일에서 데이터를 읽는다.
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

			// 파일의 크기를 기록한다.
			fileHeader->st.size += readSize;
			
			// 현재 사용하고 있는 backup buffer의 크기를 파일에서 읽은 데이터의 크기만큼 증가시킨다.
			backupBufferSize[bufferIndex] += readSize;
			
			// 읽은 데이터를 전송한다.
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


// backup server에 백업이 끝났음을 알린다.
static int SendEndOfBackup()
{
	struct fileHeader fileHeader;
	
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 1. 백업 데이터의 끝을 기록하는 file header를 만들어서 전송한다.
	
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
			
			// 현재 사용하고 있는 backup buffer에 남은 공간 만큼만 파일에서 데이터를 읽는다.
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

				// 파일의 크기를 기록한다.
				fileHeader->st.size += readSize;
				
				// 현재까지 읽어들인 파일의 크기를 기록한다.
				totalReadSize += readSize;
				
				// 현재 사용하고 있는 backup buffer의 크기를 파일에서 읽은 데이터의 크기만큼 증가시킨다.
				backupBufferSize[bufferIndex] += readSize;
				
				// 읽은 데이터를 전송한다.
				if (SendBackupBuffer(0) < 0)
				{
					va_close(fdBackupFile);
					
					return -1;
				}
			}
			
			// AIX raw device의 경우 데이터를 읽을 파일의 남은 데이터의 크기보다 read() function에서 읽어오라고 지정한 크기가 더 크면 에러가 발생한다.
			// 이 경우 errno는 ENXOI로 세팅된다. 이때는 파일의 남은 크기만큼만 정확이 읽어주어야 한다.
			if (readSize < 0)
			{
				if (errno == ENXIO)
				{
					// 파일의 남은 데이터의 크기만큼만 읽는다.
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
						// 파일의 크기를 기록한다.
						fileHeader->st.size += readSize;
						
						// 현재 사용하고 있는 backup buffer의 크기를 파일에서 읽은 데이터의 크기만큼 증가시킨다.
						backupBufferSize[bufferIndex] += readSize;
						
						// 읽은 데이터를 전송한다.
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

				// 파일의 크기를 기록한다.
				fileHeader->st.size += readSize;
				
				// 현재까지 읽어들인 파일의 크기를 기록한다.
				totalReadSize += readSize;
				
				// 읽어들인 데이터를 압축한다.
				compressedSize = compressedDiskBufferSize * 2;
				dataBufSize = (unsigned zlib_long_t)readSize;
				r = compress2(compressedBuf, &compressedSize, dataBuf, dataBufSize, commandData.compress);
				
				
				// 압축에 성공했을 경우 압축된 결과를 기록하고, 압축된 데이터를 전송한다.
				if (r == 0)
				{
					sendBuf = compressedBuf;
					sendSize = (int)compressedSize;
				}
				// 압축에 실패했을 경우 읽어들인 원본 데이터를 그대로 전송한다.
				else
				{
					sendBuf = dataBuf;
					sendSize = readSize;
				}
				
				// 압축된 데이터 블럭의 크기를 기록한다.
				fileHeader->st.compressedSize += sizeof(int) + sizeof(int) + sendSize;
				
				// 압축 여부와 압축된 데이터의 크기와 압축된 데이터를 전송한다.
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
			
			// AIX raw device의 경우 데이터를 읽을 파일의 남은 데이터의 크기보다 read() function에서 읽어오라고 지정한 크기가 더 크면 에러가 발생한다.
			// 이 경우 errno는 ENXOI로 세팅된다. 이때는 파일의 남은 크기만큼만 정확이 읽어주어야 한다.
			if (readSize < 0)
			{
				if (errno == ENXIO)
				{
					// 파일의 남은 데이터의 크기만큼만 읽는다.
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

						// 파일의 크기를 기록한다.
						fileHeader->st.size += readSize;
						
						// 읽어들인 데이터를 압축한다.
						compressedSize = compressedDiskBufferSize * 2;
						dataBufSize = (unsigned zlib_long_t)readSize;
						r = compress2(compressedBuf, &compressedSize, dataBuf, dataBufSize, commandData.compress);
						
						
						// 압축에 성공했을 경우 압축된 결과를 기록하고, 압축된 데이터를 전송한다.
						if (r == 0)
						{
							sendBuf = compressedBuf;
							sendSize = (int)compressedSize;
						}
						// 압축에 실패했을 경우 읽어들인 원본 데이터를 그대로 전송한다.
						else
						{
							sendBuf = dataBuf;
							sendSize = readSize;
						}
						
						// 압축된 데이터 블럭의 크기를 기록한다.
						fileHeader->st.compressedSize += sizeof(int) + sizeof(int) + sendSize;
						
						// 압축 여부와 압축된 데이터의 크기와 압축된 데이터를 전송한다.
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
			// 5. 압축된 데이터 블럭의 끝을 전송한다.
			
			r = 1;
			sendSize = 0;
			
			// 압축된 데이터 블럭의 크기를 기록한다.
			fileHeader->st.compressedSize += sizeof(int) + sizeof(int);
			
			// 압축 여부와 압축된 데이터의 크기를 전송한다.
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