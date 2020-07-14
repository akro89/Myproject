#include "require.h"
#include "abio.h"
#include "lib.h"
#include "vendor.h"
#include "ck.h"

typedef struct {
  long tv_sec;
  long tv_usec;
}timevals;

// start of variables for abio library comparability
char ABIOMASTER_MASTER_SERVER_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_DATA_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_LOG_FOLDER[MAX_PATH_LENGTH];
// end of variables for abio library comparability


char ABIOMASTER_CK_FOLDER[MAX_PATH_LENGTH];


//////////////////////////////////////////////////////////////////////////////////////////////////
// ABIO log option
int requestLogLevel;
char logJobID[JOB_ID_LENGTH];
int logModuleNumber;
int retryCount;

char masterIP[IP_LENGTH];				// master server ip address
char masterLogPort[PORT_LENGTH];		// master server httpd logger port


//////////////////////////////////////////////////////////////////////////////////////////////////
// action list
struct actionList * actionList;
int actionListNumber;


//////////////////////////////////////////////////////////////////////////////////////////////////
// node configuration
char nodename[NODE_NAME_LENGTH];		// node name
char ** nodeip;							// ip address
int nodeipNumber;						// number of ip address
char ckPort[PORT_LENGTH];				// communication kernel port

struct portRule * portRule;		// VIRBAK ABIO IP Table ���
int portRuleNumber;				// VIRBAK ABIO IP Table ����

//////////////////////////////////////////////////////////////////////////////////////////////////
// variables for message control between communication kenel and abio module
struct mtype_stat * msgtype;				// message type status
int msgtypeNumber;
int nextMsgtype;							// message type that will be used next time
char strTransCK[NUMBER_STRING_LENGTH];		// string form of the message transfer
char strLang[NUMBER_STRING_LENGTH];

//////////////////////////////////////////////////////////////////////////////////////////////////
// variables for communication kernel message
va_sock_t sockCK;			// message�� ���� TCP/IP server socket
va_trans_t transCK;			// message�� ���� message transfer id(unix������ message queue id, windows������ named pipe handle)
va_sem_t semidThread;		// TCP/IP server socket�� message transfer���� message�� �޴� receiver�� message�� process�� �����ϴ� sender�� ����ȭ�ϱ� ���� semaphore
va_sem_t semidBuffer;		// receiver���� ���� message�� �����ϴ� message buffer�� ���� ������ receiver ���̿��� ����ȭ�ϴ� semaphore

struct msgbuf_ck * firstMsg;
struct msgbuf_ck * lastMsg;

int debugLevel;

//////////////////////////////////////////////////////////////////////////////////////////////////
// variables for communication kernel control
int ckComplete;
enum processType processType;


#ifdef __ABIO_WIN
	SERVICE_STATUS_HANDLE g_hSrv;
	DWORD g_nowstate;
#endif

#ifdef __ABIO_UNIX
	void del_defunct_process(int signo, siginfo_t *info, void *uarg);
#endif

int main(int argc, char ** argv)
{
	#ifdef __ABIO_UNIX
		char * convertFilename;
		int convertFilenameSize;
		
		struct sigaction act;
		
		sigemptyset(&act.sa_mask);

		act.sa_sigaction = del_defunct_process;
		act.sa_flags = SA_SIGINFO;
		
		sigaction(SIGCHLD, &act, NULL);
		
		if (va_convert_string_to_utf8(ENCODING_UNKNOWN, argv[0], (int)strlen(argv[0]), (void **)&convertFilename, &convertFilenameSize) == 0)
		{
			return 2;
		}
		
		StartCommunicationDaemon(convertFilename);
		
		va_free(convertFilename);
	#elif __ABIO_WIN
		#ifdef _DEBUG
			StartCommunicationDaemon();
		#else
			SERVICE_TABLE_ENTRYW ste[] = {{ABIOMASTER_CK_SERVICE_NAME, (LPSERVICE_MAIN_FUNCTIONW)StartCommunicationDaemon}, {NULL, NULL}};
			
			StartServiceCtrlDispatcherW(ste);
		#endif
	#endif
	
	return 0;
}

#ifdef __ABIO_UNIX
	int StartCommunicationDaemon(char * filename)
	{
		printf("Initialize a process to start the communication kernel.\n");
		
		// �� �ý��ۿ� �°� process�� �ʱ�ȭ�ϰ�, ���࿡ �ʿ��� ���� ������ �о�´�.
		if (InitProcess(filename) < 0)
		{
			// process �ʱ�ȭ�� ������ ��� process�� �����Ѵ�.
			// �ڿ� �������� ���� daemon�̳� service�� ����µ� ������ ��쳪, process ���࿡ �ʿ��� ���� ������ �߸� �����ǰų� ���� ��쿡 �߻��Ѵ�.
			// �̷� ��찡 �߻��ϸ� process�� �����Ѵ�.
			printf("It could not start up the communication kernel.\n");
			PrintDaemonLog("It could not start up the communication kernel.");
			
			Exit();
			
			return 2;
		}
		
		
		// communication kernel ������ �ʱ�ȭ �Ѵ�.
		if (InitCK() < 0)
		{
			// communication kernel ���� �ʱ�ȭ�� ������ ��� ����� �ڿ����� �ݳ��ϰ� �����Ѵ�.
			printf("It could not start up the communication kernel.\n");
			PrintDaemonLog("It could not start up the communication kernel.");
			
			Exit();
			
			return 2;
		}
		
		
		// process�� daemon�̳� service ���·� �����.
		if (va_make_service() < 0)
		{
			printf("It failed to make the communication kernel as a service.\n");
			printf("It could not start up the communication kernel.\n");
			PrintDaemonLog("It failed to make the communication kernel as a service.");
			PrintDaemonLog("It could not start up the communication kernel.");
			
			Exit();
			
			return 2;
		}
		
		printf("The communication kernel was started.\n");
		PrintDaemonLog("The communication kernel was started.");
		
		
		// communication kernel�� �����Ѵ�.
		if (CommunicationKernel() < 0)
		{
			Exit();
			
			return 2;
		}
		
		
		Exit();
		
		return 0;
	}
#elif __ABIO_WIN
	int StartCommunicationDaemon()
	{
		// process�� daemon�̳� service ���·� �����.
		#ifndef _DEBUG
			if ((g_hSrv = RegisterServiceCtrlHandlerW(ABIOMASTER_CK_SERVICE_NAME, (LPHANDLER_FUNCTION)CK_service_handler)) == 0)
			{
				return 2;
			}
		#endif
		
		// ���� ���� ��
		CK_set_status(SERVICE_START_PENDING, SERVICE_ACCEPT_STOP);
		
		
		// �� �ý��ۿ� �°� process�� �ʱ�ȭ�ϰ�, ���࿡ �ʿ��� ���� ������ �о�´�.
		if (InitProcess() < 0)
		{
			// process �ʱ�ȭ�� ������ ��� process�� �����Ѵ�.
			// �ڿ� �������� ���� daemon�̳� service�� ����µ� ������ ��쳪, process ���࿡ �ʿ��� ���� ������ �߸� �����ǰų� ���� ��쿡 �߻��Ѵ�.
			// �̷� ��찡 �߻��ϸ� process�� �����Ѵ�.
			PrintDaemonLog("It could not start up the communication kernel.");
			
			Exit();
			
			return 2;
		}
		
		
		// communication kernel ������ �ʱ�ȭ �Ѵ�.
		if (InitCK() < 0)
		{
			// communication kernel ���� �ʱ�ȭ�� ������ ��� ����� �ڿ����� �ݳ��ϰ� �����Ѵ�.
			PrintDaemonLog("It could not start up the communication kernel.");
			
			Exit();
			
			return 2;
		}
		
		// ���� ���۵�
		CK_set_status(SERVICE_RUNNING, SERVICE_ACCEPT_STOP);
		PrintDaemonLog("The communication kernel was started. pid : %d",va_getpid());
		
		
		// communication kernel�� �����Ѵ�.
		if (CommunicationKernel() < 0)
		{
			Exit();
			
			return 2;
		}
		
		
		Exit();
		
		return 0;
	}
#endif

#ifdef __ABIO_UNIX
	int InitProcess(char * filename)
#elif __ABIO_WIN
	int InitProcess()
