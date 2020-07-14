#include "require.h"
#include "abio.h"
#include "lib.h"
#include "vendor.h"
#include "oracle12c.h"


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


//////////////////////////////////////////////////////////////////////////////////////////////////
// compressed files restore buffer variables
static int isCompressed;									// client���� ����� ������ ������ ����Ǿ����� ����
static int compressedSize;									// client���� ����� ������ ����� ũ��

#ifdef __ABIO_LINUX
	static unsigned char * unalignCompressedBuf;
#endif
static unsigned char * compressedBuf;						// client���� ����� ������ ������
static unsigned zlib_long_t compressedBufSize;				// client���� ����� ������ ����� ũ��

#ifdef __ABIO_LINUX
	static unsigned char * unalignUncompressedBuf;
#endif
static unsigned char * uncompressedBuf;						// client���� ����� ������ �����Ͱ� ���������� ������
static unsigned zlib_long_t uncompressedBufSize;			// client���� ����� ������ �����Ͱ� ���������� ũ��

static int compressedDiskBufferSize;						// client���� �����ؼ� ����� �� ����� disk buffer�� ũ��

static enum compressedDataType compDataType = IS_COMPRESSED;
static char * compDataBuffer = (char *)&isCompressed;
static int compDataBufferSize = 0;
static int compDataRemainingSize = sizeof(int);


//////////////////////////////////////////////////////////////////////////////////////////////////
// variables for restore tcp/ip socket
static va_sock_t dataSock;


//////////////////////////////////////////////////////////////////////////////////////////////////
// variables for reestore files
static char clientFilelistFile[MAX_PATH_LENGTH];

static struct restoreFile * restoreFileList;
static int restoreFileListNumber;

static char ** restoreBackupsetList;
static int restoreBackupsetListNumber;


//////////////////////////////////////////////////////////////////////////////////////////////////
// error control
// ������� ���߿� ������ �߻����� �� ������ �߻��� ��Ҹ� ����Ѵ�. 0�� client error, 1�� server error�� �ǹ��Ѵ�.
// client���� ������ �߻��� ���, ������ �߻��ߴٰ� server���� �˷��ش�.
static enum {ABIO_CLIENT, ABIO_BACKUP_SERVER} errorLocation;

static int errorOccured;
static int jobComplete;

static int serverConnected;
static int restoreDataComplete;
static int recvErrorFromServer;


//////////////////////////////////////////////////////////////////////////////////////////////////
// job thread
static va_thread_t tidReader;
static va_thread_t tidWriter;
static va_thread_t tidLog;
static va_thread_t tidMessageTransfer;


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
static int CheckRestoreFile(struct fileHeader * fileHeader, char * restorePath);
static va_fd_t MakeRestoreFile(char * restorePath, struct fileHeader * fileHeader, __uint64 * remainingFileSize);
static va_fd_t MakeRestoreRegularFile(char * restorePath, struct fileHeader * fileHeader);
#ifdef __ABIO_UNIX
	static va_fd_t MakeRestoreRawDeviceFile(char * restorePath);
#endif
static va_fd_t WriteToFile(va_fd_t fdRestore, char * dataBuffer, __uint64 * remainingFileSize, __uint64 * writeFileSize, char * restorePath, struct fileHeader * fileHeader);
static int WriteToRegularFile(va_fd_t fd, char * dataBuffer, __uint64 * remainingFileSize);
static int WriteToRawDeviceFile(va_fd_t fd, char * dataBuffer, __uint64 * writeFileSize, int flush);
static int WriteToCompressFile(va_fd_t fd, char * dataBuffer, __uint64 * writeFileSize);
static void CloseFile(va_fd_t fdRestore, struct fileHeader * fileHeader, struct fileHeader * newFileHeader, __uint64 * writeFileSize, char * restorePath);
static void CloseRestoreFile(va_fd_t fdRestore, char * restorePath, struct fileHeader * fileHeader, int restoreSuccess);
static int ComparatorRestoreFileList(const void * a1, const void * a2);
static int SearchRestoreFile(char * backupset, char * sourceFile);
static va_thread_return_t WriteRunningLog(void * arg);

static void ServerError(struct ckBackup * recvCommandData);
static void WaitForServerError();
static void Error();
static void Exit();

static void PrintRestoreFileResult(char * tableSpaceName, char * dataFilePath, enum fileType filetype, char * restorePath, time_t backup_t, int result, enum restoreFailCause failCause);

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


