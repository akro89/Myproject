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

extern char currentDirectory[MAX_PATH_LENGTH];

void MenuScanBackupFile()
{
	char input[DSIZ];
	
	
	
	// 기존에 scan한 backupset 목록이 있을 경우 새로 scan 할 것인지 확인한다.
	if (DetermineRescanBackupsetList() == 0)
	{
		return;
	}
	
	
	// volume을 scan하기 시작하면 기존의 scan 정보를 삭제한다.
	memset(driveDeviceFile, 0, sizeof(driveDeviceFile));
	memset(diskVolumeFile, 0, sizeof(diskVolumeFile));
	
	memset(mountedVolume, 0, sizeof(mountedVolume));
	volumeIdentify = 0;
	volumeBlockSize = 0;
	dataBufferSize = 0;
	
	va_free(backupsetList);
	backupsetList = NULL;
	backupsetListNumber = 0;
	
	
	while (1)
	{
		// get a tape drive device file
		printf("\n");
		if (mountedVolumeType == VOLUME_TAPE)
		{
			printf("Input a tape drive device file : ");
		}
		else
		{
			printf("Input disk volume file : ");
		}
		
		gets(input);
		if (strlen(input) > 1)
		{
			if (mountedVolumeType == VOLUME_TAPE)
			{
				strcpy(driveDeviceFile, input);
			}
			else
			{
				strcpy(diskVolumeFile, input);
			}
			
			// get a volume block size
			while (1)
			{
				printf("Input the block size of data in bytes which was written on tape volume(if you did not modify this value in the master.conf, just type enter) : ");
				
				gets(input);
				if (input[0] == '\0')
				{
					volumeBlockSize = DEFAULT_VOLUME_BLOCK_SIZE;
					dataBufferSize = volumeBlockSize - VOLUME_BLOCK_HEADER_SIZE;
					
					break;
				}
				else if (strlen(input) > 1)
				{
					volumeBlockSize = atoi(input);
					dataBufferSize = volumeBlockSize - VOLUME_BLOCK_HEADER_SIZE;
					
					break;
				}
				else
				{
					if (input[0] == 'q' || input[0] == 'Q')
					{
						if (QuitMenu() == 1)
						{
							return;
						}
					}
				}
			}
			
			break;
		}
		else if (strlen(input) == 1)
		{
			if (input[0] == 'q' || input[0] == 'Q')
			{
				if (QuitMenu() == 1)
				{
					return;
				}
			}
		}
	}
	
	
	if (mountedVolumeType == VOLUME_TAPE)
	{
		ScanTapeBackupFile();
	}
	else
	{
		ScanDiskBackupFile();
	}
	
	WaitContinue();
}

int DetermineRescanBackupsetList()
{
	char input[DSIZ];
	
	
	
	if (backupsetListNumber > 0)
	{
		while (1)
		{
			printf("\n");
			printf("There are previous scanned backup sets. Do you want to update it? (y/n) ");
			gets(input);
			
			if (strlen(input) == 1)
			{
				if (input[0] == 'y' || input[0] == 'Y')
				{
					break;
				}
				else if (input[0] == 'n')
				{
					return 0;
				}
			}
		}
	}
	
	
	return 1;
}

