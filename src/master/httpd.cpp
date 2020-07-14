#include "require.h"
#include "abio.h"
#include "lib.h"
#include "vendor.h"
#include "httpd.h"
#include "migrate_v25_to_v30.h"

typedef struct {
  long tv_sec;
  long tv_usec;
}timevals;

// start of variables for abio library comparability
int tapeDriveTimeOut;
int tapeDriveDelayTime;
// end of variables for abio library comparability


char ABIOMASTER_CK_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_MASTER_SERVER_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_DATA_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_CATALOG_DB_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_FILE_LIST_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_LOG_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_VOLUME_DB_FOLDER[MAX_PATH_LENGTH];
char ABIOMASTER_REPORT_FOLDER[MAX_PATH_LENGTH];


//////////////////////////////////////////////////////////////////////////////////////////////////
// ABIO log option
int requestLogLevel;
char logJobID[JOB_ID_LENGTH];
int logModuleNumber;


//////////////////////////////////////////////////////////////////////////////////////////////////
// master server configuration
char masterNodename[NODE_NAME_LENGTH];		// master server node name
char masterIP[IP_LENGTH];					// master server ip address
char masterPort[PORT_LENGTH];				// master server communication kernel port
char masterLogPort[PORT_LENGTH];			// master server httpd logger port
char lcPort[PORT_LENGTH];					// master server library controler port
char scPort[PORT_LENGTH];					// master server scheduler port
char hdPort[PORT_LENGTH];					// master server httpd port
char loginUserName[NODE_NAME_LENGTH];		// master server login User Name


char ** nodeipList;							// master server ip address list
int nodeipListNumber;						// number of master server ip address list

struct portRule * portRule;					// VIRBAK ABIO IP Table 목록
int portRuleNumber;							// VIRBAK ABIO IP Table 개수

__int64 jobSuspendTime;						// job suspend time
__int64 eventSuspendTime;					// event suspend time

int connectionRestriction;					// 특정 ip로부터의 connection 제한을 사용할 것인지 여부. 0 : 사용안함. 1 : 사용함.
char ** allowedIP;							// 접속이 허용된 ip 목록
int allowedIPNumber;						// number of allowed ips

int volumeBlockSize;						// volume block size

int jobLogRetention;						// job log, client의 backup/restore log의 보관 기간


//////////////////////////////////////////////////////////////////////////////////////////////////
// variables for httpd daemon control
va_sock_t httpdSock;			// httpd server socket
va_sock_t httpdLogSock;			// httpd logger server socket
int httpdComplete;

va_sem_t semidLog;				// semaphore for running job log thread synchronization


//////////////////////////////////////////////////////////////////////////////////////////////////
// variables for license
char hostid[HOST_ID_LENGTH];				// master server host id
struct licenseSummary * licenseSummary;		// license summary
int licenseSummaryNumber;					// license summary number
__int64 validationDate;						// validation date for evaluation key
va_sem_t semidLicense;						// semaphore for license summary synchronization


//////////////////////////////////////////////////////////////////////////////////////////////////
// variables for httpd daemon request
struct httpdRequest * firstLogRequest;
struct httpdRequest * lastLogRequest;

int DebugLevel=0;

// #issue 170 : Retry Backup
//////////////////////////////////////////////////////////////////////////////////////////////////
// master retry options
int maxRetryCount;
int retryPeriodMins;
int viewFileCount;
#ifdef __ABIO_WIN
	SERVICE_STATUS_HANDLE g_hSrv;
	DWORD g_nowstate;
#endif

int GetRegisterdHostID( char * hostid);

int main(int argc, char ** argv)
{
	#ifdef __ABIO_UNIX
		char * convertFilename;
		int convertFilenameSize;
		
		
		
		if (va_convert_string_to_utf8(ENCODING_UNKNOWN, argv[0], (int)strlen(argv[0]), (void **)&convertFilename, &convertFilenameSize) == 0)
		{
			return 2;
		}
		
		StartHttpd(convertFilename);
		
		va_free(convertFilename);
	#elif __ABIO_WIN
		#ifdef _DEBUG
			StartHttpd();
		#else
			SERVICE_TABLE_ENTRYW ste[] = {{ABIOMASTER_HTTPD_SERVICE_NAME, (LPSERVICE_MAIN_FUNCTIONW)StartHttpd}, {NULL, NULL}};
			
			StartServiceCtrlDispatcherW(ste);
		#endif
	#endif
	
	return 0;
}

#ifdef __ABIO_UNIX
	int StartHttpd(char * filename)
	{
		printf("Initialize a process to start the httpd service.\n");
		
		// 각 시스템에 맞게 process를 초기화하고, 실행에 필요한 설정 값들을 읽어온다.
		if (InitProcess(filename) < 0)
		{
			// process 초기화에 실패한 경우 process를 종료한다.
			// 자원 부족으로 인해 daemon이나 service로 만드는데 실패한 경우나, process 실행에 필요한 설정 값들이 잘못 지정되거나 없는 경우에 발생한다.
			// 이런 경우가 발생하면 process를 종료한다.
			printf("It could not start up the httpd service.\n");
			PrintDaemonLog("It could not start up the httpd service.");
			
			Exit();
			
			return 2;
		}
		
		
		// httpd 설정을 초기화 한다.
		if (InitHttpd() < 0)
		{
			// httpd 설정 초기화에 실패한 경우 사용한 자원들을 반납하고 종료한다.
			printf("It could not start up the httpd service.\n");
			PrintDaemonLog("It could not start up the httpd service.");
			
			Exit();
			
			return 2;
		}
		
		
		// process를 daemon이나 service 형태로 만든다.
		if (va_make_service() < 0)
		{
			printf("It failed to make the httpd service as a service.\n");
			printf("It could not start up the httpd service.\n");
			PrintDaemonLog("It failed to make the httpd service as a service.");
			PrintDaemonLog("It could not start up the httpd service.");
			
			Exit();
			
			return 2;
		}
		
		printf("The httpd service was started.\n");
		PrintDaemonLog("The httpd service was started. pid : %d",va_getpid());
		
		
		// httpd을 시작한다.
		if (Httpd() < 0)
		{
			Exit();
			
			return 2;
		}
		
		
		Exit();
		
		return 0;
	}
#elif __ABIO_WIN
	int StartHttpd()
	{
		// process를 daemon이나 service 형태로 만든다.
		#ifndef _DEBUG
			if ((g_hSrv = RegisterServiceCtrlHandlerW(ABIOMASTER_HTTPD_SERVICE_NAME, (LPHANDLER_FUNCTION)Httpd_service_handler)) == 0)
			{
				return 2;
			}
		#endif
		
		// 서비스 실행 중
		Httpd_set_status(SERVICE_START_PENDING, SERVICE_ACCEPT_STOP);
		
		
		// 각 시스템에 맞게 process를 초기화하고, 실행에 필요한 설정 값들을 읽어온다.
		if (InitProcess() < 0)
		{
			// process 초기화에 실패한 경우 process를 종료한다.
			// 자원 부족으로 인해 daemon이나 service로 만드는데 실패한 경우나, process 실행에 필요한 설정 값들이 잘못 지정되거나 없는 경우에 발생한다.
			// 이런 경우가 발생하면 process를 종료한다.
			PrintDaemonLog("It could not start up the httpd service.");
			
			Exit();
			
			return 2;
		}
		
		
		// httpd 설정을 초기화 한다.
		if (InitHttpd() < 0)
		{
			// httpd 설정 초기화에 실패한 경우 사용한 자원들을 반납하고 종료한다.
			PrintDaemonLog("It could not start up the httpd service.");
			
			Exit();
			
			return 2;
		}
		
		// 서비스 시작됨
		Httpd_set_status(SERVICE_RUNNING, SERVICE_ACCEPT_STOP);
		PrintDaemonLog("The httpd service was started. pid : %d",va_getpid());
		
		
		// httpd을 시작한다.
		if (Httpd() < 0)
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
	#ifdef __ABIO_WIN
		WSADATA wsaData;
	#endif
	
	int i;
	
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 1. 전역 변수들을 초기화한다.
	
	memset(ABIOMASTER_CK_FOLDER, 0, sizeof(ABIOMASTER_CK_FOLDER));
	memset(ABIOMASTER_MASTER_SERVER_FOLDER, 0, sizeof(ABIOMASTER_MASTER_SERVER_FOLDER));
	memset(ABIOMASTER_DATA_FOLDER, 0, sizeof(ABIOMASTER_DATA_FOLDER));
	memset(ABIOMASTER_FILE_LIST_FOLDER, 0, sizeof(ABIOMASTER_FILE_LIST_FOLDER));
	memset(ABIOMASTER_VOLUME_DB_FOLDER, 0, sizeof(ABIOMASTER_VOLUME_DB_FOLDER));
	memset(ABIOMASTER_CATALOG_DB_FOLDER, 0, sizeof(ABIOMASTER_CATALOG_DB_FOLDER));
	memset(ABIOMASTER_LOG_FOLDER, 0, sizeof(ABIOMASTER_LOG_FOLDER));
	memset(ABIOMASTER_REPORT_FOLDER, 0, sizeof(ABIOMASTER_REPORT_FOLDER));
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// ABIO log option
	requestLogLevel = LOG_LEVEL_JOB;
	memset(logJobID, 0, sizeof(logJobID));
	logModuleNumber = MODULE_UNKNOWN;
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// master server configuration
	memset(masterNodename, 0, sizeof(masterNodename));
	memset(masterIP, 0, sizeof(masterIP));
	memset(masterPort, 0, sizeof(masterPort));
	memset(lcPort, 0, sizeof(lcPort));
	memset(scPort, 0, sizeof(scPort));
	memset(hdPort, 0, sizeof(hdPort));
	memset(masterLogPort, 0, sizeof(masterLogPort));
	memset(loginUserName, 0, sizeof(loginUserName));

	nodeipList = NULL;
	nodeipListNumber = 0;
	
	portRule = NULL;
	portRuleNumber = 0;
	
	jobSuspendTime = 0;
	eventSuspendTime = 0;
	
	connectionRestriction = 0;
	allowedIP = NULL;
	allowedIPNumber = 0;
	
	volumeBlockSize = 0;
	
	jobLogRetention = 0;
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// variables for httpd daemon control
	httpdSock = -1;
	httpdLogSock = -1;
	httpdComplete = 0;
	
	semidLog = (va_sem_t)-1;
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// variables for license
	memset(hostid, 0, sizeof(hostid));
	licenseSummary = NULL;
	licenseSummaryNumber = 0;
	semidLicense = (va_sem_t)-1;
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// variables for library controller request
	firstLogRequest = NULL;
	lastLogRequest = NULL;
	
	// #issue 170 : Retry Backup
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// master retry options
	maxRetryCount = 0;
	retryPeriodMins = 1;
	viewFileCount = 1000;
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 2. 각 시스템에 맞게 process를 초기화한다.
	
	// process resource limit를 설정한다.
	va_setrlimit();
	
	// set module number for debug log of ABIO common library
	logModuleNumber = MODULE_MASTER_HTTPD;
	
	// 현재 작업 디렉토리를 읽어온다.
	#ifdef __ABIO_UNIX
		// 절대 경로로 실행된 경우 절대 경로에서 master server folder를 읽어온다.
		if (filename[0] == FILE_PATH_DELIMITER)
		{
			for (i = (int)strlen(filename) - 1; i > 0; i--)
			{
				if (filename[i] == FILE_PATH_DELIMITER)
				{
					strncpy(ABIOMASTER_MASTER_SERVER_FOLDER, filename, i);
					
					break;
				}
			}
		}
		// 상대 경로로 실행된 경우 master server folder로 이동한뒤 작업 디렉토리를 읽어온다.
		else
		{
			// 상대 경로상의 master server folder를 읽어온다.
			for (i = (int)strlen(filename) - 1; i > 0; i--)
			{
				if (filename[i] == FILE_PATH_DELIMITER)
				{
					strncpy(ABIOMASTER_MASTER_SERVER_FOLDER, filename, i);
					
					break;
				}
			}
			
			// master server folder로 이동한다.
			if (chdir(ABIOMASTER_MASTER_SERVER_FOLDER) < 0)
			{
				return -1;
			}
			
			// 작업 디렉토리를 읽어온다.
			memset(ABIOMASTER_MASTER_SERVER_FOLDER, 0, sizeof(ABIOMASTER_MASTER_SERVER_FOLDER));
			if (va_get_working_directory(ABIOMASTER_MASTER_SERVER_FOLDER) < 0)
			{
				return -1;
			}
		}
	#elif __ABIO_WIN
		if (va_get_working_directory(ABIOMASTER_MASTER_SERVER_FOLDER) < 0)
		{
			return -1;
		}
	#endif
	
	// set the ck and client folder
	for (i = (int)strlen(ABIOMASTER_MASTER_SERVER_FOLDER) - 1; i > 0; i--)
	{
		if (ABIOMASTER_MASTER_SERVER_FOLDER[i] == FILE_PATH_DELIMITER)
		{
			strncpy(ABIOMASTER_CK_FOLDER, ABIOMASTER_MASTER_SERVER_FOLDER, i + 1);
			strcat(ABIOMASTER_CK_FOLDER, "ck");
			
			break;
		}
	}
	
	sprintf(ABIOMASTER_DATA_FOLDER, "%s%c%s", ABIOMASTER_MASTER_SERVER_FOLDER, FILE_PATH_DELIMITER, "data");
	
	PrintDaemonLog("Initialize a process to start the httpd service.");
	
	#ifdef __ABIO_WIN
		// windows socket을 open한다.
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) 
		{
			PrintDaemonLog("It failed to start up the windows socket.");
			
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
	// 4. VA_2_5_1에서 VA_3_0_0으로 Migration한다.
	
	if (MigrateAbioV25ToV30() != 0)
	{
		return -1;
	}
	
	
	return 0;
}