static int StartRestore()
{
	char ** lines;
	int * linesLength;
	int lineNumber;
	
	int i;
	int j;
	
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 1. �۾��� �����ϱ� ���� �����ؾߵǴ� �������� �����Ѵ�.
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 2. backup server���� ���ῡ �ʿ��� �ڿ��� ����� backup server�� �����Ѵ�.
	
	// set the message transfer id to receive a message from the master server and backup server
	mtypeClient = commandData.mountStatus.mtype2;
	#ifdef __ABIO_UNIX
		midClient = midCK;
	#elif __ABIO_WIN
		if ((midClient = va_make_message_transfer(mtypeClient)) == (va_trans_t)-1)
		{
			va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_RESTORE, FUNCTION_RESTORE_StartRestore, ERROR_RESOURCE_MAKE_MESSAGE_TRANSFER, &commandData);
			
			return -1;
		}
	#endif
	
		// run the inner message transfer
	if ((tidMessageTransfer = va_create_thread(InnerMessageTransfer, NULL)) == 0)
	{
		va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_RESTORE, FUNCTION_RESTORE_StartRestore, ERROR_RESOURCE_MAKE_THREAD, &commandData);
		
		return -1;
	}
	// backup server�� �����Ѵ�.
	if ((dataSock = va_connect(serverIP, commandData.client.dataPort, 1)) == -1)
	{
		va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_RESTORE, FUNCTION_RESTORE_StartRestore, ERROR_NETWORK_DATA_SOCKET_DOWN, &commandData);
		
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
			va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_RESTORE, FUNCTION_RESTORE_StartRestore, ERROR_RESOURCE_MAKE_SEMAPHORE, &commandData);
			
			return -1;
		}
	}
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 4. restore file ����� �������� ������� ���� �α׸� ����Ѵ�.
	
	sprintf(clientFilelistFile, "%s.client.list", commandData.jobID);
	if ((lineNumber = va_load_text_file_lines(ABIOMASTER_CLIENT_FILE_LIST_FOLDER, clientFilelistFile, &lines, &linesLength)) > 0)
	{
		for (i = 2; i < lineNumber; i += 3)
		{
			if (restoreFileListNumber % DEFAULT_MEMORY_FRAGMENT == 0)
			{
				restoreFileList = (struct restoreFile *)realloc(restoreFileList, sizeof(struct restoreFile) * (restoreFileListNumber + DEFAULT_MEMORY_FRAGMENT));
				memset(restoreFileList + restoreFileListNumber, 0, sizeof(struct restoreFile) * DEFAULT_MEMORY_FRAGMENT);
			}
			restoreFileList[restoreFileListNumber].source = lines[i - 2];
			restoreFileList[restoreFileListNumber].target = lines[i - 1];
			restoreFileList[restoreFileListNumber].backupset = lines[i];
			restoreFileListNumber++;
			
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
		}
		
		if (restoreFileListNumber > 0)
		{
			qsort(restoreFileList, restoreFileListNumber, sizeof(struct restoreFile), ComparatorRestoreFileList);
		}
		
		va_free(lines);
		va_free(linesLength);
	}
	else
	{
		va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_RESTORE, FUNCTION_RESTORE_StartRestore, ERROR_JOB_OPEN_RESTORE_FILE_LIST, &commandData);
		
		return -1;
	}
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 5. ������ �����Ѵ�.
	
	// run the restore log sender
	if ((tidLog = va_create_thread(WriteRunningLog, NULL)) == 0)
	{
		va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_RESTORE, FUNCTION_RESTORE_StartRestore, ERROR_RESOURCE_MAKE_THREAD, &commandData);
		
		return -1;
	}
	// run the restore data receiver
	if ((tidReader = va_create_thread(Reader, NULL)) == 0)
	{
		va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_RESTORE, FUNCTION_RESTORE_StartRestore, ERROR_RESOURCE_MAKE_THREAD, &commandData);
		
		return -1;
	}
	
	// run the restore file writer
	if ((tidWriter = va_create_thread(Writer, NULL)) == 0)
	{
		va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_RESTORE, FUNCTION_RESTORE_StartRestore, ERROR_RESOURCE_MAKE_THREAD, &commandData);
		
		return -1;
	}
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 6. ������ ���������� ��ٸ���.
	
	va_wait_thread(tidReader);
	va_wait_thread(tidWriter);
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 7. ������ �������� backup server�� �˸���.
	
	// log sender�� �ߴܽ�Ų��.
	restoreDataComplete = 1;
	va_wait_thread(tidLog);
	
	// ������ �������� backup server�� �˸���.
	if (jobComplete == 0)
	{
		va_write_error_code(ERROR_LEVEL_INFO, MODULE_DB_ORACLE12C_SCRIPT_RESTORE, FUNCTION_RESTORE_Restore, INFO_RESTORE_END, &commandData);
		SendCommand(1, 0, 1, MODULE_NAME_SLAVE_RESTORE, "CLIENT_RESTORE_LOG", MODULE_NAME_DB_ORACLE12C, clientIP, clientPort, clientNodename, serverIP, serverPort, serverNodename);
	}
	



	return 0;
}

