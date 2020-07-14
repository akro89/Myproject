#ifndef __ORAHTTPD_H__
#define __ORAHTTPD_H__


#define CONTAINER_SEPARATOR			":"

struct tableSpaceItem
{
	char tableSpace[ABIO_FILE_LENGTH];
	char database[ABIO_FILE_LENGTH];
	char ** dataFile;
	int fileNumber;
	int fileNumber2;
	time_t mtime;
	__uint64 size;
};

struct oracleRmanBackupsetItem
{
	char backupset[ABIO_FILE_LENGTH];
	char handle[ABIO_FILE_LENGTH];
};

void SendJobLog(char * logMsgID, ...);
int InitProcess(char * filename);
int GetModuleConfiguration();
void LoadLogDescription();
void PrintError();
void ResponseRequest();

int RestoreArchivePreview();

void GetOracleRmanFileList();

void GetOracleRmanRestoreBackupsetList();
int ExcuteRmanRestoreBackupsetList(char * queryFileName, char * tablespace, char * untilTime);

void SyncOracleRmanBackupsetList();
int ExcuteRmanBackupsetList(char * queryFileName);
void ExpireOracleRmanBackupset();
void ExpireOracleRmanBackupsetSilent();
int ExcuteRmanExpireBackupset(char * queryFileName, char * backupsetID);
int ExcuteRmanArchivePreview(char * queryFileName, char* tablespacename, char * containerName, char * backupsetID);

int GetTableSpaceList();
int GetArchiveLogSize();
int GetControlFileSize();
void ExpireBackupset();
int ParsingOracleRmanBackupsetList(char * outputFilePath, char * outputFileName, struct oracleRmanBackupsetItem ** ptrBackupsetList);
int ExcuteRman(char * queryFileName, char * outputFileName);
int ExecuteTablespaceSql(char * queryFileName, int version);
int ExecuteVersionSql(char * queryFileName);
void PrintQueryFileError(char * logMsgID, ...);
int ComparatorRmanBackupsetDescending(const void * a1, const void * a2);

void CreateCommandFile(char* filename, char* tablespace, char * container ,char* tag);
void CreateExecuteFile(char* file);
void Exit();
int getDBVersion();
void GetEntireDatabases();

/**************************************************************************************************
 * function prototype : 
 * DESCRIPTION
 *
 * ARGUMENTS
 *
 * RETURN VALUES
 *************************************************************************************************/


#endif