int GetModuleConfiguration()
{
	struct confOption * moduleConfig;
	int moduleConfigNumber;
	
	int i;
	
	
	
	if (va_load_conf_file(ABIOMASTER_CK_FOLDER, ABIOMASTER_CK_CONFIG, &moduleConfig, &moduleConfigNumber) > 0)
	{
		for (i = 0; i < moduleConfigNumber; i++)
		{
			if (!strcmp(moduleConfig[i].optionName, NODE_NAME_OPTION_NAME))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(masterNodename) - 1)
				{
					strcpy(masterNodename, moduleConfig[i].optionValue);
				}
			}
			else if (!strcmp(moduleConfig[i].optionName, NODE_IP_OPTION_NAME))
			{
				if (nodeipListNumber % DEFAULT_MEMORY_FRAGMENT == 0)
				{
					nodeipList = (char **)realloc(nodeipList, sizeof(char *) * (nodeipListNumber + DEFAULT_MEMORY_FRAGMENT));
					memset(nodeipList + nodeipListNumber, 0, sizeof(char *) * DEFAULT_MEMORY_FRAGMENT);
				}
				nodeipList[nodeipListNumber] = (char *)malloc(sizeof(char) * (strlen(moduleConfig[i].optionValue) + 1));
				memset(nodeipList[nodeipListNumber], 0, sizeof(char) * (strlen(moduleConfig[i].optionValue) + 1));
				strcpy(nodeipList[nodeipListNumber], moduleConfig[i].optionValue);
				nodeipListNumber++;
			}
			else if (!strcmp(moduleConfig[i].optionName, CK_PORT_OPTION_NAME))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(masterPort) - 1)
				{
					strcpy(masterPort, moduleConfig[i].optionValue);
				}
			}
		}
		
		
		for (i = 0; i < moduleConfigNumber; i++)
		{
			va_free(moduleConfig[i].optionName);
			va_free(moduleConfig[i].optionValue);
		}
		va_free(moduleConfig);
		
		
		// set master server main ip address
		if (nodeipListNumber > 0)
		{
			strcpy(masterIP, nodeipList[0]);
		}
	}
	else
	{
		printf("It failed to open a configuration file \"%s\".\n", ABIOMASTER_CK_CONFIG);
		PrintDaemonLog("It failed to open a configuration file \"%s\".", ABIOMASTER_CK_CONFIG);
		
		return -1;
	}
	
	
	if (va_load_conf_file(ABIOMASTER_MASTER_SERVER_FOLDER, ABIOMASTER_MASTER_CONFIG, &moduleConfig, &moduleConfigNumber) > 0)
	{
		for (i = 0; i < moduleConfigNumber; i++)
		{
			if (!strcmp(moduleConfig[i].optionName, MASTER_CATALOG_DB_FOLDER_OPTION_NAME))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(ABIOMASTER_CATALOG_DB_FOLDER) - 1)
				{
					strcpy(ABIOMASTER_CATALOG_DB_FOLDER, moduleConfig[i].optionValue);
				}
			}
			else if (!strcmp(moduleConfig[i].optionName, MASTER_FILE_LIST_FOLDER_OPTION_NAME))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(ABIOMASTER_FILE_LIST_FOLDER) - 1)
				{
					strcpy(ABIOMASTER_FILE_LIST_FOLDER, moduleConfig[i].optionValue);
				}
			}
			else if (!strcmp(moduleConfig[i].optionName, MASTER_LOG_FOLDER_OPTION_NAME))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(ABIOMASTER_LOG_FOLDER) - 1)
				{
					strcpy(ABIOMASTER_LOG_FOLDER, moduleConfig[i].optionValue);
				}
			}
			else if (!strcmp(moduleConfig[i].optionName, MASTER_VOLUME_DB_FOLDER_OPTION_NAME))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(ABIOMASTER_VOLUME_DB_FOLDER) - 1)
				{
					strcpy(ABIOMASTER_VOLUME_DB_FOLDER, moduleConfig[i].optionValue);
				}
			}
			else if (!strcmp(moduleConfig[i].optionName, MASTER_REPORT_FOLDER_OPTION_NAME))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(ABIOMASTER_REPORT_FOLDER) - 1)
				{
					strcpy(ABIOMASTER_REPORT_FOLDER, moduleConfig[i].optionValue);
				}
			}
			else if (!strcmp(moduleConfig[i].optionName, MASTER_LIBRARY_CONTROLER_PORT_OPTION_NAME))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(lcPort) - 1)
				{
					strcpy(lcPort, moduleConfig[i].optionValue);
				}
			}
			else if (!strcmp(moduleConfig[i].optionName, MASTER_SCHEDULER_PORT_OPTION_NAME))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(scPort) - 1)
				{
					strcpy(scPort, moduleConfig[i].optionValue);
				}
			}
			else if (!strcmp(moduleConfig[i].optionName, MASTER_HTTPD_PORT_OPTION_NAME))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(hdPort) - 1)
				{
					strcpy(hdPort, moduleConfig[i].optionValue);
				}
			}
			else if (!strcmp(moduleConfig[i].optionName, MASTER_HTTPD_LOGGER_PORT_OPTION_NAME))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(masterLogPort) - 1)
				{
					strcpy(masterLogPort, moduleConfig[i].optionValue);
				}
			}
			else if (!strcmp(moduleConfig[i].optionName, ABIOMASTER_PORT_RULE_OPTION_NAME))
			{
				va_parse_port_rule(moduleConfig[i].optionValue, &portRule, &portRuleNumber);
			}
			else if (!strcmp(moduleConfig[i].optionName, MASTER_VOLUME_BLOCK_SIZE_OPTION_NAME))
			{
				volumeBlockSize = atoi(moduleConfig[i].optionValue);
			}
			else if (!strcmp(moduleConfig[i].optionName, MASTER_JOB_SUSPEND_TIME_OPTION_NAME))
			{
				jobSuspendTime = atoi(moduleConfig[i].optionValue);
			}
			else if (!strcmp(moduleConfig[i].optionName, MASTER_EVENT_SUSPEND_TIME_OPTION_NAME))
			{
				eventSuspendTime = atoi(moduleConfig[i].optionValue);
			}
			else if (!strcmp(moduleConfig[i].optionName, MASTER_CONNECTION_RESTRICTION_OPTION_NAME))
			{
				connectionRestriction = atoi(moduleConfig[i].optionValue);
			}
			else if (!strcmp(moduleConfig[i].optionName, MASTER_ALLOWED_IP_OPTION_NAME))
			{
				if (allowedIPNumber % DEFAULT_MEMORY_FRAGMENT == 0)
				{
					allowedIP = (char **)realloc(allowedIP, sizeof(char *) * (allowedIPNumber + DEFAULT_MEMORY_FRAGMENT));
					memset(allowedIP + allowedIPNumber, 0, sizeof(char *) * DEFAULT_MEMORY_FRAGMENT);
				}
				allowedIP[allowedIPNumber] = (char *)malloc(sizeof(char) * (strlen(moduleConfig[i].optionValue) + 1));
				memset(allowedIP[allowedIPNumber], 0, sizeof(char) * (strlen(moduleConfig[i].optionValue) + 1));
				strcpy(allowedIP[allowedIPNumber], moduleConfig[i].optionValue);
				allowedIPNumber++;
			}
			else if (!strcmp(moduleConfig[i].optionName, MASTER_JOB_LOG_RETENTION_OPTION_NAME))
			{
				jobLogRetention = atoi(moduleConfig[i].optionValue);
			}
			else if (!strcmp(moduleConfig[i].optionName, DEBUG_LEVEL))
			{
				DebugLevel=atoi(moduleConfig[i].optionValue);
			}
			// #issue 170 : Retry Backup
			else if (!strcmp(moduleConfig[i].optionName, MASTER_MAX_RETRY_COUNT_OPTION_NAME))
			{
				maxRetryCount = atoi(moduleConfig[i].optionValue);
			}
			else if (!strcmp(moduleConfig[i].optionName, MASTER_RETRY_PERIOD_MINS_OPTION_NAME))
			{
				retryPeriodMins = atoi(moduleConfig[i].optionValue);
			}
			//#issue182,187
			else if (!strcmp(moduleConfig[i].optionName, MASTER_HOST_ID_OPTION_NAME))
			{
				strcpy(hostid, moduleConfig[i].optionValue);
			}
			else if (!strcmp(moduleConfig[i].optionName, LOGIN_ADMIN_USER))
			{
				if ((int)strlen(moduleConfig[i].optionValue) < sizeof(loginUserName) - 1)
				{
					strcpy(loginUserName, moduleConfig[i].optionValue);
				}
			}
			else if (!strcmp(moduleConfig[i].optionName, VIEW_FILE_COUNT))
			{
				viewFileCount = atoi(moduleConfig[i].optionValue);
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
		printf("It failed to open a configuration file \"%s\".\n", ABIOMASTER_MASTER_CONFIG);
		PrintDaemonLog("It failed to open a configuration file \"%s\".", ABIOMASTER_MASTER_CONFIG);
		
		return -1;
	}
	
	
	if (volumeBlockSize == 0 || volumeBlockSize % DSIZ != 0 || volumeBlockSize < 16384)
	{
		volumeBlockSize = DEFAULT_VOLUME_BLOCK_SIZE;
	}
	
	if (jobSuspendTime < 0)
	{
		jobSuspendTime = RUNNING_JOB_SUSPEND_TIME;
	}
	
	if (eventSuspendTime < 0)
	{
		eventSuspendTime = RUNNING_EVENT_SUSPEND_TIME;
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
	struct abio_file_stat file_stat;
	int configValid;
	int i;
	
	
	
	// initialize variables
	configValid = 1;
	
	
	// communication kernel의 설정값이 제대로 설정되었는지 확인한다.
	if (masterNodename[0] == '\0')
	{
		configValid = 0;
		
		printf("Node name is not set.\n");
		PrintDaemonLog("Node name is not set.");
	}
	
	if (nodeipListNumber == 0)
	{
		configValid = 0;
		
		printf("IP address is not set.\n");
		PrintDaemonLog("IP address is not set.");
	}
	else
	{
		for (i = 0; i < nodeipListNumber; i++)
		{
			if (va_is_valid_ip(nodeipList[i]) == 0)
			{
				configValid = 0;
				
				printf("IP address [%s] is not valid.\n", nodeipList[i]);
				PrintDaemonLog("IP address [%s] is not valid.", nodeipList[i]);
			}
		}
	}
	
	if (masterPort[0] == '\0')
	{
		configValid = 0;
		
		printf("Communication kernel service tcp/ip port is not set.\n");
		PrintDaemonLog("Communication kernel service tcp/ip port is not set.");
	}
	else if (va_is_valid_port(masterPort) == 0)
	{
		configValid = 0;
		
		printf("Communication kernel service tcp/ip port [%s] is invalid.\n", masterPort);
		PrintDaemonLog("Communication kernel service tcp/ip port [%s] is invalid.", masterPort);
	}
	
	
	// master server의 설정값이 제대로 설정되었는지 확인한다.
	if (ABIOMASTER_DATA_FOLDER[0] == '\0')
	{
		configValid = 0;
		
		printf("ABIO data folder is not set.\n");
		PrintDaemonLog("ABIO data folder is not set.\n");
	}
	else if (va_stat(NULL, ABIOMASTER_DATA_FOLDER, &file_stat) != 0)
	{
		configValid = 0;
		
		printf("ABIO data folder [%s] does not exist.\n", ABIOMASTER_DATA_FOLDER);
		PrintDaemonLog("ABIO data folder [%s] does not exist.", ABIOMASTER_DATA_FOLDER);
	}
	
	if (ABIOMASTER_FILE_LIST_FOLDER[0] == '\0')
	{
		configValid = 0;
		
		printf("ABIO backup filelist folder is not set.\n");
		PrintDaemonLog("ABIO backup filelist folder is not set.\n");
	}
	else if (va_stat(NULL, ABIOMASTER_FILE_LIST_FOLDER, &file_stat) != 0)
	{
		configValid = 0;
		
		printf("ABIO backup filelist folder [%s] does not exist.\n", ABIOMASTER_FILE_LIST_FOLDER);
		PrintDaemonLog("ABIO backup filelist folder [%s] does not exist.", ABIOMASTER_FILE_LIST_FOLDER);
	}
	
	if (ABIOMASTER_VOLUME_DB_FOLDER[0] == '\0')
	{
		configValid = 0;
		
		printf("ABIO volume db folder is not set.\n");
		PrintDaemonLog("ABIO volume db folder is not set.\n");
	}
	else if (va_stat(NULL, ABIOMASTER_VOLUME_DB_FOLDER, &file_stat) != 0)
	{
		configValid = 0;
		
		printf("ABIO volume db folder [%s] does not exist.\n", ABIOMASTER_VOLUME_DB_FOLDER);
		PrintDaemonLog("ABIO volume db folder [%s] does not exist.", ABIOMASTER_VOLUME_DB_FOLDER);
	}
	
	if (ABIOMASTER_CATALOG_DB_FOLDER[0] == '\0')
	{
		configValid = 0;
		
		printf("ABIO catalog db folder is not set.\n");
		PrintDaemonLog("ABIO catalog db folder is not set.\n");
	}
	else if (va_stat(NULL, ABIOMASTER_CATALOG_DB_FOLDER, &file_stat) != 0)
	{
		configValid = 0;
		
		printf("ABIO catalog db folder [%s] does not exist.\n", ABIOMASTER_CATALOG_DB_FOLDER);
		PrintDaemonLog("ABIO catalog db folder [%s] does not exist.", ABIOMASTER_CATALOG_DB_FOLDER);
	}
	
	if (ABIOMASTER_LOG_FOLDER[0] == '\0')
	{
		configValid = 0;
		
		printf("ABIO log folder is not set.\n");
		PrintDaemonLog("ABIO log folder is not set.\n");
	}
	else if (va_stat(NULL, ABIOMASTER_LOG_FOLDER, &file_stat) != 0)
	{
		configValid = 0;
		
		printf("ABIO log folder [%s] does not exist.\n", ABIOMASTER_LOG_FOLDER);
		PrintDaemonLog("ABIO log folder [%s] does not exist.", ABIOMASTER_LOG_FOLDER);
	}
	
	if (ABIOMASTER_REPORT_FOLDER[0] == '\0')
	{
		configValid = 0;
		
		printf("ABIO report folder is not set.\n");
		PrintDaemonLog("ABIO report folder is not set.\n");
	}
	else if (va_stat(NULL, ABIOMASTER_REPORT_FOLDER, &file_stat) != 0)
	{
		configValid = 0;
		
		printf("ABIO report folder [%s] does not exist.\n", ABIOMASTER_REPORT_FOLDER);
		PrintDaemonLog("ABIO report folder [%s] does not exist.", ABIOMASTER_REPORT_FOLDER);
	}
	
	if (lcPort[0] == '\0')
	{
		configValid = 0;
		
		printf("Storage controller service tcp/ip port is not set.\n");
		PrintDaemonLog("Storage controller service tcp/ip port is not set.\n");
	}
	else if (va_is_valid_port(lcPort) == 0)
	{
		configValid = 0;
		
		printf("Storage controller service tcp/ip port [%s] is invalid.\n", lcPort);
		PrintDaemonLog("Storage controller service tcp/ip port [%s] is invalid.", lcPort);
	}
	
	if (scPort[0] == '\0')
	{
		configValid = 0;
		
		printf("Scheduler service tcp/ip port is not set.\n");
		PrintDaemonLog("Scheduler service tcp/ip port is not set.\n");
	}
	else if (va_is_valid_port(scPort) == 0)
	{
		configValid = 0;
		
		printf("Scheduler service tcp/ip port [%s] is invalid.\n", scPort);
		PrintDaemonLog("Scheduler service tcp/ip port [%s] is invalid.", scPort);
	}
	
	if (hdPort[0] == '\0')
	{
		configValid = 0;
		
		printf("Httpd service tcp/ip port is not set.\n");
		PrintDaemonLog("Httpd service tcp/ip port is not set.\n");
	}
	else if (va_is_valid_port(hdPort) == 0)
	{
		configValid = 0;
		
		printf("Httpd service tcp/ip port [%s] is invalid.\n", hdPort);
		PrintDaemonLog("Httpd service tcp/ip port [%s] is invalid.", hdPort);
	}
	
	if (masterLogPort[0] == '\0')
	{
		configValid = 0;
		
		printf("Httpd log service tcp/ip port is not set.\n");
		PrintDaemonLog("Httpd log service tcp/ip port is not set.\n");
	}
	else if (va_is_valid_port(masterLogPort) == 0)
	{
		configValid = 0;
		
		printf("Httpd log service tcp/ip port [%s] is invalid.\n", masterLogPort);
		PrintDaemonLog("Httpd log service tcp/ip port [%s] is invalid.", masterLogPort);
	}
	
	
	return configValid;
}

int InitHttpd()
{
	char **hostidList;
	int hostidListNumber;
	char registedHostId[HOST_ID_LENGTH];

	int i;

	memset(registedHostId,0,HOST_ID_LENGTH);
	
	hostidList = (char **)malloc(sizeof(char *) * DEFAULT_MEMORY_FRAGMENT);
	memset(hostidList, 0, sizeof(char *) * DEFAULT_MEMORY_FRAGMENT);

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 1. httpd로 오는 요청을 받아들이기 위한 TCP/IP server socket을 만든다.
	
	if ((httpdSock = va_make_server_socket(hdPort, NULL, 1)) == -1)
	{
		printf("It failed to bind the tcp/ip port \"%s\".\n", hdPort);
		PrintDaemonLog("It failed to bind the tcp/ip port \"%s\".", hdPort);
		
		return -1;
	}
	
	if ((httpdLogSock = va_make_server_socket(masterLogPort, NULL, 1)) == -1)
	{
		printf("It failed to bind the tcp/ip port \"%s\".\n", masterLogPort);
		PrintDaemonLog("It failed to bind the tcp/ip port \"%s\".", masterLogPort);
		
		return -1;
	}
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 2. httpd daemon의 thread 동기화를 위한 semaphore를 만든다.
	
	if ((semidLog = va_make_semaphore(0)) == (va_sem_t)-1)
	{
		printf("It failed to make a semaphore.\n");
		PrintDaemonLog("It failed to make a semaphore.");
		
		return -1;
	}
	
	if ((semidLicense = va_make_semaphore(1)) == (va_sem_t)-1)
	{
		printf("It failed to make a semaphore.\n");
		PrintDaemonLog("It failed to make a semaphore.");
		
		return -1;
	}
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 3. message buffer를 초기화한다.
	
	firstLogRequest = (struct httpdRequest *)malloc(sizeof(struct httpdRequest));
	memset(firstLogRequest, 0, sizeof(struct httpdRequest));
	firstLogRequest->next = NULL;
	lastLogRequest = firstLogRequest;
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 4. master server를 등록한다.
	
	// master server의 host id를 읽어온다.
	//#issue182,187
	if(hostid[0] == '\0')
	{
		if ((hostidListNumber = va_get_macaddresslist(hostidList)) < 0)
		{
			//MAC list 를 불러 올수 없는 경우
			printf("It failed to get a machine identifier of the master server.\n");
			PrintDaemonLog("It failed to get a machine identifier of the master server.");

			#if defined(__ABIO_HP) || defined (__ABIO_SOLARIS)
				va_input_macaddress(hostid);
				va_set_hostID_Conf(hostid, ABIOMASTER_MASTER_SERVER_FOLDER, ABIOMASTER_MASTER_CONFIG);
			#else			
				return -1;
			#endif
		}
		else
		{	
			//MAC list 를 불러온 경우
			if ( 0 < GetRegisterdHostID(registedHostId))
			{
				//node.dat 파일이 있고 등록된 HostID 값이 있는 경우
				for(i =0 ; i < hostidListNumber ; i++)
				{
					// node.dat 에 등록된 HostID 와 일치하는 MAC 찾기
					if(strncmp(registedHostId,hostidList[i],strlen(registedHostId)) == 0)
					{
						strcpy(hostid,registedHostId);
						break;
					}
				}
			}
			else
			{
				//node.dat 파일이 없는 경우(삭제된 경우)
				//node.dat 에 등록된 HostID 값이 없는 경우
				strcpy(hostid,hostidList[0]);
			}
		}
	}
	else
	{
		//master.conf 에 "HostID" 값이 있는 경우
	}
	
	//master.conf 에 HostID" 값이 없다. && node.dat 에 등록된 Host 값과 일치하는 MAC 이 없다.
	if(hostid[0] == '\0')
	{
		// 종료
		printf("It failed to find a machine identifier of the master server.\n");
		PrintDaemonLog("It failed to find a machine identifier of the master server.");

		return -1;
	}

	
	// master server를 등록한다.
	if (RegisterMasterServer() < 0)
	{
		printf("It failed to register the master server.\n");
		PrintDaemonLog("It failed to register the master server.");
		
		return -1;
	}

	// user를 등록한다.
	if (RegisterMasterUser() < 0)
	{
		printf("It failed to user data.\n");
		PrintDaemonLog("It failed to user data.");
		
		return -1;
	}

	// user를 등록한다.
	if (RegisterMasterUserGroup() < 0)
	{
		printf("It failed to user data.\n");
		PrintDaemonLog("It failed to user data.");
		
		return -1;
	}
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 5. system volume pool을 등록한다.
	
	if (RegisterSystemVolumePool() < 0)
	{
		printf("It failed to register system volume pools.\n");
		PrintDaemonLog("It failed to register system volume pools.");
		
		return -1;
	}
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 6. license summary를 로드한다.
	
	va_load_license_summary();
	SetLicenseValidationDate();
	
	for(i = 0 ; i < DEFAULT_MEMORY_FRAGMENT ; i++)
	{
		va_free(hostidList[i]);
	}
	
	va_free(hostidList);
	return 0;
}

