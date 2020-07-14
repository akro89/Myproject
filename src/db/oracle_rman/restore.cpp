#include "require.h"
#include "abio.h"
#include "lib.h"
#include "vendor.h"
#include "oracle_rman.h"


extern char ABIOMASTER_CLIENT_FOLDER[MAX_PATH_LENGTH];
extern char ABIOMASTER_CLIENT_CATALOG_DB_FOLDER[MAX_PATH_LENGTH];
extern char ABIOMASTER_CLIENT_FILE_LIST_FOLDER[MAX_PATH_LENGTH];
extern char ABIOMASTER_CLIENT_LOG_FOLDER[MAX_PATH_LENGTH];


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


//////////////////////////////////////////////////////////////////////////////////////////////////
// message transfer variables
static va_trans_t midClient;		// message transfer id to receive a message from the parallel server
static int mtypeClient;


//////////////////////////////////////////////////////////////////////////////////////////////////
// restore buffer variables
static char * restoreBuffer[CLIENT_RESTORE_BUFFER_NUMBER];			// server로부터 데이터를 받는 buffer
static int restoreBufferSize[CLIENT_RESTORE_BUFFER_NUMBER];		// restore buffer에 들어있는 데이터 양
static int dataBufferSize;									// restore buffer의 크기

static va_sem_t semid[CLIENT_RESTORE_BUFFER_NUMBER];

#ifdef __ABIO_LINUX
	static char * unalignedBuffer;
#endif
static char * writeBuffer;									// disk에 데이터를 쓸 buffer
static int writeBufferSize;									// write buffer에 들어있는 데이터 양
extern int writeDiskBufferSize;								// write buffer의 크기

extern char outputFile[MAX_PATH_LENGTH];
//////////////////////////////////////////////////////////////////////////////////////////////////
// variables for restore tcp/ip socket
static va_sock_t dataSock;


//////////////////////////////////////////////////////////////////////////////////////////////////
// variables for reestore files
static char clientFilelistFile[MAX_PATH_LENGTH];
static char oracleRmanFilelistFile[MAX_PATH_LENGTH];

static struct restoreFile * restoreFileList;
static int restoreFileListNumber;

static char ** restoreBackupsetList;
static int restoreBackupsetListNumber;

extern va_sock_t rmanSock;
extern char rmanPort[PORT_LENGTH];
//////////////////////////////////////////////////////////////////////////////////////////////////
// error control
// 리스토어 도중에 에러가 발생했을 때 에러가 발생한 장소를 기록한다. 0은 client error, 1은 server error를 의미한다.
// client에서 에러가 발생한 경우, 에러가 발생했다고 server에게 알려준다.
static enum {ABIO_CLIENT, ABIO_BACKUP_SERVER} errorLocation;

static int errorOccured;
static int jobComplete;

static int serverConnected;
static int clientDataRecvComplete;
static int recvErrorFromServer;


//////////////////////////////////////////////////////////////////////////////////////////////////
// job thread
static va_thread_t tidReader;
static va_thread_t tidWriter;
static va_thread_t tidLog;
static va_thread_t tidMessageTransfer;

static char restoreLogFile[MAX_PATH_LENGTH];


//////////////////////////////////////////////////////////////////////////////////////////////////
// restore log
extern struct _backupLogDescription * backupLogDescription;
extern int backupLogDescriptionNumber;


//////////////////////////////////////////////////////////////////////////////////////////////////
// function prototype
static void InitRestore();
static va_thread_return_t InnerMessageTransfer(void * arg);

static int StartRestore();
static va_thread_return_t Reader(void * arg);
static va_thread_return_t KeepSocketAlive(void * arg);
static va_thread_return_t Writer(void * arg);
static int CheckRestoreFile(struct fileHeader * fileHeader, rman_fd_t * fdOracleRman, char * restorePath, char * restoreTime, char * dataFileFrom, char *dataFileTo);

static int MakeOracleRmanRestoreFileList();

static int WriteToOracleRman(rman_fd_t * fdOracleRman, char * dataBuffer, __uint64 * writeFileSize, int flush, int encryption);
static int DisconnectOracleRman(rman_fd_t * fdOracleRman, struct fileHeader * fileHeader, char * restorePath, __uint64 * writeFileSize);

static va_fd_t MakeRestoreFile(char * restorePath, struct fileHeader * fileHeader, __uint64 * remainingFileSize);
static va_fd_t MakeRestoreRegularFile(char * restorePath, struct fileHeader * fileHeader);
static va_fd_t WriteToFile(va_fd_t fdRestore, char * dataBuffer, __uint64 * remainingFileSize, __uint64 * writeFileSize, char * restorePath, struct fileHeader * fileHeader);
static va_fd_t WriteToSPFile(va_fd_t fdRestore, char * dataBuffer, __uint64 * writeFileSize, char * restorePath, struct fileHeader * fileHeader);
static int WriteToRegularFile(va_fd_t fd, char * dataBuffer, __uint64 * remainingFileSize);
static void CloseRestoreFile(va_fd_t fdRestore, char * restorePath, struct fileHeader * fileHeader, int restoreSuccess);
static va_thread_return_t WriteRunningLog(void * arg);

static void ServerError(struct ckBackup * recvCommandData);
static void WaitForServerError();
static void Error();
static void Exit();
static int CheckComplete(struct fileHeader * fileHeader,char * restorePath);
static void PrintError();
static void PrintRestoreResult(struct fileHeader * fileHeader, int result, char * backupedPath, char * restorePath, enum restoreFailCause failCause);


int Restore()
{
	InitRestore();
	
	
	if (StartRestore() < 0)
	{
		Error();
		
		Exit();
		
		return -1;
	}
	
	
	Exit();
	
	return 0;
}

