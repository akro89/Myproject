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
 *     process�� �ʱ�ȭ�Ѵ�.
 *     1. ���� ������ �ʱ�ȭ�ϴ� ��
 *     2. directory ����. ip, port ����
 *     3. process ���࿡ �ʿ��� ���� ������ �о���� ��
 *
 * ARGUMENTS
 *     filename : main() �Լ��� argv[0]�� ���� program file name
 *
 * RETURN VALUES
 *     success : 0 is returned
 *     fail : -1 is returned
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : int GetModuleConfiguration();
 * DESCRIPTION
 *     process ���࿡ �ʿ��� ���������� �о�´�.
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
 *     master server���� �� sub command�� ���� ��û�� ó���ϴ� �Լ��� �����Ѵ�.
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
 *     client�� ������ folder�� ���� ����� �о master server�� �����Ѵ�.
 *     windows�� ��� root file system�� �����Ǹ� drive ����� �о �����ش�.
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
 *     client�� ������ folder�� sub directory ����� �о master server�� �����Ѵ�.
 *     windows�� ��� root file system�� �����Ǹ� drive ����� �о �����ش�.
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
 *     client�� file system ����� �о master server�� �����Ѵ�.
 *     AIX, SOLARIS�� raw device backup�� �����ȴ�.
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
 *     client�� �پ��ִ� ������ tape library�� tape drive ����� �о master server�� �����Ѵ�.
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
 *     client�� ��ġ�Ǿ��ִ� VIRBAK ABIO module�� ������ �о master server�� �����Ѵ�.
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
 *     client�� latest catalog db���� ������ backupset�� ����ִ� file�� expire��Ų��.
 *     latest catalog db�� ��� file�� expire�Ǹ� latest catalog db�� �����Ѵ�.
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
 *     windows���� drive ����� �о master server�� �����Ѵ�. 
 *     GetFileList()���� root file system�� �����Ǿ��� ��� ����ȴ�.
 *
 * ARGUMENTS
 *     commandSock : master server�� ����Ǿ��ִ� socket
 *
 * RETURN VALUES
 *     success : 0 is returned
 *     fail : -1 is returned
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : int GetRootDirectoryList(va_sock_t commandSock);
 * DESCRIPTION
 *     windows���� drive ����� �о master server�� �����Ѵ�. 
 *     GetDirectoryList()���� root file system�� �����Ǿ��� ��� ����ȴ�.
 *
 * ARGUMENTS
 *     commandSock : master server�� ����Ǿ��ִ� socket
 *
 * RETURN VALUES
 *     success : 0 is returned
 *     fail : -1 is returned
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : int GetFileType(struct abio_file_stat * file_stat);
 * DESCRIPTION
 *     ������ ������ �޾Ƽ� file type�� �����ش�.
 *
 * ARGUMENTS
 *     fileStatus : ���� ����
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
 *     process ���ῡ �ʿ��� �۾����� �Ѵ�. �Ϲ������� ����� �ڿ��� �ݳ��ϰ�, ����Ǵ� thread�� ������
 *     thread�� �ڿ������� ����ǵ��� �� �� �ִ� �ϵ��� �Ѵ�.
 *
 * ARGUMENTS
 *     none
 *
 * RETURN VALUES
 *     none
 *************************************************************************************************/


#endif
