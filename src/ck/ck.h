#ifndef __CK_H__
#define __CK_H__


#define THREAD_TIME_OUT		30


struct actionList
{
	char moduleName[MAX_ACTION_COMMAND_NAME_LENGTH];
	char moduleFile[MAX_ACTION_COMMAND_VALUE_LENGTH];
};

struct mtype_stat
{
	int use;
	#ifdef __ABIO_UNIX
		int pid;
	#elif __ABIO_WIN
		va_trans_t transid;
		HANDLE hProcess;
		HANDLE hThread;
	#endif 
};

struct msg_arg
{
	int pmtype;				// message type
	struct ck command;		// message that will be sent
};

struct msgbuf_ck
{
	struct ck msg;
	char connectionIP[IP_LENGTH];
	struct msgbuf_ck * next;
};

enum processType {PARENT_PROCESS, CHILD_PROCESS};


#ifdef __ABIO_UNIX
	int StartCommunicationDaemon(char * filename);
#elif __ABIO_WIN
	int StartCommunicationDaemon();
#endif

#ifdef __ABIO_UNIX
	int InitProcess(char * filename);
#elif __ABIO_WIN
	int InitProcess();
#endif

int GetModuleConfiguration();
int CheckConfigurationValid();
int LoadActionList();
int InitCK();
int CommunicationKernel();

va_thread_return_t GetMessageFromSocket(void * arg);
va_thread_return_t GetMessageFromTransfer(void * arg);
va_thread_return_t ProcessMessage(void * arg);
int AppendMessageToMessageBuffer(struct ck * requestCommand, char * connectionIP);

int FindAction(struct ck * command, char * requestModuleName, char * action);
int GetMessageType();
int SendMessageToModule(char * action, int pmtype, struct ck * command);
#ifdef __ABIO_WIN
	void SendMessageToNamedPipe(void * arg);
#endif

#ifdef __ABIO_UNIX
	va_thread_return_t WaitChild(void * arg);
#endif

void CloseThread();
void Exit();

void KillChildProcess();

#ifdef __ABIO_WIN
	void CK_service_handler(DWORD fdwcontrol);
	void CK_set_status(DWORD dwstate, DWORD dwaccept);
#endif

void PrintDaemonLog(char * message, ...);