static void InitRestore()
{
	int i;
	
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// message transfer variables
	midClient = (va_trans_t)-1;
	mtypeClient = 0;
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// restore buffer variables
	for (i = 0; i < CLIENT_RESTORE_BUFFER_NUMBER; i++)
	{
		restoreBuffer[i] = NULL;
		restoreBufferSize[i] = 0;
		
		semid[i] = (va_sem_t)-1;
	}
	
	dataBufferSize = 0;
	
	#ifdef __ABIO_LINUX
		unalignedBuffer = NULL;
	#endif
	writeBuffer = NULL;
	writeBufferSize = 0;
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// variables for restore tcp/ip socket
	dataSock = -1;
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// variables for reestore files
	memset(clientFilelistFile, 0, sizeof(clientFilelistFile));
	memset(oracleRmanFilelistFile, 0, sizeof(oracleRmanFilelistFile));
	
	restoreFileList = NULL;
	restoreFileListNumber = 0;
	
	restoreBackupsetList = NULL;
	restoreBackupsetListNumber = 0;
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// error control
	errorLocation = ABIO_CLIENT;
	
	errorOccured = 0;
	jobComplete = 0;
	
	serverConnected = 0;
	clientDataRecvComplete = 0;
	recvErrorFromServer = 0;
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// job thread
	tidReader = 0;
	tidWriter = 0;
	tidLog = 0;
	tidMessageTransfer = 0;
	
	memset(restoreLogFile, 0, sizeof(restoreLogFile));
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

static int StartRestore()
{
	char ** lines;
	int * linesLength;
	int lineNumber;
	
	struct restoreFileObject * fileObject;
	struct restoreTableSpaceObject * tableSpaceObject;

	//cdb
	char tablespaceName[DSIZ];
	char containerName[DSIZ];
	
	int i;
	int j;
	
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 1. 작업을 시작하기 전에 설정해야되는 정보들을 설정한다.
	
	sprintf(restoreLogFile, "%s.oracle.restore.log", commandData.jobID);
	sprintf(oracleRmanFilelistFile, "%s%c%s.oracle_rman.list", ABIOMASTER_CLIENT_FOLDER, FILE_PATH_DELIMITER, commandData.jobID);

	//cdb
	memset(tablespaceName ,0, sizeof(tablespaceName));
	memset(containerName ,0, sizeof(containerName));
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 2. backup server와의 연결에 필요한 자원을 만들고 backup server와 연결한다.
	
	// set the message transfer id to receive a message from the master server and backup server
	mtypeClient = commandData.mountStatus.mtype2;
	#ifdef __ABIO_UNIX
		midClient = midCK;
	#elif __ABIO_WIN
		if ((midClient = va_make_message_transfer(mtypeClient)) == (va_trans_t)-1)
		{
			va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE_RMAN_RESTORE, FUNCTION_RESTORE_StartRestore, ERROR_RESOURCE_MAKE_MESSAGE_TRANSFER, &commandData);
			
			return -1;
		}
	#endif
	
	// run the inner message transfer
	if ((tidMessageTransfer = va_create_thread(InnerMessageTransfer, NULL)) == 0)
	{
		va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE_RMAN_RESTORE, FUNCTION_RESTORE_StartRestore, ERROR_RESOURCE_MAKE_THREAD, &commandData);
		
		return -1;
	}
	
	// backup server에 연결한다.
	if ((dataSock = va_connect(serverIP, commandData.client.dataPort, 1)) == -1)
	{
		va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE_RMAN_RESTORE, FUNCTION_RESTORE_StartRestore, ERROR_NETWORK_DATA_SOCKET_DOWN, &commandData);
		
		return -1;
	}
	
	serverConnected = 1;
	
	// make a thread to keep alive the data socket
	if (commandData.version > VA_3_0_0)
	{
		va_create_thread(KeepSocketAlive, NULL);
	}
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 3. 작업에 필요한 자원들을 가져온다.
	
	// restore buffer의 크기를 media block의 data buffer size만큼 지정한다.
	dataBufferSize = commandData.mountStatus.blockSize - VOLUME_BLOCK_HEADER_SIZE;
	
	// restore buffer를 만든다.
	for (i = 0; i < CLIENT_RESTORE_BUFFER_NUMBER; i++)
	{
		restoreBuffer[i] = (char *)malloc(sizeof(char) * dataBufferSize);
		memset(restoreBuffer[i], 0, sizeof(char) * dataBufferSize);
	}
	
	#ifdef __ABIO_LINUX
		unalignedBuffer = (char *)malloc(sizeof(char) * (writeDiskBufferSize * 2));
		memset(unalignedBuffer, 0, sizeof(char) * (writeDiskBufferSize * 2));
		
		writeBuffer = va_ptr_align(unalignedBuffer, writeDiskBufferSize);
	#else
		writeBuffer = (char *)malloc(sizeof(char) * writeDiskBufferSize);
		memset(writeBuffer, 0, sizeof(char) * writeDiskBufferSize);
	#endif
	
	// restore buffer 동기화에 필요한 세마포어를 만든다.
	for (i = 0; i < CLIENT_RESTORE_BUFFER_NUMBER; i++)
	{
		if ((semid[i] = va_make_semaphore(0)) == (va_sem_t)-1)
		{
			va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE_RMAN_RESTORE, FUNCTION_RESTORE_StartRestore, ERROR_RESOURCE_MAKE_SEMAPHORE, &commandData);
			
			return -1;
		}
	}
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 4. restore file 목록을 가져오고 리스토어 시작 로그를 기록한다.
	
	sprintf(clientFilelistFile, "%s.client.list", commandData.jobID);
	if ((lineNumber = va_load_text_file_lines(ABIOMASTER_CLIENT_FILE_LIST_FOLDER, clientFilelistFile, &lines, &linesLength)) > 0)
	{

		for (i = 0; i < lineNumber; i ++)
		{
			if (!strncmp(lines[i], ORACLE_RMAN_BACKUP_FILE_LIST_OPTION_OBJECT_TABLESPACE, strlen(ORACLE_RMAN_BACKUP_FILE_LIST_OPTION_OBJECT_TABLESPACE)))
			{
				if (restoreFileListNumber % DEFAULT_MEMORY_FRAGMENT == 0)
				{
					restoreFileList = (struct restoreFile *)realloc(restoreFileList, sizeof(struct restoreFile) * (restoreFileListNumber + DEFAULT_MEMORY_FRAGMENT));
					memset(restoreFileList + restoreFileListNumber, 0, sizeof(struct restoreFile) * DEFAULT_MEMORY_FRAGMENT);
				}
				
				restoreFileList[restoreFileListNumber].filetype = ORACLE_RMAN_TABLESPACE;
				strcpy(restoreFileList[restoreFileListNumber].tableSpaceName, lines[i] + strlen(ORACLE_RMAN_BACKUP_FILE_LIST_OPTION_OBJECT_TABLESPACE) + 1);
				
				for (i = i + 1; i < lineNumber; i++)
				{
					if (!strncmp(lines[i], ORACLE_RMAN_BACKUP_FILE_LIST_OPTION_OBJECT_TABLESPACE, strlen(ORACLE_RMAN_BACKUP_FILE_LIST_OPTION_OBJECT_TABLESPACE)))
					{
						va_make_time_stamp((time_t)atoi(lines[i] + strlen(ORACLE_RMAN_BACKUP_FILE_LIST_OPTION_OBJECT_TABLESPACE) + 1), restoreFileList[restoreFileListNumber].untilTime, TIME_STAMP_TYPE_ORACLE_RMAN);
						
						break;
					}
					else
					{
						if (restoreFileList[restoreFileListNumber].objectNumber % DEFAULT_MEMORY_FRAGMENT == 0)
						{
							restoreFileList[restoreFileListNumber].object = realloc(restoreFileList[restoreFileListNumber].object, sizeof(struct restoreTableSpaceObject) * (restoreFileList[restoreFileListNumber].objectNumber + DEFAULT_MEMORY_FRAGMENT));
							tableSpaceObject = (struct restoreTableSpaceObject *)restoreFileList[restoreFileListNumber].object + restoreFileList[restoreFileListNumber].objectNumber;
							memset(tableSpaceObject, 0, sizeof(struct restoreTableSpaceObject) * DEFAULT_MEMORY_FRAGMENT);
						}
						tableSpaceObject = (struct restoreTableSpaceObject *)restoreFileList[restoreFileListNumber].object + restoreFileList[restoreFileListNumber].objectNumber;
						strcpy(tableSpaceObject->backupset, lines[i]);
						restoreFileList[restoreFileListNumber].objectNumber++;
						
						for (j = 0; j < restoreBackupsetListNumber; j++)
						{
							if (!strcmp(restoreBackupsetList[j], lines[i]))
							{
								break;
							}
						}
						if (j == restoreBackupsetListNumber)
						{
							if (restoreBackupsetListNumber % DEFAULT_MEMORY_FRAGMENT == 0)
							{
								restoreBackupsetList = (char **)realloc(restoreBackupsetList, sizeof(char *) * (restoreBackupsetListNumber + DEFAULT_MEMORY_FRAGMENT));
								memset(restoreBackupsetList + restoreBackupsetListNumber, 0, sizeof(char *) * DEFAULT_MEMORY_FRAGMENT);
							}
							restoreBackupsetList[restoreBackupsetListNumber] = (char *)malloc(sizeof(char) * (linesLength[i] + 1));
							memset(restoreBackupsetList[restoreBackupsetListNumber], 0, sizeof(char) * (linesLength[i] + 1));
							strcpy(restoreBackupsetList[restoreBackupsetListNumber], lines[i]);
							restoreBackupsetListNumber++;
						}
						break;
					}
				}
				strcpy(restoreFileList[restoreFileListNumber].dataFileFrom, lines[i+1]);
				strcpy(restoreFileList[restoreFileListNumber].dataFileTo, lines[i+2]);
				
				i+=2;
				restoreFileListNumber++;
			}
			else
			{
				if (i + 2 >= lineNumber)
				{
					break;
				}
				
				for (j = 0; j < restoreBackupsetListNumber; j++)
				{
					if (!strcmp(restoreBackupsetList[j], lines[i + 2]))
					{
						break;
					}
				}
				
				if (j == restoreBackupsetListNumber)
				{
					if (restoreBackupsetListNumber % DEFAULT_MEMORY_FRAGMENT == 0)
					{
						restoreBackupsetList = (char **)realloc(restoreBackupsetList, sizeof(char *) * (restoreBackupsetListNumber + DEFAULT_MEMORY_FRAGMENT));
						memset(restoreBackupsetList + restoreBackupsetListNumber, 0, sizeof(char *) * DEFAULT_MEMORY_FRAGMENT);
					}
					restoreBackupsetList[restoreBackupsetListNumber] = (char *)malloc(sizeof(char) * (linesLength[i + 2] + 1));
					memset(restoreBackupsetList[restoreBackupsetListNumber], 0, sizeof(char) * (linesLength[i + 2] + 1));
					strcpy(restoreBackupsetList[restoreBackupsetListNumber], lines[i + 2]);
					restoreBackupsetListNumber++;
				}
				
				
				if (restoreFileListNumber % DEFAULT_MEMORY_FRAGMENT == 0)
				{
					restoreFileList = (struct restoreFile *)realloc(restoreFileList, sizeof(struct restoreFile) * (restoreFileListNumber + DEFAULT_MEMORY_FRAGMENT));
					memset(restoreFileList + restoreFileListNumber, 0, sizeof(struct restoreFile) * DEFAULT_MEMORY_FRAGMENT);
				}
				
				fileObject = (struct restoreFileObject *)malloc(sizeof(struct restoreFileObject));
				memset(fileObject, 0, sizeof(struct restoreFileObject));
				
				if (!strncmp(lines[i], ORACLE_RMAN_BACKUP_FILE_LIST_OPTION_OBJECT_SPFILE, strlen(ORACLE_RMAN_BACKUP_FILE_LIST_OPTION_OBJECT_SPFILE)))
				{
					restoreFileList[restoreFileListNumber].filetype = ORACLE_RMAN_SPFILE;
					
					fileObject->source = (char *)malloc(sizeof(char) * (strlen(lines[i] + strlen(ORACLE_RMAN_BACKUP_FILE_LIST_OPTION_OBJECT_SPFILE) + 1) + 1));
					memset(fileObject->source, 0, strlen(lines[i] + strlen(ORACLE_RMAN_BACKUP_FILE_LIST_OPTION_OBJECT_SPFILE) + 1) + 1);
					strcpy(fileObject->source, lines[i] + strlen(ORACLE_RMAN_BACKUP_FILE_LIST_OPTION_OBJECT_SPFILE) + 1);
				}
				else if (!strncmp(lines[i], ORACLE_RMAN_BACKUP_FILE_LIST_OPTION_OBJECT_ARCHIVE_LOG, strlen(ORACLE_RMAN_BACKUP_FILE_LIST_OPTION_OBJECT_ARCHIVE_LOG)))
				{
					restoreFileList[restoreFileListNumber].filetype = ORACLE_RMAN_ARCHIVE_LOG;
					
					fileObject->source = (char *)malloc(sizeof(char) * (strlen(lines[i] + strlen(ORACLE_RMAN_BACKUP_FILE_LIST_OPTION_OBJECT_ARCHIVE_LOG) + 1) + 1));
					memset(fileObject->source, 0, strlen(lines[i] + strlen(ORACLE_RMAN_BACKUP_FILE_LIST_OPTION_OBJECT_ARCHIVE_LOG) + 1) + 1);
					strcpy(fileObject->source, lines[i] + strlen(ORACLE_RMAN_BACKUP_FILE_LIST_OPTION_OBJECT_ARCHIVE_LOG) + 1);
				}
				else if (!strncmp(lines[i], ORACLE_RMAN_BACKUP_FILE_LIST_OPTION_OBJECT_CONTROL_FILE, strlen(ORACLE_RMAN_BACKUP_FILE_LIST_OPTION_OBJECT_CONTROL_FILE)))
				{
					restoreFileList[restoreFileListNumber].filetype = ORACLE_RMAN_CONTROL_FILE;
					
					fileObject->source = (char *)malloc(sizeof(char) * (linesLength[i] + 1));
					memset(fileObject->source, 0, linesLength[i] + 1);
					strcpy(fileObject->source, lines[i]);
				}
				
				fileObject->target = (char *)malloc(sizeof(char) * (linesLength[i + 1] + 1));
				memset(fileObject->target, 0, linesLength[i + 1] + 1);
				strcpy(fileObject->target, lines[i + 1]);
				
				memset(fileObject->backupset, 0, sizeof(fileObject->backupset));
				strcpy(fileObject->backupset, lines[i + 2]);
				restoreFileList[restoreFileListNumber].object = fileObject;
				restoreFileList[restoreFileListNumber].objectNumber++;
				restoreFileListNumber++;
				i+=2;
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
		va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE_RMAN_RESTORE, FUNCTION_RESTORE_StartRestore, ERROR_JOB_OPEN_RESTORE_FILE_LIST, &commandData);
		
		return -1;
	}
	if (MakeOracleRmanRestoreFileList() < 0)
	{
		va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE_RMAN_RESTORE, FUNCTION_RESTORE_StartRestore, ERROR_JOB_OPEN_RESTORE_FILE_LIST, &commandData);
		
		return -1;
	}
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 5. 리스토어를 시작한다.
	
	// run the restore log sender
	if ((tidLog = va_create_thread(WriteRunningLog, NULL)) == 0)
	{
		va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE_RMAN_RESTORE, FUNCTION_RESTORE_StartRestore, ERROR_RESOURCE_MAKE_THREAD, &commandData);
		
		return -1;
	}
	
	// run the restore data receiver
	if ((tidReader = va_create_thread(Reader, NULL)) == 0)
	{
		va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE_RMAN_RESTORE, FUNCTION_RESTORE_StartRestore, ERROR_RESOURCE_MAKE_THREAD, &commandData);
		
		return -1;
	}
	
	// run the restore file writer
	if ((tidWriter = va_create_thread(Writer, NULL)) == 0)
	{
		va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE_RMAN_RESTORE, FUNCTION_RESTORE_StartRestore, ERROR_RESOURCE_MAKE_THREAD, &commandData);
		
		return -1;
	}
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 6. 리스토어가 끝날때까지 기다린다.
	
	va_wait_thread(tidReader);
	va_wait_thread(tidWriter);
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 7. 리스토어가 끝났음을 backup server에 알린다.
	
	// log sender를 중단시킨다.
	clientDataRecvComplete = 1;
	va_wait_thread(tidLog);
	
	// 리스토어가 끝났음을 backup server에 알린다.
	if (jobComplete == 0)
	{
		va_write_error_code(ERROR_LEVEL_INFO, MODULE_DB_ORACLE_RMAN_RESTORE, FUNCTION_RESTORE_Restore, INFO_RESTORE_END, &commandData);
		SendCommand(1, 0, 1, MODULE_NAME_SLAVE_RESTORE, "CLIENT_RESTORE_LOG", MODULE_NAME_DB_ORACLE, clientIP, clientPort, clientNodename, serverIP, serverPort, serverNodename);
	}
	
	
	return 0;
}

