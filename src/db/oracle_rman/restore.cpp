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
static char * restoreBuffer[CLIENT_RESTORE_BUFFER_NUMBER];			// server�κ��� �����͸� �޴� buffer
static int restoreBufferSize[CLIENT_RESTORE_BUFFER_NUMBER];		// restore buffer�� ����ִ� ������ ��
static int dataBufferSize;									// restore buffer�� ũ��

static va_sem_t semid[CLIENT_RESTORE_BUFFER_NUMBER];

#ifdef __ABIO_LINUX
	static char * unalignedBuffer;
#endif
static char * writeBuffer;									// disk�� �����͸� �� buffer
static int writeBufferSize;									// write buffer�� ����ִ� ������ ��
extern int writeDiskBufferSize;								// write buffer�� ũ��

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
// ������� ���߿� ������ �߻����� �� ������ �߻��� ��Ҹ� ����Ѵ�. 0�� client error, 1�� server error�� �ǹ��Ѵ�.
// client���� ������ �߻��� ���, ������ �߻��ߴٰ� server���� �˷��ش�.
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
	// 1. �۾��� �����ϱ� ���� �����ؾߵǴ� �������� �����Ѵ�.
	
	sprintf(restoreLogFile, "%s.oracle.restore.log", commandData.jobID);
	sprintf(oracleRmanFilelistFile, "%s%c%s.oracle_rman.list", ABIOMASTER_CLIENT_FOLDER, FILE_PATH_DELIMITER, commandData.jobID);

	//cdb
	memset(tablespaceName ,0, sizeof(tablespaceName));
	memset(containerName ,0, sizeof(containerName));
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 2. backup server���� ���ῡ �ʿ��� �ڿ��� ����� backup server�� �����Ѵ�.
	
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
	
	// backup server�� �����Ѵ�.
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
	// 3. �۾��� �ʿ��� �ڿ����� �����´�.
	
	// restore buffer�� ũ�⸦ media block�� data buffer size��ŭ �����Ѵ�.
	dataBufferSize = commandData.mountStatus.blockSize - VOLUME_BLOCK_HEADER_SIZE;
	
	// restore buffer�� �����.
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
	
	// restore buffer ����ȭ�� �ʿ��� ������� �����.
	for (i = 0; i < CLIENT_RESTORE_BUFFER_NUMBER; i++)
	{
		if ((semid[i] = va_make_semaphore(0)) == (va_sem_t)-1)
		{
			va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE_RMAN_RESTORE, FUNCTION_RESTORE_StartRestore, ERROR_RESOURCE_MAKE_SEMAPHORE, &commandData);
			
			return -1;
		}
	}
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 4. restore file ����� �������� ������� ���� �α׸� ����Ѵ�.
	
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
	// 5. ������ �����Ѵ�.
	
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
	// 6. ������ ���������� ��ٸ���.
	
	va_wait_thread(tidReader);
	va_wait_thread(tidWriter);
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 7. ������ �������� backup server�� �˸���.
	
	// log sender�� �ߴܽ�Ų��.
	clientDataRecvComplete = 1;
	va_wait_thread(tidLog);
	
	// ������ �������� backup server�� �˸���.
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

		// backup server�κ��� �����͸� �޴´�.
		if ((restoreBufferSize[readerIndex] = va_recv(dataSock, restoreBuffer[readerIndex], dataBufferSize, 0, DATA_TYPE_NOT_CHANGE)) > 0)
		{
			commandData.capacity += restoreBufferSize[readerIndex];
		}
		// backup server�κ��� �����͸� �޴� socket�� �������ų� �̻��� ����ٸ� �����Ѵ�.
		else
		{
			break;
		}
	
		// writer���� ���� ����ϰ� �ִ� restore buffer���� �����͸� �о� ��������϶�� �˷��ش�.
		if (va_release_semaphore(semid[0]) < 0)
		{
			va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE_RMAN_RESTORE, FUNCTION_RESTORE_Reader, ERROR_RESOURCE_SEMAPHORE_REMOVED, &commandData);
			Error();

			break;
		}
		
		// writer�� ���� ����ϰ� �ִ� restore buffer���� �����͸� �о� ������ �ϱ� �����ؼ� ���� restore buffer�� �����͸� ä�� �� �ְ� �ɶ����� ��ٸ���.
		if (va_grab_semaphore(semid[1]) < 0)
		{
			va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE_RMAN_RESTORE, FUNCTION_RESTORE_Reader, ERROR_RESOURCE_SEMAPHORE_REMOVED, &commandData);
			Error();

			break;
		}
		
		// restore buffer�� index�� ���� buffer�� �����Ѵ�.
		readerIndex++;
		readerIndex = readerIndex % CLIENT_RESTORE_BUFFER_NUMBER;
	}
	
	// writer���� ������ restore buffer�� ������� �����ش�.
	va_release_semaphore(semid[0]);
	// backup server���� ������ �����Ѵ�.
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
		// ���� ����ϰ� �ִ� read buffer�� �����Ͱ� ������ ������ �� ���������� ��ٸ���.
		if (va_grab_semaphore(semid[0]) < 0)
		{
			va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE_RMAN_RESTORE, FUNCTION_RESTORE_Writer, ERROR_RESOURCE_SEMAPHORE_REMOVED, &commandData);
			Error();

			break;
		}
		
		// ���� ����ϰ� �ִ� read buffer�� client�� �����ϱ� ����������, ���� read buffer�� �����͸� ä���� �˷��ش�.
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
					// file header�� head�̸� ������� ������� Ȯ���ϰ�, ������� ������ �����.

					printf("FILE_HEADER : %d \n",newFileHeader.headerType);

					//if(strcmp(newFileHeader.abio_check_code,ABIO_CHECK_CODE) == 0)
					{
						if (newFileHeader.headerType == FILE_HEADER_HEAD)
						{
							// �ҿ����ϰ� ������� �� ������ ó��
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
								// ������ ������� ������� Ȯ���ϰ�, restore path�� �����.
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
								// ������ ������� ������� Ȯ���ϰ�, restore path�� �����.
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
								// ������ ������� ������� Ȯ���ϰ�, restore path�� �����.
								if (CheckRestoreFile(&fileHeader, NULL, restorePath, restoreTime, NULL, NULL) < 0)
								{								
									continue;
								}
								
								// ���� Ÿ�Կ� ���� ������ �����.
								fdRestore = MakeRestoreFile(restorePath, &fileHeader, &remainingFileSize);
							}
							else if (fileHeader.st.filetype == ORACLE_RMAN_COMMAND_ARCHIVE_LOG)
							{
								// ������ ������� ������� Ȯ���ϰ�, restore path�� �����.
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
						// file header�� tail�̸� ������ ��� ���������� Ȯ���ϰ�, ���� ũ�⸦ ������ ���� ũ��� �������ش�.
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
							// restore buffer�� ���Ͽ� ����Ѵ�.						
							fdRestore = WriteToFile(fdRestore, restoreBuffer[writerIndex] + i, &remainingFileSize, &writeFileSize, restorePath, &fileHeader);
						}
					}
				}
			}
			
			
			// read buffer�� �ִ� �����͸� �����.
			restoreBufferSize[writerIndex] = 0;
			memset(restoreBuffer[writerIndex], 0, dataBufferSize);
		}
		// backup server���� �����͸� ��� �޾����� �����Ѵ�.
		else
		{
			break;
		}
		
		
		// restore buffer�� index�� ���� buffer�� �����Ѵ�.
		writerIndex++;
		writerIndex = writerIndex % CLIENT_RESTORE_BUFFER_NUMBER;
	}
	
	
	// �ҿ����ϰ� ������� �� ������ ó��
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
	// write buffer�� �����ִ� �����͸� oracle rman ���� ��� �����Ѵ�.
	WriteToOracleRman(fdOracleRman, NULL, writeFileSize, 1,fileHeader->encryption);
	
	// oracle rman backup server���� ������ �����Ѵ�.
	va_close_socket(fdOracleRman->sock, ABIO_SOCKET_CLOSE_CLIENT);		
	
	// oracle rman file descriptor �ʱ�ȭ
	if (fdOracleRman->fileNumber > fdOracleRman->fileCompleteNumber)
	{
		// ������� ����� ����Ѵ�.
		//PrintRestoreResult(fileHeader, 0, ((struct oracleRmanFilePath *)fileHeader->filepath)->name, restorePath, RESTORE_IN_PROGRESS);
	}
	else
	{
		// load ����� Ȯ���Ѵ�.
		//r = CheckQueryResult(fdOracleRman);
		// ������� ����� ����Ѵ�.
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
		// ������� ������ �α׿� �����Ѵ�.
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
	
	
	
	// restore�Ϸ��� file�� �̹� ������ ��� overwrite �ɼǿ� ���� �����ϰ� ���� ������, ����� ������ �����Ѵ�.
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
			// ������ ������ �������� ������ �ݰ� ������ ������ ��� ����� ������ �����Ѵ�.
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
	
	// data buffer���� ��������� �����͸� �����´�.
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
	
	// ������ ������ block�̰ų� ���۰� ���� á���� ���Ͽ� ����.
	if (remainSize == 0 || writeBufferSize == writeDiskBufferSize)
	{
		// ���Ͽ� ������� �����͸� ����.
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
		// ������ ������ ��� ������ ������ ��� ����� ������ �����Ѵ�.
		if (restoreSuccess == 1)
		{
			#ifdef __ABIO_UNIX
				// directory�� ������ ������ �����Ѵ�.
				chown(restorePath, fileHeader->st.uid, fileHeader->st.gid);
			#endif
			
			// file�� last modification time�� ��� ����� �ð����� �����Ѵ�.
			va_set_file_time(NULL, restorePath, (time_t)fileHeader->st.mtime);
		}
		// ������ �����ϸ� ������ �����Ѵ�.
		else
		{
			// va_remove(NULL, restorePath);
		}
	}
	
	// ������� �α׸� �ۼ��Ѵ�.
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
		// 1�ʿ� �ѹ��� �۾��� ����Ǿ����� Ȯ���ϸ鼭 ���� �α� ���� �ð����� ����Ѵ�.
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
		
		// backup server�� ������� �α׸� �����Ѵ�.
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
		// server���� �߻��� ������ ����Ѵ�.
		errorLocation = ABIO_BACKUP_SERVER;
		memcpy(commandData.errorCode, recvCommandData->errorCode, sizeof(struct errorCode) * ERROR_CODE_NUMBER);
		
		// �۾��� �����Ѵ�.
		Error();
	}
	
	// server�κ��� ������ �޾����� ����Ѵ�.
	recvErrorFromServer = 1;
}

