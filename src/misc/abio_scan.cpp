#include "require.h"
#include "abio.h"
#include "lib.h"
#include "vendor.h"
#include "abio_scan.h"


// start of variables for abio library comparability
char ABIOMASTER_CK_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_MASTER_SERVER_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_LOG_FOLDER[MAX_PATH_LENGTH];

char masterIP[IP_LENGTH];
char masterLogPort[PORT_LENGTH];

int requestLogLevel;
char logJobID[JOB_ID_LENGTH];
int logModuleNumber;
// end of variables for abio library comparability


enum volumeType mountedVolumeType;

char driveDeviceFile[STORAGE_DEVICE_FILE_LENGTH];
char diskVolumeFile[MAX_PATH_LENGTH];

char mountedVolume[VOLUME_NAME_LENGTH];
int volumeIdentify;		// 0 : didn't read yet. 1 : virbak abio type. 2 : cannot identify
int volumeBlockSize;
int dataBufferSize;

struct backupsetInfo * backupsetList;
int backupsetListNumber;

char restoreBackupset[BACKUPSET_ID_LENGTH];
char restoreClient[NODE_NAME_LENGTH];
char restoreDirectory[MAX_PATH_LENGTH];
int * restoreFileList;
int restoreFileNumber;

int successFileNumber;
int failFileNumber;

int tapeDriveTimeOut;
int tapeDriveDelayTime;

//ky88 2012.03.22 
//윈도우의 경우 현재디렉토리를 찾아서 작업 디렉토리로 사용하여야 하는 데...
//unc_path 로 조인 될 경우 파일이름이 \\?\backupset.txt 이런식으로 되어서 안됨.
//따라서 현재 디렉토리를 조사하여 사용.
//프로그램을 단순화 하기위하여 unix에서도 동일하게 수행하면...
char currentDirectory[MAX_PATH_LENGTH];

int main(int argc, char ** argv)
{
	// set a resource limit
	va_setrlimit();
	
	
	// initialize global variables
	mountedVolumeType = VOLUME_TAPE;
	
	memset(driveDeviceFile, 0, sizeof(driveDeviceFile));
	memset(diskVolumeFile, 0, sizeof(diskVolumeFile));
	
	memset(mountedVolume, 0, sizeof(mountedVolume));
	volumeIdentify = 0;
	volumeBlockSize = 0;
	dataBufferSize = 0;
	
	backupsetList = NULL;
	backupsetListNumber = 0;
	
	memset(restoreBackupset, 0, sizeof(restoreBackupset));
	memset(restoreClient, 0, sizeof(restoreClient));
	memset(restoreDirectory, 0, sizeof(restoreDirectory));
	restoreFileList = NULL;
	restoreFileNumber = 0;
	
	successFileNumber = 0;
	failFileNumber = 0;
	
	tapeDriveTimeOut = 60;
	tapeDriveDelayTime = 1;

	//ky88 2012.03.22
	memset(currentDirectory, 0x00, MAX_PATH_LENGTH);
#ifdef __ABIO_WIN
	GetCurrentDirectory(MAX_PATH_LENGTH, currentDirectory);
#endif

	MenuInit();
	
	
	return 0;
}

void MenuInit()
{
	char input[DSIZ];
	
	
	
	while (1)
	{
		printf("\n\n");
		printf("Volume scanning tool %s\n", CURRENT_VERSION_DISPLAY_TYPE);
		
		printf("1. Scan and restore files from tape volume\n");
		printf("2. Scan and restore files from disk volume\n");
		printf("Select : ");
		
		gets(input);
		if (strlen(input) == 1)
		{
			if (input[0] == '1')
			{
				mountedVolumeType = VOLUME_TAPE;
				
				MenuScan();
			}
			else if (input[0] == '2')
			{
				mountedVolumeType = VOLUME_DISK;
				
				MenuScan();
			}
			else if (input[0] == 'q' || input[0] == 'Q')
			{
				while (1)
				{
					printf("Do you want to quit? [y/N] ");
					
					gets(input);
					if (input[0] == '\0')
					{
						break;
					}
					else if (strlen(input) == 1)
					{
						if (input[0] == 'y' || input[0] == 'Y')
						{
							return;
						}
						else if (input[0] == 'n' || input[0] == 'N')
						{
							break;
						}
					}
				}
			}
		}
	}
}

