#ifndef __CLIENT_HTTPD_H__
#define __CLIENT_HTTPD_H__
#define PROCESS_USAGE		10

int InitProcess(char * filename);
int GetModuleConfiguration();
int GetModuleConfigurationValue(char *ckFolder,char *moduleName,char * confFileName, char* optionName , char *value);
void ResponseRequest();
void GetFileList();
int sendEntireFileList(va_sock_t commandSock,char* backupFolder, int multiSessionNumber);
void GetEntireFileList();
void GetDirectoryList();
void GetFilesystemList();
void PutFileList();
void MakeDirectory();
void MakeFile();
void RemoveFile();

void GetDiskInventoryVolumeList();
void GetTapeLibraryInventory();
int MoveMedium();

void GetNodeModule();
void GetModuleConf();
void SetModuleConf();
void GetCatalogInfo();

void ServiceStart();
void ServiceStop();

void ShrinkClientCatalog();
//void ShrinkCatalog();

void DeleteSlaveCatalog();
void DeleteSlaveLog();

int va_mapping_moduleName_confFile(char * confFileName, char *moduleName);
int va_convert_moduleName_actionList(char* actionListModuleName , char *moduleName);

void GetNodeInformation();
void GetCheckProcessUsage();
void ExecuteGetProcessUasge(char *commandName);
void ExcuteGetProcessUsageAIX();
void ExpireBackupset();
//ky88
void ExpireNotesBackupset();
void CreateCheckProcess(char * fileName);
void LoadOutPutFile();

void CleaningMoveMedium();


#define CheckProcessOutPutFile			"CheckProcess.out"
#ifdef __ABIO_UNIX
	#define CheckProcessUsage		"CheckProcessUsage.sh"
	#define GETPROCESSPID				"GetProcessPID.sh"
#elif __ABIO_WIN
	#define CheckProcessUsage		"CheckProcessUsage.cmd"
#endif
#ifdef __ABIO_WIN
	int GetFilesystemListWin(va_sock_t commandSock);
#elif __ABIO_AIX
	int GetFilesystemListAIX(va_sock_t commandSock);
#elif __ABIO_SOLARIS
	int GetFilesystemListSolaris(va_sock_t commandSock);
#elif __ABIO_HP
	int GetFilesystemListHP(va_sock_t commandSock);
	void CopyMntent(struct mntent * dest, struct mntent * src);
#elif __ABIO_LINUX
	int GetFilesystemListLinux(va_sock_t commandSock);
	void CopyMntent(struct mntent * dest, struct mntent * src);
#endif

#ifdef __ABIO_WIN
	int GetRootFileList(va_sock_t commandSock);
	int GetRootDirectoryList(va_sock_t commandSock);
#endif

int GetFileTypeAbio(struct abio_file_stat * file_stat);

void Exit();

//ky88
#define NOTES_CATALOG_DB_LATEST			"catalog.db.NOTES"

struct checkprocessitem
{
	char cpuusage[PROCESS_USAGE];
	char memusage[ABIO_FILE_LENGTH];
	char processname[ABIO_FILE_LENGTH];
};


enum functionNumber
{
	FUNCTION_InitProcess = 1,
	FUNCTION_GetModuleConfiguration = 2,
	FUNCTION_ResponseRequest = 3,
	
	FUNCTION_GetFileList = 10,
	FUNCTION_GetDirectoryList = 11,
	FUNCTION_GetFilesystemList = 12,
	FUNCTION_PutFileList = 13,
	FUNCTION_MakeDirectory = 14,
	FUNCTION_CreateFile = 15,
	FUNCTION_RemoveFile = 16,
	
	FUNCTION_GetDiskInventoryVolumeList = 20,
	FUNCTION_GetTapeLibraryInventory = 21,
	FUNCTION_MoveMedium = 22,
	
	FUNCTION_GetNodeModule = 40,
	FUNCTION_ExpireBackupset = 41,
	FUNCTION_GetNodeInformation = 42,
	
	FUNCTION_GetFilesystemListAIX = 50,
	FUNCTION_GetFilesystemListSolaris = 51,
	FUNCTION_GetFilesystemListHP = 52,
	FUNCTION_GetFilesystemListLinux = 53,
	FUNCTION_CopyMntent = 54,
	
	FUNCTION_GetRootFileList = 60,
	FUNCTION_GetRootDirectoryList = 61,
	
	FUNCTION_GetFileType = 90,
	
	FUNCTION_Exit = 100,
};