static void WaitForServerError()
{
	int count;
	
	
	
	// initialize variables
	count = 0;
	
	// server�κ��� ������ ���� ������ ����Ѵ�.
	while (recvErrorFromServer == 0)
	{
		va_sleep(1);
		
		count++;
		
		// client���� ������ �߻��� �Ŀ� ���� �ð����� ������ ���� ������ �����Ѵ�.
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
		// 1. ���� �޼����� client�� backup server ���̿��� �ְ�޾Ƽ� ���ʿ��� ��� ������ �ν��� �� �ֵ��� �Ѵ�.
		
		// client���� �߻��� ������ ��� backup server�� ������ �߻������� �˸���, backup server���� ������ �ν��� ������ ��ٸ���.
		if (errorLocation == ABIO_CLIENT)
		{
			// backup server���� client���� ������ �߻������� �˷��ش�.
			SendCommand(1, 0, 1, MODULE_NAME_SLAVE_RESTORE, "ERROR", MODULE_NAME_DB_ORACLE, clientIP, clientPort, clientNodename, serverIP, serverPort, serverNodename);
			
			// �������� ������ �޾Ҵٴ� �޼����� �ö����� ��ٸ���.
			WaitForServerError();
		}
		// server���� �߻��� ������ ��� backup server���� ������ �ν������� �˸���.
		else
		{
			// backup server���� client���� ������ �޾Ҵٰ� �˷��ش�.
			SendCommand(1, 0, 1, MODULE_NAME_SLAVE_RESTORE, "ERROR", MODULE_NAME_DB_ORACLE, clientIP, clientPort, clientNodename, serverIP, serverPort, serverNodename);
		}
		
		// �۾��� �������� ǥ���Ѵ�.
		jobComplete = 1;
		
		
		//////////////////////////////////////////////////////////////////////////////////////////////////
		// 2. backup server���� ������ �ν��� �Ͱ� ������� client���� �۾��� ������ �� �ֵ��� �Ѵ�.
		
		// backup server���� ������ �����ؼ� receiver�� ����� �� �ֵ��� �Ѵ�.
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
	// �۾��� �������� ǥ���Ѵ�.
	jobComplete = 1;
	
	// InnerMessageTransfer thread ����
	if (tidMessageTransfer != 0)
	{
		memset(&closeCommand, 0, sizeof(struct ck));
		strcpy(closeCommand.subCommand, "EXIT_INNER_MESSAGE_TRANSFER");
		va_msgsnd(midClient, mtypeClient, &closeCommand, sizeof(struct ck), 0);
		
		va_wait_thread(tidMessageTransfer);
	}
	
	// restore file list ����

	va_remove(ABIOMASTER_CLIENT_FILE_LIST_FOLDER, clientFilelistFile);
	va_remove(NULL, oracleRmanFilelistFile);
	
	// system �ڿ� �ݳ�
	#ifdef __ABIO_WIN
		va_remove_message_transfer(midClient);
	#endif
	
	for (i = 0; i < CLIENT_RESTORE_BUFFER_NUMBER; i++)
	{
		va_remove_semaphore(semid[i]);
	}
	
	// memory ����
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