static va_thread_return_t Writer(void * arg)
{
	struct fileHeader fileHeader;
	struct fileHeader newFileHeader;
	int isFileHeader;
	
	va_fd_t fdRestore;
	__uint64 writeFileSize;
	__uint64 remainingFileSize;
	char restorePath[MAX_PATH_LENGTH];
	
	int writerIndex;
	
	int i;
	int j;
	int offset;
	
	
	
	// initialize variables
	fdRestore = (va_fd_t)-1;
	memset(commandData.backupFile, 0, sizeof(commandData.backupFile));
	
	
	writerIndex = 0;
	
	while (1)
	{
		// ���� ����ϰ� �ִ� read buffer�� �����Ͱ� ������ ������ �� ���������� ��ٸ���.
		if (va_grab_semaphore(semid[0]) < 0)
		{
			va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_RESTORE, FUNCTION_RESTORE_Writer, ERROR_RESOURCE_SEMAPHORE_REMOVED, &commandData);
			Error();

			break;
		}
		
		// ���� ����ϰ� �ִ� read buffer�� client�� �����ϱ� ����������, ���� read buffer�� �����͸� ä���� �˷��ش�.
		if (va_release_semaphore(semid[1]) < 0)
		{
			va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_RESTORE, FUNCTION_RESTORE_Writer, ERROR_RESOURCE_SEMAPHORE_REMOVED, &commandData);
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
				}
				
				if (isFileHeader == 1)
				{
					// read a file header
					memcpy(&newFileHeader, restoreBuffer[writerIndex] + i, sizeof(struct fileHeader));
					va_change_byte_order_fileHeader(&newFileHeader);	// network to host
					va_convert_v25_to_v30_fileHeader(&newFileHeader);
					
					// file header�� head�̸� ������� ������� Ȯ���ϰ�, ������� ������ �����.
					if (newFileHeader.headerType == FILE_HEADER_HEAD)
					{
						// �ҿ����ϰ� ������� �� ������ ó��
						if (fdRestore != (va_fd_t)-1)
						{
							CloseRestoreFile(fdRestore, restorePath, &fileHeader, 0);
						}
						
						// initialize variables
						fdRestore = (va_fd_t)-1;
						writeFileSize = 0;
						remainingFileSize = 0;
						memset(restorePath, 0, sizeof(restorePath));
						
						writeBufferSize = 0;
						memset(writeBuffer, 0, writeDiskBufferSize);
						
						memset(commandData.backupFile, 0, sizeof(commandData.backupFile));
						
						
						// read a file header
						memcpy(&fileHeader, &newFileHeader, sizeof(struct fileHeader));
						
						// ������ ������� ������� Ȯ���ϰ�, restore path�� �����.
						if (CheckRestoreFile(&fileHeader, restorePath) < 0)
						{
							continue;
						}
						
						
						// ���� Ÿ�Կ� ���� ������ �����.
						fdRestore = MakeRestoreFile(restorePath, &fileHeader, &remainingFileSize);
					}
					// file header�� tail�̸� ������ ��� ���������� Ȯ���ϰ�, ���� ũ�⸦ ������ ���� ũ��� �������ش�.
					else if (newFileHeader.headerType == FILE_HEADER_TAIL)
					{
						if (fdRestore != (va_fd_t)-1)
						{
							CloseFile(fdRestore, &fileHeader, &newFileHeader, &writeFileSize, restorePath);
							
							fdRestore = (va_fd_t)-1;
						}
					}
				}
				else
				{
					if (fdRestore != (va_fd_t)-1)
					{
						offset = i;
						
						if (fileHeader.version == VA_2_4)
						{
							if (sizeof(struct fileHeader) != sizeof(struct fileHeader_v24))
							{
								offset -= FILE_HEADER_V24_OFFSET;
							}
						}

						// encryption
						if (fileHeader.encryption == 1)
						{
							va_aes_decrypt(
								restoreBuffer[writerIndex] + offset,
								restoreBuffer[writerIndex] + offset,
								DSIZ);
						}

						
						// restore buffer�� ���Ͽ� ����Ѵ�.
						fdRestore = WriteToFile(fdRestore, restoreBuffer[writerIndex] + offset, &remainingFileSize, &writeFileSize, restorePath, &fileHeader);
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
			va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_RESTORE, FUNCTION_RESTORE_Reader, ERROR_RESOURCE_SEMAPHORE_REMOVED, &commandData);
			Error();

			break;
		}
		
		// writer�� ���� ����ϰ� �ִ� restore buffer���� �����͸� �о� ������ �ϱ� �����ؼ� ���� restore buffer�� �����͸� ä�� �� �ְ� �ɶ����� ��ٸ���.
		if (va_grab_semaphore(semid[1]) < 0)
		{
			va_write_error_code(ERROR_LEVEL_ERROR, MODULE_DB_ORACLE12C_SCRIPT_RESTORE, FUNCTION_RESTORE_Reader, ERROR_RESOURCE_SEMAPHORE_REMOVED, &commandData);
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
		while (jobComplete == 0 && restoreDataComplete == 0 && count < logSendInterval)
		{
			va_sleep(1);
			count++;
		}
		
		if (jobComplete == 1 || restoreDataComplete == 1)
		{
			break;
		}
		
		// backup server�� ������� �α׸� �����Ѵ�.
		SendCommand(1, 0, 1, MODULE_NAME_SLAVE_RESTORE, "CLIENT_RESTORE_LOG", MODULE_NAME_DB_ORACLE12C, clientIP, clientPort, clientNodename, serverIP, serverPort, serverNodename);
	}
	
	
	#ifdef __ABIO_UNIX
		return NULL;
	#else
		_endthreadex(0);
		return 0;
	#endif
}


static int ComparatorRestoreFileList(const void * a1, const void * a2)
{
	struct restoreFile * c1;
	struct restoreFile * c2;
	
	int r;
	
	
	
	c1 = (struct restoreFile *)a1;
	c2 = (struct restoreFile *)a2;
	
	r = strcmp(c1->backupset, c2->backupset);
	
	if (r > 0)
	{
		return 1;
	}
	else if (r < 0)
	{
		return -1;
	}
	else
	{
		r = strcmp(c1->source, c2->source);
		
		return r;
	}
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
	// compressed files restore buffer variables
	isCompressed = 0;
	compressedSize = 0;
	
	#ifdef __ABIO_LINUX
		unalignCompressedBuf = NULL;
	#endif
	compressedBuf = NULL;
	compressedBufSize = 0;
	
	#ifdef __ABIO_LINUX
		unalignUncompressedBuf = NULL;
	#endif
	uncompressedBuf = NULL;
	uncompressedBufSize = 0;
	
	compressedDiskBufferSize = 0;
	
	compDataType = (enum compressedDataType)0;
	compDataBuffer = NULL;
	compDataBufferSize = 0;
	compDataRemainingSize = 0;
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// variables for restore tcp/ip socket
	dataSock = -1;
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// variables for reestore files
	memset(clientFilelistFile, 0, sizeof(clientFilelistFile));
	
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
	restoreDataComplete = 0;
	recvErrorFromServer = 0;
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// job thread
	tidReader = 0;
	tidWriter = 0;
	tidLog = 0;
	tidMessageTransfer = 0;
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
			SendCommand(1, 0, 1, MODULE_NAME_SLAVE_RESTORE, "ERROR", MODULE_NAME_DB_ORACLE12C, clientIP, clientPort, clientNodename, serverIP, serverPort, serverNodename);
			
			// �������� ������ �޾Ҵٴ� �޼����� �ö����� ��ٸ���.
			WaitForServerError();
		}
		// server���� �߻��� ������ ��� backup server���� ������ �ν������� �˸���.
		else
		{
			// backup server���� client���� ������ �޾Ҵٰ� �˷��ش�.
			SendCommand(1, 0, 1, MODULE_NAME_SLAVE_RESTORE, "ERROR", MODULE_NAME_DB_ORACLE12C, clientIP, clientPort, clientNodename, serverIP, serverPort, serverNodename);
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
		va_free(restoreFileList[i].source);
		va_free(restoreFileList[i].target);
		va_free(restoreFileList[i].backupset);
	}
	va_free(restoreFileList);
	
	for (i = 0; i < restoreBackupsetListNumber; i++)
	{
		va_free(restoreBackupsetList[i]);
	}
	va_free(restoreBackupsetList);
}

