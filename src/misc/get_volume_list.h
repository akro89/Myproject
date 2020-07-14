#ifndef __MISC_GET_VOLUME_LIST_H__
#define __MISC_GET_VOLUME_LIST_H__

#define SELECTLIST_NUMBER				12
#define DEFAULTS_INFORMATION			10
#define OPTION_NUMBER					28
#define DELIMITER						1
#define VOLUME_ITEM_NUMBER				10 
#define VOLUME_ITEM_NUMBER_OPTION		18
// 입력받아야 하는 정보
struct volumepoolItem
{
	char pool[VOLUME_POOL_NAME_LENGTH];
	__int64 usageint64;
	char usage[NUMBER_STRING_LENGTH];
	__int64 maxcapacityint64;
	char maxcapacity[NUMBER_STRING_LENGTH];
	int volumecount;
	char volumecountchar[NUMBER_STRING_LENGTH];
	char p[NUMBER_STRING_LENGTH];
};

struct volumeItem
{
	char name[VOLUME_NAME_LENGTH];
	char type[NUMBER_STRING_LENGTH];
	char pool[VOLUME_POOL_NAME_LENGTH];
	char storage[STORAGE_NAME_LENGTH];
	char location[NUMBER_STRING_LENGTH];
	char use[NUMBER_STRING_LENGTH];
	char usage[NUMBER_STRING_LENGTH];
	char volumeSize[NUMBER_STRING_LENGTH];
	char volumePoolAccess[NUMBER_STRING_LENGTH];
	char volumeAccess[NUMBER_STRING_LENGTH];
	//추가정보
	char volumeIndex[NUMBER_STRING_LENGTH];
	//char jobID[NUMBER_STRING_LENGTH];
	//char version[NUMBER_STRING_LENGTH];
	//char address[NUMBER_STRING_LENGTH];
	char backupsetNumber[NUMBER_STRING_LENGTH];
	char expiredBackupsetNumber[NUMBER_STRING_LENGTH];

	char mountCount[NUMBER_STRING_LENGTH];
	char unmountCount[NUMBER_STRING_LENGTH];

	char writeErrorCount[NUMBER_STRING_LENGTH];
	char readErrorCount[NUMBER_STRING_LENGTH];

	char mountTime[NUMBER_STRING_LENGTH];
	char unmountTime[NUMBER_STRING_LENGTH];

	char writeTime[NUMBER_STRING_LENGTH];
	char readTime[NUMBER_STRING_LENGTH];

	char writeErrorTime[NUMBER_STRING_LENGTH];
	char readErrorTime[NUMBER_STRING_LENGTH];
	//char unavailableTime[NUMBER_STRING_LENGTH];
	char assignTime[NUMBER_STRING_LENGTH];
	char expirationTime[NUMBER_STRING_LENGTH];
	char exportedTime[NUMBER_STRING_LENGTH];

	char capacity[NUMBER_STRING_LENGTH];
	char maxcapacity[NUMBER_STRING_LENGTH];
	__int64 maxcapacityint64;
	//char backupsetPosition[NUMBER_STRING_LENGTH];


};
enum volumeSelect
{
	name = 0,
	type = 1,
	pool = 2 ,
	storage = 3,
	location = 4 ,
	use = 5 ,
	usage= 6 ,
	volumeSize=7,
	volumePoolAccess=8,
	volumeAccess=9,
	//추가정보
	volumeIndex=10,
	//jobID,
	//version,
	//address,
	backupsetNumber=11,
	expiredBackupsetNumber=12,
	mountCount=13,
	unmountCount=14,
	writeErrorCount=15,
	readErrorCount=16,
	mountTime=17,
	unmountTime=18,
	writeTime=19,
	readTime=20,
	writeErrorTime=21,
	readErrorTime=22,
	//unavailableTime,
	assignTime=23,
	expirationTime=24,
	exportedTime=25,
	capacity=26,
	maxcapacity=27,
	//backupsetPosition,
};
char *strsub(char *input, int i_begin, int i_end)
{
	int i;
	int cnt = 0;
	int size = (i_end - i_begin)+2;
	char *str = (char*)malloc(size);

	memset(str, 0, size);
	for(i = i_begin; i<= i_end; i++)
	{
		str[cnt] = input[i];
		cnt++;
	}
	return str;
}
int countDelimiter(char *searchObject, char delimiter)
{
	int i, count;
	count=0;
	for(i=0; searchObject[i] != '\0'; i++)
	{
		if(searchObject[i]==delimiter)
			count++;
		else
			continue;
	}
	return count;
}
int InitProcess(char * filename);
int GetArgument(int argc, char ** argv);
int SendCommand();
int countDelimiter(char *searchObject, char delimiter);
int RecvReply(va_sock_t sock, char * data, int size, int flag);
void PrintUsage(char * cmd);
void Exit();
char *strsub(char *input, int i_begin, int i_end);
int setArg(int argc, char **argv, int *countArgument);
int countOptionValue(int argc, char **argv, int *countArgument);

#endif