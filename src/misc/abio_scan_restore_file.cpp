#include "require.h"
#include "abio.h"
#include "lib.h"
#include "vendor.h"
#include "abio_scan.h"


extern enum volumeType mountedVolumeType;

extern char driveDeviceFile[STORAGE_DEVICE_FILE_LENGTH];
extern char diskVolumeFile[MAX_PATH_LENGTH];

extern char mountedVolume[VOLUME_NAME_LENGTH];
extern int volumeIdentify;		// 0 : didn't read yet. 1 : virbak abio type. 2 : cannot identify
extern int volumeBlockSize;
extern int dataBufferSize;

extern struct backupsetInfo * backupsetList;
extern int backupsetListNumber;

extern char restoreBackupset[BACKUPSET_ID_LENGTH];
extern char restoreClient[NODE_NAME_LENGTH];
extern char restoreDirectory[MAX_PATH_LENGTH];
extern int * restoreFileList;
extern int restoreFileNumber;

extern int successFileNumber;
extern int failFileNumber;


static char writeBuffer[DEFAULT_WRITE_DISK_BUFFER_SIZE];
static int writeBufferSize;

static int isCompressed;										// client���� ����� ������ ������ ����Ǿ����� ����
static int compressedSize;										// client���� ����� ������ ����� ũ��

#ifdef __ABIO_LINUX
	static unsigned char * unalignCompressedBuf;
#endif
static unsigned char * compressedBuf;							// client���� ����� ������ ������
static unsigned zlib_long_t compressedBufSize;					// client���� ����� ������ ����� ũ��

#ifdef __ABIO_LINUX
	static unsigned char * unalignUncompressedBuf;
#endif
static unsigned char * uncompressedBuf;							// client���� ����� ������ �����Ͱ� ���������� ������
static unsigned zlib_long_t uncompressedBufSize;				// client���� ����� ������ �����Ͱ� ���������� ũ��

static int compressedDiskBufferSize;							// client���� �����ؼ� ����� �� ����� disk buffer�� ũ��

static enum compressedDataType compDataType;
static char * compDataBuffer;
static int compDataBufferSize;
static int compDataRemainingSize;


static int restoredFileNumber;


void Restore()
{
	char input[DSIZ];
	
	
	
	// initialize variables
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
	
	restoredFileNumber = 0;
	
	
	if (restoreDirectory[0] == '\0')
	{
		printf("\n");
		printf("Restore destination directory is not set. All files will be restored to its original location.\n");
		printf("And if the restore file is already existed, then it will be replaced.\n");
	}
	
	while (1)
	{
		printf("\n");
		printf("If you want to restore, then input 's', else input 'q' (you can see the restore log in %s.log) : ", restoreBackupset);
		
		gets(input);
		if (strlen(input) == 1)
		{
			if (input[0] == 's' || input[0] == 'S')
			{
				break;
			}
			else if (input[0] == 'q' || input[0] == 'Q')
			{
				return;
			}
		}
	}
	
	
	PrintRestoreStartLog();
	
	RestoreFile();
	
	PrintRestoreResult();
	
	
	// ������� �۾��� ������ ������� ����� �����Ѵ�.
	successFileNumber = 0;
	failFileNumber = 0;
}

