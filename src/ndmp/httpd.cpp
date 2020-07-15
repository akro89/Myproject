#include "require.h"
#include "abio.h"
#include "lib.h"
#include "vendor.h"
#include "ndmpc.h"
#include "httpd.h"


// start of variables for abio library comparability
char ABIOMASTER_CK_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_MASTER_SERVER_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_DATA_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_CATALOG_DB_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_VOLUME_DB_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_FILE_LIST_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_LOG_FOLDER[MAX_PATH_LENGTH];
int DebugLevel;


struct portRule * portRule;		// VIRBAK ABIO IP Table 목록
int portRuleNumber;				// VIRBAK ABIO IP Table 개수
// end of variables for abio library comparability


char ABIOMASTER_CLIENT_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_NDMP_FILE_LIST_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_NDMP_LOG_FOLDER[MAX_PATH_LENGTH];


char ABIOMASTER_NDMP_TAPE_NAME[NDMP_DEVICE_NAME];

struct ck command;
struct ckBackup commandData;


//////////////////////////////////////////////////////////////////////////////////////////////////
// ABIO log option
int requestLogLevel;
char logJobID[JOB_ID_LENGTH];
int logModuleNumber;


//////////////////////////////////////////////////////////////////////////////////////////////////
// message transfer variables
va_trans_t mid;		// message transfer id
int mtype;			// message type number


//////////////////////////////////////////////////////////////////////////////////////////////////
// ndmp connect infomation
char ndmpAccount[NDMP_ACCOUNT_LENGTH];
char ndmpPasswd[NDMP_PASSWORD_LENGTH];
char ndmpIP[IP_LENGTH];
char ndmpPort[PORT_LENGTH];


//////////////////////////////////////////////////////////////////////////////////////////////////
// ndmp variables
ndmp_mover_listen_reply connmoveraddr;

int nMoverPauseState;
int nMoverHaltState;
int nDataHaltState;

int nMoverPauseReason;
int nMoverHaltReason;
int nDataHaltReason;


//////////////////////////////////////////////////////////////////////////////////////////////////
// ndmp configuration
int tapeDriveTimeOut;
int tapeDriveDelayTime;


//////////////////////////////////////////////////////////////////////////////////////////////////
// ip and port number
char masterIP[IP_LENGTH];					// master server ip address
char masterPort[PORT_LENGTH];				// master server ck port
char masterNodename[NODE_NAME_LENGTH];		// master server node name
char masterLogPort[PORT_LENGTH];			// master server httpd logger port


//////////////////////////////////////////////////////////////////////////////////////////////////
// ndmp log
extern SendJobLogFunc_t SendNdmpLogToMaster;


#ifdef __ABIO_UNIX
	int main(int argc, char ** argv)
#elif __ABIO_WIN
	int wmain(int argc, wchar_t ** argv)
#endif
	{
		char * convertFilename;
		int convertFilenameSize;
		
		
		
		// receive a message from ck
		#ifdef __ABIO_UNIX
			mid = (va_trans_t)atoi(argv[1]);
			mtype = atoi(argv[2]);
		#elif __ABIO_WIN
			#ifdef __ABIO_64
				mid = (va_trans_t)_wtoi64(argv[1]);
			#else
				mid = (va_trans_t)_wtoi(argv[1]);
			#endif
			mtype = _wtoi(argv[2]);
		#endif
		
		va_msgrcv(mid, mtype, &command, sizeof(struct ck), 0);
		memcpy(&commandData, command.dataBlock, sizeof(struct ckBackup));
		
		
		// initialize a process
		#ifdef __ABIO_UNIX
			if (va_convert_string_to_utf8(ENCODING_UNKNOWN, argv[0], (int)strlen(argv[0]), (void **)&convertFilename, &convertFilenameSize) == 0)
			{
				Exit();
				
				return 2;
			}
		#elif __ABIO_WIN
			if (va_convert_string_to_utf8(ENCODING_UTF16_LITTLE, (char *)argv[0], (int)wcslen(argv[0]) * sizeof(wchar_t), (void **)&convertFilename, &convertFilenameSize) == 0)
			{
				Exit();
				
				return 2;
			}
		#endif
		
		if (InitProcess(convertFilename) < 0)
		{
			va_free(convertFilename);
			
			Exit();
			
			return 2;
		}
		
		va_free(convertFilename);
		
		
		ResponseRequest();
		
		
		Exit();
		
		return 0;
	}

int InitProcess(char * filename)
{
	#ifdef __ABIO_WIN
		WSADATA wsaData;
		HANDLE token;
	#endif
	
	int i;
	
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 1. 전역 변수들을 초기화한다.
	
	memset(ABIOMASTER_CLIENT_FOLDER, 0, sizeof(ABIOMASTER_CLIENT_FOLDER));
	memset(ABIOMASTER_NDMP_FILE_LIST_FOLDER, 0, sizeof(ABIOMASTER_NDMP_FILE_LIST_FOLDER));
	memset(ABIOMASTER_NDMP_LOG_FOLDER, 0, sizeof(ABIOMASTER_NDMP_LOG_FOLDER));
	memset(ABIOMASTER_NDMP_TAPE_NAME, 0, sizeof(ABIOMASTER_NDMP_TAPE_NAME));
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// ABIO log option
	requestLogLevel = LOG_LEVEL_JOB;
	memset(logJobID, 0, sizeof(logJobID));
	logModuleNumber = MODULE_UNKNOWN;
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// ndmp connect infomation
	memset(ndmpAccount, 0, sizeof(ndmpAccount));
	memset(ndmpPasswd, 0, sizeof(ndmpPasswd));;
	memset(ndmpIP, 0, sizeof(ndmpIP));
	memset(ndmpPort, 0, sizeof(ndmpPort));
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// ndmp variables
	memset(&connmoveraddr, 0, sizeof(ndmp_mover_listen_reply));
	
	nMoverPauseState = 0;
	nMoverHaltState = 0;
	nDataHaltState = 0;
	
	nMoverPauseReason = 0;
	nMoverHaltReason = 0;
	nDataHaltReason = 0;
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// ndmp configuration
	tapeDriveTimeOut = 0;
	tapeDriveDelayTime = 0;
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// ip and port number
	memset(masterIP, 0, sizeof(masterIP));
	memset(masterPort, 0, sizeof(masterPort));
	memset(masterNodename, 0, sizeof(masterNodename));
	memset(masterLogPort, 0, sizeof(masterLogPort));
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// ndmp log
	SendNdmpLogToMaster = NULL;
	
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 2. 각 시스템에 맞게 process를 초기화한다.
	
	// set a resource limit
	va_setrlimit();
	
	// set the client working directory
	for (i = (int)strlen(filename) - 1; i > 0; i--)
	{
		if (filename[i] == FILE_PATH_DELIMITER)
		{
			strncpy(ABIOMASTER_CLIENT_FOLDER, filename, i);
			
			break;
		}
	}
	
	// set module number for debug log of ABIO common library
	logModuleNumber = MODULE_NDMP_HTTPD;
	
	// set master server ip and port
	strcpy(masterIP, command.sourceIP);
	strcpy(masterPort, command.sourcePort);
	strcpy(masterNodename, command.sourceNodename);
	strcpy(masterLogPort, commandData.client.masterLogPort);

	// ndmp connect information
	strcpy(ndmpIP, commandData.client.ip);
	strcpy(ndmpPort, commandData.client.port);
	strcpy(ndmpAccount, commandData.ndmp.account);
	strcpy(ndmpPasswd, commandData.ndmp.passwd);
	
	#ifdef __ABIO_WIN
		// windows socket을 open한다.
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) 
		{
			return -1;
		}
	#endif
	
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 3. process 실행에 필요한 설정 값들을 읽어온다.
	
	// get a module configuration
	if (GetModuleConfiguration() < 0)
	{
		return -1;
	}
	
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 4. process 보안 권한 변경을 시도한다.
	
	#ifdef __ABIO_WIN
		if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &token) == TRUE)
		{
			if (SetPrivilege(token, ABIO_NT_PRIVILEGE_SE_BACKUP_NAME, 1) == FALSE)
			{
				return -1;
	  		}
			
	  		if (SetPrivilege(token, ABIO_NT_PRIVILEGE_SE_RESTORE_NAME, 1) == FALSE)
	  		{
				return -1;
	  		}
			
	  		if (SetPrivilege(token, ABIO_NT_PRIVILEGE_SE_SECURITY_NAME, 1) == FALSE)
			{
				return -1;
	  		}
		}
		else
		{
			return -1;
		}
	#endif
	
	
	return 0;
}