#endif
{
	#ifdef __ABIO_UNIX
		int i;
	#elif __ABIO_WIN
		WSADATA wsaData;
	#endif
	
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 1. ���� �������� �ʱ�ȭ�Ѵ�.
	
	memset(ABIOMASTER_CK_FOLDER, 0, sizeof(ABIOMASTER_CK_FOLDER));
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// ABIO log option
	requestLogLevel = LOG_LEVEL_JOB;
	memset(logJobID, 0, sizeof(logJobID));
	logModuleNumber = MODULE_UNKNOWN;
	retryCount = 0;
	
	memset(masterIP, 0, sizeof(masterIP));
	memset(masterLogPort, 0, sizeof(masterLogPort));
	
	portRule = NULL;
	portRuleNumber = 0;

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// action list
	actionList = NULL;
	actionListNumber = 0;
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// node configuration
	nodeip = NULL;
	nodeipNumber = 0;
	memset(nodename, 0, sizeof(nodename));
	memset(ckPort, 0, sizeof(ckPort));
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// variables for message control between communication kenel and abio module
	msgtype = NULL;
	msgtypeNumber = CK_MESSAGE_TYPE_DEFAULT;
	
	debugLevel = 0;
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// variables for communication kernel message
	sockCK = -1;
	transCK = (va_trans_t)-1;
	semidThread = (va_sem_t)-1;
	semidBuffer = (va_sem_t)-1;
	
	firstMsg = NULL;
	lastMsg = NULL;
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// variables for communication kernel control
	ckComplete = 0;
	processType = PARENT_PROCESS;
	
	memset(strLang, 0, sizeof(strLang));
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 2. �� �ý��ۿ� �°� process�� �ʱ�ȭ�Ѵ�.
	
	// process resource limit�� �����Ѵ�.
	va_setrlimit();
	
	// set module number for debug log of ABIO common library
	logModuleNumber = MODULE_CK;
	
	// ���� �۾� ���丮�� �о�´�.
	#ifdef __ABIO_UNIX
		// ���� ��η� ����� ��� ���� ��ο��� ck folder�� �о�´�.
		if (filename[0] == FILE_PATH_DELIMITER)
		{
			for (i = (int)strlen(filename) - 1; i > 0; i--)
			{
				if (filename[i] == FILE_PATH_DELIMITER)
				{
					strncpy(ABIOMASTER_CK_FOLDER, filename, i);
					
					break;
				}
			}
		}
		// ��� ��η� ����� ��� ck folder�� �̵��ѵ� �۾� ���丮�� �о�´�.
		else
		{
			// ��� ��λ��� ck folder�� �о�´�.
			for (i = (int)strlen(filename) - 1; i > 0; i--)
			{
				if (filename[i] == FILE_PATH_DELIMITER)
				{
					strncpy(ABIOMASTER_CK_FOLDER, filename, i);
					
					break;
				}
			}
			
			// ck folder�� �̵��Ѵ�.
			if (chdir(ABIOMASTER_CK_FOLDER) < 0)
			{
				return -1;
			}
			
			// �۾� ���丮�� �о�´�.
			memset(ABIOMASTER_CK_FOLDER, 0, sizeof(ABIOMASTER_CK_FOLDER));
			if (va_get_working_directory(ABIOMASTER_CK_FOLDER) < 0)
			{
				return -1;
			}
		}
	#elif __ABIO_WIN
		if (va_get_working_directory(ABIOMASTER_CK_FOLDER) < 0)
		{
			return -1;
		}
	#endif
	
	
	PrintDaemonLog("Initialize a process to start the communication kernel.");
	
	#ifdef __ABIO_WIN
		// windows socket�� open�Ѵ�.
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) 
		{
			PrintDaemonLog("It failed to start up the windows socket.");
			
			return -1;
		}
	#endif
	
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 3. process ���࿡ �ʿ��� ���� ������ �о�´�.
	
	// get a module configuration
	if (GetModuleConfiguration() < 0)
	{
		return -1;
	}
	
	// load action list
	if (LoadActionList() < 0)
	{
		return -1;
	}
	
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 4. ������ Named pipe ������ ���� �޸� �Ҵ��� �Ѵ�.
	msgtype = (struct mtype_stat *)malloc(sizeof(struct mtype_stat) * msgtypeNumber);
	memset(msgtype, 0, sizeof(struct mtype_stat) * msgtypeNumber);
	nextMsgtype = 0;
	memset(strTransCK, 0, sizeof(strTransCK));
	
	
	return 0;
}

int GetModuleConfiguration()
{
	struct confOption * moduleConfig;
	int moduleConfigNumber;
	
	int i;
	
	
	
	// communication kernel�� ���������� �о�´�.
	if (va_load_conf_file(ABIOMASTER_CK_FOLDER, ABIOMASTER_CK_CONFIG, &moduleConfig, &moduleConfigNumber) > 0)
	{
		for (i = 0; i < moduleConfigNumber; i++)
		{
			if (!strcmp(moduleConfig[i].optionName, NODE_NAME_OPTION_NAME))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(nodename) - 1)
				{
					strcpy(nodename, moduleConfig[i].optionValue);
				}
			}
			else if (!strcmp(moduleConfig[i].optionName, NODE_IP_OPTION_NAME))
			{
				if (nodeipNumber % DEFAULT_MEMORY_FRAGMENT == 0)
				{
					nodeip = (char **)realloc(nodeip, sizeof(char *) * (nodeipNumber + DEFAULT_MEMORY_FRAGMENT));
					memset(nodeip + nodeipNumber, 0, sizeof(char *) * DEFAULT_MEMORY_FRAGMENT);
				}
				nodeip[nodeipNumber] = (char *)malloc(sizeof(char) * (strlen(moduleConfig[i].optionValue) + 1));
				memset(nodeip[nodeipNumber], 0, sizeof(char) * (strlen(moduleConfig[i].optionValue) + 1));
				strcpy(nodeip[nodeipNumber], moduleConfig[i].optionValue);
				nodeipNumber++;
			}
			else if (!strcmp(moduleConfig[i].optionName, CK_PORT_OPTION_NAME))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(ckPort) - 1)
				{
					strcpy(ckPort, moduleConfig[i].optionValue);
				}
			}
			else if (!strcmp(moduleConfig[i].optionName, DEBUG_LEVEL))
			{
				debugLevel =atoi(moduleConfig[i].optionValue);
			}
			else if (!strcmp(moduleConfig[i].optionName, ABIOMASTER_PORT_RULE_OPTION_NAME))
			{
				va_parse_port_rule(moduleConfig[i].optionValue, &portRule, &portRuleNumber);
			}
			else if (!strcmp(moduleConfig[i].optionName, LANGUAGEPACK_NAME_OPTION_NAME))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(strLang) - 1)
				{
					strcpy(strLang, moduleConfig[i].optionValue);
				}
			}
			#ifdef __ABIO_WIN
				else if (!strcmp(moduleConfig[i].optionName, NAMED_PIPE_NUMBER_OPTION_NAME))
				{
					msgtypeNumber = atoi(moduleConfig[i].optionValue);
					
					if (msgtypeNumber > CK_MESSAGE_TYPE_MAX)
					{
						msgtypeNumber = CK_MESSAGE_TYPE_MAX;
					}
				}
			#endif
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
		printf("It failed to open a configuration file \"%s\".\n", ABIOMASTER_CK_CONFIG);
		PrintDaemonLog("It failed to open a configuration file \"%s\".", ABIOMASTER_CK_CONFIG);
		
		return -1;
	}
	
	
	if (CheckConfigurationValid() == 1)
	{
		return 0;
	}
	else
	{
		return -1;
	}
}

