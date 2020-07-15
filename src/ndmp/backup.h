#ifndef __NDMP_BACKUP_H__
#define __NDMP_BACKUP_H__


#define NDMP_POLLING_INTERVAL		10


int InitProcess(char * filename);
int GetModuleConfiguration();
void LoadLogDescription();
int InitNdmpServer();
void SendCommand(int request, int reply, int execute, char * requestCommand, char * subCommand, char * executeCommand, char * sourceIP, char * sourcePort, char * sourceNodename, char * destinationIP, char * destinationPort, char * destinationNodename);
void SendCommand_changeBackupset(int request, int reply, int execute, char * requestCommand, char * subCommand, char * executeCommand, char * sourceIP, char * sourcePort, char * sourceNodename, char * destinationIP, char * destinationPort, char * destinationNodename, char * changedBackupset);

va_thread_return_t InnerMessageTransfer(void * arg);

int Backup();
NdmpConnection MakeNewConnection();
va_thread_return_t NdmpBackup(void * arg);
void CheckNdmpState(NdmpConnection connection);

int LoadNdmpTapeMedia(NdmpConnection connection);
int WriteMediaHeader(NdmpConnection connection, char * mediaName);
void UnloadNdmpTapeMedia(NdmpConnection connection);

int MakeCatalogDB();
int WriteCatalogDB(va_fd_t fdCatalog, dirinfo * dir, char * catalogBuf, int * catalogBufSize);
int SendCatalogToMaster();
int SendENDCommand();

void MakeVolumeDB(enum volumeUsage usage, struct volumeDB * returnVolumeDB);
int MountNdmpNewVolume(NdmpConnection connection);

int UpdateBackupsetList();
int SetNdmpDumpDate();

dirinfo * va_ndmp_file_make_dir_info_array_to_dir_info_debug(dirinfo * dir, dirinfoarray * dirarray, __uint64 maxPosition);
int va_ndmp_file_stat_index_ndmp_backup_file_array_search_debug(dirinfoarray * darray, __uint64 max, __uint64 node);

va_thread_return_t WriteRunningLog(void * arg);
void UpdateJobStatus();
void UpdateJobState(enum jobState state);

void CancelJob(struct ckBackup * recvCommandData);
void Error(int functionNumber, int errorNumber);
void Exit();

void SendJobLog(char * logMsgID, ...);


enum functionNumber
{
	FUNCTION_InitProcess = 1,
	FUNCTION_GetModuleConfiguration = 2,
	FUNCTION_LoadLogDescription = 3,
	FUNCTION_InitNdmpServer = 4,
	FUNCTION_SendCommand = 5,
	FUNCTION_InnerMessageTransfer = 6,
	
	FUNCTION_Backup = 10,
	FUNCTION_MakeNewConnection = 11,
	FUNCTION_NdmpBackup = 12,
	FUNCTION_CheckNdmpState = 13,
	FUNCTION_LoadNdmpTapeMedia = 14,
	FUNCTION_WriteMediaHeader = 15,
	FUNCTION_UnloadNdmpTapeMedia = 16,
	FUNCTION_MakeCatalogDB = 17,
	FUNCTION_WriteCatalogDB = 18,
	FUNCTION_SendCatalogToMaster = 19,
	FUNCTION_MakeVolumeDB = 20,
	FUNCTION_MountNdmpNewVolume = 21,
	FUNCTION_WriteRunningLog = 22,
	FUNCTION_UpdateJobState = 23,
	FUNCTION_UpdateJobStatus = 24,
	FUNCTION_SendENDCommand = 25,
	
	FUNCTION_CancelJob = 100,
	FUNCTION_Error = 101,
	FUNCTION_Exit = 102
};


#endif
