#include "require.h"
#include "abio.h"
#include "lib.h"
#include "vendor.h"
#include "get_backupset_list.h"
// start of variables for abio library comparability
char ABIOMASTER_LOG_FOLDER[MAX_PATH_LENGTH];
// end of variables for abio library comparability


char ABIOMASTER_TOOL_FOLDER[MAX_PATH_LENGTH];


//////////////////////////////////////////////////////////////////////////////////////////////////
// master server ip and port
char masterIP[IP_LENGTH];
char masterPort[PORT_LENGTH];
char jobName[JOB_ID_LENGTH];
char volumeName[VOLUME_NAME_LENGTH];
char scheduleName[SCHEDULE_NAME_LENGTH];
char clientName[NODE_NAME_LENGTH];
char status[JOB_STATUS_LENGTH];



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
		
		Exit();
		
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
		
		Exit();
		
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
		
		Exit();
		
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
		Exit();
		
		return 2;
	}
	
	
	Exit();

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
	
	memset(ABIOMASTER_TOOL_FOLDER, 0, sizeof(ABIOMASTER_TOOL_FOLDER));
	
	memset(masterIP, 0, sizeof(masterIP));
	memset(masterPort, 0, sizeof(masterPort));
	memset(volumeName, 0, sizeof(volumeName));
	memset(jobName, 0, sizeof(jobName));
	memset(scheduleName, 0 ,sizeof(scheduleName));
	memset(clientName, 0, sizeof(clientName));
	memset(status, 0, sizeof(status));

	
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