int CheckConfigurationValid()
{
	int configValid;
	int i;
	
	
	
	// initialize variables
	configValid = 1;
	
	
	// communication kernel�� �������� ����� �����Ǿ����� Ȯ���Ѵ�.
	if (nodename[0] == '\0')
	{
		configValid = 0;
		
		printf("Node name is not set.\n");
		PrintDaemonLog("Node name is not set.");
	}
	
	if (nodeipNumber == 0)
	{
		configValid = 0;
		
		printf("IP address is not set.\n");
		PrintDaemonLog("IP address is not set.");
	}
	else
	{
		for (i = 0; i < nodeipNumber; i++)
		{
			if (va_is_valid_ip(nodeip[i]) == 0)
			{
				configValid = 0;
				
				printf("IP address [%s] is not valid.\n", nodeip[i]);
				PrintDaemonLog("IP address [%s] is not valid.", nodeip[i]);
			}
		}
	}
	
	if (ckPort[0] == '\0')
	{
		configValid = 0;
		
		printf("Communication kernel service tcp/ip port is not set.\n");
		PrintDaemonLog("Communication kernel service tcp/ip port is not set.");
	}
	else if (va_is_valid_port(ckPort) == 0)
	{
		configValid = 0;
		
		printf("Communication kernel service tcp/ip port [%s] is invalid.\n", ckPort);
		PrintDaemonLog("Communication kernel service tcp/ip port [%s] is invalid.", ckPort);
	}
	
	
	return configValid;
}

int LoadActionList()
{
	char ** lines;
	int * linesLength;
	int lineNumber;
	
	char moduleName[MAX_ACTION_COMMAND_NAME_LENGTH];
	char moduleFile[MAX_ACTION_COMMAND_VALUE_LENGTH];
	
	int firstBlank;
	int secondBlank;
	
	int i;
	int j;
	
	
	
	if ((lineNumber = va_load_text_file_lines(ABIOMASTER_CK_FOLDER, ACTION_LIST, &lines, &linesLength)) > 0)
	{
		for (i = 0; i < lineNumber; i++)
		{
			if (lines[i][0] == '\0')
			{
				continue;
			}
			
			memset(moduleName, 0, sizeof(moduleName));
			memset(moduleFile, 0, sizeof(moduleFile));
			
			for (j = 0; j < linesLength[i]; j++)
			{
				if (lines[i][j] == ' ' || lines[i][j] == '\t')
				{
					firstBlank = j;
					break;
				}
			}
			
			for (j = linesLength[i] - 1; j > 0; j--)
			{
				if (lines[i][j] == ' ' || lines[i][j] == '\t')
				{
					secondBlank = j;
					break;
				}
			}
			
			strncpy(moduleName, lines[i], firstBlank);
			strncpy(moduleFile, lines[i] + firstBlank + 1, secondBlank - firstBlank - 1);
			
			
			if (actionListNumber % DEFAULT_MEMORY_FRAGMENT == 0)
			{
				actionList = (struct actionList *)realloc(actionList, sizeof(struct actionList) * (actionListNumber + DEFAULT_MEMORY_FRAGMENT));
				memset(actionList + actionListNumber, 0, sizeof(struct actionList) * DEFAULT_MEMORY_FRAGMENT);
			}
			strcpy(actionList[actionListNumber].moduleName, moduleName);
			strcpy(actionList[actionListNumber].moduleFile, moduleFile);
			actionListNumber++;
		}
		
		for (i = 0; i < lineNumber; i++)
		{
			va_free(lines[i]);
		}
		va_free(lines);
		va_free(linesLength);
	}
	else
	{
		printf("It failed to open the action list file.\n");
		PrintDaemonLog("It failed to open the action list file.");
		
		return -1;
	}
	
	
	return 0;
}

int InitCK()
{
	int i;
	
	#ifdef __ABIO_HP
		char langEnvValue[MAX_PATH_LENGTH];
		memset(langEnvValue,0,MAX_PATH_LENGTH);
	#endif
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 1. Communication Kernel�� �ٸ� ���μ������� message�� �ְ�ޱ� ���� ��θ� �����Ѵ�.
	
	// 1.1 daemon�� ��Ű� �ٸ� node�� ����� ���� TCP/IP server socket�� �����.
	if ((sockCK = va_make_server_socket(ckPort, NULL, 1)) == -1)
	{
		printf("It failed to bind the tcp/ip port \"%s\".\n", ckPort);
		PrintDaemonLog("It failed to bind the tcp/ip port \"%s\".", ckPort);
		
		return -1;
	}
	
	
	// 1.2 Communication Kernel�� �ٸ� ���μ����� ����� ���� message transfer�� �����.
	// system�� unix�� ��쿡�� message queue�� �����, windows�� ��쿡�� named pipe�� �����.
	#ifdef __ABIO_UNIX
		if ((transCK = va_make_message_transfer(0)) == (va_trans_t)-1)
		{
			printf("It failed to make a message queue.\n");
			PrintDaemonLog("It failed to make a message queue.");
			
			return -2;
		}
	#elif __ABIO_WIN
		// ck�� �ٸ� ��⿡�� message�� ������ ����� named pipe instance�� �����.
		if ((msgtype[1].transid = va_make_message_transfer(1)) == (va_trans_t)-1)
		{
			printf("It failed to make a named pipe\n");
			PrintDaemonLog("It failed to make a named pipe.");
			
			return -2;
		}
		transCK = msgtype[1].transid;
		
		// ck�� �ٸ� ��⿡ message�� ������ �� ����� named pipe instance�� �����.
		for (i = CK_MESSAGE_TYPE_INIT; i < msgtypeNumber; i++)
		{
			if ((msgtype[i].transid = va_make_message_transfer(i)) == (va_trans_t)-1)
			{
				printf("It failed to make a named pipe.\n");
				PrintDaemonLog("It failed to make a named pipe.");
				
				return -2;
			}
		}
	#endif
	
	#if defined(__ABIO_WIN) && defined(__ABIO_64)
		va_lltoa((__int64)transCK, strTransCK);
	#else
		va_itoa((int)transCK, strTransCK);
	#endif
	
	#ifdef __ABIO_UNIX
		if( 0 < strlen(strLang))
		{
			#if defined(__ABIO_HP)					
				sprintf(langEnvValue,"LC_ALL=%s",strLang);
				printf("%s\n",langEnvValue);
				putenv( langEnvValue );

				memset(langEnvValue,0,MAX_PATH_LENGTH);
				sprintf(langEnvValue,"LANG=%s",strLang);
				printf("%s\n",langEnvValue);				
				putenv( langEnvValue );								
			#else
				setenv( "LANG", strLang , 1);
				setenv( "LC_ALL", strLang , 1);
			#endif

			printf("Communication kernel service use defined language in configuration file. - %s\n",safe(getenv("LANG")));
			PrintDaemonLog("Communication kernel service use defined language in configuration file. - %s",safe(getenv("LANG")));
		}
		else
		{
			printf("Communication kernel service use defalut language of operation system. - %s\n",safe(getenv("LANG")));
			PrintDaemonLog("Communication kernel service use defalut language of operation system. - %s",safe(getenv("LANG")));
		}
	#endif
	
	// initialize message type
	for (i = CK_MESSAGE_TYPE_INIT; i < msgtypeNumber; i++)
	{
		msgtype[i].use = 0;
	}
	nextMsgtype = CK_MESSAGE_TYPE_INIT;
	
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 2. Communication Kernel�� thread�� message buffer ����ȭ�� ���� semaphore�� �����.
	
	// receiver threads�� sender thread�� ����ȭ�ϱ� ���� sempahore�� �����.
	if ((semidThread = va_make_semaphore(0)) == (va_sem_t)-1)
	{
		printf("It failed to make a semaphore.\n");
		PrintDaemonLog("It failed to make a semaphore.");
		
		return -3;
	}
	
	// message buffer�� ����ȭ�ϱ� ���� sempahore�� �����.
	if ((semidBuffer = va_make_semaphore(1)) == (va_sem_t)-1)
	{
		printf("It failed to make a semaphore.\n");
		PrintDaemonLog("It failed to make a semaphore.");
		
		return -3;
	}
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 3. message buffer�� �ʱ�ȭ�Ѵ�.
	
	firstMsg = (struct msgbuf_ck *)malloc(sizeof(struct msgbuf_ck));
	memset(firstMsg, 0, sizeof(struct msgbuf_ck));
	firstMsg->next = NULL;
	lastMsg = firstMsg;
	
	
	return 0;
}

