#ifndef __ABIO_SCAN_H__
#define __ABIO_SCAN_H__


struct backupsetInfo
{
	char backupset[BACKUPSET_ID_LENGTH];
	char client[NODE_NAME_LENGTH];
	char backupTime[TIME_STAMP_LENGTH];
	__uint64 backupsetPosition;
	
	int fileNumber;
	va_fd_t fd;
};

enum compressedDataType
{
	IS_COMPRESSED,
	COMPRESSED_SIZE,
	COMPRESSED_BUFFER
};


void MenuInit();
void MenuScan();
void MenuScanBackupFile();
void MenuStatus();
int QuitMenu();
int DetermineRescanBackupsetList();
void WaitContinue();

void PrintRestoreStartLog();
void PrintRestoreFileLog(int fileNumber, char * source, char * target);
void PrintRestoreFileResult(int result);
void PrintRestoreResult();

void ScanTapeBackupFile();
va_fd_t LoadTapeVolume(struct volumeDB * headerVolumeDB);
void UnloadTapeVolume(va_fd_t fdDrive);
void ScanDiskBackupFile();
va_fd_t LoadDiskVolume(struct volumeDB * headerVolumeDB);
void UnloadDiskVolume(va_fd_t fdDrive);
int DetermineRescanVolume();
int ReadBackupFileFromVolumeBlock(char * volumeBuffer, __uint64 backupsetPosition, va_fd_t fdBackupset);

void MenuRestore();
void SetRestoreBackupset();
void AddRestoreFiles();
void RemoveRestoreFiles();
void ResetRestoreFiles();
void CheckRestoreFiles();
void SetRestoreDirectory();
void ShowBackupFiles();
int ComparatorRestoreFile(const void * a1, const void * a2);
int SearchRestoreFile(int restoreFile);


void Restore();
void RestoreFile();

int CheckRestoreFile(struct fileHeader * fileHeader, int fileNumber, char * restorePath);
va_fd_t MakeRestoreFile(int fileNumber, char * restorePath, struct fileHeader * fileHeader, __uint64 * remainingFileSize);
va_fd_t MakeRestoreRegularFile(char * restorePath, struct fileHeader * fileHeader);

#ifdef __ABIO_UNIX
	va_fd_t MakeRestoreRawDeviceFile(char * restorePath);
#endif

va_fd_t WriteToFile(va_fd_t fdRestore, char * dataBuffer, __uint64 * remainingFileSize, __uint64 * writeFileSize, char * restorePath, struct fileHeader * fileHeader);
int WriteToRegularFile(va_fd_t fd, char * dataBuffer, __uint64 * remainingFileSize,int encryption);
int WriteToRawDeviceFile(va_fd_t fd, char * dataBuffer, __uint64 * writeFileSize, int flush);
int WriteToCompressFile(va_fd_t fd, char * dataBuffer, __uint64 * writeFileSize,int encryption);

void CloseFile(va_fd_t fdRestore, struct fileHeader * fileHeader, struct fileHeader * newFileHeader, __uint64 * writeFileSize, char * restorePath);
void CloseRestoreFile(va_fd_t fdRestore, char * restorePath, struct fileHeader * fileHeader, int restoreSuccess);

//sharepoint path setting
int sharepointRestorePath(char * sourceUrl, char * sourceName , char * target ,char * restorePath);

#endif