static va_thread_return_t Reader(void * arg)
{
	int readerIndex;
	
	
	
	// receive a backup data from the backup server
	readerIndex = 0;
	
	while (jobComplete == 0)
	{		

		// backup server로부터 데이터를 받는다.
		if ((restoreBufferSize[readerIndex] = va_recv(dataSock, restoreBuffer[readerIndex], dataBufferSize, 0, DATA_TYPE_NOT_CHANGE)) > 0)
		{
			commandData.capacity += restoreBufferSize[readerIndex];
		}
		// backup server로부터 데이터를 받는 socket이 끊어졌거나 이상이 생겼다면 종료한다.
		else
		{
			break;
		}
	
		// writer에게 현재 사용하고 있는 restore buffer에서 데이터를 읽어 리스토어하라고 알려준다.
		if (va_release_semaphore(semid[0]) < 0)
		{
			va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE_RMAN_RESTORE, FUNCTION_RESTORE_Reader, ERROR_RESOURCE_SEMAPHORE_REMOVED, &commandData);
			Error();

			break;
		}
		
		// writer가 현재 사용하고 있는 restore buffer에서 데이터를 읽어 리스토어를 하기 시작해서 다음 restore buffer에 데이터를 채울 수 있게 될때까지 기다린다.
		if (va_grab_semaphore(semid[1]) < 0)
		{
			va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE_RMAN_RESTORE, FUNCTION_RESTORE_Reader, ERROR_RESOURCE_SEMAPHORE_REMOVED, &commandData);
			Error();

			break;
		}
		
		// restore buffer의 index를 다음 buffer로 변경한다.
		readerIndex++;
		readerIndex = readerIndex % CLIENT_RESTORE_BUFFER_NUMBER;
	}
	
	// writer에게 마지막 restore buffer를 읽으라고 전해준다.
	va_release_semaphore(semid[0]);
	// backup server와의 연결을 종료한다.
	va_close_socket(dataSock, ABIO_SOCKET_CLOSE_SERVER);
	dataSock = -1;
	
	
	#ifdef __ABIO_UNIX
		return NULL;
	#else
		_endthreadex(0);
		return 0;
	#endif
}

static va_thread_return_t KeepSocketAlive(void * arg)
{
	char tmpbuf[1024];
	
	
	
	va_sleep(1);
	
	while (jobComplete == 0 && dataSock != -1)
	{
		va_send(dataSock, tmpbuf, sizeof(tmpbuf), 0, DATA_TYPE_NOT_CHANGE);
		
		va_sleep(1);
	}
	
	#ifdef __ABIO_UNIX
		return NULL;
	#else
		_endthreadex(0);
		return 0;
	#endif
}

