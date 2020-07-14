#include "require.h"
#include "abio.h"
#include "lib.h"
#include "vendor.h"
#include "find_files.h"

// start of variables for abio library comparability
char ABIOMASTER_LOG_FOLDER[MAX_PATH_LENGTH];
// end of variables for abio library comparability
char ABIOMASTER_TOOL_FOLDER[MAX_PATH_LENGTH];

//////////////////////////////////////////////////////////////////////////////////////////////////
// master server ip and port
char masterIP[IP_LENGTH];
char masterPort[PORT_LENGTH];

//////////////////////////////////////////////////////////////////////////////////////////////////
// 입력받아야 하는 정보
char backupset[BACKUPSET_ID_LENGTH];	// backupset id
char findFileName[FIND_CATALOGLIST_LENGTH];	// length
char catalogFileName[CATALOGLIST_NAME];		// catalog name 


int main(int argc, char ** argv)
{
	char ** convertArgList;
	int convertArgSize;

	int i;

	// argument의 encoding을 변환한다.
	convertArgList = (char **)malloc(sizeof(char *) * (argc + 1));
	memset(convertArgList, 0, sizeof(char *) * (argc + 1));

	for (i = 0; i < argc; i++)
	{
		if (va_convert_string_to_utf8(ENCODING_UNKNOWN, argv[i], (int)strlen(argv[i]), (void **)(convertArgList + i), &convertArgSize) == 0)
		{
			printf("Unable to convert argument [%s] to utf-8.\n", argv[i]);

			break;
		}
	}

	if (i < argc)
	{
		for (i = 0; i < argc; i++)
		{
			va_free(convertArgList[i]);
		}
		va_free(convertArgList);

		return 2;
	}


	if (InitProcess(convertArgList[0]) < 0)
	{
		if (i < argc)
		{
			for (i = 0; i < argc; i++)
			{
				va_free(convertArgList[i]);
			}
			va_free(convertArgList);
		}

		return 2;
	}


	if (GetArgument(argc, convertArgList) < 0)
	{
		if (i < argc)
		{
			for (i = 0; i < argc; i++)
			{
				va_free(convertArgList[i]);
			}
			va_free(convertArgList);
		}

		return 2;
	}

	if (i < argc)
	{
		for (i = 0; i < argc; i++)
		{
			va_free(convertArgList[i]);
		}
		va_free(convertArgList);
	}


	if (SendCommand() < 0)
	{
		return 2;
	}

#ifdef __ABIO_WIN
	//윈속 제거
	WSACleanup();
#endif

	return 0;
}

int InitProcess(char * filename)
{
#ifdef __ABIO_WIN
	WSADATA wsaData;
#elif __ABIO_UNIX
	int i;
#endif

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 1. 전역 변수들을 초기화한다.

	memset(masterIP, 0, sizeof(masterIP));
	memset(masterPort, 0, sizeof(masterPort));
	memset(backupset, 0, sizeof(backupset));
	memset(findFileName, 0, sizeof(findFileName));
	memset(catalogFileName, 0, sizeof(catalogFileName));

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 2. 각 시스템에 맞게 process를 초기화한다.

	// process resource limit를 설정한다.
	va_setrlimit();

	// 현재 작업 디렉토리를 읽어온다.
#ifdef __ABIO_UNIX
	// 절대 경로로 실행된 경우 절대 경로에서 folder를 읽어온다.
	if (filename[0] == FILE_PATH_DELIMITER)
	{
		for (i = (int)strlen(filename) - 1; i > 0; i--)
		{
			if (filename[i] == FILE_PATH_DELIMITER)
			{
				strncpy(ABIOMASTER_TOOL_FOLDER, filename, i);

				break;
			}
		}
	}
	// 상대 경로로 실행된 경우 folder로 이동한뒤 작업 디렉토리를 읽어온다.
	else
	{
		// 상대 경로상의 master server folder를 읽어온다.
		for (i = (int)strlen(filename) - 1; i > 0; i--)
		{
			if (filename[i] == FILE_PATH_DELIMITER)
			{
				strncpy(ABIOMASTER_TOOL_FOLDER, filename, i);

				break;
			}
		}

		// master server folder로 이동한다.
		if (chdir(ABIOMASTER_TOOL_FOLDER) < 0)
		{
			printf("Unable to identify the current working folder.\n");

			return -1;
		}

		// 작업 디렉토리를 읽어온다.
		memset(ABIOMASTER_TOOL_FOLDER, 0, sizeof(ABIOMASTER_TOOL_FOLDER));
		if (va_get_working_directory(ABIOMASTER_TOOL_FOLDER) < 0)
		{
			printf("Unable to identify the current working folder.\n");

			return -1;
		}
	}
#elif __ABIO_WIN
	if (va_get_working_directory(ABIOMASTER_TOOL_FOLDER) < 0)
	{
		printf("Failed to get a woring directory.\n");

		return -1;
	}
#endif


#ifdef __ABIO_WIN
	// windows socket을 open한다.
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) 
	{
		printf("Failed to initialize windows socket.\n");

		return -1;
	}