int SendCommand()
{
	char cmd[DSIZ][DSIZ];
	char msg[DSIZ * DSIZ];
	char reply[DSIZ][DSIZ];

	va_sock_t sock;
	
	int i;
	int j;

	struct backupsetitem * backupsetList;
	int backupsetNumber;


	char *** itemList;
	int listNumber;
	int itemNumber;

	// initialize variables
	backupsetList = NULL;
	backupsetNumber = 0;


	itemList = NULL;
	listNumber = 0;
	itemNumber = 0;

		// command를 만든다.
	va_init_reply_buf(cmd);
	
	strcpy(cmd[0], "<GET_BACKUPSET_LOG>");
	strcpy(cmd[156], "0");
	if (strlen(volumeName) != 0)
	{
		strcpy(cmd[40], volumeName);
	}	
	if (strlen(jobName) != 0)
	{
		strcpy(cmd[70], jobName);
	}
	if (strlen(scheduleName) != 0)
	{
		strcpy(cmd[80], scheduleName);
	}
	if (strlen(clientName) != 0)
	{
		strcpy(cmd[110], clientName);
	}
	if (strlen(status) != 0)
	{
		if(!strcmp(status, "0"))
		{
			strcpy(status, "2");
		}
		strcpy(cmd[156], status);
	}

	va_make_reply_msg(cmd, msg);

	// command를 master server로 전송한다.
	if ((sock = va_connect(masterIP, masterPort, 1)) != -1)
	{
		va_send(sock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE);
			
		// command 전송 결과를 받음
		while (RecvReply(sock, msg, sizeof(msg), 0) > 0)
		{
			va_parser(msg, reply);
			memset(msg, 0, sizeof(msg));

			if (!strcmp(reply[0], "BACKUPSET_LOG_START"))
			{
				if (backupsetNumber % DEFAULT_MEMORY_FRAGMENT == 0)
				{
					backupsetList = (struct backupsetitem *)realloc(backupsetList, sizeof(struct backupsetitem) * (backupsetNumber + DEFAULT_MEMORY_FRAGMENT));
					memset(backupsetList + backupsetNumber, 0, sizeof(struct backupsetitem) * DEFAULT_MEMORY_FRAGMENT);
				}
				strcpy(backupsetList[backupsetNumber].backupset, "Backup Set ID");		
				strcpy(backupsetList[backupsetNumber].backupsetCopyNumber ,"Copies");
				strcpy(backupsetList[backupsetNumber].client ,"Client");
				strcpy(backupsetList[backupsetNumber].policy ,"Job");
				strcpy(backupsetList[backupsetNumber].schedule ,"Schedule");
				strcpy(backupsetList[backupsetNumber].btime ,"Creation Date");
				strcpy(backupsetList[backupsetNumber].fileNumber ,"Files");
				strcpy(backupsetList[backupsetNumber].fileSize ,"Data Size");
				strcpy(backupsetList[backupsetNumber].volume ,"Volume");
				strcpy(backupsetList[backupsetNumber].retention ,"Retention Period");
				strcpy(backupsetList[backupsetNumber].etime ,"Expiration Date");
				strcpy(backupsetList[backupsetNumber].backupsetStatus ,"Status");
				strcpy(backupsetList[backupsetNumber].backupData ,"Backup Data");
				strcpy(backupsetList[backupsetNumber].backupMethod ,"Backup Method");
				backupsetNumber++;
			}
			else if (!strcmp(reply[0], "BACKUPSET_LOG"))
			{
				if (backupsetNumber % DEFAULT_MEMORY_FRAGMENT == 0)
				{
					backupsetList = (struct backupsetitem *)realloc(backupsetList, sizeof(struct backupsetitem) * (backupsetNumber + DEFAULT_MEMORY_FRAGMENT));
					memset(backupsetList + backupsetNumber, 0, sizeof(struct backupsetitem) * DEFAULT_MEMORY_FRAGMENT);
				}

				// list의 내용을 확인한다.
				for (i = 0; i < 1; i++)
				{
					strcpy(backupsetList[backupsetNumber].backupset, reply[150]);	
					strcpy(backupsetList[backupsetNumber].backupsetCopyNumber ,reply[151]);
					strcpy(backupsetList[backupsetNumber].client ,reply[110]);
					strcpy(backupsetList[backupsetNumber].policy ,reply[70]);
					strcpy(backupsetList[backupsetNumber].schedule ,reply[80]);					
					strcpy(backupsetList[backupsetNumber].btime ,reply[153]);
					strcpy(backupsetList[backupsetNumber].fileNumber ,reply[154]);
					strcpy(backupsetList[backupsetNumber].fileSize ,reply[155]);
					strcpy(backupsetList[backupsetNumber].volume ,reply[40]);
					strcpy(backupsetList[backupsetNumber].retention ,reply[158]);
					strcpy(backupsetList[backupsetNumber].etime ,reply[157]);
					strcpy(backupsetList[backupsetNumber].backupsetStatus , reply[156]);
					strcpy(backupsetList[backupsetNumber].backupData ,GetBackupData((int)va_atoll(reply[152])));
					strcpy(backupsetList[backupsetNumber].backupMethod ,GetBackupMethod((int)va_atoll(reply[167])));

				}
				backupsetNumber++;
			}
			else if (!strcmp(reply[0], "BACKUPSET_LOG_END"))
			{
				listNumber = backupsetNumber;
				itemNumber = 14;
				
				itemList = (char ***)malloc(sizeof(char **) * listNumber);

				for (i = 0; i < listNumber; i++)
				{
					itemList[i] = (char **)malloc(sizeof(char *) * itemNumber);
					
					itemList[i][0] = (char *)malloc(sizeof(char) * (strlen(backupsetList[i].backupset) + 1));
					memset(itemList[i][0], 0, sizeof(char) * (strlen(backupsetList[i].backupset) + 1));
					strcpy(itemList[i][0], backupsetList[i].backupset);

					itemList[i][1] = (char *)malloc(sizeof(char) * (strlen(backupsetList[i].backupsetCopyNumber) + 1));
					memset(itemList[i][1], 0, sizeof(char) * (strlen(backupsetList[i].backupsetCopyNumber) + 1));
					strcpy(itemList[i][1], backupsetList[i].backupsetCopyNumber);

					itemList[i][2] = (char *)malloc(sizeof(char) * (strlen(backupsetList[i].client) + 1));
					memset(itemList[i][2], 0, sizeof(char) * (strlen(backupsetList[i].client) + 1));
					strcpy(itemList[i][2], backupsetList[i].client);

					itemList[i][3] = (char *)malloc(sizeof(char) * (strlen(backupsetList[i].policy) + 1));
					memset(itemList[i][3], 0, sizeof(char) * (strlen(backupsetList[i].policy) + 1));
					strcpy(itemList[i][3], backupsetList[i].policy);

					itemList[i][4] = (char *)malloc(sizeof(char) * (strlen(backupsetList[i].schedule) + 1));
					memset(itemList[i][4], 0, sizeof(char) * (strlen(backupsetList[i].schedule) + 1));
					strcpy(itemList[i][4], backupsetList[i].schedule);

					itemList[i][5] = (char *)malloc(sizeof(char) * (strlen(backupsetList[i].btime) + 1));
					memset(itemList[i][5], 0, sizeof(char) * (strlen(backupsetList[i].btime) + 1));
					strcpy(itemList[i][5], backupsetList[i].btime);

					itemList[i][6] = (char *)malloc(sizeof(char) * (strlen(backupsetList[i].fileNumber) + 1));
					memset(itemList[i][6], 0, sizeof(char) * (strlen(backupsetList[i].fileNumber) + 1));
					strcpy(itemList[i][6], backupsetList[i].fileNumber);

					itemList[i][7] = (char *)malloc(sizeof(char) * (strlen(backupsetList[i].fileSize) + 1));
					memset(itemList[i][7], 0, sizeof(char) * (strlen(backupsetList[i].fileSize) + 1));
					strcpy(itemList[i][7], backupsetList[i].fileSize);

					itemList[i][8] = (char *)malloc(sizeof(char) * (strlen(backupsetList[i].volume) + 1));
					memset(itemList[i][8], 0, sizeof(char) * (strlen(backupsetList[i].volume) + 1));
					strcpy(itemList[i][8], backupsetList[i].volume);

					itemList[i][9] = (char *)malloc(sizeof(char) * (strlen(backupsetList[i].retention) + 1));
					memset(itemList[i][9], 0, sizeof(char) * (strlen(backupsetList[i].retention) + 1));
					strcpy(itemList[i][9], backupsetList[i].retention);

					itemList[i][10] = (char *)malloc(sizeof(char) * (strlen(backupsetList[i].etime) + 1));
					memset(itemList[i][10], 0, sizeof(char) * (strlen(backupsetList[i].etime) + 1));
					strcpy(itemList[i][10], backupsetList[i].etime);

					itemList[i][11] = (char *)malloc(sizeof(char) * (strlen(backupsetList[i].backupsetStatus) + 1));
					memset(itemList[i][11], 0, sizeof(char) * (strlen(backupsetList[i].backupsetStatus) + 1));
					strcpy(itemList[i][11], backupsetList[i].backupsetStatus);
					
					itemList[i][12] = (char *)malloc(sizeof(char) * (strlen(backupsetList[i].backupData) + 1));
					memset(itemList[i][12], 0, sizeof(char) * (strlen(backupsetList[i].backupData) + 1));
					strcpy(itemList[i][12], backupsetList[i].backupData);

					itemList[i][13] = (char *)malloc(sizeof(char) * (strlen(backupsetList[i].backupMethod) + 1));
					memset(itemList[i][13], 0, sizeof(char) * (strlen(backupsetList[i].backupMethod) + 1));
					strcpy(itemList[i][13], backupsetList[i].backupMethod);
					for(j = 0; j < 14; j++)
					{
						if(!strcmp(itemList[i][j], ""))
							strcpy(itemList[i][j],"-");
					}
				}
				
				va_print_item_on_screen(NULL, 14, itemList, listNumber, itemNumber);
				break;

			}

			else
			{
				// 잘못된 메세지가 온 경우 종료한다.
				printf("Received a wrong message.\n");
				
				break;
			}
		}
		va_close_socket(sock, ABIO_SOCKET_CLOSE_SERVER);
	}
	else
	{
		printf("Failed to send the request to master server.\n");
		
		return -1;
	}
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
		else if (!strncmp(argv[i], "-j", 2))
		{		
			if (strlen(argv[i]) == 2)
			{
				strcpy(jobName, argv[i + 1]);
				i++;
			}
			else
			{
				strcpy(jobName, argv[i] + 2);
			}
		}
		else if (!strncmp(argv[i], "-s", 2))
		{		
			if (strlen(argv[i]) == 2)
			{
				strcpy(scheduleName, argv[i + 1]);
				i++;
			}
			else
			{
				strcpy(scheduleName, argv[i] + 2);
			}
		}
		else if (!strncmp(argv[i], "-c", 2))
		{		
			if (strlen(argv[i]) == 2)
			{
				strcpy(clientName, argv[i + 1]);
				i++;
			}
			else
			{
				strcpy(clientName, argv[i] + 2);
			}
		}
		else if (!strncmp(argv[i], "-v", 2))
		{		
			if (strlen(argv[i]) == 2)
			{
				strcpy(volumeName, argv[i + 1]);
				i++;
			}
			else
			{
				strcpy(volumeName, argv[i] + 2);
			}
		}
		else if (!strncmp(argv[i], "-t", 2))
		{		
			if (strlen(argv[i]) == 2)
			{
				strcpy(status, argv[i + 1]);
				i++;
			}
			else
			{
				strcpy(status, argv[i] + 2);
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
		if (strlen(masterIP) == 0)
		{
			printf("IP address of the Master Server must be specified.\n");
			
			validOption = 0;
		}
		
		if (strlen(masterPort) == 0)
		{
			printf("Port number of the Master Server Httpd.\n");
			
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
		
		return -1;
	}
}
void PrintUsage(char * cmd)
{
	printf("get backupset list command %s\n", CURRENT_VERSION_DISPLAY_TYPE);
	printf("Usage : %s -i<ip> -p<port> -j<job> -s<schedule> -c<client> -v<volume> -t<status> \n", cmd);
	printf("  -i<ip>               IP address of the Master Server.\n");
	printf("  -p<port>             Port number of the Master Server Httpd.\n");
	printf("  -j<job>			   name of client job.\n");
	printf("  -s<schedule>		   name of client schedule.\n");
	printf("  -c<client>		   name of client server.\n");
	printf("  -v<volume>		   name of Volume.\n");
	printf("  -t<status>		   name of Status.\n");	
}

int RecvReply(va_sock_t sock, char * data, int size, int flag)
{
	int rcvSize;
	int totalRcvSize;
	rcvSize = 0;
	totalRcvSize = 0;

	while ((size - totalRcvSize) > 0)
	{
		if ((rcvSize = va_recv(sock, data + totalRcvSize, 1, flag, DATA_TYPE_NOT_CHANGE)) < 0)
		{
			return rcvSize;
		}

		if (data[totalRcvSize] == '\0')
		{
			totalRcvSize += rcvSize;
			break;
		}
		totalRcvSize += rcvSize;
	}
	return totalRcvSize;
}

void Exit()
{
	#ifdef __ABIO_WIN
		//윈속 제거
		WSACleanup();
	#endif
}

