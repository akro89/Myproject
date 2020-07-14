#ifndef __ORACLE_RMAN_MULTI_H__
#define __ORACLE_RMAN_MULTI_H__


#define ORACLE_QUERY_LINE_NUMBER			5

#define ORACLE_LINESIZE_QUERY				"set linesize 1024"
#define ORACLE_SPOOL_ON_QUERY				"spool %s%c%s"
#define ORACLE_CONNECT_QUERY				"connect %s/%s"
#define ORACLE_SPOOL_OFF_QUERY				"spool off"
#define ORACLE_QUIT_QUERY					"quit"
#define GET_ARCHIVE_LOG_DIR_QUERY			"select destination || '^' from v$archive_dest;"
#define GET_CONTROL_FILE_QUERY				"select name || '^' from v$controlfile;"
#define GET_ARCHIVE_LOG_MODE_QUERY			"select log_mode || '^' from v$database;"
#define GET_SPFILE_NAME_QUERY				"SELECT value||'^' FROM v$parameter WHERE name='spfile';"
#define SWITCH_LOG_FILE_QUERY				"ALTER SYSTEM SWITCH LOGFILE;"
#define ALTER_CONTROL_FILE_BINARY_QUERY		"ALTER DATABASE BACKUP CONTROLFILE TO '%s';"
#define ALTER_CONTROL_FILE_TRACE_QUERY		"ALTER DATABASE BACKUP CONTROLFILE TO TRACE AS '%s';"

#define CONTAINER_SEPARATOR					":"


#define ORACLE_RMAN_MAX_SCRIPT_ARGUMENT_NUMBER		32

enum backupFailCause {READ_FILE_ERROR, OPEN_FILE_ERROR, FILE_LENGTH_ERROR};
enum restoreFailCause {WRITE_FILE_ERROR, MAKE_FILE_ERROR, RESTORE_IN_PROGRESS};
enum oracleRmanConnectType {ORACLE_RMAN_CONNECT_BACKUP_TABLESPACE, ORACLE_RMAN_CONNECT_BACKUP_SPFILE, ORACLE_RMAN_CONNECT_RESTORE_TABLESPACE, ORACLE_RMAN_CONNECT_RESTORE_SPFILE,ORACLE_RMAN_CONNECT_BACKUP_ARCHIVELOG, ORACLE_RMAN_CONNECT_BACKUP_COMMAND_ARCHIVELOG, ORACLE_RMAN_CONNECT_RESTORE_ARCHIVELOG, ORACLE_RMAN_CONNECT_RESTORE_COMMAND_ARCHIVELOG, ORACLE_RMAN_CONNECT_REMOVE_ARCHIVELOG};

#define ORACLE_RMAN_DELETE_ARCHIVE_START	"DELETE START"
#define ORACLE_RMAN_DELETE_ARCHIVE_COMPLETE	"DELETE COMPLETE"


#ifdef __ABIO_UNIX
	#define EXCUTE_RMAN_FILE_FORMAT		"excute.sh"
	#define EXPIRE_ARCHIVE_FORMAT		"expire.sh"
#elif __ABIO_WIN
	#define EXCUTE_RMAN_FILE_FORMAT		"excute.cmd"
	#define EXPIRE_ARCHIVE_FORMAT		"expire.cmd"
#endif

typedef struct
{
	int pid;
	va_sock_t sock;
	int fileNumber;
	int fileCompleteNumber;
	enum oracleRmanConnectType connectType;
} rman_fd_t;

struct tableSpaceItem
{
	char tableSpace[ABIO_FILE_LENGTH];
	char ** dataFile;
	int fileNumber;
	__uint64 size;
};

struct archiveLogItem
{
	char * name;
	struct abio_file_stat st;
};

struct restoreFileObject
{
	char * source;
	char * target;
	char backupset[BACKUPSET_ID_LENGTH];
};

struct restoreTableSpaceObject
{
	char backupset[BACKUPSET_ID_LENGTH];
};

struct restoreFile
{
	enum fileType filetype;
	
	char tableSpaceName[ABIO_FILE_LENGTH];
	char untilTime[TIME_STAMP_LENGTH];
	
	int objectNumber;
	void * object;
	char dataFileFrom[ABIO_FILE_LENGTH];
	char dataFileTo[ABIO_FILE_LENGTH];
};


struct deleteArchive
{
	char seqenceNumber[ABIO_FILE_LENGTH];
	char threadNumber[ABIO_FILE_LENGTH];
};

int DeleteArchiveLog(char * sequence, char * thread);
// in oracle.cpp
int InitProcess(char * filename);
int GetModuleConfiguration();
void LoadLogDescription();


void CreateExecuteFileFormat();
int ConnectOracleRman(rman_fd_t * fdOracleRman, char * backupTargetName, char * backupTargetDatabase, char * backupset, char * restoreFileListPath);
int ConnectOracleRmanRestore(rman_fd_t * fdOracleRman, char * backupTargetName, char * backupset, char * restoreFileListPath, char * dataFileFrom, char * dataFileTo);
int ExecuteSql(char * query);
int ExecuteSqlCmd(char * queryFileName);
int ExcuteRmanExpireBackupset(char * queryFileName, char * backupsetID);

//
int ExecuteVersionSql(char * queryFileName);
int ExecuteCdbSql(char * queryFileName);
int getDBVersion();
int getCDB();
void PrintQueryFileError(char * logMsgID, ...);
//
int MakeQueryFile(char * query);
int CheckQueryError(char * query);
void PrintQueryError();
int CheckQueryResult(rman_fd_t * fdOracleRman);

void SendJobLog(char * logMsgID, ...);

void CreateCommandFile(char* fileType,char* filename, char* tablespace, char* database, char* tag, char * thread);
void CreateCommandFileDataFile(char* filename, char* tablespace, char* database, char * backupset, char * dataFileFrom, char * dataFileTo);
// in backup.cpp
int Backup();
void RemoveASMControlBackupFile(char *file);
void CreateASMControlBackupFileCTL(char* file);
void CreateASMControlBackupFileTRC(char* file);
//void ASMControlBackupFile(char* filename);
void CreateExecuteFile(char* file);
int ASMControlBackupCTL();
int ASMControlBackupTRC();
int RemoveBackupFile();



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



/**************************************************************************************************
 * function prototype : 
 * DESCRIPTION
 *
 * ARGUMENTS
 *
 * RETURN VALUES
 *************************************************************************************************/


#endif