int CommunicationKernel()
{
	va_thread_t tidSocket;
	va_thread_t tidTransfer;
	va_thread_t tidProcess;

	#ifdef __ABIO_UNIX
		va_thread_t tidWait;
	#endif
	
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 1. message ���Ű� ������ ���� thread�� �����Ѵ�.
	
	// 1.1 TCP/IP server socket���� message�� �޴� thread�� �����Ѵ�.
	if ((tidSocket = va_create_thread(GetMessageFromSocket, NULL)) == 0)
	{
		printf("It failed to make a thread.\n");
		PrintDaemonLog("It failed to make a thread.");
		
		return -1;
	}
		
	// 1.2 message transfer���� message�� �޴� thread�� �����Ѵ�.
	if ((tidTransfer = va_create_thread(GetMessageFromTransfer, NULL)) == 0)
	{
		printf("It failed to make a thread.\n");
		PrintDaemonLog("It failed to make a thread.");
		
		CloseThread();
		
		va_wait_thread(tidSocket);
		
		return -1;
	}
		
	// 1.3 message�� �޾Ƽ� ó���ϴ� thread�� �����Ѵ�.
	if ((tidProcess = va_create_thread(ProcessMessage, NULL)) == 0)
	{
		printf("It failed to make a thread.\n");
		PrintDaemonLog("It failed to make a thread.");
		
		CloseThread();
		
		va_wait_thread(tidSocket);
		va_wait_thread(tidTransfer);
		
		return -1;
	}
	
	// 1.4 child process�� �����⸦ ��ٸ��� thread�� �����.
	#ifdef __ABIO_UNIX
		if ((tidWait = va_create_thread(WaitChild, NULL)) == 0)
		{
			printf("It failed to make a thread.\n");
			PrintDaemonLog("It failed to make a thread.");
			
			CloseThread();
			
			va_wait_thread(tidSocket);
			va_wait_thread(tidTransfer);
			va_wait_thread(tidProcess);
			
			return -1;
		}
		else
		{
			pthread_detach(tidWait);
		}
	#endif
	
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 2. thread���� ��� ���������� ��ٸ���.
	
	va_wait_thread(tidSocket);
	va_wait_thread(tidTransfer);
	va_wait_thread(tidProcess);
	
	
	return 0;
}

va_thread_return_t GetMessageFromSocket(void * arg)
{
	struct ck requestCommand;
	struct ck reCommand;
	
	int connectionSock;
	char connectionIP[IP_LENGTH];
	
	int recvSize;
	
	#ifdef __ABIO_UNIX
		timevals tv;
	#endif

	struct ckBackup commandData;
	
	va_sleep(1);
	
	while (1)
	{
		// �ٸ� process�� �ٸ� system���� ������ ���⸦ ��ٸ���.
		if ((connectionSock = va_wait_connection(sockCK, 1, connectionIP)) >= 0)
		{
			// message�� �޾Ƽ� message buffer�� �����ϰ�, message processor���� message�� ������ �˸���.





			#ifdef __ABIO_WIN
				DWORD recvTimeout = 5000;  // 5��.
				if(setsockopt(connectionSock, SOL_SOCKET, SO_RCVTIMEO, (char*)&recvTimeout, sizeof(recvTimeout)) < 0)
				{
					PrintDaemonLog("fail");
				}
			#elif __ABIO_UNIX 	 							
				tv.tv_sec  = 5;
				tv.tv_usec = 0;
				setsockopt(connectionSock, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(timevals));
			#endif

			// message�� �޾Ƽ� message buffer�� �����ϰ�, message processor���� message�� ������ �˸���.

			if ((recvSize = va_recv(connectionSock, &requestCommand, sizeof(struct ck), 0, DATA_TYPE_CK)) == sizeof(struct ck))
			{
				//ky88 for debug...
				if ( 10 < debugLevel ) 
				{
					printf("CK va_recv : %s,%s,%s. source : %s(%s:%s), destination : %s(%s:%s)\n", requestCommand.requestCommand, requestCommand.subCommand, requestCommand.executeCommand,
						requestCommand.sourceNodename,requestCommand.sourceIP,requestCommand.sourcePort,requestCommand.destinationNodename,requestCommand.destinationIP,requestCommand.destinationPort);
					PrintDaemonLog("va_recv : %s,%s,%s. source : %s(%s:%s), destination : %s(%s:%s)", requestCommand.requestCommand, requestCommand.subCommand, requestCommand.executeCommand,
						requestCommand.sourceNodename,requestCommand.sourceIP,requestCommand.sourcePort,requestCommand.destinationNodename,requestCommand.destinationIP,requestCommand.destinationPort);
				}

				
				// ������ ������ �� ���� �޽����� �ٽ� ������, Ȯ���ϵ��� �ϴ� ��ƾ�� �߰���.				
				if ( ( !strcmp(requestCommand.requestCommand, "MOUNT") || !strcmp(requestCommand.requestCommand, "UNMOUNT")
					|| !strcmp(requestCommand.requestCommand, "MOUNT_NEW_VOLUME") || !strcmp(requestCommand.requestCommand, "UNMOUNT_VOLUME") 
					|| !strcmp(requestCommand.requestCommand, "MASTER_BACKUP") ) 
				   || ( strcmp(requestCommand.sourceIP, requestCommand.destinationIP) && strcmp(requestCommand.sourceNodename, requestCommand.destinationNodename) 
					 && !strcmp(requestCommand.requestCommand, "SLAVE_BACKUP") && !strcmp(requestCommand.executeCommand, "MASTER_BACKUP") )  
					|| (!strcmp(requestCommand.requestCommand, "CLIENT_HTTPD") && !strcmp(requestCommand.executeCommand, "MASTER_LIBRARYD"))  
|| (!strcmp(requestCommand.requestCommand, "NDMP_HTTPD") && !strcmp(requestCommand.executeCommand, "MASTER_LIBRARYD"))  ) //2018.05.11 ky88 NAS ���� changer �� ���� ���� ��.					// <MOVE_MEDIUM> ���� �����ϰ��� �� ��.

				{
					//duplicate jobID.	
					memcpy(&reCommand, &requestCommand, sizeof(struct ck));
					if (va_send(connectionSock, &reCommand, sizeof(struct ck), 0, DATA_TYPE_CK) < 0)
					{
						printf("CK va_send(%s,%s,%s) failed after va_recv(), source : %s(%s:%s), destination : %s(%s:%s)\n", requestCommand.requestCommand, requestCommand.subCommand, requestCommand.executeCommand,
							requestCommand.sourceNodename,requestCommand.sourceIP,requestCommand.sourcePort,requestCommand.destinationNodename,requestCommand.destinationIP,requestCommand.destinationPort);
						PrintDaemonLog("va_send(%s,%s,%s) failed after va_recv(), source : %s(%s:%s), destination : %s(%s:%s)", requestCommand.requestCommand, requestCommand.subCommand, requestCommand.executeCommand,
							requestCommand.sourceNodename,requestCommand.sourceIP,requestCommand.sourcePort,requestCommand.destinationNodename,requestCommand.destinationIP,requestCommand.destinationPort);
						
						//ky88 2015.09.21. if va_send fail then  continue recv()...
						// connection�� �����Ѵ�.
						va_close_socket(connectionSock, ABIO_SOCKET_CLOSE_SERVER);
						continue;
					}
					
					memcpy(&commandData, requestCommand.dataBlock, sizeof(struct ckBackup));				
					//ky88 AIX alway va_send ok, so job maybe duplicate...
					if ( !strcmp(requestCommand.requestCommand, "SLAVE_BACKUP") && !strcmp(requestCommand.executeCommand, "MASTER_BACKUP") 
						 && !strcmp(requestCommand.subCommand, "") ) 
					{
						if (!strcmp(logJobID, commandData.jobID) && (retryCount == commandData.retryCount)) 
						{
							printf("CK %s va_recv(%s,%s,%s) same jobID , source : %s(%s:%s), destination : %s(%s:%s)\n", commandData.jobID, requestCommand.requestCommand, requestCommand.subCommand, requestCommand.executeCommand,
								requestCommand.sourceNodename,requestCommand.sourceIP,requestCommand.sourcePort,requestCommand.destinationNodename,requestCommand.destinationIP,requestCommand.destinationPort);
							PrintDaemonLog("%s va_recv(%s,%s,%s) same jobID , source : %s(%s:%s), destination : %s(%s:%s)", commandData.jobID, requestCommand.requestCommand, requestCommand.subCommand, requestCommand.executeCommand,
								requestCommand.sourceNodename,requestCommand.sourceIP,requestCommand.sourcePort,requestCommand.destinationNodename,requestCommand.destinationIP,requestCommand.destinationPort);
							
							va_close_socket(connectionSock, ABIO_SOCKET_CLOSE_SERVER);
							continue;
						}
						else
						{
							strcpy(logJobID, commandData.jobID);
							retryCount = commandData.retryCount;

						}
					}
										
				}
										
		
				// connection�� �����Ѵ�.
				va_close_socket(connectionSock, ABIO_SOCKET_CLOSE_SERVER);
				
				// communication kernel�� �����ϴ� message�� ���� communication kernel�� �����Ѵ�.
				if (!strcmp(requestCommand.requestCommand, "EXIT_CK"))
				{
					break;
				}
				// �� ���� message�̸� message�� message buffer�� �����ϰ�, message processor���� message�� ������ �˸���.
				else
				{
					if (!strcmp(requestCommand.requestCommand, MODULE_NAME_SERVICE_CONTROLLER))
					{									
						va_itoa(sockCK,requestCommand.reserved4);						
					}
					// message�� message buffer�� �����ϰ�, message processor���� message�� ������ �˸���.
					if (AppendMessageToMessageBuffer(&requestCommand, connectionIP) < 0)
					{
						break;
					}
				}
			}
			// message�� ���� �������� connection�� �����Ѵ�.
			else if (recvSize != 0)
			{
				if ( 10 < debugLevel ) 
				{
					//ky88 for debug...
					if (recvSize > 0) 
					{
						printf("CK va_recv != 0 recvSize : %d - %s,%s,%s. source : %s(%s:%s), destination : %s(%s:%s)\n", recvSize, requestCommand.requestCommand, requestCommand.subCommand, requestCommand.executeCommand,
							requestCommand.sourceNodename,requestCommand.sourceIP,requestCommand.sourcePort,requestCommand.destinationNodename,requestCommand.destinationIP,requestCommand.destinationPort);
						PrintDaemonLog("va_recv != 0 recvSize : %d - %s,%s,%s. source : %s(%s:%s), destination : %s(%s:%s)", recvSize, requestCommand.requestCommand, requestCommand.subCommand, requestCommand.executeCommand,
							requestCommand.sourceNodename,requestCommand.sourceIP,requestCommand.sourcePort,requestCommand.destinationNodename,requestCommand.destinationIP,requestCommand.destinationPort);
					}
					else
					{
						//printf("CK va_recv() != 0 recvSize : %d\n", recvSize);
						//PrintDaemonLog("va_recv() != 0 recvSize : %d", recvSize);
					}
				}
				
				
				va_close_socket(connectionSock, 0);
			}
			// message�� ���� �������� connection�� �����Ѵ�.
			else
			{
				//printf("CK va_recv() == 0 recvSize : %d\n", recvSize);
				//PrintDaemonLog("va_recv() == 0 recvSize : %d", recvSize);			
				
				va_close_socket(connectionSock, ABIO_SOCKET_CLOSE_SERVER);
			}
		}
		else if (connectionSock == -1)
		{
			if (ckComplete == 1)
			{
				break;
			}
		}
		else
		{
			printf("The server socket has been closed.\n");
			PrintDaemonLog("The server socket has been closed.");
			
			break;
		}
	}
	
	printf("GetMessageFromSocket has been closed.\n");
	PrintDaemonLog("GetMessageFromSocket has been closed.");
	
	if (processType == PARENT_PROCESS)
	{
		CloseThread();
	}
	
	
	#ifdef __ABIO_UNIX
		return NULL;
	#else
		_endthreadex(0);
		return 0;
	#endif
}