int GetModuleConfiguration()
{
	struct confOption * moduleConfig;
	int moduleConfigNumber;
	
	int i;
	

	if (va_load_conf_file(ABIOMASTER_CLIENT_FOLDER, ABIOMASTER_NDMP_CONFIG, &moduleConfig, &moduleConfigNumber) > 0)
	{
		for (i = 0; i < moduleConfigNumber; i++)
		{
			if (!strcmp(moduleConfig[i].optionName, NDMP_FILE_LIST_FOLDER_OPTION_NAME))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(ABIOMASTER_NDMP_FILE_LIST_FOLDER) - 1)
				{
					strcpy(ABIOMASTER_NDMP_FILE_LIST_FOLDER, moduleConfig[i].optionValue);
				}
			}
			else if (!strcmp(moduleConfig[i].optionName, NDMP_LOG_FOLDER_OPTION_NAME))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(ABIOMASTER_NDMP_LOG_FOLDER) - 1)
				{
					strcpy(ABIOMASTER_NDMP_LOG_FOLDER, moduleConfig[i].optionValue);
				}
			}
			else if (!strcmp(moduleConfig[i].optionName, NDMP_TAPE_DRIVE_TIME_OUT_OPTION_NAME))
			{
				tapeDriveTimeOut = atoi(moduleConfig[i].optionValue);
			}
			else if (!strcmp(moduleConfig[i].optionName, NDMP_TAPE_DRIVE_DELAY_TIME_OPTION_NAME))
			{
				tapeDriveDelayTime = atoi(moduleConfig[i].optionValue);
			}
			else if (!strcmp(moduleConfig[i].optionName, DEBUG_LEVEL))
			{
				DebugLevel=atoi(moduleConfig[i].optionValue);
			}
			else if (!strcmp(moduleConfig[i].optionName, NDMP_TAPE_NAME_OPTION_NAME))
			{
				strcpy(ABIOMASTER_NDMP_TAPE_NAME, moduleConfig[i].optionValue);
			}
		}
		
		for (i = 0; i < moduleConfigNumber; i++)
		{
			va_free(moduleConfig[i].optionName);
			va_free(moduleConfig[i].optionValue);
		}
		va_free(moduleConfig);
	}
	else
	{
		return -1;
	}
	
	
	if (tapeDriveTimeOut < 60)
	{
		tapeDriveTimeOut = 60;
	}
	
	
	if (tapeDriveDelayTime == 0)
	{
		tapeDriveDelayTime = 1;
	}
	
	
	return 0;
}

void ResponseRequest()
{
	if(0 < DebugLevel)
	{
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "ResponseRequest() Start");
	}

	if (!strcmp(command.subCommand, "<GET_NDMP_FILE_LIST>"))
	{
		GetNdmpFileList();
	}
	else if (!strcmp(command.subCommand, "<PUT_FILE_LIST>"))
	{
		PutFileList();
	}
	else if (!strcmp(command.subCommand, "<PUT_BLOCK_LIST>"))
	{
		PutBlockList();
	}
	else if (!strcmp(command.subCommand, "<GET_TAPE_LIBRARY_INVENTORY>"))
	{
		GetTapeLibraryInventory();
	}
	else if (!strcmp(command.subCommand, "<MOVE_MEDIUM>"))
	{
		MoveMedium();
	}
	if (!strcmp(command.subCommand, "<WRITE_LABEL>"))
	{
		WriteLabel();
	}
	else if (!strcmp(command.subCommand, "<GET_LABEL>"))
	{
		GetLabel();
	}
	else if (!strcmp(command.subCommand, "<UNLOAD_TAPE_VOLUME>"))
	{
		UnloadTapeVolume();
	}
	else if (!strcmp(command.subCommand, "<GET_TAPE_DRIVE_SERIAL>"))
	{
		GetTapeDriveSerial();
	}
	else if (!strcmp(command.subCommand, "<GET_TAPE_LIBRARY_SERIAL>"))
	{
		GetTapeLibrarySerial();
	}	
	else if (!strcmp(command.subCommand, "<GET_SCSI_INVENTORY>"))
	{
		GetScsiInventory();
	}
	else if (!strcmp(command.subCommand, "<GET_NODE_MODULE>"))
	{
		GetNodeModule();
	}	
	else if (!strcmp(command.subCommand, "<CHECK_DEVICE>"))
	{
		CheckDevice();
	}
	if(0 < DebugLevel)
	{
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "ResponseRequest() End\n");
	}
}

void CheckDevice()
{	
	va_sock_t commandSock;		
	NdmpConnection connection;		
	
	if(0 < DebugLevel)
	{
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "CheckDevice() Start");
	}
	if ((commandSock = va_connect(masterIP, commandData.catalogPort, 1)) != -1)
	{
		if (va_ndmp_connect_ndmp_server(ndmpIP, (unsigned long)atol(ndmpPort), ndmpAccount, ndmpPasswd, &connection) == 0)
		{
			if (va_ndmp_library_open_tape_drive(connection, commandData.mountStatus.device, O_RDWR, 1) == 0)
			{
				if (va_ndmp_library_load_medium(connection) == 0)
				{
					va_send(commandSock, "1", 2, 0, DATA_TYPE_NOT_CHANGE);
				}
				else
				{
					va_send(commandSock, "0", 2, 0, DATA_TYPE_NOT_CHANGE);					
				}				
				va_ndmp_tape_close(connection);
			}
			else
			{
				va_send(commandSock, "0", 2, 0, DATA_TYPE_NOT_CHANGE);
			}
			va_ndmp_connect_close(connection);		
		}
	}
	if(0 < DebugLevel)
	{
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "CheckDevice() End\n");
	}
	va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
}
void GetNdmpFileList()
{
	char reply[DSIZ][DSIZ];
	char msg[DSIZ * DSIZ];
	
	va_sock_t commandSock;
	
	NdmpConnection connection;
	ndmpFileSystemList fslist;
	
	int i;
	
	
	
	// initialize variables
	memset(&fslist, 0, sizeof(ndmpFileSystemList));
	
	if(0 < DebugLevel)
	{
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "GetNdmpFileList() Start");
	}
	
	if(0 < DebugLevel)
	{
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "1. masterIP : %s", masterIP);
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "1. commandData.catalogPort : %s", commandData.catalogPort);

		if ((commandSock = va_connect(masterIP, commandData.catalogPort, 1)) != -1)
		{
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "2. ndmpIP : %s", masterIP);
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "2. (unsigned long)atol(ndmpPort) : %lld", (unsigned long)atol(ndmpPort));
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "2. ndmpAccount : %s", ndmpAccount);
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "2. ndmpPasswd : %s", ndmpPasswd);

			if (va_ndmp_connect_ndmp_server(ndmpIP, (unsigned long)atol(ndmpPort), ndmpAccount, ndmpPasswd, &connection) == 0)
			{
				// nas의 volume 목록을 읽어온다.
				if (va_ndmp_config_get_fs_info(connection, &fslist) == 0)
				{
					for (i = 0; i < fslist.fsnum; i++)
					{
						// #issue148 size가 0 일경우에는 offline으로 판단해서 리스트를 전송하지 않는다.
						if(fslist.fs[i].size != 0)
						{
							// make a reply message
							va_init_reply_buf(reply);
							
							strcpy(reply[0], "NDMP_FILE_LIST");
							strcpy(reply[91], fslist.fs[i].filesystem);
							va_ulltoa(fslist.fs[i].size, reply[96]);
							
							WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "4. reply[0] : NDMP_FILE_LIST");
							WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "4. fslist.fs[i].filesystem : %s", reply[91]);
							WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "4. fslist.fs[i].size : %s", reply[96]);

							va_make_reply_msg(reply, msg);
							if (va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE) < 0)
							{
								break;
							}
						}
					}
					
					va_free(fslist.fs);
				}
				else
				{
					WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "va_ndmp_config_get_fs_info() fail");
				}
				
				
				va_ndmp_connect_close(connection);
			}
			else
			{
				WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "va_ndmp_connect_ndmp_server() fail");
			}
			
			va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
		}
		else
		{
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "va_connect() fail");
		}
	}
	else
	{
		if ((commandSock = va_connect(masterIP, commandData.catalogPort, 1)) != -1)
		{
			if (va_ndmp_connect_ndmp_server(ndmpIP, (unsigned long)atol(ndmpPort), ndmpAccount, ndmpPasswd, &connection) == 0)
			{
				// nas의 volume 목록을 읽어온다.
				if (va_ndmp_config_get_fs_info(connection, &fslist) == 0)
				{
					for (i = 0; i < fslist.fsnum; i++)
					{
						// #issue148 size가 0 일경우에는 offline으로 판단해서 리스트를 전송하지 않는다.
						if(fslist.fs[i].size != 0)
						{
							// make a reply message
							va_init_reply_buf(reply);
							
							strcpy(reply[0], "NDMP_FILE_LIST");
							strcpy(reply[91], fslist.fs[i].filesystem);
							va_ulltoa(fslist.fs[i].size, reply[96]);
							
							va_make_reply_msg(reply, msg);
							if (va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE) < 0)
							{
								break;
							}
						}
					}
					
					va_free(fslist.fs);
				}
				
				
				va_ndmp_connect_close(connection);
			}
			
			va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
		}
	
	}
	if(0 < DebugLevel)
	{
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "GetNdmpFileList() End\n");
	}
}