/**************************************************************************************************
 * function prototype : 
 * DESCRIPTION
 *
 * ARGUMENTS
 *
 * RETURN VALUES
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : int StartCommunicationDaemon(); 
 *                      int StartCommunicationDaemon(DWORD argc, LPTSTR * argv);
 * DESCRIPTION
 *     communication kernel�� �����Ѵ�.
 *     process �ʱ�ȭ, communication kernel ���� �ʱ�ȭ, communication kernel ������ ������ �Ѵ�.
 *
 * ARGUMENTS
 *     argc : argument number
 *     argv : argument list
 *
 * RETURN VALUES
 *     success : 0 is returned
 *     fail : 2 is returned
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : int InitProcess();
 * DESCRIPTION
 *     process�� �ʱ�ȭ�Ѵ�.
 *     1. ���� ������ �ʱ�ȭ �ϴ� ��
 *     2. �� �ý��ۿ� �°� process�� �ʱ�ȭ�ϴ� ��
 *     3. process ���࿡ �ʿ��� ���� ������ �о���� ��
 *
 * ARGUMENTS
 *     none
 *
 * RETURN VALUES
 *     success : 0 is returned
 *     fail : return a negative integer
 *         -1 : daemon�̳� service ���·� process�� ����µ� �������� ���
 *         -2 : ���� �۾� ���丮�� �о���µ� ������ ���
 *         -3 : process ���࿡ �ʿ��� ���� ������ �о���µ� �������� ���
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
 *     fail : return a negative integer
 *         -1 : ck.conf ������ ���� ���
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : int InitCK();
 * DESCRIPTION
 *     communication kernel ������ �ʱ�ȭ �Ѵ�.
 *     1. communication kernel�� �ٸ� process���� message�� �ְ�ޱ� ���� ��θ� �����Ѵ�. ��δ� 
 *        TCP/IP server socket�� message transfer(unix������ message queue, windows������ named pipe)
 *        �ΰ����� �����. 
 *     2. communication kernel�� thread�� message buffer ����ȭ�� ���� semaphore�� �����.
 *
 * ARGUMENTS
 *     none
 *
 * RETURN VALUES
 *     success : 0 is returned
 *     fail : return a negative integer
 *         -1 : TCP/IP server socket�� ����µ� �������� ���
 *         -2 : message transfer�� ����µ� �������� ���
 *         -3 : semaphore�� ����µ� �������� ���
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : int CommunicationKernel();
 * DESCRIPTION
 *     communication kernel�� �����Ѵ�. message ���Ű� ������ ���� thread�� ���� message ó����
 *     �� �� �ֵ��� �Ѵ�.
 *
 * ARGUMENTS
 *     none
 *
 * RETURN VALUES
 *     0 : communication kernel�� �������� ������� ����� ���.
 *     -1 : �������� ����� �ƴ� �ٸ� ��η� ����Ǵ� ��� - �ڿ� ����, IPC method ���� ��
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : va_thread_return_t GetMessageFromSocket();
 * DESCRIPTION
 *     TCP/IP server socket���� message�� �޾Ƽ� message processor�� �����Ѵ�.
 *     message buffer�� ���� message�� �ְ�, message processor���� message buffer�� message�� �������� �˸���.
 *
 * ARGUMENTS
 *     none
 *
 * RETURN VALUES
 *     none
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : va_thread_return_t GetMessageFromTransfer();
 * DESCRIPTION
 *     message transfer���� message�� �޾Ƽ� message processor�� �����Ѵ�.
 *     message buffer�� ���� message�� �ְ�, message processor���� message buffer�� message�� �������� �˸���.
 *
 * ARGUMENTS
 *     none
 *
 * RETURN VALUES
 *     none
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : va_thread_return_t ProcessMessage();
 * DESCRIPTION
 *     TCP/IP server socket�� message transfer�κ��� ���� message�� ó���Ѵ�.
 *     request�� execute�� �°� ������ process�� ã�Ƽ� �����ϰ� message�� �����Ѵ�.
 *
 * ARGUMENTS
 *     none
 *
 * RETURN VALUES
 *     none
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : int AppendMessageToMessageBuffer(struct ck * requestCommand, struct sockaddr_in * connectionaddr);
 * DESCRIPTION
 *     TCP/IP server socket�� message transfer���� message�� �޾Ƽ� message buffer�� �����Ѵ�.
 *
 * ARGUMENTS
 *     requestCommand : TCP/IP server socket�� message buffer���� ���� message
 *     connectionaddr : TCP/IP server socket���κ��� message�� ���� ���, message�� ������ node�� ����
 *
 * RETURN VALUES
 *     success : 0 is returned
 *     fail : return a negative integer
 *         -1 : semaphore�� ���ŵǾ ����ȭ�� ������ ���
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : int FindAction(struct ck * command, char * action);
 * DESCRIPTION
 *     action list���� request�� execute ���տ� �°� ������ process�� ã�´�.
 *
 * ARGUMENTS
 *     command : process�� ������ message
 *     action : request�� execute ���տ� �°� action list���� ã�� process
 *
 * RETURN VALUES
 *     success : 0 is returned
 *     fail : return a negative integer
 *         -1 : action list�� ���� ���
 *         -2 : action list���� message�� ������ process�� ã�� ���� ���
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : int GetMessageType();
 * DESCRIPTION
 *     process�� message�� �����Ҷ� ������ process���� message�� ����� ���� �� �ֵ��� �� message��
 *     ����������Ѵ�. �̶� message�� ������ ���� id�� message type�� �����Ѵ�. �� message type�� 
 *     �����Ѵ�.
 *
 * ARGUMENTS
 *     none
 *
 * RETURN VALUES
 *     success : return a positive integer representing message type
 *     fail : -1 is returned. ������ message type�� ���ٴ� ���� child process�� ����� ������� ���� ��찡
 *         �ݺ��ؼ� �߻��ϰ� �ְų�, child process�� ����� ���� ����� �����ϰ� ���� ���ϴٴ� ����.
 *         ���̻� ��밡���� message type�� ���� ������ communication kernel�� �����Ѵ�.
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : int SendMessageToModule(char * action, int pmtype, struct ck * command);
 * DESCRIPTION
 *     message�� ������ process�� �����ϰ� message�� �����Ѵ�.
 *
 * ARGUMENTS
 *     action : message�� ������ process
 *     pmtype : process�� ������ message�� ���� message type
 *     command : process�� ������ message
 *
 * RETURN VALUES
 *     success : 0 is returned
 *     fail : return a negative integer
 *         -1 : child process�� ����µ� ������ ���
 *         -2 : child process���� process ���࿡ ������ ���
 *         -3 : process�� message ���޿� ������ ���
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : va_thread_return_t SendMessageToNamedPipe(void * arg)
 * DESCRIPTION
 *     named pipe�� message�� �����Ѵ�. unix������ �� thread�� ������� �ʴ´�.
 *     windows�� message transfer�� named pipe�� connection base�� �����ϱ� ������ message�� ���޵��� ������ �� 
 *     CK�� block�Ǵ� ���� �����ϱ� ���ؼ� ���� named pipe�� message�� �����ϴ� �κ��� thread�� ���� ó���Ѵ�.
 *
 * ARGUMENTS
 *     arg : 
 *         struct msg_arg
 *         {
 *             va_trans_t transid;		// message transfer id
 *             int pmtype;				// message type
 *             struct ck command;		// message that will be sent
 *         };
 *         
 *         transid : message transfer id. it's meaningless
 *         pmtype : named pipe�� ������ message�� ���� message type
 *         command : named pipe�� ������ message
 *
 * RETURN VALUES
 *     none
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : void CloseThread();
 * DESCRIPTION
 *     ����� thread���� ��� �������� ��θ� ���� ����ǵ����ϴµ� �ʿ��� ��ġ���� ���Ѵ�.
 *     communication kernel������ message ��θ� ������Ѽ� �����ϵ��� �Ѵ�.
 *     unix������ TCP/IP server socket�� �����Ű��, message queue�� �����ϰ�, semaphore�� �����Ѵ�.
 *
 * ARGUMENTS
 *     none
 *
 * RETURN VALUES
 *     none
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : void Exit();
 * DESCRIPTION
 *     communication kernel���� ����� �ڿ����� �ݳ��ϰ�, process�� ������ �غ� �Ѵ�.
 *
 * ARGUMENTS
 *     none
 *
 * RETURN VALUES
 *     none
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : void KillChildProcess();
 * DESCRIPTION
 *     ck�� ��� child process�� �����Ѵ�.
 *
 * ARGUMENTS
 *     none
 *
 * RETURN VALUES
 *     none
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : void StopLibraryd();
 * DESCRIPTION
 *     master server�� ��� library controller�� �����Ѵ�.
 *
 * ARGUMENTS
 *     none
 *
 * RETURN VALUES
 *     none
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : void StopSchedulerd();
 * DESCRIPTION
 *     master server�� ��� scheduler�� �����Ѵ�.
 *
 * ARGUMENTS
 *     none
 *
 * RETURN VALUES
 *     none
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : va_thread_return_t WaitChild(void * arg);
 * DESCRIPTION
 *     unix���� child process�� ����ɶ� �߻��ϴ� signal�� �޾Ƽ� �� child process���� ���� 
 *     message type�� �����ϵ��� ó���Ѵ�. �� signal�� ódkf�������� ������ ���밡���� message type��
 *     �������� process�� message�� ������ �� ���Եȴ�. ���� zombie process�� ��� ���Ƽ� �ý���
 *     �ڿ��� �����ϰ� �ȴ�.
 *
 * ARGUMENTS
 *     none
 *
 * RETURN VALUES
 *     none
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : void CK_service_handler(DWORD fdwcontrol);
 * DESCRIPTION
 *     ������ ���� ��ȣ�� ó���ϴ� �Լ��ν� �ڵ鷯�� ���� ���ο��� ����ó�� ��ϵȴ�.
 *  
 * ARGUMENTS
 *     fdwcontrol : ���񽺰� �ؾ��� �۾� ������ ���� ���� ��ȣ��
 *
 * RETURN VALUES
 *     none
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : void CK_set_status(DWORD dwstate, DWORD dwaccept);
 * DESCRIPTION
 *     ������ ���¸� �����ϰ�, ���� ���� ���¸� �����Ѵ�.
 *  
 * ARGUMENTS
 *     dwstate : ������ �ֱ� ����
 *     dwaccept : �ڵ鷯 �Լ����� ���񽺸� ����ϰ� ó���ϴ� ���� �ڵ�
 *
 * RETURN VALUES
 *     none
 *************************************************************************************************/


#endif