va_thread_return_t GetMessageFromTransfer(void * arg)
{
	struct ck requestCommand;
	
	
	
	va_sleep(1);
	
	while (1)
	{
		// message transfer�κ��� message�� ��ٸ���.
		if (va_msgrcv(transCK, 1, &requestCommand, sizeof(struct ck), 0) > 0)
		{
			// communication kernel�� �����ϴ� message�� ���� communication kernel�� �����Ѵ�.
			if (!strcmp(requestCommand.requestCommand, "EXIT_CK"))
			{
				break;
			}
			// �� ���� message�̸� message�� message buffer�� �����ϰ�, message processor���� message�� ������ �˸���.
			else
			{
				// message�� message buffer�� �����ϰ�, message processor���� message�� ������ �˸���.
				if (AppendMessageToMessageBuffer(&requestCommand, NULL) < 0)
				{
					printf("The message transfer has been closed.\n");
					PrintDaemonLog("The message transfer has been closed.");
					
					break;
				}
			}
		}
		else
		{
			printf("The message transfer has been closed.\n");
			PrintDaemonLog("The message transfer has been closed.");
			
			break;
		}
	}
	
	printf("GetMessageFromTransfer has been closed.\n");
	PrintDaemonLog("GetMessageFromTransfer has been closed.");
	
	if (processType == PARENT_PROCESS)
	{
		CloseThread();
	}
	
	
	#ifdef __ABIO_UNIX
		return NULL;
	#else
		_endthreadex(0);
		return 0;
	#endif
}