#endif

	return 0;
}

int GetArgument(int argc, char ** argv)
{
	int validOption;
	int i;
	validOption = 1;

	if (argc == 1)
	{
		PrintUsage(argv[0]);

		return -1;
	}

	for (i = 1; i < argc; i++)
	{
		// master ip
		if (!strncmp(argv[i], "-i", 2))
		{
			if (strlen(argv[i]) == 2)
			{
				strcpy(masterIP, argv[i + 1]);
				i++;
			}
			else
			{
				strcpy(masterIP, argv[i] + 2);
			}
		}
		// master port
		else if (!strncmp(argv[i], "-p", 2))
		{
			if (strlen(argv[i]) == 2)
			{
				strcpy(masterPort, argv[i + 1]);
				i++;
			}
			else
			{
				strcpy(masterPort, argv[i] + 2);
			}
		}

		// backupset id
		else if (!strncmp(argv[i], "-b", 2))
		{
			if (strlen(argv[i]) == 2)
			{
				strcpy(backupset, argv[i + 1]);
				i++;
			}
			else
			{
				strcpy(backupset, argv[i] + 2);
			}
		}

		//find file name 
		else if (!strncmp(argv[i], "-f", 2))
		{
			if(strlen(argv[i]) == 2)
			{
				strcpy(findFileName, argv[i + 1]);
				i++;
			}
			else
			{
				strcpy(findFileName, argv[i] +2);
			}
		}

		// print usage
		else if (!strcmp(argv[i], "--help"))
		{
			PrintUsage(argv[0]);

			return -1;
		}

		// invalid option
		else
		{
			printf("%s: invalid option -- %s\n", argv[0], argv[i]);
			validOption = 0;

			break;
		}
	}

	if (validOption == 1)
	{
		if (masterIP[0] == '\0')
		{
			printf("IP address of the Master Server must be specified.\n");

			validOption = 0;
		}

		if (masterPort[0] == '\0')
		{
			printf("Port number of the Master Server Httpd must be specified.\n");

			validOption = 0;
		}

		if(backupset[0] == '\0')
		{
			printf("Backupset ID must be specified.\n");

			validOption = 0;
		}

		if(findFileName[0] == '\0')
		{
			printf("Find cataloglist name must be specified.\n");

			validOption = 0;
		}
	}

	if (validOption == 1)
	{
		return 0;
	}
	else
	{
		printf("Try `%s --help' for more information.\n", argv[0]);
		PrintUsage(argv[0]);

		return -1;
	}
}