void PutFileList()
{
	va_sock_t commandSock;
	
	va_fd_t fdFilelist;
	
	char tmpbuf[DATA_BLOCK_SIZE];
	int recvSize;
	
	
	
	if ((commandSock = va_connect(masterIP, commandData.catalogPort, 1)) != -1)
	{
		// server에서 file list를 받는다.
		if ((fdFilelist = va_open(ABIOMASTER_NDMP_FILE_LIST_FOLDER, commandData.filelist, O_CREAT|O_RDWR, 1, 1)) != (va_fd_t)-1)
		{
			while ((recvSize = va_recv(commandSock, tmpbuf, sizeof(tmpbuf), 0, DATA_TYPE_NOT_CHANGE)) > 0)
			{
				if (va_write(fdFilelist, tmpbuf, recvSize, DATA_TYPE_NOT_CHANGE) < 0)
				{
					break;
				}
			}
			
			va_close(fdFilelist);
		}
		
		va_close_socket(commandSock, ABIO_SOCKET_CLOSE_SERVER);
	}
}

void PutBlockList()
{
	va_sock_t commandSock;
	
	va_fd_t fdFilelist;
	
	char tmpbuf[DATA_BLOCK_SIZE];
	int recvSize;
	
	
	
	if ((commandSock = va_connect(masterIP, commandData.catalogPort, 1)) != -1)
	{
		// server에서 file list를 받는다.
		if ((fdFilelist = va_open(ABIOMASTER_NDMP_FILE_LIST_FOLDER, commandData.filelist, O_CREAT|O_RDWR, 1, 1)) != (va_fd_t)-1)
		{
			while ((recvSize = va_recv(commandSock, tmpbuf, sizeof(tmpbuf), 0, DATA_TYPE_NOT_CHANGE)) > 0)
			{
				if (va_write(fdFilelist, tmpbuf, recvSize, DATA_TYPE_NOT_CHANGE) < 0)
				{
					break;
				}
			}
			
			va_close(fdFilelist);
		}
		
		va_close_socket(commandSock, ABIO_SOCKET_CLOSE_SERVER);
	}
}

