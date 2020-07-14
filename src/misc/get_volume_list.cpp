#include "require.h"
#include "abio.h"
#include "lib.h"
#include "vendor.h"
#include "get_volume_list.h"


// start of variables for abio library comparability
char ABIOMASTER_LOG_FOLDER[MAX_PATH_LENGTH];
// end of variables for abio library comparability

char ABIOMASTER_TOOL_FOLDER[MAX_PATH_LENGTH];

//////////////////////////////////////////////////////////////////////////////////////////////////
// master server ip and port
char masterIP[IP_LENGTH];
char masterPort[PORT_LENGTH];
int  viewOption;
int  selectList[SELECTLIST_NUMBER];
int	 selectNumber;
char delimiterBuffer[DELIMITER];

//////////////////////////////////////////////////////////////////////////////////////////////////

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
	viewOption=0;

	memset(&selectList,0,sizeof(selectList));
	selectNumber=0;


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
		printf("Failed to get a working directory.\n");

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
	int j;
	//Delimiter Check Object
	char *delimiterCheck;
	char *delimiterStrtok;

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
		else if (!strncmp(argv[i], "-o", 2))
		{
			if (strlen(argv[i]) == 2)
			{		
				i++;

				if(argc==i || 0 == atoi(argv[i]))
				{
					for( j=1;j<DEFAULTS_INFORMATION;j++)
					{
						selectList[selectNumber]=j;
						selectNumber++;
					}
					i++;
				}
				else
				{
					delimiterCheck = strcpy(delimiterBuffer, argv[i]);
					if(countDelimiter(delimiterCheck,',')>=1)
					{
						delimiterStrtok = strtok(argv[i], ",");

						for(i;i<argc;i++)
						{
							if(selectNumber<OPTION_NUMBER)
							{
								while(delimiterStrtok != NULL)
								{
									argv[i] = delimiterStrtok;
									selectList[selectNumber]=atoi(argv[i]);
									selectNumber++;
									delimiterStrtok = strtok(NULL, ",");
								}
								if(atoi(argv[i]) >= OPTION_NUMBER)
								{
									PrintUsage(argv[0]);
									return -1;
								}
							}
						}
					}
					else 
					{
						for(i;i<argc;i++)
						{
							if(selectNumber<OPTION_NUMBER)
							{
								selectList[selectNumber]=atoi(argv[i]);
								selectNumber++;
							}
							else
							{
								PrintUsage(argv[0]);
								return -1;
							}
						}
					}
				}
			}
			else
			{
				char *strsubResult;
				char *strsubTokResult;
				strsubResult = strsub(argv[i], 0, 1);
				//WriteDebugData("C:\\", "t1.txt", "%s", strsubResult);
				strsubTokResult = strtok(argv[i], strsubResult);
				argv[i] = strsubTokResult;


				if(argc==i || 0 == atoi(argv[i]))
				{
					for( j=1;j<DEFAULTS_INFORMATION;j++)
					{
						selectList[selectNumber]=j;
						selectNumber++;
					}
					i++;
				}
				else
				{
					delimiterCheck = strcpy(delimiterBuffer, argv[i]);
					if(countDelimiter(delimiterBuffer,',')>=1)
					{
						delimiterStrtok = strtok(argv[i], ",");

						for(i;i<argc;i++)
						{
							if(selectNumber<OPTION_NUMBER)
							{
								while(delimiterStrtok != NULL)
								{
									argv[i] = delimiterStrtok;
									selectList[selectNumber]=atoi(argv[i]);
									selectNumber++;
									delimiterStrtok = strtok(NULL, ",");
								}
								if(atoi(argv[i]) >= OPTION_NUMBER)
								{
									PrintUsage(argv[0]);
									return -1;
								}
							}
						}
					}
					else 
					{
						for(i;i<argc;i++)
						{
							if(selectNumber<OPTION_NUMBER)
							{
								selectList[selectNumber]=atoi(argv[i]);
								selectNumber++;
							}
							else
							{
								PrintUsage(argv[0]);
								return -1;
							}
						}
					}
				}
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
		return 0;
	}
	else
	{
		printf("Try `%s --help' for more information.\n", argv[0]);

		return -1;
	}
}