va_thread_return_t ProcessMessage(void * arg)
{
	struct ck command;
	struct ck Recommand;
	struct ckBackup commandData;
	char connectionIP[IP_LENGTH];
	
	va_sock_t sock;
	char requestModuleName[MAX_ACTION_COMMAND_NAME_LENGTH];
	char action[MAX_ACTION_COMMAND_VALUE_LENGTH];
	int pmtype;
	
	enum commandDestination { INSIDE, OUTSIDE } destination;		// whether destination of the command is outside.
	
	struct msgbuf_ck * newMsg;
	struct msgbuf_ck * oldMsg;
	
	#ifdef __ABIO_WIN
		struct msg_arg * msgInfo;
	#endif
	
	int count;
	int i;
	
	int retry_count = 0;
			
	
	va_sleep(1);
	
	newMsg = firstMsg;
	
	while (1)
	{
		// wait a new message
		if (va_grab_semaphore(semidThread) < 0)
		{
			printf("The message transfer has been closed.\n");
			PrintDaemonLog("The message transfer has been closed.");
			
			break;
		}
		
		if (ckComplete == 1)
		{
			break;
		}
		
		// get a new message
		oldMsg = newMsg;
		newMsg = newMsg->next;
		
		// delete a old message
		oldMsg->next = NULL;
		va_free(oldMsg);
		
		// if the message buffer is empty, terminate a communication kernel
		if (newMsg == NULL)
		{
			break;
		}
		
		
		// initialize variables
		memset(&command, 0, sizeof(struct ck));
		memset(&commandData, 0, sizeof(struct ckBackup));
		memset(connectionIP, 0, sizeof(connectionIP));
		memset(action, 0, sizeof(action));
		
		
		// copy the new message and connection ip
		memcpy(&command, &newMsg->msg, sizeof(struct ck));
		memcpy(&commandData, command.dataBlock, sizeof(struct ckBackup));
		
		strcpy(connectionIP, newMsg->connectionIP);
		if (masterIP[0] == '\0')
		{
			if (commandData.client.masterIP[0] != '\0')
			{
				strcpy(masterIP, commandData.client.masterIP);
				strcpy(masterLogPort, commandData.client.masterLogPort);
			}
		}
		
		
		// message�� destination�� �����Ѵ�.
		// message�� destination node�� ck�� node���� �ƴ��� Ȯ���Ѵ�.
		destination = OUTSIDE;
		
		if (!strcmp(nodename, command.destinationNodename))
		{
			destination = INSIDE;
		}
		else
		{
			for (i = 0; i < nodeipNumber; i++)
			{
				if (!strcmp(nodeip[i], command.destinationIP) && !strcmp(ckPort, command.destinationPort))
				{
					destination = INSIDE;
					
					break;
				}
			}
		}
		
		// destination�� �ܺ��� ���, destination���� message�� ������.
		if (destination == OUTSIDE)
		{
			retry_count = 0;
			RETRY:
			// destination���� �����ؼ� message�� �����Ѵ�.
			if ((sock = va_make_client_socket_iptable(masterIP,command.destinationIP, portRule, portRuleNumber,command.destinationPort, 0)) != -1)
			{
				if (va_send(sock, &command, sizeof(struct ck), 0, DATA_TYPE_CK) < 0) 
				{
					printf("CK va_send(%s,%s,%s) failed, source : %s(%s:%s), destination : %s(%s:%s) retry %d.\n", command.requestCommand, command.subCommand, command.executeCommand,
						command.sourceNodename,command.sourceIP,command.sourcePort,command.destinationNodename,command.destinationIP,command.destinationPort, retry_count);
					PrintDaemonLog("va_send(%s,%s,%s) failed, source : %s(%s:%s), destination : %s(%s:%s) retry %d.", command.requestCommand, command.subCommand, command.executeCommand,
						command.sourceNodename,command.sourceIP,command.sourcePort,command.destinationNodename,command.destinationIP,command.destinationPort, retry_count);

					va_close_socket(sock, ABIO_SOCKET_CLOSE_CLIENT);
					
					if (retry_count < 5) 
					{
						retry_count++;
						va_sleep_random_range(10, 30);
						goto RETRY;
					}

				}
				
				//������ ������ �� �տ� ���� Ŀ�ǵ带 �ٽ� �޾Ƽ� Ȯ���ϰ� �ȿ��� ��õ�.
				if ( ( strcmp(command.sourceIP, command.destinationIP) && strcmp(command.sourceNodename, command.destinationNodename) 
					 && !strcmp(command.requestCommand, "SLAVE_BACKUP") && !strcmp(command.executeCommand, "MASTER_BACKUP") )
					|| (!strcmp(command.requestCommand, "CLIENT_HTTPD") && !strcmp(command.executeCommand, "MASTER_LIBRARYD")) ) 
					 // <MOVE_MEDIUM> ���� �����ϰ��� �� ��
 
				{
					//ky88 va_recv...
					memset(&Recommand, 0, sizeof(struct ck));
					if (va_recv(sock, &Recommand, sizeof(struct ck), 0, DATA_TYPE_CK) != sizeof(struct ck))
					{
						printf("CK va_recv(%s,%s,%s) failed after va_send(), source : %s(%s:%s), destination : %s(%s:%s) retry %d.\n", command.requestCommand, command.subCommand, command.executeCommand,
							command.sourceNodename,command.sourceIP,command.sourcePort,command.destinationNodename,command.destinationIP,command.destinationPort, retry_count);
						PrintDaemonLog("va_recv(%s,%s,%s) failed after va_send(), source : %s(%s:%s), destination : %s(%s:%s) retry %d.", command.requestCommand, command.subCommand, command.executeCommand,
							command.sourceNodename,command.sourceIP,command.sourcePort,command.destinationNodename,command.destinationIP,command.destinationPort, retry_count);

						va_close_socket(sock, ABIO_SOCKET_CLOSE_CLIENT);
					
						if (retry_count < 5) {
							retry_count++;
							va_sleep_random_range(10, 30);
							goto RETRY;
						}
					}
					
				}
				
				
				va_close_socket(sock, ABIO_SOCKET_CLOSE_CLIENT);
				
				
			}
			else
			{
				// ck�� ���ļ� �ܺη� ���� message�� �ٸ� node�� �ִ� ck�� ����.
				// �ٸ� node�� �ִ� ck�� ������ �����ߴٴ� ���� ��� ck�� �׾��ִٴ� ����̴�.
				// �� ��� message�� ������ source process�� message�� ������� �Ѵ�.
				// ���� ����(VIRBAK ABIO v2.2.x)������ message�� ������ source process����
				// ���ۿ� ������ message�� �޾Ƽ� ó���ϴ� ��ƾ�� ������ �Ǿ����� �ʱ⶧����
				// ���� �������� ó���ϴ� ������ �Ѵ�.
			}
		}
		// destination�� ������ ���, action list���� process�� ã�Ƽ� �����Ѵ�.
		else
		{
			// �ܺο��� ck�� message�� �� ��� message�� source ip�� connection ip�� �����Ѵ�.
			if (connectionIP[0] != '\0')
			{
				memset(command.sourceIP, 0, sizeof(command.sourceIP));
				strcpy(command.sourceIP, connectionIP);
			}
			
			// action list���� process�� ã�Ƽ� message�� �����Ѵ�.
			if (FindAction(&command, requestModuleName, action) == 0)
			{
				// process�� ���� �����ؼ� message�� �����ؾ��ϴ� ��� message transfer���� ����� message type�� ã��, 
				// process�� �����ϰ� message�� �����Ѵ�.
				if (command.execute == 0)
				{
					// message transfer���� ����Ҽ� �ִ� message type�� ������ process�� �����ϰ� message�� �����Ѵ�.
					if ((pmtype = GetMessageType()) > 0)
					{
						// process�� �����ϰ� message�� �����Ѵ�.
						if (SendMessageToModule(action, pmtype, &command) < 0)
						{
							//ky88 for debug...
							PrintDaemonLog("SendMesageToModule(%s, %s, ...) failed.", action, pmtype);
							
							// ������ module�� message ���ۿ� ������ ����̴�.
							// �� ��� message�� ������ source process�� message�� ������� �Ѵ�.
							// ���� ����(VIRBAK ABIO v2.2.x)������ message�� ������ source process����
							// ���ۿ� ������ message�� �޾Ƽ� ó���ϴ� ��ƾ�� ������ �Ǿ����� �ʱ⶧����
							// ���� �������� ó���ϴ� ������ �Ѵ�.
						}
					}
					// message transfer���� ����� �� �ִ� message type�� ���� ��� communication kernel�� �����Ѵ�.
					else
					{
						// message transfer���� ����� �� �ִ� message type�� ���ٴ� ���� child process�� ����� ������� ���� ��찡
						// �ݺ��ؼ� �߻��ϰ� �ְų�, child process�� ����� ���� ����� �����ϰ� ���� ���ϴٴ� ����.
						// ���̻� ��밡���� message type�� ���� ������ communication kernel�� �����Ѵ�.
						printf("There are no unused message type.\n");
						PrintDaemonLog("There are no unused message type.");
						
						break;
					}
				}
				// �̹� ����ǰ� �ִ� process�� message�� �����ؾ��ϴ� ��� ������ message type�� ����ؼ� message�� �����Ѵ�.
				else
				{
					// message�� �����Ѵ�.
					if (!strncmp(requestModuleName, MODULE_NAME_PREFIX_CLIENT, strlen(MODULE_NAME_PREFIX_CLIENT)) || !strncmp(requestModuleName, MODULE_NAME_PREFIX_DB, strlen(MODULE_NAME_PREFIX_DB)))
					{
						#ifdef __ABIO_UNIX
							if (va_msgsnd(transCK, commandData.mountStatus.mtype2, &command, sizeof(struct ck), 0) < 0)
							{
								// ������ module�� message ���ۿ� ������ ����̴�.
								// �� ��� message�� ������ source process�� message�� ������� �Ѵ�.
								// ���� ����(VIRBAK ABIO v2.2.x)������ message�� ������ source process����
								// ���ۿ� ������ message�� �޾Ƽ� ó���ϴ� ��ƾ�� ������ �Ǿ����� �ʱ⶧����
								// ���� �������� ó���ϴ� ������ �Ѵ�.
							}
						#elif __ABIO_WIN
							msgInfo = (struct msg_arg *)malloc(sizeof(struct msg_arg));
							msgInfo->pmtype = commandData.mountStatus.mtype2;
							memcpy(&msgInfo->command, &command, sizeof(struct ck));
							
							count = 0;
							while (_beginthread(SendMessageToNamedPipe, 0, (void *)msgInfo) == (uintptr_t)-1)
							{
								if (count == THREAD_TIME_OUT)
								{
									break;
								}
								else
								{
									va_sleep(1);
									count++;
								}
							}
						#endif
					}
					else
					{
						#ifdef __ABIO_UNIX
							if (va_msgsnd(transCK, commandData.mountStatus.mtype, &command, sizeof(struct ck), 0) < 0)
							{
								// ������ module�� message ���ۿ� ������ ����̴�.
								// �� ��� message�� ������ source process�� message�� ������� �Ѵ�.
								// ���� ����(VIRBAK ABIO v2.2.x)������ message�� ������ source process����
								// ���ۿ� ������ message�� �޾Ƽ� ó���ϴ� ��ƾ�� ������ �Ǿ����� �ʱ⶧����
								// ���� �������� ó���ϴ� ������ �Ѵ�.
							}
						#elif __ABIO_WIN
							msgInfo = (struct msg_arg *)malloc(sizeof(struct msg_arg));
							msgInfo->pmtype = commandData.mountStatus.mtype;
							memcpy(&msgInfo->command, &command, sizeof(struct ck));
							
							count = 0;
							while (_beginthread(SendMessageToNamedPipe, 0, (void *)msgInfo) == (uintptr_t)-1)
							{
								if (count == THREAD_TIME_OUT)
								{
									break;
								}
								else
								{
									va_sleep(1);
									count++;
								}
							}
						#endif
					}
				}
			}
		}
	}
	
	while (newMsg != NULL)
	{
		oldMsg = newMsg;
		newMsg = newMsg->next;
		
		va_free(oldMsg);
	}
	
	
	printf("ProcessMessage has been closed.\n");
	PrintDaemonLog("ProcessMessage has been closed.");

	CloseThread();
	
	
	#ifdef __ABIO_UNIX
		return NULL;
	#else
		_endthreadex(0);
		return 0;
	#endif
}