void GetTapeLibraryInventory()
{
	char reply[DSIZ][DSIZ];
	char msg[DSIZ * DSIZ];
	
	va_sock_t commandSock;
	
	struct tapeInventory inventory;
	int elementNumber;

	NdmpConnection connection;
	
	int i;
	
	if(0 < DebugLevel)
	{
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "GetTapeLibraryInventory() Start");
	}
	if(0 < DebugLevel)
	{
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "1. masterIP : %s", masterIP);
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "1. commandData.catalogPort : %s", commandData.catalogPort);
		if ((commandSock = va_connect(masterIP, commandData.catalogPort, 1)) != -1)
		{
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "2. ndmpIP : %s", masterIP);
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "2. (unsigned long)atol(ndmpPort) : %lld", (unsigned long)atol(ndmpPort));
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "2. ndmpAccount : %s", ndmpAccount);
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "2. ndmpPasswd : %s", ndmpPasswd);

			if (va_ndmp_connect_ndmp_server(ndmpIP, (unsigned long)atol(ndmpPort), ndmpAccount, ndmpPasswd, &connection) == 0)
			{
				// tape library의 inventory 정보를 읽어온다.
				memset(&inventory, 0, sizeof(struct tapeInventory));
				
				
				if ((elementNumber = va_ndmp_library_make_inventory(connection, commandData.mountStatus.device, ELEMENT_ALL, &inventory, 1)) > 0)
				{
					WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "3. elementNumber : %d", elementNumber);
					WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "3. inventory.robotNumber : %d", inventory.robotNumber);

					for (i = 0; i < inventory.robotNumber; i++)
					{
						// make a reply message
						va_init_reply_buf(reply);
						
						strcpy(reply[0], "TAPE_LIBRARY_INVENTORY");
						strcpy(reply[280], "1");
						va_itoa(inventory.robot[i].address, reply[281]);
						va_itoa(inventory.robot[i].full, reply[282]);
						strcpy(reply[283], inventory.robot[i].volume);
						

						WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "4. reply[0] : TAPE_LIBRARY_INVENTORY");
						WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "4. reply[280] : 1");
						WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "4. inventory.robot[i].address : %s", reply[281]);
						WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "4. inventory.robot[i].full : %s", reply[282]);
						WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "4. inventory.robot[i].volume : %s", reply[283]);

						va_make_reply_msg(reply, msg);
						if (va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE) < 0)
						{
							break;
						}
					}
					WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "5. inventory.storageNumber : %d", inventory.storageNumber);
					for (i = 0; i < inventory.storageNumber; i++)
					{
						// make a reply message
						va_init_reply_buf(reply);
						
						strcpy(reply[0], "TAPE_LIBRARY_INVENTORY");
						strcpy(reply[280], "2");
						va_itoa(inventory.storage[i].address, reply[281]);
						va_itoa(inventory.storage[i].full, reply[282]);
						strcpy(reply[283], inventory.storage[i].volume);
						
						WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "6. reply[0] : TAPE_LIBRARY_INVENTORY");
						WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "6. reply[280] : 2");
						WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "6. inventory.storage[i].address : %s", reply[281]);
						WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "6. inventory.storage[i].full : %s", reply[282]);
						WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "6. inventory.storage[i].volume : %s", reply[283]);

						va_make_reply_msg(reply, msg);
						if (va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE) < 0)
						{
							break;
						}
					}
					WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "7. inventory.ioNumber : %d", inventory.ioNumber);
					for (i = 0; i < inventory.ioNumber; i++)
					{
						// make a reply message
						va_init_reply_buf(reply);
						
						strcpy(reply[0], "TAPE_LIBRARY_INVENTORY");
						strcpy(reply[280], "3");
						va_itoa(inventory.io[i].address, reply[281]);
						va_itoa(inventory.io[i].full, reply[282]);
						strcpy(reply[283], inventory.io[i].volume);
						
						WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "8. reply[0] : TAPE_LIBRARY_INVENTORY");
						WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "8. reply[280] : 3");
						WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "8. inventory.io[i].address : %s", reply[281]);
						WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "8. inventory.io[i].full : %s", reply[282]);
						WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "8. inventory.io[i].volume : %s", reply[283]);

						va_make_reply_msg(reply, msg);
						if (va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE) < 0)
						{
							break;
						}
					}
					WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "9. inventory.driveNumber : %d", inventory.driveNumber);
					for (i = 0; i < inventory.driveNumber; i++)
					{
						// make a reply message
						va_init_reply_buf(reply);
						
						strcpy(reply[0], "TAPE_LIBRARY_INVENTORY");
						strcpy(reply[280], "4");
						va_itoa(inventory.drive[i].address, reply[281]);
						va_itoa(inventory.drive[i].full, reply[282]);
						strcpy(reply[283], inventory.drive[i].volume);
						strcpy(reply[284], inventory.drive[i].serial);
						
						WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "10. reply[0] : TAPE_LIBRARY_INVENTORY");
						WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "10. reply[280] : 4");
						WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "10. inventory.drive[i].address : %s", reply[281]);
						WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "10. inventory.drive[i].full : %s", reply[282]);
						WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "10. inventory.drive[i].volume : %s", reply[283]);
						WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "10. inventory.drive[i].serial : %s", reply[284]);

						va_make_reply_msg(reply, msg);
						if (va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE) < 0)
						{
							break;
						}
					}
					
					va_free_inventory(&inventory);
				}
				// tape library의 inventory 정보를 읽어오지 못한 경우 element type을 0으로 넘겨서 inventory 정보를 읽어오지 못했다는 것을 서버에서 알 수 있도록 한다.
				else if (elementNumber < 0)
				{
					// make a reply message
					va_init_reply_buf(reply);
					
					strcpy(reply[0], "TAPE_LIBRARY_INVENTORY");
					strcpy(reply[280], "0");
					WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "11. elementNumber : %d", elementNumber);
					WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "11. reply[0] : TAPE_LIBRARY_INVENTORY");
					WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "11. reply[280] : 0");
					
					va_make_reply_msg(reply, msg);
					va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE);
				}
				
				va_ndmp_connect_close(connection);
			}
			else
			{
				WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "va_ndmp_connect_ndmp_server() fail");
			}
			
			va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
		}
		else
		{
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "va_connect() fail");
		}
	}
	else
	{
		if ((commandSock = va_connect(masterIP, commandData.catalogPort, 1)) != -1)
		{
			if (va_ndmp_connect_ndmp_server(ndmpIP, (unsigned long)atol(ndmpPort), ndmpAccount, ndmpPasswd, &connection) == 0)
			{
				// tape library의 inventory 정보를 읽어온다.
				memset(&inventory, 0, sizeof(struct tapeInventory));
				if ((elementNumber = va_ndmp_library_make_inventory(connection, commandData.mountStatus.device, ELEMENT_ALL, &inventory, 1)) > 0)
				{
					for (i = 0; i < inventory.robotNumber; i++)
					{
						// make a reply message
						va_init_reply_buf(reply);
						
						strcpy(reply[0], "TAPE_LIBRARY_INVENTORY");
						strcpy(reply[280], "1");
						va_itoa(inventory.robot[i].address, reply[281]);
						va_itoa(inventory.robot[i].full, reply[282]);
						strcpy(reply[283], inventory.robot[i].volume);
						
						va_make_reply_msg(reply, msg);
						if (va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE) < 0)
						{
							break;
						}
					}
					
					for (i = 0; i < inventory.storageNumber; i++)
					{
						// make a reply message
						va_init_reply_buf(reply);
						
						strcpy(reply[0], "TAPE_LIBRARY_INVENTORY");
						strcpy(reply[280], "2");
						va_itoa(inventory.storage[i].address, reply[281]);
						va_itoa(inventory.storage[i].full, reply[282]);
						strcpy(reply[283], inventory.storage[i].volume);
						
						va_make_reply_msg(reply, msg);
						if (va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE) < 0)
						{
							break;
						}
					}
					
					for (i = 0; i < inventory.ioNumber; i++)
					{
						// make a reply message
						va_init_reply_buf(reply);
						
						strcpy(reply[0], "TAPE_LIBRARY_INVENTORY");
						strcpy(reply[280], "3");
						va_itoa(inventory.io[i].address, reply[281]);
						va_itoa(inventory.io[i].full, reply[282]);
						strcpy(reply[283], inventory.io[i].volume);
						
						va_make_reply_msg(reply, msg);
						if (va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE) < 0)
						{
							break;
						}
					}
					
					for (i = 0; i < inventory.driveNumber; i++)
					{
						// make a reply message
						va_init_reply_buf(reply);
						
						strcpy(reply[0], "TAPE_LIBRARY_INVENTORY");
						strcpy(reply[280], "4");
						va_itoa(inventory.drive[i].address, reply[281]);
						va_itoa(inventory.drive[i].full, reply[282]);
						strcpy(reply[283], inventory.drive[i].volume);
						strcpy(reply[284], inventory.drive[i].serial);
						
						va_make_reply_msg(reply, msg);
						if (va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE) < 0)
						{
							break;
						}
					}
					
					va_free_inventory(&inventory);
				}
				// tape library의 inventory 정보를 읽어오지 못한 경우 element type을 0으로 넘겨서 inventory 정보를 읽어오지 못했다는 것을 서버에서 알 수 있도록 한다.
				else if (elementNumber < 0)
				{
					// make a reply message
					va_init_reply_buf(reply);
					
					strcpy(reply[0], "TAPE_LIBRARY_INVENTORY");
					strcpy(reply[280], "0");
					
					va_make_reply_msg(reply, msg);
					va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE);
				}
				
				va_ndmp_connect_close(connection);
			}
			
			va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
		}
	}
	

	if(0 < DebugLevel)
	{
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "GetTapeLibraryInventory() End\n");
	}
}