static void CloseFile(va_fd_t fdRestore, struct fileHeader * fileHeader, struct fileHeader * newFileHeader, __uint64 * writeFileSize, char * restorePath)
{
	// ����� �����Ͱ� �ƴϸ� ���� write buffer ���ۿ� �����ִ� �����͸� ��� ���Ͽ� ����, ������� �Ϸ� ���θ� �˻��Ѵ�.
	// raw device file�� ��쿡�� �߻��Ѵ�.
	if (fileHeader->compress == 0)
	{
		// regular file�� ��� TAIL file header�� �����µ� restore file�� �����ִٸ� ������ ������������ ����� ���̴�.
		if (fileHeader->filetype == ORACLE12C_REGULAR_DATA_FILE || fileHeader->filetype == ORACLE12C_ARCHIVE_LOG || fileHeader->filetype == ORACLE12C_CONTROL_FILE)
		{
			CloseRestoreFile(fdRestore, restorePath, fileHeader, 0);
		}
		else if (fileHeader->filetype == ORACLE12C_RAW_DEVICE_DATA_FILE)
		{
			// write buffer�� �����ִ� �����͸� ���Ͽ� ��� ����.
			if (WriteToRawDeviceFile(fdRestore, NULL, writeFileSize, 1) == 0)
			{
				// raw device file�� ��쿡�� 1K byte�� ����� ������ ������� �ֱ� ������ 
				// ������ ���� ũ�� ��ŭ�� �����Ͱ� ������� ���Ͽ� ������ ��� ������ ���������� �� ������ �Ǵ��Ѵ�.
				if (!strcmp(fileHeader->backupset, newFileHeader->backupset) &&
					!strcmp(((struct oracleFilePath *)fileHeader->filepath)->name, ((struct oracleFilePath *)newFileHeader->filepath)->name) && 
					*writeFileSize == (__uint64)newFileHeader->st.size)
				{
					CloseRestoreFile(fdRestore, restorePath, fileHeader, 1);
				}
				else
				{
					CloseRestoreFile(fdRestore, restorePath, fileHeader, 0);
				}
			}
			else
			{
				CloseRestoreFile(fdRestore, restorePath, fileHeader, 0);
			}
		}
	}
	// ����� �������� ��� file header tail�� ���� ���� ������ �Ϸ�Ǿ���� �Ѵ�.
	else
	{
		// ������ �� ��� ������ ���� ũ�� ��ŭ�� �����Ͱ� ������� ���Ͽ� ������ ��� ������ ���������� �� ������ �Ǵ��Ѵ�.
		if (!strcmp(fileHeader->backupset, newFileHeader->backupset) &&
			!strcmp(((struct oracleFilePath *)fileHeader->filepath)->name, ((struct oracleFilePath *)newFileHeader->filepath)->name) && 
			*writeFileSize == (__uint64)newFileHeader->st.size)
		{
			CloseRestoreFile(fdRestore, restorePath, fileHeader, 1);
		}
		else
		{
			CloseRestoreFile(fdRestore, restorePath, fileHeader, 0);
		}
	}
}