int Httpd()
{
	va_sock_t msgSock;
	char connectionIP[IP_LENGTH];
	char * connectionArg;
	
	char msg[DSIZ * DSIZ];
	
	va_thread_t tidCheckRetention;
	va_thread_t tidCheckRunningJob;
	va_thread_t tidGetLogRequest;
	va_thread_t tidProcessLogRequest;
	va_thread_t tidLoadLicenseSummary;
	
	int returnCode;

	returnCode = 0;

	#ifdef __ABIO_UNIX
		va_thread_t tidWait;
		va_thread_t tid;
		
		timevals tv;	
	#endif
	
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 1. master server 관리를 위한 thread를 실행한다.
	//    이 thread 들은 실행하는데 실패해도 관계가 없다.
	//    또한 process가 종료될때 이 thread들이 끝나기를 기다리지 않는다.
	
	// 1.1 backupset retention 검사를 위한 thread를 실행한다.
	if ((tidCheckRetention = va_create_thread(CheckRetention, NULL)) == 0)
	{
		printf("It failed to make a thread.\n");
		PrintDaemonLog("It failed to make a thread.");
		
		return -1;
	}
	
	// 1.2 suspend job을 검사하기 위한 thread를 실행한다.
	if ((tidCheckRunningJob = va_create_thread(CheckRunningJob, NULL)) == 0)
	{
		printf("It failed to make a thread.\n");
		PrintDaemonLog("It failed to make a thread.");
		
		CloseThread();
		
		va_wait_thread(tidCheckRetention);
		
		return -1;
	}
	
	// 1.3 suspend job을 검사하기 위한 thread를 실행한다.
	if ((tidGetLogRequest = va_create_thread(GetLogRequest, NULL)) == 0)
	{
		printf("It failed to make a thread.\n");
		PrintDaemonLog("It failed to make a thread.");
		
		CloseThread();
		
		va_wait_thread(tidCheckRetention);
		va_wait_thread(tidCheckRunningJob);
		
		return -1;
	}
	
	// 1.4 suspend job을 검사하기 위한 thread를 실행한다.
	if ((tidProcessLogRequest = va_create_thread(ProcessLogRequest, NULL)) == 0)
	{
		printf("It failed to make a thread.\n");
		PrintDaemonLog("It failed to make a thread.");
		
		CloseThread();
		
		va_wait_thread(tidCheckRetention);
		va_wait_thread(tidCheckRunningJob);
		va_wait_thread(tidGetLogRequest);
		
		return -1;
	}
	
	// 1.5 license summary를 주기적으로 로딩하는 thread를 실행한다.
	if ((tidLoadLicenseSummary = va_create_thread(LoadLicenseSummary, NULL)) == 0)
	{
		printf("It failed to make a thread.\n");
		PrintDaemonLog("It failed to make a thread.");
		
		CloseThread();
		
		va_wait_thread(tidCheckRetention);
		va_wait_thread(tidCheckRunningJob);
		va_wait_thread(tidGetLogRequest);
		va_wait_thread(tidProcessLogRequest);
		
		return -1;
	}
	
	// 1.6 child process가 끝나기를 기다리는 thread를 만든다.
	#ifdef __ABIO_UNIX
		if ((tidWait = va_create_thread(WaitChild, NULL)) == 0)
		{
			printf("It failed to make a thread.\n");
			PrintDaemonLog("It failed to make a thread.");
			
			CloseThread();
			
			va_wait_thread(tidCheckRetention);
			va_wait_thread(tidCheckRunningJob);
			va_wait_thread(tidGetLogRequest);
			va_wait_thread(tidProcessLogRequest);
			va_wait_thread(tidLoadLicenseSummary);
			
			return -1;
		}
		else
		{
			pthread_detach(tidWait);
		}
	#endif
	
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 2. user interface에서 메세지를 받아서 처리해준다.
	while (1)
	{
		if ((msgSock = va_wait_connection_returncode(httpdSock, 1, connectionIP,&returnCode)) >= 0)
		{






			#ifdef __ABIO_WIN
				DWORD recvTimeout = 5000;  // 5초.
				if(setsockopt(msgSock, SOL_SOCKET, SO_RCVTIMEO, (char*)&recvTimeout, sizeof(recvTimeout)) < 0)
				{
					PrintDaemonLog("fail");
				}
			#elif __ABIO_UNIX 	 							
				tv.tv_sec  = 5;
				tv.tv_usec = 0;
				setsockopt(msgSock, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(timevals));
			#endif


			if (CheckConnectionRestriction(connectionIP) == 0)
			{
				connectionArg = (char *)malloc(sizeof(va_sock_t) + sizeof(connectionIP));
				memcpy(connectionArg, &msgSock, sizeof(va_sock_t));
				memcpy(connectionArg + sizeof(va_sock_t), connectionIP, sizeof(connectionIP));
				
				#ifdef __ABIO_UNIX
					if ((tid = va_create_thread(ProcessMessageUnix, (void *)connectionArg)) == 0)
					{
						va_free(connectionArg);
						va_close_socket(msgSock, ABIO_SOCKET_CLOSE_CLIENT);
					}
					else
					{
						pthread_detach(tid);
					}
				#elif __ABIO_WIN
					if (_beginthread(ProcessMessageWin, 0, (void *)connectionArg) == (uintptr_t)-1)
					{
						va_free(connectionArg);
						va_close_socket(msgSock, ABIO_SOCKET_CLOSE_CLIENT);
					}
				#endif
			}
			else
			{
				memset(msg, 0, sizeof(msg));
				sprintf(msg, "Connection from %s is not allowed,", connectionIP);
				va_send(msgSock, msg, (int)strlen(msg) + 1, 0, DATA_TYPE_NOT_CHANGE);
				va_close_socket(msgSock, ABIO_SOCKET_CLOSE_CLIENT);
			}
		}
		else if (msgSock == -1)
		{
			if (httpdComplete == 1)
			{
				break;
			}
		}
		else if (msgSock == -2)
		{
			if (httpdComplete == 1)
			{
				break;
			}
		}
		else
		{
			printf("The server socket has been closed. (errno = %d)\n",returnCode);
			PrintDaemonLog("The server socket has been closed. (errno = %d)\n",returnCode);
			
			break;
		}
	}
	
	
	va_wait_thread(tidCheckRetention);
	va_wait_thread(tidCheckRunningJob);
	va_wait_thread(tidGetLogRequest);
	va_wait_thread(tidProcessLogRequest);
	va_wait_thread(tidLoadLicenseSummary);
	
	
	return 0;
}

int CheckConnectionRestriction(char * ip)
{
	int i;
	
	
	
	if (connectionRestriction != 1)
	{
		return 0;
	}
	
	for (i = 0; i < allowedIPNumber; i++)
	{
		if (!strcmp(allowedIP[i], ip))
		{
			return 0;
		}
	}
	
	return -1;
}

#ifdef __ABIO_UNIX
	va_thread_return_t ProcessMessageUnix(void * arg)
	{
		va_sock_t sock;
		char connectionIP[IP_LENGTH];
		
		char cmd[DSIZ * DSIZ];
		int cmdLength;
		
		
		
		memcpy(&sock, arg, sizeof(va_sock_t));
		memcpy(connectionIP, (char *)arg + sizeof(va_sock_t), sizeof(connectionIP));
		va_free(arg);
		
		
		memset(cmd, 0, sizeof(cmd));
		cmdLength = 0;
		
		if (RecvCommand(sock, cmd) > 0)
		{
			if (!strncmp(cmd, "<EXIT_HTTPD>", strlen("<EXIT_HTTPD>")))
			{
				CloseThread();
				
				va_close_socket(sock, ABIO_SOCKET_CLOSE_SERVER);
			}
			else
			{
				if (!strncmp(cmd, "<LOG_IN>", strlen("<LOG_IN>")))
				{
					LogIn(cmd, sock, connectionIP);
				}
				else if (!strncmp(cmd, "<LOG_IN_SELECTED>", strlen("<LOG_IN_SELECTED>")))
				{
					LogInSelected(cmd, sock, connectionIP);
				}
				else if (!strncmp(cmd, "<LOG_IN_V5_UWAY>", strlen("<LOG_IN_V5_UWAY>")))
				{
					LogIn_v5_uway(cmd, sock, connectionIP);
				}
				else
				{
					ResponseCommand(cmd, sock);
				}
				
				va_close_socket(sock, ABIO_SOCKET_CLOSE_CLIENT);
			}
		}
		else
		{
			va_close_socket(sock, ABIO_SOCKET_CLOSE_SERVER);
		}
		
		
		return NULL;
	}
#elif __ABIO_WIN
	void ProcessMessageWin(void * arg)
	{
		va_sock_t sock;
		char connectionIP[IP_LENGTH];
		
		char cmd[DSIZ * DSIZ];
		int cmdLength;
		
		
		
		memcpy(&sock, arg, sizeof(va_sock_t));
		memcpy(connectionIP, (char *)arg + sizeof(va_sock_t), sizeof(connectionIP));
		va_free(arg);
		
		
		memset(cmd, 0, sizeof(cmd));
		cmdLength = 0;
		
		if (RecvCommand(sock, cmd) > 0)
		{
			if (!strncmp(cmd, "<EXIT_HTTPD>", strlen("<EXIT_HTTPD>")))
			{
				CloseThread();
				
				va_close_socket(sock, ABIO_SOCKET_CLOSE_SERVER);
			}
			else
			{
				if (!strncmp(cmd, "<LOG_IN>", strlen("<LOG_IN>")))
				{
					LogIn(cmd, sock, connectionIP);
				}
				else if (!strncmp(cmd, "<LOG_IN_SELECTED>", strlen("<LOG_IN_SELECTED>")))
				{
					LogInSelected(cmd, sock, connectionIP);
				}
				else if (!strncmp(cmd, "<LOG_IN_V5_UWAY>", strlen("<LOG_IN_V5_UWAY>")))
				{
					LogIn_v5_uway(cmd, sock, connectionIP);
				}
				else
				{
					ResponseCommand(cmd, sock);
				}
				
				va_close_socket(sock, ABIO_SOCKET_CLOSE_CLIENT);
			}
		}
		else
		{
			va_close_socket(sock, ABIO_SOCKET_CLOSE_SERVER);
		}
		
		
		_endthread();
	}
#endif

int RecvCommand(va_sock_t sock, char * cmd)
{
	int cmdLength;
	
	
	
	cmdLength = 0;
	
	while (recv(sock, cmd + cmdLength, 1, 0) > 0)
	{
		if (!strncmp(cmd, "GET", strlen("GET")) || !strncmp(cmd, "POST", strlen("POST")) || !strncmp(cmd, "POS", strlen("POS")))
		{			
			if ((cmdLength = recv(sock, cmd + 3, DSIZ * DSIZ, 0)) > 0)
			{
				return cmdLength;
			}
		}

		cmdLength++;
		
		if (cmd[cmdLength - 1] == '\0')
		{
			break;
		}
		
		if (cmdLength == DSIZ * DSIZ)
		{
			return 0;
		}
	}
	
	
	return cmdLength;
}

int ResponseCommand(char * cmd, va_sock_t sock)
{
	int r;
	
	if (!strncmp(cmd, "GET", strlen("GET")) || !strncmp(cmd, "get", strlen("get")))
	{
		return GetCommand(cmd, sock);
	}
	else if (!strncmp(cmd, "POST", strlen("POST")) || !strncmp(cmd, "post", strlen("post")))
	{
		return PostCommand(cmd, sock);
	}
	else if ((r = ResponseCommandBackup(cmd, sock)) <= 0)
	{
		return r;
	}
	else if ((r = ResponseCommandBackupset(cmd, sock)) <= 0)
	{
		return r;
	}
	else if ((r = ResponseCommandDevice(cmd, sock)) <= 0)
	{
		return r;
	}
	else if ((r = ResponseCommandEtc(cmd, sock)) <= 0)
	{
		return r;
	}
	else if ((r = ResponseCommandHost(cmd, sock)) <= 0)
	{
		return r;
	}
	else if ((r = ResponseCommandJob(cmd, sock)) <= 0)
	{
		return r;
	}
	else if ((r = ResponseCommandMedia(cmd, sock)) <= 0)
	{
		return r;
	}
	else if ((r = ResponseCommandSchedule(cmd, sock)) <= 0)
	{
		return r;
	}
	else if ((r = ResponseCommandUser(cmd, sock)) <= 0)
	{
		return r;
	}
	else if ((r = ResponseCommandFrontPage(cmd, sock)) <= 0)
	{
		return r;
	}
	else
	{
		return 0;
	}
}