void MoveMedium()
{
	va_sock_t commandSock;
	
	struct tapeInventory inventory;
	
	int sourceElementNumber;
	struct tapeElement * sourceElement;
	int sourceStatus;
	
	int destinationElementNumber;
	struct tapeElement * destinationElement;
	int destinationStatus;

	NdmpConnection connection;
	
	int i;
	int r;
	
	
	
	// initialize variables
	sourceStatus = -1;
	destinationStatus = -1;
	
	
	if ((commandSock = va_connect(masterIP, commandData.catalogPort, 1)) != -1)
	{
		if ((r = va_ndmp_connect_ndmp_server(ndmpIP, (unsigned long)atol(ndmpPort), ndmpAccount, ndmpPasswd, &connection)) == 0)
		{
			// source slot에서 destination slot으로 volume을 이동한다.
			if ((r = va_ndmp_library_move_medium(connection, commandData.mountStatus.device, commandData.mountStatus.volumeAddress, commandData.mountStatus.driveAddress)) == 0)
			{
				// volume 이동 결과를 확인하기 위해 tape library의 inventory를 읽어온다.
				memset(&inventory, 0, sizeof(struct tapeInventory));
				if ((r = va_ndmp_library_make_inventory(connection, commandData.mountStatus.device, ELEMENT_ALL, &inventory, 0)) > 0)
				{
					// volume 이동 결과를 확인하기 위해 source slot의 상태를 확인한다.
					if (commandData.mountStatus.mtype == ELEMENT_ROBOT)
					{
						sourceElementNumber = inventory.robotNumber;
						sourceElement = inventory.robot;
					}
					else if (commandData.mountStatus.mtype == ELEMENT_STORAGE)
					{
						sourceElementNumber = inventory.storageNumber;
						sourceElement = inventory.storage;
					}
					else if (commandData.mountStatus.mtype == ELEMENT_IO)
					{
						sourceElementNumber = inventory.ioNumber;
						sourceElement = inventory.io;
					}
					else if (commandData.mountStatus.mtype == ELEMENT_DRIVE)
					{
						sourceElementNumber = inventory.driveNumber;
						sourceElement = inventory.drive;
					}
					
					for (i = 0; i < sourceElementNumber; i++)
					{
						if (sourceElement[i].address == commandData.mountStatus.volumeAddress)
						{
							sourceStatus = sourceElement[i].full;
							
							break;
						}
					}
					
					// volume 이동 결과를 확인하기 위해 destination slot의 상태를 확인한다.
					if (commandData.mountStatus.mtype2 == ELEMENT_ROBOT)
					{
						destinationElementNumber = inventory.robotNumber;
						destinationElement = inventory.robot;
					}
					else if (commandData.mountStatus.mtype2 == ELEMENT_STORAGE)
					{
						destinationElementNumber = inventory.storageNumber;
						destinationElement = inventory.storage;
					}
					else if (commandData.mountStatus.mtype2 == ELEMENT_IO)
					{
						destinationElementNumber = inventory.ioNumber;
						destinationElement = inventory.io;
					}
					else if (commandData.mountStatus.mtype2 == ELEMENT_DRIVE)
					{
						destinationElementNumber = inventory.driveNumber;
						destinationElement = inventory.drive;
					}
					
					for (i = 0; i < destinationElementNumber; i++)
					{
						if (destinationElement[i].address == commandData.mountStatus.driveAddress)
						{
							destinationStatus = destinationElement[i].full;
							
							break;
						}
					}
					
					
					// volume이 storage slot에서 tape drive로 이동하지 못한 경우 결과를 저장한다.
					if (sourceStatus != 0 || destinationStatus != 1)
					{
						va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_MoveMedium, ERROR_STORAGE_MOVE_MEDIUM, &commandData);
					}
					
					va_free_inventory(&inventory);
				}
				// volume 이동 결과를 확인하지 못한 경우 결과를 저장한다.
				else
				{
					if (r == 0)
					{
						va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_MoveMedium, ERROR_STORAGE_NO_ELEMENT, &commandData);
					}
					else if (r == -1)
					{
						va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_MoveMedium, ERROR_STORAGE_OPEN_TAPE_LIBRARY, &commandData);
					}
					else if (r == -2)
					{
						va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_MoveMedium, ERROR_STORAGE_CHECK_BUS_TYPE, &commandData);
					}
					else if (r == -3)
					{
						va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_MoveMedium, ERROR_STORAGE_READ_ELEMENT, &commandData);
					}
					else if (r == -4)
					{
						va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_MoveMedium, ERROR_STORAGE_READ_ELEMENT, &commandData);
					}
					else if (r == -6)
					{
						va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_MoveMedium, ERROR_STORAGE_READ_ELEMENT, &commandData);
					}
					else
					{
						va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_MoveMedium, ERROR_STORAGE_MOVE_MEDIUM, &commandData);
					}
				}
			}
			else
			{
				if (r == -1)
				{
					va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_MoveMedium, ERROR_STORAGE_OPEN_TAPE_LIBRARY, &commandData);
				}
				else if (r == -2)
				{
					va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_MoveMedium, ERROR_STORAGE_CHECK_BUS_TYPE, &commandData);
				}
				else if (r == -3)
				{
					va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_MoveMedium, ERROR_STORAGE_READ_ELEMENT, &commandData);
				}
				else if (r == -4)
				{
					va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_MoveMedium, ERROR_STORAGE_NO_ELEMENT, &commandData);
				}
				else if (r == -5)
				{
					va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_MoveMedium, ERROR_STORAGE_SOURCE_ELEMENT_EMPTY, &commandData);
				}
				else if (r == -6)
				{
					va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_MoveMedium, ERROR_STORAGE_DESTINATION_ELEMENT_FULL, &commandData);
				}
				else
				{
					va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_MoveMedium, ERROR_STORAGE_MOVE_MEDIUM, &commandData);
				}
			}
			
			va_ndmp_connect_close(connection);
		}
		else
		{
			if (r == -1)
			{
				va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_MoveMedium, ERROR_NDMP_CONNECT_RPC, &commandData);
			}
			else if (r == -2)
			{
				va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_MoveMedium, ERROR_NDMP_CONNECT_CONNECTION, &commandData);
			}
			else if (r == -3)
			{
				va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_MoveMedium, ERROR_NDMP_CONNECT_NEGOTIATION, &commandData);
			}
			else if (r == -4)
			{
				va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_MoveMedium, ERROR_NDMP_CONNECT_AUTHENTICATION, &commandData);
			}
			else
			{
				va_write_error_code(ERROR_LEVEL_ERROR, MODULE_CLIENT_HTTPD, FUNCTION_MoveMedium, ERROR_NDMP_CONNECT, &commandData);
			}
		}
		
		// volume 이동 결과를 전달한다.
		va_send(commandSock, &commandData, sizeof(struct ckBackup), 0, DATA_TYPE_CK_BACKUP);
		va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
	}
}

void WriteLabel()
{
	va_sock_t commandSock;
	
	struct volumeDB volumeDB;
	char * tapeHeader;

	NdmpConnection connection;

	int volumeBlockSize;
	int dataBufferSize;
	
	int r;
	
	
	
	// set a media block size and data buffer size
	volumeBlockSize = commandData.mountStatus.blockSize;
	dataBufferSize = volumeBlockSize - DSIZ;
	
	tapeHeader = (char *)malloc(sizeof(char) * volumeBlockSize);
	memset(tapeHeader, 0, sizeof(char) * volumeBlockSize);
	
	
	if ((commandSock = va_connect(masterIP, commandData.catalogPort, 1)) != -1)
	{
		if ((r = va_ndmp_connect_ndmp_server(ndmpIP, (unsigned long)atol(ndmpPort), ndmpAccount, ndmpPasswd, &connection)) == 0)
		{
			// open the tape drive
			if (va_ndmp_library_open_tape_drive(connection, commandData.mountStatus.device, O_RDWR, 1) == 0)
			{
				// load the tape media
				if (va_ndmp_library_load_medium(connection) == 0)
				{
					// make a tape header
					memset(&volumeDB, 0, sizeof(struct volumeDB));
					volumeDB.version = CURRENT_VERSION_INTERNAL;
					strcpy(volumeDB.volume, commandData.mountStatus.volume);
					
					// write a media header to the media
					va_change_byte_order_volumeDB(&volumeDB);		// host to network
					memcpy(tapeHeader, &volumeDB, sizeof(struct volumeDB));
					va_change_byte_order_volumeDB(&volumeDB);		// network to host
					
					if (va_ndmp_library_write(connection, tapeHeader, volumeBlockSize, DATA_TYPE_NOT_CHANGE) == volumeBlockSize)
					{
						if (va_ndmp_library_write_filemark(connection, 1) != 0)
						{
							va_write_error_code(ERROR_LEVEL_ERROR, MODULE_NDMP_HTTPD, FUNCTION_WriteLabel, ERROR_VOLUME_WRITE_FILEMARK, &commandData);
						}
					}
					else
					{
						va_write_error_code(ERROR_LEVEL_ERROR, MODULE_NDMP_HTTPD, FUNCTION_WriteLabel, ERROR_VOLUME_WRITE_DATA, &commandData);
					}
					
					// unload the tape media
					if (commandData.mountStatus.storageType == STORAGE_TAPE_LIBRARY)
					{
						va_ndmp_library_unload_medium(connection);
					}
					else
					{
						va_ndmp_library_rewind_tape(connection);
					}
				}
				else
				{
					va_write_error_code(ERROR_LEVEL_ERROR, MODULE_NDMP_HTTPD, FUNCTION_WriteLabel, ERROR_VOLUME_LOAD_VOLUME, &commandData);
				}
				
				va_ndmp_tape_close(connection);
			}
			else
			{
				va_write_error_code(ERROR_LEVEL_ERROR, MODULE_NDMP_HTTPD, FUNCTION_WriteLabel, ERROR_STORAGE_OPEN_TAPE_DRIVE, &commandData);
			}
			
			va_ndmp_connect_close(connection);
		}
		else
		{
			if (r == -1)
			{
				va_write_error_code(ERROR_LEVEL_ERROR, MODULE_NDMP_HTTPD, FUNCTION_WriteLabel, ERROR_NDMP_CONNECT_RPC, &commandData);
			}
			else if (r == -2)
			{
				va_write_error_code(ERROR_LEVEL_ERROR, MODULE_NDMP_HTTPD, FUNCTION_WriteLabel, ERROR_NDMP_CONNECT_CONNECTION, &commandData);
			}
			else if (r == -3)
			{
				va_write_error_code(ERROR_LEVEL_ERROR, MODULE_NDMP_HTTPD, FUNCTION_WriteLabel, ERROR_NDMP_CONNECT_NEGOTIATION, &commandData);
			}
			else if (r == -4)
			{
				va_write_error_code(ERROR_LEVEL_ERROR, MODULE_NDMP_HTTPD, FUNCTION_WriteLabel, ERROR_NDMP_CONNECT_AUTHENTICATION, &commandData);
			}
			else
			{
				va_write_error_code(ERROR_LEVEL_ERROR, MODULE_NDMP_HTTPD, FUNCTION_WriteLabel, ERROR_NDMP_CONNECT, &commandData);
			}
		}
		
		
		va_send(commandSock, &commandData, sizeof(struct ckBackup), 0, DATA_TYPE_CK_BACKUP);
		va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
	}
	
	
	va_free(tapeHeader);
}