void RestoreFile()
{
	va_fd_t fdDrive;
	struct volumeDB volumeDB;
	
	char * volumeBuffer;
	char * dataBuffer;
	
	struct fileHeader fileHeader;
	struct fileHeader newFileHeader;
	
	int restoreComplete;
	
	int fileNumber;
	
	va_fd_t fdRestore;
	__uint64 writeFileSize;
	__uint64 remainingFileSize;
	char restorePath[MAX_PATH_LENGTH];
	
	int i;
	int offset;
	
	
	
	// initialize variables
	volumeBuffer = NULL;
	dataBuffer = NULL;
	
	restoreComplete = 0;
	fileNumber = 0;
	
	fdRestore = (va_fd_t)-1;
	
	
	// open volume
	if (mountedVolumeType == VOLUME_TAPE)
	{
		if ((fdDrive = LoadTapeVolume(&volumeDB)) == (va_fd_t)-1)
		{
			return;
		}
	}
	else
	{
		if ((fdDrive = LoadDiskVolume(&volumeDB)) == (va_fd_t)-1)
		{
			return;
		}
	}
	
	// mount�Ǿ� �ִ� volume�� scan�ߴ� volume�� �ٸ� ��� scan ������ �����Ѵ�.
	if (strcmp(volumeDB.volume, mountedVolume) != 0)
	{
		printf("The tape volume volume is diffrent with before one. Scan backup sets again.\n");
		
		// ������ mount�Ǿ� �ִ� volume ������ �����Ѵ�.
		memset(driveDeviceFile, 0, sizeof(driveDeviceFile));
		memset(diskVolumeFile, 0, sizeof(diskVolumeFile));
		memset(mountedVolume, 0, sizeof(mountedVolume));
		volumeIdentify = 0;
		
		// ������ backup file ����� �����Ѵ�.
		va_free(backupsetList);
		backupsetList = NULL;
		backupsetListNumber = 0;
		
		// ������ �����ߴ� restore file ����� �����Ѵ�.
		memset(restoreBackupset, 0, sizeof(restoreBackupset));
		memset(restoreClient, 0, sizeof(restoreClient));
		memset(restoreDirectory, 0, sizeof(restoreDirectory));
		va_free(restoreFileList);
		restoreFileList = NULL;
		restoreFileNumber = 0;
		
		if (mountedVolumeType == VOLUME_TAPE)
		{
			UnloadTapeVolume(fdDrive);
		}
		else
		{
			UnloadDiskVolume(fdDrive);
		}
		
		return;
	}
	
	// ��������� backupset�� �ִ� ��ġ���� volume�� ���´�.
	printf("\n");
	printf("Positioning...\n\n");
	
	for (i = 0; i < backupsetListNumber; i++)
	{
		if (!strcmp(backupsetList[i].backupset, restoreBackupset))
		{
			if (mountedVolumeType == VOLUME_TAPE)
			{
				if (va_forward_filemark(fdDrive, (int)backupsetList[i].backupsetPosition) != 0)
				{
					printf("Cannot move the tape forward by %d.\n", (int)backupsetList[i].backupsetPosition);
					
					UnloadTapeVolume(fdDrive);
					
					return;
				}
			}
			else
			{
				if (va_lseek(fdDrive, (va_offset_t)backupsetList[i].backupsetPosition, SEEK_CUR) == (va_offset_t)-1)
				{
					printf("Cannot move the disk volume forward by %llu.\n", backupsetList[i].backupsetPosition);
					
					UnloadDiskVolume(fdDrive);
					
					return;
				}
			}
			
			break;
		}
	}
	
	if (i == backupsetListNumber)
	{
		printf("Restore backup set is not in scanned backup sets.\n");
		
		if (mountedVolumeType == VOLUME_TAPE)
		{
			UnloadTapeVolume(fdDrive);
		}
		else
		{
			UnloadDiskVolume(fdDrive);
		}
		
		return;
	}
	
	
	// restore a backup data
	printf("\n");
	printf("Reading...\n\n");
	
	volumeBuffer = (char *)malloc(sizeof(char) * volumeBlockSize);
	dataBuffer = volumeBuffer + VOLUME_BLOCK_HEADER_SIZE;
	
	while (va_read(fdDrive, volumeBuffer, volumeBlockSize, DATA_TYPE_NOT_CHANGE) == volumeBlockSize && restoreComplete == 0)
	{
		// volume block header���� backupset�� �о ��������Ϸ��� volume block�� �´��� Ȯ���Ѵ�.
		if (!strncmp(volumeBuffer, restoreBackupset, strlen(restoreBackupset)))
		{
			for (i = 0; i < dataBufferSize; i += DSIZ)
			{
				if (!strncmp(dataBuffer + i, ABIO_CHECK_CODE, strlen(ABIO_CHECK_CODE)) && 
					!strncmp(dataBuffer + i + ABIO_CHECK_CODE_SIZE, restoreBackupset, strlen(restoreBackupset)))
				{
					// read a file header
					memcpy(&newFileHeader, dataBuffer + i, sizeof(struct fileHeader));
					va_change_byte_order_fileHeader(&newFileHeader);		// network to host
					va_convert_v25_to_v30_fileHeader(&newFileHeader);
					
					// ABIO v3.0 sp0 pl3���� ����� va_backslash_to_slash()�Լ����� D:/ �� ���� ������ ��θ� /D/�� �ٲ��� �ʰ� �ʿ� ���� file backup���� D:\ �� /D/�� ������� �ʰ� D:/�� ����Ǿ���.
					// �׷��� D:/ ������ ��θ� ó���ϱ� ���� �ڵ带 �����Ѵ�.
					if (((struct regularFilePath *)newFileHeader.filepath)->name[0] != DIRECTORY_IDENTIFIER)
					{
						va_backslash_to_slash(((struct regularFilePath *)newFileHeader.filepath)->name);
					}
					
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
						memset(writeBuffer, 0, sizeof(writeBuffer));
						
						
						// read a file header
						memcpy(&fileHeader, &newFileHeader, sizeof(struct fileHeader));
						
						if (fileHeader.filetype == REGULAR_FILE || fileHeader.filetype == RAW_DEVICE || fileHeader.filetype == SYSTEM_OBJECT_FILE || 
							fileHeader.filetype == ORACLE_REGULAR_DATA_FILE || fileHeader.filetype == ORACLE_RAW_DEVICE_DATA_FILE || fileHeader.filetype == ORACLE_ARCHIVE_LOG || fileHeader.filetype == ORACLE_CONTROL_FILE || 
							fileHeader.filetype == ALTIBASE_DATA_FILE || fileHeader.filetype == ALTIBASE_ARCHIVE_LOG || fileHeader.filetype == ALTIBASE_LOG_ANCHOR|| 
							fileHeader.filetype == EXCHANGE_DATABASE_FILE|| fileHeader.filetype == EXCHANGE_LOG_FILE||
							fileHeader.filetype ==MSSQL_DATABASE_FULL||fileHeader.filetype ==MSSQL_DATABASE_DIFFERENTIAL||
							( fileHeader.filetype >= VIRBAK_VMWARE_CONF_FILE && fileHeader.filetype <= VIRBAK_VMWARE_DATA_FILE) ||
							 fileHeader.filetype == SHAREPOINT_DUMP_FILE)
						{
							fileNumber++;
							
							// ������ ������� ������� Ȯ���ϰ�, restore path�� �����.
							if (CheckRestoreFile(&fileHeader, fileNumber, restorePath) < 0)
							{
								continue;
							}
							
							// ���� Ÿ�Կ� ���� ������ �����.
							fdRestore = MakeRestoreFile(fileNumber, restorePath, &fileHeader, &remainingFileSize);
						}
					}
					// file header�� tail�̸� ������ ��� ���������� Ȯ���ϰ�, ���� ũ�⸦ ������ ���� ũ��� �������ش�.
					else if (newFileHeader.headerType == FILE_HEADER_TAIL)
					{
						if (fdRestore != (va_fd_t)-1)
						{
							CloseFile(fdRestore, &fileHeader, &newFileHeader, &writeFileSize, restorePath);
							
							fdRestore = (va_fd_t)-1;
						}
						
						if (restoreFileNumber > 0 && restoreFileNumber == restoredFileNumber)
						{
							restoreComplete = 1;
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
						
						// restore buffer�� ���Ͽ� ����Ѵ�.
						fdRestore = WriteToFile(fdRestore, dataBuffer + offset, &remainingFileSize, &writeFileSize, restorePath, &fileHeader);
					}
				}
			}
		}
	}
	
	// �ҿ����ϰ� ������� �� ������ ó��
	if (fdRestore != (va_fd_t)-1)
	{
		CloseRestoreFile(fdRestore, restorePath, &fileHeader, 0);
	}
	
	
	// unload volume
	va_free(volumeBuffer);
	
	if (mountedVolumeType == VOLUME_TAPE)
	{
		UnloadTapeVolume(fdDrive);
	}
	else
	{
		UnloadDiskVolume(fdDrive);
	}
}