int AppendMessageToMessageBuffer(struct ck * requestCommand, char * connectionIP)
{
	struct msgbuf_ck * t;
	
	
	
	// message�� message buffer�� �����Ѵ�.
	if (va_grab_semaphore(semidBuffer) < 0)
	{
		return -1;
	}
	
	if ((t = (struct msgbuf_ck *)malloc(sizeof(struct msgbuf_ck))) != NULL)
	{
		memset(t, 0, sizeof(struct msgbuf_ck));
		
		memcpy(&t->msg, requestCommand, sizeof(struct ck));
		if (connectionIP != NULL)
		{
			strcpy(t->connectionIP, connectionIP);
		}
		
		t->next = NULL;
		lastMsg->next = t;
		lastMsg = t;
		
		// message processor���� message�� ������ �˸���.
		if (va_release_semaphore(semidThread) < 0)
		{
			return -1;
		}
	}
	
	if (va_release_semaphore(semidBuffer) < 0)
	{
		return -1;
	}
	
	
	return 0;
}

int FindAction(struct ck * command, char * requestModuleName, char * action)
{
	int i;
	
	
	
	// request, reply�� ���� action list���� ��� module�� ã���� �����Ѵ�.
	// message�� ó�����ֱ� ��û�� ��� action list���� message�� ���� command(request command)�� ã�´�.
	if (command->request == 1)
	{
		for (i = 0; i < actionListNumber; i++)
		{
			if (!strcmp(actionList[i].moduleName, command->requestCommand))
			{
				strcpy(requestModuleName, actionList[i].moduleName);
				strcpy(action, actionList[i].moduleFile);
				
				break;
			}
		}
	}
	// ó���� message�� ���� ������ �����ִ� ��� ���ʿ� message�� ó�����ֱ� ��û�� command(execute command)�� action list���� ã�´�.
	else
	{
		for (i = 0; i < actionListNumber; i++)
		{
			if (!strcmp(actionList[i].moduleName, command->executeCommand))
			{
				strcpy(requestModuleName, actionList[i].moduleName);
				strcpy(action, actionList[i].moduleFile);
				
				break;
			}
		}
	}
	
	
	if (action[0] != '\0')
	{
		//ky88 for debug...
		if ( 1 < debugLevel) {
			PrintDaemonLog("The module for \"%s\" \'%s\' \'%s\' found. source : %s(%s:%s), destination : %s(%s:%s)", command->requestCommand,
						command->subCommand, command->executeCommand,
						command->sourceNodename,command->sourceIP,command->sourcePort, command->destinationNodename,command->destinationIP,command->destinationPort);
		}
		return 0;
	}
	else
	{
		printf("The module for \"%s\" is not installed.\n", command->requestCommand);
		PrintDaemonLog("The module for \"%s\" is not installed.", command->requestCommand);
		
		return -1;
	}
}

int GetMessageType()
{
	int i;

	// get a available message type
	if (nextMsgtype == msgtypeNumber)
	{
		i = CK_MESSAGE_TYPE_INIT;
	}
	else
	{
		i = nextMsgtype;
	}
	
	for ( ; i < msgtypeNumber; i++)
	{
		if (msgtype[i].use == 0)
		{
			msgtype[i].use = 1;
			nextMsgtype = i + 1;
			
			break;
		}
	}
	
	
	if (i == msgtypeNumber)
	{
		return -1;
	}
	else
	{
		return i;
	}
}

#ifdef __ABIO_UNIX
	//#issue 184 defunct process
	void del_defunct_process(int signo, siginfo_t *info, void *uarg)
	{
		int i;
		
			
		if(1 < debugLevel)
		{
			printf("Reset msg type. - %d",info->si_pid);
		}

			for (i = CK_MESSAGE_TYPE_INIT; i < msgtypeNumber; i++)
			{
			if (msgtype[i].pid ==  info->si_pid)
				{
					msgtype[i].use = 0;
					msgtype[i].pid = 0;
					
					va_clear_message_transfer(transCK,i);

					break;
				}
			}
	}

	int SendMessageToModule(char * action, int pmtype, struct ck * command)
	{
		int pid;
		
		char strMtype[NUMBER_STRING_LENGTH];
		
		//#issue 184 defunct process
		//signal(SIGCHLD,(void *)del_defunct_process);
		
		// unix������ child process�� message queue id�� message type�� �������ش�.
		memset(strMtype, 0, sizeof(strMtype));
		va_itoa(pmtype, strMtype);
		
		// child process�� �����Ѵ�.
		if ((pid = fork()) == -1)
		{
			return -1;
		}
		// child process������ ������ command�� �����Ѵ�.
		else if (pid == 0)
		{
			processType = CHILD_PROCESS;
			
			va_close_socket(sockCK, 0);
			
			
			// action list���� ã�� command�� �����Ѵ�.
			if (execl(action, action, strTransCK, strMtype, NULL) < 0)
			{
				return -2;
			}
		}
		// parent process������ message transfer�� message�� ���޽����ش�.
		else
		{
			// child process id�� �����Ѵ�.
			msgtype[pmtype].pid = pid;
			
			// message queue�� message�� ����.
			if (va_msgsnd(transCK, pmtype, command, sizeof(struct ck), 0) < 0)
			{
				return -3;
			}
		}
		
		
		return 0;
	}