void GetLabel()
{
	va_sock_t commandSock;
	
	struct volumeDB volumeDB;
	char * tapeHeader;

	NdmpConnection connection;

	int volumeBlockSize;
	int dataBufferSize;
	
	
	
	// set a media block size and data buffer size
	volumeBlockSize = commandData.mountStatus.blockSize;
	dataBufferSize = volumeBlockSize - DSIZ;
	
	tapeHeader = (char *)malloc(sizeof(char) * volumeBlockSize);
	memset(tapeHeader, 0, sizeof(char) * volumeBlockSize);
	
	if(0 < DebugLevel)
	{
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "GetLabel() Start");
	}

	if(0 < DebugLevel)
	{
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "1. masterIP : %s", masterIP);
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "1. commandData.catalogPort : %s", commandData.catalogPort);
		if ((commandSock = va_connect(masterIP, commandData.catalogPort, 1)) != -1)
		{
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "2. ndmpIP : %s", masterIP);
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "2. (unsigned long)atol(ndmpPort) : %lld", (unsigned long)atol(ndmpPort));
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "2. ndmpAccount : %s", ndmpAccount);
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "2. ndmpPasswd : %s", ndmpPasswd);
			if (va_ndmp_connect_ndmp_server(ndmpIP, (unsigned long)atol(ndmpPort), ndmpAccount, ndmpPasswd, &connection) == 0)
			{
				WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "3. commandData.mountStatus.device : %s", commandData.mountStatus.device);
				// open the tape drive
				if (va_ndmp_library_open_tape_drive(connection, commandData.mountStatus.device, O_RDWR, 1) == 0)
				{	
					// load the tape media
					if (va_ndmp_library_load_medium(connection) == 0)
					{
						WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "4. volumeBlockSize : %d", volumeBlockSize);
						// set a tape block size
						if (va_ndmp_library_set_tape_block_size(connection, volumeBlockSize) == 0)
						{
							commandData.success = 1;
							
							WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "5. tapeHeader : %s, volumeBlockSize : %d,  DATA_TYPE_NOT_CHANGE : 0", tapeHeader, volumeBlockSize);
							// media에서 media header를 읽어온다.
							if (va_ndmp_library_read(connection, tapeHeader, volumeBlockSize, DATA_TYPE_NOT_CHANGE) == volumeBlockSize)
							{
								// media header에 있는 label 정보를 읽는다.
								memcpy(&volumeDB, tapeHeader, sizeof(struct volumeDB));
								va_change_byte_order_volumeDB(&volumeDB);		// network to host
								
								WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "6. volumeDB.volume : %s", volumeDB.volume);
								if (va_is_abio_volume(&volumeDB) == 1)
								{
									strcpy(commandData.mountStatus.volume, volumeDB.volume);
								}
								else
								{
									WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "va_is_abio_volume() fail");
								}
							}
							else
							{
								WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "va_ndmp_library_read() fail");
							}
						}
						else
						{
							WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "va_ndmp_library_set_tape_block_size() fail");
						}
						
						WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "7. storageType : %d", commandData.mountStatus.storageType);

						// unload the tape media
						if (commandData.mountStatus.storageType == STORAGE_TAPE_LIBRARY)
						{
							va_ndmp_library_unload_medium(connection);
						}
						else
						{
							va_ndmp_library_rewind_tape(connection);
						}
					}
					else
					{
						WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "va_ndmp_library_load_medium() fail");
					}

					va_ndmp_tape_close(connection);
				}
				else
				{
					WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "va_ndmp_library_open_tape_drive() fail");
				}
				
				va_ndmp_connect_close(connection);
			}
			else
			{
				WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "va_ndmp_connect_ndmp_server() fail");
			}
			
			
			va_send(commandSock, &commandData, sizeof(struct ckBackup), 0, DATA_TYPE_CK_BACKUP);
			va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
		}
		else
		{
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "va_connect() fail");
		}
	}
	else
	{
		if ((commandSock = va_connect(masterIP, commandData.catalogPort, 1)) != -1)
		{
			if (va_ndmp_connect_ndmp_server(ndmpIP, (unsigned long)atol(ndmpPort), ndmpAccount, ndmpPasswd, &connection) == 0)
			{
				// open the tape drive
				if (va_ndmp_library_open_tape_drive(connection, commandData.mountStatus.device, O_RDWR, 1) == 0)
				{
					// load the tape media
					if (va_ndmp_library_load_medium(connection) == 0)
					{
						// set a tape block size
						if (va_ndmp_library_set_tape_block_size(connection, volumeBlockSize) == 0)
						{
							commandData.success = 1;
							
							// media에서 media header를 읽어온다.
							if (va_ndmp_library_read(connection, tapeHeader, volumeBlockSize, DATA_TYPE_NOT_CHANGE) == volumeBlockSize)
							{
								// media header에 있는 label 정보를 읽는다.
								memcpy(&volumeDB, tapeHeader, sizeof(struct volumeDB));
								va_change_byte_order_volumeDB(&volumeDB);		// network to host
								
								if (va_is_abio_volume(&volumeDB) == 1)
								{
									strcpy(commandData.mountStatus.volume, volumeDB.volume);
								}
							}
						}
						
						// unload the tape media
						if (commandData.mountStatus.storageType == STORAGE_TAPE_LIBRARY)
						{
							va_ndmp_library_unload_medium(connection);
						}
						else
						{
							va_ndmp_library_rewind_tape(connection);
						}
					}

					va_ndmp_tape_close(connection);
				}
				
				va_ndmp_connect_close(connection);
			}
			
			
			va_send(commandSock, &commandData, sizeof(struct ckBackup), 0, DATA_TYPE_CK_BACKUP);
			va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
		}
	}
	
	
	if(0 < DebugLevel)
	{
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "GetLabel() End\n");
	}
	va_free(tapeHeader);
}

void UnloadTapeVolume()
{
	va_sock_t commandSock;
	
	NdmpConnection connection;
	
	
	
	if ((commandSock = va_connect(masterIP, commandData.catalogPort, 1)) != -1)
	{
		if (va_ndmp_connect_ndmp_server(ndmpIP, (unsigned long)atol(ndmpPort), ndmpAccount, ndmpPasswd, &connection) == 0)
		{
			// open drive
			if (va_ndmp_library_open_tape_drive(connection, commandData.mountStatus.device, O_RDWR, 1) == 0)
			{
				// unload the tape media
				if (va_ndmp_library_unload_medium(connection) == 0)
				{
					commandData.success = 1;
				}
				else
				{
					commandData.success = 0;
				}
				
				va_ndmp_tape_close(connection);
			}
			else
			{
				commandData.success = 0;
			}
			
			va_ndmp_connect_close(connection);
		}
		
		
		va_send(commandSock, &commandData, sizeof(struct ckBackup), 0, DATA_TYPE_CK_BACKUP);
		va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
	}
}