int SendCommand()
{
	char cmd[DSIZ][DSIZ];
	char msg[DSIZ * DSIZ];
	char reply[DSIZ][DSIZ];

	va_sock_t sock;

	int i;
	int j;

	char cmd2[DSIZ][DSIZ];
	char msg2[DSIZ * DSIZ];
	char reply2[DSIZ][DSIZ];

	va_sock_t sock2;

	int z;
	int k;
	
	// initialize variables
	struct volumeItem * volumeList = NULL;
	int volumeNumber= 0;

	struct volumepoolItem * volumepoolList = NULL;
	int volumepoolNumber = 0;
	int volumepoolNumberfor = 0;

	char *** itemList = NULL;
	int listNumber = 0;;
	int itemNumber = 0;

	char *** poolitemList = NULL;
	int poollistNumber = 0;
	int poolitemNumber = 0;
	
	// command를 만든다.
	va_init_reply_buf(cmd);

	strcpy(cmd[0], "<GET_VOLUME_LIST>");

	va_make_reply_msg(cmd, msg);

	// command를 master server로 전송한다.
	if ((sock = va_connect(masterIP, masterPort, 1)) != -1)
	{
		va_send(sock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE);
		memset(msg, 0, sizeof(msg));
		// command 전송 결과를 받음
		
		//master server로부터응답받음
		while (RecvReply(sock, msg, sizeof(msg), 0) > 0)
		{
			va_parser(msg, reply);
			memset(msg, 0, sizeof(msg));

			if (!strcmp(reply[0], "VOLUME_LIST_START"))
			{
				if (volumeNumber % DEFAULT_MEMORY_FRAGMENT == 0)
				{
					volumeList = (struct volumeItem *)realloc(volumeList, sizeof(struct volumeItem) * (volumeNumber + DEFAULT_MEMORY_FRAGMENT));
					memset(volumeList + volumeNumber, 0, sizeof(struct volumeItem) * DEFAULT_MEMORY_FRAGMENT);
				}

				strcpy(volumeList[volumeNumber].name, "Volume");
				strcpy(volumeList[volumeNumber].type, "Volume_Type");
				strcpy(volumeList[volumeNumber].pool, "Volume_Pool");
				strcpy(volumeList[volumeNumber].storage, "Storage");
				strcpy(volumeList[volumeNumber].location, "Volume_Location");
				strcpy(volumeList[volumeNumber].use, "In_Use");
				strcpy(volumeList[volumeNumber].usage, "Volume_Usage");
				strcpy(volumeList[volumeNumber].volumeSize, "Bytes_Written");
				strcpy(volumeList[volumeNumber].volumePoolAccess, "Scratch");
				strcpy(volumeList[volumeNumber].volumeAccess, "Volume_Access");
				//추가 정보
				strcpy(volumeList[volumeNumber].volumeIndex, "VolumeIndex");
				//strcpy(volumeList[volumeNumber].jobID, "jobID");
				//strcpy(volumeList[volumeNumber].version, "version");
				//strcpy(volumeList[volumeNumber].address, "address");
				strcpy(volumeList[volumeNumber].backupsetNumber, "backupset");
				strcpy(volumeList[volumeNumber].expiredBackupsetNumber, "validBackupset");//Number");
				strcpy(volumeList[volumeNumber].mountCount, "mountCount");
				strcpy(volumeList[volumeNumber].unmountCount, "unmountCount");
				strcpy(volumeList[volumeNumber].writeErrorCount, "writeErrorCount");
				strcpy(volumeList[volumeNumber].readErrorCount, "readErrorCount");
				strcpy(volumeList[volumeNumber].mountTime, "mountTime");
				strcpy(volumeList[volumeNumber].unmountTime, "unmountTime");
				strcpy(volumeList[volumeNumber].writeTime, "writeTime");
				strcpy(volumeList[volumeNumber].readTime, "readTime");
				strcpy(volumeList[volumeNumber].writeErrorTime, "writeErrorTime");
				strcpy(volumeList[volumeNumber].readErrorTime, "readErrorTime");
				//strcpy(volumeList[volumeNumber].unavailableTime, "unavailableTime");
				strcpy(volumeList[volumeNumber].assignTime, "assignTime");
				strcpy(volumeList[volumeNumber].expirationTime, "expirationTime");
				strcpy(volumeList[volumeNumber].exportedTime, "exportedTime");
				strcpy(volumeList[volumeNumber].capacity, "capacity");
				strcpy(volumeList[volumeNumber].maxcapacity, "max_capacity");
				//strcpy(volumeList[volumeNumber].backupsetPosition, "backupsetPosition");

				volumeNumber++;

				if (volumepoolNumber % DEFAULT_MEMORY_FRAGMENT == 0)
				{
					volumepoolList = (struct volumepoolItem *)realloc(volumepoolList, sizeof(struct volumepoolItem) * (volumepoolNumber + DEFAULT_MEMORY_FRAGMENT));
					memset(volumepoolList + volumepoolNumber, 0, sizeof(struct volumepoolItem) * DEFAULT_MEMORY_FRAGMENT);
				}

				volumepoolNumber++;
			}
			else if (!strcmp(reply[0], "VOLUME_LIST"))
			{
				if (volumeNumber % DEFAULT_MEMORY_FRAGMENT == 0)
				{
					volumeList = (struct volumeItem *)realloc(volumeList, sizeof(struct volumeItem) * (volumeNumber + DEFAULT_MEMORY_FRAGMENT));
					memset(volumeList + volumeNumber, 0, sizeof(struct volumeItem) * DEFAULT_MEMORY_FRAGMENT);
				}

				// volume name
				strcpy(volumeList[volumeNumber].name, reply[40]);
				volumeList[volumeNumber].maxcapacityint64 = 0;
				sprintf(volumeList[volumeNumber].maxcapacity,"-");

				// volume type
				if (atoi(reply[42]) == 0)
				{
					strcpy(volumeList[volumeNumber].type, "Unknown");
				}
				else if (atoi(reply[42]) == 1)
				{
					strcpy(volumeList[volumeNumber].type, "Disk_Volume");

					// command를 만든다.
					va_init_reply_buf(cmd2);

					strcpy(cmd2[0], "<GET_STORAGE_DISK>");
					strcpy(cmd2[210], reply[65]);

					va_make_reply_msg(cmd2, msg2);


					// command를 master server로 전송한다.
					if ((sock2 = va_connect(masterIP, masterPort, 1)) != -1)
					{
						va_send(sock2, msg2, (int)strlen(msg2) + 1, 0, DATA_TYPE_NOT_CHANGE);
						memset(msg2, 0, sizeof(msg2));
						// command 전송 결과를 받음

						while (RecvReply(sock2, msg2, sizeof(msg2), 0) > 0)
						{
							va_parser(msg2, reply2);
							memset(msg2, 0, sizeof(msg2));

							if (!strcmp(reply2[0], "STORAGE_DISK_START"))
							{
							}
							else if (!strcmp(reply2[0], "STORAGE_DISK"))
							{
								volumeList[volumeNumber].maxcapacityint64 = va_atoll(reply2[230])*1024*1024*1024;
								sprintf(volumeList[volumeNumber].maxcapacity,"%sGB", reply2[230]);
							}
							else if (!strcmp(reply2[0], "STORAGE_DISK_END"))
							{
								break;
							}
							else
							{
								// 잘못된 메세지가 온 경우 종료한다.
								printf("Received a wrong message.\n");
								break;
							}
						}
						va_close_socket(sock2, ABIO_SOCKET_CLOSE_SERVER);
					}
					else
					{
						printf("Failed to send the request to master server.\n");

						return -1;
					}
				}
				else if (atoi(reply[42]) == 2)
				{
					strcpy(volumeList[volumeNumber].type, "Generic");
				}
				else if (atoi(reply[42]) == 10)
				{
					strcpy(volumeList[volumeNumber].type, "LTO1");
				}
				else if (atoi(reply[42]) == 11)
				{
					strcpy(volumeList[volumeNumber].type, "LTO2");
				}
				else if (atoi(reply[42]) == 12)
				{
					strcpy(volumeList[volumeNumber].type, "LTO3");
				}
				else if (atoi(reply[42]) == 13)
				{
					strcpy(volumeList[volumeNumber].type, "LTO4");
				}
				else if (atoi(reply[42]) == 14)
				{
					strcpy(volumeList[volumeNumber].type, "LTO5");
				}
				else
				{
					strcpy(volumeList[volumeNumber].type, "Unknown");
				}

				// volume pool
				strcpy(volumeList[volumeNumber].pool, reply[66]);

				// storage
				strcpy(volumeList[volumeNumber].storage, reply[65]);

				// location
				if (atoi(reply[42]) == 1 || reply[65][0] == '\0')
				{
					strcpy(volumeList[volumeNumber].location, "-");
				}
				else
				{
					if (atoi(reply[46]) == 0)
					{
						strcpy(volumeList[volumeNumber].location, "In_Storage");
					}
					else
					{
						strcpy(volumeList[volumeNumber].location, "Out_Of_Storage");
					}
				}

				// use
				if (atoi(reply[45]) == 0)
				{
					strcpy(volumeList[volumeNumber].use, "No");
				}
				else
				{
					strcpy(volumeList[volumeNumber].use, "Yes");
				}

				// usage
				if (atoi(reply[47]) == 0)
				{
					strcpy(volumeList[volumeNumber].usage, "Full");
				}
				else if (atoi(reply[47]) == 1)
				{
					strcpy(volumeList[volumeNumber].usage, "Empty");
				}
				else if (atoi(reply[47]) == 2)
				{
					strcpy(volumeList[volumeNumber].usage, "Filling");
				}

				// volume size
				if (va_atoll(reply[63]) == 0)
				{
					sprintf(volumeList[volumeNumber].volumeSize, "0KB");
				}
				else if (va_atoll(reply[63]) / 1024 == 0)
				{
					sprintf(volumeList[volumeNumber].volumeSize, "%.2fKB", va_atoll(reply[63]) / 1024.0);
				}
				else if (va_atoll(reply[63]) / 1024 / 1024 == 0)
				{
					sprintf(volumeList[volumeNumber].volumeSize, "%.2fKB", va_atoll(reply[63]) / 1024.0);
				}
				else if (va_atoll(reply[63]) / 1024 / 1024 / 1024 == 0)
				{
					sprintf(volumeList[volumeNumber].volumeSize, "%.2fMB", va_atoll(reply[63]) / 1024.0 / 1024.0);
				}
				else
				{
					sprintf(volumeList[volumeNumber].volumeSize, "%.2fGB", va_atoll(reply[63]) / 1024.0 / 1024.0 / 1024.0);
				}

				// volume pool access
				if (atoi(reply[44]) == 0)
				{
					strcpy(volumeList[volumeNumber].volumePoolAccess, "Enable");
				}
				else
				{
					strcpy(volumeList[volumeNumber].volumePoolAccess, "Disable");
				}

				// volume access
				if (atoi(reply[43]) == 0)
				{
					strcpy(volumeList[volumeNumber].volumeAccess, "Read/Write");
				}
				else
				{
					strcpy(volumeList[volumeNumber].volumeAccess, "Read_Only");
				}


				////////////////////////
				//추가 정보
				// volume Index				
				sprintf(volumeList[volumeNumber].volumeIndex,"%d", volumeNumber-1);
				// volume jobiD				
				//strcpy(volumeList[volumeNumber].jobID, "--");
				// volume version
				//strcpy(volumeList[volumeNumber].version, "--");
				// volume address
				//strcpy(volumeList[volumeNumber].address, reply[41]);
				// volume backupsetNumber
				strcpy(volumeList[volumeNumber].backupsetNumber, reply[48]);
				// volume expiredBackupsetNumber
				strcpy(volumeList[volumeNumber].expiredBackupsetNumber, reply[49]);
				// volume mountCount
				strcpy(volumeList[volumeNumber].mountCount, reply[50]);
				// volume unmountCount
				strcpy(volumeList[volumeNumber].unmountCount, reply[51]);
				// volume writeErrorCount
				strcpy(volumeList[volumeNumber].writeErrorCount, reply[52]);
				// volume readErrorCount
				strcpy(volumeList[volumeNumber].readErrorCount, reply[53]);
				// volume mountTime
				strcpy(volumeList[volumeNumber].mountTime, reply[54]);
				// volume unmountTime
				strcpy(volumeList[volumeNumber].unmountTime, reply[55]);
				// volume writeTime
				strcpy(volumeList[volumeNumber].writeTime, reply[56]);
				// volume readTime
				if (reply[57][0] == '\0' || reply[57] == "")
				{
					strcpy(volumeList[volumeNumber].readTime, "-");
				}
				else
				{
					strcpy(volumeList[volumeNumber].readTime, reply[57]);
				}

				// volume writeErrorTime
				if (reply[58][0] == '\0' || reply[58] == "")
				{
					strcpy(volumeList[volumeNumber].writeErrorTime, "-");
				}
				else
				{
					strcpy(volumeList[volumeNumber].writeErrorTime, reply[58]);
				}
				// volume readErrorTime
				if (reply[59][0] == '\0' || reply[59] == "")
				{
					strcpy(volumeList[volumeNumber].readErrorTime, "-");
				}
				else
				{
					strcpy(volumeList[volumeNumber].readErrorTime, reply[59]);
				}
				// volume unavailableTime ??
				//strcpy(volumeList[volumeNumber].unavailableTime, "--");
				// volume assignTime
				strcpy(volumeList[volumeNumber].assignTime, reply[60]);
				// volume expirationTime
				if (reply[61][0] == '\0' || reply[61] == "")
				{
					strcpy(volumeList[volumeNumber].expirationTime, "-");
				}
				else
				{
					strcpy(volumeList[volumeNumber].expirationTime, reply[61]);
				}
				// volume exportedTime
				if (reply[62][0] == '\0' || reply[62] == "")
				{
					strcpy(volumeList[volumeNumber].exportedTime, "-");
				}
				else
				{
					strcpy(volumeList[volumeNumber].exportedTime, reply[62]);
				}
				// volume capacity
				sprintf(volumeList[volumeNumber].capacity,"%sGB", reply[64]);

				// volume backupsetPosition ??
				//strcpy(volumeList[volumeNumber].backupsetPosition, "--");

				///////////////////////


				///////////////////////////////////////////////////
				k=0;
				for(z=0; z<volumepoolNumberfor+1; z++)
				{
					if (!strcmp(volumeList[volumeNumber].pool, volumepoolList[z].pool))
					{
						k=1;
						volumepoolList[z].usageint64 += va_atoll(reply[63]);
						volumepoolList[z].volumecount += 1;
						break;
					}					
				}
				if(k!=1)
				{
					if (volumepoolNumber % DEFAULT_MEMORY_FRAGMENT == 0)
					{
						volumepoolList = (struct volumepoolItem *)realloc(volumepoolList, sizeof(struct volumepoolItem) * (volumepoolNumber + DEFAULT_MEMORY_FRAGMENT));
						memset(volumepoolList + volumepoolNumber, 0, sizeof(struct volumepoolItem) * DEFAULT_MEMORY_FRAGMENT);
					}

					strcpy(volumepoolList[volumepoolNumber].pool, volumeList[volumeNumber].pool);
					sprintf(volumepoolList[volumepoolNumber].maxcapacity ,volumeList[volumeNumber].maxcapacity);	
					volumepoolList[volumepoolNumber].usageint64 = va_atoll(reply[63]);
					volumepoolList[volumepoolNumber].maxcapacityint64 = volumeList[volumeNumber].maxcapacityint64;
					volumepoolList[volumepoolNumber].volumecount = 1;

					volumepoolNumber++;
				}
				volumepoolNumberfor = volumepoolNumber;
				//////////////////////////////////////////////////

				volumeNumber++;
			}
			else if (!strcmp(reply[0], "VOLUME_LIST_END"))
			{
				listNumber = volumeNumber;				
				itemNumber = selectNumber+1;

				itemList = (char ***)malloc(sizeof(char **) * listNumber);

				for (i = 0; i < listNumber; i++)
				{
					itemList[i] = (char **)malloc(sizeof(char *) * itemNumber);

					itemList[i][0] = (char *)malloc(sizeof(char) * (strlen(volumeList[i].name) + 1));
					memset(itemList[i][0], 0, sizeof(char) * (strlen(volumeList[i].name) + 1));
					strcpy(itemList[i][0], volumeList[i].name);

					for( j=0;j<selectNumber;j++)
					{
						if(((enum volumeSelect)type)==selectList[j])
						{
							itemList[i][j+1] = (char *)malloc(sizeof(char) * (strlen(volumeList[i].type) + 1));
							memset(itemList[i][j+1], 0, sizeof(char) * (strlen(volumeList[i].type) + 1));
							strcpy(itemList[i][j+1], volumeList[i].type);
						}
						else if(((enum volumeSelect)pool)==selectList[j])
						{	
							itemList[i][j+1] = (char *)malloc(sizeof(char) * (strlen(volumeList[i].pool) + 1));
							memset(itemList[i][j+1], 0, sizeof(char) * (strlen(volumeList[i].pool) + 1));
							strcpy(itemList[i][j+1], volumeList[i].pool);
						}
						else if(((enum volumeSelect)storage)==selectList[j])
						{
							itemList[i][j+1] = (char *)malloc(sizeof(char) * (strlen(volumeList[i].storage) + 1));
							memset(itemList[i][j+1], 0, sizeof(char) * (strlen(volumeList[i].storage) + 1));
							strcpy(itemList[i][j+1], volumeList[i].storage);
						}
						else if(((enum volumeSelect)location)==selectList[j])
						{	
							itemList[i][j+1] = (char *)malloc(sizeof(char) * (strlen(volumeList[i].location) + 1));
							memset(itemList[i][j+1], 0, sizeof(char) * (strlen(volumeList[i].location) + 1));
							strcpy(itemList[i][j+1], volumeList[i].location);
						}
						else if(((enum volumeSelect)use)==selectList[j])
						{	
							itemList[i][j+1] = (char *)malloc(sizeof(char) * (strlen(volumeList[i].use) + 1));
							memset(itemList[i][j+1], 0, sizeof(char) * (strlen(volumeList[i].use) + 1));
							strcpy(itemList[i][j+1], volumeList[i].use);
						}
						else if(((enum volumeSelect)usage)==selectList[j])
						{	
							itemList[i][j+1] = (char *)malloc(sizeof(char) * (strlen(volumeList[i].usage) + 1));
							memset(itemList[i][j+1], 0, sizeof(char) * (strlen(volumeList[i].usage) + 1));
							strcpy(itemList[i][j+1], volumeList[i].usage);
						}
						else if(((enum volumeSelect)volumeSize)==selectList[j])
						{	
							itemList[i][j+1] = (char *)malloc(sizeof(char) * (strlen(volumeList[i].volumeSize) + 1));
							memset(itemList[i][j+1], 0, sizeof(char) * (strlen(volumeList[i].volumeSize) + 1));
							strcpy(itemList[i][j+1], volumeList[i].volumeSize);
						}
						else if(((enum volumeSelect)volumePoolAccess)==selectList[j])
						{	
							itemList[i][j+1] = (char *)malloc(sizeof(char) * (strlen(volumeList[i].volumePoolAccess) + 1));
							memset(itemList[i][j+1], 0, sizeof(char) * (strlen(volumeList[i].volumePoolAccess) + 1));
							strcpy(itemList[i][j+1], volumeList[i].volumePoolAccess);
						}
						else if(((enum volumeSelect)volumeAccess)==selectList[j])
						{	
							itemList[i][j+1] = (char *)malloc(sizeof(char) * (strlen(volumeList[i].volumeAccess) + 1));
							memset(itemList[i][j+1], 0, sizeof(char) * (strlen(volumeList[i].volumeAccess) + 1));
							strcpy(itemList[i][j+1], volumeList[i].volumeAccess);
						}

						////////////////////////
						//추가 정보
						/*itemList[i][1] = (char *)malloc(sizeof(char) * (strlen(volumeList[i].jobID) + 1));
						memset(itemList[i][1], 0, sizeof(char) * (strlen(volumeList[i].jobID) + 1));
						strcpy(itemList[i][1], volumeList[i].jobID);

						itemList[i][2] = (char *)malloc(sizeof(char) * (strlen(volumeList[i].version) + 1));
						memset(itemList[i][2], 0, sizeof(char) * (strlen(volumeList[i].version) + 1));
						strcpy(itemList[i][2], volumeList[i].version);*/

						/*itemList[i][1] = (char *)malloc(sizeof(char) * (strlen(volumeList[i].address) + 1));
						memset(itemList[i][1], 0, sizeof(char) * (strlen(volumeList[i].address) + 1));
						strcpy(itemList[i][1], volumeList[i].address);*/

						else if(((enum volumeSelect)volumeIndex)==selectList[j])
						{
							itemList[i][j+1] = (char *)malloc(sizeof(char) * (strlen(volumeList[i].volumeIndex) + 1));
							memset(itemList[i][j+1], 0, sizeof(char) * (strlen(volumeList[i].volumeIndex) + 1));
							strcpy(itemList[i][j+1], volumeList[i].volumeIndex);
						}
						else if(((enum volumeSelect)backupsetNumber)==selectList[j])
						{
							itemList[i][j+1] = (char *)malloc(sizeof(char) * (strlen(volumeList[i].backupsetNumber) + 1));
							memset(itemList[i][j+1], 0, sizeof(char) * (strlen(volumeList[i].backupsetNumber) + 1));
							strcpy(itemList[i][j+1], volumeList[i].backupsetNumber);
						}
						else if(((enum volumeSelect)expiredBackupsetNumber)==selectList[j])
						{
							itemList[i][j+1] = (char *)malloc(sizeof(char) * (strlen(volumeList[i].expiredBackupsetNumber) + 1));
							memset(itemList[i][j+1], 0, sizeof(char) * (strlen(volumeList[i].expiredBackupsetNumber) + 1));
							strcpy(itemList[i][j+1], volumeList[i].expiredBackupsetNumber);
						}
						else if(((enum volumeSelect)mountCount)==selectList[j])
						{
							itemList[i][j+1] = (char *)malloc(sizeof(char) * (strlen(volumeList[i].mountCount) + 1));
							memset(itemList[i][j+1], 0, sizeof(char) * (strlen(volumeList[i].mountCount) + 1));
							strcpy(itemList[i][j+1], volumeList[i].mountCount);
						}
						else if(((enum volumeSelect)unmountCount)==selectList[j])
						{
							itemList[i][j+1] = (char *)malloc(sizeof(char) * (strlen(volumeList[i].unmountCount) + 1));
							memset(itemList[i][j+1], 0, sizeof(char) * (strlen(volumeList[i].unmountCount) + 1));
							strcpy(itemList[i][j+1], volumeList[i].unmountCount);
						}
						else if(((enum volumeSelect)writeErrorCount)==selectList[j])
						{
							itemList[i][j+1] = (char *)malloc(sizeof(char) * (strlen(volumeList[i].writeErrorCount) + 1));
							memset(itemList[i][j+1], 0, sizeof(char) * (strlen(volumeList[i].writeErrorCount) + 1));
							strcpy(itemList[i][j+1], volumeList[i].writeErrorCount);
						}
						else if(((enum volumeSelect)readErrorCount)==selectList[j])
						{
							itemList[i][j+1] = (char *)malloc(sizeof(char) * (strlen(volumeList[i].readErrorCount) + 1));
							memset(itemList[i][j+1], 0, sizeof(char) * (strlen(volumeList[i].readErrorCount) + 1));
							strcpy(itemList[i][j+1], volumeList[i].readErrorCount);
						}
						else if(((enum volumeSelect)mountTime)==selectList[j])
						{
							itemList[i][j+1] = (char *)malloc(sizeof(char) * (strlen(volumeList[i].mountTime) + 1));
							memset(itemList[i][j+1], 0, sizeof(char) * (strlen(volumeList[i].mountTime) + 1));
							strcpy(itemList[i][j+1], volumeList[i].mountTime);
						}
						else if(((enum volumeSelect)unmountTime)==selectList[j])
						{
							itemList[i][j+1] = (char *)malloc(sizeof(char) * (strlen(volumeList[i].unmountTime) + 1));
							memset(itemList[i][j+1], 0, sizeof(char) * (strlen(volumeList[i].unmountTime) + 1));
							strcpy(itemList[i][j+1], volumeList[i].unmountTime);
						}
						else if(((enum volumeSelect)writeTime)==selectList[j])
						{
							itemList[i][j+1] = (char *)malloc(sizeof(char) * (strlen(volumeList[i].writeTime) + 1));
							memset(itemList[i][j+1], 0, sizeof(char) * (strlen(volumeList[i].writeTime) + 1));
							strcpy(itemList[i][j+1], volumeList[i].writeTime);
						}
						else if(((enum volumeSelect)readTime)==selectList[j])
						{
							itemList[i][j+1] = (char *)malloc(sizeof(char) * (strlen(volumeList[i].readTime) + 1));
							memset(itemList[i][j+1], 0, sizeof(char) * (strlen(volumeList[i].readTime) + 1));
							strcpy(itemList[i][j+1], volumeList[i].readTime);
						}
						else if(((enum volumeSelect)writeErrorTime)==selectList[j])
						{
							itemList[i][j+1] = (char *)malloc(sizeof(char) * (strlen(volumeList[i].writeErrorTime) + 1));
							memset(itemList[i][j+1], 0, sizeof(char) * (strlen(volumeList[i].writeErrorTime) + 1));
							strcpy(itemList[i][j+1], volumeList[i].writeErrorTime);
						}
						else if(((enum volumeSelect)readErrorTime)==selectList[j])
						{
							itemList[i][j+1] = (char *)malloc(sizeof(char) * (strlen(volumeList[i].readErrorTime) + 1));
							memset(itemList[i][j+1], 0, sizeof(char) * (strlen(volumeList[i].readErrorTime) + 1));
							strcpy(itemList[i][j+1], volumeList[i].readErrorTime);
						}
						/*itemList[i][16] = (char *)malloc(sizeof(char) * (strlen(volumeList[i].unavailableTime) + 1));
						memset(itemList[i][16], 0, sizeof(char) * (strlen(volumeList[i].unavailableTime) + 1));
						strcpy(itemList[i][16], volumeList[i].unavailableTime);*/

						else if(((enum volumeSelect)assignTime)==selectList[j])
						{
							itemList[i][j+1] = (char *)malloc(sizeof(char) * (strlen(volumeList[i].assignTime) + 1));
							memset(itemList[i][j+1], 0, sizeof(char) * (strlen(volumeList[i].assignTime) + 1));
							strcpy(itemList[i][j+1], volumeList[i].assignTime);
						}
						else if(((enum volumeSelect)expirationTime)==selectList[j])
						{
							itemList[i][j+1] = (char *)malloc(sizeof(char) * (strlen(volumeList[i].expirationTime) + 1));
							memset(itemList[i][j+1], 0, sizeof(char) * (strlen(volumeList[i].expirationTime) + 1));
							strcpy(itemList[i][j+1], volumeList[i].expirationTime);
						}
						else if(((enum volumeSelect)exportedTime)==selectList[j])
						{
							itemList[i][j+1] = (char *)malloc(sizeof(char) * (strlen(volumeList[i].exportedTime) + 1));
							memset(itemList[i][j+1], 0, sizeof(char) * (strlen(volumeList[i].exportedTime) + 1));
							strcpy(itemList[i][j+1], volumeList[i].exportedTime);
						}
						else if(((enum volumeSelect)capacity)==selectList[j])
						{
							itemList[i][j+1] = (char *)malloc(sizeof(char) * (strlen(volumeList[i].capacity) + 1));
							memset(itemList[i][j+1], 0, sizeof(char) * (strlen(volumeList[i].capacity) + 1));
							strcpy(itemList[i][j+1], volumeList[i].capacity);
						}
						else if(((enum volumeSelect)maxcapacity)==selectList[j])
						{
							itemList[i][j+1] = (char *)malloc(sizeof(char) * (strlen(volumeList[i].maxcapacity) + 1));
							memset(itemList[i][j+1], 0, sizeof(char) * (strlen(volumeList[i].maxcapacity) + 1));
							strcpy(itemList[i][j+1], volumeList[i].maxcapacity);
						}
						/*itemList[i][21] = (char *)malloc(sizeof(char) * (strlen(volumeList[i].backupsetPosition) + 1));
						memset(itemList[i][21], 0, sizeof(char) * (strlen(volumeList[i].backupsetPosition) + 1));
						strcpy(itemList[i][21], volumeList[i].backupsetPosition);*/
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
	///////////////////////////////////////////////////

	if(0)
	{
		poollistNumber = volumepoolNumber;				
		poolitemNumber = 5;

		poolitemList = (char ***)malloc(sizeof(char **) * poollistNumber);

		for (i = 0; i < poollistNumber; i++)
		{
			poolitemList[i] = (char **)malloc(sizeof(char *) * poolitemNumber);

			if(i==0)
			{
				strcpy(volumepoolList[i].pool, "Volume_pool");
				strcpy(volumepoolList[i].volumecountchar, "Volume_count");
				strcpy(volumepoolList[i].usage, "Bytes_Written");
				strcpy(volumepoolList[i].maxcapacity, "Max_capacity");
				strcpy(volumepoolList[i].p, "Usage");
			}
			else
			{
				//	volume count
				sprintf(volumepoolList[i].volumecountchar, "%d", volumepoolList[i].volumecount);

				// volume size
				if (volumepoolList[i].usageint64 == 0)
				{
					sprintf(volumepoolList[i].usage, "0KB");
				}
				else if (volumepoolList[i].usageint64 / 1024 == 0)
				{
					sprintf(volumepoolList[i].usage, "%.2fKB", volumepoolList[i].usageint64 / 1024.0);
				}
				else if (volumepoolList[i].usageint64 / 1024 / 1024 == 0)
				{
					sprintf(volumepoolList[i].usage, "%.2fKB", volumepoolList[i].usageint64 / 1024.0);
				}
				else if (volumepoolList[i].usageint64 / 1024 / 1024 / 1024 == 0)
				{
					sprintf(volumepoolList[i].usage, "%.2fMB", volumepoolList[i].usageint64 / 1024.0 / 1024.0);
				}
				else
				{
					sprintf(volumepoolList[i].usage, "%.2fGB", volumepoolList[i].usageint64 / 1024.0 / 1024.0 / 1024.0);
				}

				//usage
				if(volumepoolList[i].maxcapacityint64 == 0)
				{
					sprintf(volumepoolList[i].p, "-");
				}
				else
				{
					sprintf(volumepoolList[i].p, "%.2lf%%", ((double)volumepoolList[i].usageint64/(double)volumepoolList[i].maxcapacityint64)*100);
				}
			}

			poolitemList[i][0] = (char *)malloc(sizeof(char) * (strlen(volumepoolList[i].pool) + 1));
			memset(poolitemList[i][0], 0, sizeof(char) * (strlen(volumepoolList[i].pool) + 1));
			strcpy(poolitemList[i][0], volumepoolList[i].pool);

			poolitemList[i][1] = (char *)malloc(sizeof(char) * (strlen(volumepoolList[i].volumecountchar) + 1));
			memset(poolitemList[i][1], 0, sizeof(char) * (strlen(volumepoolList[i].volumecountchar) + 1));
			strcpy(poolitemList[i][1], volumepoolList[i].volumecountchar);

			poolitemList[i][2] = (char *)malloc(sizeof(char) * (strlen(volumepoolList[i].usage) + 1));
			memset(poolitemList[i][2], 0, sizeof(char) * (strlen(volumepoolList[i].usage) + 1));
			strcpy(poolitemList[i][2], volumepoolList[i].usage);

			poolitemList[i][3] = (char *)malloc(sizeof(char) * (strlen(volumepoolList[i].maxcapacity) + 1));
			memset(poolitemList[i][3], 0, sizeof(char) * (strlen(volumepoolList[i].maxcapacity) + 1));
			strcpy(poolitemList[i][3], volumepoolList[i].maxcapacity);

			poolitemList[i][4] = (char *)malloc(sizeof(char) * (strlen(volumepoolList[i].p) + 1));
			memset(poolitemList[i][4], 0, sizeof(char) * (strlen(volumepoolList[i].p) + 1));
			strcpy(poolitemList[i][4], volumepoolList[i].p);
		}

		va_print_item_on_screen(NULL, 1, poolitemList, poollistNumber, poolitemNumber);
	}	

	return 0;
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
void PrintUsage(char * cmd)
{
	printf("get volume list command %s\n", CURRENT_VERSION_DISPLAY_TYPE);
	printf("Usage : %s -i<ip> -p<port> -o<option>\n", cmd);
	printf("  -i<ip>               IP address of the Master Server.\n");
	printf("  -p<port>             Port number of the Master Server Httpd.\n");
	printf("  -o<option option ..> View option. defalt : 0 ( 1 ~ 9 ) \n");
	printf("  1 :type            2 :pool           3 :storage          4 :location       5 :use\n");
	printf("  6 :usage           7 :volumeSize     8 :volumePoolAccess 9 :volumeAccess   10:volumeIndex\n");
	printf("  11:backupsetNumber 12:validBackupset 13:mountCount       14:unmountCount   15:writeErrorCount\n");
	printf("  16:readErrorCount  17:mountTime      18:unmountTime      19:writeTime      20:readTime \n");
	printf("  21:writeErrorTime  22:readErrorTime  23:assignTime       24:expirationTime 25:exportedTime\n");
	printf("  26:capacity\n");
}

void Exit()
{
#ifdef __ABIO_WIN
	//윈속 제거
	WSACleanup();
#endif
}
