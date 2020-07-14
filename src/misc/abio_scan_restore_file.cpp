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

static int isCompressed;										// client에서 압축된 파일이 실제로 압축되었는지 여부
static int compressedSize;										// client에서 압축된 파일의 압축된 크기

#ifdef __ABIO_LINUX
	static unsigned char * unalignCompressedBuf;
#endif
static unsigned char * compressedBuf;							// client에서 압축된 파일의 데이터
static unsigned zlib_long_t compressedBufSize;					// client에서 압축된 파일의 압축된 크기

#ifdef __ABIO_LINUX
	static unsigned char * unalignUncompressedBuf;
#endif
static unsigned char * uncompressedBuf;							// client에서 압축된 파일의 데이터가 압축해제된 데이터
static unsigned zlib_long_t uncompressedBufSize;				// client에서 압축된 파일의 데이터가 압축해제된 크기

static int compressedDiskBufferSize;							// client에서 압축해서 백업할 때 사용한 disk buffer의 크기

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
	
	
	// 리스토어 작업이 끝나면 리스토어 결과를 삭제한다.
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
	
	// mount되어 있는 volume이 scan했던 volume과 다를 경우 scan 정보를 삭제한다.
	if (strcmp(volumeDB.volume, mountedVolume) != 0)
	{
		printf("The tape volume volume is diffrent with before one. Scan backup sets again.\n");
		
		// 기존에 mount되어 있던 volume 정보를 삭제한다.
		memset(driveDeviceFile, 0, sizeof(driveDeviceFile));
		memset(diskVolumeFile, 0, sizeof(diskVolumeFile));
		memset(mountedVolume, 0, sizeof(mountedVolume));
		volumeIdentify = 0;
		
		// 기존의 backup file 목록을 삭제한다.
		va_free(backupsetList);
		backupsetList = NULL;
		backupsetListNumber = 0;
		
		// 기존에 선택했던 restore file 목록을 삭제한다.
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
	
	// 리스토어할 backupset이 있는 위치까지 volume을 감는다.
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
		// volume block header에서 backupset을 읽어서 리스토어하려는 volume block이 맞는지 확인한다.
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
					
					// ABIO v3.0 sp0 pl3에서 변경된 va_backslash_to_slash()함수에서 D:/ 와 같은 형태의 경로를 /D/로 바꾸지 않게 됨에 따라 file backup에서 D:\ 가 /D/로 저장되지 않고 D:/로 저장되었다.
					// 그래서 D:/ 형태의 경로를 처리하기 위한 코드를 삽입한다.
					if (((struct regularFilePath *)newFileHeader.filepath)->name[0] != DIRECTORY_IDENTIFIER)
					{
						va_backslash_to_slash(((struct regularFilePath *)newFileHeader.filepath)->name);
					}
					
					// file header가 head이면 리스토어 대상인지 확인하고, 리스토어 파일을 만든다.
					if (newFileHeader.headerType == FILE_HEADER_HEAD)
					{
						// 불완전하게 리스토어 된 파일의 처리
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
							
							// 파일이 리스토어 대상인지 확인하고, restore path를 만든다.
							if (CheckRestoreFile(&fileHeader, fileNumber, restorePath) < 0)
							{
								continue;
							}
							
							// 파일 타입에 따라 파일을 만든다.
							fdRestore = MakeRestoreFile(fileNumber, restorePath, &fileHeader, &remainingFileSize);
						}
					}
					// file header가 tail이면 파일이 모두 쓰여졌는지 확인하고, 파일 크기를 원래의 파일 크기로 조정해준다.
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
						
						// restore buffer를 파일에 기록한다.
						fdRestore = WriteToFile(fdRestore, dataBuffer + offset, &remainingFileSize, &writeFileSize, restorePath, &fileHeader);
					}
				}
			}
		}
	}
	
	// 불완전하게 리스토어 된 파일의 처리
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

	
	// 백업 파일이 리스토어 대상인지 확인한다.
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
		
		// regular file은 restore directory가 지정된 경우에 전체 백업 경로를 restore directory에 복구한다.
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

						//restoreDirectory 가 설정되어있는 경우 입력받은 값을 인코딩한다.
						if (va_convert_string_to_utf8(ENCODING_CP949, restoreDirectory, (int)strlen(restoreDirectory), (void **)&convertFile, &convertFileSize) > 0)
						{
							if(fileHeader->filetype ==MSSQL_DATABASE_FULL||fileHeader->filetype ==MSSQL_DATABASE_DIFFERENTIAL)
							{
								sprintf(restorePath, "%s\\%s.bak", convertFile, filePath);
							}
							//Windows 데이터인 경우 D: 와 같은 형태의 경로를 제외하고 경로를 설정한다.
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
		// windows system object file, oracle regular file, altibase file은 restore directory가 지정된 경우에 file name을 restore directory에 복구한다.
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
		// raw device file은 원본 경로에 복구한다.
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
			// file size가 0이면 바로 리스토어를 끝낸다.
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
			// 이외의 시스템에서는 raw device file 백업을 지원하지 않는다.
		#endif
	}
	
	
	// client에서 압축해서 백업된 파일일 경우 압축된 파일의 데이터를 저장하기 위한 버퍼를 만든다.
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
	
	
	
	// restore하려는 file이 이미 존재할 경우 삭제하고 새로 만든다.
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
		
		
		// 리스토어하려는 raw device file이 없는 경우 리스토어를 종료한다.
		if (va_lstat(NULL, restorePath, &file_stat) != 0)
		{
			return (va_fd_t)-1;
		}
		
		// symbolic link file인 경우 link 경로가 raw device file인지 확인한다.
		if (va_is_symlink(file_stat.mode, file_stat.windowsAttribute))
		{
			// link 파일의 status를 가져온다.
			if (va_stat(NULL, restorePath, &linkStat) != 0)
			{
				return (va_fd_t)-1;
			}
			
			// link 경로가 raw device file이 아니면 종료한다.
			if (!S_ISBLK(linkStat.mode) && !S_ISCHR(linkStat.mode))
			{
				return (va_fd_t)-1;
			}
		}
		// 리스토어파일이 raw device file이 아니면 종료한다.
		else if (!S_ISBLK(file_stat.mode) && !S_ISCHR(file_stat.mode))
		{
			return (va_fd_t)-1;
		}
		
		
		// get the restore file system path and open it
		#ifdef __ABIO_AIX
			if (va_is_symlink(file_stat.mode, file_stat.windowsAttribute))
			{
				// link 파일의 경로를 읽어서 file system 경로를 찾아낸다.
				if (va_read_symlink(restorePath, linkPath, sizeof(linkPath)) == 0)
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
	
	// regular file의 경우 file size가 HEAD file header에 기록되어 있기 때문에 data buffer를 모두 파일에 쓰고 truncate하는 것이 아니라
	// file의 남은 크기만큼만 data buffer에서 읽어서 파일에 쓴다.
	// 이에 반해 raw device file이나 압축한 파일의 경우 HEAD file header에 file size가 기록이 되어 있지 않기 때문에
	// data buffer를 모두 파일에 기록하고 truncate한다.
	// 다만, raw device file의 경우에는 1K byte의 배수로 파일이 만들어져 있기 때문에 truncate할 필요 없다.
	
	
	// 압축되지 않은 데이터인 경우 리스토어 데이터를 파일에 쓴다.
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
		else if(fileHeader->filetype ==MSSQL_DATABASE_FULL|| fileHeader->filetype ==MSSQL_DATABASE_DIFFERENTIAL)
		{
			// 리스토어 데이터를 파일에 쓰고 현재까지 리스토어한 파일 크기를 얻어온다.
			if (WriteToRawDeviceFile(fd, dataBuffer, writeFileSize, 0) < 0)
			{
				CloseRestoreFile(fd, restorePath, fileHeader, 0);
				
				fd = (va_fd_t)-1;
			}
		}
		else
		{
			// 리스토어 데이터를 파일에 쓰고 현재까지 리스토어한 파일 크기를 얻어온다.
			if (WriteToRawDeviceFile(fd, dataBuffer, writeFileSize, 0) < 0)
			{
				CloseRestoreFile(fd, restorePath, fileHeader, 0);
				
				fd = (va_fd_t)-1;
			}
		}
	}
	else
	{
		// 리스토어 데이터를 파일에 쓰고 현재까지 리스토어한 파일 크기를 얻어온다.
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
	
	// data buffer에서 리스토어할 데이터를 가져온다.
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
	
	// 파일의 마지막 block이거나 버퍼가 가득 찼으면 파일에 쓴다.
	if (remainSize == 0 || writeBufferSize == sizeof(writeBuffer))
	{
		// 파일에 리스토어 데이터를 쓴다.
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
		// data buffer에서 리스토어할 데이터를 가져온다.
		memcpy(writeBuffer + writeBufferSize, dataBuffer, DSIZ);
		writeBufferSize += DSIZ;
		
		// 버퍼가 가득 찼으면 파일에 쓴다.
		if (writeBufferSize == sizeof(writeBuffer))
		{
			// 파일에 리스토어 데이터를 쓴다.
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
		// 파일에 리스토어 데이터를 쓴다.
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
		// data buffer에서 리스토어할 데이터를 가져온다.
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
		
		// compressed data buffer가 가득차면 압축된 데이터의 다음 정보를 읽고, 데이터를 리스토어 파일에 쓴다.
		if (compDataRemainingSize == 0)
		{
			// 이번에 읽은 데이터가 백업 데이터의 압축 여부를 나타내면, 다음에는 압축된 데이터의 크기를 읽는다.
			if (compDataType == IS_COMPRESSED)
			{
				isCompressed = htonl(isCompressed);		// network to host
				
				// 다음에 읽을 데이터를 압축된 데이터의 크기로 설정한다.
				compDataType = COMPRESSED_SIZE;
				compDataBuffer = (char *)&compressedSize;
				compDataBufferSize = 0;
				compDataRemainingSize = sizeof(int);
			}
			// 이번에 읽은 데이터가 백업 데이터의 압축된 크기를 나타내면, 다음에는 압축된 백업 데이터를 읽는다.
			else if (compDataType == COMPRESSED_SIZE)
			{
				// 압축 여부가 파일의 끝을 표시하고 있으면 종료한다.
				if (isCompressed == 1)
				{
					// 다음에 읽을 데이터를 백업 데이터의 압축 여부로 설정한다.
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
					
					// 다음에 읽을 데이터를 압축된 백업 데이터로 설정한다.
					compDataType = COMPRESSED_BUFFER;
					compDataBuffer = (char *)compressedBuf;
					compDataBufferSize = 0;
					compDataRemainingSize = compressedSize;
				}
			}
			// 이번에 읽은 데이터가 압축된 백업 데이터이면 압축을 풀고 리스토어 파일에 쓴다.
			else if (compDataType == COMPRESSED_BUFFER)
			{
				// 다음에 읽을 데이터를 백업 데이터의 압축 여부로 설정한다.
				compDataType = IS_COMPRESSED;
				compDataBuffer = (char *)&isCompressed;
				compDataBufferSize = 0;
				compDataRemainingSize = sizeof(int);
				
				// 압축을 풀고 리스토어 파일에 쓴다.
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
		
		// data buffer의 데이터를 모두 읽은 경우 종료한다.
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
	
	
	// 압축된 데이터가 아니면 현재 write buffer 버퍼에 남아있는 데이터를 모두 파일에 쓰고, 리스토어 완료 여부를 검사한다.
	// raw device file의 경우에만 발생한다.
	if (fileHeader->compress == 0)
	{
		// regular file의 경우 TAIL file header를 만났는데 restore file이 열려있다면 리스토어가 비정상적으로 종료된 것이다.
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
			// write buffer에 남아있는 데이터를 파일에 모두 쓴다.
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
			// write buffer에 남아있는 데이터를 파일에 모두 쓴다.
			if (WriteToRawDeviceFile(fdRestore, NULL, writeFileSize, 1) == 0)
			{
				// raw device file의 경우에는 1K byte의 배수로 파일이 만들어져 있기 때문에 
				// 원래의 파일 크기 만큼의 데이터가 리스토어 파일에 쓰여진 경우 리스토어가 정상적으로 된 것으로 판단한다.
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
	// 압축된 데이터인 경우 file header tail이 오기 전에 리스토어가 완료되었어야 한다.
	else
	{
		// 압축을 한 경우 원래의 파일 크기 만큼의 데이터가 리스토어 파일에 쓰여진 경우 리스토어가 정상적으로 된 것으로 판단한다.
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
		
		// client에서 압축해서 백업된 파일의 리스토어가 끝난 경우 압축된 파일의 데이터를 저장한 버퍼를 삭제한다.
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
	
	
	// 리스토어에 성공한 파일 개수를 증가시키고 로그를 남긴다.
	if (restoreSuccess == 1)
	{
		successFileNumber++;
		PrintRestoreFileResult(0);
	}
	// 리스토어에 실패한 파일 개수를 증가시키고 로그를 남긴다.
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
	//site path 찾기
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