void GetScsiInventory()
{
	va_sock_t commandSock;
		
	if( 0 < DebugLevel )
	{
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER,ABIOMASTER_NDMP_DEBUG,DEBUG_CODE_DETAIL " Get Scsi Inventory(%s) Start",MODULE_NAME_NDMP_HTTPD,__FUNCTION__,commandData.mountStatus.device);
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "GetScsiInventory() Start");
	}

	if( 0 < DebugLevel )
	{
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER,ABIOMASTER_NDMP_DEBUG,DEBUG_CODE_DETAIL "ABIO doesn't support this OS",MODULE_NAME_NDMP_HTTPD,__FUNCTION__);
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "1. masterIP : %s", masterIP);
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "1. commandData.catalogPort : %s", commandData.catalogPort);
	}

	if ((commandSock = va_connect(masterIP, commandData.catalogPort, 1)) != -1)
	{
		/* ABIO doesn't support a tape device operation on another system */
		va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
	}

	if( 0 < DebugLevel )
	{
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER,ABIOMASTER_NDMP_DEBUG,DEBUG_CODE_DETAIL " Get Scsi Inventory End",MODULE_NAME_NDMP_HTTPD,__FUNCTION__);	
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "GetScsiInventory() End\n");
	}
}

void GetTapeLibrarySerial()
{
	char reply[DSIZ][DSIZ];
	char msg[DSIZ * DSIZ];
	
	va_sock_t commandSock;
	
	struct tapeInventory inventory;
	int elementNumber;

	NdmpConnection connection;

	if(0 < DebugLevel)
	{
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "GetTapeLibrarySerial() Start");
	}
	if(0 < DebugLevel)
	{
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "1. masterIP : %s", masterIP);
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "1. commandData.catalogPort : %s", commandData.catalogPort);
		if ((commandSock = va_connect(masterIP, commandData.catalogPort, 1)) != -1)
		{
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "2. ndmpIP : %s", masterIP);
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "2. (unsigned long)atol(ndmpPort) : %lld", (unsigned long)atol(ndmpPort));
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "2. ndmpAccount : %s", ndmpAccount);
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "2. ndmpPasswd : %s", ndmpPasswd);

			if (va_ndmp_connect_ndmp_server(ndmpIP, (unsigned long)atol(ndmpPort), ndmpAccount, ndmpPasswd, &connection) == 0)
			{
				// tape library의 inventory 정보를 읽어온다.
				memset(&inventory, 0, sizeof(struct tapeInventory));
				if ((elementNumber = va_ndmp_library_make_inventory(connection, commandData.mountStatus.device, ELEMENT_ALL, &inventory, 1)) > 0)
				{				
					WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "3. elementNumber : %d", elementNumber);
					// make a reply message
					va_init_reply_buf(reply);
					
					strcpy(reply[0], "TAPE_LIBRARY_SERIAL");
					strcpy(reply[280], "1");		
					strcpy(reply[269], inventory.product);
					strcpy(reply[251], inventory.serial);
					
					WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "4. reply[0] : TAPE_LIBRARY_SERIAL");
					WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "4. reply[280] : 1");
					WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "4. inventory.product : %s", reply[269]);
					WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "4. inventory.serial : %s", reply[251]);
					
					va_make_reply_msg(reply, msg);
					if (va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE) < 0)
					{
						
					}			
					
					va_free_inventory(&inventory);
				}
				// tape library의 inventory 정보를 읽어오지 못한 경우 element type을 0으로 넘겨서 inventory 정보를 읽어오지 못했다는 것을 서버에서 알 수 있도록 한다.
				else if (elementNumber < 0)
				{
					WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "5. elementNumber : %d", elementNumber);
					// make a reply message
					va_init_reply_buf(reply);
					
					strcpy(reply[0], "TAPE_LIBRARY_SERIAL");
					strcpy(reply[280], "0");
					
					WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "6. reply[0] : TAPE_LIBRARY_SERIAL");
					WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "6. reply[280] : 0");

					va_make_reply_msg(reply, msg);
					va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE);
				}
				
				va_ndmp_connect_close(connection);
			}
			else
			{
				WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "va_ndmp_connect_ndmp_server() fail");
			}
			
			va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
		}
		else
		{
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "va_connect() fail");
		}
	}
	else
	{
		if ((commandSock = va_connect(masterIP, commandData.catalogPort, 1)) != -1)
		{
			if (va_ndmp_connect_ndmp_server(ndmpIP, (unsigned long)atol(ndmpPort), ndmpAccount, ndmpPasswd, &connection) == 0)
			{
				// tape library의 inventory 정보를 읽어온다.
				memset(&inventory, 0, sizeof(struct tapeInventory));
				if ((elementNumber = va_ndmp_library_make_inventory(connection, commandData.mountStatus.device, ELEMENT_ALL, &inventory, 1)) > 0)
				{				
					// make a reply message
					va_init_reply_buf(reply);
					
					strcpy(reply[0], "TAPE_LIBRARY_SERIAL");
					strcpy(reply[280], "1");		
					strcpy(reply[269], inventory.product);
					strcpy(reply[251], inventory.serial);
					
					va_make_reply_msg(reply, msg);
					if (va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE) < 0)
					{
						
					}			
					
					va_free_inventory(&inventory);
				}
				// tape library의 inventory 정보를 읽어오지 못한 경우 element type을 0으로 넘겨서 inventory 정보를 읽어오지 못했다는 것을 서버에서 알 수 있도록 한다.
				else if (elementNumber < 0)
				{
					// make a reply message
					va_init_reply_buf(reply);
					
					strcpy(reply[0], "TAPE_LIBRARY_SERIAL");
					strcpy(reply[280], "0");
					
					va_make_reply_msg(reply, msg);
					va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE);
				}
				
				va_ndmp_connect_close(connection);
			}
			
			va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
		}
	}
	

	if(0 < DebugLevel)
	{
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "GetTapeLibrarySerial() End\n");
	}
}

