#include "require.h"
#include "abio.h"
#include "lib.h"
#include "vendor.h"
#include "abio_scan.h"


extern enum volumeType mountedVolumeType;

extern char driveDeviceFile[STORAGE_DEVICE_FILE_LENGTH];
extern char diskVolumeFile[MAX_PATH_LENGTH];

extern char mountedVolume[VOLUME_NAME_LENGTH];
extern int volumeIdentify;		// 0 : didn't read yet. 1 : virbak abio type. 2 : cannot identify
extern int volumeBlockSize;
extern int dataBufferSize;

extern struct backupsetInfo * backupsetList;
extern int backupsetListNumber;

extern char restoreBackupset[BACKUPSET_ID_LENGTH];
extern char restoreClient[NODE_NAME_LENGTH];
extern char restoreDirectory[MAX_PATH_LENGTH];
extern int * restoreFileList;
extern int restoreFileNumber;

extern int successFileNumber;
extern int failFileNumber;

//ky88 2012.03.22
extern char currentDirectory[MAX_PATH_LENGTH];

void MenuRestore()
{
	char input[DSIZ];
	
	
	
	// volume이 scan이 되었는지 확인한다.
	if (volumeIdentify == 0)
	{
		printf("\n\n");
		printf("Did not scan a volume yet. You have to scan a volume first.\n");
		
		WaitContinue();
		
		return;
	}
	else if (volumeIdentify == 2)
	{
		printf("\n\n");
		printf("This volume is not a valid %s volume.\n", CURRENT_VERSION_DISPLAY_TYPE);
		
		WaitContinue();
		
		return;
	}
	
	
	// backupset list가 scan이 되었는지 확인한다.
	if (backupsetListNumber == 0)
	{
		printf("\n\n");
		printf("There are no scanned backup sets. You have to scan backup set first.\n");
		
		WaitContinue();
		
		return;
	}
	
	
	while (1)
	{
		printf("\n\n");
		printf("Restore files\n");
		
		MenuStatus();
		
		if (restoreBackupset[0] == '\0')
		{
			printf("0. Select the backup set that you want to restore\n");
			printf("Select : [0] ");
			
			gets(input);
			if (input[0] == '\0')
			{
				SetRestoreBackupset();
			}
			else if (strlen(input) == 1)
			{
				if (input[0] == '0')
				{
					SetRestoreBackupset();
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
		else
		{
			printf("0. Restore all of files in the backup set\n");
			printf("1. Add restore files\n");
			printf("2. Remove restore files\n");
			printf("3. Reset restore files\n");
			printf("4. Check out restore files\n");
			printf("5. Set a restore destination directory\n");
			printf("6. Show backup files\n");
			printf("r. Start restore\n");
			printf("Select : ");
			
			gets(input);
			if (strlen(input) == 1)
			{
				if (input[0] == '0')
				{
					Restore();
				}
				else if (input[0] == '1')
				{
					AddRestoreFiles();
				}
				else if (input[0] == '2')
				{
					RemoveRestoreFiles();
				}
				else if (input[0] == '3')
				{
					ResetRestoreFiles();
				}
				else if (input[0] == '4')
				{
					CheckRestoreFiles();
				}
				else if (input[0] == '5')
				{
					SetRestoreDirectory();
				}
				else if (input[0] == '6')
				{
					ShowBackupFiles();
				}
				else if (input[0] == 'r' || input[0] == 'R')
				{
					if (restoreFileNumber == 0)
					{
						printf("\n");
						printf("There are no seleted restore files.\n");
					}
					else
					{
						Restore();
					}
				}
				else if (input[0] == 'q' || input[0] == 'Q')
				{
					if (QuitMenu() == 1)
					{
						break;
					}
				}
				
				WaitContinue();
			}
		}
	}
	
	
	// 리스토어 작업이 끝나면 기존의 리스토어 정보를 삭제한다.
	memset(restoreBackupset, 0, sizeof(restoreBackupset));
	memset(restoreClient, 0, sizeof(restoreClient));
	memset(restoreDirectory, 0, sizeof(restoreDirectory));
	va_free(restoreFileList);
	restoreFileList = NULL;
	restoreFileNumber = 0;
	
	successFileNumber = 0;
	failFileNumber = 0;
}

void SetRestoreBackupset()
{
	char input[DSIZ];
	
	int numericInput;
	
	int i;
	
	
	
	while (1)
	{
		printf("\n");
		printf("Select backup set id number. If you want to see backup sets of this volume, then input 'l' : ");
		
		gets(input);
		if (strlen(input) == 1)
		{
			if (input[0] == 'l' || input[0] == 'L')
			{
				printf("\n");
				
				for (i = 0; i < backupsetListNumber; i++)
				{
					printf("%d. backup set id : %s\tclient : %s\t\tbackup time : %s\n", i + 1, backupsetList[i].backupset, backupsetList[i].client, backupsetList[i].backupTime); 
				}
			}
			else if (input[0] == 'q' || input[0] == 'Q')
			{
				if (QuitMenu() == 1)
				{
					break;
				}
			}
			else if ('1' <= input[0] && input[0] <= '9' && atoi(input) <= backupsetListNumber)
			{
				memset(restoreBackupset, 0, sizeof(restoreBackupset));
				memset(restoreClient, 0, sizeof(restoreClient));
				strcpy(restoreBackupset, backupsetList[atoi(input) - 1].backupset);
				strcpy(restoreClient, backupsetList[atoi(input) - 1].client);
				
				break;
			}
		}
		else if (strlen(input) > 1)
		{
			numericInput = 1;
			
			for (i = 0; i < (int)strlen(input); i++)
			{
				// 요것이 1로 되어 있어 10일때 체크안됨.왜 1일까?ㅎㅎㅎ
				if (input[i] < '0' || input[i] > '9')
				//if (input[i] < '1' || '9' < input[i])
				{
					numericInput = 0;
					
					break;
				}
			}
			
			if (numericInput == 1 && atoi(input) <= backupsetListNumber)
			{
				memset(restoreBackupset, 0, sizeof(restoreBackupset));
				memset(restoreClient, 0, sizeof(restoreClient));
				strcpy(restoreBackupset, backupsetList[atoi(input) - 1].backupset);
				strcpy(restoreClient, backupsetList[atoi(input) - 1].client);
				
				break;
			}
		}
	}
}

void AddRestoreFiles()
{
	char input[DSIZ];
	int validInput;
	
	char item[DSIZ][DSIZ];
	int itemNumber;
	
	int i;
	int j;
	int k;
	int len;
	
	int rangeInput;
	char strStartNumber[DSIZ];
	char strLastNumber[DSIZ];
	int startNumber;
	int lastNumber;
	
	int backupsetIndex;
	
	
	
	for (i = 0; i < backupsetListNumber; i++)
	{
		if (!strcmp(backupsetList[i].backupset, restoreBackupset))
		{
			backupsetIndex = i;
			
			break;
		}
	}
	
	if (i == backupsetListNumber)
	{
		printf("\n");
		printf("Restore backup set is not in scanned backup sets.\n");
		
		return;
	}
	
	
	printf("\n");
	printf("Add restore files\n");
	printf("Input restore files number like this '1,3-10,14,20'. Do not remain blank.\n");
	
	while (1)
	{
		printf("Number : ");
		
		gets(input);
		if (input[0] == '\0')
		{
			break;
		}
		else if (strlen(input) == 1 && (input[0] == 'q' || input[0] == 'Q'))
		{
			break;
		}
		else
		{
			// check out valid input
			validInput = 1;
			for (i = 0; i < (int)strlen(input); i++)
			{
				if ((input[i] < '0' || '9' < input[i]) && input[i] != ',' && input[i] != '-')
				{
					validInput = 0;
					
					break;
				}
			}
			
			if (validInput == 0)
			{
				printf("Invalid input\n");
			}
			else
			{
				// parse input
				
				// first, parse input by ','
				for (i = 0; i < DSIZ; i++)
				{
					memset(item[i], 0, DSIZ);
				}
				
				len = (int)strlen(input);
				j = 0;
				k = 0;
				for (i = 0; i < len; i++)
				{
					if (input[i] == ',')
					{
						strncpy(item[k], input + j, i - j);
						k++;
						j = i + 1;
					}
				}
				strcpy(item[k], input + j);
				k++;
				
				itemNumber = k;
				
				
				// second, identify number
				for (i = 0; i < itemNumber; i++)
				{
					rangeInput = 0;
					
					for (j = 0; j < (int)strlen(item[i]); j++)
					{
						if (item[i][j] == '-')
						{
							rangeInput = 1;
							
							break;
						}
					}
					
					if (rangeInput == 0)
					{
						if (atoi(item[i]) <= backupsetList[backupsetIndex].fileNumber && SearchRestoreFile(atoi(item[i])) < 0)
						{
							if (restoreFileNumber % DEFAULT_MEMORY_FRAGMENT == 0)
							{
								restoreFileList = (int *)realloc(restoreFileList, sizeof(int) * (restoreFileNumber + DEFAULT_MEMORY_FRAGMENT));
								memset(restoreFileList + restoreFileNumber, 0, sizeof(int) * DEFAULT_MEMORY_FRAGMENT);
							}
							restoreFileList[restoreFileNumber] = atoi(item[i]);
							restoreFileNumber++;
						}
					}
					else
					{
						// parse range input
						memset(strStartNumber, 0, sizeof(strStartNumber));
						strncpy(strStartNumber, item[i], j);
						startNumber = atoi(strStartNumber);
						
						memset(strLastNumber, 0, sizeof(strLastNumber));
						strcpy(strLastNumber, item[i] + j + 1);
						lastNumber = atoi(strLastNumber);
						
						for (j = startNumber; j <= lastNumber; j++)
						{
							if (j <= backupsetList[backupsetIndex].fileNumber && SearchRestoreFile(j) < 0)
							{
								if (restoreFileNumber % DEFAULT_MEMORY_FRAGMENT == 0)
								{
									restoreFileList = (int *)realloc(restoreFileList, sizeof(int) * (restoreFileNumber + DEFAULT_MEMORY_FRAGMENT));
									memset(restoreFileList + restoreFileNumber, 0, sizeof(int) * DEFAULT_MEMORY_FRAGMENT);
								}
								restoreFileList[restoreFileNumber] = j;
								restoreFileNumber++;
							}
						}
					}
				}
			}
		}
	}
	
	
	qsort(restoreFileList, restoreFileNumber, sizeof(int), ComparatorRestoreFile);
}

void RemoveRestoreFiles()
{
	char input[DSIZ];
	int validInput;
	
	char item[DSIZ][DSIZ];
	int itemNumber;
	
	int i;
	int j;
	int k;
	int len;
	
	int rangeInput;
	char strStartNumber[DSIZ];
	char strLastNumber[DSIZ];
	int startNumber;
	int lastNumber;
	
	int * newRestoreFileList;
	int newRestoreFileNumber;
	
	
	
	// initialize variables
	newRestoreFileList = NULL;
	newRestoreFileNumber = 0;
	
	
	if (restoreFileNumber == 0)
	{
		printf("\n");
		printf("There are no selected restore files.\n");
		
		return;
	}
	
	
	printf("\n");
	printf("Remove restore files\n");
	printf("Input restore files number like this '1,3-10,14,20'. Do not remain blank.\n");
	
	while (1)
	{
		printf("Number : ");
		
		gets(input);
		if (input[0] == '\0')
		{
			break;
		}
		else if (strlen(input) == 1 && (input[0] == 'q' || input[0] == 'Q'))
		{
			break;
		}
		else
		{
			// check out valid input
			validInput = 1;
			for (i = 0; i < (int)strlen(input); i++)
			{
				if ((input[i] < '0' || input[i] > '9') && input[i] != ',' && input[i] != '-')
				{
					validInput = 0;
					
					break;
				}
			}
			
			if (validInput == 0)
			{
				printf("Invalid input\n");
			}
			else
			{
				// parse input
				
				// first, parse input by ','
				for (i = 0; i < DSIZ; i++)
				{
					memset(item[i], 0, DSIZ);
				}
				
				len = (int)strlen(input);
				j = 0;
				k = 0;
				for (i = 0; i < len; i++)
				{
					if (input[i] == ',')
					{
						strncpy(item[k], input + j, i - j);
						k++;
						j = i + 1;
					}
				}
				strcpy(item[k], input + j);
				k++;
				
				itemNumber = k;
				
				
				// second, identify number
				for (i = 0; i < itemNumber; i++)
				{
					rangeInput = 0;
					
					for (j = 0; j < (int)strlen(item[i]); j++)
					{
						if (item[i][j] == '-')
						{
							rangeInput = 1;
							
							break;
						}
					}
					
					if (rangeInput == 0)
					{
						// 기존 restore file 목록에서 삭제하도록 지정한 파일을 삭제한다.
						if ((k = SearchRestoreFile(atoi(item[i]))) >= 0)
						{
							restoreFileList[k] = -1;
						}
					}
					else
					{
						// parse range input
						memset(strStartNumber, 0, sizeof(strStartNumber));
						strncpy(strStartNumber, item[i], j);
						startNumber = atoi(strStartNumber);
						
						memset(strLastNumber, 0, sizeof(strLastNumber));
						strcpy(strLastNumber, item[i] + j + 1);
						lastNumber = atoi(strLastNumber);
						
						// 기존 restore file 목록에서 삭제하도록 지정한 파일을 삭제한다.
						for (j = startNumber; j <= lastNumber; j++)
						{
							if ((k = SearchRestoreFile(j)) >= 0)
							{
								restoreFileList[k] = -1;
							}
						}
					}
				}
				
				// 삭제 후 남아있는 restore file이 있는지 확인한다.
				for (i = 0; i < restoreFileNumber; i++)
				{
					if (restoreFileList[i] != -1)
					{
						break;
					}
				}
				
				if (i == restoreFileNumber)
				{
					break;
				}
			}
		}
	}
	
	
	// 기존 restore file 목록에서 삭제하도록 지정한 파일을 삭제한다.
	for (i = 0; i < restoreFileNumber; i++)
	{
		if (restoreFileList[i] != -1)
		{
			if (newRestoreFileNumber % DEFAULT_MEMORY_FRAGMENT == 0)
			{
				newRestoreFileList = (int *)realloc(newRestoreFileList, sizeof(int) * (newRestoreFileNumber + DEFAULT_MEMORY_FRAGMENT));
				memset(newRestoreFileList + newRestoreFileNumber, 0, sizeof(int) * DEFAULT_MEMORY_FRAGMENT);
			}
			newRestoreFileList[newRestoreFileNumber] = restoreFileList[i];
			newRestoreFileNumber++;
		}
	}
	
	va_free(restoreFileList);
	restoreFileList = newRestoreFileList;
	restoreFileNumber = newRestoreFileNumber;
}

void ResetRestoreFiles()
{
	va_free(restoreFileList);
	restoreFileList = NULL;
	restoreFileNumber = 0;
}

void CheckRestoreFiles()
{
	char backupFileListFile[DSIZ];
	
	char ** lines;
	int lineNumber;
	
	#ifdef __ABIO_WIN
		char * convertLine;
		int convertLineSize;
	#endif
	
	int i;
	
	
	
	printf("\n");
	
	
	memset(backupFileListFile, 0, sizeof(backupFileListFile));
	sprintf(backupFileListFile, "%s.txt", restoreBackupset);
	
	//ky88 2012.03.22
	if ((lineNumber = va_load_text_file_lines(currentDirectory, backupFileListFile, &lines, NULL)) > 0)
	//if ((lineNumber = va_load_text_file_lines(NULL, backupFileListFile, &lines, NULL)) > 0)
	{
		for (i = 0; i < lineNumber; i++)
		{
			if (SearchRestoreFile(i + 1) >= 0)
			{
				#ifdef __ABIO_UNIX
					printf("%s\n", lines[i]);
				#elif __ABIO_WIN
					if (va_convert_string_from_utf8(ENCODING_UNKNOWN, lines[i], (int)strlen(lines[i]), (void **)&convertLine, &convertLineSize) > 0)
					{
						printf("%s\n", convertLine);
						va_free(convertLine);
					}
				#endif
			}
		}
		
		for (i = 0; i < lineNumber; i++)
		{
			va_free(lines[i]);
		}
		va_free(lines);
	}
	else
	{
		printf("Cannot open the file '%s' which contains files in this backup set.\n", backupFileListFile);
	}
}

void SetRestoreDirectory()
{
	char input[DSIZ];
	
	
	
	if (restoreDirectory[0] != '\0')
	{
		while (1)
		{
			printf("\n");
			printf("You did set the restore destination directory. Do you want to update it? (y/n) ");
			gets(input);
			
			if (strlen(input) == 1)
			{
				// 새로 리스토어할 backupset을 선택한 경우 기존의 리스토어 목록을 삭제한다.
				if (input[0] == 'y' || input[0] == 'Y')
				{
					memset(restoreDirectory, 0, sizeof(restoreDirectory));
					
					break;
				}
				else if (input[0] == 'n' || input[0] == 'N')
				{
					return;
				}
			}
		}
	}
	
	printf("\n");
	printf("Set a restore destination directory.\n");
	printf("Input a restore destination directory : ");
	
	while (1)
	{
		gets(input);
		if (input[0] == '\0')
		{
			break;
		}
		else if (strlen(input) == 1 && (input[0] == 'q' || input[0] == 'Q'))
		{
			break;
		}
		else
		{
			#ifdef __ABIO_UNIX
				if (va_is_valid_unix_path(input) == 0)
				{
					printf("Invalid directory.\n");
				}
				else
				{
					strcpy(restoreDirectory, input);
					
					if (strlen(input) > 1)
					{
						if (restoreDirectory[strlen(restoreDirectory) - 1] == FILE_PATH_DELIMITER)
						{
							restoreDirectory[strlen(restoreDirectory) - 1] = '\0';
						}
					}
					
					break;
				}
			#elif __ABIO_WIN
				if (va_is_valid_windows_path(input) == 0)
				{
					printf("Invalid directory.\n");
				}
				else
				{
					strcpy(restoreDirectory, input);
					
					if (strlen(input) > 3)
					{
						if (restoreDirectory[strlen(restoreDirectory) - 1] == FILE_PATH_DELIMITER)
						{
							restoreDirectory[strlen(restoreDirectory) - 1] = '\0';
						}
					}
					
					break;
				}
			#endif
		}
	}
}

void ShowBackupFiles()
{
	char backupFileListFile[DSIZ];
	
	char ** lines;
	int lineNumber;
	
	#ifdef __ABIO_WIN
		char * convertLine;
		int convertLineSize;
	#endif
	
	int i;
	
	printf("\n");
	
	
	memset(backupFileListFile, 0, sizeof(backupFileListFile));
	sprintf(backupFileListFile, "%s.txt", restoreBackupset);

//k88 2012.03.22
	if ((lineNumber = va_load_text_file_lines(currentDirectory, backupFileListFile, &lines, NULL)) > 0)
	//if ((lineNumber = va_load_text_file_lines(NULL, backupFileListFile, &lines, NULL)) > 0)
	{
		for (i = 0; i < lineNumber; i++)
		{
			#ifdef __ABIO_UNIX
				printf("%s\n", lines[i]);
			#elif __ABIO_WIN
				if (va_convert_string_from_utf8(ENCODING_UNKNOWN, lines[i], (int)strlen(lines[i]), (void **)&convertLine, &convertLineSize) > 0)
				{
					printf("%s\n", convertLine);
					va_free(convertLine);
				}
			#endif
		}
		
		for (i = 0; i < lineNumber; i++)
		{
			va_free(lines[i]);
		}
		va_free(lines);
	}
	else
	{
		printf("Cannot open the file '%s' which contains files int this backup set.\n", backupFileListFile);
	}
}

int ComparatorRestoreFile(const void * a1, const void * a2)
{
	int * l1;
	int * l2;
	
	
	l1 = (int *)a1;
	l2 = (int *)a2;
	
	return *l1 - *l2;
}

int SearchRestoreFile(int restoreFile)
{
	int middle;
	int left;
	int right;
	
	
	
	middle = 0;
	left = 0;
	right = restoreFileNumber - 1;
	
	while (right >= left)
	{
		middle = (right + left) / 2;
		
		if (restoreFile < restoreFileList[middle])
		{
			right = middle - 1;
		}
		else if (restoreFile > restoreFileList[middle])
		{
			left = middle + 1;
		}
		else
		{
			return middle;
		}
	}
	
	return -1;
}