int ResponseCommandBackup(char * cmd, va_sock_t sock)
{	
	if (!strncmp(cmd, "<FS_BACKUP>", strlen("<FS_BACKUP>")))
	{
		return FsBackup(cmd, sock);
	}
	else if (!strncmp(cmd, "<DB_BACKUP>", strlen("<DB_BACKUP>")))
	{
		return DBBackup(cmd, sock);
	}
	else if (!strncmp(cmd, "<FS_RESTORE>", strlen("<FS_RESTORE>")))
	{
		return FsRestore(cmd, sock);
	}
	else if (!strncmp(cmd, "<DB_RESTORE>", strlen("<DB_RESTORE>")))
	{
		return DBRestore(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_FILE_LIST>", strlen("<GET_FILE_LIST>")))
	{
		return GetFileList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_DIRECTORY_LIST>", strlen("<GET_DIRECTORY_LIST>")))
	{
		return GetDirectoryList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_FILESYSTEM_LIST>", strlen("<GET_FILESYSTEM_LIST>")))
	{
		return GetFilesystemList(cmd, sock);
	}
	//issue344
	else if (!strncmp(cmd, "<GET_OS_FILESYSTEM_LIST>", strlen("<GET_OS_FILESYSTEM_LIST>")))
	{
		return GetOSFilesystemList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_VMWARE_DATASTORE_LIST>", strlen("<GET_VMWARE_DATASTORE_LIST>")))
	{
		return GetVmwareDatastoreList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_VMWARE_DISK_LIST>", strlen("<GET_VMWARE_DISK_LIST>")))
	{
		return GetVmwareDiskList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_VMWARE_FILE_LIST>", strlen("<GET_VMWARE_FILE_LIST>")))
	{
		return GetVmwareFileList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_NDMP_FILE_LIST>", strlen("<GET_NDMP_FILE_LIST>")))
	{
		return GetNdmpFileList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_ORACLE_FILE_LIST>", strlen("<GET_ORACLE_FILE_LIST>")))
	{
		return GetOracleFileList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_MYSQL_FILE_LIST>", strlen("<GET_MYSQL_FILE_LIST>")))
	{
		return GetMysqlFileList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_ORACLE12C_FILE_LIST>", strlen("<GET_ORACLE12C_FILE_LIST>")))
	{
		return GetOracle12cFileList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_TIBERO_FILE_LIST>", strlen("<GET_TIBERO_FILE_LIST>")))
	{
		return GetTiberoFileList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_ORACLE_RMAN_FILE_LIST>", strlen("<GET_ORACLE_RMAN_FILE_LIST>")))
	{
		return GetOracleRmanFileList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_MSSQL_FILE_LIST>", strlen("<GET_MSSQL_FILE_LIST>")))
	{
		return GetMssqlFileList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_EXCHANGE_FILE_LIST>", strlen("<GET_EXCHANGE_FILE_LIST>")))
	{
		return GetExchangeFileList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_EXCHANGE_MAILBOX_FILE_LIST>", strlen("<GET_EXCHANGE_MAILBOX_FILE_LIST>")))
	{
		return GetExchangeMailboxFileList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_EXCHANGE_MAILBOX_DIRECTORY_LIST>", strlen("<GET_EXCHANGE_MAILBOX_DIRECTORY_LIST>")))
	{
		return GetExchangeMailboxDirectoryList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_EXCHANGE_MAILBOX_STORE_LIST>", strlen("<GET_EXCHANGE_MAILBOX_STORE_LIST>")))
	{
		return GetExchangeMailboxStoreList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_SYBASE_FILE_LIST>", strlen("<GET_SYBASE_FILE_LIST>")))
	{
		return GetSybaseFileList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_SYBASE_IQ_FILE_LIST>", strlen("<GET_SYBASE_IQ_FILE_LIST>")))
	{
		return GetSybaseIqFileList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_DB2_DATABASE_LIST>", strlen("<GET_DB2_DATABASE_LIST>")))
	{
		return GetDB2DatabaseList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_DB2_TABLESPACE_LIST>", strlen("<GET_DB2_TABLESPACE_LIST>")))
	{
		return GetDB2TablespaceList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_DB2_CATALOG_DB>", strlen("<GET_DB2_CATALOG_DB>")))
	{
		return GetDB2CatalogDB(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_DB2_BACKUP_DATABASE_LIST>", strlen("<GET_DB2_BACKUP_DATABASE_LIST>")))
	{
		return GetDB2BackupDatabaseList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_DB2_BACKUP_TABLESPACE_LIST>", strlen("<GET_DB2_BACKUP_TABLESPACE_LIST>")))
	{
		return GetDB2BackupTablespaceList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_DB2_CATALOG_DB_LOG>", strlen("<GET_DB2_CATALOG_DB_LOG>")))
	{
		return GetDB2CatalogDBLog(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_DB2_BACKUP_LOG_DATABASE_LIST>", strlen("<GET_DB2_BACKUP_LOG_DATABASE_LIST>")))
	{
		return GetDB2BackupDatabaseList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_ALTIBASE_FILE_LIST>", strlen("<GET_ALTIBASE_FILE_LIST>")))
	{
		return GetAltibaseFileList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_NOTES_FILE_LIST>", strlen("<GET_NOTES_FILE_LIST>")))
	{
		return GetNotesFileList(cmd, sock);
	}
	else if (!strncmp(cmd, "<PUT_FILE_LIST>", strlen("<PUT_FILE_LIST>")))
	{
		return PutFileList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_DB_INFO>", strlen("<GET_DB_INFO>")))
	{
		return GetDBInfo(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_BACKUPSET_LIST>", strlen("<GET_BACKUPSET_LIST>")))
	{
		return GetBackupsetList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_NDMP_BACKUP_ID_LIST>", strlen("<GET_NDMP_BACKUP_ID_LIST>")))
	{
		return GetNdmpBackupIDList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_VMWARE_LATEST_LIST>", strlen("<GET_VMWARE_LATEST_LIST>")))
	{
		return GetVmwareLatestList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_VMWARE_BACKUP_ID_LIST>", strlen("<GET_VMWARE_BACKUP_ID_LIST>")))
	{
		return GetVmwareBackupIDList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_VMWARE_CATALOG_DB>", strlen("<GET_VMWARE_CATALOG_DB>")))
	{
		return GetVmwareCatalogDB(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_VMWARE_BACKUP_LIST>", strlen("<GET_VMWARE_BACKUP_LIST>")))
	{
		return GetVmwareBackupList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_VMWARE_BACKUP_PATH>", strlen("<GET_VMWARE_BACKUP_PATH>")))
	{
		return GetVmwareBackupPath(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_CATALOG_DB>", strlen("<GET_CATALOG_DB>")))
	{
		return GetCatalogDB(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_CATALOG_DIRECOTRY_DB>", strlen("<GET_CATALOG_DIRECOTRY_DB>")))
	{
		return GetCatalogDirectoryDB(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_CATALOG_FILE_DB>", strlen("<GET_CATALOG_FILE_DB>")))
	{
		return GetCatalogFileDB(cmd, sock);
	}
	//SharePoint
	else if (!strncmp(cmd, "<GET_SHAREPOINT_FARM_LIST>", strlen("<GET_SHAREPOINT_FARM_LIST>")))
	{
		return GetSharePointFarmList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_SHAREPOINT_SITE_LIST>", strlen("<GET_SHAREPOINT_SITE_LIST>")))
	{
		return GetSharePointSiteList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_SHAREPOINT_CATALOG_DB>", strlen("<GET_SHAREPOINT_CATALOG_DB>")))
	{
		return GetSharepointCatalogDB(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_SHAREPOINT_BACKUPSET_LIST>", strlen("<GET_SHAREPOINT_BACKUPSET_LIST>")))
	{
		return GetSharepointBackupsetList(cmd, sock);
	}
	
	//#issue 164
	else if (!strncmp(cmd, "<GET_SMART_RESTORE_FILE_LIST>", strlen("<GET_SMART_RESTORE_FILE_LIST>")))
	{
		return GetSmartRestoreFileList(cmd, sock);
	}
	//
	else if (!strncmp(cmd, "<GET_NDMP_CATALOG_DB>", strlen("<GET_NDMP_CATALOG_DB>")))
	{
		return GetNdmpCatalogDB(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_DB_BACKUP_INSTANCE>", strlen("<GET_DB_BACKUP_INSTANCE>")))
	{
		return GetDBBackupInstance(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_ORACLE_BACKUP_TABLESPACE_LIST>", strlen("<GET_ORACLE_BACKUP_TABLESPACE_LIST>")))
	{
		return GetOracleBackupTablespaceList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_ORACLE12C_BACKUP_TABLESPACE_LIST>", strlen("<GET_ORACLE12C_BACKUP_TABLESPACE_LIST>")))
	{
		return GetOracle12cBackupTablespaceList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_ORACLE_CATALOG_DB>", strlen("<GET_ORACLE_CATALOG_DB>")))
	{
		return GetOracleCatalogDB(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_ORACLE12C_CATALOG_DB>", strlen("<GET_ORACLE12C_CATALOG_DB>")))
	{
		return GetOracle12cCatalogDB(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_ORACLE_RMAN_BACKUP_TABLESPACE_LIST>", strlen("<GET_ORACLE_RMAN_BACKUP_TABLESPACE_LIST>")))
	{
		return GetOracleRmanBackupTablespaceList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_ORACLE_RMAN_CATALOG_DB>", strlen("<GET_ORACLE_RMAN_CATALOG_DB>")))
	{
		return GetOracleRmanCatalogDB(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_ORACLE_RMAN_RESTORE_BACKUPSET_LIST>", strlen("<GET_ORACLE_RMAN_RESTORE_BACKUPSET_LIST>")))
	{
		return GetOracleRmanRestoreBackupset(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_MSSQL_BACKUP_DATABASE_LIST>", strlen("<GET_MSSQL_BACKUP_DATABASE_LIST>")))
	{
		return GetMssqlBackupDatabaseList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_MSSQL_CATALOG_DB>", strlen("<GET_MSSQL_CATALOG_DB>")))
	{
		return GetMssqlCatalogDB(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_EXCHANGE_BACKUP_STORAGE_GROUP_LIST>", strlen("<GET_EXCHANGE_BACKUP_STORAGE_GROUP_LIST>")))
	{
		return GetExchangeBackupStorageGroupList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_EXCHANGE_CATALOG_DB>", strlen("<GET_EXCHANGE_CATALOG_DB>")))
	{
		return GetExchangeCatalogDB(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_EXCHANGE_MAILBOX_CATALOG_DB>", strlen("<GET_EXCHANGE_MAILBOX_CATALOG_DB>")))
	{
		return GetExchangeMailboxCatalogDB(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_SAP_BACKUP_ID_LIST>", strlen("<GET_SAP_BACKUP_ID_LIST>")))
	{
		return GetSapBackupIDList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_SAP_CATALOG_DB>", strlen("<GET_SAP_CATALOG_DB>")))
	{
		return GetSapCatalogDB(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_SYBASE_BACKUP_DATABASE_LIST>", strlen("<GET_SYBASE_BACKUP_DATABASE_LIST>")))
	{
		return GetSybaseBackupDatabaseList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_SYBASE_IQ_BACKUP_DATABASE_LIST>", strlen("<GET_SYBASE_IQ_BACKUP_DATABASE_LIST>")))
	{
		return GetSybaseIqBackupDatabaseList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_SYBASE_CATALOG_DB>", strlen("<GET_SYBASE_CATALOG_DB>")))
	{
		return GetSybaseCatalogDB(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_SYBASE_IQ_CATALOG_DB>", strlen("<GET_SYBASE_IQ_CATALOG_DB>")))
	{
		return GetSybaseIqCatalogDB(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_ALTIBASE_BACKUP_TABLESPACE_LIST>", strlen("<GET_ALTIBASE_BACKUP_TABLESPACE_LIST>")))
	{
		return GetAltibaseBackupTablespaceList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_ALTIBASE_CATALOG_DB>", strlen("<GET_ALTIBASE_CATALOG_DB>")))
	{
		return GetAltibaseCatalogDB(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_NOTES_CATALOG_DB>", strlen("<GET_NOTES_CATALOG_DB>")))
	{
		return GetNotesCatalogDB(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_INFORMIX_LIST>", strlen("<GET_INFORMIX_LIST>")))
	{
		return GetInformixList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_INFORMIX_BACKUP_TABLESPACE_LIST>", strlen("<GET_INFORMIX_BACKUP_TABLESPACE_LIST>")))
	{
		return GetInformixBackupTablespaceList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_INFORMIX_CATALOG_DB>", strlen("<GET_INFORMIX_CATALOG_DB>")))
	{
		return GetInformixCatalogDB(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_INFORMIX_INSTANCE>", strlen("<GET_INFORMIX_INSTANCE>")))
	{
		return GetInformixInstance(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_INFORMIX_TABLESPACE>", strlen("<GET_INFORMIX_TABLESPACE>")))
	{
		return GetInformixTablespace(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_INFORMIX_TABLESPACE_CATALOG>", strlen("<GET_INFORMIX_TABLESPACE_CATALOG>")))
	{
		return GetInformixTablespaceCatalog(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_INFORMIX_ONBAR_LIST>", strlen("<GET_INFORMIX_ONBAR_LIST>")))
	{
		return GetInformixOnbarList(cmd, sock);
	}	
	else if (!strncmp(cmd, "<GET_INFORMIX_BACKUP_DBSPACE_LIST>", strlen("<GET_INFORMIX_BACKUP_DBSPACE_LIST>")))
	{
		return GetInformixBackupDBspaceList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_INFORMIX_ONBAR_CATALOG_DB>", strlen("<GET_INFORMIX_ONBAR_CATALOG_DB>")))
	{
		return GetInformixOnbarCatalogDB(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_INFORMIX_ONBAR_INSTANCE>", strlen("<GET_INFORMIX_ONBAR_INSTANCE>")))
	{
		return GetInformixOnbarInstance(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_INFORMIX_ONBAR_DBSPACE>", strlen("<GET_INFORMIX_ONBAR_DBSPACE>")))
	{
		return GetInformixOnbarDBSPACE(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_INFORMIX_ONBAR_DBSPACE_CATALOG>", strlen("<GET_INFORMIX_ONBAR_DBSPACE_CATALOG>")))
	{
		return GetInformixOnbarDBSPACECatalog(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_ORACLE_INSTANCE>", strlen("<GET_ORACLE_INSTANCE>")))
	{
		return GetOracleInstance(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_ORACLE12C_INSTANCE>", strlen("<GET_ORACLE12C_INSTANCE>")))
	{
		return GetOracle12cInstance(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_ORACLE_TABLESPACE>", strlen("<GET_ORACLE_TABLESPACE>")))
	{
		return GetOracleTableSpace(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_ORACLE12C_TABLESPACE>", strlen("<GET_ORACLE12C_TABLESPACE>")))
	{
		return GetOracle12cTableSpace(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_ORACLE_TABLESPACE_CATALOG>", strlen("<GET_ORACLE_TABLESPACE_CATALOG>")))
	{
		return GetOracleTableSpaceCatalog(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_ORACLE12C_TABLESPACE_CATALOG>", strlen("<GET_ORACLE12C_TABLESPACE_CATALOG>")))
	{
		return GetOracle12cTableSpaceCatalog(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_TIBERO_BACKUP_TABLESPACE_LIST>", strlen("<GET_TIBERO_BACKUP_TABLESPACE_LIST>")))
	{
		return GetTiberoBackupTablespaceList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_TIBERO_CATALOG_DB>", strlen("<GET_TIBERO_CATALOG_DB>")))
	{
		return GetTiberoCatalogDB(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_TIBERO_INSTANCE>", strlen("<GET_TIBERO_INSTANCE>")))
	{
		return GetTiberoInstance(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_TIBERO_TABLESPACE>", strlen("<GET_TIBERO_TABLESPACE>")))
	{
		return GetTiberoTableSpace(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_TIBERO_TABLESPACE_CATALOG>", strlen("<GET_TIBERO_TABLESPACE_CATALOG>")))
	{
		return GetTiberoTableSpaceCatalog(cmd, sock);
	}

	else if (!strncmp(cmd, "<GET_ORACLE_ARCHIVE_LIST>", strlen("<GET_ORACLE_ARCHIVE_LIST>")))
	{
		return GetOracleArchiveList(cmd, sock);
	}

	else if (!strncmp(cmd, "<GET_ORACLE_RMAN_ARCHIVE_LIST>", strlen("<GET_ORACLE_RMAN_ARCHIVE_LIST>")))
	{
		return GetOracleRmanArchiveList(cmd, sock);
	}

	else if (!strncmp(cmd, "<GET_SQLBACKTRACKE_FILE_LIST>", strlen("<GET_SQLBACKTRACKE_FILE_LIST>")))
	{
		return GetSQLBackTrackFileList(cmd, sock);
	}

	else if (!strncmp(cmd, "<GET_SQL_BACKTRACK_INSTANCE>", strlen("<GET_SQL_BACKTRACK_INSTANCE>")))
	{
		return GetSQLBackTrackstance(cmd, sock);
	}

	else if (!strncmp(cmd, "<GET_SQL_BACKTRACK_TABLESPACE>", strlen("<GET_SQL_BACKTRACK_TABLESPACE>")))
	{
		return GetSQLBackTrackTableSpace(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_SQL_BACKTRACK_TABLESPACE_CATALOG>", strlen("<GET_SQL_BACKTRACK_TABLESPACE_CATALOG>")))
	{
		return GetSQLBackTrackTableSpaceCatalog(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_MYSQL_BACKUP_DATABASE_LIST>", strlen("<GET_MYSQL_BACKUP_DATABASE_LIST>")))
	{
		return GetMySQLBackupDatabaseList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_MYSQL_CATALOG_DB>", strlen("<GET_MYSQL_CATALOG_DB>")))
	{
		return GetMySQLCatalogDB(cmd, sock);
	}
	else
	{
		return 1;
	}
}
int ResponseCommandBackupset(char * cmd, va_sock_t sock)
{
	if (!strncmp(cmd, "<GET_BACKUPSET_LOG>", strlen("<GET_BACKUPSET_LOG>")))
	{
		return GetBackupsetLog(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_BACKUPSET_VOLUME_LIST>", strlen("<GET_BACKUPSET_VOLUME_LIST>")))
	{
		return GetBackupsetVolumeList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_BACKUPSET_FILE_LIST>", strlen("<GET_BACKUPSET_FILE_LIST>")))
	{
		return GetBackupsetFilelist(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_BACKUPSET_CATALOG>", strlen("<GET_BACKUPSET_CATALOG>")))
	{
		return GetBackupsetCatalog(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_BACKUPSET_CATALOGLIST>", strlen("<GET_BACKUPSET_CATALOGLIST>")))
	{
		return GetBackupsetCataloglist(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_LATEST_CATALOGLIST>", strlen("<GET_LATEST_CATALOGLIST>")))
	{
		return GetLatestCataloglist(cmd, sock);
	}
	else if (!strncmp(cmd, "<EXPIRE_BACKUPSET>", strlen("<EXPIRE_BACKUPSET>")))
	{
		return ExpireBackupset(cmd, sock);
	}
	else if (!strncmp(cmd, "<VERIFY_BACKUPSET>", strlen("<VERIFY_BACKUPSET>")))
	{
		return VerifyBackupset(cmd, sock);
	}
	else if (!strncmp(cmd, "<MIGRATE_BACKUPSET>", strlen("<MIGRATE_BACKUPSET>")))
	{
		return MigrateBackupset(cmd, sock);
	}
	else if (!strncmp(cmd, "<DUPLICATE_BACKUPSET>", strlen("<DUPLICATE_BACKUPSET>")))
	{
		return DuplicateBackupset(cmd, sock);
	}
	else if (!strncmp(cmd, "<CHANGE_BACKUPSET_RETENTION>", strlen("<CHANGE_BACKUPSET_RETENTION>")))
	{
		return ChangeBackupsetRetention(cmd, sock);
	}
	else if (!strncmp(cmd, "<SHRINK_MASTER_CATALOG>", strlen("<SHRINK_MASTER_CATALOG>")))
	{
		return ShrinkMasterCatalog(cmd, sock);
	}
	else if (!strncmp(cmd, "<SHRINK_CLIENT_CATALOG>", strlen("<SHRINK_CLIENT_CATALOG>")))
	{
		return ShrinkClientCatalog(cmd, sock);
	}
	else if (!strncmp(cmd, "<DELETE_SLAVE_CATALOG>", strlen("<DELETE_SLAVE_CATALOG>")))
	{
		return DeleteSlaveCatalog(cmd, sock);
	}
	else if (!strncmp(cmd, "<DELETE_SLAVE_LOG>", strlen("<DELETE_SLAVE_LOG>")))
	{
		return DeleteSlaveLog(cmd, sock);
	}
	
	else if (!strncmp(cmd, "<FIND_FILES>", strlen("<FIND_FILES>")))
	{
		return FindFiles(cmd, sock);
	}
	else
	{
		return 1;
	}
}

int ResponseCommandDevice(char * cmd, va_sock_t sock)
{
	if (!strncmp(cmd, "<GET_STORAGE_LIST>", strlen("<GET_STORAGE_LIST>")))
	{
		return GetStorageList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_STORAGE_TAPE_LIBRARY>", strlen("<GET_STORAGE_TAPE_LIBRARY>")))
	{
		return GetStorageTapeLibrary(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_TAPE_LIBRARY_OWNER_LIST>", strlen("<GET_TAPE_LIBRARY_OWNER_LIST>")))
	{
		return GetTapeLibraryOwnerList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_TAPE_DRIVE_LIST>", strlen("<GET_TAPE_DRIVE_LIST>")))
	{
		return GetTapeDriveList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_TAPE_DRIVE_OWNER_LIST>", strlen("<GET_TAPE_DRIVE_OWNER_LIST>")))
	{
		return GetTapeDriveOwnerList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_STORAGE_STANDALONE_TAPE_DRIVE>", strlen("<GET_STORAGE_STANDALONE_TAPE_DRIVE>")))
	{
		return GetStorageStandAloneTapeDrive(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_STORAGE_DISK>", strlen("<GET_STORAGE_DISK>")))
	{
		return GetStorageDisk(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_STORAGE_OWNER_LIST>", strlen("<GET_STORAGE_OWNER_LIST>")))
	{
		return GetStorageOwnerList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_TAPE_DRIVE_ADDRESS>", strlen("<GET_TAPE_DRIVE_ADDRESS>")))
	{
		return GetTapeDriveAddress(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_SCSI_INVENTORY>", strlen("<GET_SCSI_INVENTORY>")))
	{
		return GetScsiInventory(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_TAPE_DRIVE_SERIAL>", strlen("<GET_TAPE_DRIVE_SERIAL>")))
	{
		return GetTapeDriveSerial(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_TAPE_LIBRARY_SERIAL>", strlen("<GET_TAPE_LIBRARY_SERIAL>")))
	{
		return GetTapeLibrarySerial(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_TAPE_LIBRARY_SLOT_NUMBER>", strlen("<GET_TAPE_LIBRARY_SLOT_NUMBER>")))
	{
		return GetTapeLibrarySlotNumber(cmd, sock);
	}
	else if (!strncmp(cmd, "<ADD_STORAGE_TAPE_LIBRARY>", strlen("<ADD_STORAGE_TAPE_LIBRARY>")))
	{
		return AddStorageTapeLibrary(cmd, sock);
	}
	else if (!strncmp(cmd, "<ADD_TAPE_DRIVE>", strlen("<ADD_TAPE_DRIVE>")))
	{
		return AddTapeDrive(cmd, sock);
	}
	else if (!strncmp(cmd, "<ADD_TAPE_DRIVE_STORAGE_OWNER>", strlen("<ADD_TAPE_DRIVE_STORAGE_OWNER>")))
	{
		return AddTapeDriveStorageOwner(cmd, sock);
	}
	else if (!strncmp(cmd, "<ADD_STORAGE_STANDALONE_TAPE_DRIVE>", strlen("<ADD_STORAGE_STANDALONE_TAPE_DRIVE>")))
	{
		return AddStorageStandAloneTapeDrive(cmd, sock);
	}
	else if (!strncmp(cmd, "<ADD_STORAGE_DISK>", strlen("<ADD_STORAGE_DISK>")))
	{
		return AddStorageDisk(cmd, sock);
	}
	else if (!strncmp(cmd, "<RESET_STORAGE_CONFIGURATION>", strlen("<RESET_STORAGE_CONFIGURATION>")))
	{
		return ResetStorageConfiguration(cmd, sock);
	}
	else if (!strncmp(cmd, "<UPDATE_STORAGE_TAPE_LIBRARY>", strlen("<UPDATE_STORAGE_TAPE_LIBRARY>")))
	{
		return UpdateStorageTapeLibrary(cmd, sock);
	}
	else if (!strncmp(cmd, "<UPDATE_TAPE_DRIVE>", strlen("<UPDATE_TAPE_DRIVE>")))
	{
		return UpdateTapeDrive(cmd, sock);
	}
	else if (!strncmp(cmd, "<UPDATE_TAPE_DRIVE_STORAGE_OWNER>", strlen("<UPDATE_TAPE_DRIVE_STORAGE_OWNER>")))
	{
		return UpdateTapeDriveStorageOwner(cmd, sock);
	}
	else if (!strncmp(cmd, "<UPDATE_STORAGE_STANDALONE_TAPE_DRIVE>", strlen("<UPDATE_STORAGE_STANDALONE_TAPE_DRIVE>")))
	{
		return UpdateStorageStandAloneTapeDrive(cmd, sock);
	}
	else if (!strncmp(cmd, "<UPDATE_STORAGE_DISK>", strlen("<UPDATE_STORAGE_DISK>")))
	{
		return UpdateStorageDisk(cmd, sock);
	}
	else if (!strncmp(cmd, "<DELETE_STORAGE_TAPE_LIBRARY>", strlen("<DELETE_STORAGE_TAPE_LIBRARY>")))
	{
		return DeleteStorageTapeLibrary(cmd, sock);
	}
	else if (!strncmp(cmd, "<DELETE_TAPE_DRIVE>", strlen("<DELETE_TAPE_DRIVE>")))
	{
		return DeleteTapeDrive(cmd, sock);
	}
	else if (!strncmp(cmd, "<DELETE_TAPE_DRIVE_STORAGE_OWNER>", strlen("<DELETE_TAPE_DRIVE_STORAGE_OWNER>")))
	{
		return DeleteTapeDriveStorageOwner(cmd, sock);
	}
	else if (!strncmp(cmd, "<DELETE_STORAGE_STANDALONE_TAPE_DRIVE>", strlen("<DELETE_STORAGE_STANDALONE_TAPE_DRIVE>")))
	{
		return DeleteStorageStandAloneTapeDrive(cmd, sock);
	}
	else if (!strncmp(cmd, "<DELETE_STORAGE_DISK>", strlen("<DELETE_STORAGE_DISK>")))
	{
		return DeleteStorageDisk(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_STORAGE>", strlen("<GET_STORAGE>")))
	{
		return GetStorage(cmd, sock);
	}
	else
	{
		return 1;
	}
}

int ResponseCommandEtc(char * cmd, va_sock_t sock)
{
	if (!strncmp(cmd, "<GET_LICENSE_KEY_LIST>", strlen("<GET_LICENSE_KEY_LIST>")))
	{
		return GetLicenseKeyList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_LICENSE_SUMMARY>", strlen("<GET_LICENSE_SUMMARY>")))
	{
		return GetLicenseSummary(cmd, sock);
	}
	else if (!strncmp(cmd, "<ADD_LICENSE_KEY>", strlen("<ADD_LICENSE_KEY>")))
	{
		return AddLicenseKey(cmd, sock);
	}
	else if (!strncmp(cmd, "<REMOVE_LICENSE_KEY>", strlen("<REMOVE_LICENSE_KEY>")))
	{
		return RemoveLicenseKey(cmd, sock);
	}
	else if (!strncmp(cmd, "<GENERATE_EVALUATION_KEY>", strlen("<GENERATE_EVALUATION_KEY>")))
	{
		return GenerateEvaluationKey(cmd, sock);
	}
	else
	{
		return 1;
	}
}

int ResponseCommandHost(char * cmd, va_sock_t sock)
{
	if (!strncmp(cmd, "<GET_NODE_LIST>", strlen("<GET_NODE_LIST>")))
	{
		return GetNodeList(cmd, sock);
	}
	if (!strncmp(cmd, "<GET_SELECTED_NODE_LIST>", strlen("<GET_SELECTED_NODE_LIST>")))
	{
		return GetSelectedNodeList(cmd, sock);
	}
	if (!strncmp(cmd, "<GET_NODE_NUMBER>", strlen("<GET_NODE_NUMBER>")))
	{
		return GetNodeNumber(cmd, sock);
	}
	if (!strncmp(cmd, "<GET_NODE_INFORMATION>", strlen("<GET_NODE_INFORMATION>")))
	{
		return GetNodeInformation(cmd, sock);
	}
	else if (!strncmp(cmd, "<ADD_NODE>", strlen("<ADD_NODE>")))
	{
		return AddNode(cmd, sock);
	}
	else if (!strncmp(cmd, "<UPDATE_NODE>", strlen("<UPDATE_NODE>")))
	{
		return UpdateNode(cmd, sock);
	}
	else if (!strncmp(cmd, "<DELETE_NODE>", strlen("<DELETE_NODE>")))
	{
		return DeleteNode(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_NODE_MODULE>", strlen("<GET_NODE_MODULE>")))
	{
		return GetNodeModule(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_MODULE_CONF>", strlen("<GET_MODULE_CONF>")))
	{		
		return GetModuleConf(cmd, sock);
	}
	else if (!strncmp(cmd, "<UPDATE_MODULE_CONF>", strlen("<UPDATE_MODULE_CONF>"))
			|| !strncmp(cmd, "<ADD_MODULE_CONF>", strlen("<ADD_MODULE_CONF>"))
			|| !strncmp(cmd, "<DELETE_MODULE_CONF>", strlen("<DELETE_MODULE_CONF>")))
	{		
		return SetModuleConf(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_CATALOG_INFO>", strlen("<GET_CATALOG_INFO>")))
	{
		return GetCatalogInfo(cmd, sock);
	}
	else if (!strncmp(cmd, "<SET_SERVICE>",strlen("<SET_SERVICE>")))
	{		
		return ServiceStart(cmd, sock);
	}
	else if (!strncmp(cmd, "<SET_SERVICE_INACTIVE>",strlen("<SET_SERVICE_INACTIVE>")))
	{		
		return ServiceStop(cmd, sock);
	}
	else if (!strncmp(cmd, "<CHECK_NODE_MODULE>", strlen("<CHECK_NODE_MODULE>")))
	{
		return CheckNodeModule(cmd, sock);
	}
	else
	{
		return 1;
	}
}

int ResponseCommandJob(char * cmd, va_sock_t sock)
{
	if (!strncmp(cmd, "<GET_RUNNING_JOB_LOG>", strlen("<GET_RUNNING_JOB_LOG>")))
	{
		return GetRunningJobLog(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_TERMINAL_JOB_LOG>", strlen("<GET_TERMINAL_JOB_LOG>")))
	{
		return GetTerminalJobLog(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_TERMINAL_ERROR_JOB_LOG>", strlen("<GET_TERMINAL_ERROR_JOB_LOG>")))
	{
		return GetTerminalErrorJobLog(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_JOB_LOG_RUN>", strlen("<GET_JOB_LOG_RUN>")))
	{
		return GetJobLogRun(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_JOB_LOG_DESCRIPTION>", strlen("<GET_JOB_LOG_DESCRIPTION>")))
	{
		return GetJobLogDescription(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_EVENT_LOG>", strlen("<GET_EVENT_LOG>")))
	{
		return GetEventLog(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_EVENT_LOG_RUN>", strlen("<GET_EVENT_LOG_RUN>")))
	{
		return GetEventLogRun(cmd, sock);
	}
	else if (!strncmp(cmd, "<DELETE_LOG>", strlen("<DELETE_LOG>")))
	{
		return DeleteLog(cmd, sock);
	}
	else if (!strncmp(cmd, "<CANCEL_JOB>", strlen("<CANCEL_JOB>")))
	{
		return CancelJob(cmd, sock);
	}
	else if (!strncmp(cmd, "<STOP_JOB>", strlen("<STOP_JOB>")))
	{
		return StopJob(cmd, sock);
	}
	else if (!strncmp(cmd, "<PAUSE_JOB>", strlen("<PAUSE_JOB>")))
	{
		return PauseJob(cmd, sock);
	}
	else if (!strncmp(cmd, "<RESUME_JOB>", strlen("<RESUME_JOB>")))
	{
		return ResumeJob(cmd, sock);
	}
	else
	{
		return 1;
	}
}

int ResponseCommandMedia(char * cmd, va_sock_t sock)
{
	if (!strncmp(cmd, "<GET_VOLUME_POOL_LIST>", strlen("<GET_VOLUME_POOL_LIST>")))
	{
		return GetVolumePoolList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_SELECTED_VOLUME_POOL_LIST>", strlen("<GET_SELECTED_VOLUME_POOL_LIST>")))
	{
		return GetSelectedVolumePoolList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_VOLUME_LIST>", strlen("<GET_VOLUME_LIST>")))
	{
		return GetVolumeList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_VOLUME_POOL_VOLUME_LIST>", strlen("<GET_VOLUME_POOL_VOLUME_LIST>")))
	{
		return GetVolumePoolVolumeList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_STORAGE_VOLUME_LIST>", strlen("<GET_STORAGE_VOLUME_LIST>")))
	{
		return GetStorageVolumeList(cmd, sock);
	}
	else if (!strncmp(cmd, "<ADD_VOLUME_POOL>", strlen("<ADD_VOLUME_POOL>")))
	{
		return AddVolumePool(cmd, sock);
	}
	else if (!strncmp(cmd, "<UPDATE_VOLUME_POOL>", strlen("<UPDATE_VOLUME_POOL>")))
	{
		return UpdateVolumePool(cmd, sock);
	}
	else if (!strncmp(cmd, "<DELETE_VOLUME_POOL>", strlen("<DELETE_VOLUME_POOL>")))
	{
		return DeleteVolumePool(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_TAPE_LIBRARY_INVENTORY_VOLUME_LIST>", strlen("<GET_TAPE_LIBRARY_INVENTORY_VOLUME_LIST>")))
	{
		return GetTapeLibraryInventoryVolumeList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_DISK_INVENTORY_VOLUME_LIST>", strlen("<GET_DISK_INVENTORY_VOLUME_LIST>")))
	{
		return GetDiskInventoryVolumeList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_TAPE_LIBRARY_VOLUME_HEADER>", strlen("<GET_TAPE_LIBRARY_VOLUME_HEADER>")))
	{
		return GetTapeLibraryVolumeHeader(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_IMPORT_VOLUME_LIST>", strlen("<GET_IMPORT_VOLUME_LIST>")))
	{
		return GetImportVolumeList(cmd, sock);
	}
	else if (!strncmp(cmd, "<ADD_TAPE_LIBRARY_VOLUME>", strlen("<ADD_TAPE_LIBRARY_VOLUME>")))
	{
		return AddTapeLibraryVolume(cmd, sock);
	}
	else if (!strncmp(cmd, "<ADD_STANDALONE_TAPE_DRIVE_VOLUME>", strlen("<ADD_STANDALONE_TAPE_DRIVE_VOLUME>")))
	{
		return AddStandAloneTapeDriveVolume(cmd, sock);
	}
	else if (!strncmp(cmd, "<RESET_VOLUME_CONFIGURATION>", strlen("<RESET_VOLUME_CONFIGURATION>")))
	{
		return ResetVolumeConfiguration(cmd, sock);
	}
	else if (!strncmp(cmd, "<UPDATE_VOLUME>", strlen("<UPDATE_VOLUME>")))
	{
		return UpdateVolume(cmd, sock);
	}
	else if (!strncmp(cmd, "<UPDATE_VOLUME_VOLUME_POOL>", strlen("<UPDATE_VOLUME_VOLUME_POOL>")))
	{
		return UpdateVolumeVolumePool(cmd, sock);
	}
	else if (!strncmp(cmd, "<MAKE_VOLUME_ACCESS_READ_WRITE>", strlen("<MAKE_VOLUME_ACCESS_READ_WRITE>")))
	{
		return MakeVolumeAccessReadWrite(cmd, sock);
	}
	else if (!strncmp(cmd, "<MAKE_VOLUME_ACCESS_READ_ONLY>", strlen("<MAKE_VOLUME_ACCESS_READ_ONLY>")))
	{
		return MakeVolumeAccessReadOnly(cmd, sock);
	}
	else if (!strncmp(cmd, "<MAKE_VOLUME_POOL_ACCESS_NON_PERMANENT>", strlen("<MAKE_VOLUME_POOL_ACCESS_NON_PERMANENT>")))
	{
		return MakeVolumePoolAccessNonPermanent(cmd, sock);
	}
	else if (!strncmp(cmd, "<MAKE_VOLUME_POOL_ACCESS_PERMANENT>", strlen("<MAKE_VOLUME_POOL_ACCESS_PERMANENT>")))
	{
		return MakeVolumePoolAccessPermanent(cmd, sock);
	}
	else if (!strncmp(cmd, "<DELETE_VOLUME>", strlen("<DELETE_VOLUME>")))
	{
		return DeleteVolume(cmd, sock);
	}
	else if (!strncmp(cmd, "<IMPORT_VOLUME>", strlen("<IMPORT_VOLUME>")))
	{
		return ImportVolume(cmd, sock);
	}
	else if (!strncmp(cmd, "<EXPORT_VOLUME>", strlen("<EXPORT_VOLUME>")))
	{
		return ExportVolume(cmd, sock);
	}
	else if (!strncmp(cmd, "<AUTO_IMPORT_VOLUME>", strlen("<AUTO_IMPORT_VOLUME>")))
	{
		return AutoImportVolume(cmd);
	}
	else if (!strncmp(cmd, "<AUTO_EXPORT_VOLUME>", strlen("<AUTO_EXPORT_VOLUME>")))
	{
		return AutoExportVolume(cmd);
	}
	else if (!strncmp(cmd, "<UNMOUNT_VOLUME>", strlen("<UNMOUNT_VOLUME>")))
	{
		return UnmountVolume(cmd, sock);
	}
	else if (!strncmp(cmd, "<EXPIRE_VOLUME>", strlen("<EXPIRE_VOLUME>")))
	{
		return ExpireVolume(cmd, sock);
	}
	else
	{
		return 1;
	}
}

int ResponseCommandSchedule(char * cmd, va_sock_t sock)
{
	if (!strncmp(cmd, "<GET_POLICY_LIST>", strlen("<GET_POLICY_LIST>")))
	{
		return GetPolicyList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_POLICY>", strlen("<GET_POLICY>")))
	{
		return GetPolicy(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_POLICY_FILE_LIST>", strlen("<GET_POLICY_FILE_LIST>")))
	{
		return GetPolicyFileList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_SCHEDULE_LIST>", strlen("<GET_SCHEDULE_LIST>")))
	{
		return GetScheduleList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_SCHEDULE>", strlen("<GET_SCHEDULE>")))
	{
		return GetSchedule(cmd, sock);
	}
	else if (!strncmp(cmd, "<ADD_POLICY>", strlen("<ADD_POLICY>")))
	{
		return AddPolicy(cmd, sock);
	}
	else if (!strncmp(cmd, "<UPDATE_POLICY>", strlen("<UPDATE_POLICY>")))
	{
		return UpdatePolicy(cmd, sock);
	}
	else if (!strncmp(cmd, "<DELETE_POLICY>", strlen("<DELETE_POLICY>")))
	{
		return DeletePolicy(cmd, sock);
	}
	else if (!strncmp(cmd, "<COPY_POLICY>", strlen("<COPY_POLICY>")))
	{
		return CopyPolicy(cmd, sock);
	}
	else if (!strncmp(cmd, "<ACTIVATE_POLICY>", strlen("<ACTIVATE_POLICY>")))
	{
		return ActivatePolicy(cmd, sock);
	}
	else if (!strncmp(cmd, "<DEACTIVATE_POLICY>", strlen("<DEACTIVATE_POLICY>")))
	{
		return DeactivatePolicy(cmd, sock);
	}
	else if (!strncmp(cmd, "<ADD_SCHEDULE>", strlen("<ADD_SCHEDULE>")))
	{
		return AddSchedule(cmd, sock);
	}
	else if (!strncmp(cmd, "<UPDATE_SCHEDULE>", strlen("<UPDATE_SCHEDULE>")))
	{
		return UpdateSchedule(cmd, sock);
	}
	else if (!strncmp(cmd, "<DELETE_SCHEDULE>", strlen("<DELETE_SCHEDULE>")))
	{
		return DeleteSchedule(cmd, sock);
	}
	else if (!strncmp(cmd, "<COPY_SCHEDULE>", strlen("<COPY_SCHEDULE>")))
	{
		return CopySchedule(cmd, sock);
	}
	else if (!strncmp(cmd, "<ACTIVATE_SCHEDULE>", strlen("<ACTIVATE_SCHEDULE>")))
	{
		return ActivateSchedule(cmd, sock);
	}
	else if (!strncmp(cmd, "<DEACTIVATE_SCHEDULE>", strlen("<DEACTIVATE_SCHEDULE>")))
	{
		return DeactivateSchedule(cmd, sock);
	}
	else if (!strncmp(cmd, "<RUN_SCHEDULE>", strlen("<RUN_SCHEDULE>")))
	{
		return RunSchedule(cmd, sock);
	}	
	//Other Schedule
	else if (!strncmp(cmd, "<GET_OTHER_SCHEDULE_LIST>", strlen("<GET_OTHER_SCHEDULE_LIST>")))
	{
		return GetOtherScheduleList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_OTHER_SCHEDULE>", strlen("<GET_OTHER_SCHEDULE>")))
	{
		return GetOtherSchedule(cmd, sock);
	}
	//other export
	else if (!strncmp(cmd, "<ADD_OTHER_SCHEDULE>", strlen("<ADD_OTHER_SCHEDULE>")))
	{
		return AddOtherSchedule(cmd, sock);
	}
	else if (!strncmp(cmd, "<UPDATE_OTHER_SCHEDULE>", strlen("<UPDATE_OTHER_SCHEDULE>")))
	{
		return UpdateOtherSchedule(cmd, sock);
	}
	else if (!strncmp(cmd, "<DELETE_OTHER_SCHEDULE>", strlen("<DELETE_OTHER_SCHEDULE>")))
	{
		return DeleteOtherSchedule(cmd, sock);
	}
	else if (!strncmp(cmd, "<COPY_OTHER_SCHEDULE>", strlen("<COPY_OTHER_SCHEDULE>")))
	{
		return CopyOtherSchedule(cmd, sock);
	}
	else if (!strncmp(cmd, "<ACTIVATE_OTHER_SCHEDULE>", strlen("<ACTIVATE_OTHER_SCHEDULE>")))
	{
		return ActivateOtherSchedule(cmd, sock);
	}
	else if (!strncmp(cmd, "<DEACTIVATE_OTHER_SCHEDULE>", strlen("<DEACTIVATE_OTHER_SCHEDULE>")))
	{
		return DeactivateOtherSchedule(cmd, sock);
	}
	else if (!strncmp(cmd, "<RUN_OTHER_SCHEDULE>", strlen("<RUN_SCHEDULE>")))
	{
		return RunOtherSchedule(cmd, sock);
	}	
	else if (!strncmp(cmd, "<SF_CHECK_SCHEDULE>", strlen("<SF_CHECK_SCHEDULE>")))
	{
		return SFCheckSchedule(cmd, sock);
	}	
	//
	else
	{
		return 1;
	}
}

int ResponseCommandUser(char * cmd, va_sock_t sock)
{
	if (!strncmp(cmd, "<GET_USER_LIST>", strlen("<GET_USER_LIST>")))
	{
		return GetUserList(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_UNREGISTERED_CLIENT_LIST>", strlen("<GET_UNREGISTERED_CLIENT_LIST>")))
	{
		return GetUnregisteredClientList(cmd, sock);
	}
	else if (!strncmp(cmd, "<ADD_USER>", strlen("<ADD_USER>")))
	{
		return AddUser(cmd, sock);
	}
	else if (!strncmp(cmd, "<CHANGE_USER_PASSWD>", strlen("<CHANGE_USER_PASSWD>")))
	{
		return ChangeUserPasswd(cmd, sock);
	}
	else if (!strncmp(cmd, "<DELETE_USER>", strlen("<DELETE_USER>")))
	{
		return DeleteUser(cmd, sock);
	}
	else if (!strncmp(cmd, "<CHANGE_USER_PERMISSION>", strlen("<CHANGE_USER_PERMISSION>")))
	{
		return ChangeUserPermission(cmd, sock);
	}
	else if (!strncmp(cmd, "<CHANGE_USER_INFO>", strlen("<CHANGE_USER_INFO>")))
	{
		return ChangeUserInfo(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_USER_GROUP_LIST>", strlen("<GET_USER_GROUP_LIST>")))
	{
		return GetUserGroupList(cmd, sock);
	}
	else if (!strncmp(cmd, "<ADD_USER_GROUP>", strlen("<ADD_USER_GROUP>")))
	{
		return AddUserGroup(cmd, sock);
	}
	else if (!strncmp(cmd, "<DELETE_USER_GROUP>", strlen("<DELETE_USER_GROUP>")))
	{
		return DeleteUserGroup(cmd, sock);
	}
	else if (!strncmp(cmd, "<RENAME_USER_GROUP>", strlen("<RENAME_USER_GROUP>")))
	{
		return RenameUserGroup(cmd, sock);
	}
	else if (!strncmp(cmd, "<CHANGE_USER_GROUP_PERMISSION>", strlen("<CHANGE_USER_GROUP_PERMISSION>")))
	{
		return ChangeUserGroupPermission(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_USER_POOL_LIST>", strlen("<GET_USER_POOL_LIST>")))
	{
		return GetUserPoolList(cmd, sock);
	}
	else if (!strncmp(cmd, "<ADD_USER_POOL>", strlen("<ADD_USER_POOL>")))
	{
		return AddUserPool(cmd, sock);
	}
	else if (!strncmp(cmd, "<DELETE_USER_POOL>", strlen("<DELETE_USER_POOL>")))
	{
		return DeleteUserPool(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_USER_NODE_LIST>", strlen("<GET_USER_NODE_LIST>")))
	{
		return GetUserNodeList(cmd, sock);
	}
	else if (!strncmp(cmd, "<ADD_USER_NODE>", strlen("<ADD_USER_NODE>")))
	{
		return AddUserNode(cmd, sock);
	}
	else if (!strncmp(cmd, "<DELETE_USER_NODE>", strlen("<DELETE_USER_NODE>")))
	{
		return DeleteUserNode(cmd, sock);
	}
	else if (!strncmp(cmd, "<DELETE_ALL_USER_POOL>", strlen("<DELETE_ALL_USER_POOL>")))
	{
		return DeleteAllUserPool(cmd, sock);
	}
	else if (!strncmp(cmd, "<DELETE_ALL_USER_NODE>", strlen("<DELETE_ALL_USER_NODE>")))
	{
		return DeleteAllUserNode(cmd, sock);
	}
	else
	{
		return 1;
	}
}

int ResponseCommandFrontPage(char * cmd, va_sock_t sock)
{
	if (!strncmp(cmd, "<GET_FRONT_PAGE_NODE_INFO>", strlen("<GET_FRONT_PAGE_NODE_INFO>")))
	{
		return GetFrontPageNodeInfo(cmd, sock);
	}
	if (!strncmp(cmd, "<GET_FRONT_PAGE_INFO>", strlen("<GET_FRONT_PAGE_INFO>")))
	{
		return GetFrontPageInfo(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_BACKUP_JOBS>", strlen("<GET_BACKUP_JOBS>")))
	{
		return GetBackupJobs(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_CURRENT_AND_FUTURE_JOBS>", strlen("<GET_CURRENT_AND_FUTURE_JOBS>")))
	{
		return GetCurrentAndFutureJobs(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_STORAGE_INFO>", strlen("<GET_STORAGE_INFO>")))
	{
		return GetStorageInfo(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_DATABASE_AGENT_NUMBER>", strlen("<GET_DATABASE_AGENT_NUMBER>")))
	{
		return GetDataBaseAgentNumber(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_MASTER_SERVER_STATUS>", strlen("<GET_MASTER_SERVER_STATUS>")))
	{
		return GetMasterServerStatus(cmd, sock);
	}
	else if (!strncmp(cmd, "<GET_CHECK_PROCESS_USAGE>", strlen("<GET_CHECK_PROCESS_USAGE>")))
	{
		return GetCheckProcessUsage(cmd, sock);
	}
	else
	{
		return 1;
	}
}

int ResponseCommandWebCommand(va_sock_t sock, char * cmd)
{
	if (!strncmp(cmd, "<GET_FRONT_PAGE_NODE_INFO>", strlen("<GET_FRONT_PAGE_NODE_INFO>")))
	{
		return GetFrontPageNodeInfo(cmd, sock);
	}
	else
	{
		return 1;
	}
}


int RegisterMasterUser()
{
	va_fd_t fdUser;
	struct abioUser user;
	char passwd[PASSWORD_LENGTH];
	char encryptPasswd[ENCRYPTED_PASSWORD_LENGTH];
	
	time_t current_t;
	
	if ((fdUser = va_open(ABIOMASTER_DATA_FOLDER, ABIOMASTER_USER_LIST, O_CREAT|O_RDWR, 1, 1)) != (va_fd_t)-1)
	{		
		while (va_read(fdUser, &user, sizeof(struct abioUser), DATA_TYPE_ABIO_USER) > 0)
		{
			if(!strcmp(user.account, ABIOMASTER_DEFAULT_USER_NAME))
			{
				break;
			}
		}
		
		// user 가 없거나 삭제 되어있으면 ubkadmin 유저를 추가한다.
		if (user.account[0] == '\0')
		{
			memset(&user, 0, sizeof(struct abioUser));
			user.level = ABIO_USER_LEVEL_ADMIN;
			strcpy(user.account, ABIOMASTER_DEFAULT_USER_NAME);
			strcpy(passwd, ABIOMASTER_DEFAULT_USER_PASSWORD);

			memset(&encryptPasswd, 0, sizeof(encryptPasswd));
			if (MD5String(passwd, (int)strlen(passwd), (unsigned char *)encryptPasswd) != 0)
			{
				strcpy(encryptPasswd, passwd);
			}
			strcpy(user.passwd, encryptPasswd);

			time(&current_t);
			sprintf(user.userID, "uid_%d", current_t);

			sprintf(user.permission, "%d", USER_TOP_PERMISSION);
			sprintf(user.backupType, "%d", USER_ALL_BACKUPTYPE);
			sprintf(user.restoreType, "%d", USER_ALL_RESTORETYPE);

			user.version = CURRENT_VERSION_INTERNAL;
			user.exist = ABIO_USER_EXIST;

			va_lseek(fdUser, 0, SEEK_SET);
			va_write(fdUser, &user, sizeof(struct abioUser), DATA_TYPE_ABIO_USER);
		}
		else if (user.exist == ABIO_USER_EXPIRE)
		{
			user.exist = ABIO_USER_EXIST;
			va_lseek(fdUser, -(va_offset_t)sizeof(struct abioUser), SEEK_CUR);
			va_write(fdUser, &user, sizeof(struct abioUser), DATA_TYPE_ABIO_USER);
		}
		
		va_close(fdUser);
	}
	else
	{
		printf("It failed to make a file \"%s\".\n", ABIOMASTER_USER_LIST);
		PrintDaemonLog("It failed to make a file \"%s\".", ABIOMASTER_USER_LIST);
		
		return -1;
	}
	return 0;
}

int RegisterMasterUserGroup()
{
	va_fd_t fdGroup;
	struct abioUserGroup group;
	
	time_t current_t;

	if ((fdGroup = va_open(ABIOMASTER_DATA_FOLDER, ABIOMASTER_USER_GROUP_LIST, O_CREAT|O_RDWR, 1, 1)) != (va_fd_t)-1)
	{		
		while (va_read(fdGroup, &group, sizeof(struct abioUserGroup), DATA_TYPE_ABIO_USER_GROUP) > 0)
		{
			if(!strcmp(group.groupName, "GROUP"))
			{
				break;
			}
		}
		
		// default group 이 없으면 GROUP 을 추가한다.
		if (group.groupName[0] == '\0')
		{
			memset(&group, 0, sizeof(struct abioUserGroup));
			group.level = ABIO_USER_LEVEL_ADMIN;
			strcpy(group.groupName, "GROUP");

			time(&current_t);
			sprintf(group.groupID, "gid_%d", current_t);

			sprintf(group.permission, "%d", USER_TOP_PERMISSION);
			group.version = CURRENT_VERSION_INTERNAL;
			group.exist = ABIO_USER_EXIST;
			
			va_lseek(fdGroup, 0, SEEK_SET);
			va_write(fdGroup, &group, sizeof(struct abioUserGroup), DATA_TYPE_ABIO_USER_GROUP);
		}
		else if (group.exist == ABIO_USER_EXPIRE)
		{
			group.exist = ABIO_USER_EXIST;
			va_lseek(fdGroup, -(va_offset_t)sizeof(struct abioUser), SEEK_CUR);
			va_write(fdGroup, &group, sizeof(struct abioUserGroup), DATA_TYPE_ABIO_USER_GROUP);
		}
		
		va_close(fdGroup);
	}
	else
	{
		printf("It failed to make a file \"%s\".\n", ABIOMASTER_USER_GROUP_LIST);
		PrintDaemonLog("It failed to make a file \"%s\".", ABIOMASTER_USER_GROUP_LIST);
		
		return -1;
	}
	return 0;
}

int RegisterMasterServer()
{
	va_fd_t fdNode;
	struct abioNode node;
	enum platformType platformType;
	__int64 tmpValidationDate;
	
	
	
	platformType = GetPlatformType();
	
	if ((fdNode = va_open(ABIOMASTER_DATA_FOLDER, ABIOMASTER_NODE_LIST, O_CREAT|O_RDWR, 1, 1)) != (va_fd_t)-1)
	{
		// 기존에 master server가 등록되어 있으면 master server를 client로 변경하고, hostid를 삭제한다.
		while (va_read(fdNode, &node, sizeof(struct abioNode), DATA_TYPE_NODE) > 0)
		{
			if (node.nodeType == NODE_MASTER_SERVER)
			{
				node.nodeType = NODE_LAN_CLIENT;
				memset(node.hostid, 0, sizeof(node.hostid));
				
				va_lseek(fdNode, -(va_offset_t)sizeof(struct abioNode), SEEK_CUR);
				va_write(fdNode, &node, sizeof(struct abioNode), DATA_TYPE_NODE);
				
				break;
			}
		}
		
		// master server와 이름이 같은 node가 있으면 master server로 변경한다.
		va_lseek(fdNode, 0, SEEK_SET);
		while (va_read(fdNode, &node, sizeof(struct abioNode), DATA_TYPE_NODE) > 0)
		{
			if (!strcmp(node.name, masterNodename))
			{
				tmpValidationDate = node.validationDate;
				
				memset(&node, 0, sizeof(struct abioNode));
				node.version = CURRENT_VERSION_INTERNAL;
				strcpy(node.name, masterNodename);
				strcpy(node.ip, masterIP);
				strcpy(node.port, masterPort);
				strcpy(node.hostid, hostid);
				node.platformType = platformType; 
				node.nodeType = NODE_MASTER_SERVER;
				node.validationDate = tmpValidationDate;
				
				va_lseek(fdNode, -(va_offset_t)sizeof(struct abioNode), SEEK_CUR);
				va_write(fdNode, &node, sizeof(struct abioNode), DATA_TYPE_NODE);
				
				break;
			}
		}
		
		// master server가 등록되어 있지 않으면 새로 master server를 등록한다.
		if (node.name[0] == '\0')
		{
			memset(&node, 0, sizeof(struct abioNode));
			node.version = CURRENT_VERSION_INTERNAL;
			strcpy(node.name, masterNodename);
			strcpy(node.ip, masterIP);
			strcpy(node.port, masterPort);
			strcpy(node.hostid, hostid);
			node.platformType = platformType; 
			node.nodeType = NODE_MASTER_SERVER;
			
			va_write(fdNode, &node, sizeof(struct abioNode), DATA_TYPE_NODE);
		}
		
		va_close(fdNode);
	}
	else
	{
		printf("It failed to make a file \"%s\".\n", ABIOMASTER_NODE_LIST);
		PrintDaemonLog("It failed to make a file \"%s\".", ABIOMASTER_NODE_LIST);
		
		return -1;
	}
	
	
	return 0;
}

enum platformType GetPlatformType()
{
	#ifdef __ABIO_AIX
		return PLATFORM_AIX;
	#elif __ABIO_HP
		return PLATFORM_HP;
	#elif __ABIO_LINUX
		return PLATFORM_LINUX;
	#elif __ABIO_SOLARIS
		return PLATFORM_SOLARIS;
	#elif __ABIO_UNIXWARE
		return PLATFORM_UNIXWARE;
	#elif __ABIO_TRU64
		return PLATFORM_TRU64;
	#elif __ABIO_FREEBSD
		return PLATFORM_FREEBSD;
	#elif __ABIO_WIN
		return PLATFORM_WINDOWS;
	#endif
}

int RegisterSystemVolumePool()
{
	va_fd_t fdVolumePool;
	struct volumePool volumePool;
	struct volumePool volumePoolAdd;
	
	
	
	if ((fdVolumePool = va_open(ABIOMASTER_DATA_FOLDER, ABIOMASTER_VOLUME_POOL_LIST, O_CREAT|O_RDWR, 1, 1)) != (va_fd_t)-1)
	{
		// scratch pool을 등록한다.
		while (va_read(fdVolumePool, &volumePool, sizeof(struct volumePool), DATA_TYPE_VOLUME_POOL) > 0)
		{
			if (!strcmp(volumePool.name, ABIOMASTER_SCRATCH_POOL_NAME))
			{
				break;
			}
		}
		
		if (strcmp(volumePool.name, ABIOMASTER_SCRATCH_POOL_NAME) != 0)
		{
			memset(&volumePoolAdd, 0, sizeof(struct volumePool));
			volumePoolAdd.version = CURRENT_VERSION_INTERNAL;
			strcpy(volumePoolAdd.name, ABIOMASTER_SCRATCH_POOL_NAME);
			volumePoolAdd.type = VOLUME_POOL_SCRATCH;
			
			va_lseek(fdVolumePool, 0, SEEK_END);
			va_write(fdVolumePool, &volumePoolAdd, sizeof(struct volumePool), DATA_TYPE_VOLUME_POOL);
		}
		
		
		// catalog pool을 등록한다.
		va_lseek(fdVolumePool, 0, SEEK_SET);
		while (va_read(fdVolumePool, &volumePool, sizeof(struct volumePool), DATA_TYPE_VOLUME_POOL) > 0)
		{
			if (!strcmp(volumePool.name, ABIOMASTER_CATALOG_POOL_NAME))
			{
				break;
			}
		}
		
		if (strcmp(volumePool.name, ABIOMASTER_CATALOG_POOL_NAME) != 0)
		{
			memset(&volumePoolAdd, 0, sizeof(struct volumePool));
			volumePoolAdd.version = CURRENT_VERSION_INTERNAL;
			strcpy(volumePoolAdd.name, ABIOMASTER_CATALOG_POOL_NAME);
			volumePoolAdd.type = VOLUME_POOL_CATALOG;
			
			va_lseek(fdVolumePool, 0, SEEK_END);
			va_write(fdVolumePool, &volumePoolAdd, sizeof(struct volumePool), DATA_TYPE_VOLUME_POOL);
		}
		//#issue #447
		// cleaning pool을 등록한다.
		va_lseek(fdVolumePool, 0, SEEK_SET);
		while (va_read(fdVolumePool, &volumePool, sizeof(struct volumePool), DATA_TYPE_VOLUME_POOL) > 0)
		{
			if (!strcmp(volumePool.name, ABIOMASTER_CLEANING_POOL_NAME))
			{
				break;
			}
		}
		
		if (strcmp(volumePool.name, ABIOMASTER_CLEANING_POOL_NAME) != 0)
		{
			memset(&volumePoolAdd, 0, sizeof(struct volumePool));
			volumePoolAdd.version = CURRENT_VERSION_INTERNAL;
			strcpy(volumePoolAdd.name, ABIOMASTER_CLEANING_POOL_NAME);
			volumePoolAdd.type = VOLUME_POOL_CLEANING;
			
			va_lseek(fdVolumePool, 0, SEEK_END);
			va_write(fdVolumePool, &volumePoolAdd, sizeof(struct volumePool), DATA_TYPE_VOLUME_POOL);
		}
		
		
		va_close(fdVolumePool);
	}
	else
	{
		printf("It failed to make a file \"%s\".\n", ABIOMASTER_VOLUME_POOL_LIST);
		PrintDaemonLog("It failed to make a file \"%s\".", ABIOMASTER_VOLUME_POOL_LIST);
		
		return -1;
	}
	
	
	return 0;
}

// #issue 170 : Retry Backup
/*
int SetNodeInfo(char * nodename, struct ckClient * ckClient, struct ndmpNode * ndmp)
{
	va_fd_t fdNode;
	struct abioNode node;
			
	// client setting
	if ((fdNode = va_open(ABIOMASTER_DATA_FOLDER, ABIOMASTER_NODE_LIST, O_RDONLY, 1, 1)) != (va_fd_t)-1)
	{
		while (va_read(fdNode, &node, sizeof(struct abioNode), DATA_TYPE_NODE) > 0)
		{
	
			if (!strcmp(node.name, nodename))
			{
				ckClient->platformType = node.platformType;
				ckClient->nodeType = node.nodeType;
				strcpy(ckClient->nodename, node.name);
				strcpy(ckClient->ip, node.ip);
				strcpy(ckClient->port, node.port);
				strcpy(ckClient->masterIP, masterIP);
				strcpy(ckClient->masterPort, masterPort);
				strcpy(ckClient->masterLogPort, masterLogPort);
				
				//2012.02.23 add for vmware ky88.
				if (node.nodeType == NODE_NDMP_CLIENT || node.nodeType == NODE_VMWARE_CLIENT)
				{
					strcpy(ndmp->account, node.ndmp.account);
					strcpy(ndmp->passwd, node.ndmp.passwd);
				}
				
				break;
			}
		}
		
		va_close(fdNode);
		
		if (strcmp(node.name, nodename) != 0)
		{
			return -1;
		}
	}
	else
	{
		return -1;
	}
	
	
	return 0;
}

int CheckNodeAlive(char * ip, char * port)
{
	va_sock_t sock;
	
	
	
	if ((sock = va_connect(ip, port, 0)) != -1)
	{
		va_close_socket(sock, ABIO_SOCKET_CLOSE_CLIENT);
	}
	else
	{
		return -1;
	}
	
	
	return 0;
}
*/

int ComparatorString(const void * a1, const void * a2)
{
	char ** l1;
	char ** l2;
	
	
	
	l1 = (char **)a1;
	l2 = (char **)a2;
	
	return strcmp(*l1, *l2);
}

int GetTapeLibraryInventory(char * nodename, char * device, struct tapeInventory * inventory)
{
	char reply[DSIZ][DSIZ];
	char msg[DSIZ * DSIZ * 4];
	
	struct ck command;
	struct ckBackup commandData;
	
	va_sock_t commandSock;
	va_sock_t masterSock;
	va_sock_t clientSock;
	
	int recvSize;
	
	int elementNumber;
	
	int i;
	int r;
	
	
	// initialize variables
	memset(&command, 0, sizeof(struct ck));
	memset(&commandData, 0, sizeof(struct ckBackup));
	
	elementNumber = 0;
	r = -1;
	
	
	// client setting
	if (SetNodeInfo(nodename, &commandData.client, &commandData.ndmp) == 0)
	{
		// client가 살아있는지 확인한다.
		if (CheckNodeAlive(commandData.client.ip, commandData.client.port) < 0)
		{
			va_write_error_code(ERROR_LEVEL_ERROR, 0, 0, 0, &commandData);
		}
	}
	else
	{
		va_write_error_code(ERROR_LEVEL_ERROR, 0, 0, 0, &commandData);
	}
	
	
	if (!GET_ERROR(commandData.errorCode))
	{
		command.request = 1;
		command.reply = 0;
		strcpy(command.subCommand, "<GET_TAPE_LIBRARY_INVENTORY>");
		strcpy(command.executeCommand, MODULE_NAME_MASTER_HTTPD);
		strcpy(command.sourceIP, masterIP);
		strcpy(command.sourcePort, masterPort);
		strcpy(command.sourceNodename, masterNodename);
		
		if (commandData.client.nodeType == NODE_NDMP_CLIENT)
		{
			strcpy(command.requestCommand, MODULE_NAME_NDMP_HTTPD);
			strcpy(command.destinationIP, masterIP);
			strcpy(command.destinationPort, masterPort);
			strcpy(command.destinationNodename, masterNodename);
		}
		else
		{
			strcpy(command.requestCommand, MODULE_NAME_CLIENT_HTTPD);
			strcpy(command.destinationIP, commandData.client.ip);
			strcpy(command.destinationPort, commandData.client.port);
			strcpy(command.destinationNodename, commandData.client.nodename);
		}
		
		strcpy(commandData.mountStatus.device, device);
		
		
		// 명령 처리 결과를 받을 server socket을 만든다.
		if ((masterSock = va_make_server_socket_iptable(command.destinationIP, command.sourceIP, portRule, portRuleNumber, commandData.catalogPort)) != -1)
		{
			// 명령을 storage node로 보낸다.
			if ((commandSock = va_connect("127.0.0.1", masterPort, 1)) != -1)
			{
				memcpy(command.dataBlock, &commandData, sizeof(struct ckBackup));
				
				va_send(commandSock, &command, sizeof(struct ck), 0, DATA_TYPE_CK);
				va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
				
				
				// 명령 처리 결과를 기다린다.
				if ((clientSock = va_wait_connection(masterSock, TIME_OUT_HTTPD_REQUEST, NULL)) >= 0)
				{
					r = 0;
					
					while ((recvSize = va_recv(clientSock, msg , sizeof(msg) , 0, DATA_TYPE_NOT_CHANGE)) > 0)
					{						
						
						for (i = 0; i < recvSize; i++)
						{
							if (msg[i] != '\0')
							{
								va_parser(msg + i, reply);
								
								if (!strcmp(reply[280], "1"))
								{
									if (inventory->robotNumber % DEFAULT_MEMORY_FRAGMENT == 0)
									{
										inventory->robot = (struct tapeElement *)realloc(inventory->robot, sizeof(struct tapeElement) * (inventory->robotNumber + DEFAULT_MEMORY_FRAGMENT));
										memset(inventory->robot + inventory->robotNumber, 0, sizeof(struct tapeElement) * DEFAULT_MEMORY_FRAGMENT);
									}
									inventory->robot[inventory->robotNumber].address = atoi(reply[281]);
									inventory->robot[inventory->robotNumber].full = atoi(reply[282]);
									strcpy(inventory->robot[inventory->robotNumber].volume, reply[283]);
									inventory->robotNumber++;
									
									elementNumber++;
								}
								else if (!strcmp(reply[280], "2"))
								{
									if (inventory->storageNumber % DEFAULT_MEMORY_FRAGMENT == 0)
									{
										inventory->storage = (struct tapeElement *)realloc(inventory->storage, sizeof(struct tapeElement) * (inventory->storageNumber + DEFAULT_MEMORY_FRAGMENT));
										memset(inventory->storage + inventory->storageNumber, 0, sizeof(struct tapeElement) * DEFAULT_MEMORY_FRAGMENT);
									}
									inventory->storage[inventory->storageNumber].address = atoi(reply[281]);
									inventory->storage[inventory->storageNumber].full = atoi(reply[282]);
									strcpy(inventory->storage[inventory->storageNumber].volume, reply[283]);
									inventory->storageNumber++;
									
									elementNumber++;
								}
								else if (!strcmp(reply[280], "3"))
								{
									if (inventory->ioNumber % DEFAULT_MEMORY_FRAGMENT == 0)
									{
										inventory->io = (struct tapeElement *)realloc(inventory->io, sizeof(struct tapeElement) * (inventory->ioNumber + DEFAULT_MEMORY_FRAGMENT));
										memset(inventory->io + inventory->ioNumber, 0, sizeof(struct tapeElement) * DEFAULT_MEMORY_FRAGMENT);
									}
									inventory->io[inventory->ioNumber].address = atoi(reply[281]);
									inventory->io[inventory->ioNumber].full = atoi(reply[282]);
									strcpy(inventory->io[inventory->ioNumber].volume, reply[283]);
									inventory->ioNumber++;
									
									elementNumber++;
								}
								else if (!strcmp(reply[280], "4"))
								{
									if (inventory->driveNumber % DEFAULT_MEMORY_FRAGMENT == 0)
									{
										inventory->drive = (struct tapeElement *)realloc(inventory->drive, sizeof(struct tapeElement) * (inventory->driveNumber + DEFAULT_MEMORY_FRAGMENT));
										memset(inventory->drive + inventory->driveNumber, 0, sizeof(struct tapeElement) * DEFAULT_MEMORY_FRAGMENT);
									}
									inventory->drive[inventory->driveNumber].address = atoi(reply[281]);
									inventory->drive[inventory->driveNumber].full = atoi(reply[282]);
									strcpy(inventory->drive[inventory->driveNumber].volume, reply[283]);
									strcpy(inventory->drive[inventory->driveNumber].serial, reply[284]);
									inventory->driveNumber++;
									
									elementNumber++;
								}

								else
								{
									WriteDebugData(ABIOMASTER_LOG_FOLDER, ABIOMASTER_DEBUG, DEBUG_CODE_DETAIL " reply[280] : %s - Can't read Tape Library Inventory.",MODULE_NAME_MASTER_HTTPD,__FUNCTION__ ,reply[280]);
								/*	elementNumber = -1;
									
									break;*/
								}
								
								i += (int)strlen(msg + i);
							}
						}
					}
					
					va_close_socket(clientSock, ABIO_SOCKET_CLOSE_SERVER);
				}
			}
			
			va_close_socket(masterSock, 0);
		}
	}
	
	
	if (r == 0)
	{
		return elementNumber;
	}
	else
	{
		return -2;
	}
}

int MoveMedium(char * nodename, char * device, int source, int destination, enum elementType sourceElementType, enum elementType destinationElementType, struct ckBackup * commandData)
{
	struct ck moveCommand;
	struct ckBackup moveCommandData;
	
	va_sock_t commandSock;
	va_sock_t masterSock;
	va_sock_t clientSock;
	
	int timeoutCnt;
	
	
	// initialize variables
	memset(&moveCommand, 0, sizeof(struct ck));
	memset(&moveCommandData, 0, sizeof(struct ckBackup));
	
	timeoutCnt = 0;
	
	// client setting
	if (SetNodeInfo(nodename, &moveCommandData.client, &moveCommandData.ndmp) == 0)
	{
		// client가 살아있는지 확인한다.
		if (CheckNodeAlive(moveCommandData.client.ip, moveCommandData.client.port) < 0)
		{
			va_write_error_code(ERROR_LEVEL_ERROR, MODULE_MASTER_HTTPD_MEDIA, FUNCTION_MEDIA_MoveMedium, ERROR_NETWORK_CLIENT_DOWN, &moveCommandData);
		}
	}
	else
	{
		va_write_error_code(ERROR_LEVEL_ERROR, MODULE_MASTER_HTTPD_MEDIA, FUNCTION_MEDIA_MoveMedium, ERROR_NODE_FIND_NODE, &moveCommandData);
	}
	
	
	if (!GET_ERROR(moveCommandData.errorCode))
	{
		moveCommand.request = 1;
		moveCommand.reply = 0;
		strcpy(moveCommand.subCommand, "<MOVE_MEDIUM>");
		strcpy(moveCommand.executeCommand, MODULE_NAME_MASTER_HTTPD);
		strcpy(moveCommand.sourceIP, masterIP);
		strcpy(moveCommand.sourcePort, masterPort);
		strcpy(moveCommand.sourceNodename, masterNodename);
		
		if (moveCommandData.client.nodeType == NODE_NDMP_CLIENT)
		{
			strcpy(moveCommand.requestCommand, MODULE_NAME_NDMP_HTTPD);
			strcpy(moveCommand.destinationIP, masterIP);
			strcpy(moveCommand.destinationPort, masterPort);
			strcpy(moveCommand.destinationNodename, masterNodename);
		}
		else
		{
			strcpy(moveCommand.requestCommand, MODULE_NAME_CLIENT_HTTPD);
			strcpy(moveCommand.destinationIP, moveCommandData.client.ip);
			strcpy(moveCommand.destinationPort, moveCommandData.client.port);
			strcpy(moveCommand.destinationNodename, moveCommandData.client.nodename);
		}
		
		strcpy(moveCommandData.mountStatus.device, device);
		moveCommandData.mountStatus.volumeAddress = source;
		moveCommandData.mountStatus.driveAddress = destination;
		moveCommandData.mountStatus.mtype = sourceElementType;
		moveCommandData.mountStatus.mtype2 = destinationElementType;
		
		
		//#ISSUE144 ERROR_NETWORK_TIMEOUT시 재시도
		for(timeoutCnt = 0 ; timeoutCnt < NETWORK_TIME_OUT ; timeoutCnt++)
		{
			// 명령 처리 결과를 받을 server socket을 만든다.
			if ((masterSock = va_make_server_socket_iptable(moveCommand.destinationIP, moveCommand.sourceIP, portRule, portRuleNumber, moveCommandData.catalogPort)) != -1)
			{
				// 명령을 storage node로 보낸다.
				if ((commandSock = va_connect("127.0.0.1", masterPort, 1)) != -1)
				{
					memcpy(moveCommand.dataBlock, &moveCommandData, sizeof(struct ckBackup));
					
					va_send(commandSock, &moveCommand, sizeof(struct ck), 0, DATA_TYPE_CK);
					va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
					
					
					// 명령 처리 결과를 기다린다.
					if ((clientSock = va_wait_connection(masterSock, TIME_OUT_HTTPD_REQUEST, NULL)) >= 0)
					{
						if (va_recv(clientSock, &moveCommandData, sizeof(struct ckBackup), 0, DATA_TYPE_CK_BACKUP) != sizeof(struct ckBackup))
						{
							va_write_error_code(ERROR_LEVEL_ERROR, MODULE_MASTER_HTTPD_MEDIA, FUNCTION_MEDIA_MoveMedium, ERROR_NETWORK_CLIENT_DOWN, &moveCommandData);
						}
						
						va_close_socket(clientSock, ABIO_SOCKET_CLOSE_SERVER);
					}
					else
					{
						//ERROR_NETWORK_TIMEOUT시 Log를 남기고 재시도
						va_write_error_code(ERROR_LEVEL_ERROR, MODULE_MASTER_HTTPD_MEDIA, FUNCTION_MEDIA_MoveMedium, ERROR_NETWORK_TIMEOUT, &moveCommandData);
						va_close_socket(masterSock, 0);
						continue;
					}
				}
				else
				{
					va_write_error_code(ERROR_LEVEL_ERROR, MODULE_MASTER_HTTPD_MEDIA, FUNCTION_MEDIA_MoveMedium, ERROR_NETWORK_CK_DOWN, &moveCommandData);
				}
				
				va_close_socket(masterSock, 0);
			}
			else
			{
				va_write_error_code(ERROR_LEVEL_ERROR, MODULE_MASTER_HTTPD_MEDIA, FUNCTION_MEDIA_MoveMedium, ERROR_NETWORK_PORT_BIND, &moveCommandData);
			}
			break;
		}
	}
	
	
	if (!GET_ERROR(moveCommandData.errorCode))
	{
		return 0;
	}
	else
	{
		if (commandData != NULL)
		{
			va_copy_error_code(commandData->errorCode, moveCommandData.errorCode);
		}
		
		return -1;
	}
}

void CloseThread()
{
	if (httpdComplete == 0)
	{
		httpdComplete = 1;
	}
	
	va_release_semaphore(semidLog);
	va_release_semaphore(semidLicense);
}

void Exit()
{
	int i;
	
	
	
	CloseThread();
	
	// release system resources
	va_close_socket(httpdSock, 0);
	va_close_socket(httpdLogSock, 0);
	va_remove_semaphore(semidLog);
	va_remove_semaphore(semidLicense);
	
	for (i = 0; i < nodeipListNumber; i++)
	{
		va_free(nodeipList[i]);
	}
	va_free(nodeipList);
	
	va_free(portRule);
	
	for (i = 0; i < allowedIPNumber; i++)
	{
		va_free(allowedIP[i]);
	}
	va_free(allowedIP);
	
	va_free(licenseSummary);
	
	#ifdef __ABIO_WIN
		//윈속 제거
		WSACleanup();
		
		#ifndef _DEBUG
			// 서비스 중지
			Httpd_set_status(SERVICE_STOPPED, 0);
		#endif
	#endif
	
	printf("The httpd service was stoped.\n");
	PrintDaemonLog("The httpd service was stoped.");
}

#ifdef __ABIO_UNIX	
	va_thread_return_t WaitChild(void * arg)
	{
		int pid;
		
		
		
		va_sleep(1);
		
		while (1)
		{
			if ((pid = waitpid(-1, NULL, WNOHANG)) > 0)
			{
				// child process가 종료되었다.
			}
			else
			{
				if (httpdComplete == 1)
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

#ifdef __ABIO_WIN
	void Httpd_service_handler(DWORD fdwcontrol)
	{
		if (fdwcontrol == g_nowstate)
		{
			return ;
		}
		
		switch(fdwcontrol)
		{
			case SERVICE_CONTROL_STOP :
			{
				Httpd_set_status(SERVICE_STOP_PENDING, 0); 
				
				CloseThread();
				
				break;
			}
			
			case SERVICE_CONTROL_INTERROGATE :
			{
				
			}
			
			default :
			{
				Httpd_set_status(g_nowstate, SERVICE_ACCEPT_STOP);
				break ;
			}
		}
	}
	
	void Httpd_set_status(DWORD dwstate, DWORD dwaccept)
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
	
	va_append_text_file_lines(ABIOMASTER_CK_FOLDER, ABIOMASTER_HTTPD_LOG_FILE, &serviceLogMessage, NULL, 1);
	
	va_free(serviceLogMessage);
}

void initCommandDataSetCommandIDIP(struct ckBackup * pCommandData, char value[][1024])
{
	memset(pCommandData, 0, sizeof(struct ckBackup));
	strcpy(pCommandData->loginID, value[520]);
	strcpy(pCommandData->loginIP, value[521]);
}

//----------------------------------------------------------

int GetCommand(char * cmd, va_sock_t sock)
{
	char value[DSIZ][DSIZ];
	int valueNumber;
	
	va_fd_t fdFile;
	char file[MAX_PATH_LENGTH];
	
	char buf[8192];
	int readSize;
	
	int i;
	
	memset(file, 0, sizeof(file));
	
	for (i = 0; i < DSIZ; i++)
	{
		memset(value[i], 0, sizeof(value[i]));
	}
	valueNumber = get_argument(cmd, value);

	if (value[0][0] == 0)
	{
		sprintf(file, "%s%c%s%c%s", ABIOMASTER_MASTER_SERVER_FOLDER, FILE_PATH_DELIMITER,"interface",FILE_PATH_DELIMITER, "index.html");
		goto OPEN_PAGE;
	}
	else if (strstr(value[0], "data/test.json" ))
	{		
		WebGetTerminalJobLog(cmd,sock,value,valueNumber);
	}
	else if (strstr(value[0], "data/login.json" ))
	{		
		Web_LogIn(cmd,sock,value,valueNumber,"");
	}
	else if (strstr(value[0], "data/frontnodeinfo.json" ))
	{		
		WebGetFrontPageNodeInfo(cmd,sock,value,valueNumber);
	}
	else if (strstr(value[0], "data/frontpageinfo.json" ))
	{		
		WebGetFrontPageInfo(cmd,sock,value,valueNumber);
	}
	else if (strstr(value[0], "data/frontstorageinfo.json" ))
	{		
		WebGetStorageInfo(cmd,sock,value,valueNumber);
	}
	else if (strstr(value[0], "data/runningThroughput.json" ))
	{		
		WebGetRunningJobThroughput(cmd,sock,value,valueNumber);
	}
	else if (strstr(value[0], "data/finishedlog.json" ))
	{		
		WebGetTerminalJobLog(cmd,sock,value,valueNumber);
	}
	else if (strstr(value[0], "data/runninglog.json" ))
	{		
		WebGetRunningJobLog(cmd,sock,value,valueNumber);
	}
	else if (strstr(value[0], "data/frontDatabaseAgentNumber.json" ))
	{		
		WebGetDataBaseAgentNumber(cmd,sock,value,valueNumber);
	}		
	else
	{		
		#ifdef __ABIO_WIN
			// catalog db에는 unix file format으로 저장되어있기 때문에 windows file format으로 변환한다.
			va_change_slash_to_backslash(value[0]);				
		#endif
		//sprintf(file, "%s%c%s", ABIOMASTER_MASTER_SERVER_FOLDER, FILE_PATH_DELIMITER, value[0]);
		sprintf(file, "%s%c%s%c%s", ABIOMASTER_MASTER_SERVER_FOLDER, FILE_PATH_DELIMITER,"interface",FILE_PATH_DELIMITER, value[0]);		

OPEN_PAGE:
		if ((fdFile = va_open(NULL, file, O_RDONLY,0, 0)) != 0)
		{
			response_ok(sock,file,200);
			

			memset(buf,0,sizeof(buf));
			
			while ((readSize = va_read(fdFile, buf, sizeof(buf), DATA_TYPE_NOT_CHANGE)) > 0)
			{
				va_send(sock, buf, readSize, 0, DATA_TYPE_NOT_CHANGE);
			}
			
			va_close(fdFile);
		}
		else
		{
			response_not_found(file, sock);
		}		
	}

	va_close_socket(sock,ABIO_SOCKET_CLOSE_CLIENT);		

	return 0;
}

int PostCommand(char * cmd, va_sock_t sock)
{
	char value[DSIZ][DSIZ];
	int valueNumber;
		
	char file[MAX_PATH_LENGTH];
		
	int i;

	memset(file, 0, sizeof(file));
	
	for (i = 0; i < DSIZ; i++)
	{
		memset(value[i], 0, sizeof(value[i]));
	}
	
	valueNumber = get_argument(cmd, value);
	
	if (strstr(value[0], "LOGIN" ) || strstr(value[0], "login"))
	{
		if(0 < Web_LogIn(cmd , sock,value,valueNumber, ""))
		{
			WebGetMainPage(cmd , sock);
		}
		else
		{
			SendLoginPage(sock);
		}
		//WebGetTerminalJobLog(cmd,sock);
	}
	else if (strstr(value[0], "TERMINATEDLOG" ) || strstr(value[0], "terminatedLog"))
	{		
		WebGetTerminalJobLog(cmd,sock,value,valueNumber);		
	}
	else if (strstr(value[0], "RUNNINGLOG" ) || strstr(value[0], "runningLog"))
	{		
		WebGetRunningJobLog(cmd,sock,value,valueNumber);		
	}
	else
	{
		//Web_LogIn(cmd , sock, "");
		//WebGetTerminalJobLog(cmd,sock);
		SendLoginPage(sock);
	}

	return 0;
}

int GetRegisterdHostID( char * hostid)
{
	va_fd_t fdNode;
	struct abioNode node;
			
	int result;

	result = 0;
		
	if ((fdNode = va_open(ABIOMASTER_DATA_FOLDER, ABIOMASTER_NODE_LIST, O_RDONLY, 1, 1)) != (va_fd_t)-1)
	{			
		while (va_read(fdNode, &node, sizeof(struct abioNode), DATA_TYPE_NODE) > 0)
		{
			if (!strncmp(node.name, masterNodename,strlen(masterNodename)))
			{
				result = 1;
				strcpy(hostid,node.hostid);				
				break;
			}
		}		
		
		va_close(fdNode);
	}
	else
	{		
		result = -1;
	}
	
	
	return result;
}