void ScanTapeBackupFile()
{
	va_fd_t fdDrive;
	struct volumeDB volumeDB;
	
	char * volumeBuffer;
	int backupsetPosition;
	
	va_fd_t fdBackupset;
	char backupsetListFile[MAX_PATH_LENGTH];
	
	int errorOccured;
	
	int i;
	
	// initialize variables
	volumeBuffer = NULL;
	backupsetPosition = 0;
	
	memset(backupsetListFile, 0, sizeof(backupsetListFile));
	
	errorOccured = 0;
	
	
	// open tape drive
	if ((fdDrive = LoadTapeVolume(&volumeDB)) != (va_fd_t)-1)
	{
		strcpy(mountedVolume, volumeDB.volume);
		
		printf("Mounted tape volume : %s\tVersion : %d\n\n", volumeDB.volume, volumeDB.version);
	}
	else
	{
		return;
	}
	
	
	// 이전에 이 volume에서 scan한 backupset 목록이 저장된 파일이 있는지 확인한다.
	if (DetermineRescanVolume() == 0)
	{
		UnloadTapeVolume(fdDrive);
		
		return;
	}
	
	
	// scan backupsets in tape volume
	printf("\n");
	printf("Reading...\n\n");
	
	volumeBuffer = (char *)malloc(sizeof(char) * volumeBlockSize);
	sprintf(backupsetListFile, "%s.txt", mountedVolume);

	//ky88 2012.03.22
	if ((fdBackupset = va_open(currentDirectory, backupsetListFile, O_CREAT|O_RDWR, 0, 0)) != (va_fd_t)-1)
	//if ((fdBackupset = va_open(NULL, backupsetListFile, O_CREAT|O_RDWR, 0, 0)) != (va_fd_t)-1)
	{
		// pass volume header
		if (va_forward_filemark(fdDrive, 1) == 0)
		{
			// volume의 모든 backupset을 읽는다.
			while (errorOccured == 0)
			{
				backupsetPosition++;
				
				// read a first volume block of backupset
				if (va_read(fdDrive, volumeBuffer, volumeBlockSize, DATA_TYPE_NOT_CHANGE) != volumeBlockSize)
				{
					// tape volume has reached the end of the volume
					break;
				}
				
				while (1)
				{
					// 이 block의 파일들을 읽어서 backupset의 backup file 목록 파일에 저장한다.
					if (ReadBackupFileFromVolumeBlock(volumeBuffer, backupsetPosition, fdBackupset) != 0)
					{
						errorOccured = 1;
						
						break;
					}
					
					// read a next volume block of backupset
					if (va_read(fdDrive, volumeBuffer, volumeBlockSize, DATA_TYPE_NOT_CHANGE) != volumeBlockSize)
					{
						// volume position이 end of filemark에 다다르면 read 동작의 리턴값은 0으로 나오고 volume position은 filemark를 지나간다.
						// 따라서 filemark를 건너뛰기 위해 forward filemark 동작을 할 필요는 없다.
						break;
					}
				}
			}
		}
		
		va_close(fdBackupset);
		
		
		for (i = 0; i < backupsetListNumber; i++)
		{
			va_close(backupsetList[i].fd);
		}
	}
	else
	{
		printf("Cannot make the file to save backup sets of this volume.\n");
	}
	
	
	// unload volume
	va_free(volumeBuffer);
	UnloadTapeVolume(fdDrive);
}

va_fd_t LoadTapeVolume(struct volumeDB * headerVolumeDB)
{
	va_fd_t fdDrive;
	char * volumeHeader;
	
	struct volumeDB volumeDB;
	enum version version;
	
	
	
	printf("\n");
	printf("Loading...\n\n");
	
	
	if ((fdDrive = va_open_tape_drive(driveDeviceFile, O_RDONLY, 0)) == (va_fd_t)-1)
	{
		printf("Cannot open the tape drive.\n");
		
		return (va_fd_t)-1;
	}
	
	if (va_load_medium(fdDrive) != 0)
	{
		printf("Cannot load tape volume.\n");
		
		UnloadTapeVolume(fdDrive);
		
		return (va_fd_t)-1;
	}
	
	// set a tape block size
	if (va_set_tape_block_size(fdDrive, volumeBlockSize) != 0)
	{
		printf("Cannot set the tape block size as %d.\n", volumeBlockSize);
		
		UnloadTapeVolume(fdDrive);
		
		return (va_fd_t)-1;
	}
	
	// volume에서 volume header를 읽어온다.
	volumeHeader = (char *)malloc(sizeof(char) * volumeBlockSize);
	if (va_read(fdDrive, volumeHeader, volumeBlockSize, DATA_TYPE_NOT_CHANGE) != volumeBlockSize)
	{
		printf("Cannot read tape volume header.\n");
		
		va_free(volumeHeader);
		UnloadTapeVolume(fdDrive);
		
		return (va_fd_t)-1;
	}
	
	memcpy(&version, volumeHeader, sizeof(version));
	version = (enum version)htonl(version);		// network to host
	
	// volume header에 있는 label 정보를 읽는다.
	va_read_label_from_volume_header(volumeHeader, &volumeDB);
	
	// volume header에 label이 기록되어있는지 확인한다.
	if (va_is_abio_volume(&volumeDB) == 1)
	{
		memcpy(headerVolumeDB, &volumeDB, sizeof(struct volumeDB));
		headerVolumeDB->version = version;
		volumeIdentify = 1;
		
		va_free(volumeHeader);
		
		return fdDrive;
	}
	else
	{
		printf("Cannot identify tape volume.\n");
		volumeIdentify = 2;
		
		va_free(volumeHeader);
		UnloadTapeVolume(fdDrive);
		
		return (va_fd_t)-1;
	}
}