static void CloseRestoreFile(va_fd_t fdRestore, char * restorePath, struct fileHeader * fileHeader, int restoreSuccess)
{
	if (fdRestore != (va_fd_t)-1)
	{
		va_close(fdRestore);
		
		// client���� �����ؼ� ����� ������ ������ ���� ��� ����� ������ �����͸� ������ ���۸� �����Ѵ�.
		if (fileHeader->compress != 0)
		{
			isCompressed = 0;
			compressedSize = 0;
			
			#ifdef __ABIO_LINUX
				va_free(unalignCompressedBuf);
				unalignCompressedBuf = NULL;
			#else
				va_free(compressedBuf);
			#endif
			compressedBuf = NULL;
			compressedBufSize = 0;
			
			#ifdef __ABIO_LINUX
				va_free(unalignUncompressedBuf);
				unalignUncompressedBuf = NULL;
			#else
				va_free(uncompressedBuf);
			#endif
			uncompressedBuf = NULL;
			uncompressedBufSize = 0;
			
			compressedDiskBufferSize = 0;
			
			compDataType = (enum compressedDataType)0;
			compDataBuffer = NULL;
			compDataBufferSize = 0;
			compDataRemainingSize = 0;
		}
	}
	
	if (fileHeader->filetype == ORACLE12C_REGULAR_DATA_FILE || fileHeader->filetype == ORACLE12C_ARCHIVE_LOG || fileHeader->filetype == ORACLE12C_CONTROL_FILE)
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
		if (fileHeader->st.backupTime == 0)
		{
			PrintRestoreFileResult(((struct oracle12cFilePath *)fileHeader->filepath)->tableSpaceName, ((struct oracle12cFilePath *)fileHeader->filepath)->name, fileHeader->filetype, restorePath, (time_t)atoi(fileHeader->backupset), 0, (enum restoreFailCause)0);
		}
		else
		{
			PrintRestoreFileResult(((struct oracle12cFilePath *)fileHeader->filepath)->tableSpaceName, ((struct oracle12cFilePath *)fileHeader->filepath)->name, fileHeader->filetype, restorePath, (time_t)fileHeader->st.backupTime, 0, (enum restoreFailCause)0);
		}
	}
	else
	{
		if (fdRestore == (va_fd_t)-1)
		{
			if (fileHeader->st.backupTime == 0)
			{
				PrintRestoreFileResult(((struct oracle12cFilePath *)fileHeader->filepath)->tableSpaceName, ((struct oracle12cFilePath *)fileHeader->filepath)->name, fileHeader->filetype, restorePath, (time_t)atoi(fileHeader->backupset), -1, MAKE_FILE_ERROR);
			}
			else
			{
				PrintRestoreFileResult(((struct oracleFilePath *)fileHeader->filepath)->tableSpaceName, ((struct oracle12cFilePath *)fileHeader->filepath)->name, fileHeader->filetype, restorePath, (time_t)fileHeader->st.backupTime, -1, MAKE_FILE_ERROR);
			}
		}
		else
		{
			if (fileHeader->st.backupTime == 0)
			{
				PrintRestoreFileResult(((struct oracle12cFilePath *)fileHeader->filepath)->tableSpaceName, ((struct oracle12cFilePath *)fileHeader->filepath)->name, fileHeader->filetype, restorePath, (time_t)atoi(fileHeader->backupset), -1, WRITE_FILE_ERROR);
			}
			else
			{
				PrintRestoreFileResult(((struct oracle12cFilePath *)fileHeader->filepath)->tableSpaceName, ((struct oracle12cFilePath *)fileHeader->filepath)->name, fileHeader->filetype, restorePath, (time_t)fileHeader->st.backupTime, -1, WRITE_FILE_ERROR);
			}
		}
	}
}



static int SearchRestoreFile(char * backupset, char * sourceFile)
{
	int middle;
	int left;
	int right;
	
	int r;
	
	
	
	middle = 0;
	left = 0;
	right = restoreFileListNumber - 1;
	
	while (right >= left)
	{
		middle = (right+left) / 2;
		
		r = strcmp(backupset, restoreFileList[middle].backupset);
		
		if (r < 0)
		{
			right = middle - 1;
		}
		else if (r > 0)
		{
			left = middle + 1;
		}
		else
		{
			r = strcmp(sourceFile, restoreFileList[middle].source);
			
			if (r < 0)
			{
				right = middle -1;
			}
			else if (r > 0)
			{
				left = middle + 1;
			}
			else
			{
				return middle;
			}
		}
	}
	
	return -1;
}

static void PrintRestoreFileResult(char * tableSpaceName, char * dataFilePath, enum fileType filetype, char * restorePath, time_t backup_t, int result, enum restoreFailCause failCause)
{
	char backupTime[TIME_STAMP_LENGTH];
	
	
	
	if (result == 0)
	{
		commandData.fileNumber++;
	}
	else
	{
		commandData.objectNumber++;
	}
	
	
	#ifdef __ABIO_WIN
		va_backslash_to_slash(dataFilePath);
		va_backslash_to_slash(restorePath);
	#endif
	
	memset(backupTime, 0, sizeof(backupTime));
	va_make_time_stamp(backup_t, backupTime, TIME_STAMP_TYPE_EXTERNAL_EN);
	
	if (filetype == ORACLE12C_REGULAR_DATA_FILE || filetype == ORACLE12C_RAW_DEVICE_DATA_FILE)
	{
		if (result == 0)
		{
			SendJobLog(LOG_MSG_ID_ORACLE12C_RESTORE_DATA_FILE_SUCCESS, tableSpaceName, backupTime, dataFilePath, restorePath, "");
		}
		else
		{
			if (failCause == MAKE_FILE_ERROR)
			{
				SendJobLog(LOG_MSG_ID_ORACLE12C_RESTORE_DATA_FILE_FAIL_MAKE, tableSpaceName, backupTime, dataFilePath, restorePath, "");
			}
			else if (failCause == WRITE_FILE_ERROR)
			{
				SendJobLog(LOG_MSG_ID_ORACLE12C_RESTORE_DATA_FILE_FAIL_WRITE, tableSpaceName, backupTime, dataFilePath, restorePath, "");
			}
		}
	}
	else if (filetype == ORACLE12C_ARCHIVE_LOG)
	{
		if (result == 0)
		{
			SendJobLog(LOG_MSG_ID_ORACLE12C_RESTORE_ARCHIVE_LOG_SUCCESS, backupTime, dataFilePath, restorePath, "");
		}
		else
		{
			if (failCause == MAKE_FILE_ERROR)
			{
				SendJobLog(LOG_MSG_ID_ORACLE12C_RESTORE_ARCHIVE_LOG_FAIL_MAKE, backupTime, dataFilePath, restorePath, "");
			}
			else if (failCause == WRITE_FILE_ERROR)
			{
				SendJobLog(LOG_MSG_ID_ORACLE12C_RESTORE_ARCHIVE_LOG_FAIL_WRITE, backupTime, dataFilePath, restorePath, "");
			}
		}
	}
	else if (filetype == ORACLE12C_CONTROL_FILE)
	{
		if (result == 0)
		{
			SendJobLog(LOG_MSG_ID_ORACLE12C_RESTORE_CONTROL_FILE_SUCCESS, backupTime, dataFilePath, restorePath, "");
		}
		else
		{
			if (failCause == MAKE_FILE_ERROR)
			{
				SendJobLog(LOG_MSG_ID_ORACLE12C_RESTORE_CONTROL_FILE_FAIL_MAKE, backupTime, dataFilePath, restorePath, "");
			}
			else if (failCause == WRITE_FILE_ERROR)
			{
				SendJobLog(LOG_MSG_ID_ORACLE12C_RESTORE_CONTROL_FILE_FAIL_WRITE, backupTime, dataFilePath, restorePath, "");
			}
		}
	}
	
	#ifdef __ABIO_WIN
		va_slash_to_backslash(dataFilePath);
		va_slash_to_backslash(restorePath);
	#endif
}