int CheckRestoreFile(struct fileHeader * fileHeader, int fileNumber, char * restorePath)
{
	char * filePath;
	char * directoryPath;
	
	int isRestoreFile;
	
	int i;

	#ifdef __ABIO_UNIX
	char * convertFile;
	#elif __ABIO_WIN
		wchar_t * convertFile;
	#endif
	int convertFileSize;
	convertFile = NULL;
	convertFileSize = 0;	

	
	// ��� ������ ������� ������� Ȯ���Ѵ�.
	isRestoreFile = 0;
	
	// find restore file number
	if (restoreFileNumber > 0)
	{
		if (SearchRestoreFile(fileNumber) >= 0)
		{
			isRestoreFile = 1;
		}
	}
	else
	{
		isRestoreFile = 1;
	}
	
	if (isRestoreFile == 1)
	{
		if (fileHeader->filetype == REGULAR_FILE || fileHeader->filetype == SYSTEM_OBJECT_FILE || fileHeader->filetype == RAW_DEVICE)
		{
			filePath = ((struct regularFilePath *)fileHeader->filepath)->name;
		}
		else if (fileHeader->filetype == ORACLE_REGULAR_DATA_FILE || fileHeader->filetype == ORACLE_ARCHIVE_LOG || fileHeader->filetype == ORACLE_CONTROL_FILE || fileHeader->filetype == ORACLE_RAW_DEVICE_DATA_FILE)
		{
			filePath = ((struct oracleFilePath *)fileHeader->filepath)->name;
		}
		else if (fileHeader->filetype == EXCHANGE_DATABASE_FILE || fileHeader->filetype == EXCHANGE_LOG_FILE)
		{
			filePath = ((struct exchangeFilePath *)fileHeader->filepath)->databaseFile;
		}
		else if(fileHeader->filetype ==MSSQL_DATABASE_FULL ||fileHeader->filetype ==MSSQL_DATABASE_DIFFERENTIAL)
		{
			filePath = ((struct mssqlFilePath *)fileHeader->filepath)->databaseName;
		}
		else if (fileHeader->filetype == ALTIBASE_DATA_FILE || fileHeader->filetype == ALTIBASE_ARCHIVE_LOG || fileHeader->filetype == ALTIBASE_LOG_ANCHOR)
		{
			filePath = ((struct altibaseFilePath *)fileHeader->filepath)->name;
		}
		else if (fileHeader->filetype >= VIRBAK_VMWARE_CONF_FILE && fileHeader->filetype <= VIRBAK_VMWARE_DATA_FILE)
		{
			filePath = ((struct vmwareFilePath *)fileHeader->filepath)->file;
		}
		else if( fileHeader->filetype == SHAREPOINT_DUMP_FILE)
		{
			filePath = ((struct sharepointFilePath *)fileHeader->filepath)->name;
			directoryPath = ((struct sharepointFilePath *)fileHeader->filepath)->Url;
		}
		
		// regular file�� restore directory�� ������ ��쿡 ��ü ��� ��θ� restore directory�� �����Ѵ�.
		if (fileHeader->filetype == REGULAR_FILE||fileHeader->filetype == EXCHANGE_DATABASE_FILE || fileHeader->filetype == EXCHANGE_LOG_FILE||
			fileHeader->filetype ==MSSQL_DATABASE_FULL||fileHeader->filetype ==MSSQL_DATABASE_DIFFERENTIAL)
		{
			if (restoreDirectory[0] == '\0')
			{
				strcpy(restorePath, filePath);
				
				#ifdef __ABIO_WIN
					va_slash_to_backslash(restorePath);
				#endif
			}
			else
			{
				#ifdef __ABIO_UNIX
					if (strlen(restoreDirectory) == 1)
					{
						strcpy(restorePath, filePath);
					}
					else
					{
						if (va_convert_string_to_utf8(ENCODING_UNKNOWN, restoreDirectory, (int)strlen(restoreDirectory), (void **)&convertFile, &convertFileSize) > 0)
						{
							sprintf(restorePath, "%s%s", restoreDirectory, filePath);
						}
					}
				#elif __ABIO_WIN
					if (strlen(restoreDirectory) < 4)
					{
						strcpy(restorePath, filePath);
						va_slash_to_backslash(restorePath);
						restorePath[0] = restoreDirectory[0];
					}
					else
					{
						va_slash_to_backslash(filePath);

						//restoreDirectory �� �����Ǿ��ִ� ��� �Է¹��� ���� ���ڵ��Ѵ�.
						if (va_convert_string_to_utf8(ENCODING_CP949, restoreDirectory, (int)strlen(restoreDirectory), (void **)&convertFile, &convertFileSize) > 0)
						{
							if(fileHeader->filetype ==MSSQL_DATABASE_FULL||fileHeader->filetype ==MSSQL_DATABASE_DIFFERENTIAL)
							{
								sprintf(restorePath, "%s\\%s.bak", convertFile, filePath);
							}
							//Windows �������� ��� D: �� ���� ������ ��θ� �����ϰ� ��θ� �����Ѵ�.
							else if (filePath[0] != DIRECTORY_IDENTIFIER)
							{							
								sprintf(restorePath, "%s%s", convertFile, filePath + 2);
							}
							else
							{
								sprintf(restorePath, "%s%s", convertFile, filePath);
							}
						}
						
						va_backslash_to_slash(filePath);
						va_backslash_to_slash(restorePath);
					}
				#endif
			}
		}
		// windows system object file, oracle regular file, altibase file�� restore directory�� ������ ��쿡 file name�� restore directory�� �����Ѵ�.
		else if (fileHeader->filetype == SYSTEM_OBJECT_FILE || 
				fileHeader->filetype == ORACLE_REGULAR_DATA_FILE || fileHeader->filetype == ORACLE_ARCHIVE_LOG || fileHeader->filetype == ORACLE_CONTROL_FILE || 
				fileHeader->filetype == ALTIBASE_DATA_FILE || fileHeader->filetype == ALTIBASE_ARCHIVE_LOG || fileHeader->filetype == ALTIBASE_LOG_ANCHOR ||
				(fileHeader->filetype >= VIRBAK_VMWARE_CONF_FILE && fileHeader->filetype <= VIRBAK_VMWARE_DATA_FILE) )
		{
			if (restoreDirectory[0] == '\0')
			{
				strcpy(restorePath, filePath);
				
				#ifdef __ABIO_WIN
					va_slash_to_backslash(restorePath);
				#endif
			}
			else
			{
				for (i = (int)strlen(filePath) - 1; i > -1; i--)
				{
					if (filePath[i] == DIRECTORY_IDENTIFIER)
					{
						break;
					}
				}
				
				#ifdef __ABIO_UNIX
					if (strlen(restoreDirectory) == 1)
					{
						sprintf(restorePath, "%s%s", restoreDirectory, filePath + i + 1);
					}
					else
					{
						sprintf(restorePath, "%s%c%s", restoreDirectory, FILE_PATH_DELIMITER, filePath + i + 1);
					}
				#elif __ABIO_WIN
					if (strlen(restoreDirectory) == 3)
					{
						sprintf(restorePath, "%s%s", restoreDirectory, filePath + i + 1);
					}
					else
					{
						sprintf(restorePath, "%s%c%s", restoreDirectory, FILE_PATH_DELIMITER, filePath + i + 1);
					}
				#endif
			}
		}
		else if(fileHeader->filetype == SHAREPOINT_DUMP_FILE)
		{
			if (restoreDirectory[0] == '\0')
			{
				strcpy(restorePath, filePath);
				
				#ifdef __ABIO_WIN
					va_slash_to_backslash(restorePath);
				#endif
			}
			else
			{
				#ifdef __ABIO_WIN
					va_backslash_to_slash(restoreDirectory);
				#endif

				sharepointRestorePath(directoryPath,filePath,restoreDirectory,restorePath);				

				#ifdef __ABIO_WIN
					va_slash_to_backslash(restorePath);
				#endif
			}
			
		}
		// raw device file�� ���� ��ο� �����Ѵ�.
		else
		{
			strcpy(restorePath, filePath);
		}
		
		
		restoredFileNumber++;
		
		return 0;
	}
	else
	{
		return -1;
	}
}

