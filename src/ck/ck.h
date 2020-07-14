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
 *     communication kernel을 시작한다.
 *     process 초기화, communication kernel 설정 초기화, communication kernel 시작의 순서로 한다.
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
 *     process를 초기화한다.
 *     1. 전역 변수를 초기화 하는 일
 *     2. 각 시스템에 맞게 process를 초기화하는 일
 *     3. process 실행에 필요한 설정 값들을 읽어오는 일
 *
 * ARGUMENTS
 *     none
 *
 * RETURN VALUES
 *     success : 0 is returned
 *     fail : return a negative integer
 *         -1 : daemon이나 service 형태로 process를 만드는데 실패했을 경우
 *         -2 : 현재 작업 디렉토리를 읽어오는데 실패한 경우
 *         -3 : process 실행에 필요한 설정 값들을 읽어오는데 실패했을 경우
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : int GetModuleConfiguration();
 * DESCRIPTION
 *     process 실행에 필요한 설정값들을 읽어온다.
 *
 * ARGUMENTS
 *     none
 *
 * RETURN VALUES
 *     success : 0 is returned
 *     fail : return a negative integer
 *         -1 : ck.conf 파일이 없는 경우
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : int InitCK();
 * DESCRIPTION
 *     communication kernel 설정을 초기화 한다.
 *     1. communication kernel과 다른 process간에 message를 주고받기 위한 경로를 설정한다. 경로는 
 *        TCP/IP server socket과 message transfer(unix에서는 message queue, windows에서는 named pipe)
 *        두가지를 만든다. 
 *     2. communication kernel의 thread와 message buffer 동기화를 위한 semaphore를 만든다.
 *
 * ARGUMENTS
 *     none
 *
 * RETURN VALUES
 *     success : 0 is returned
 *     fail : return a negative integer
 *         -1 : TCP/IP server socket을 만드는데 실패했을 경우
 *         -2 : message transfer를 만드는데 실패했을 경우
 *         -3 : semaphore를 만드는데 실패했을 경우
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : int CommunicationKernel();
 * DESCRIPTION
 *     communication kernel을 시작한다. message 수신과 전송을 위한 thread를 만들어서 message 처리를
 *     할 수 있도록 한다.
 *
 * ARGUMENTS
 *     none
 *
 * RETURN VALUES
 *     0 : communication kernel이 정상적인 방법으로 종료된 경우.
 *     -1 : 정상적인 방법이 아닌 다른 경로로 종료되는 경우 - 자원 부족, IPC method 삭제 등
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : va_thread_return_t GetMessageFromSocket();
 * DESCRIPTION
 *     TCP/IP server socket에서 message를 받아서 message processor에 전달한다.
 *     message buffer에 받은 message를 넣고, message processor에게 message buffer에 message가 들어왔음을 알린다.
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
 *     message transfer에서 message를 받아서 message processor에 전달한다.
 *     message buffer에 받은 message를 넣고, message processor에게 message buffer에 message가 들어왔음을 알린다.
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
 *     TCP/IP server socket과 message transfer로부터 받은 message를 처리한다.
 *     request와 execute에 맞게 실행할 process를 찾아서 실행하고 message를 전달한다.
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
 *     TCP/IP server socket과 message transfer에서 message를 받아서 message buffer에 저장한다.
 *
 * ARGUMENTS
 *     requestCommand : TCP/IP server socket과 message buffer에서 받은 message
 *     connectionaddr : TCP/IP server socket으로부터 message를 받은 경우, message를 보내온 node의 정보
 *
 * RETURN VALUES
 *     success : 0 is returned
 *     fail : return a negative integer
 *         -1 : semaphore가 제거되어서 동기화에 실패한 경우
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : int FindAction(struct ck * command, char * action);
 * DESCRIPTION
 *     action list에서 request와 execute 조합에 맞게 실행할 process를 찾는다.
 *
 * ARGUMENTS
 *     command : process로 전달할 message
 *     action : request와 execute 조합에 맞게 action list에서 찾은 process
 *
 * RETURN VALUES
 *     success : 0 is returned
 *     fail : return a negative integer
 *         -1 : action list가 없는 경우
 *         -2 : action list에서 message를 전달할 process를 찾지 못한 경우
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : int GetMessageType();
 * DESCRIPTION
 *     process로 message를 전달할때 지정한 process에서 message를 제대로 받을 수 있도록 각 message를
 *     구분해줘야한다. 이때 message를 구분할 고유 id로 message type을 지정한다. 이 message type을 
 *     지정한다.
 *
 * ARGUMENTS
 *     none
 *
 * RETURN VALUES
 *     success : return a positive integer representing message type
 *     fail : -1 is returned. 가용한 message type이 없다는 얘기는 child process가 제대로 종료되지 않은 경우가
 *         반복해서 발생하고 있거나, child process가 종료된 것을 제대로 감지하고 있지 못하다는 얘기다.
 *         더이상 사용가능한 message type이 없기 때문에 communication kernel을 종료한다.
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : int SendMessageToModule(char * action, int pmtype, struct ck * command);
 * DESCRIPTION
 *     message를 전달할 process를 실행하고 message를 전달한다.
 *
 * ARGUMENTS
 *     action : message를 전달할 process
 *     pmtype : process로 전달할 message의 고유 message type
 *     command : process로 전달할 message
 *
 * RETURN VALUES
 *     success : 0 is returned
 *     fail : return a negative integer
 *         -1 : child process를 만드는데 실패한 경우
 *         -2 : child process에서 process 실행에 실패한 경우
 *         -3 : process에 message 전달에 실패한 경우
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : va_thread_return_t SendMessageToNamedPipe(void * arg)
 * DESCRIPTION
 *     named pipe로 message를 전달한다. unix에서는 이 thread를 사용하지 않는다.
 *     windows의 message transfer인 named pipe가 connection base로 동작하기 때문에 message가 전달되지 못했을 때 
 *     CK가 block되는 일을 방지하기 위해서 실제 named pipe로 message를 전달하는 부분을 thread로 만들어서 처리한다.
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
 *         pmtype : named pipe로 전달할 message의 고유 message type
 *         command : named pipe로 전달할 message
 *
 * RETURN VALUES
 *     none
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : void CloseThread();
 * DESCRIPTION
 *     실행된 thread들을 모두 정상적인 경로를 통해 종료되도록하는데 필요한 조치들을 취한다.
 *     communication kernel에서는 message 경로를 종료시켜서 종료하도록 한다.
 *     unix에서는 TCP/IP server socket을 종료시키고, message queue를 제거하고, semaphore를 제거한다.
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
 *     communication kernel에서 사용한 자원들을 반납하고, process를 종료할 준비를 한다.
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
 *     ck의 모든 child process를 종료한다.
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
 *     master server일 경우 library controller를 종료한다.
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
 *     master server일 경우 scheduler를 종료한다.
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
 *     unix에서 child process가 종료될때 발생하는 signal을 받아서 이 child process에서 사용된 
 *     message type을 재사용하도록 처리한다. 이 signal을 처dkf리해주지 않으면 재사용가능한 message type이
 *     없어져서 process로 message를 전달할 수 없게된다. 또한 zombie process가 계속 남아서 시스템
 *     자원을 차지하게 된다.
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
 *     서비스의 제어 신호를 처리하는 함수로써 핸들러는 서비스 메인에서 디스패처에 등록된다.
 *  
 * ARGUMENTS
 *     fdwcontrol : 서비스가 해야할 작업 내용을 담은 제어 신호값
 *
 * RETURN VALUES
 *     none
 *************************************************************************************************/


/**************************************************************************************************
 * function prototype : void CK_set_status(DWORD dwstate, DWORD dwaccept);
 * DESCRIPTION
 *     서비스의 상태를 변경하고, 현재 서비스 상태를 보관한다.
 *  
 * ARGUMENTS
 *     dwstate : 서비스의 최근 상태
 *     dwaccept : 핸들러 함수에서 서비스를 허용하고 처리하는 제어 코드
 *
 * RETURN VALUES
 *     none
 *************************************************************************************************/


#endif