static int CheckRestoreFile(struct fileHeader * fileHeader, char * restorePath)
{
	int k;
	
	
	
	if ((k = SearchRestoreFile(fileHeader->backupset, ((struct oracle12cFilePath *)fileHeader->filepath)->name)) >= 0)
	{
		strcpy(restorePath, restoreFileList[k].target);
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

static int WriteToRawDeviceFile(va_fd_t fd, char * dataBuffer, __uint64 * writeFileSize, int flush)
{
	int writeError;
	
	
	
	writeError = 0;
	
	if (flush == 0)
	{
		// data buffer���� ��������� �����͸� �����´�.
		memcpy(writeBuffer + writeBufferSize, dataBuffer, DSIZ);
		writeBufferSize += DSIZ;
		
		// ���۰� ���� á���� ���Ͽ� ����.
		if (writeBufferSize == writeDiskBufferSize)
		{
			// ���Ͽ� ������� �����͸� ����.
			if (va_write(fd, writeBuffer, writeBufferSize, DATA_TYPE_NOT_CHANGE) == writeBufferSize)
			{
				commandData.filesSize += writeBufferSize;
				*writeFileSize += writeBufferSize;
			}
			else
			{
				writeError = 1;
			}
			
			// make a data buffer empty
			writeBufferSize = 0;
			memset(writeBuffer, 0, writeDiskBufferSize);
		}
	}
	else
	{
		// ���Ͽ� ������� �����͸� ����.
		if (va_write(fd, writeBuffer, writeBufferSize, DATA_TYPE_NOT_CHANGE) == writeBufferSize)
		{
			commandData.filesSize += writeBufferSize;
			*writeFileSize += writeBufferSize;
		}
		else
		{
			writeError = 1;
		}
		
		// make a data buffer empty
		writeBufferSize = 0;
		memset(writeBuffer, 0, writeDiskBufferSize);
	}
	
	
	if (writeError == 0)
	{
		return 0;
	}
	else
	{
		return -1;
	}
}
#if defined(__ABIO_AIX) || defined(__ABIO_SOLARIS) || defined(__ABIO_HP) || defined(__ABIO_LINUX)
	static va_fd_t MakeRestoreRawDeviceFile(char * restorePath)
	{
		va_fd_t fd;
		char filesystemPath[MAX_PATH_LENGTH];
		
		struct abio_file_stat file_stat;
		struct abio_file_stat linkStat;
		char linkPath[MAX_PATH_LENGTH];
		
		int i;
		
		
		
		memset(filesystemPath, 0, sizeof(filesystemPath));
		memset(linkPath, 0, sizeof(linkPath));
		
		
		// ��������Ϸ��� raw device file�� ���� ��� ������ �����Ѵ�.
		if (va_lstat(NULL, restorePath, &file_stat) != 0)
		{
			return (va_fd_t)-1;
		}
		
		// symbolic link file�� ��� link ��ΰ� raw device file���� Ȯ���Ѵ�.
		if (va_is_symlink(file_stat.mode, file_stat.windowsAttribute))
		{
			// link ������ status�� �����´�.
			if (va_stat(NULL, restorePath, &linkStat) != 0)
			{
				return (va_fd_t)-1;
			}
			
			// link ��ΰ� raw device file�� �ƴϸ� �����Ѵ�.
			if (!S_ISBLK(linkStat.mode) && !S_ISCHR(linkStat.mode))
			{
				return (va_fd_t)-1;
			}
		}
		// ������������� raw device file�� �ƴϸ� �����Ѵ�.
		else if (!S_ISBLK(file_stat.mode) && !S_ISCHR(file_stat.mode))
		{
			return (va_fd_t)-1;
		}
		
		
		// get the restore file system path and open it
		#ifdef __ABIO_AIX
			if (va_is_symlink(file_stat.mode, file_stat.windowsAttribute))
			{
				// link ������ ��θ� �о file system ��θ� ã�Ƴ���.
				if (va_read_symlink(restorePath, linkPath, sizeof(linkPath)) == 0)
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
					return (va_fd_t)-1;
				}
			}
			else
			{
				// get the character special file name from the block special file name
				if (S_ISBLK(file_stat.mode))
				{
					for (i = strlen(restorePath) - 1; i > -1; i--)
					{
						if (restorePath[i] == FILE_PATH_DELIMITER)
						{
							break;
						}
					}
					
					strncpy(filesystemPath, restorePath, i + 1);
					strcat(filesystemPath, "r");
					strcat(filesystemPath, restorePath + i + 1);
				}
				else
				{
					strcpy(filesystemPath, restorePath);
				}
			}
			
			// open a character special raw device file
			if ((fd = va_open_restore_file(NULL, filesystemPath)) != (va_fd_t)-1)
			{
				return fd;
			}
			else
			{
				return (va_fd_t)-1;
			}
		#else
			// open a character special raw device file
			if ((fd = va_open_restore_file(NULL, restorePath)) != (va_fd_t)-1)
			{
				return fd;
			}
			else
			{
				return (va_fd_t)-1;
			}
		#endif
	}
#endif

static va_fd_t WriteToFile(va_fd_t fdRestore, char * dataBuffer, __uint64 * remainingFileSize, __uint64 * writeFileSize, char * restorePath, struct fileHeader * fileHeader)
{
	va_fd_t fd;
	
	
	
	fd = fdRestore;
	
	// regular file�� ��� file size�� HEAD file header�� ��ϵǾ� �ֱ� ������ data buffer�� ��� ���Ͽ� ���� truncate�ϴ� ���� �ƴ϶�
	// file�� ���� ũ�⸸ŭ�� data buffer���� �о ���Ͽ� ����.
	// �̿� ���� raw device file�̳� ������ ������ ��� HEAD file header�� file size�� ����� �Ǿ� ���� �ʱ� ������
	// data buffer�� ��� ���Ͽ� ����ϰ� truncate�Ѵ�.
	// �ٸ�, raw device file�� ��쿡�� 1K byte�� ����� ������ ������� �ֱ� ������ truncate�� �ʿ� ����.
	
	
	// ������� ���� �������� ��� ������� �����͸� ���Ͽ� ����.
	if (fileHeader->compress == 0)
	{
		if (fileHeader->filetype == ORACLE12C_REGULAR_DATA_FILE || fileHeader->filetype == ORACLE12C_ARCHIVE_LOG || fileHeader->filetype == ORACLE12C_CONTROL_FILE)
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
		else if (fileHeader->filetype == ORACLE_RAW_DEVICE_DATA_FILE)
		{
			// ������� �����͸� ���Ͽ� ���� ������� ��������� ���� ũ�⸦ ���´�.
			if (WriteToRawDeviceFile(fd, dataBuffer, writeFileSize, 0) < 0)
			{
				CloseRestoreFile(fd, restorePath, fileHeader, 0);
				
				fd = (va_fd_t)-1;
			}
		}
	}
	else
	{
		// ������� �����͸� ���Ͽ� ���� ������� ��������� ���� ũ�⸦ ���´�.
		if (WriteToCompressFile(fd, dataBuffer, writeFileSize) < 0)
		{
			CloseRestoreFile(fd, restorePath, fileHeader, 0);
			
			fd = (va_fd_t)-1;
		}
	}
	
	
	return fd;
}

static int WriteToCompressFile(va_fd_t fd, char * dataBuffer, __uint64 * writeFileSize)
{
	int dataBufferSize;
	
	char * writeData;
	int writeSize;
	
	int writeError;
	
	
	
	dataBufferSize = 0;
	writeError = 0;
	
	while (1)
	{
		// data buffer���� ��������� �����͸� �����´�.
		if (compDataRemainingSize <= DSIZ - dataBufferSize)
		{
			memcpy(compDataBuffer + compDataBufferSize, dataBuffer + dataBufferSize, compDataRemainingSize);
			dataBufferSize += compDataRemainingSize;
			compDataBufferSize += compDataRemainingSize;
			compDataRemainingSize -= compDataRemainingSize;
		}
		else
		{
			memcpy(compDataBuffer + compDataBufferSize, dataBuffer + dataBufferSize, DSIZ - dataBufferSize);
			compDataBufferSize += DSIZ - dataBufferSize;
			compDataRemainingSize -= DSIZ - dataBufferSize;
			dataBufferSize += DSIZ - dataBufferSize;
		}
		
		// compressed data buffer�� �������� ����� �������� ���� ������ �а�, �����͸� ������� ���Ͽ� ����.
		if (compDataRemainingSize == 0)
		{
			// �̹��� ���� �����Ͱ� ��� �������� ���� ���θ� ��Ÿ����, �������� ����� �������� ũ�⸦ �д´�.
			if (compDataType == IS_COMPRESSED)
			{
				isCompressed = htonl(isCompressed);		// network to host
				
				// ������ ���� �����͸� ����� �������� ũ��� �����Ѵ�.
				compDataType = COMPRESSED_SIZE;
				compDataBuffer = (char *)&compressedSize;
				compDataBufferSize = 0;
				compDataRemainingSize = sizeof(int);
			}
			// �̹��� ���� �����Ͱ� ��� �������� ����� ũ�⸦ ��Ÿ����, �������� ����� ��� �����͸� �д´�.
			else if (compDataType == COMPRESSED_SIZE)
			{
				// ���� ���ΰ� ������ ���� ǥ���ϰ� ������ �����Ѵ�.
				if (isCompressed == 1)
				{
					// ������ ���� �����͸� ��� �������� ���� ���η� �����Ѵ�.
					compDataType = IS_COMPRESSED;
					compDataBuffer = (char *)&isCompressed;
					compDataBufferSize = 0;
					compDataRemainingSize = sizeof(int);
					
					break;
				}
				else
				{
					compressedSize = htonl(compressedSize);		// network to host
					compressedBufSize = (unsigned zlib_long_t)compressedSize;
					
					// ������ ���� �����͸� ����� ��� �����ͷ� �����Ѵ�.
					compDataType = COMPRESSED_BUFFER;
					compDataBuffer = (char *)compressedBuf;
					compDataBufferSize = 0;
					compDataRemainingSize = compressedSize;
				}
			}
			// �̹��� ���� �����Ͱ� ����� ��� �������̸� ������ Ǯ�� ������� ���Ͽ� ����.
			else if (compDataType == COMPRESSED_BUFFER)
			{
				// ������ ���� �����͸� ��� �������� ���� ���η� �����Ѵ�.
				compDataType = IS_COMPRESSED;
				compDataBuffer = (char *)&isCompressed;
				compDataBufferSize = 0;
				compDataRemainingSize = sizeof(int);
				
				// ������ Ǯ�� ������� ���Ͽ� ����.
				if (isCompressed == 0)
				{
					// uncompress
					uncompressedBufSize = (unsigned zlib_long_t)compressedDiskBufferSize;
					if (uncompress(uncompressedBuf, &uncompressedBufSize, compressedBuf, compressedBufSize) < 0)
					{
						writeError = 1;
						
						break;
					}
					
					writeData = (char *)uncompressedBuf;
					writeSize = (int)uncompressedBufSize;
				}
				else
				{
					writeData = (char *)compressedBuf;
					writeSize = (int)compressedBufSize;
				}
				
				if (va_write(fd, writeData, writeSize, DATA_TYPE_NOT_CHANGE) == writeSize)
				{
					commandData.filesSize += writeSize;
					*writeFileSize += writeSize;
				}
				else
				{
					writeError = 1;
					
					break;
				}
			}
		}
		
		// data buffer�� �����͸� ��� ���� ��� �����Ѵ�.
		if (dataBufferSize == DSIZ)
		{
			break;
		}
	}
	
	
	if (writeError == 0)
	{
		return 0;
	}
	else
	{
		return -1;
	}
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
		// encryption
		//if (encryption == 1)
		//{
		//	va_aes_decrypt(dataBuffer,dataBuffer,(int)remainSize);
		//}

		memcpy(writeBuffer + writeBufferSize, dataBuffer, (int)remainSize);
		writeBufferSize += (int)remainSize;
		remainSize -= remainSize;
	}
	else
	{
		// encryption
		//if (encryption == 1)
		//{
		//	va_aes_decrypt(dataBuffer,dataBuffer,DSIZ);
		//}

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
static va_fd_t MakeRestoreFile(char * restorePath, struct fileHeader * fileHeader, __uint64 * remainingFileSize)
{
	va_fd_t fd;
	
	
	
	fd = (va_fd_t)-1;
	
	if (fileHeader->filetype == ORACLE12C_REGULAR_DATA_FILE || fileHeader->filetype == ORACLE12C_ARCHIVE_LOG || fileHeader->filetype == ORACLE12C_CONTROL_FILE)
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
	else if (fileHeader->filetype == ORACLE12C_RAW_DEVICE_DATA_FILE)
	{
		#if defined(__ABIO_AIX) || defined(__ABIO_SOLARIS) || defined(__ABIO_HP) || defined(__ABIO_LINUX)
			if ((fd = MakeRestoreRawDeviceFile(restorePath)) == (va_fd_t)-1)
			{
				CloseRestoreFile(fd, restorePath, fileHeader, 0);
			}
		#else
			// �̿��� �ý��ۿ����� raw device file ����� �������� �ʴ´�.
		#endif
	}
	
	
	// client���� �����ؼ� ����� ������ ��� ����� ������ �����͸� �����ϱ� ���� ���۸� �����.
	if (fileHeader->compress != 0)
	{
		if (fd != (va_fd_t)-1)
		{
			isCompressed = 0;
			compressedSize = 0;
			
			if (fileHeader->st.compressedDiskBufferSize != 0)
			{
				compressedDiskBufferSize = fileHeader->st.compressedDiskBufferSize;
			}
			else
			{
				compressedDiskBufferSize = DISK_BUFFER_SIZE;
			}
			
			#ifdef __ABIO_LINUX
				unalignCompressedBuf = (unsigned char *)malloc(sizeof(char) * (compressedDiskBufferSize * 2 * 2));
				memset(unalignCompressedBuf, 0, sizeof(char) * (compressedDiskBufferSize * 2 * 2));
				
				compressedBuf = va_ptr_align(unalignCompressedBuf, compressedDiskBufferSize * 2);
			#else
				compressedBuf = (unsigned char *)malloc(sizeof(char) * (compressedDiskBufferSize * 2));
				memset(compressedBuf, 0, sizeof(char) * (compressedDiskBufferSize * 2));
			#endif
			compressedBufSize = 0;
			
			#ifdef __ABIO_LINUX
				unalignUncompressedBuf = (unsigned char *)malloc(sizeof(char) * (compressedDiskBufferSize * 2));
				memset(unalignUncompressedBuf, 0, sizeof(char) * (compressedDiskBufferSize * 2));
				
				uncompressedBuf = va_ptr_align(unalignUncompressedBuf, compressedDiskBufferSize);
			#else
				uncompressedBuf = (unsigned char *)malloc(sizeof(char) * compressedDiskBufferSize);
				memset(uncompressedBuf, 0, sizeof(char) * compressedDiskBufferSize);
			#endif
			uncompressedBufSize = 0;
			
			compDataType = IS_COMPRESSED;
			compDataBuffer = (char *)&isCompressed;
			compDataBufferSize = 0;
			compDataRemainingSize = sizeof(int);
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