void UnloadTapeVolume(va_fd_t fdDrive)
{
	printf("\n");
	printf("Unloading...\n\n");
	
	va_rewind_tape(fdDrive);
	
	va_close(fdDrive);
}

void ScanDiskBackupFile()
{
	va_fd_t fdDrive;
	struct volumeDB volumeDB;
	
	char * volumeBuffer;
	__uint64 backupsetPosition;

	va_fd_t fdBackupset;
	char backupsetListFile[MAX_PATH_LENGTH];
	
	int i;
	
	// initialize variables
	volumeBuffer = NULL;
	backupsetPosition = 0;
	
	memset(backupsetListFile, 0, sizeof(backupsetListFile));
	
	
	// open disk volume
	if ((fdDrive = LoadDiskVolume(&volumeDB)) != (va_fd_t)-1)
	{
		strcpy(mountedVolume, volumeDB.volume);
		
		printf("Mounted disk volume : %s\tVersion : %d\n\n", volumeDB.volume, volumeDB.version);
	}
	else
	{
		return;
	}
	
	
	// 이전에 이 volume에서 scan한 backupset 목록이 저장된 파일이 있는지 확인한다.
	if (DetermineRescanVolume() == 0)
	{
		UnloadDiskVolume(fdDrive);
		
		return;
	}
	
	
	// scan backupsets in disk volume
	printf("\n");
	printf("Reading...\n\n");
	
	volumeBuffer = (char *)malloc(sizeof(char) * volumeBlockSize);
	//ky88 add
	memset(volumeBuffer, 0x00, sizeof(volumeBuffer));

	sprintf(backupsetListFile, "%s.txt", mountedVolume);
//k88 2012.03.22
	if ((fdBackupset = va_open(currentDirectory, backupsetListFile, O_CREAT|O_RDWR, 0, 0)) != (va_fd_t)-1)
	//if ((fdBackupset = va_open(NULL, backupsetListFile, O_CREAT|O_RDWR, 0, 0)) != (va_fd_t)-1)
	{
		while (va_read(fdDrive, volumeBuffer, volumeBlockSize, DATA_TYPE_NOT_CHANGE) == volumeBlockSize)
		{
			// 이 block의 파일들을 읽어서 backupset의 backup file 목록 파일에 저장한다.
			if (ReadBackupFileFromVolumeBlock(volumeBuffer, backupsetPosition, fdBackupset) != 0)
			{
				break;
			}
			
			backupsetPosition += volumeBlockSize;
		}
		
		va_close(fdBackupset);
		
		
		for (i = 0; i < backupsetListNumber; i++)
		{
			va_close(backupsetList[i].fd);
		}
	}
	else
	{
		printf("Cannot make the file to save backup sets of this volume.\n");
	}
	
	
	// unload volume
	va_free(volumeBuffer);
	UnloadDiskVolume(fdDrive);
}

va_fd_t LoadDiskVolume(struct volumeDB * headerVolumeDB)
{
	va_fd_t fdDrive;
	char * volumeHeader;
	
	struct volumeDB volumeDB;
	enum version version;
	
	
	
	printf("\n");
	printf("Loading...\n\n");
	
	
	if ((fdDrive = va_open(NULL, diskVolumeFile, O_RDONLY, 0, 0)) == (va_fd_t)-1)
	{
		printf("Cannot open disk volume file.\n");
		
		return (va_fd_t)-1;
	}
	
	// volume에서 volume header를 읽어온다.
	volumeHeader = (char *)malloc(sizeof(char) * volumeBlockSize);
	if (va_read(fdDrive, volumeHeader, volumeBlockSize, DATA_TYPE_NOT_CHANGE) != volumeBlockSize)
	{
		printf("Cannot read disk volume header.\n");
		
		va_free(volumeHeader);
		UnloadDiskVolume(fdDrive);
		
		return (va_fd_t)-1;
	}
	
	memcpy(&version, volumeHeader, sizeof(version));
	version = (enum version)htonl(version);		// network to host
	
	// volume header에 있는 label 정보를 읽는다.
	va_read_label_from_volume_header(volumeHeader, &volumeDB);
	
	// volume header에 label이 기록되어있는지 확인한다.
	if (va_is_abio_volume(&volumeDB) == 1)
	{
		memcpy(headerVolumeDB, &volumeDB, sizeof(struct volumeDB));
		headerVolumeDB->version = version;
		volumeIdentify = 1;
		
		va_free(volumeHeader);
		
		return fdDrive;
	}
	else
	{
		printf("Cannot identify disk volume.\n");
		volumeIdentify = 2;
		
		va_free(volumeHeader);
		UnloadDiskVolume(fdDrive);
		
		return (va_fd_t)-1;
	}
}

