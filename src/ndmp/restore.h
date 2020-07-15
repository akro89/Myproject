#ifndef __NDMP_RESTORE_H__
#define __NDMP_RESTORE_H__


#define NDMP_POLLING_INTERVAL		10

struct backupsetRestoreFile
{
	char * sourceFile;					// �� backupset���� ��������� ����
	char * targetFile;					// �� backupset���� ��������� ������ ������� ���

	__uint64 inode;						// ndmp inode
	__uint64 fh_info;					// ndmp file history info
};

struct backupsetFile
{
	char backupset[BACKUPSET_ID_LENGTH];	// ��������� backupset
	int copyNumber;							// ��������� backupset�� copy number
	__uint64 fileSize;						// �� backupset�� data size
	
	int	mediaNumber;						// �� backupset���� ��������� �� ����� media�� ����
	char ** mediaName;						// �� backupset���� ��������� ������ ����ִ� media
	__uint64 * backupsetPosition;			// �� backupset���� ��������� �� ����� �� media�� backupset position
	
	int fileNumber;							// �� backupset���� ��������� ������ ����

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
