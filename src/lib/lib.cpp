#include "require.h"
#include "abio.h"
#include "lib.h"
#include "vendor.h"


extern char masterIP[IP_LENGTH];
extern char masterLogPort[PORT_LENGTH];

extern char logJobID[JOB_ID_LENGTH];
extern int logModuleNumber;


__int64 va_atoll(char * s)
{
	#ifdef __ABIO_UNIX
		__int64 value;
		__int64 sum;
		
		int j;
	#endif
	
	int len;
	int i;
	
	
	
	if (s == NULL || s[0] == '\0')
	{
		return 0;
	}
	
	if (s[0] != '-' && (s[0] < '0' || '9' < s[0]))
	{
		return 0;
	}
	
	len = (int)strlen(s);
	for (i = 1; i < len; i++)
	{
		if (s[i] < '0' || '9' < s[i])
		{
			return 0;
		}
	}
	
	
	#ifdef __ABIO_UNIX
		sum = 0;
		
		len = (int)strlen(s);
		
		for (i = len - 1; i > 0; i--)
		{
			value = s[i] - '0';
			for (j = 0; j < len - 1 - i; j++)
			{
				value *= 10;
			}
			
			sum += value;
		}
		
		if (s[0] != '-')
		{
			value = s[0] - '0';
			for (j = 0; j < len - 1; j++)
			{
				value *= 10;
			}
			
			sum += value;
		}
		else
		{
			sum *= -1;
		}
		
		return sum;
	#elif __ABIO_WIN
		return (__int64)_atoi64(s);
	#endif
}

void va_itoa(int n, char * s)
{
	int i, sign;
	
	
	
	if ((sign=n) < 0)
	{
		n = -n;
	}
	
	i = 0;
	do
	{
		s[i++] = n % 10 + '0';
	}
	while ((n /= 10) > 0);
	
	if (sign < 0)
	{
		s[i++] = '-';
	}
	s[i] = '\0';
	
	va_reverse(s);
}

void va_lltoa(__int64 n, char * s)
{
	__int64 sign;
	int i;
	
	
	
	if ((sign=n) < 0)
	{
		n = -n;
	}
	
	i = 0;
	do
	{
		s[i++] = (int)(n % 10) + '0';
	}
	while ((n /= 10) > 0);
	
	if (sign < 0)
	{
		s[i++] = '-';
	}
	s[i] = '\0';
	
	va_reverse(s);
}

void va_ulltoa(__uint64 n, char * s)
{
	int i;
	
	
	
	i = 0;
	do
	{
		s[i++] = (int)(n % 10) + '0';
	}
	while ((n /= 10) > 0);
	
	s[i] = '\0';
	va_reverse(s);
}

void va_reverse(char * s)
{
	int c, i, j;
	
	
	
	for (i = 0, j = (int)strlen(s) - 1; i < j; i++, j--)
	{
		c = s[i];
		s[i] = s[j];
		s[j] = c;
	}
}

void va_setrlimit()
{
	#ifdef __ABIO_UNIX
		struct rlimit rl;		/* system resource limit */
		
		
		
		// ���� resource�� ���Ѵ�� �����Ѵ�.
		#if defined (__ABIO_TRU64) || defined (__ABIO_FREEBSD)
			rl.rlim_cur = RLIM_INFINITY;
			rl.rlim_max = RLIM_INFINITY;
		#else
			rl.rlim_cur = RLIM64_INFINITY;
			rl.rlim_max = RLIM64_INFINITY;
		#endif
		setrlimit(RLIMIT_CORE, &rl);
		setrlimit(RLIMIT_CPU, &rl);
		setrlimit(RLIMIT_DATA, &rl);
		setrlimit(RLIMIT_FSIZE, &rl);
		setrlimit(RLIMIT_STACK, &rl);
		setrlimit(RLIMIT_AS, &rl);
		setrlimit(RLIMIT_NOFILE, &rl);
		
		// broken socket�� data�� �� �� �߻��ϴ� SIGPIPE signal�� �����ϵ��� ����.
		signal(SIGPIPE, SIG_IGN);
		
		// locale�� �ý��� �⺻ ������ �����Ѵ�.
		setlocale(LC_CTYPE, "");
	#elif __ABIO_WIN
		
	#endif
}

int va_make_service()
{
	#ifdef __ABIO_UNIX
		pid_t pid;
		
		if ((pid = fork()) == -1)
		{
			return -1;
		}
		else if (pid != 0)
		{
			// parent goes bye-bye
			exit(0);
		}
		
		
		/* child continues */
		setsid();		// become session leader
		umask(0);		// clear our file mode creation mask
		
		return 0;
	#elif __ABIO_WIN
		
		// ���μ����� ���񽺷� �����ϵ��� �ϴ� ����
		// 
		return 0;
	#endif
}

int va_getpid()
{
	#ifdef __ABIO_UNIX
		return getpid();
	#elif __ABIO_WIN
		return _getpid();
	#endif
}

va_thread_t va_create_thread(va_thread_return_t (*startRoutine)(void *) , void * arg)
{
	va_thread_t tid;
	
	#ifdef __ABIO_UNIX
		pthread_attr_t attr;
	#endif
	
	
	
	#ifdef __ABIO_UNIX
		pthread_attr_init(&attr);
		pthread_attr_setstacksize(&attr, THREAD_STACK_SIZE);
		
		if (pthread_create(&tid, &attr, startRoutine, arg) != 0)
		{
			return 0;
		}
	#elif __ABIO_WIN
		if ((tid = (va_thread_t)_beginthreadex(NULL, 0, (unsigned (__stdcall *)(void *))startRoutine, arg, 0, NULL)) == 0)
		{
			return 0;
		}
	#endif
	
	
	return tid;
}

int va_wait_thread(va_thread_t tid)
{
	#ifdef __ABIO_UNIX
		#ifdef __ABIO_TRU64
			if (tid != NULL)
			{
				pthread_join(tid, NULL);
			}
		#else
			if (tid > 0)
			{
				pthread_join(tid, NULL);
			}
		#endif
	#elif __ABIO_WIN
		if ((HANDLE)tid != NULL)
		{
			WaitForSingleObject((HANDLE)tid, INFINITE);
			CloseHandle((HANDLE)tid);
		}
	#endif
	
	return 0;
}

void va_wait_thread_pool(va_thread_t * threadPool, int threadNumber)
{
	int i;
	
	
	
	for (i = 0; i < threadNumber; i++)
	{
		#ifdef __ABIO_UNIX
			pthread_join(threadPool[i], NULL);
		#elif __ABIO_WIN
			WaitForSingleObject((void *)threadPool[i], INFINITE);
		#endif
	}
}