void UnloadDiskVolume(va_fd_t fdDrive)
{
	printf("\n");
	printf("Unloading...\n\n");
	
	va_close(fdDrive);
}

int DetermineRescanVolume()
{
	va_fd_t fdBackupset;
	char backupsetListFile[MAX_PATH_LENGTH];
	struct backupsetInfo backupsetInfo;
	
	char input[DSIZ];
	
	char backupFileListFile[MAX_PATH_LENGTH];
	char ** lines;
	
	int i;
	
	
	memset(backupsetListFile, 0, sizeof(backupsetListFile));
	sprintf(backupsetListFile, "%s.txt", mountedVolume);

//k88 2012.03.22
	if ((fdBackupset = va_open(currentDirectory, backupsetListFile, O_RDONLY, 0, 0)) != (va_fd_t)-1)
	//if ((fdBackupset = va_open(NULL, backupsetListFile, O_RDONLY, 0, 0)) != (va_fd_t)-1)
	{
		while (1)
		{
			printf("\n");
			printf("There are previous scanned backup sets of this volume. Do you want to update it? (y/n) ");
			gets(input);
			
			if (strlen(input) == 1)
			{
				// 새로 scan할 것을 선택한 경우 기존의 목록이 저장되어있는 파일을 삭제한다.
				if (input[0] == 'y' || input[0] == 'Y')
				{
					while (va_read(fdBackupset, &backupsetInfo, sizeof(struct backupsetInfo), DATA_TYPE_NOT_CHANGE) > 0)
					{
						memset(backupFileListFile, 0, sizeof(backupFileListFile));
						sprintf(backupFileListFile, "%s.txt", backupsetInfo.backupset);
						va_remove(NULL, backupFileListFile);
					}
					
					va_close(fdBackupset);
					va_remove(NULL, backupsetListFile);
					
					break;
				}
				// 기존에 scan한 목록을 사용하려는 경우 backupset 목록을 load한다.
				else if (input[0] == 'n' || input[0] == 'N')
				{
					while (va_read(fdBackupset, &backupsetInfo, sizeof(struct backupsetInfo), DATA_TYPE_NOT_CHANGE) > 0)
					{
						memset(backupFileListFile, 0, sizeof(backupFileListFile));
						sprintf(backupFileListFile, "%s.txt", backupsetInfo.backupset);
						
						if (backupsetListNumber % DEFAULT_MEMORY_FRAGMENT == 0)
						{
							backupsetList = (struct backupsetInfo *)realloc(backupsetList, sizeof(struct backupsetInfo) * (backupsetListNumber + DEFAULT_MEMORY_FRAGMENT));
							memset(backupsetList + backupsetListNumber, 0, sizeof(struct backupsetInfo) * DEFAULT_MEMORY_FRAGMENT);
						}
						strcpy(backupsetList[backupsetListNumber].backupset, backupsetInfo.backupset);
						strcpy(backupsetList[backupsetListNumber].client, backupsetInfo.client);
						backupsetList[backupsetListNumber].backupsetPosition = backupsetInfo.backupsetPosition;
						backupsetList[backupsetListNumber].fileNumber = va_load_text_file_lines(currentDirectory, backupFileListFile, &lines, NULL);
						backupsetListNumber++;
						
						for (i = 0; i < backupsetList[backupsetListNumber - 1].fileNumber; i++)
						{
							va_free(lines[i]);
						}
						va_free(lines);
					}
					
					va_close(fdBackupset);
					
					return 0;
				}
			}
		}
	}
	
	
	return 1;
}
	
