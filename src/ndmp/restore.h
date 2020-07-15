#ifndef __NDMP_RESTORE_H__
#define __NDMP_RESTORE_H__


#define NDMP_POLLING_INTERVAL		10

struct backupsetRestoreFile
{
	char * sourceFile;					// 이 backupset에서 리스토어할 파일
	char * targetFile;					// 이 backupset에서 리스토어할 파일의 리스토어 경로

	__uint64 inode;						// ndmp inode
	__uint64 fh_info;					// ndmp file history info
};

struct backupsetFile
{
	char backupset[BACKUPSET_ID_LENGTH];	// 리스토어할 backupset
	int copyNumber;							// 리스토어할 backupset의 copy number
	__uint64 fileSize;						// 이 backupset의 data size
	
	int	mediaNumber;						// 이 backupset에서 리스토어할 때 사용할 media의 갯수
	char ** mediaName;						// 이 backupset에서 리스토어할 파일이 들어있는 media
	__uint64 * backupsetPosition;			// 이 backupset에서 리스토어할 때 사용할 각 media의 backupset position
	
	int fileNumber;							// 이 backupset에서 리스토어할 파일의 개수

	struct backupsetRestoreFile * file;
};


int InitProcess(char * filename);
int GetModuleConfiguration();
void LoadLogDescription();
int InitNdmpServer();
void SendCommand(int request, int reply, int execute, char * requestCommand, char * subCommand, char * executeCommand, char * sourceIP, char * sourcePort, char * sourceNodename, char * destinationIP, char * destinationPort, char * destinationNodename);
va_thread_return_t InnerMessageTransfer(void * arg);

int Restore();
int GetBackupsetFileList();
NdmpConnection MakeNewConnection();
va_thread_return_t NdmpRestore(void * arg);
int AddRestoreList(void * connection, int idx);
int va_compare_restore_file_list(const void * a1, const void * a2);

int LoadNdmpTapeMedia(void * connection);
void UnloadNdmpTapeMedia(void * connection);
int CheckNdmpLabeling(void * connection);

int MountNdmpNextVolume(void * connection, char * nextMedia);

va_thread_return_t WriteRunningLog(void * arg);
void UpdateJobStatus();
void UpdateJobState(enum jobState state);

void CancelJob(struct ckBackup * recvCommandData);
void WaitForRestoreComplete();
void Error(int functionNumber, int errorNumber);
void Exit();

void SendJobLog(char * logMsgID, ...);


enum functionNumber
{
	FUNCTION_InitProcess = 1,
	FUNCTION_GetModuleConfiguration = 2,
	FUNCTION_InitNdmpServer = 3,
	FUNCTION_SendCommand = 4,
	FUNCTION_InnerMessageTransfer = 5,
	
	FUNCTION_Restore = 10,
	FUNCTION_GetBackupsetFileList = 11,
	FUNCTION_MakeNewConnection = 12,
	FUNCTION_NdmpRestore = 13,
	FUNCTION_AddRestoreList = 14,
	FUNCTION_LoadNdmpTapeMedia = 15,
	FUNCTION_UnloadNdmpTapeMedia = 16,
	FUNCTION_CheckNdmpLabeling = 17,
	FUNCTION_MountNdmpNextVolume = 18,
	FUNCTION_WriteRunningLog = 19,
	FUNCTION_UpdateJobState = 20,
	FUNCTION_UpdateJobStatus = 21,
	
	FUNCTION_CancelJob = 100,
	FUNCTION_WaitForRestoreComplete = 101,
	FUNCTION_Error = 102,
	FUNCTION_Exit = 103
};


#endif