void va_free(void * ptr)
{
	if (ptr != NULL)
	{
		free(ptr);
	}
}

void va_sleep(int seconds)
{
	#ifdef __ABIO_UNIX
		sleep(seconds);
	#elif __ABIO_WIN
		Sleep(seconds * 1000);
	#endif
}

void va_sleep_random(int RAND)
{
	srand((unsigned)time(NULL));
	#ifdef __ABIO_UNIX
		sleep(rand() % RAND);
	#elif __ABIO_WIN
		Sleep(rand() % RAND * 1000);
	#endif
}

void va_sleep_random_range(int min, int max)
{
	srand((unsigned)time(NULL));
	#ifdef __ABIO_UNIX
		sleep( (rand() % max) + min );
	#elif __ABIO_WIN
		Sleep( ((rand() % max) + min) * 1000 );
	#endif
}

void va_usleep(int useconds)
{
	#ifdef __ABIO_UNIX
		usleep(useconds);
	#elif __ABIO_WIN
		Sleep(useconds / 1000);
	#endif
}

int va_get_working_directory(char * folder)
{
	#ifdef __ABIO_UNIX
		char directory[MAX_PATH_LENGTH];
	#elif __ABIO_WIN
		wchar_t filename[MAX_PATH_LENGTH];
		
		int i;
	#endif
	
	char * convertBuffer;
	int convertBufferSize;
	int convertSize;
	
	
	
	if (folder == NULL)
	{
		return -1;
	}
	
	
	// initialize variables
	convertBuffer = 0;
	convertBufferSize = 0;
	convertSize = 0;
	
	
	#ifdef __ABIO_UNIX
		memset(directory, 0, sizeof(directory));
		
		// ���� �۾� ���丮�� �о�´�.
		if (getcwd(directory, sizeof(directory)) != NULL)
		{
			// ���� �۾� ���丮 �̸��� utf8�� ��ȯ�Ѵ�.
			if ((convertSize = va_convert_string_to_utf8(ENCODING_UNKNOWN, directory, (int)strlen(directory), (void **)&convertBuffer, &convertBufferSize)) > 0)
			{
				strncpy(folder, convertBuffer, convertSize);
			}
		}
	#elif __ABIO_WIN
		memset(filename, 0, sizeof(filename));
		
		// ���� ����� �̸��� �о�´�.
		if (GetModuleFileNameW(NULL, filename, sizeof(filename) / sizeof(wchar_t)) != 0)
		{
			// ���� ����� �̸��� utf8�� ��ȯ�Ѵ�.
			if ((convertSize = va_convert_string_to_utf8(ENCODING_UTF16_LITTLE, (char *)filename, (int)wcslen(filename) * sizeof(wchar_t), (void **)&convertBuffer, &convertBufferSize)) > 0)
			{
				// ���� ����� �̸����� ����� ����Ǿ� �ִ� ���丮�� �̸��� �о�´�.
				for (i = convertSize - 1; i > 0; i--)
				{
					if (convertBuffer[i] == FILE_PATH_DELIMITER)
					{
						strncpy(folder, convertBuffer, i);
						
						break;
					}
				}
			}
		}
	#endif
	
	va_free(convertBuffer);
	
	
	if (folder[0] != '\0')
	{
		return 0;
	}
	else
	{
		return -1;
	}
}

