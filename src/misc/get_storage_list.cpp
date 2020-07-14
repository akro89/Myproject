#include "require.h"
#include "abio.h"
#include "lib.h"
#include "vendor.h"
#include "get_storage_list.h"


// start of variables for abio library comparability
char ABIOMASTER_LOG_FOLDER[MAX_PATH_LENGTH];
// end of variables for abio library comparability
char ABIOMASTER_TOOL_FOLDER[MAX_PATH_LENGTH];

//////////////////////////////////////////////////////////////////////////////////////////////////
// master server ip and port
char masterIP[IP_LENGTH];
char masterPort[PORT_LENGTH];
char viewOption[OPTIONSIZE];
int  selectList[BUFSIZE];
char delimiterBuffer[DELIMITER];
int	 selectNumber;

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
	//윈속제거
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
	// 1. 전역변수들을초기화한다.
	memset(ABIOMASTER_TOOL_FOLDER, 0, sizeof(ABIOMASTER_TOOL_FOLDER));
	memset(masterIP, 0, sizeof(masterIP));
	memset(masterPort, 0, sizeof(masterPort));
	memset(viewOption, 0, sizeof(viewOption));
	memset(&selectList,0,sizeof(selectList));
	memset(delimiterBuffer,0,sizeof(delimiterBuffer));
	selectNumber=0;


	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 2. 각시스템에맞게process를초기화한다.

	// process resource limit를설정한다.
	va_setrlimit();

	// 현재작업디렉토리를읽어온다.
#ifdef __ABIO_UNIX
	// 절대경로로실행된경우절대경로에서folder를읽어온다.
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
	// 상대경로로실행된경우folder로이동한뒤작업디렉토리를읽어온다.
	else
	{
		// 상대경로상의master server folder를읽어온다.
		for (i = (int)strlen(filename) - 1; i > 0; i--)
		{
			if (filename[i] == FILE_PATH_DELIMITER)
			{
				strncpy(ABIOMASTER_TOOL_FOLDER, filename, i);

				break;
			}
		}

		// master server folder로이동한다.
		if (chdir(ABIOMASTER_TOOL_FOLDER) < 0)
		{
			printf("Unable to identify the current working folder.\n");

			return -1;
		}

		// 작업디렉토리를읽어온다.
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
		printf("Failed to get a working directory.\n");

		return -1;
	}
#endif

