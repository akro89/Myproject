#ifndef __ORACLE12C_H__
#define __ORACLE12C_H__


enum backupFailCause {READ_FILE_ERROR, OPEN_FILE_ERROR, FILE_LENGTH_ERROR};
enum restoreFailCause {WRITE_FILE_ERROR, MAKE_FILE_ERROR};


struct tableSpaceItem
{
	char database[ABIO_FILE_LENGTH];
	char tableSpace[ABIO_FILE_LENGTH];
	char ** dataFile;
	int fileNumber;
};

struct archiveLogItem
{
	char * name;
	struct abio_file_stat st;
};

struct restoreFile
{
	char * source;
	char * target;
	char * backupset;
};

enum compressedDataType
{
	IS_COMPRESSED,
	COMPRESSED_SIZE,
	COMPRESSED_BUFFER
};


// in oracle.cpp
int InitProcess(char * filename);
int GetModuleConfiguration();
void LoadLogDescription();

void SendCommand(int request, int reply, int execute, char * requestCommand, char * subCommand, char * executeCommand, char * sourceIP, char * sourcePort, char * sourceNodename, char * destinationIP, char * destinationPort, char * destinationNodename);

int ExecuteSql(char * query,char * query2);
int MakeQueryFile(char * query,char * query2);
int CheckQueryError(char * query);
void PrintQueryError();

void SendJobLog(char * logMsgID, ...);
void SendJobLog2(int logLevel,char * logCode, char * logMsgID, ...);


// in backup.pc
int Backup();


// in restore.cpp
int Restore();


enum functionNumber
{
	FUNCTION_InitProcess = 1, 
	FUNCTION_GetModuleConfiguration = 2, 
	FUNCTION_SendCommand = 3, 
	FUNCTION_Error = 4, 
	FUNCTION_Exit = 5
};

enum functionNumberBackup
{
	FUNCTION_BACKUP_Backup = 1, 
	FUNCTION_BACKUP_InitBackup = 2, 
	FUNCTION_BACKUP_InnerMessageTransfer = 3, 
	
	FUNCTION_BACKUP_StartBackup = 10, 
	FUNCTION_BACKUP_BackupTableSpace = 11, 
	FUNCTION_BACKUP_BackupArchiveLog = 12, 
	FUNCTION_BACKUP_BackupControlFileBinary = 13, 
	FUNCTION_BACKUP_BackupFile = 14, 
	FUNCTION_BACKUP_FileBackup = 15, 
	FUNCTION_BACKUP_BackupRawDeviceAIX = 16, 
	FUNCTION_BACKUP_RawDeviceBackup = 17, 
	FUNCTION_BACKUP_MakeFileHeader = 18, 
	FUNCTION_BACKUP_MakeAbioStat = 19, 
	FUNCTION_BACKUP_MakeClientCatalog = 20, 
	FUNCTION_BACKUP_SendFileHeader = 21, 
	FUNCTION_BACKUP_SendFileContents = 22, 
	FUNCTION_BACKUP_SendFileContentsCompress = 23, 
	FUNCTION_BACKUP_SendFileContentsRawDeviceAIX = 24, 
	FUNCTION_BACKUP_SendFileContentsRawDeviceAIXCompress = 25, 
	FUNCTION_BACKUP_SendBackupData = 26, 
	FUNCTION_BACKUP_SendBackupBuffer = 27, 
	FUNCTION_BACKUP_SendBackupFile = 28, 
	FUNCTION_BACKUP_CheckBackupComplete = 29, 
	FUNCTION_BACKUP_BackupControlFileTrace = 30, 
	
	FUNCTION_BACKUP_GetTableSpaceList = 50, 
	FUNCTION_BACKUP_GetArchiveLogList = 51, 
	FUNCTION_BACKUP_GetControlFileList = 52, 
	FUNCTION_BACKUP_GetOracleLogMode = 53, 
	FUNCTION_BACKUP_BeginBackup = 54, 
	FUNCTION_BACKUP_EndBackup = 55, 
	FUNCTION_BACKUP_SwitchLogFile = 56, 
	FUNCTION_BACKUP_AlterBackupControlFileBinary = 57, 
	FUNCTION_BACKUP_AlterBackupControlFileTrace = 58, 
	
	FUNCTION_BACKUP_ServerError = 100, 
	FUNCTION_BACKUP_WaitForServerError = 101, 
	FUNCTION_BACKUP_Error = 102, 
	FUNCTION_BACKUP_ReleaseResource = 103, 
	FUNCTION_BACKUP_Exit = 104, 
	
	FUNCTION_BACKUP_PrintBackupStartLog = 120, 
	FUNCTION_BACKUP_PrintQueryError = 121, 
	FUNCTION_BACKUP_PrintBackupTableSpaceStartLog = 122, 
	FUNCTION_BACKUP_PrintBackupTableSpaceLog = 123, 
	FUNCTION_BACKUP_PrintBackupTableSpaceResult = 124
};

enum functionNumberRestore
{
	FUNCTION_RESTORE_Restore = 1, 
	FUNCTION_RESTORE_InitRestore = 2, 
	FUNCTION_RESTORE_InnerMessageTransfer = 3, 
	
	FUNCTION_RESTORE_StartRestore = 10, 
	FUNCTION_RESTORE_Reader = 11, 
	FUNCTION_RESTORE_Writer = 12, 
	FUNCTION_RESTORE_CheckRestoreFile = 13, 
	FUNCTION_RESTORE_MakeRestoreFile = 14, 
	FUNCTION_RESTORE_MakeRestoreRegularFile = 15, 
	FUNCTION_RESTORE_MakeRestoreRawDeviceFile = 16, 
	FUNCTION_RESTORE_WriteToFile = 17, 
	FUNCTION_RESTORE_WriteToRegularFile = 18, 
	FUNCTION_RESTORE_WriteToRawDeviceFile = 19, 
	FUNCTION_RESTORE_WriteToCompressFile = 20, 
	FUNCTION_RESTORE_CloseFile = 21, 
	FUNCTION_RESTORE_CloseRestoreFile = 22, 
	FUNCTION_RESTORE_SetFileTime = 23, 
	FUNCTION_RESTORE_MakeDirectory = 24, 
	FUNCTION_RESTORE_WriteRunningLog = 25, 
	
	FUNCTION_RESTORE_ServerError = 100, 
	FUNCTION_RESTORE_WaitForServerError = 101, 
	FUNCTION_RESTORE_Error = 102, 
	FUNCTION_RESTORE_ReleaseResource = 103, 
	FUNCTION_RESTORE_Exit = 104, 
	
	FUNCTION_RESTORE_PrintRestoreStartLog = 120, 
	FUNCTION_RESTORE_PrintRestoreFileLog = 121, 
	FUNCTION_RESTORE_PrintRestoreFailLog = 122, 
	FUNCTION_RESTORE_PrintRestoreResult = 123
};



/**************************************************************************************************
 * function prototype : 
 * DESCRIPTION
 *
 * ARGUMENTS
 *
 * RETURN VALUES
 *************************************************************************************************/


#endif