#elif __ABIO_WIN
	int SendMessageToModule(char * action, int pmtype, struct ck * command)
	{
		STARTUPINFOW si;
		PROCESS_INFORMATION pi;
		
		char appname[MAX_PATH_LENGTH];
		wchar_t * convertAppname;
		int convertAppnameSize;
		
		struct msg_arg * msgInfo;
		
		int count;
		
		
		
		// initialize variables
		memset(&si, 0, sizeof(si));
		memset(&pi, 0, sizeof(pi));
		
		memset(appname, 0, sizeof(appname));
		
		
		// windows������ child process�� ������ message type�� �ش��ϴ� named pipe instance�� handle�� �������ش�.
		#if defined(__ABIO_WIN) && defined(__ABIO_64)
			sprintf(appname, "\"%s\" \"%dI64\" \"%d\"", action, (__int64)msgtype[pmtype].transid, pmtype);
		#else
			sprintf(appname, "\"%s\" \"%d\" \"%d\"", action, (int)msgtype[pmtype].transid, pmtype);
		#endif
		
		if (va_convert_string_from_utf8(ENCODING_UTF16_LITTLE, appname, (int)strlen(appname), (void **)&convertAppname, &convertAppnameSize) == 0)
		{
			return -2;
		}
		
		
		// child process�� �����Ѵ�.
		if (CreateProcessW(NULL, convertAppname, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi) != 0)
		{
			// child process information�� �����Ѵ�.
			msgtype[pmtype].hProcess = pi.hProcess;
			msgtype[pmtype].hThread = pi.hThread;
			
			// child process�� message�� �����Ѵ�.
			msgInfo = (struct msg_arg *)malloc(sizeof(struct msg_arg));
			msgInfo->pmtype = pmtype;
			memcpy(&msgInfo->command, command, sizeof(struct ck));
			
			count = 0;
			while (_beginthread(SendMessageToNamedPipe, 0, (void *)msgInfo) == (uintptr_t)-1)
			{
				// thread�� ������ �ð����� ��������� ������ ���μ����� �����Ų��.
				if (count == THREAD_TIME_OUT)
				{
					if (TerminateProcess(msgtype[pmtype].hProcess, 2) != 0)
					{
						WaitForSingleObject(msgtype[pmtype].hProcess, INFINITE);
						CloseHandle(msgtype[pmtype].hProcess);
						CloseHandle(msgtype[pmtype].hThread);
					}
					
					msgtype[pmtype].hProcess = NULL;
					msgtype[pmtype].hThread = NULL;
					msgtype[pmtype].use = 0;
					
					break;
				}
				else
				{
					va_sleep(1);
					count++;
				}
			}
		}
		else
		{
			va_free(convertAppname);
			
			return -2;
		}
		
		va_free(convertAppname);
		
		
		return 0;
	}
	
	void SendMessageToNamedPipe(void * arg)
	{
		struct msg_arg msgInfo;
		
		int retry_count = 0;
		
		// windows�� message transfer�� named pipe�� connection base�� �����ϱ� ������ message�� ���޵��� ������ �� 
		// CK�� block�Ǵ� ���� �����ϱ� ���ؼ� ���� named pipe�� message�� �����ϴ� �κ��� thread�� ���� ó���Ѵ�.
		
		// child process�� message�� �����Ѵ�.
		memcpy(&msgInfo, arg, sizeof(struct msg_arg));
		va_free(arg);
		
		/*
		RETRY_MSGSND:
		if ( va_msgsnd(0, msgInfo.pmtype, &msgInfo.command, sizeof(struct ck), 0) < 0) 
		{
			//ky88 ...
			if (retry_count < 5)
			{
				PrintDaemonLog("va_msgsnd(%d, %s, ...) failed.retry %d.", msgInfo.pmtype, msgInfo.command.requestCommand , retry_count);
				retry_count++;
				va_sleep_random_range(1,5);
				goto RETRY_MSGSND;
			}
		}
		*/
		for(retry_count = 0; retry_count < 5; retry_count++)
		{
			if (va_msgsnd(0, msgInfo.pmtype, &msgInfo.command, sizeof(struct ck), 0) < 0) 
			{
				PrintDaemonLog("va_msgsnd(%d, %s, ...) failed.retry %d.", msgInfo.pmtype, msgInfo.command.requestCommand , retry_count);
				va_sleep_random_range(1, 5);
				continue;
			}
			break;
		}
		
		// message ���޿� ����� �ڿ��� ȸ���Ѵ�.
		if (msgInfo.pmtype < msgtypeNumber)
		{
			if (msgtype[msgInfo.pmtype].hProcess != NULL)
			{
				WaitForSingleObject(msgtype[msgInfo.pmtype].hProcess, INFINITE);
				CloseHandle(msgtype[msgInfo.pmtype].hProcess);
				CloseHandle(msgtype[msgInfo.pmtype].hThread);
			}
			
			msgtype[msgInfo.pmtype].hProcess = NULL;
			msgtype[msgInfo.pmtype].hThread = NULL;
			msgtype[msgInfo.pmtype].use = 0;
		}
		
		
		_endthread();
	}
#endif

#ifdef __ABIO_UNIX
	va_thread_return_t WaitChild(void * arg)
	{
		int pid;
		int i;
		
		
		
		va_sleep(1);
		
		while (1)
		{
			// process�� ����Ǹ� message ���޿� ����� �ڿ��� ȸ���Ѵ�.
			if ((pid = waitpid(-1, NULL, WNOHANG)) > 0)
			{
				for (i = CK_MESSAGE_TYPE_INIT; i < msgtypeNumber; i++)
				{
					if (msgtype[i].pid == pid)
					{
						msgtype[i].pid = 0;
						msgtype[i].use = 0;			
						
						break;
					}
				}
			}
			else
			{
				if (transCK < 0)
				{
					break;
				}
				
				va_sleep(1);
			}
		}
		
		
		#ifdef __ABIO_UNIX
			return NULL;
		#else
			_endthreadex(0);
			return 0;
		#endif
	}
#endif

void CloseThread()
{
	struct ck command;
	
	
	
	memset(&command, 0, sizeof(struct ck));
	strcpy(command.requestCommand, "EXIT_CK");
	
	if (ckComplete == 0)
	{
		// GetMessageFromSocket() thread ����
		ckComplete = 1;
		
		// GetMessageFromTransfer() thread ����
		va_msgsnd(transCK, 1, &command, sizeof(struct ck), 0);
		
		// ProcessMessage thread ����
		va_release_semaphore(semidThread);
	}
}

void Exit()
{
	int i;
	
	
	
	CloseThread();
	
	// release a system resource
	va_close_socket(sockCK, 0);
	va_remove_message_transfer(transCK);
	va_remove_semaphore(semidThread);
	va_remove_semaphore(semidBuffer);

	#ifdef __ABIO_WIN
		if (msgtype != NULL)
		{
			for (i = CK_MESSAGE_TYPE_INIT; i < msgtypeNumber; i++)
			{
				va_remove_message_transfer(msgtype[i].transid);
			}
		}
	#endif
	
	
	va_free(actionList);
	
	for (i = 0; i < nodeipNumber; i++)
	{
		va_free(nodeip[i]);
	}
	va_free(nodeip);
	
	// kill all of child processes of the ck
	KillChildProcess();
	
	
	#ifdef __ABIO_WIN	
		//���� ����
		WSACleanup();
		
		#ifndef _DEBUG
			// ���� ����
			CK_set_status(SERVICE_STOPPED, 0);
		#endif
	#endif
	
	printf("The communication kernel was stoped.\n");
	PrintDaemonLog("The communication kernel was stoped.");
}

void KillChildProcess()
{
	
}

#ifdef __ABIO_WIN
	void CK_service_handler(DWORD fdwcontrol)
	{
		if (fdwcontrol == g_nowstate)
		{
			return ;
		}
		
		switch (fdwcontrol)
		{
			case SERVICE_CONTROL_STOP :
			{
				CK_set_status(SERVICE_STOP_PENDING, 0);
				
				CloseThread();
				
				break;
			}
			
			case SERVICE_CONTROL_INTERROGATE :
			{
				
			}
			
			default :
			{
				CK_set_status(g_nowstate, SERVICE_ACCEPT_STOP);
				break;
			}
		}
	}
	
	void CK_set_status(DWORD dwstate, DWORD dwaccept)
	{
		#ifndef _DEBUG
			SERVICE_STATUS ss;
			ss.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
			ss.dwCurrentState = dwstate;
			ss.dwControlsAccepted = dwaccept;
			ss.dwWin32ExitCode = 0;
			ss.dwServiceSpecificExitCode = 0;
			ss.dwCheckPoint = 0;
			ss.dwWaitHint = 0;

			g_nowstate = dwstate;
			SetServiceStatus(g_hSrv, &ss);
		#endif
	}
#endif

void PrintDaemonLog(char * message, ...)
{
	time_t current_t;
	char logTime[TIME_STAMP_LENGTH];
	
	char logMessage[USER_LOG_MESSAGE_LENGTH];
	va_list argptr;
	
	char * serviceLogMessage;
	
	
	
	// initialize variables
	memset(logTime, 0, sizeof(logTime));
	memset(logMessage, 0, sizeof(logMessage));
	serviceLogMessage = NULL;
	
	
	current_t = time(NULL);
	va_make_time_stamp(current_t, logTime, TIME_STAMP_TYPE_EXTERNAL_EN);
	
	va_start(argptr, message);
	vsprintf(logMessage, message, argptr);
	va_end(argptr);
	
	serviceLogMessage = (char *)malloc(sizeof(char) * USER_LOG_MESSAGE_LENGTH);
	memset(serviceLogMessage, 0, USER_LOG_MESSAGE_LENGTH);
	sprintf(serviceLogMessage, "%s    %s", logTime, logMessage);
	
	va_append_text_file_lines(ABIOMASTER_CK_FOLDER, ABIOMASTER_CK_LOG_FILE, &serviceLogMessage, NULL, 1);
	
	va_free(serviceLogMessage);
}