static va_thread_return_t Writer(void * arg)
{
	struct fileHeader fileHeader;
	struct fileHeader newFileHeader;
	int isFileHeader;
	
	va_fd_t fdRestore;
	rman_fd_t fdOracleRman;

	__uint64 writeFileSize;
	__uint64 remainingFileSize;

	char restorePath[MAX_PATH_LENGTH];
	char restoreTime[ORACLE_RMAN_UNTIL_TIME_LENGTH];
	char restoreDataFileFrom[MAX_PATH_LENGTH];
	char restoreDataFileTo[MAX_PATH_LENGTH];
	char directory[MAX_PATH_LENGTH];
	
	int writerIndex;

	int i;
	int j;
	int k;
	
	char buff[262144];

	memset(buff,0,262144);
	strcpy(buff,"PIECE_CLOSE");

	static int check;


	
	// initialize variables
	fdRestore = (va_fd_t)-1;
	memset(commandData.backupFile, 0, sizeof(commandData.backupFile));

	memset(&fdOracleRman, 0, sizeof(rman_fd_t));
	fdOracleRman.pid = -1;
	fdOracleRman.sock = -1;
	
	writerIndex = 0;
	
	while (1)
	{
		// 현재 사용하고 있는 read buffer에 데이터가 꽉차서 전송할 수 있을때까지 기다린다.
		if (va_grab_semaphore(semid[0]) < 0)
		{
			va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE_RMAN_RESTORE, FUNCTION_RESTORE_Writer, ERROR_RESOURCE_SEMAPHORE_REMOVED, &commandData);
			Error();

			break;
		}
		
		// 현재 사용하고 있는 read buffer를 client로 전송하기 시작했으니, 다음 read buffer에 데이터를 채우라고 알려준다.
		if (va_release_semaphore(semid[1]) < 0)
		{
			va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE_RMAN_RESTORE, FUNCTION_RESTORE_Writer, ERROR_RESOURCE_SEMAPHORE_REMOVED, &commandData);
			Error();

			break;
		}
		
		// write data
		if (restoreBufferSize[writerIndex] > 0)
		{
			for (i = 0; i < dataBufferSize; i += DSIZ)
			{
				isFileHeader = 0;
				if (!strncmp(restoreBuffer[writerIndex] + i, ABIO_CHECK_CODE, strlen(ABIO_CHECK_CODE)))
				{	
					for (j = 0; j < restoreBackupsetListNumber; j++)
					{
						if (!strncmp(restoreBackupsetList[j], restoreBuffer[writerIndex] + i + ABIO_CHECK_CODE_SIZE, strlen(restoreBackupsetList[j])))
						{
							isFileHeader = 1;
							
							break;
						}
					}
					
					printf("backupset(%s) - %d\n",restoreBuffer[writerIndex] + i + ABIO_CHECK_CODE_SIZE,isFileHeader);					
				}
					
				if (isFileHeader == 1)
				{
					
					// read a file header
					memcpy(&newFileHeader, restoreBuffer[writerIndex] + i, sizeof(struct fileHeader));
					va_change_byte_order_fileHeader(&newFileHeader);	// network to host
					va_convert_v25_to_v30_fileHeader(&newFileHeader);
					// file header가 head이면 리스토어 대상인지 확인하고, 리스토어 파일을 만든다.

					printf("FILE_HEADER : %d \n",newFileHeader.headerType);

					//if(strcmp(newFileHeader.abio_check_code,ABIO_CHECK_CODE) == 0)
					{
						if (newFileHeader.headerType == FILE_HEADER_HEAD)
						{
							// 불완전하게 리스토어 된 파일의 처리
							if (fdOracleRman.sock != -1)
							{
								DisconnectOracleRman(&fdOracleRman, &fileHeader, restorePath, &writeFileSize);
							}
							if (fdRestore != (va_fd_t)-1)
							{
								CloseRestoreFile(fdRestore, restorePath, &fileHeader, 0);
							}

							// initialize variables
							fdRestore = (va_fd_t)-1;
							memset(&fdOracleRman, 0, sizeof(rman_fd_t));
							fdOracleRman.pid = -1;
							fdOracleRman.sock = -1;
							
							writeFileSize = 0;
							remainingFileSize = 0;
							memset(restorePath, 0, sizeof(restorePath));
							memset(restoreTime, 0, sizeof(restoreTime));
							
							memset(restoreDataFileFrom, 0, sizeof(restoreDataFileFrom));
							memset(restoreDataFileTo, 0, sizeof(restoreDataFileTo));
							writeBufferSize = 0;
							memset(writeBuffer, 0, writeDiskBufferSize);
							
							memset(commandData.backupFile, 0, sizeof(commandData.backupFile));
							
							// read a file header
							memcpy(&fileHeader, &newFileHeader, sizeof(struct fileHeader));

							check = 1;

							if (fileHeader.st.filetype == ORACLE_RMAN_TABLESPACE)
							{
								// 파일이 리스토어 대상인지 확인하고, restore path를 만든다.
								if (CheckRestoreFile(&fileHeader, &fdOracleRman, restorePath, restoreTime, restoreDataFileFrom, restoreDataFileTo) < 0)
								{
									continue;
								}
								fdOracleRman.connectType = ORACLE_RMAN_CONNECT_RESTORE_TABLESPACE;
								if (ConnectOracleRmanRestore(&fdOracleRman, restorePath, fileHeader.backupset, oracleRmanFilelistFile, restoreDataFileFrom, restoreDataFileTo) != 0)
								{
									PrintRestoreResult(&fileHeader, -1, ((struct oracleRmanFilePath *)fileHeader.filepath)->name, restorePath, WRITE_FILE_ERROR);
								}

								/*if (ConnectOracleRman(&fdOracleRman, restorePath, fileHeader.backupset, oracleRmanFilelistFile) != 0)
								{
									PrintRestoreResult(&fileHeader, -1, ((struct oracleRmanFilePath *)fileHeader.filepath)->name, restorePath, WRITE_FILE_ERROR);
								}		*/	
							}
							else if (fileHeader.st.filetype == ORACLE_RMAN_SPFILE)
							{
								// 파일이 리스토어 대상인지 확인하고, restore path를 만든다.
								if (CheckRestoreFile(&fileHeader, &fdOracleRman, restorePath, restoreTime, NULL, NULL) < 0)
								{
									continue;
								}

								memset(directory, 0, sizeof(directory));
								for (k = (int)strlen(restorePath) - 1; k > 2; k--)
								{
									if (restorePath[k] == '\\')
									{
										strncpy(directory, restorePath, k);
										va_make_directory(directory, 0, NULL);
										
										break;
									}
								}
								
								fdRestore = MakeRestoreFile(restorePath, &fileHeader, &remainingFileSize);
							}
							else if (fileHeader.st.filetype == ORACLE_RMAN_ARCHIVE_LOG || fileHeader.st.filetype == ORACLE_RMAN_CONTROL_FILE)
							{		
								// 파일이 리스토어 대상인지 확인하고, restore path를 만든다.
								if (CheckRestoreFile(&fileHeader, NULL, restorePath, restoreTime, NULL, NULL) < 0)
								{								
									continue;
								}
								
								// 파일 타입에 따라 파일을 만든다.
								fdRestore = MakeRestoreFile(restorePath, &fileHeader, &remainingFileSize);
							}
							else if (fileHeader.st.filetype == ORACLE_RMAN_COMMAND_ARCHIVE_LOG)
							{
								// 파일이 리스토어 대상인지 확인하고, restore path를 만든다.
								if (CheckRestoreFile(&fileHeader, &fdOracleRman, restorePath, restoreTime, NULL, restoreDataFileTo) < 0)
								{
									continue;
								}
								fdOracleRman.connectType = ORACLE_RMAN_CONNECT_RESTORE_COMMAND_ARCHIVELOG;
								
								if (restoreDataFileTo[0] != '\0')
								{
									if (ConnectOracleRmanRestore(&fdOracleRman, restorePath, fileHeader.backupset, (((struct oracleRmanFilePath *)fileHeader.filepath)->rmanFormat),NULL, restoreDataFileTo) != 0)
									{				
										check = 0;

										PrintRestoreResult(&fileHeader, -1, ((struct oracleRmanFilePath *)fileHeader.filepath)->name, (((struct oracleRmanFilePath *)fileHeader.filepath)->rmanFormat), WRITE_FILE_ERROR);
									}
								}
								else
								{
									if (ConnectOracleRman(&fdOracleRman, restorePath, NULL, fileHeader.backupset, (((struct oracleRmanFilePath *)fileHeader.filepath)->rmanFormat)) != 0)
									{
										check = 0;

										PrintRestoreResult(&fileHeader, -1, ((struct oracleRmanFilePath *)fileHeader.filepath)->name, (((struct oracleRmanFilePath *)fileHeader.filepath)->rmanFormat), WRITE_FILE_ERROR);
									}
								}
							}
						}
						// file header가 tail이면 파일이 모두 쓰여졌는지 확인하고, 파일 크기를 원래의 파일 크기로 조정해준다.
						else if (newFileHeader.headerType == FILE_HEADER_TAIL)
						{		
							if (fileHeader.st.filetype == ORACLE_RMAN_TABLESPACE  || fileHeader.st.filetype == ORACLE_RMAN_COMMAND_ARCHIVE_LOG)
							{
								fdOracleRman.fileCompleteNumber++;
								if (fdOracleRman.sock != -1)
								{
									DisconnectOracleRman(&fdOracleRman, &fileHeader, restorePath, &writeFileSize);				
								}
								va_sleep(2);
								if ( check != 0)
								{
									CheckComplete(&fileHeader, restorePath);
								}
							}
							else if ( fileHeader.st.filetype == ORACLE_RMAN_SPFILE)
							{	
								fdRestore = (va_fd_t)-1;
								CloseRestoreFile(fdRestore, restorePath, &fileHeader, 1);
							}
							else if (fileHeader.st.filetype == ORACLE_RMAN_ARCHIVE_LOG || fileHeader.st.filetype == ORACLE_RMAN_CONTROL_FILE )
							{
								if (fdRestore != (va_fd_t)-1)
								{
									CloseRestoreFile(fdRestore, restorePath, &fileHeader, 0);
								}
							}
						}
						else if (newFileHeader.headerType == FILE_HEADER_PIECE_TAIL  || newFileHeader.headerType == 50331648)
						{
							printf("FILE_HEADER_PIECE_TAIL : %s \n",newFileHeader.filepath);

							if (fdOracleRman.sock != -1)
							{
								if (WriteToOracleRman(&fdOracleRman, restoreBuffer[writerIndex] + i, &writeFileSize, 1, newFileHeader.encryption ) < 0)
								{
									DisconnectOracleRman(&fdOracleRman, &fileHeader, restorePath, &writeFileSize);
								}

								va_sleep(30);

								if (va_send(fdOracleRman.sock, (void*)&newFileHeader,sizeof(newFileHeader), 0, DATA_TYPE_NOT_CHANGE) < 0)
								{							
									DisconnectOracleRman(&fdOracleRman, &fileHeader, restorePath, &writeFileSize);
								}
							}
							va_sleep(30);
						}
						else if (newFileHeader.headerType == FILE_HEADER_PIECE_HEADER  || newFileHeader.headerType == 33554432)
						{
							printf("FILE_HEADER_PIECE_HEADER : %s \n",newFileHeader.filepath);

							/*if (fdOracleRman.sock != -1)
							{
								if (WriteToOracleRman(&fdOracleRman, restoreBuffer[writerIndex] + i, &writeFileSize, 1) < 0)
								{
									DisconnectOracleRman(&fdOracleRman, &fileHeader, restorePath, &writeFileSize);
								}

								va_sleep(30);

								if (va_send(fdOracleRman.sock, buff,11, 0, DATA_TYPE_NOT_CHANGE) < 0)
								{							
									DisconnectOracleRman(&fdOracleRman, &fileHeader, restorePath, &writeFileSize);
								}
							}
							va_sleep(30);*/
						}
					}
				} // end of file header
				else
				{	
					
					if(fileHeader.encryption == 1)
					{
						va_aes_decrypt(
							restoreBuffer[writerIndex] + i,
							restoreBuffer[writerIndex] + i,
							DSIZ
							);
					}

					if (fileHeader.st.filetype == ORACLE_RMAN_TABLESPACE  || fileHeader.st.filetype == ORACLE_RMAN_COMMAND_ARCHIVE_LOG)
					{
						if (fdOracleRman.sock != -1)
						{
							if (WriteToOracleRman(&fdOracleRman, restoreBuffer[writerIndex] + i, &writeFileSize, 0, fileHeader.encryption) < 0)
							{
								DisconnectOracleRman(&fdOracleRman, &fileHeader, restorePath, &writeFileSize);
							}
						}
					}
					else if (fileHeader.st.filetype == ORACLE_RMAN_SPFILE)
					{
						if (fdRestore != (va_fd_t)-1)
						{
							fdRestore = WriteToSPFile(fdRestore,restoreBuffer[writerIndex] + i, &writeFileSize, restorePath , &fileHeader);
						}
					}
					else if (fileHeader.st.filetype == ORACLE_RMAN_ARCHIVE_LOG || fileHeader.st.filetype == ORACLE_RMAN_CONTROL_FILE )
					{
						if (fdRestore != (va_fd_t)-1)
						{
							// restore buffer를 파일에 기록한다.						
							fdRestore = WriteToFile(fdRestore, restoreBuffer[writerIndex] + i, &remainingFileSize, &writeFileSize, restorePath, &fileHeader);
						}
					}
				}
			}
			
			
			// read buffer에 있던 데이터를 지운다.
			restoreBufferSize[writerIndex] = 0;
			memset(restoreBuffer[writerIndex], 0, dataBufferSize);
		}
		// backup server에서 데이터를 모두 받았으면 종료한다.
		else
		{
			break;
		}
		
		
		// restore buffer의 index를 다음 buffer로 변경한다.
		writerIndex++;
		writerIndex = writerIndex % CLIENT_RESTORE_BUFFER_NUMBER;
	}
	
	
	// 불완전하게 리스토어 된 파일의 처리
	if (fdOracleRman.sock != -1)
	{
		DisconnectOracleRman(&fdOracleRman, &fileHeader, restorePath, &writeFileSize);
	}
	if (fdRestore != (va_fd_t)-1)
	{
		CloseRestoreFile(fdRestore, restorePath, &fileHeader, 0);
	}
	
	
	#ifdef __ABIO_UNIX
		return NULL;
	#else
		_endthreadex(0);
		return 0;
	#endif
}