void va_make_time_stamp(time_t timer, char * timeStamp, enum timeStampType type)
{
	struct tm tm;
	
	time_t current_t;

	current_t = time(NULL);

	
	if(LAST_TIME_STAMP < (__int64)timer)
	{
		timer = (time_t)1;
	}
	
	
	if (timer > 0)
	{
		memcpy(&tm, localtime(&timer), sizeof(struct tm));
		
		switch (type)
		{
			case TIME_STAMP_TYPE_INTERNAL : 
				sprintf(timeStamp, "%04d%02d%02d%02d%02d%02d", tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
				break;
				
			case TIME_STAMP_TYPE_EXTERNAL_KO : 
				sprintf(timeStamp, "%04d-%02d-%02d %02d:%02d:%02d", tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
				break;
				
			case TIME_STAMP_TYPE_EXTERNAL_EN : 
				sprintf(timeStamp, "%02d-%02d-%04d %02d:%02d:%02d", tm.tm_mon + 1, tm.tm_mday, tm.tm_year + 1900, tm.tm_hour, tm.tm_min, tm.tm_sec);
				break;
				
			case TIME_STAMP_TYPE_ORACLE_RMAN : 
				sprintf(timeStamp, "%04d-%02d-%02d:%02d:%02d:%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
				break;

			case TIME_STAMP_TYPE_MON_DAY:
				sprintf(timeStamp, "%02d-%02d", tm.tm_mon + 1, tm.tm_mday);
				break;
				
			case TIME_STAMP_TYPE_TIMESTAMP : 
				va_lltoa((__int64)timer, timeStamp);
				
			default : 
				sprintf(timeStamp, "%d%02d%02d%02d%02d%02d", tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
				break;
		}
	}
}

#ifdef __ABIO_WIN
	enum windowsPlatformType va_get_windowssystem()
	{
		OSVERSIONINFO osvi;
		OSVERSIONINFOEX osviex;
		
		
		
		memset(&osvi, 0, sizeof(OSVERSIONINFO));
		osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
		
		if (GetVersionEx((OSVERSIONINFO *)&osvi) == 0)
		{
			return WINDOWS_PLATFORM_UNKNOWN;
		}
		
		if (osvi.dwMajorVersion == 4)
		{
			if (osvi.dwMinorVersion == 0)
			{
				if (osvi.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS)
				{
					return WINDOWS_PLATFORM_95;
				}
				else if (osvi.dwPlatformId == VER_PLATFORM_WIN32_NT)
				{
					return WINDOWS_PLATFORM_NT4;
				}
			}
			else if (osvi.dwMinorVersion == 10)
			{
				return WINDOWS_PLATFORM_98;
			}
			else if (osvi.dwMinorVersion == 90)
			{
				return WINDOWS_PLATFORM_ME;
			}
		}
		else if (osvi.dwMajorVersion == 5)
		{
			if (osvi.dwMinorVersion == 0)
			{
				return WINDOWS_PLATFORM_2000;
			}
			else if (osvi.dwMinorVersion == 1)
			{
				return WINDOWS_PLATFORM_XP;
			}
			else if (osvi.dwMinorVersion == 2)
			{
				#ifdef SM_SERVERR2
					if (GetSystemMetrics(SM_SERVERR2) == 0)
					{
						return WINDOWS_PLATFORM_2003;
					}
					else
					{
						return WINDOWS_PLATFORM_2003R2;
					}
				#else
					return WINDOWS_PLATFORM_2003;
				#endif
			}
		}
		else if (osvi.dwMajorVersion == 6)
		{
			if (osvi.dwMinorVersion == 0)
			{
				memset(&osviex, 0, sizeof(OSVERSIONINFOEX));
				osviex.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
				
				if (GetVersionEx((OSVERSIONINFO *)&osviex) == 0)
				{
					return WINDOWS_PLATFORM_UNKNOWN;
				}
				
				if (osviex.wProductType == VER_NT_WORKSTATION)
				{
					return WINDOWS_PLATFORM_VISTA;
				}
				else
				{
					return WINDOWS_PLATFORM_2008;
				}
			}
			else if (osvi.dwMinorVersion == 1)
			{
				memset(&osviex, 0, sizeof(OSVERSIONINFOEX));
				osviex.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
				
				if (GetVersionEx((OSVERSIONINFO *)&osviex) == 0)
				{
					return WINDOWS_PLATFORM_UNKNOWN;
				}
				
				if (osviex.wProductType == VER_NT_WORKSTATION)
				{
					return WINDOWS_PLATFORM_7;
				}
				else
				{
					return WINDOWS_PLATFORM_2008R2;
				}
			}
			else if (osvi.dwMinorVersion > 1)
			{
				return WINDOWS_PLATFORM_OTHER;
			}
		}
		else if (osvi.dwMajorVersion > 6)
		{
			return WINDOWS_PLATFORM_OTHER;
		}
		
		
		return WINDOWS_PLATFORM_UNKNOWN;
	}
#endif

#ifdef __ABIO_LINUX
	// �޸𸮸� alignment ������ block���� �����ߴٰ� �������� �� ptr���� ū �ּҸ� ���� block�� �ּҸ� �������ִ� �Լ�.
	// linux���� character special file�� read/write�� �� �ʿ���.
	// �̸� ���� ptr�� alignment�� 2�� ũ��� �Ҵ��� �Ǿ����.
	void * va_ptr_align(void * ptr, int alignment)
	{
		char * p0;
		char * p1;
		
		
		
		p0 = ptr;
		p1 = p0 + alignment - 1;
		
		#ifdef __ABIO_64
			return (void *)(p1 - ((__int64)p1 % (__int64)alignment));
		#else
			return (void *)(p1 - ((int)p1 % (int)alignment));
		#endif
	}
#endif

int va_list_insert_first(struct listNode * first, void * item, int itemSize)
{
	struct listNode * node;
	
	
	
	// list�� ������ list�� item�� �߰����� �ʴ´�.
	if (first == NULL)
	{
		return -1;
	}
	
	// ���ο� list node�� ����
	if ((node = (struct listNode *)malloc(sizeof(struct listNode))) != NULL)
	{
		if ((node->item = malloc(itemSize)) != NULL)
		{
			memcpy(node->item, item, itemSize);
			node->next = NULL;
		}
		else
		{
			va_free(node);
			
			return -1;
		}
	}
	else
	{
		return -1;
	}
	
	// list�� ���� �տ� ���ο� node�� ����
	node->next = first->next;
	first->next = node;
	
	return 0;
}

int va_list_insert_last(struct listNode * first, void * item, int itemSize)
{
	struct listNode * node;
	struct listNode * temp;
	
	
	
	// list�� ������ list�� item�� �߰����� �ʴ´�.
	if (first == NULL)
	{
		return -1;
	}
	
	// ���ο� list node�� ����
	if ((node = (struct listNode *)malloc(sizeof(struct listNode))) != NULL)
	{
		if ((node->item = malloc(itemSize)) != NULL)
		{
			memcpy(node->item, item, itemSize);
			node->next = NULL;
		}
		else
		{
			va_free(node);
			
			return -1;
		}
	}
	else
	{
		return -1;
	}
	
	// list�� ������ node�� ã�´�.
	temp = first;
	while (temp->next != NULL)
	{
		temp = temp->next;
	}
	
	// list�� ���� �ڿ� ���ο� node�� ����
	temp->next = node;
	
	return 0;
}

void * va_list_pop_first(struct listNode * first)
{
	struct listNode * node;
	void * item;
	
	
	
	// list�� ������ list�� ù��° item�� �������� �ʴ´�.
	if (first == NULL)
	{
		return NULL;
	}
	
	// list�� ��������� list�� ù��° item�� �������� �ʴ´�.
	if (first->next == NULL)
	{
		return NULL;
	}
	
	// ������ item�� �����´�.
	node = first->next;
	item = node->item;
	
	// list�� ù��° node�� �����Ѵ�.
	first->next = node->next;
	va_free(node);
	
	// list�� ù��° item�� �����Ѵ�.
	return item;
}

void * va_list_pop_last(struct listNode * first)
{
	struct listNode * node;
	struct listNode * temp;
	void * item;
	
	
	
	// list�� ������ list�� ������ item�� �������� �ʴ´�.
	if (first == NULL)
	{
		return NULL;
	}
	
	// list�� ��������� list�� ������ item�� �������� �ʴ´�.
	if (first->next == NULL)
	{
		return NULL;
	}
	
	// list�� ������ node�� ã�´�.
	temp = first;
	node = temp->next;
	while (node->next != NULL)
	{
		temp = temp->next;
		node = temp->next;
	}
	
	// ������ item�� �����´�.
	item = node->item;
	
	// list�� ������ node�� �����Ѵ�.
	temp->next = NULL;
	va_free(node);
	
	// list�� ������ item�� �����Ѵ�.
	return item;
}

void va_list_delete_list(struct listNode * first)
{
	struct listNode * node;
	struct listNode * temp;
	
	
	
	// list�� ������ list�� �������� �ʴ´�.
	if (first == NULL)
	{
		return;
	}
	
	// list�� ��������� list�� �������� �ʴ´�.
	if (first->next == NULL)
	{
		return;
	}
	
	// first�� ������ list�� ��� node�� �����Ѵ�.
	node = first->next;
	temp = node->next;
	while (temp != NULL)
	{
		va_free(node->item);
		va_free(node);
		
		node = temp;
		temp = node->next;
	}
	
	va_free(node->item);
	va_free(node);
	
	// first�� list���� ��ũ�� �����Ѵ�.
	first->next = NULL;
}

void va_print_item_on_screen(char * title, int thereIsItemTitle, char *** itemList, int listNumber, int itemNumber)
{
	int * itemPosition;
	int * itemLength;
	int separatorLength;
	
	int screenPosition;
	
	int i;
	int j;
	int k;
	
	#define FIRST_ITEM_SPACE		0
	#define BETWEEN_ITEM_SPACE		4
	#define LINE_SEPARATOR			'='
	#define TITLE_SEPARATOR			'-'
	
	
	
	itemPosition = (int *)malloc(sizeof(int) * itemNumber);
	memset(itemPosition, 0, sizeof(int) * itemNumber);
	
	itemLength = (int *)malloc(sizeof(int) * itemNumber);
	memset(itemLength, 0, sizeof(int) * itemNumber);
	
	separatorLength = 0;
	
	
	// item list�� �� item���� item�� �ִ� ���̸� ���Ѵ�.
	for (i = 0; i < listNumber; i++)
	{
		for (j = 0; j < itemNumber; j++)
		{
			if ((int)strlen(itemList[i][j]) > itemLength[j])
			{
				itemLength[j] = (int)strlen(itemList[i][j]);
			}
		}
	}
	
	// �� item�� �ִ� ���̸� �������� �� item�� screen position�� ���Ѵ�.
	itemPosition[0] = FIRST_ITEM_SPACE;
	for (i = 1; i < itemNumber; i++)
	{
		itemPosition[i] = itemPosition[i - 1] + itemLength[i - 1] + BETWEEN_ITEM_SPACE;
	}
	
	// �� item�� �ִ� ���̿� empty space�� ���� separator length�� ���Ѵ�.
	for (i = 0; i < itemNumber; i++)
	{
		separatorLength += itemLength[i] + BETWEEN_ITEM_SPACE;
	}
	separatorLength -= 4;
	
	// title�� ����Ѵ�.
	if (title != NULL && title[0] != '\0')
	{
		// line separator�� ����Ѵ�.
		for (i = 0; i < separatorLength; i++)
		{
			printf("%c", LINE_SEPARATOR);
		}
		printf("\n");
		
		// title�� ����Ѵ�.
		printf("%s\n", title);
	}
	
	// item title�� ����Ѵ�.
	if (thereIsItemTitle == 1)
	{
		// line separator�� ����Ѵ�.
		for (i = 0; i < separatorLength; i++)
		{
			printf("%c", LINE_SEPARATOR);
		}
		printf("\n");
		
		// item title�� ����Ѵ�.
		screenPosition = 0;
		
		for (i = 0; i < itemNumber; i++)
		{
			for (j = screenPosition; j < itemPosition[i]; j++)
			{
				printf(" ");
				screenPosition++;
			}
			
			printf("%s", itemList[0][i]);
			screenPosition += (int)strlen(itemList[0][i]);
		}
		
		printf("\n");
		
		// title separator�� ����Ѵ�.
		for (i = 0; i < separatorLength; i++)
		{
			printf("%c", TITLE_SEPARATOR);
		}
		printf("\n");
	}
	else
	{
		// line separator�� ����Ѵ�.
		for (i = 0; i < separatorLength; i++)
		{
			printf("%c", LINE_SEPARATOR);
		}
		printf("\n");
	}
	
	// �� item�� screen position�� ����Ѵ�.
	for ((thereIsItemTitle == 1)?(i = 1):(i = 0); i < listNumber; i++)
	{
		screenPosition = 0;
		
		for (j = 0; j < itemNumber; j++)
		{
			for (k = screenPosition; k < itemPosition[j]; k++)
			{
				printf(" ");
				screenPosition++;
			}
			
			printf("%s", itemList[i][j]);
			screenPosition += (int)strlen(itemList[i][j]);
		}
		
		printf("\n");
	}
	
	// line separator�� ����Ѵ�.
	for (i = 0; i < separatorLength; i++)
	{
		printf("%c", LINE_SEPARATOR);
	}
	printf("\n");
	printf("\n");
}
__int64 va_update_next_day_time(__int64 nextTime)
{
	struct tm next_tm;
	time_t next_t;

	next_t= (time_t)nextTime;
	memcpy(&next_tm, localtime(&next_t), sizeof(struct tm));
	next_tm.tm_mday++;
	return (__int64)mktime(&next_tm);
}
__int64 va_set_current_day(int setTime)
{
	time_t current_t;

	struct tm current_tm;
	struct tm set_tm;
	
	__int64 set_t;
	
	current_t=time(NULL);

	memcpy(&current_tm, localtime(&current_t), sizeof(struct tm));

	memset(&set_tm, 0, sizeof(struct tm));
	set_tm.tm_year = current_tm.tm_year;
	set_tm.tm_mon =	current_tm.tm_mon ;
	set_tm.tm_mday = current_tm.tm_mday;
	set_tm.tm_hour = setTime/100;
	set_tm.tm_min = setTime%100 ;
	set_tm.tm_sec = 0;

	set_t=(__int64)mktime(&set_tm);

	if( (__int64)current_t < set_t)
	{
		return set_t;
	}
	else
	{
		return va_update_next_day_time(set_t);
	}
}
void va_update_schedule_time(struct scheduleType * schedule, time_t current_t)
{
	char effectiveYear[8];
	char effectiveMonth[8];
	char effectiveMday[8];
	char effectiveHour[8];
	char effectiveMin[8];
	
	struct tm tmEffective;
	time_t effectiveTime;
	
	int windowStartMinute;
	int windowEndMinute;
	
	struct tm tmLastSchedule;
	time_t lastScheduleTime;
	int lastScheduleYear;
	int lastScheduleMonth;
	int lastScheduleMday;
	
	struct tm tmCurrent;
	int currentMinute;
	
	struct tm tmToday;
	time_t todayTime;
	int todayYear;
	int todayMonth;
	int todayMday;
	int todayWday;
	
	int lastMday;
	int lastWday;
	
	int nextMonthFirstWday;
	int nextMonthLastMday;
	
	time_t weekStartTime;
	time_t weekEndTime;
	time_t monthStartTime;
	time_t monthEndTime;
	
	int findSchedule;
	int scheduleOnToday;
	
	struct tm tmPeriodStart;
	time_t periodStartTime;
	time_t periodEndTime;
	
	int periodStartMinute;
	int periodEndMinute;
	
	int scheduleMonth;
	int scheduleMday;
	
	struct tm tmStart;
	struct tm tmEnd;
	
	int i;
	
	
	
	// initialize variables
	memset(effectiveYear, 0, sizeof(effectiveYear));
	memset(effectiveMonth, 0, sizeof(effectiveMonth));
	memset(effectiveMday, 0, sizeof(effectiveMday));
	memset(effectiveHour, 0, sizeof(effectiveHour));
	memset(effectiveMin, 0, sizeof(effectiveMin));
	
	memset(&tmLastSchedule, 0, sizeof(struct tm));
	lastScheduleTime = 0;
	lastScheduleYear = 0;
	lastScheduleMonth = 0;
	lastScheduleMday = 0;
	
	weekStartTime = 0;
	weekEndTime = 0;
	monthStartTime = 0;
	monthEndTime = 0;
	periodStartTime = 0;
	periodEndTime = 0;
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 1. ���� �ð��� ������ ���� ���� �ð��� ���ؼ� ������ ���� �ð��� ����Ѵ�.
	
	// ���� �ð��� ���Ѵ�.
	if (current_t == 0)
	{
		current_t = time(NULL);
	}
	
	// �������� ����Ǳ� ������ �ð��� ����Ѵ�.
	strncpy(effectiveYear, schedule->effectiveTime, 4);
	strncpy(effectiveMonth, schedule->effectiveTime + 4, 2);
	strncpy(effectiveMday, schedule->effectiveTime + 6, 2);
	strncpy(effectiveHour, schedule->effectiveTime + 8, 2);
	strncpy(effectiveMin, schedule->effectiveTime + 10, 2);
	
	memset(&tmEffective, 0, sizeof(struct tm));
	tmEffective.tm_year = atoi(effectiveYear) - 1900;
	tmEffective.tm_mon = atoi(effectiveMonth) - 1;
	tmEffective.tm_mday = atoi(effectiveMday);
	tmEffective.tm_hour = atoi(effectiveHour);
	tmEffective.tm_min = atoi(effectiveMin);
	tmEffective.tm_sec = 0;
	effectiveTime = mktime(&tmEffective);
	
	// ���� �ð��� schedule ���� �ð� �����̸� ���� �ð��� schedule ���� �ð����� �����Ѵ�.
	if (current_t < effectiveTime)
	{
		current_t = effectiveTime;
	}
	memcpy(&tmCurrent, localtime(&current_t), sizeof(struct tm));
	
	// ������ ��, ��, ��, ������ ����
	memcpy(&tmToday, &tmCurrent, sizeof(struct tm));
	tmToday.tm_hour = 0;
	tmToday.tm_min = 0;
	tmToday.tm_sec = 0;
	todayTime = mktime(&tmToday);
	todayYear = tmToday.tm_year + 1900;
	todayMonth = tmToday.tm_mon + 1;
	todayMday = tmToday.tm_mday;
	todayWday = tmToday.tm_wday;
	
	// �̴��� ������ ���� ����
	lastMday = va_get_last_day_of_month(todayYear, todayMonth);
	
	// �̴� ������ ���� ������ ����
	lastWday = ((lastMday - todayMday) % 7 + todayWday) % 7;
	
	// ������ 1���� ������ ����
	nextMonthFirstWday = (lastWday + 1) % 7;
	
	// �������� ������ ���� ����
	if (todayMonth == 12)
	{
		nextMonthLastMday = va_get_last_day_of_month(todayYear + 1, 1);
	}
	else
	{
		nextMonthLastMday = va_get_last_day_of_month(todayYear, todayMonth);
	}
	
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 2. ���� �ð��� backup window�� ������ Ȯ���ϱ� ���ؼ� backup window�� ���� ������ �ð����� �д����� ����Ѵ�.
	
	// ���� �ð��� ���� ���� �ð����� �д����� ����
	currentMinute = tmCurrent.tm_hour * 60 + tmCurrent.tm_min;
	
	// backup window�� ���� ���� �ð����� �д����� ����
	windowStartMinute = atoi(schedule->startTime) / 100 * 60 + atoi(schedule->startTime + 2);
	windowEndMinute = atoi(schedule->endTime) / 100 * 60 + atoi(schedule->endTime + 2);
	
	// backup window end time�� backup window start time �������� ��� end time�� �Ϸ縦 ���Ѵ�.
	if (windowEndMinute < windowStartMinute)
	{
		windowEndMinute += 24 * 60;
	}
	
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 3. ���������� �������� ����� �ð��� ����Ѵ�.
	
	if (schedule->lastTime != 0)
	{
		lastScheduleTime = (time_t)schedule->lastTime;
		memcpy(&tmLastSchedule, localtime(&lastScheduleTime), sizeof(struct tm));
		lastScheduleYear = tmLastSchedule.tm_year + 1900;
		lastScheduleMonth = tmLastSchedule.tm_mon + 1;
		lastScheduleMday = tmLastSchedule.tm_mday;
	}
	
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 4. days of week option�� �����ؼ� ���� �ð� ������ ���� schedule ���� �ð��� ���Ѵ�.
	
	if (schedule->option.check[0] == '1')
	{
		findSchedule = 0;
		
		// ���� ����� �������� ���� ��� ���� ����� �������� �ִ��� Ȯ���Ѵ�.
		if (lastScheduleTime == 0 || lastScheduleYear != todayYear || lastScheduleMonth != todayMonth || lastScheduleMday != todayMday)
		{
			scheduleOnToday = 0;
			
			// ���� �ð��� backup window start time �����̸� ���� �������� �����ؾߵǴ� ������ �Ǵ��Ѵ�.
			if (currentMinute <= windowStartMinute)
			{
				scheduleOnToday = 1;
			}
			
			// ���� �������� �����ؾߵǴ� ��� ���� �������� �����Ǿ��ִ��� Ȯ���Ѵ�.
			if (scheduleOnToday == 1)
			{
				// ������ ù��°~�׹�° ���� ��� �������� �����Ǿ��ִ��� Ȯ���Ѵ�.
				if (todayMday < 29)
				{
					if (schedule->option.week[(todayMday - 1) / 7][todayWday] == '1')
					{
						findSchedule = 1;
					}
				}
				
				// ù��°~�׹�° ���ε� �������� �����Ǿ����� �ʰ� ������ ������ ���� ��� last week option���� �������� �����Ǿ��ִ��� Ȯ���Ѵ�.
				if (findSchedule == 0)
				{
					// ������ �̴��� ������ ������ Ȯ��
					if (todayMday + 7 > lastMday)
					{
						if (schedule->option.week[4][todayWday] == '1')
						{
							findSchedule = 1;
						}
					}
				}
				
				// ���� �������� �����Ǿ������� ������ ���۽ð��� �����Ѵ�.
				if (findSchedule == 1)
				{
					scheduleMonth = todayMonth;
					scheduleMday = todayMday;
				}
			}
		}
		
		
		// �̴޿� ���� ���Ŀ� ������ �������� �ִ��� Ȯ���Ѵ�.
		if (findSchedule == 0)
		{
			// �̴޿� ���� ���ĺ��� �׹�° �ֱ��� ������ �������� �ִ��� Ȯ���Ѵ�.
			for (i = todayMday + 1; i < 29; i++)
			{
				if (schedule->option.week[(i - 1) / 7][((i - todayMday) % 7 + todayWday) % 7] == '1')
				{
					findSchedule = 1;
					
					break;
				}
			}
			
			// ������ �ֿ� ������ �������� �ִ��� Ȯ���Ѵ�.
			if (findSchedule == 0)
			{
				if (todayMday < lastMday - 7 + 1)
				{
					i = lastMday - 7 + 1;
				}
				else
				{
					i = todayMday + 1;
				}
				
				for ( ; i < lastMday + 1; i++)
				{
					if (schedule->option.week[4][((i - todayMday) % 7 + todayWday) % 7] == '1')
					{
						findSchedule = 1;
						
						break;
					}
				}
			}
			
			// �̴޿� ������ �������� �ִ� ��� ������ ���۽ð��� backup window end time�� �����Ѵ�.
			if (findSchedule == 1)
			{
				scheduleMonth = todayMonth;
				scheduleMday = i;
			}
		}
		
		
		// �̴޿� ���� ���Ŀ� �������� �����Ǿ����� ������ �����޿� ù ������ ��¥�� ã�´�.
		if (findSchedule == 0)
		{
			// �����޿� ù ������ ��¥�� ù��°~�׹�° �ֿ��� ã�´�.
			for (i = 1; i < 29; i++)
			{
				if (schedule->option.week[(i - 1) / 7][((i - 1) % 7 + nextMonthFirstWday) % 7] == '1')
				{
					findSchedule = 1;
					
					break;
				}
			}
			
			// �������� ù ������ ��¥�� ������ �ֿ��� ã�´�.
			if (findSchedule == 0)
			{
				for (i = nextMonthLastMday - 7 + 1; i < nextMonthLastMday + 1; i++)
				{
					if (schedule->option.week[4][((i - 1) % 7 + nextMonthFirstWday) % 7] == '1')
					{
						findSchedule = 1;
						
						break;
					}
				}
			}
			
			// �����޿� ������ �������� �ִ� ��� ������ ���۽ð��� backup window end time�� �����Ѵ�.
			if (findSchedule == 1)
			{
				scheduleMonth = todayMonth + 1;
				scheduleMday = i;
			}
		}
		
		
		// schedule ���� �ð��� ���� �ð��� ����Ѵ�.
		memcpy(&tmStart, &tmCurrent, sizeof(struct tm));
		tmStart.tm_mon = scheduleMonth - 1;
		tmStart.tm_mday = scheduleMday;
		tmStart.tm_hour = windowStartMinute / 60;
		tmStart.tm_min = windowStartMinute % 60;
		tmStart.tm_sec = 0;
		weekStartTime = mktime(&tmStart);
		
		memcpy(&tmEnd, &tmCurrent, sizeof(struct tm));
		tmEnd.tm_mon = scheduleMonth - 1;
		tmEnd.tm_mday = scheduleMday;
		tmEnd.tm_hour = windowEndMinute / 60;
		tmEnd.tm_min = windowEndMinute % 60;
		tmEnd.tm_sec = 0;
		weekEndTime = mktime(&tmEnd);
	}
	
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 5. days of month option�� �����ؼ� ���� �ð� ������ ���� schedule ���� �ð��� ���Ѵ�.
	
	if (schedule->option.check[1] == '1')
	{
		findSchedule = 0;
		
		// ���� ����� �������� ���� ��� ���� ����� �������� �ִ��� Ȯ���Ѵ�.
		if (lastScheduleTime == 0 || lastScheduleYear != todayYear || lastScheduleMonth != todayMonth || lastScheduleMday != todayMday)
		{
			scheduleOnToday = 0;
			
			// ���� �ð��� backup window start time �����̸� ���� �������� �����ؾߵǴ� ������ �Ǵ��Ѵ�.
			if (currentMinute <= windowStartMinute)
			{
				scheduleOnToday = 1;
			}
			
			// ���� �������� �����ؾߵǴ� ��� ���� �������� �����Ǿ��ִ��� Ȯ���Ѵ�.
			if (scheduleOnToday == 1)
			{
				// ���� ��¥�� �������� �����Ǿ��ִ��� Ȯ���Ѵ�.
				if (schedule->option.month[todayMday - 1] == '1')
				{
					findSchedule = 1;
				}
				
				// ���� ��¥�� ������ �������� ���� ��� ������ �̴��� ������ ���� ��� ������ ���� �������� �����Ǿ��ִ��� Ȯ���Ѵ�.
				if (findSchedule == 0)
				{
					if (todayMday == lastMday)
					{
						if (schedule->option.month[31] == '1')
						{
							findSchedule = 1;
						}
					}
				}
				
				// ���� �������� �����Ǿ������� ������ ���۽ð��� backup window end time�� �����Ѵ�.
				if (findSchedule == 1)
				{
					scheduleMonth = todayMonth;
					scheduleMday = todayMday;
				}
			}
		}
		
		
		// �̴޿� ���� ���Ŀ� ������ �������� �ִ��� Ȯ���Ѵ�.
		if (findSchedule == 0)
		{
			// �̴޿� ���� ���Ŀ� ������ �������� �ִ��� Ȯ���Ѵ�.
			for (i = todayMday + 1; i < lastMday + 1; i++)
			{
				if (schedule->option.month[i - 1] == '1')
				{
					findSchedule = 1;
					
					break;
				}
			}
			
			// ���������� �������� �����Ǿ��ִ��� Ȯ���Ѵ�.
			if (findSchedule == 0)
			{
				if (todayMday != lastMday)
				{
					if (schedule->option.month[31] == '1')
					{
						findSchedule = 1;
					}
				}
			}
			
			// �̴޿� ������ �������� �ִ� ��� ������ ���۽ð��� backup window end time�� �����Ѵ�.
			if (findSchedule == 1)
			{
				scheduleMonth = todayMonth;
				scheduleMday = i;
			}
		}
		
		
		// �̴޿� ���� ���Ŀ� �������� �����Ǿ����� ������ �����޿� ù ������ ��¥�� ã�´�.
		if (findSchedule == 0)
		{
			// �����޿� ù ������ ��¥�� ã�´�.
			for (i = 1; i < nextMonthLastMday + 1; i++)
			{
				if (schedule->option.month[i - 1] == '1')
				{
					findSchedule = 1;
					
					break;
				}
			}
			
			// ���������� �������� �����Ǿ��ִ��� Ȯ���Ѵ�.
			if (findSchedule == 0)
			{
				if (schedule->option.month[31] == '1')
				{
					findSchedule = 1;
				}
			}
			
			// �����޿� ������ �������� �ִ� ��� ������ ���۽ð��� backup window end time�� �����Ѵ�.
			if (findSchedule == 1)
			{
				scheduleMonth = todayMonth + 1;
				scheduleMday = i;
			}
		}
		
		
		// schedule ���� �ð��� ���� �ð��� ����Ѵ�.
		memcpy(&tmStart, &tmCurrent, sizeof(struct tm));
		tmStart.tm_mon = scheduleMonth - 1;
		tmStart.tm_mday = scheduleMday;
		tmStart.tm_hour = windowStartMinute / 60;
		tmStart.tm_min = windowStartMinute % 60;
		tmStart.tm_sec = 0;
		monthStartTime = mktime(&tmStart);
		
		memcpy(&tmEnd, &tmCurrent, sizeof(struct tm));
		tmEnd.tm_mon = scheduleMonth - 1;
		tmEnd.tm_mday = scheduleMday;
		tmEnd.tm_hour = windowEndMinute / 60;
		tmEnd.tm_min = windowEndMinute % 60;
		tmEnd.tm_sec = 0;
		monthEndTime = mktime(&tmEnd);
	}
	
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 6. period option�� �����ؼ� ���� �ð� ������ ���� schedule ���� �ð��� ���Ѵ�.
	
	if (schedule->option.check[2] == '1')
	{
		// period option�� hour�� ���
		if (schedule->option.period[1] == 0)
		{
			// period option�� ó�� ����� �ð��� ���Ѵ�.
			periodStartMinute = windowStartMinute;
			periodEndMinute = windowEndMinute;
			
			// end time�� start time�� �������� �� current time�� end time�� ���谡 current time < end time�̸� ���� �ð��� backup window �ȿ� ������ �ȴ�.
			// �� ��� start time�� end time�� �Ϸ� ������ ������ current time�� start time, end time�� ���踦 start time < current time < end time���� ���� �������� ����� �ð��� ����ϵ��� �Ѵ�.
			if (periodEndMinute > 24 * 60)
			{
				if (currentMinute < periodEndMinute - 24 * 60)
				{
					periodStartMinute -= 24 * 60;
					periodEndMinute -= 24 * 60;
				}
			}
			
			// period option�� ���� ���� ���� �ð� ���Ŀ� ù �������� ����� �ð��� ���Ѵ�.
			while (periodStartMinute <= currentMinute)
			{
				periodStartMinute += schedule->option.period[0] * 60;
			}
			
			// period option�� ���� ���� ���� �ð� ���Ŀ� ����� ù �������� backup window end time�� ������ ���� ����� �������� ���Ѵ�.
			if (periodStartMinute > periodEndMinute)
			{
				periodStartMinute = windowStartMinute;
				periodEndMinute = windowEndMinute;
				
				if (periodStartMinute <= currentMinute)
				{
					periodStartMinute += 24 * 60;
					periodEndMinute += 24 * 60;
				}
			}
			
			// schedule ���� �ð��� ���� �ð��� ����Ѵ�.
			memcpy(&tmStart, &tmCurrent, sizeof(struct tm));
			tmStart.tm_hour = periodStartMinute / 60;
			tmStart.tm_min = periodStartMinute % 60;
			tmStart.tm_sec = 0;
			periodStartTime = mktime(&tmStart);
			
			memcpy(&tmEnd, &tmCurrent, sizeof(struct tm));
			tmEnd.tm_hour = periodEndMinute / 60;
			tmEnd.tm_min = periodEndMinute % 60;
			tmEnd.tm_sec = 0;
			periodEndTime = mktime(&tmEnd);
		}
		// period option�� min�� ���
		else if (schedule->option.period[1] == 4)
		{			
			// period option�� ó�� ����� �ð��� ���Ѵ�.
			periodStartMinute = windowStartMinute;
			periodEndMinute = windowEndMinute;
			
			// end time�� start time�� �������� �� current time�� end time�� ���谡 current time < end time�̸� ���� �ð��� backup window �ȿ� ������ �ȴ�.
			// �� ��� start time�� end time�� �Ϸ� ������ ������ current time�� start time, end time�� ���踦 start time < current time < end time���� ���� �������� ����� �ð��� ����ϵ��� �Ѵ�.
			if (periodEndMinute > 24 * 60)
			{
				if (currentMinute < periodEndMinute - 24 * 60)
				{
					periodStartMinute -= 24 * 60;
					periodEndMinute -= 24 * 60;
				}
			}
			
			// period option�� ���� ���� ���� �ð� ���Ŀ� ù �������� ����� �ð��� ���Ѵ�.
			while (periodStartMinute <= currentMinute)
			{
				periodStartMinute += schedule->option.period[0];
			}
			
			// period option�� ���� ���� ���� �ð� ���Ŀ� ����� ù �������� backup window end time�� ������ ���� ����� �������� ���Ѵ�.
			if (periodStartMinute > periodEndMinute)
			{
				periodStartMinute = windowStartMinute;
				periodEndMinute = windowEndMinute;
				
				if (periodStartMinute <= currentMinute)
				{
					periodStartMinute += 24* 60;
					periodEndMinute += 24* 60;
				}
			}
			
			// schedule ���� �ð��� ���� �ð��� ����Ѵ�.
			memcpy(&tmStart, &tmCurrent, sizeof(struct tm));
			tmStart.tm_hour = periodStartMinute / 60;
			tmStart.tm_min = periodStartMinute % 60;
			tmStart.tm_sec = 0;
			periodStartTime = mktime(&tmStart);
			
			memcpy(&tmEnd, &tmCurrent, sizeof(struct tm));
			tmEnd.tm_hour = periodEndMinute / 60;
			tmEnd.tm_min = periodEndMinute % 60;
			tmEnd.tm_sec = 0;
			periodEndTime = mktime(&tmEnd);
		}
		//#issue 220 : period option�� Once�� ���
		else if(schedule->option.period[1] == 9)
		{
			// period option�� ó�� ����� �ð��� ���Ѵ�.
			periodStartMinute = windowStartMinute;
			periodEndMinute = windowEndMinute;

			// ���� �ð����� ������ Schedule�� Inactive ��Ų��.
			if(schedule->nextEndTime != 0 && schedule->nextEndTime < current_t)
			{
				strcpy(schedule->copyName, "OVERTIME");
				return;
			}

			memcpy(&tmStart, &tmEffective, sizeof(struct tm));
			tmStart.tm_hour = windowStartMinute / 60;
			tmStart.tm_min = windowStartMinute % 60;
			tmStart.tm_sec = 0;
			periodStartTime = mktime(&tmStart);

			memcpy(&tmEnd, &tmEffective, sizeof(struct tm));
			tmEnd.tm_hour = windowEndMinute / 60;
			tmEnd.tm_min = windowEndMinute % 60;
			tmEnd.tm_sec = 0;
			periodEndTime = mktime(&tmEnd);
		}
		// period option�� day, week, month�� ���
		else
		{
			// period option�� ó�� ����� ��¥�� ���Ѵ�.
			memcpy(&tmPeriodStart, &tmEffective, sizeof(struct tm));
			tmPeriodStart.tm_hour = 0;
			tmPeriodStart.tm_min = 0;
			tmPeriodStart.tm_sec = 0;
			periodStartTime = mktime(&tmPeriodStart);
			
			// period option�� ���� �����̳� ���� ������ ��¥�� ����� ù ������ ��¥�� ã�´�.
			while (periodStartTime < todayTime)
			{
				if (schedule->option.period[1] == 1)
				{
					tmPeriodStart.tm_mday += schedule->option.period[0];
				}
				else if (schedule->option.period[1] == 2)
				{
					tmPeriodStart.tm_mday += schedule->option.period[0] * 7;
				}
				else if (schedule->option.period[1] == 3)
				{
					tmPeriodStart.tm_mon += schedule->option.period[0];
				}
				
				periodStartTime = mktime(&tmPeriodStart);
			}
			
			// period option�� ���� ����� ù �������� �����ε� ���� �ð��� backup window start time �����̸� ���� ������ ��¥�� ù ������ ��¥�� �����Ѵ�.
			if (periodStartTime == todayTime && currentMinute > windowStartMinute)
			{
				if (schedule->option.period[1] == 1)
				{
					tmPeriodStart.tm_mday += schedule->option.period[0];
				}
				else if (schedule->option.period[1] == 2)
				{
					tmPeriodStart.tm_mday += schedule->option.period[0] * 7;
				}
				else if (schedule->option.period[1] == 3)
				{
					tmPeriodStart.tm_mon += schedule->option.period[0];
				}
				
				periodStartTime = mktime(&tmPeriodStart);
			}
			
			// ���� ����� �������� ���� ��� period option�� ���� ����� ù ������ ��¥�� �����̸� ���� ������ ��¥�� ã�´�.
			if (lastScheduleTime != 0 && lastScheduleYear == todayYear && lastScheduleMonth == todayMonth && lastScheduleMday == todayMday)
			{
				if (periodStartTime == todayTime)
				{
					if (schedule->option.period[1] == 1)
					{
						tmPeriodStart.tm_mday += schedule->option.period[0];
					}
					else if (schedule->option.period[1] == 2)
					{
						tmPeriodStart.tm_mday += schedule->option.period[0] * 7;
					}
					else if (schedule->option.period[1] == 3)
					{
						tmPeriodStart.tm_mon += schedule->option.period[0];
					}
				}
			}
			
			// schedule ���� �ð��� ���� �ð��� ����Ѵ�.
			memcpy(&tmStart, &tmPeriodStart, sizeof(struct tm));
			tmStart.tm_hour = windowStartMinute / 60;
			tmStart.tm_min = windowStartMinute % 60;
			tmStart.tm_sec = 0;
			periodStartTime = mktime(&tmStart);
			
			memcpy(&tmEnd, &tmPeriodStart, sizeof(struct tm));
			tmEnd.tm_hour = windowEndMinute / 60;
			tmEnd.tm_min = windowEndMinute % 60;
			tmEnd.tm_sec = 0;
			periodEndTime = mktime(&tmEnd);
		}
	}
	
	
	
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 7. day of week, day of month, period ������ option�� �����ؼ� ���� ���� �������� ����� �ð� �߿���
	//    ���� ����� ���� ���� ������ �ð����� �Ѵ�.
	
	schedule->nextStartTime = weekStartTime;
	schedule->nextEndTime = weekEndTime;
	
	if (schedule->nextStartTime == 0)
	{
		schedule->nextStartTime = monthStartTime;
		schedule->nextEndTime = monthEndTime;
	}
	else if (monthStartTime != 0)
	{
		if (schedule->nextStartTime > (__int64)monthStartTime)
		{
			schedule->nextStartTime = monthStartTime;
			schedule->nextEndTime = monthEndTime;
		}
	}
	
	if (schedule->nextStartTime == 0)
	{
		schedule->nextStartTime = periodStartTime;
		schedule->nextEndTime = periodEndTime;
	}
	else if (periodStartTime != 0)
	{
		if (schedule->nextStartTime > (__int64)periodStartTime)
		{
			schedule->nextStartTime = periodStartTime;
			schedule->nextEndTime = periodEndTime;
		}
	}
}

int va_get_last_day_of_month(int year, int month)
{
	int lastDay;
	
	
	
	switch (month)
	{
		case 1 :
		case 3 :
		case 5 :
		case 7 :
		case 8 :
		case 10 :
		case 12 :
			lastDay = 31;
			break;
			
		case 4 :
		case 6 :
		case 9 :
		case 11 :
			lastDay = 30;
			break;
	}
	
	// 2���̸� ������ ���
	if (month == 2)
	{
		if (year % 4 == 0)
		{
			lastDay = 29;
			
			if (year % 100 == 0)
			{
				lastDay = 28;
				
				if (year % 400 == 0)
				{
					lastDay = 29;
				}
			}
		}
		else
		{
			lastDay = 28;
		}
	}
	
	
	return lastDay;
}