void MenuScan()
{
	char input[DSIZ];
	
	
	
	while (1)
	{
		printf("\n\n");
		if (mountedVolumeType == VOLUME_TAPE)
		{
			printf("Scan and restore files from tape volume\n");
		}
		else
		{
			printf("Scan and restore files from disk volume\n");
		}
		
		MenuStatus();
		
		printf("1. Scan files\n");
		printf("2. Restore files\n");
		printf("Select : ");
		
		gets(input);
		if (strlen(input) == 1)
		{
			if (input[0] == '1')
			{
				MenuScanBackupFile();
			}
			else if (input[0] == '2')
			{
				MenuRestore();
			}
			else if (input[0] == 'q' || input[0] == 'Q')
			{
				if (QuitMenu() == 1)
				{
					break;
				}
			}
		}
	}
	
	
	// volume의 scan과 리스토어 작업이 끝나면 기존의 정보를 삭제한다.
	mountedVolumeType = VOLUME_TAPE;
	
	memset(driveDeviceFile, 0, sizeof(driveDeviceFile));
	memset(diskVolumeFile, 0, sizeof(diskVolumeFile));
	
	memset(mountedVolume, 0, sizeof(mountedVolume));
	volumeIdentify = 0;
	volumeBlockSize = 0;
	dataBufferSize = 0;
	
	va_free(backupsetList);
	backupsetList = NULL;
	backupsetListNumber = 0;
	
	memset(restoreBackupset, 0, sizeof(restoreBackupset));
	memset(restoreClient, 0, sizeof(restoreClient));
	memset(restoreDirectory, 0, sizeof(restoreDirectory));
	va_free(restoreFileList);
	restoreFileList = NULL;
	restoreFileNumber = 0;
	
	successFileNumber = 0;
	failFileNumber = 0;
}

void MenuStatus()
{
	int i;
	
	
	
	printf("\n");
	if (mountedVolumeType == VOLUME_TAPE)
	{
		if (driveDeviceFile[0] != '\0')
		{
			printf("Tape drive device file : [%s]\n", driveDeviceFile);
			
			if (volumeIdentify == 0)
			{
				printf("Tape volume : did not read yet\n");
			}
			else if (volumeIdentify == 1)
			{
				printf("Tape volume : %s\n", mountedVolume);
			}
			else if (volumeIdentify == 2)
			{
				printf("Tape volume : Unable to identify\n");
			}
			
			if (restoreBackupset[0] != '\0')
			{
				printf("Backup set id : %s\n", restoreBackupset);
				printf("Client : %s\n", restoreClient);
				
				for (i = 0; i < backupsetListNumber; i++)
				{
					if (!strcmp(backupsetList[i].backupset, restoreBackupset))
					{
						printf("Number of files in backup set : %d\n", backupsetList[i].fileNumber);
						
						break;
					}
				}
			}
			
			if (restoreDirectory[0] != '\0')
			{
				printf("Restore destination directory : %s\n", restoreDirectory);
			}
		}
	}
	else if (mountedVolumeType == VOLUME_DISK)
	{
		if (diskVolumeFile[0] != '\0')
		{
			printf("Disk volume file : %s\n", diskVolumeFile);
			
			if (volumeIdentify == 0)
			{
				printf("Disk volume : did not read yet\n");
			}
			else if (volumeIdentify == 1)
			{
				printf("Disk volume : %s\n", mountedVolume);
			}
			else if (volumeIdentify == 2)
			{
				printf("Disk volume : Unable to identify\n");
			}
			
			if (restoreBackupset[0] != '\0')
			{
				printf("Backup set id : %s\n", restoreBackupset);
				printf("Client : %s\n", restoreClient);
				
				for (i = 0; i < backupsetListNumber; i++)
				{
					if (!strcmp(backupsetList[i].backupset, restoreBackupset))
					{
						printf("Number of files in backup set : %d\n", backupsetList[i].fileNumber);
						
						break;
					}
				}
			}
			
			if (restoreDirectory[0] != '\0')
			{
				printf("Restore destination directory : %s\n", restoreDirectory);
			}
		}
	}
	printf("\n");
}

int QuitMenu()
{
	char input[DSIZ];
	
	
	
	while (1)
	{
		printf("Do you want to quit? [Y/n] ");
		
		gets(input);
		if (input[0] == '\0')
		{
			return 1;
		}
		else if (strlen(input) == 1)
		{
			if (input[0] == 'y' || input[0] == 'Y')
			{
				return 1;
			}
			else if (input[0] == 'n' || input[0] == 'N')
			{
				break;
			}
		}
	}
	
	return 0;
}

void WaitContinue()
{
	char input[DSIZ];
	
	
	
	printf("\n");
	printf("continue...");
	gets(input);
}