va_fd_t MakeRestoreFile(int fileNumber, char * restorePath, struct fileHeader * fileHeader, __uint64 * remainingFileSize)
{
	va_fd_t fd;
	char * filePath;
	
	
	
	fd = (va_fd_t)-1;
	
	
	if (fileHeader->filetype == REGULAR_FILE || fileHeader->filetype == SYSTEM_OBJECT_FILE || fileHeader->filetype == RAW_DEVICE)
	{
		filePath = ((struct regularFilePath *)fileHeader->filepath)->name;
	}
	else if (fileHeader->filetype == ORACLE_REGULAR_DATA_FILE || fileHeader->filetype == ORACLE_ARCHIVE_LOG || fileHeader->filetype == ORACLE_CONTROL_FILE || fileHeader->filetype == ORACLE_RAW_DEVICE_DATA_FILE)
	{
		filePath = ((struct oracleFilePath *)fileHeader->filepath)->name;
	}
	else if (fileHeader->filetype == EXCHANGE_DATABASE_FILE || fileHeader->filetype == EXCHANGE_LOG_FILE)
	{
		filePath = ((struct exchangeFilePath *)fileHeader->filepath)->databaseFile;
	}
	else if(fileHeader->filetype ==MSSQL_DATABASE_FULL||fileHeader->filetype ==MSSQL_DATABASE_DIFFERENTIAL)
	{
		filePath = ((struct mssqlFilePath *)fileHeader->filepath)->databaseName;
	}
	else if (fileHeader->filetype == ALTIBASE_DATA_FILE || fileHeader->filetype == ALTIBASE_ARCHIVE_LOG || fileHeader->filetype == ALTIBASE_LOG_ANCHOR)
	{
		filePath = ((struct altibaseFilePath *)fileHeader->filepath)->name;
	}
	else if ( fileHeader->filetype >= VIRBAK_VMWARE_CONF_FILE && fileHeader->filetype <= VIRBAK_VMWARE_DATA_FILE)
	{
		filePath = ((struct vmwareFilePath *)fileHeader->filepath)->file;
	}
	else if (  fileHeader->filetype == SHAREPOINT_DUMP_FILE )
	{
		filePath = ((struct sharepointFilePath *)fileHeader->filepath)->name;
	}
	
	PrintRestoreFileLog(fileNumber, filePath, restorePath);
	
	
	if (fileHeader->filetype == REGULAR_FILE || fileHeader->filetype == SYSTEM_OBJECT_FILE || 
		fileHeader->filetype == ORACLE_REGULAR_DATA_FILE || fileHeader->filetype == ORACLE_ARCHIVE_LOG || fileHeader->filetype == ORACLE_CONTROL_FILE || 
		fileHeader->filetype == ALTIBASE_DATA_FILE || fileHeader->filetype == ALTIBASE_ARCHIVE_LOG || fileHeader->filetype == ALTIBASE_LOG_ANCHOR|| 
		fileHeader->filetype == EXCHANGE_DATABASE_FILE|| fileHeader->filetype == EXCHANGE_LOG_FILE ||
		( fileHeader->filetype >= VIRBAK_VMWARE_CONF_FILE && fileHeader->filetype <= VIRBAK_VMWARE_DATA_FILE) ||
		 fileHeader->filetype == SHAREPOINT_DUMP_FILE)
	{
		if ((fd = MakeRestoreRegularFile(restorePath, fileHeader)) != (va_fd_t)-1)
		{
			// file size�� 0�̸� �ٷ� ������ ������.
			if (fileHeader->st.size == 0)
			{
				CloseRestoreFile(fd, restorePath, fileHeader, 1);
				
				fd = (va_fd_t)-1;
			}
			else
			{
				*remainingFileSize = fileHeader->st.size;
			}
		}
		else
		{
			CloseRestoreFile(fd, restorePath, fileHeader, 0);
		}
	}
	else if(fileHeader->filetype ==MSSQL_DATABASE_FULL|| fileHeader->filetype ==MSSQL_DATABASE_DIFFERENTIAL )
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
	else
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

va_fd_t MakeRestoreRegularFile(char * restorePath, struct fileHeader * fileHeader)
{
	va_fd_t fd;
	struct abio_file_stat file_stat;
	
	
	
	// restore�Ϸ��� file�� �̹� ������ ��� �����ϰ� ���� �����.
	if (va_stat(NULL, restorePath, &file_stat) == 0)
	{
		va_remove(NULL, restorePath);
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

#if defined(__ABIO_AIX) || defined(__ABIO_SOLARIS) || defined(__ABIO_HP) || defined(__ABIO_LINUX)
	va_fd_t MakeRestoreRawDeviceFile(char * restorePath)
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

va_fd_t WriteToFile(va_fd_t fdRestore, char * dataBuffer, __uint64 * remainingFileSize, __uint64 * writeFileSize, char * restorePath, struct fileHeader * fileHeader)
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
		if (fileHeader->filetype == REGULAR_FILE || fileHeader->filetype == SYSTEM_OBJECT_FILE || 
			fileHeader->filetype == ORACLE_REGULAR_DATA_FILE || fileHeader->filetype == ORACLE_ARCHIVE_LOG || fileHeader->filetype == ORACLE_CONTROL_FILE || 
			fileHeader->filetype == ALTIBASE_DATA_FILE || fileHeader->filetype == ALTIBASE_ARCHIVE_LOG || fileHeader->filetype == ALTIBASE_LOG_ANCHOR|| 
			fileHeader->filetype == EXCHANGE_DATABASE_FILE|| fileHeader->filetype == EXCHANGE_LOG_FILE ||
			( fileHeader->filetype >= VIRBAK_VMWARE_CONF_FILE && fileHeader->filetype <= VIRBAK_VMWARE_DATA_FILE) ||
			 fileHeader->filetype == SHAREPOINT_DUMP_FILE)
		{
			if (WriteToRegularFile(fd, dataBuffer, remainingFileSize,fileHeader->encryption) == 0)
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
		else if(fileHeader->filetype ==MSSQL_DATABASE_FULL|| fileHeader->filetype ==MSSQL_DATABASE_DIFFERENTIAL)
		{
			// ������� �����͸� ���Ͽ� ���� ������� ��������� ���� ũ�⸦ ���´�.
			if (WriteToRawDeviceFile(fd, dataBuffer, writeFileSize, 0) < 0)
			{
				CloseRestoreFile(fd, restorePath, fileHeader, 0);
				
				fd = (va_fd_t)-1;
			}
		}
		else
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
		if (WriteToCompressFile(fd, dataBuffer, writeFileSize,fileHeader->encryption) < 0)
		{
			CloseRestoreFile(fd, restorePath, fileHeader, 0);
			
			fd = (va_fd_t)-1;
		}
	}
	
	
	return fd;
}

int WriteToRegularFile(va_fd_t fd, char * dataBuffer, __uint64 * remainingFileSize, int encryption)
{
	__uint64 remainSize;
	int writeError;
	
	
	
	remainSize = *remainingFileSize;
	writeError = 0;
	
	// data buffer���� ��������� �����͸� �����´�.
	if (remainSize <= DSIZ)
	{
		if(encryption == 1)
		{								
			va_aes_encrypt(dataBuffer,dataBuffer,(int)remainSize);
		}	

		memcpy(writeBuffer + writeBufferSize, dataBuffer, (int)remainSize);
		writeBufferSize += (int)remainSize;
		remainSize -= remainSize;
	}
	else
	{
		if(encryption == 1)
		{				
			va_aes_encrypt(dataBuffer,dataBuffer,DSIZ);			
		}	

		memcpy(writeBuffer + writeBufferSize, dataBuffer, DSIZ);
		writeBufferSize += DSIZ;
		remainSize -= DSIZ;
	}
	
	// ������ ������ block�̰ų� ���۰� ���� á���� ���Ͽ� ����.
	if (remainSize == 0 || writeBufferSize == sizeof(writeBuffer))
	{
		// ���Ͽ� ������� �����͸� ����.
		if (va_write(fd, writeBuffer, writeBufferSize, DATA_TYPE_NOT_CHANGE) < writeBufferSize)
		{
			writeError = 1;
		}
		
		// make a data buffer empty
		writeBufferSize = 0;
		memset(writeBuffer, 0, sizeof(writeBuffer));
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

int WriteToRawDeviceFile(va_fd_t fd, char * dataBuffer, __uint64 * writeFileSize, int flush)
{
	int writeError;
	
	
	
	writeError = 0;
	
	if (flush == 0)
	{
		// data buffer���� ��������� �����͸� �����´�.
		memcpy(writeBuffer + writeBufferSize, dataBuffer, DSIZ);
		writeBufferSize += DSIZ;
		
		// ���۰� ���� á���� ���Ͽ� ����.
		if (writeBufferSize == sizeof(writeBuffer))
		{
			// ���Ͽ� ������� �����͸� ����.
			if (va_write(fd, writeBuffer, writeBufferSize, DATA_TYPE_NOT_CHANGE) == writeBufferSize)
			{
				*writeFileSize += writeBufferSize;
			}
			else
			{
				writeError = 1;
			}
			
			// make a data buffer empty
			writeBufferSize = 0;
			memset(writeBuffer, 0, sizeof(writeBuffer));
		}
	}
	else
	{
		// ���Ͽ� ������� �����͸� ����.
		if (va_write(fd, writeBuffer, writeBufferSize, DATA_TYPE_NOT_CHANGE) == writeBufferSize)
		{
			*writeFileSize += writeBufferSize;
		}
		else
		{
			writeError = 1;
		}
		
		// make a data buffer empty
		writeBufferSize = 0;
		memset(writeBuffer, 0, sizeof(writeBuffer));
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

int WriteToCompressFile(va_fd_t fd, char * dataBuffer, __uint64 * writeFileSize,int encryption)
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
					if(encryption == 1)
					{
						aes_encrypt(compressedBuf,compressedBuf,(int)compressedBufSize);							
					}

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
					if(encryption == 1)
					{
						aes_encrypt(compressedBuf,compressedBuf,(int)compressedBufSize);	
					}

					writeData = (char *)compressedBuf;
					writeSize = (int)compressedBufSize;
				}
				
				if (va_write(fd, writeData, writeSize, DATA_TYPE_NOT_CHANGE) == writeSize)
				{
					
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

void CloseFile(va_fd_t fdRestore, struct fileHeader * fileHeader, struct fileHeader * newFileHeader, __uint64 * writeFileSize, char * restorePath)
{
	char * filePath;
	char * newFilePath;
	
	
	
	if (fileHeader->filetype == REGULAR_FILE || fileHeader->filetype == SYSTEM_OBJECT_FILE || fileHeader->filetype == RAW_DEVICE)
	{
		filePath = ((struct regularFilePath *)fileHeader->filepath)->name;
		newFilePath = ((struct regularFilePath *)newFileHeader->filepath)->name;
	}
	else if (fileHeader->filetype == ORACLE_REGULAR_DATA_FILE || fileHeader->filetype == ORACLE_ARCHIVE_LOG || fileHeader->filetype == ORACLE_CONTROL_FILE || fileHeader->filetype == ORACLE_RAW_DEVICE_DATA_FILE)
	{
		filePath = ((struct oracleFilePath *)fileHeader->filepath)->name;
		newFilePath = ((struct oracleFilePath *)newFileHeader->filepath)->name;
	}
	else if (fileHeader->filetype == EXCHANGE_DATABASE_FILE || fileHeader->filetype == EXCHANGE_LOG_FILE)
	{
		filePath = ((struct exchangeFilePath *)fileHeader->filepath)->databaseFile;
	}
	else if(fileHeader->filetype ==MSSQL_DATABASE_FULL||fileHeader->filetype ==MSSQL_DATABASE_DIFFERENTIAL)
	{
		filePath = ((struct mssqlFilePath *)fileHeader->filepath)->databaseName;
	}
	else if (fileHeader->filetype == ALTIBASE_DATA_FILE || fileHeader->filetype == ALTIBASE_ARCHIVE_LOG || fileHeader->filetype == ALTIBASE_LOG_ANCHOR)
	{
		filePath = ((struct altibaseFilePath *)fileHeader->filepath)->name;
		newFilePath = ((struct altibaseFilePath *)newFileHeader->filepath)->name;
	}
	
	
	// ����� �����Ͱ� �ƴϸ� ���� write buffer ���ۿ� �����ִ� �����͸� ��� ���Ͽ� ����, ������� �Ϸ� ���θ� �˻��Ѵ�.
	// raw device file�� ��쿡�� �߻��Ѵ�.
	if (fileHeader->compress == 0)
	{
		// regular file�� ��� TAIL file header�� �����µ� restore file�� �����ִٸ� ������ ������������ ����� ���̴�.
		if (fileHeader->filetype == REGULAR_FILE || fileHeader->filetype == SYSTEM_OBJECT_FILE || 
			fileHeader->filetype == ORACLE_REGULAR_DATA_FILE || fileHeader->filetype == ORACLE_ARCHIVE_LOG || fileHeader->filetype == ORACLE_CONTROL_FILE || 
			fileHeader->filetype == ALTIBASE_DATA_FILE || fileHeader->filetype == ALTIBASE_ARCHIVE_LOG || fileHeader->filetype == ALTIBASE_LOG_ANCHOR|| 
			fileHeader->filetype == EXCHANGE_DATABASE_FILE|| fileHeader->filetype == EXCHANGE_LOG_FILE ||
			( fileHeader->filetype >= VIRBAK_VMWARE_CONF_FILE && fileHeader->filetype <= VIRBAK_VMWARE_DATA_FILE) ||
			 fileHeader->filetype == SHAREPOINT_DUMP_FILE)
		{
			CloseRestoreFile(fdRestore, restorePath, fileHeader, 0);
		}
		else if(fileHeader->filetype ==MSSQL_DATABASE_FULL||fileHeader->filetype ==MSSQL_DATABASE_DIFFERENTIAL)
		{
			// write buffer�� �����ִ� �����͸� ���Ͽ� ��� ����.
			if (WriteToRawDeviceFile(fdRestore, NULL, writeFileSize, 1) == 0)
			{				
				if (!strcmp(fileHeader->backupset, newFileHeader->backupset) &&
					*writeFileSize >= (__uint64)newFileHeader->st.size)
				{
					va_ftruncate(fdRestore, (va_offset_t)newFileHeader->st.size);
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
		else
		{
			// write buffer�� �����ִ� �����͸� ���Ͽ� ��� ����.
			if (WriteToRawDeviceFile(fdRestore, NULL, writeFileSize, 1) == 0)
			{
				// raw device file�� ��쿡�� 1K byte�� ����� ������ ������� �ֱ� ������ 
				// ������ ���� ũ�� ��ŭ�� �����Ͱ� ������� ���Ͽ� ������ ��� ������ ���������� �� ������ �Ǵ��Ѵ�.
				if (!strcmp(fileHeader->backupset, newFileHeader->backupset) &&
					!strcmp(filePath, newFilePath) && 
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
			!strcmp(filePath, newFilePath) && 
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

void CloseRestoreFile(va_fd_t fdRestore, char * restorePath, struct fileHeader * fileHeader, int restoreSuccess)
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
	
	
	if (fileHeader->filetype == REGULAR_FILE || fileHeader->filetype == SYSTEM_OBJECT_FILE || 
		fileHeader->filetype == ORACLE_REGULAR_DATA_FILE || fileHeader->filetype == ORACLE_ARCHIVE_LOG || fileHeader->filetype == ORACLE_CONTROL_FILE || 
		fileHeader->filetype == ALTIBASE_DATA_FILE || fileHeader->filetype == ALTIBASE_ARCHIVE_LOG || fileHeader->filetype == ALTIBASE_LOG_ANCHOR|| 
		fileHeader->filetype == EXCHANGE_DATABASE_FILE|| fileHeader->filetype == EXCHANGE_LOG_FILE ||
		( fileHeader->filetype >= VIRBAK_VMWARE_CONF_FILE && fileHeader->filetype <= VIRBAK_VMWARE_DATA_FILE) ||
		 fileHeader->filetype == SHAREPOINT_DUMP_FILE)
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
	
	
	// ������ ������ ���� ������ ������Ű�� �α׸� �����.
	if (restoreSuccess == 1)
	{
		successFileNumber++;
		PrintRestoreFileResult(0);
	}
	// ������ ������ ���� ������ ������Ű�� �α׸� �����.
	else
	{
		failFileNumber++;
		PrintRestoreFileResult(-1);
	}
}

int sharepointRestorePath(char * sourceUrl, char * sourceName , char * target ,char * restorePath)
{
	char directory[256];	
	char sourcePath[256];	

	int i;
	//site path ã��
	memset(directory,0,sizeof(directory));
	strcpy(directory, sourceUrl);
	for (i = 0 ; i < (int)strlen(sourceUrl) ; i++)
	{
		if (sourceUrl[i] == DIRECTORY_IDENTIFIER)
		{
			memset(directory,0,sizeof(directory));
			strcpy(directory, sourceUrl + i);
		}
	}
	
	memset(sourcePath,0,sizeof(sourcePath));
	for (i = (int)strlen(sourceName) - 1; i > UNICODE_FILE_PREFIX_LENGTH + 2; i--)
	{
		if (!strncmp(directory,sourceName + i,(int)strlen(directory)))
		{
			strcpy(sourcePath,sourceName + i);
			break;
		}
	}

	
	memset(restorePath,0,sizeof(restorePath));
	if (sourcePath[0] != '\0')
	{
		if(sourcePath[0] == DIRECTORY_IDENTIFIER)
		{
			sprintf(restorePath,"%s%s",target,sourcePath);
		}
		else
		{
			sprintf(restorePath,"%s/%s",target,sourcePath);
		}		
	}
	else
	{
		return -1;
	}

	return 0;
}