static int MakeOracleRmanRestoreFileList()
{
	va_fd_t fd;
	int itemNumber;
	int i;
	int j;

	struct restoreTableSpaceObject * tablespaceObject;
	struct restoreFileObject * fileObject;

	char buf[ABIO_FILE_LENGTH];


	fd = (va_fd_t)-1;
	itemNumber = 0;

	tablespaceObject = NULL;
	fileObject = NULL;
	/*for (i = 0; i < restoreFileListNumber; i++)
	{
		if (restoreFileList[i].filetype == ORACLE_RMAN_TABLESPACE || restoreFileList[i].filetype == ORACLE_RMAN_SPFILE)
		{
			itemNumber += restoreFileList[i].objectNumber;
		}
	}*/
	for (i = 0; i < restoreFileListNumber; i++)
	{
		if (restoreFileList[i].filetype == ORACLE_RMAN_TABLESPACE)
		{
			itemNumber += restoreFileList[i].objectNumber;
		}
	}
	if (itemNumber > 0 && (fd = va_open(NULL, oracleRmanFilelistFile, O_CREAT | O_RDWR, 1, 0)) != (va_fd_t)-1)
	{
		memset(buf, 0, sizeof(buf));
		sprintf(buf, "%d\n", itemNumber);
		va_write(fd, buf, (int)strlen(buf), DATA_TYPE_NOT_CHANGE);

		for (i = 0; i < restoreFileListNumber; i++)
		{
			if (restoreFileList[i].filetype == ORACLE_RMAN_TABLESPACE)
			{
				for (j = 0; j < restoreFileList[i].objectNumber; j++)
				{
					tablespaceObject = ((struct restoreTableSpaceObject *)restoreFileList[i].object) + j;
					
					memset(buf, 0, sizeof(buf));
					sprintf(buf, "%s_%s\n", restoreFileList[i].tableSpaceName, tablespaceObject->backupset);
					va_write(fd, buf, (int)strlen(buf), DATA_TYPE_NOT_CHANGE);
				}
			}
			/*else if (restoreFileList[i].filetype == ORACLE_RMAN_SPFILE && restoreFileList[i].objectNumber > 0)
			{
				fileObject = (struct restoreFileObject *)restoreFileList[i].object;
				
				memset(buf, 0, sizeof(buf));
				sprintf(buf, "SPFILE_%s\n", fileObject->backupset);
				va_write(fd, buf, (int)strlen(buf), DATA_TYPE_NOT_CHANGE);
			}*/
		}

		va_close(fd);
	}
	/*else
	{
		return -1;
	}*/


	return 0;
}