void GetTapeDriveSerial()
{
	char msg[DSIZ * DSIZ];
	char reply[DSIZ][DSIZ];
	char ndmp_device_name[NDMP_DEVICE_NAME] = "ta";

	va_sock_t commandSock;

	int serialNumber;
	char ** serialList;
	char ** deviceFileList;
	
	int i;

	NdmpConnection connection;
	ndmp_config_get_tape_info_reply* ndmp_reply = 0;

	int r;
	ndmp_device_info* info;
	ndmp_device_capability* cap;
	ndmp_pval* pval;

	unsigned long j;
	unsigned long k;
	unsigned long l;
	
	//haesung
	//////////////////////////////////////
	/*char vendor[9];
	char product[17];
	char firmware[5];*/
	char ** vendorList;
	char ** productList;
	char ** firmwareList;		

	vendorList = NULL;
	productList = NULL;
	firmwareList = NULL;	
	//////////////////////////////////////
	
	serialNumber = 0;
	serialList = NULL;
	deviceFileList = NULL;
	
	tapeDriveTimeOut = 0;
	tapeDriveDelayTime = 0;
	
	if( 0 < DebugLevel )
	{
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER,ABIOMASTER_NDMP_DEBUG,DEBUG_CODE_DETAIL " Get Drive Serial Start",MODULE_NAME_NDMP_HTTPD,__FUNCTION__);	
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, " GetTapeLibrarySerial() Start");	
	}
	if(0 < DebugLevel)
	{
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "1. masterIP : %s", masterIP);
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "1. commandData.catalogPort : %s", commandData.catalogPort);
	}
	if ((commandSock = va_connect(masterIP, commandData.catalogPort, 1)) != -1)
	{
		if(0 < DebugLevel)
		{
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "2. ndmpIP : %s", masterIP);
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "2. (unsigned long)atol(ndmpPort) : %lld", (unsigned long)atol(ndmpPort));
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "2. ndmpAccount : %s", ndmpAccount);
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "2. ndmpPasswd : %s", ndmpPasswd);
		}
		if (va_ndmp_connect_ndmp_server(ndmpIP, (unsigned long)atol(ndmpPort), ndmpAccount, ndmpPasswd, &connection) == 0)
		{
			if ((r = va_ndmp_comm_send_request(connection, NDMP_CONFIG_GET_TAPE_INFO, NDMP_NO_ERR, 0, (void **)&ndmp_reply)) == 0)
			{
				if (ndmp_reply != NULL && ndmp_reply->error == NDMP_NO_ERR)
				{
					if( 0 < DebugLevel )
					{
						WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "3. ndmp_reply->tape_info.tape_info_len : %lld", ndmp_reply->tape_info.tape_info_len);
					}
					for (l = 0; l < ndmp_reply->tape_info.tape_info_len; l++)
					{
						info = &ndmp_reply->tape_info.tape_info_val[l];
						if( 0 < DebugLevel )
						{
							WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "4. info->caplist.caplist_len : %lld", info->caplist.caplist_len);
						}
						for (j = 0; j < info->caplist.caplist_len; j++)
						{
							cap = &info->caplist.caplist_val[j];
							
							if( 0 < DebugLevel )
							{
								WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER,ABIOMASTER_NDMP_DEBUG,DEBUG_CODE_DETAIL " Check device : %s",MODULE_NAME_NDMP_HTTPD,__FUNCTION__,cap->device);		
								WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "5. cap->device : %s",cap->device);	
								WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "5. cap->attr : %lld", cap->attr);	
								WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "5. cap->capability.capability_len : %lld",cap->capability.capability_len);
								WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "5. cap->capability.capability_val->name : %s",cap->capability.capability_val->name);
								WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "5. cap->capability.capability_val->value : %s",cap->capability.capability_val->value);
								WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "**. tape_name : %s", ABIOMASTER_NDMP_TAPE_NAME);
							}
							
							if ((strncmp(cap->device, "nr", 2) == 0 && cap->device[strlen(cap->device) - 1] == 'a' && isalnumstr(cap->device) == 1) || 
								(strlen(ABIOMASTER_NDMP_TAPE_NAME) > 0 && strncmp(cap->device, ABIOMASTER_NDMP_TAPE_NAME, strlen(ABIOMASTER_NDMP_TAPE_NAME)) == 0))
							{						
								if( 0 < DebugLevel )
								{
									WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER,ABIOMASTER_NDMP_DEBUG,DEBUG_CODE_DETAIL " Get device info(nr) : %s",MODULE_NAME_NDMP_HTTPD,__FUNCTION__,cap->device);		
								}
								
								for (k = 0; k < cap->capability.capability_len; k++)
								{
									pval = &cap->capability.capability_val[k];
									
									if (strcmp(pval->name, "SERIAL_NUMBER") == 0)
									{
										if( 0 < DebugLevel )
										{
											WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER,ABIOMASTER_NDMP_DEBUG,DEBUG_CODE_DETAIL " Get device info - %s : %s",MODULE_NAME_NDMP_HTTPD,__FUNCTION__,pval->name,pval->value);		
											WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "6. pval->name, pval->value %s : %s",pval->name, pval->value);		
										}

										if (serialNumber % DEFAULT_MEMORY_FRAGMENT == 0)
										{
											serialList = (char **)realloc(serialList, sizeof(char *) * (serialNumber + DEFAULT_MEMORY_FRAGMENT));
											memset(serialList + serialNumber, 0, sizeof(char *) * DEFAULT_MEMORY_FRAGMENT);
										}
										serialList[serialNumber] = (char *)malloc(sizeof(char) * (strlen(pval->value) + 1));
										memset(serialList[serialNumber], 0, sizeof(char) * (strlen(pval->value) + 1));
										strcpy(serialList[serialNumber], pval->value);
										
										if (serialNumber % DEFAULT_MEMORY_FRAGMENT == 0)
										{
											deviceFileList = (char **)realloc(deviceFileList, sizeof(char *) * (serialNumber + DEFAULT_MEMORY_FRAGMENT));
											memset(deviceFileList + serialNumber, 0, sizeof(char *) * DEFAULT_MEMORY_FRAGMENT);
										}
										deviceFileList[serialNumber] = (char *)malloc(sizeof(char) * (strlen(cap->device) + 1));
										memset(deviceFileList[serialNumber], 0, sizeof(char) * (strlen(cap->device) + 1));
										strcpy(deviceFileList[serialNumber], cap->device);

										WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "7. serialNumber : %d", serialNumber);	
										serialNumber++;
									}
									else
									{
										WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "*pval->name : %s != SERIAL_NUMBER", pval->name);	
									}
								}
							}
							
							else
							{
								WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "*5. strncmp fail");	
							}
						}
					}
				}
				else
				{
					WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "ndmp_reply->error : %d", ndmp_reply->error);
				}
			}
			else
			{
				WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "va_ndmp_comm_send_request() fail ==> r : %d", r);
			}
			
			va_ndmp_comm_free_message(connection);
			va_ndmp_connect_close(connection);
		}
		else
		{
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "va_ndmp_connect_ndmp_server() fail");
		}

		if( 0 < DebugLevel )
		{
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER,ABIOMASTER_NDMP_DEBUG,DEBUG_CODE_DETAIL " Get Drive Serial End",MODULE_NAME_NDMP_HTTPD,__FUNCTION__);	
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "serialNumber : %d", serialNumber);

		}
		for (i = 0; i < serialNumber; i++)
		{
			// make a reply message
			va_init_reply_buf(reply);
			
			strcpy(reply[0], "TAPE_DRIVE_SERIAL");
			strcpy(reply[214], deviceFileList[i]);
			strcpy(reply[251], serialList[i]);
			
			if(0 < DebugLevel)
			{
				WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "reply[0] : TAPE_DRIVE_SERIAL");
				WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "deviceFileList[i] : %s", reply[214]);
				WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "serialList[i] : %s", reply[251]);
			}


			va_make_reply_msg(reply, msg);
			if (va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE) < 0)
			{
				break;
			}
		}
		
		va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
	
		if(0 < DebugLevel)
		{
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER,ABIOMASTER_NDMP_HTTPD_LOG,DEBUG_CODE_DETAIL " GetTapeLibrarySerial() End\n",MODULE_NAME_NDMP_HTTPD,__FUNCTION__);	
		}
		for (i = 0; i < serialNumber; i++)
		{
			va_free(serialList[i]);
			va_free(deviceFileList[i]);
		}
		va_free(serialList);
		va_free(deviceFileList);
	}
}

void GetNodeModule()
{
	char reply[DSIZ][DSIZ];
	char msg[DSIZ * DSIZ];
	
	va_sock_t commandSock;
	
	int r;
	
	
	if(0 < DebugLevel)
	{
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "GetNodeModule() Start");
	}
	
	if ((commandSock = va_connect(masterIP, commandData.catalogPort, 1)) != -1)
	{
		r = va_ndmp_connect_ndmp_server(ndmpIP, (unsigned long)atol(ndmpPort), ndmpAccount, ndmpPasswd, NULL);
		if(0 < DebugLevel)
		{
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "r : %d", r);
		}
		// make a reply message
		va_init_reply_buf(reply);
		
		strcpy(reply[0], "NODE_MODULE");
		
		if (r == 0)
		{
			strcpy(reply[350], "1");
			if(0 < DebugLevel)
			{
				WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "reply[350] : 1");
			}
		}
		else if (r == -1)
		{
			strcpy(reply[350], "2");
			if(0 < DebugLevel)
			{
				WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "reply[350] : 2");
			}
		}
		else if (r == -2)
		{
			strcpy(reply[350], "3");
			if(0 < DebugLevel)
			{
				WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "reply[350] : 3");
			}
		}
		else if (r == -3)
		{
			strcpy(reply[350], "4");
			if(0 < DebugLevel)
			{
				WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "reply[350] : 4");
			}
		}
		else if (r == -4)
		{
			strcpy(reply[350], "0");
			if(0 < DebugLevel)
			{
				WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "reply[350] : 0");
			}
		}
		
		va_make_reply_msg(reply, msg);
		va_send(commandSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE);
		
		va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
	}
	else
	{
		if(0 < DebugLevel)
		{
			WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "va_connect() fail");
		}
	}
	
	if(0 < DebugLevel)
	{
		WriteDebugData(ABIOMASTER_NDMP_LOG_FOLDER, ABIOMASTER_NDMP_HTTPD_LOG, "GetNodeModule() End\n");
	}
}

int isalnumstr(char * str)
{
	int len;
	int i;
	
	
	
	if (str == NULL || str[0] == '\0')
	{
		return 0;
	}
	
	len = (int)strlen(str);
	
	for (i = 0; i < len; i++)
	{
		if (isalnum(str[i]) == 0)
		{
			return 0;
		}
	}
	
	return 1;
}

void Exit()
{
	#ifdef __ABIO_WIN
		//윈속 제거
		WSACleanup();
	#endif
}