int ReadBackupFileFromVolumeBlock(char * volumeBuffer, __uint64 backupsetPosition, va_fd_t fdBackupset)
{
	char backupFileListFile[MAX_PATH_LENGTH];
	
	struct fileHeader fileHeader;
	char * filePath;
	
	char fileSize[NUMBER_STRING_LENGTH];
	char tmpbuf[DSIZ];
	
	#ifdef __ABIO_WIN
		char * convertTmpbuf;
		int convertTmpbufSize;
	#endif
	
	char * dataBuffer;
	
	int i;
	int k;
	
	if (volumeBuffer == NULL || volumeBuffer[0] == '\0')
	{
		return 0;
	}
	
	
	// initialize variables
	memset(backupFileListFile, 0, sizeof(backupFileListFile));
	
	
	// 이 block의 backupset을 backupset 목록에서 찾는다.
	for (i = 0; i < backupsetListNumber; i++)
	{
		if (!strncmp(backupsetList[i].backupset, volumeBuffer, strlen(backupsetList[i].backupset)))
		{
			break;
		}
	}
	
	// 이 block의 backupset이 backupset 목록에 없으면 추가하고 backupset 목록 파일에 기록한다.
	if (i == backupsetListNumber)
	{
		// backupset 목록에 backupset을 추가한다.
		if (i % DEFAULT_MEMORY_FRAGMENT == 0)
		{
			backupsetList = (struct backupsetInfo *)realloc(backupsetList, sizeof(struct backupsetInfo) * (i + DEFAULT_MEMORY_FRAGMENT));
			memset(backupsetList + i, 0, sizeof(struct backupsetInfo) * DEFAULT_MEMORY_FRAGMENT);
		}
		memcpy(backupsetList[i].backupset, volumeBuffer, sizeof(backupsetList[i].backupset));
		memcpy(backupsetList[i].client, volumeBuffer + BACKUPSET_ID_LENGTH, sizeof(backupsetList[i].client));
		va_make_time_stamp((time_t)atoi(backupsetList[i].backupset), backupsetList[i].backupTime, TIME_STAMP_TYPE_EXTERNAL_EN);
		backupsetList[i].backupsetPosition = backupsetPosition;
		
		// backupset 목록 파일에 기록한다.
		printf("\n");
		printf("(%d). backup set id : %s\tclient : %s\tbackup time : %s\n", i + 1, backupsetList[i].backupset, backupsetList[i].client, backupsetList[i].backupTime);
		va_write(fdBackupset, backupsetList + i, sizeof(struct backupsetInfo), DATA_TYPE_NOT_CHANGE);
		
		// backupset의 backup file 목록이 저장될 파일을 만든다.
		sprintf(backupFileListFile, "%s.txt", backupsetList[i].backupset);

		//k88 2012.03.22
		if ((backupsetList[i].fd = va_open(currentDirectory, backupFileListFile, O_CREAT|O_RDWR, 0, 0)) == (va_fd_t)-1)
		//if ((backupsetList[i].fd = va_open(NULL, backupFileListFile, O_CREAT|O_RDWR, 0, 0)) == (va_fd_t)-1)
		{
			printf("Cannot make the file to save files in this backup set.\n");
			
			return -1;
		}
		
		backupsetListNumber++;
	}
	
	
	// 이 block의 파일들을 읽어서 backupset의 backup file 목록 파일에 저장한다.
	dataBuffer = volumeBuffer + VOLUME_BLOCK_HEADER_SIZE;
	
	for (k = 0; k < dataBufferSize; k += DSIZ)
	{
		if (!strncmp(dataBuffer + k, ABIO_CHECK_CODE, strlen(ABIO_CHECK_CODE)) && 
			!strncmp(dataBuffer + k + ABIO_CHECK_CODE_SIZE, backupsetList[i].backupset, strlen(backupsetList[i].backupset)))
		{
			// read file header
			memcpy(&fileHeader, dataBuffer + k, sizeof(struct fileHeader));
			va_change_byte_order_fileHeader(&fileHeader);		// network to host
			va_convert_v25_to_v30_fileHeader(&fileHeader);
			
			if (fileHeader.headerType == FILE_HEADER_HEAD)
			{
				if (fileHeader.filetype == REGULAR_FILE || fileHeader.filetype == RAW_DEVICE || fileHeader.filetype == SYSTEM_OBJECT_FILE || 
					fileHeader.filetype == ORACLE_REGULAR_DATA_FILE || fileHeader.filetype == ORACLE_RAW_DEVICE_DATA_FILE || fileHeader.filetype == ORACLE_ARCHIVE_LOG || fileHeader.filetype == ORACLE_CONTROL_FILE || 
					fileHeader.filetype == ALTIBASE_DATA_FILE || fileHeader.filetype == ALTIBASE_ARCHIVE_LOG || fileHeader.filetype == ALTIBASE_LOG_ANCHOR|| 
					fileHeader.filetype == EXCHANGE_DATABASE_FILE|| fileHeader.filetype == EXCHANGE_LOG_FILE||
					fileHeader.filetype ==MSSQL_DATABASE_FULL|| fileHeader.filetype ==MSSQL_DATABASE_DIFFERENTIAL||
					( fileHeader.filetype >= VIRBAK_VMWARE_CONF_FILE && fileHeader.filetype <= VIRBAK_VMWARE_DATA_FILE) ||
					fileHeader.filetype ==SHAREPOINT_DUMP_DIRECTORY || fileHeader.filetype == SHAREPOINT_DUMP_FILE)
				{
					// 이 block의 파일을 읽는다.
					backupsetList[i].fileNumber++;
					
                    memset(fileSize, 0, sizeof(fileSize));
					memset(tmpbuf, 0, sizeof(tmpbuf));


					if (fileHeader.filetype == REGULAR_FILE || fileHeader.filetype == RAW_DEVICE || fileHeader.filetype == SYSTEM_OBJECT_FILE)
					{
						filePath = ((struct regularFilePath *)fileHeader.filepath)->name;
					}
					else if (fileHeader.filetype == ORACLE_REGULAR_DATA_FILE || fileHeader.filetype == ORACLE_RAW_DEVICE_DATA_FILE || fileHeader.filetype == ORACLE_ARCHIVE_LOG || fileHeader.filetype == ORACLE_CONTROL_FILE)
					{
						filePath = ((struct oracleFilePath *)fileHeader.filepath)->name;
					}
					else if (fileHeader.filetype == EXCHANGE_DATABASE_FILE || fileHeader.filetype == EXCHANGE_LOG_FILE)
					{
						filePath = ((struct exchangeFilePath *)fileHeader.filepath)->databaseFile;
					}
					else if (fileHeader.filetype == ALTIBASE_DATA_FILE || fileHeader.filetype == ALTIBASE_ARCHIVE_LOG || fileHeader.filetype == ALTIBASE_LOG_ANCHOR)
					{
						filePath = ((struct altibaseFilePath *)fileHeader.filepath)->name;
					}
					else if(fileHeader.filetype ==MSSQL_DATABASE_FULL||fileHeader.filetype ==MSSQL_DATABASE_DIFFERENTIAL)
					{
						filePath = ((struct mssqlFilePath *)fileHeader.filepath)->databaseName;
					}
					//ky88 add for vmware
					else if ( fileHeader.filetype >= VIRBAK_VMWARE_CONF_FILE && fileHeader.filetype <= VIRBAK_VMWARE_DATA_FILE )
					{
						filePath = ((struct vmwareFilePath *)fileHeader.filepath)->file;
					}
					//ky88
					//sharepoint
					else if ( fileHeader.filetype == SHAREPOINT_DUMP_DIRECTORY  || fileHeader.filetype == SHAREPOINT_DUMP_FILE )
					{
						filePath = ((struct sharepointFilePath *)fileHeader.filepath)->name;
					}
					
					if (fileHeader.st.size / 1024 / 1024 / 1024 / 1024 != 0)
					{
						sprintf(fileSize, "%.2f TB", (__int64)fileHeader.st.size / 1024.0 / 1024.0 / 1024.0 / 1024.0);
					}
					else if (fileHeader.st.size / 1024 / 1024 / 1024 != 0)
					{
						sprintf(fileSize, "%.2f GB", (__int64)fileHeader.st.size / 1024.0 / 1024.0 / 1024.0);
					}
					else if (fileHeader.st.size / 1024 / 1024 != 0)
					{
						sprintf(fileSize, "%.2f MB", (__int64)fileHeader.st.size / 1024.0 / 1024.0);
					}
					else
					{
						sprintf(fileSize, "%.2f KB", (__int64)fileHeader.st.size / 1024.0);
					}
					
					#ifdef __ABIO_UNIX
						sprintf(tmpbuf, "%d. %s. %s.\n", backupsetList[i].fileNumber, filePath, fileSize);
						printf("%s", tmpbuf);
					#elif __ABIO_WIN
						va_slash_to_backslash(filePath);
						sprintf(tmpbuf, "%d. %s. %s.\r\n", backupsetList[i].fileNumber, filePath, fileSize);
						
						if (va_convert_string_from_utf8(ENCODING_UNKNOWN, tmpbuf, (int)strlen(tmpbuf), (void **)&convertTmpbuf, &convertTmpbufSize) > 0)
						{
							printf("%s", convertTmpbuf);
							va_free(convertTmpbuf);
						}
					#endif
					
					// 파일을 backupset의 backup file 목록 파일에 저장한다.
					va_write(backupsetList[i].fd, tmpbuf, (int)strlen(tmpbuf), DATA_TYPE_NOT_CHANGE);
				}
			}
		}
	}
	
	
	return 0;
}