static int WriteToOracleRman(rman_fd_t * fdOracleRman, char * dataBuffer, __uint64 * writeFileSize, int flush, int encryption)
{
	int sendResult;
	
	
	
	sendResult = 0;
	
	if (flush == 0)
	{
		memcpy(writeBuffer + writeBufferSize, dataBuffer, DSIZ);
		writeBufferSize += DSIZ;
		
		if (writeBufferSize == writeDiskBufferSize)
		{
			if (va_send(fdOracleRman->sock, writeBuffer, writeBufferSize, 0, DATA_TYPE_NOT_CHANGE) == writeBufferSize)
			{
				*writeFileSize += writeBufferSize;
				commandData.filesSize += writeBufferSize;
			}
			else
			{
				sendResult = -1;
			}
			
			// make a data buffer empty
			writeBufferSize = 0;
			memset(writeBuffer, 0, writeDiskBufferSize);
		}
	}
	else
	{
		if (va_send(fdOracleRman->sock, writeBuffer, writeBufferSize, 0, DATA_TYPE_NOT_CHANGE) == writeBufferSize)
		{
			*writeFileSize += writeBufferSize;
			commandData.filesSize += writeBufferSize;
		}
		else
		{
			sendResult = -1;
		}
		
		// make a data buffer empty
		writeBufferSize = 0;
		memset(writeBuffer, 0, writeDiskBufferSize);
	}
	
	
	if (sendResult == 0)
	{
		return 0;
	}
	else
	{
		return -1;
	}
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
static int CheckComplete(struct fileHeader * fileHeader,char * restorePath)
{
	char ** lines;
	int * linesLength;
	int lineNumber;
	
	int i;
	int j;
	int result = 0;
	if ((lineNumber = va_load_text_file_lines(ABIOMASTER_CLIENT_FOLDER, outputFile, &lines, &linesLength)) > 0)
	{
		for(i = 0; i < lineNumber; i++)
		{		
			if(lines[i][0]=='O' && lines[i][1]=='R' && lines[i][2]=='A')
			{				
				PrintError();
				PrintRestoreResult(fileHeader, 1, ((struct oracleRmanFilePath *)fileHeader->filepath)->name, restorePath, (enum restoreFailCause)0);

				for (j = 0; j < lineNumber; j++)
				{
					va_free(lines[j]);
				}
				va_free(lines);
				va_free(linesLength);				
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
	PrintRestoreResult(fileHeader, 0, ((struct oracleRmanFilePath *)fileHeader->filepath)->name, restorePath, RESTORE_IN_PROGRESS);
	return 0;
}
static int DisconnectOracleRman(rman_fd_t * fdOracleRman, struct fileHeader * fileHeader, char * restorePath, __uint64 * writeFileSize)
{
	// write buffer에 남아있는 데이터를 oracle rman 으로 모두 전송한다.
	WriteToOracleRman(fdOracleRman, NULL, writeFileSize, 1,fileHeader->encryption);
	
	// oracle rman backup server와의 연결을 종료한다.
	va_close_socket(fdOracleRman->sock, ABIO_SOCKET_CLOSE_CLIENT);		
	
	// oracle rman file descriptor 초기화
	if (fdOracleRman->fileNumber > fdOracleRman->fileCompleteNumber)
	{
		// 리스토어 결과를 출력한다.
		//PrintRestoreResult(fileHeader, 0, ((struct oracleRmanFilePath *)fileHeader->filepath)->name, restorePath, RESTORE_IN_PROGRESS);
	}
	else
	{
		// load 결과를 확인한다.
		//r = CheckQueryResult(fdOracleRman);
		// 리스토어 결과를 출력한다.
		//PrintRestoreResult(fileHeader, r, ((struct oracleRmanFilePath *)fileHeader->filepath)->name, restorePath, (enum restoreFailCause)0);
		fdOracleRman->pid = -1;
	}

	fdOracleRman->sock = -1;
	return 0;
}

static int CheckRestoreFile(struct fileHeader * fileHeader, rman_fd_t * fdOracleRman, char * restorePath, char * restoreTime , char * dataFileFrom, char *dataFileTo)
{
	struct restoreFileObject * fileObject;
	struct restoreTableSpaceObject * tableSpaceObject;

	int i;
	int j;	
	
	for (i = 0; i < restoreFileListNumber; i++)
	{
		if (restoreFileList[i].objectNumber > 0)
		{

			if (fileHeader->filetype == ORACLE_RMAN_TABLESPACE)
			{
				if (strcmp(restoreFileList[i].tableSpaceName,((struct oracleRmanFilePath *)fileHeader->filepath)->name) == 0)
				{
					for (j = 0; j < restoreFileList[i].objectNumber; j++)
					{
						tableSpaceObject = ((struct restoreTableSpaceObject *)restoreFileList[i].object + j);
						if (strcmp(tableSpaceObject->backupset, fileHeader->backupset) == 0)
						{
							strcpy(restorePath, restoreFileList[i].tableSpaceName);
							strcpy(restoreTime, restoreFileList[i].untilTime);
							strcpy(dataFileFrom, restoreFileList[i].dataFileFrom);
							strcpy(dataFileTo, restoreFileList[i].dataFileTo);
							fdOracleRman->fileNumber = restoreFileList[i].objectNumber;
							
							break;
						}
					}
				}
			}
			else if (fileHeader->filetype == ORACLE_RMAN_CONTROL_FILE)
			{
				fileObject = (struct restoreFileObject *)restoreFileList[i].object;
				if (!strcmp(fileObject->backupset, fileHeader->backupset))
				{
					if (fileObject->target[0] != '\0')
					{
						for (j = (int)strlen(((struct oracleFilePath *)fileHeader->filepath)->name) - 1; j >= 0; j--)
						{
							if (((struct oracleFilePath *)fileHeader->filepath)->name[j] == DIRECTORY_IDENTIFIER)
							{
								break;
							}
						}
						sprintf(restorePath, "%s%c%s", fileObject->target, DIRECTORY_IDENTIFIER, ((struct oracleFilePath *)fileHeader->filepath)->name + j + 1);
					}
					else
					{
						strcpy(restorePath, ((struct oracleFilePath *)fileHeader->filepath)->name);
					}
					
					break;
				}
			}
			else if (fileHeader->filetype == ORACLE_RMAN_ARCHIVE_LOG)
			{
				fileObject = (struct restoreFileObject *)restoreFileList[i].object;

				for (j = (int)strlen(((struct oracleFilePath *)fileHeader->filepath)->name) - 1; j >= 0; j--)
				{
					if (((struct oracleFilePath *)fileHeader->filepath)->name[j] == DIRECTORY_IDENTIFIER)
					{
						break;
					}
				}

				if (!strcmp(fileObject->backupset, fileHeader->backupset) && !strcmp(fileObject->source, ((struct oracleFilePath *)fileHeader->filepath)->name + j + 1))
				{
					if (fileObject->target[0] != '\0')
					{
						sprintf(restorePath, "%s%c%s", fileObject->target, DIRECTORY_IDENTIFIER, ((struct oracleFilePath *)fileHeader->filepath)->name + j + 1);
					}
					else
					{
						strcpy(restorePath, ((struct oracleFilePath *)fileHeader->filepath)->name);
					}

					break;
				}
			}
			else if (fileHeader->filetype == ORACLE_RMAN_COMMAND_ARCHIVE_LOG)
			{

				fileObject = (struct restoreFileObject *)restoreFileList[i].object;
				if (!strcmp(fileObject->backupset, fileHeader->backupset) && strcmp(fileObject->source,((struct oracleFilePath *)fileHeader->filepath)->name) == 0)
				{

					strcpy(restorePath, ((struct oracleFilePath *)fileHeader->filepath)->name);

					if (fileObject->target[0] != '\0')
					{
						strcpy(dataFileTo, fileObject->target);
					}
					/*
					for (j = 0; j < restoreFileList[i].objectNumber; j++)
					{
						tableSpaceObject = ((struct restoreTableSpaceObject *)restoreFileList[i].object + j);
						if (strcmp(tableSpaceObject->backupset, fileHeader->backupset) == 0)
						{
							strcpy(restorePath, restoreFileList[i].tableSpaceName);
							
							fdOracleRman->fileNumber = restoreFileList[i].objectNumber;
							
							break;
						}
					}
					*/
				}


			}
		}
	}
	
	
	if (restorePath[0] != '\0')
	{
		// 리스토어 파일을 로그에 저장한다.
		strncpy(commandData.backupFile, restorePath, sizeof(commandData.backupFile) - 1);
		
		#ifdef __ABIO_WIN
			va_slash_to_backslash(restorePath);
		#endif
		
		return 0;
	}
	else
	{
		return -1;
	}
}

static va_fd_t MakeRestoreFile(char * restorePath, struct fileHeader * fileHeader, __uint64 * remainingFileSize)
{
	va_fd_t fd;
	
	
	
	fd = (va_fd_t)-1;
	
	if (fileHeader->filetype == ORACLE_RMAN_ARCHIVE_LOG || fileHeader->filetype == ORACLE_RMAN_CONTROL_FILE || fileHeader->filetype == ORACLE_RMAN_SPFILE)
	{
		if ((fd = MakeRestoreRegularFile(restorePath, fileHeader)) != (va_fd_t)-1)
		{
			*remainingFileSize = fileHeader->st.size;
		}
		else
		{
			CloseRestoreFile(fd, restorePath, fileHeader, 0);
		}
	}

	
	return fd;
}

static va_fd_t MakeRestoreRegularFile(char * restorePath, struct fileHeader * fileHeader)
{
	va_fd_t fd;
	struct abio_file_stat file_stat;
	
	
	
	// restore하려는 file이 이미 존재할 경우 overwrite 옵션에 따라 삭제하고 새로 만들지, 덮어쓰지 않을지 결정한다.
	if (va_stat(NULL, restorePath, &file_stat) == 0)
	{
		if (commandData.overwrite == 1)
		{
			va_remove(NULL, restorePath);
		}
		else
		{
			return (va_fd_t)-1;
		}
	}
	
	// make a restore file
	if ((fd = va_make_restore_file(NULL, restorePath, fileHeader->st.mode)) != (va_fd_t)-1)
	{
		return fd;
	}
	else
	{
		return (va_fd_t)-1;
	}
}

static va_fd_t WriteToSPFile(va_fd_t fdRestore, char * dataBuffer, __uint64 * writeFileSize, char * restorePath, struct fileHeader * fileHeader)
{	
	va_fd_t fd;
	fd = fdRestore;
	memcpy(writeBuffer + writeBufferSize, dataBuffer, DSIZ);
	writeBufferSize += DSIZ;
	
	if (writeBufferSize == writeDiskBufferSize)
	{
		if (va_write(fd, writeBuffer, writeBufferSize, DATA_TYPE_NOT_CHANGE) == writeBufferSize)
		{
			*writeFileSize += writeBufferSize;
			commandData.filesSize += writeBufferSize;
		}	
		else
		{
			CloseRestoreFile(fd, restorePath, fileHeader, 0);
			
			fd = (va_fd_t)-1;
		}
		// make a data buffer empty
		writeBufferSize = 0;
		memset(writeBuffer, 0, writeDiskBufferSize);
	}

	return fd;
}
static va_fd_t WriteToFile(va_fd_t fdRestore, char * dataBuffer, __uint64 * remainingFileSize, __uint64 * writeFileSize, char * restorePath, struct fileHeader * fileHeader)
{
	va_fd_t fd;
	
	fd = fdRestore;
	if (fileHeader->filetype == ORACLE_RMAN_ARCHIVE_LOG || fileHeader->filetype == ORACLE_RMAN_CONTROL_FILE)
	{
		if (WriteToRegularFile(fd, dataBuffer, remainingFileSize) == 0)
		{
			// 파일의 리스토어가 끝났으면 파일을 닫고 파일의 정보를 백업 당시의 정보로 변경한다.
			if (*remainingFileSize == 0)
			{
				CloseRestoreFile(fd, restorePath, fileHeader, 1);
				
				fd = (va_fd_t)-1;
			}
		}
		else
		{
			CloseRestoreFile(fd, restorePath, fileHeader, 0);
			
			fd = (va_fd_t)-1;
		}
	}
	
	
	return fd;
}

static int WriteToRegularFile(va_fd_t fd, char * dataBuffer, __uint64 * remainingFileSize)
{
	__uint64 remainSize;
	int writeError;
	
	
	remainSize = *remainingFileSize;
	writeError = 0;
	
	// data buffer에서 리스토어할 데이터를 가져온다.
	if (remainSize <= DSIZ)
	{
		memcpy(writeBuffer + writeBufferSize, dataBuffer, (int)remainSize);
		writeBufferSize += (int)remainSize;
		remainSize -= remainSize;
	}
	else
	{
		memcpy(writeBuffer + writeBufferSize, dataBuffer, DSIZ);
		writeBufferSize += DSIZ;
		remainSize -= DSIZ;
	}
	
	// 파일의 마지막 block이거나 버퍼가 가득 찼으면 파일에 쓴다.
	if (remainSize == 0 || writeBufferSize == writeDiskBufferSize)
	{
		// 파일에 리스토어 데이터를 쓴다.
		if (va_write(fd, writeBuffer, writeBufferSize, DATA_TYPE_NOT_CHANGE) == writeBufferSize)
		{
			commandData.filesSize += writeBufferSize;
		}
		else
		{
			writeError = 1;
		}
		
		// make a data buffer empty
		writeBufferSize = 0;
		memset(writeBuffer, 0, writeDiskBufferSize);
	}
	
	*remainingFileSize = remainSize;
	
	
	if (writeError == 0)
	{
		return 0;
	}
	else
	{
		return -1;
	}
}

static void CloseRestoreFile(va_fd_t fdRestore, char * restorePath, struct fileHeader * fileHeader, int restoreSuccess)
{
	if (fdRestore != (va_fd_t)-1)
	{
		va_close(fdRestore);
	}
	
	if (fileHeader->filetype == ORACLE_RMAN_ARCHIVE_LOG || fileHeader->filetype == ORACLE_RMAN_CONTROL_FILE ||  fileHeader->filetype == ORACLE_RMAN_SPFILE)
	{
		// 리스토어에 성공한 경우 파일의 정보를 백업 당시의 정보로 변경한다.
		if (restoreSuccess == 1)
		{
			#ifdef __ABIO_UNIX
				// directory의 소유자 정보를 변경한다.
				chown(restorePath, fileHeader->st.uid, fileHeader->st.gid);
			#endif
			
			// file의 last modification time을 백업 당시의 시간으로 변경한다.
			va_set_file_time(NULL, restorePath, (time_t)fileHeader->st.mtime);
		}
		// 리스토어에 실패하면 파일을 삭제한다.
		else
		{
			// va_remove(NULL, restorePath);
		}
	}
	
	// 리스토어 로그를 작성한다.
	if (restoreSuccess == 1)
	{
		PrintRestoreResult(fileHeader, 0, ((struct oracleRmanFilePath *)fileHeader->filepath)->name, restorePath, (enum restoreFailCause)0);
	}
	else
	{
		if (fdRestore == (va_fd_t)-1)
		{
			PrintRestoreResult(fileHeader, -1, ((struct oracleRmanFilePath *)fileHeader->filepath)->name, restorePath, MAKE_FILE_ERROR);
		}
		else
		{
			PrintRestoreResult(fileHeader, -1, ((struct oracleRmanFilePath *)fileHeader->filepath)->name, restorePath, WRITE_FILE_ERROR);
		}
	}
}

static va_thread_return_t WriteRunningLog(void * arg)
{
	int logSendInterval;
	int count;
	
	
	
	va_sleep(1);
	
	logSendInterval = 5;
	
	while (1)
	{
		// 1초에 한번씩 작업이 종료되었는지 확인하면서 다음 로그 전송 시간까지 대기한다.
		count = 0;
		while (jobComplete == 0 && clientDataRecvComplete == 0 && count < logSendInterval)
		{
			va_sleep(1);
			count++;
		}
		
		if (jobComplete == 1 || clientDataRecvComplete == 1)
		{
			break;
		}
		
		// backup server로 리스토어 로그를 전송한다.
		SendCommand(1, 0, 1, MODULE_NAME_SLAVE_RESTORE, "CLIENT_RESTORE_LOG", MODULE_NAME_DB_ORACLE, clientIP, clientPort, clientNodename, serverIP, serverPort, serverNodename);
	}
	
	
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

static void WaitForServerError()
{
	int count;
	
	
	
	// initialize variables
	count = 0;
	
	// server로부터 에러를 받을 때까지 대기한다.
	while (recvErrorFromServer == 0)
	{
		va_sleep(1);
		
		count++;
		
		// client에서 에러가 발생한 후에 일정 시간내에 에러가 오지 않으면 종료한다.
		if (count == TIME_OUT_JOB_CONNECTION && serverConnected == 1)
		{
			break;
		}
	}
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
			SendCommand(1, 0, 1, MODULE_NAME_SLAVE_RESTORE, "ERROR", MODULE_NAME_DB_ORACLE, clientIP, clientPort, clientNodename, serverIP, serverPort, serverNodename);
			
			// 서버에서 에러를 받았다는 메세지가 올때까지 기다린다.
			WaitForServerError();
		}
		// server에서 발생한 에러일 경우 backup server에게 에러를 인식했음을 알린다.
		else
		{
			// backup server에게 client에서 에러를 받았다고 알려준다.
			SendCommand(1, 0, 1, MODULE_NAME_SLAVE_RESTORE, "ERROR", MODULE_NAME_DB_ORACLE, clientIP, clientPort, clientNodename, serverIP, serverPort, serverNodename);
		}
		
		// 작업이 끝났음을 표시한다.
		jobComplete = 1;
		
		
		//////////////////////////////////////////////////////////////////////////////////////////////////
		// 2. backup server에서 에러를 인식한 것과 관계없이 client에서 작업을 종료할 수 있도록 한다.
		
		// backup server와의 연결을 종료해서 receiver가 종료될 수 있도록 한다.
		va_close_socket(dataSock, ABIO_SOCKET_CLOSE_SERVER);
		dataSock = -1;
	}
}

static void Exit()
{
	struct ck closeCommand;
	
	int i;
	int j;
	va_fd_t temp;
	temp = (va_fd_t)-1;
	int result=1;
	// 작업이 끝났음을 표시한다.
	jobComplete = 1;
	
	// InnerMessageTransfer thread 종료
	if (tidMessageTransfer != 0)
	{
		memset(&closeCommand, 0, sizeof(struct ck));
		strcpy(closeCommand.subCommand, "EXIT_INNER_MESSAGE_TRANSFER");
		va_msgsnd(midClient, mtypeClient, &closeCommand, sizeof(struct ck), 0);
		
		va_wait_thread(tidMessageTransfer);
	}
	
	// restore file list 삭제

	va_remove(ABIOMASTER_CLIENT_FILE_LIST_FOLDER, clientFilelistFile);
	va_remove(NULL, oracleRmanFilelistFile);
	
	// system 자원 반납
	#ifdef __ABIO_WIN
		va_remove_message_transfer(midClient);
	#endif
	
	for (i = 0; i < CLIENT_RESTORE_BUFFER_NUMBER; i++)
	{
		va_remove_semaphore(semid[i]);
	}
	
	// memory 해제
	for (i = 0; i < CLIENT_RESTORE_BUFFER_NUMBER; i++)
	{
		va_free(restoreBuffer[i]);
	}
	#ifdef __ABIO_LINUX
		va_free(unalignedBuffer);
	#else
		va_free(writeBuffer);
	#endif
	
	for (i = 0; i < restoreFileListNumber; i++)
	{
		if (restoreFileList[i].filetype != ORACLE_RMAN_TABLESPACE)
		{
			for (j = 0; j < restoreFileList[i].objectNumber; j++)
			{
				va_free(((struct restoreFileObject *)restoreFileList[i].object)[j].source);
				va_free(((struct restoreFileObject *)restoreFileList[i].object)[j].target);
			}
		}
		va_free(restoreFileList[i].object);
	}
	va_free(restoreFileList);
	
	for (i = 0; i < restoreBackupsetListNumber; i++)
	{
		va_free(restoreBackupsetList[i]);
	}
	va_free(restoreBackupsetList);
	if((temp = va_open(ABIOMASTER_CLIENT_FOLDER, outputFile, O_RDWR, 0, 0)) != (va_fd_t)-1)
	{
		va_close(temp);
		while(1)
		{
			result = va_remove(ABIOMASTER_CLIENT_FOLDER, outputFile);
			if(result == 0)
			{
				break;
			}
			va_sleep(1);
		}
	}
	
}

static void PrintRestoreResult(struct fileHeader * fileHeader, int result, char * backupedPath, char * restorePath, enum restoreFailCause failCause)
{
	char backupTime[TIME_STAMP_LENGTH];
	enum fileType filetype;

	
	if (result == 0)
	{
		commandData.fileNumber++;
	}
	else
	{
		commandData.objectNumber++;
	}
	
	
	
	memset(backupTime, 0, sizeof(backupTime));

	#ifdef __ABIO_WIN
		va_backslash_to_slash(restorePath);
	#endif

	filetype = fileHeader->st.filetype;


	if (filetype == ORACLE_RMAN_TABLESPACE)
	{
		va_make_time_stamp(fileHeader->st.fileBackupTime, backupTime, TIME_STAMP_TYPE_EXTERNAL_EN);

		if (result == 0)
		{
			if (failCause == RESTORE_IN_PROGRESS)
			{
				SendJobLog(LOG_MSG_ID_ORACLE_RMAN_RESTORE_TABLESPACE_IN_PROGRESS, backupTime, restorePath, "");
			}
			else
			{
				SendJobLog(LOG_MSG_ID_ORACLE_RMAN_RESTORE_TABLESPACE_SUCCESS, backupTime, restorePath, "");
			}
		}
		else
		{
			SendJobLog(LOG_MSG_ID_ORACLE_RMAN_RESTORE_TABLESPACE_FAIL, backupTime, restorePath, "");
		}
	}
	else if (filetype == ORACLE_RMAN_SPFILE)
	{
		va_make_time_stamp(fileHeader->st.fileBackupTime, backupTime, TIME_STAMP_TYPE_EXTERNAL_EN);

		if (result == 0)
		{
			SendJobLog(LOG_MSG_ID_ORACLE_RMAN_RESTORE_SPFILE_SUCCESS, backupTime, restorePath, "");
		}
		else
		{
			SendJobLog(LOG_MSG_ID_ORACLE_RMAN_RESTORE_SPFILE_FAIL, backupTime, restorePath, "");
		}
	}
	else if (filetype == ORACLE_RMAN_ARCHIVE_LOG || filetype == ORACLE_RMAN_COMMAND_ARCHIVE_LOG)
	{
		va_make_time_stamp(fileHeader->st.backupTime, backupTime, TIME_STAMP_TYPE_EXTERNAL_EN);

		if (result == 0)
		{
			SendJobLog(LOG_MSG_ID_ORACLE_RMAN_RESTORE_ARCHIVE_LOG_SUCCESS, backupTime, backupedPath,  (((struct oracleRmanFilePath *)fileHeader->filepath)->rmanFormat), "");
		}
		else
		{
			if (failCause == MAKE_FILE_ERROR)
			{
				SendJobLog(LOG_MSG_ID_ORACLE_RMAN_RESTORE_ARCHIVE_LOG_FAIL_MAKE, backupTime, backupedPath,  (((struct oracleRmanFilePath *)fileHeader->filepath)->rmanFormat), "");
			}
			else if (failCause == WRITE_FILE_ERROR)
			{
				SendJobLog(LOG_MSG_ID_ORACLE_RMAN_RESTORE_ARCHIVE_LOG_FAIL_WRITE, backupTime, backupedPath,  (((struct oracleRmanFilePath *)fileHeader->filepath)->rmanFormat), "");
			}
		}
	}
	else if (filetype == ORACLE_RMAN_CONTROL_FILE)
	{
		va_make_time_stamp(fileHeader->st.backupTime, backupTime, TIME_STAMP_TYPE_EXTERNAL_EN);

		if (result == 0)
		{
			SendJobLog(LOG_MSG_ID_ORACLE_RMAN_RESTORE_CONTROL_FILE_SUCCESS, backupTime, backupedPath, restorePath, "");
		}
		else
		{
			if (failCause == MAKE_FILE_ERROR)
			{
				SendJobLog(LOG_MSG_ID_ORACLE_RMAN_RESTORE_CONTROL_FILE_FAIL_MAKE, backupTime, backupedPath, restorePath, "");
			}
			else if (failCause == WRITE_FILE_ERROR)
			{
				SendJobLog(LOG_MSG_ID_ORACLE_RMAN_RESTORE_CONTROL_FILE_FAIL_WRITE, backupTime, backupedPath, restorePath, "");
			}
		}
	}

	#ifdef __ABIO_WIN
		va_slash_to_backslash(restorePath);
	#endif
}