/**************************************************************************************************
 * function prototype : 
 * DESCRIPTION
 *
 * ARGUMENTS
 *
 * RETURN VALUES
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : int InitProcess(char * filename);
 * DESCRIPTION
 *     process를 초기화한다.
 *     1. 전역 변수를 초기화하는 일
 *     2. directory 설정. ip, port 설정
 *     3. process 실행에 필요한 설정 값들을 읽어오는 일
 *
 * ARGUMENTS
 *     filename : main() 함수에 argv[0]로 들어온 program file name
 *
 * RETURN VALUES
 *     success : 0 is returned
 *     fail : -1 is returned
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : int GetModuleConfiguration();
 * DESCRIPTION
 *     process 실행에 필요한 설정값들을 읽어온다.
 *
 * ARGUMENTS
 *     none
 *
 * RETURN VALUES
 *     success : 0 is returned
 *     fail : -1 is returned
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : int ResponseRequest();
 * DESCRIPTION
 *     master server에서 온 sub command에 따라 요청을 처리하는 함수를 실행한다.
 *
 * ARGUMENTS
 *     none
 *
 * RETURN VALUES
 *     success : 0 is returned
 *     fail : -1 is returned
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : int GetFileList();
 * DESCRIPTION
 *     client의 지정된 folder의 파일 목록을 읽어서 master server로 전송한다.
 *     windows의 경우 root file system이 지정되면 drive 목록을 읽어서 보내준다.
 *
 * ARGUMENTS
 *     none
 *
 * RETURN VALUES
 *     success : 0 is returned
 *     fail : -1 is returned
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : int GetDirectoryList();
 * DESCRIPTION
 *     client의 지정된 folder의 sub directory 목록을 읽어서 master server로 전송한다.
 *     windows의 경우 root file system이 지정되면 drive 목록을 읽어서 보내준다.
 *
 * ARGUMENTS
 *     none
 *
 * RETURN VALUES
 *     success : 0 is returned
 *     fail : -1 is returned
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : int GetFilesystemList();
 * DESCRIPTION
 *     client의 file system 목록을 읽어서 master server로 전송한다.
 *     AIX, SOLARIS만 raw device backup이 지원된다.
 *
 * ARGUMENTS
 *     none
 *
 * RETURN VALUES
 *     success : 0 is returned
 *     fail : -1 is returned
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : int GetTapeDriveAddress();
 * DESCRIPTION
 *     client에 붙어있는 지정된 tape library의 tape drive 목록을 읽어서 master server로 전송한다.
 *
 * ARGUMENTS
 *     none
 *
 * RETURN VALUES
 *     success : 0 is returned
 *     fail : -1 is returned
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : int GetNodeModule();
 * DESCRIPTION
 *     client에 설치되어있는 VIRBAK ABIO module의 정보를 읽어서 master server로 전송한다.
 *
 * ARGUMENTS
 *     none
 *
 * RETURN VALUES
 *     success : 0 is returned
 *     fail : -1 is returned
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : int ExpireBackupset();
 * DESCRIPTION
 *     client의 latest catalog db에서 지정된 backupset에 들어있는 file을 expire시킨다.
 *     latest catalog db의 모든 file이 expire되면 latest catalog db를 삭제한다.
 *
 * ARGUMENTS
 *     none
 *
 * RETURN VALUES
 *     success : 0 is returned
 *     fail : -1 is returned
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : int GetRootFileList(va_sock_t commandSock);
 * DESCRIPTION
 *     windows에서 drive 목록을 읽어서 master server로 전송한다. 
 *     GetFileList()에서 root file system이 지정되었을 경우 실행된다.
 *
 * ARGUMENTS
 *     commandSock : master server에 연결되어있는 socket
 *
 * RETURN VALUES
 *     success : 0 is returned
 *     fail : -1 is returned
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : int GetRootDirectoryList(va_sock_t commandSock);
 * DESCRIPTION
 *     windows에서 drive 목록을 읽어서 master server로 전송한다. 
 *     GetDirectoryList()에서 root file system이 지정되었을 경우 실행된다.
 *
 * ARGUMENTS
 *     commandSock : master server에 연결되어있는 socket
 *
 * RETURN VALUES
 *     success : 0 is returned
 *     fail : -1 is returned
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : int GetFileType(struct abio_file_stat * file_stat);
 * DESCRIPTION
 *     파일의 정보를 받아서 file type을 돌려준다.
 *
 * ARGUMENTS
 *     fileStatus : 파일 정보
 *
 * RETURN VALUES
 *     file type
 *     0 : directory
 *     1 : regular file
 *     2 : character special file
 *     3 : block special file
 *     4 : fifo file
 *     5 : symbolic link file
 *     6 : socket file descriptor
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : void Exit();
 * DESCRIPTION
 *     process 종료에 필요한 작업들을 한다. 일반적으로 사용한 자원을 반납하고, 실행되던 thread가 있으면
 *     thread가 자연스럽게 종료되도록 할 수 있는 일들을 한다.
 *
 * ARGUMENTS
 *     none
 *
 * RETURN VALUES
 *     none
 *************************************************************************************************/


#endif