#ifdef __ABIO_WIN
	// windows socket을open한다.
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
	int i;
	int validOption =1;
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
			if(argc==i+1)
			{
				PrintUsage(argv[0]);
				return -1;
			}
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
			if(argc==i+1)
			{
				PrintUsage(argv[0]);
				return -1;
			}
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
		// View option
		else if (!strncmp(argv[i], GET_STORAGE_SELECT_ALL_OPTION, 2))
		{
			if(setArg(argc,argv,&i,ALL_OPTION_NUMBER) == -1)
				return -1;

		}
		else if (!strncmp(argv[i], GET_STORAGE_SELECT_TAPE_OPTION, 2))
		{
			if(setArg(argc,argv,&i,TAPE_OPTION_NUMBER) == -1)
				return -1;
		}
		else if (!strncmp(argv[i], GET_STORAGE_SELECT_DISK_OPTION, 2))
		{
			if(setArg(argc,argv,&i,DISK_OPTION_NUMBER) == -1)
				return -1;
		}
		else if (!strcmp(argv[i], "--help"))
		{
			PrintUsage(argv[0]);
			return -1;
		}
		//WriteDebugData("C:\\", "test.txt", "%d, %s ", argc, argv[i]);
	}
	if (validOption == 1)
	{
		if (strlen(masterIP) == 0)
		{
			printf("***IP address of the Master Server must be specified.***\n");
			printf("Try `%s --help' for more information.\n", argv[0]);
			validOption = 0;
			return -1;
		}

		if (strlen(masterPort) == 0)
		{
			printf("***Port number of the Master Server Httpd.***\n");
			printf("Try `%s --help' for more information.\n", argv[0]);
			validOption = 0;
			return -1;
		}

		if (strlen(viewOption) == 0)
		{
			printf("***Get Storage list Type must be specified.***\n");
			printf("Try `%s --help' for more information.\n", argv[0]);
			validOption = 0;
			return -1;
		}

		return 0;
	}
	else
	{
		printf("Try `%s --help' for more information.\n", argv[0]);

		return -1;
	}

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

	//구조체사용및변수초기화	
	struct storage_list * storage = NULL;
	char *** itemList = NULL;

	int storageNumber = 0;
	int listNumber = 0;
	int itemNumber = 0;

	//master server로보낼command를만든다.
	va_init_reply_buf(cmd);

	strcpy(cmd[0], "<GET_STORAGE>");
	strcpy(cmd[210], viewOption);

	va_make_reply_msg(cmd, msg);

	// command를master server로전송한다.
	if ((sock = va_connect(masterIP, masterPort, 1)) != -1)
	{
		//master server로command 보냄
		va_send(sock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE);
		memset(msg, 0, sizeof(msg));

		//master server로부터응답받음
		while (RecvReply(sock, msg, sizeof(msg), 0) > 0)
		{
			va_parser(msg, reply);
			memset(msg, 0, sizeof(msg));

			if (!strcmp(reply[0], "STORAGE_START"))
			{
				if (storageNumber % DEFAULT_MEMORY_FRAGMENT == 0)
				{
					storage = (struct storage_list *)realloc(storage, sizeof(struct storage_list) * (storageNumber + DEFAULT_MEMORY_FRAGMENT));
					memset(storage + storageNumber, 0, sizeof(struct storage_list) * DEFAULT_MEMORY_FRAGMENT);
				}
				strcpy(storage[storageNumber].name, "Storage_Name");
				strcpy(storage[storageNumber].type, "Storage_Type");
				strcpy(storage[storageNumber].status, "Storage_Status");
				strcpy(storage[storageNumber].nodename, "Node_Name");
				strcpy(storage[storageNumber].ownerstatus, "Owner_Status");
				strcpy(storage[storageNumber].librarytype, "Library_Type");
				strcpy(storage[storageNumber].device, "Device");
				strcpy(storage[storageNumber].address, "Address");
				strcpy(storage[storageNumber].drivetype, "Drive_Type");
				strcpy(storage[storageNumber].repository, "Repository");
				strcpy(storage[storageNumber].volumeSize, "Volume_Size");
				strcpy(storage[storageNumber].capacity, "Capacity");

				storageNumber++;
			}
			else if (!strcmp(reply[0], "STORAGE"))
			{
				if (storageNumber % DEFAULT_MEMORY_FRAGMENT == 0)
				{
					storage = (struct storage_list *)realloc(storage, sizeof(struct storage_list) * (storageNumber + DEFAULT_MEMORY_FRAGMENT));
					memset(storage + storageNumber, 0, sizeof(struct storage_list) * DEFAULT_MEMORY_FRAGMENT);
				}
				// storage name
				strcpy(storage[storageNumber].name, reply[210]);

				// storage status
				strcpy(storage[storageNumber].status, getStorageStatus(atoi(reply[212])));

				//storage nodename 
				strcpy(storage[storageNumber].nodename, reply[213]);

				// storage ownerstatus
				strcpy(storage[storageNumber].ownerstatus, getStorageOwnerStatus(atoi(reply[216])));

				// storage type
				if (atoi(reply[211]) == STORAGE_UNKNOWN)
				{
					strcpy(storage[storageNumber].type, "STORAGE_UNKNOWN");
				}
				else if (atoi(reply[211]) == STORAGE_TAPE_LIBRARY)
				{
					strcpy(storage[storageNumber].type, "STORAGE_TAPE");
					//tape library Type
					strcpy(storage[storageNumber].librarytype, getLibraryType(atoi(reply[240])));

					//tape address
					strcpy(storage[storageNumber].address, reply[250]);

					//tape tapeDrive Type
					strcpy(storage[storageNumber].drivetype, getTapeDriveType(atoi(reply[252])));

					//tape device
					strcpy(storage[storageNumber].device, reply[214]);

					if (reply[215][0] == '\0' || reply[215] == "")
					{
						strcpy(storage[storageNumber].repository, "-");
					}
					if (reply[231][0] == '\0' || reply[231] == "")
					{
						strcpy(storage[storageNumber].volumeSize, "-");
					}
					if (reply[230][0] == '\0' || reply[230] == "")
					{
						strcpy(storage[storageNumber].capacity, "-");
					}
				}
				//standalone
				else if (atoi(reply[211]) == STORAGE_STANDALONE_TAPE_DRIVE)
				{
					strcpy(storage[storageNumber].type, "STORAGE_STANDALONE");
					//standalone library
					strcpy(storage[storageNumber].librarytype, getLibraryType(atoi(reply[240])));

					//standalone tapeDrive Type
					strcpy(storage[storageNumber].drivetype, getTapeDriveType(atoi(reply[252])));

					//standalone device
					strcpy(storage[storageNumber].device, reply[214]);

					if (reply[215][0] == '\0' || reply[215] == "")
					{
						strcpy(storage[storageNumber].repository, "-");
					}
					if (reply[231][0] == '\0' || reply[231] == "")
					{
						strcpy(storage[storageNumber].volumeSize, "-");
					}
					if (reply[230][0] == '\0' || reply[230] == "")
					{
						strcpy(storage[storageNumber].capacity, "-");
					}
					if (reply[250][0] == '\0' || reply[250] == "")
					{
						strcpy(storage[storageNumber].address, "-");
					}
				}

				//disk
				else if (atoi(reply[211]) == STORAGE_DISK)
				{
					strcpy(storage[storageNumber].type, "STORAGE_DISK");

					//repository 
					strcpy(storage[storageNumber].repository, reply[215]);

					//volumeSize
					sprintf(storage[storageNumber].volumeSize, "%sGB", reply[231]);

					//capacity
					sprintf(storage[storageNumber].capacity,"%sGB", reply[230]);

					if (reply[240][0] == '\0' || reply[240] == "")
					{
						strcpy(storage[storageNumber].librarytype, "-");
					}
					if (reply[250][0] == '\0' || reply[250] == "")
					{
						strcpy(storage[storageNumber].address, "-");
					}
					if (reply[252][0] == '\0' || reply[252] == "")
					{
						strcpy(storage[storageNumber].drivetype, "-");
					}
					if (reply[214][0] == '\0' || reply[214] == "")
					{
						strcpy(storage[storageNumber].device, "-");
					}
				}
				storageNumber++;
			}

			else if (!strcmp(reply[0], "STORAGE_END"))
			{
				listNumber = storageNumber;				
				itemNumber = selectNumber+1;
				itemList = (char ***)malloc(sizeof(char **) * listNumber);

				for (i = 0; i < listNumber; i++)
				{
					itemList[i] = (char **)malloc(sizeof(char *) * itemNumber);
					itemList[i][0] = (char *)malloc(sizeof(char) * (strlen(storage[i].name) + 1));
					memset(itemList[i][0], 0, sizeof(char) * (strlen(storage[i].name) + 1));
					strcpy(itemList[i][0], storage[i].name);

					for( j=0;j<selectNumber;j++)
					{
						if(((enum storageSelect)type)==selectList[j])
						{
							itemList[i][j+1] = (char *)malloc(sizeof(char) * (strlen(storage[i].type) + 1));
							memset(itemList[i][j+1], 0, sizeof(char) * (strlen(storage[i].type) + 1));
							strcpy(itemList[i][j+1], storage[i].type);
						}
						else if(((enum storageSelect)status)==selectList[j])
						{
							itemList[i][j+1] = (char *)malloc(sizeof(char) * (strlen(storage[i].status) + 1));
							memset(itemList[i][j+1], 0, sizeof(char) * (strlen(storage[i].status) + 1));
							strcpy(itemList[i][j+1], storage[i].status);
						}
						else if(((enum storageSelect)ownerstatus)==selectList[j])
						{
							itemList[i][j+1] = (char *)malloc(sizeof(char) * (strlen(storage[i].ownerstatus) + 1));
							memset(itemList[i][j+1], 0, sizeof(char) * (strlen(storage[i].ownerstatus) + 1));
							strcpy(itemList[i][j+1], storage[i].ownerstatus);
						}
						else if(((enum storageSelect)nodename)==selectList[j])
						{
							itemList[i][j+1] = (char *)malloc(sizeof(char) * (strlen(storage[i].nodename) + 1));
							memset(itemList[i][j+1], 0, sizeof(char) * (strlen(storage[i].nodename) + 1));
							strcpy(itemList[i][j+1], storage[i].nodename);
						}
						else if(((enum storageSelect)librarytype)==selectList[j])
						{	
							itemList[i][j+1] = (char *)malloc(sizeof(char) * (strlen(storage[i].librarytype) + 1));
							memset(itemList[i][j+1], 0, sizeof(char) * (strlen(storage[i].librarytype) + 1));
							strcpy(itemList[i][j+1], storage[i].librarytype);
						}
						else if(((enum storageSelect)device)==selectList[j])
						{	
							itemList[i][j+1] = (char *)malloc(sizeof(char) * (strlen(storage[i].device) + 1));
							memset(itemList[i][j+1], 0, sizeof(char) * (strlen(storage[i].device) + 1));
							strcpy(itemList[i][j+1], storage[i].device);
						}
						else if(((enum storageSelect)address)==selectList[j])
						{	
							itemList[i][j+1] = (char *)malloc(sizeof(char) * (strlen(storage[i].address) + 1));
							memset(itemList[i][j+1], 0, sizeof(char) * (strlen(storage[i].address) + 1));
							strcpy(itemList[i][j+1], storage[i].address);
						}
						else if(((enum storageSelect)drivetype)==selectList[j])
						{	
							itemList[i][j+1] = (char *)malloc(sizeof(char) * (strlen(storage[i].drivetype) + 1));
							memset(itemList[i][j+1], 0, sizeof(char) * (strlen(storage[i].drivetype) + 1));
							strcpy(itemList[i][j+1], storage[i].drivetype);
						}
						if(!strcmp(viewOption, GET_STORAGE_SELECT_ALL_OPTION))
						{
							if(((enum storageSelect)capacity)==selectList[j])
							{
								itemList[i][j+1] = (char *)malloc(sizeof(char) * (strlen(storage[i].capacity) + 1));
								memset(itemList[i][j+1], 0, sizeof(char) * (strlen(storage[i].capacity) + 1));
								strcpy(itemList[i][j+1], storage[i].capacity);
							}
							else if(((enum storageSelect)volumeSize)==selectList[j])
							{
								itemList[i][j+1] = (char *)malloc(sizeof(char) * (strlen(storage[i].volumeSize) + 1));
								memset(itemList[i][j+1], 0, sizeof(char) * (strlen(storage[i].volumeSize) + 1));
								strcpy(itemList[i][j+1], storage[i].volumeSize);
							}
							else if(((enum storageSelect)repository)==selectList[j])
							{
								itemList[i][j+1] = (char *)malloc(sizeof(char) * (strlen(storage[i].repository) + 1));
								memset(itemList[i][j+1], 0, sizeof(char) * (strlen(storage[i].repository) + 1));
								strcpy(itemList[i][j+1], storage[i].repository);
							}
						}
						else if(!strcmp(viewOption, GET_STORAGE_SELECT_DISK_OPTION))
						{
							if(((enum storageSelect)type)==selectList[j])
							{
								itemList[i][j+1] = (char *)malloc(sizeof(char) * (strlen(storage[i].type) + 1));
								memset(itemList[i][j+1], 0, sizeof(char) * (strlen(storage[i].type) + 1));
								strcpy(itemList[i][j+1], storage[i].type);
							}
							else if(((enum storageSelect)status-DISK_LEFT_SHIFT_1)==selectList[j])
							{
								itemList[i][j+1] = (char *)malloc(sizeof(char) * (strlen(storage[i].status) + 1));
								memset(itemList[i][j+1], 0, sizeof(char) * (strlen(storage[i].status) + 1));
								strcpy(itemList[i][j+1], storage[i].status);
							}
							else if(((enum storageSelect)ownerstatus-DISK_LEFT_SHIFT_1)==selectList[j])
							{
								itemList[i][j+1] = (char *)malloc(sizeof(char) * (strlen(storage[i].ownerstatus) + 1));
								memset(itemList[i][j+1], 0, sizeof(char) * (strlen(storage[i].ownerstatus) + 1));
								strcpy(itemList[i][j+1], storage[i].ownerstatus);
							}
							else if(((enum storageSelect)nodename-DISK_LEFT_SHIFT_1)==selectList[j])
							{
								itemList[i][j+1] = (char *)malloc(sizeof(char) * (strlen(storage[i].nodename) + 1));
								memset(itemList[i][j+1], 0, sizeof(char) * (strlen(storage[i].nodename) + 1));
								strcpy(itemList[i][j+1], storage[i].nodename);
							}
							else if(((enum storageSelect)capacity-DISK_LEFT_SHIFT_4)==selectList[j])
							{
								itemList[i][j+1] = (char *)malloc(sizeof(char) * (strlen(storage[i].capacity) + 1));
								memset(itemList[i][j+1], 0, sizeof(char) * (strlen(storage[i].capacity) + 1));
								strcpy(itemList[i][j+1], storage[i].capacity);
							}
							else if(((enum storageSelect)volumeSize-DISK_LEFT_SHIFT_4)==selectList[j])
							{
								itemList[i][j+1] = (char *)malloc(sizeof(char) * (strlen(storage[i].volumeSize) + 1));
								memset(itemList[i][j+1], 0, sizeof(char) * (strlen(storage[i].volumeSize) + 1));
								strcpy(itemList[i][j+1], storage[i].volumeSize);
							}
							else if(((enum storageSelect)repository-DISK_LEFT_SHIFT_4)==selectList[j])
							{
								itemList[i][j+1] = (char *)malloc(sizeof(char) * (strlen(storage[i].repository) + 1));
								memset(itemList[i][j+1], 0, sizeof(char) * (strlen(storage[i].repository) + 1));
								strcpy(itemList[i][j+1], storage[i].repository);
							}
						}
					}
				}
				va_print_item_on_screen(NULL, 1, itemList, listNumber, itemNumber);
				for (i = 0; i < listNumber; i++)
				{
					for (j = 0; j < selectNumber; j++)
					{
						va_free(itemList[i][j]);
					}
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
	}
	else
	{
		printf("Failed to send the request to master server.\n");
		return -1;
	}

	return 0;
}

int setArg(int argc, char **argv, int *countArgument, int option)
{
	int i = *countArgument;
	int j;

	if (strlen(argv[i]) == 2)
	{   
		strcpy(viewOption, argv[i]);
		i++;

		if(argc==i || 0 == atoi(argv[i]))
		{
			for( j=1;j<option;j++)
			{
				selectList[selectNumber]=j;
				selectNumber++;
			}
			i++;
		}
		else
		{	
			if(countOptionValue(argc,argv,&i,option) == -1)
				return -1;
		}
	}
	else //옵션과 옵션값이 붙어있는 경우
	{
		char *strsubResult; 
		char *strsubTokResult;

		strsubResult = strsub(argv[i], 0, 1);
		strcpy(viewOption, strsubResult);
		strsubTokResult = strtok(argv[i], strsubResult);
		argv[i] = strsubTokResult;

		if(argc==i ||  0 == atoi(argv[i]))
		{	
			for( j=1;j<option;j++)
			{
				selectList[selectNumber]=j;
				selectNumber++;
			}
			i++;
		}
		else
		{
			if(countOptionValue(argc,argv,&i, option) == -1)
				return -1;
		}
	}

	return 1;
}
int countOptionValue(int argc, char **argv, int *countArgument, int option)
{
	int i = *countArgument;

	char *delimiterCheck;
	char *delimiterStrtok;

	//Delimiter Check
	delimiterCheck = strcpy(delimiterBuffer, argv[i]); 

	if(countDelimiter(delimiterCheck, ',') >= 1)
	{
		delimiterStrtok = strtok(argv[i], ",");

		for(i; i < argc; i++)
		{
			if(selectNumber<option)
			{
				while(delimiterStrtok != NULL)
				{
					argv[i] = delimiterStrtok;
					selectList[selectNumber]=atoi(argv[i]);
					selectNumber++;
					delimiterStrtok = strtok(NULL, ",");

				}
				if(atoi(argv[i]) >= option)
				{
					PrintUsage(argv[0]);
					return -1;
				}

			}
		}
	}
	else
	{
		for(i; i < argc; i++)
		{
			if(selectNumber<option)
			{
				selectList[selectNumber]=atoi(argv[i]);
				selectNumber++;

				if(atoi(argv[i]) >= option)
				{
					PrintUsage(argv[0]);
					return -1;
				}
			}
		}
	}

	return 1;
}

void PrintUsage(char * cmd)
{
	printf("get storage list command %s\n", CURRENT_VERSION_DISPLAY_TYPE);
	printf("Usage : %s -i<ip> -p<port> (-a<all> or -t<tape> or -d<disk>)\n", cmd);
	printf("  -i<ip>               IP address of the Master Server.\n");
	printf("  -p<port>             Port number of the Master Server Httpd.\n");
	printf("  -a<option ..> View All Storage. defalt : 0 ( 1 ~ 11 ) \n"); 
	printf("  -t<option ..> Only View Tape Storage. defalt : 0 ( 1 ~ 8 ) \n"); 
	printf("  -d<option ..> Only View Disk Storage. defalt : 0 ( 1 ~ 7 ) \n"); 
	printf("  < -a Option Example ...>\n");
	printf("  1 :Type               2 :DriveType (TAPE)      3 :Status                 4 :NodeName              5 :OwnerStatus \n");
	printf("  6 :Device (TAPE)      7 :Address (TAPE)        8 :Library Type (TAPE)    9 :Volume Size (DISK)   10 :Capacity (DISK)   11 :Repository (DISK)\n");
	printf("  < -t Option Example ...>\n");
	printf("  1 :Type               2 :DriveType             3 :Status                 4 :NodeName              5 :OwnerStatus \n");
	printf("  6 :Device             7 :Address               8 :Library Type \n");
	printf("  < -d Option Example ...>\n");
	printf("  1 :Type               2 :Status                3 :NodeName               4 :OwnerStatus           5 :Volume Size \n");
	printf("  6 :Capacity           7 :Repository\n");
}
int countDelimiter(char *searchObject, char delimiter)
{
	int i;
	int count=0;

	for(i=0; searchObject[i] != '\0'; i++)
	{
		if(searchObject[i]==delimiter)
			count++;
		else
			continue;
	}
	return count;
}