int SendCommand()
{
	char cmd[DSIZ][DSIZ];
	char msg[DSIZ * DSIZ];
	char reply[DSIZ][DSIZ];

	char * forwardFind;
	char * backwardFind;
	int i;
	va_sock_t sock;

	struct find_files *catalogDB = NULL;
	char ***itemList = NULL;

	int findfilesNumber = 0;
	int listNumber = 0;
	int itemNumber = 0;

	// backupset catalog command를 만든다.
	va_init_reply_buf(cmd);

	strcpy(cmd[0], "<FIND_FILES>");

	// set backupset id	[150]
	strcpy(cmd[150], backupset);
	va_make_reply_msg(cmd, msg);


	// backupset log command를 master server로 전송한다.
	if ((sock = va_connect(masterIP, masterPort, 0)) != -1)
	{
		// catalog db command 전송 결과를 받음
		va_send(sock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE);
		memset(msg, 0, sizeof(msg));

		//PrintMenu();

		while (RecvReply(sock, msg, 0) > 0)
		{
			va_parser(msg, reply);
			memset(msg, 0, sizeof(msg));

			if (!strcmp(reply[0], "FIND_FILES_START"))
			{
				if (findfilesNumber % DEFAULT_MEMORY_FRAGMENT == 0)
				{
					catalogDB = (struct find_files *)realloc(catalogDB, sizeof(struct find_files) * (findfilesNumber + DEFAULT_MEMORY_FRAGMENT));
					memset(catalogDB + findfilesNumber, 0, sizeof(struct find_files) * DEFAULT_MEMORY_FRAGMENT);
				}

				strcpy(catalogDB[findfilesNumber].filename, "File_Name");
				strcpy(catalogDB[findfilesNumber].volume, "Volume_Name");
				strcpy(catalogDB[findfilesNumber].filesize, "File_Size");
				strcpy(catalogDB[findfilesNumber].filepath, "File_Path");

				findfilesNumber++;
			}
			else if (!strcmp(reply[0], "FIND_FILES"))
			{
				if (findfilesNumber % DEFAULT_MEMORY_FRAGMENT == 0)
				{
					catalogDB = (struct find_files *)realloc(catalogDB, sizeof(struct find_files) * (findfilesNumber + DEFAULT_MEMORY_FRAGMENT));
					memset(catalogDB + findfilesNumber, 0, sizeof(struct find_files) * DEFAULT_MEMORY_FRAGMENT);
				}

				strcpy(catalogFileName, reply[91]);
				#ifdef __ABIO_UNIX
				backwardFind = strstr(strrchr(catalogFileName, '/'), findFileName);
				forwardFind = strstr(strchr(catalogFileName, '/'), findFileName);
				
				#elif __ABIO_WIN 
				backwardFind = stristr(strrchr(catalogFileName, '/'), findFileName);
				forwardFind = stristr(strchr(catalogFileName, '/'), findFileName);
				#endif
				
				if(forwardFind != NULL)
				{
					strcpy(catalogDB[findfilesNumber].filename, strrchr(catalogFileName, '/'));
					strcpy(catalogDB[findfilesNumber].filepath, strcpy(catalogFileName, reply[91]));
					strcpy(catalogDB[findfilesNumber].volume, reply[40]);
					
					// volume size
					if (va_atoll(reply[96]) == 0)
					{
						sprintf(catalogDB[findfilesNumber].filesize, "0KB");
					}
					else if (va_atoll(reply[96]) / 1024 == 0)
					{
						sprintf(catalogDB[findfilesNumber].filesize, "%.2fKB", va_atoll(reply[96]) / 1024.0);
					}
					else if (va_atoll(reply[96]) / 1024 / 1024 == 0)
					{
						sprintf(catalogDB[findfilesNumber].filesize, "%.2fKB", va_atoll(reply[96]) / 1024.0);
					}
					else if (va_atoll(reply[96]) / 1024 / 1024 / 1024 == 0)
					{
						sprintf(catalogDB[findfilesNumber].filesize, "%.2fMB", va_atoll(reply[96]) / 1024.0 / 1024.0);
					}
					else
					{
						sprintf(catalogDB[findfilesNumber].filesize, "%.2fGB", va_atoll(reply[96]) / 1024.0 / 1024.0 / 1024.0);
					}
					
					findfilesNumber++;
				}
				else if(backwardFind != NULL)
				{
					strcpy(catalogDB[findfilesNumber].filename, strchr(catalogFileName, '/'));
					strcpy(catalogDB[findfilesNumber].filepath, strcpy(catalogFileName, reply[91]));
					strcpy(catalogDB[findfilesNumber].volume, reply[40]);
				
					// volume size
					if (va_atoll(reply[96]) == 0)
					{
						sprintf(catalogDB[findfilesNumber].filesize, "0KB");
					}
					else if (va_atoll(reply[96]) / 1024 == 0)
					{
						sprintf(catalogDB[findfilesNumber].filesize, "%.2fKB", va_atoll(reply[96]) / 1024.0);
					}
					else if (va_atoll(reply[96]) / 1024 / 1024 == 0)
					{
						sprintf(catalogDB[findfilesNumber].filesize, "%.2fKB", va_atoll(reply[96]) / 1024.0);
					}
					else if (va_atoll(reply[96]) / 1024 / 1024 / 1024 == 0)
					{
						sprintf(catalogDB[findfilesNumber].filesize, "%.2fMB", va_atoll(reply[96]) / 1024.0 / 1024.0);
					}
					else
					{
						sprintf(catalogDB[findfilesNumber].filesize, "%.2fGB", va_atoll(reply[96]) / 1024.0 / 1024.0 / 1024.0);
					}
					
					findfilesNumber++;
				}
			}
			else if(!strcmp(reply[0], "FIND_FILES_END"))
			{
				listNumber = findfilesNumber;				
				itemNumber = 4;
				itemList = (char ***)malloc(sizeof(char **) * listNumber);

				for (i = 0; i < listNumber; i++)
				{
					itemList[i] = (char **)malloc(sizeof(char *) * itemNumber);

					itemList[i][0] = (char *)malloc(sizeof(char) * (strlen(catalogDB[i].filename) + 1));
					memset(itemList[i][0], 0, sizeof(char) * (strlen(catalogDB[i].filename) + 1));
					strcpy(itemList[i][0], catalogDB[i].filename);

					itemList[i][1] = (char *)malloc(sizeof(char) * (strlen(catalogDB[i].filepath) + 1));
					memset(itemList[i][1], 0, sizeof(char) * (strlen(catalogDB[i].filepath) + 1));
					strcpy(itemList[i][1], catalogDB[i].filepath);

					itemList[i][2] = (char *)malloc(sizeof(char) * (strlen(catalogDB[i].volume) + 1));
					memset(itemList[i][2], 0, sizeof(char) * (strlen(catalogDB[i].volume) + 1));
					strcpy(itemList[i][2], catalogDB[i].volume);

					itemList[i][3] = (char *)malloc(sizeof(char) * (strlen(catalogDB[i].filesize) + 1));
					memset(itemList[i][3], 0, sizeof(char) * (strlen(catalogDB[i].filesize) + 1));
					strcpy(itemList[i][3], catalogDB[i].filesize);
				}
				va_print_item_on_screen(NULL, 1, itemList, listNumber, itemNumber);

				for (i = 0; i < listNumber; i++)
				{
					va_free(itemList[i]);
				}
				va_free(itemList);

				break;
			}
			else
			{
				// 잘못된메세지가온경우종료한다.
				printf("Received a wrong message.\n");
				break;
			}
		}
		va_close_socket(sock, ABIO_SOCKET_CLOSE_SERVER);

		printf("\n");
	}
	else
	{
		printf("\n");
		printf("Failed to send the catalog request to master server.\n");
	}
	return 0;
}

void PrintUsage(char * cmd)
{
	printf("cataloglist command %s\n", CURRENT_VERSION_DISPLAY_TYPE);
	printf("Usage : %s -i<ip> -p<port> -b<backupsetid> -f<file name>\n", cmd);
	printf("  -i<ip>               IP address of the Master Server.\n");
	printf("  -p<port>             Port number of the Master Server Httpd.\n");
	printf("  -b<backupsetid>      It will get catalog db from specified backupset id.\n");
	printf("  -f<find filename>    It will find catalog file name from catalog DB.\n");
}