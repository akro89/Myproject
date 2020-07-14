
#ifndef __MISC_GET_STORAGE_LIST_H__
#define __MISC_GET_STORAGE_LIST_H__
#define BUFSIZE				12
#define OPTIONSIZE			2
#define DELIMITER			1
#define ALL_OPTION_NUMBER	12
#define TAPE_OPTION_NUMBER	9
#define DISK_OPTION_NUMBER	8
#define DISK_LEFT_SHIFT_1	1
#define DISK_LEFT_SHIFT_4	4

struct storage_list
{
	char name[STORAGE_NAME_LENGTH];
	char type[NUMBER_STRING_LENGTH];
	char status[NUMBER_STRING_LENGTH];
	char nodename[NODE_NAME_LENGTH];
	char ownerstatus[NUMBER_STRING_LENGTH];
	char librarytype[NUMBER_STRING_LENGTH];
	char device[STORAGE_DEVICE_FILE_LENGTH];
	char address[NUMBER_STRING_LENGTH];
	char drivetype[NUMBER_STRING_LENGTH];
	char repository[STORAGE_REPOSITORY_LENGTH];
	char capacity[NUMBER_STRING_LENGTH];
	char volumeSize[NUMBER_STRING_LENGTH];
};

enum storageSelect 
{
	storagename = 0,
	type = 1,
	drivetype = 2,
	status = 3,
	nodename = 4,
	ownerstatus = 5,
	device = 6,
	address = 7,
	librarytype = 8, 
	volumeSize = 9,
	capacity = 10,
	repository = 11,
};

char * getLibraryType(int librarytype)
{
	switch(librarytype)
	{
	case 0:
		return "GENERIC";
	case 1:
		return "IBM";
	case 2:
		return "ADIC ";
	case 3:
		return "STORAGE_TEK";
	case 4:
		return "DELL";
	case 5:
		return "OVERLAND";
	case 6:
		return "QUANTUM";
	case 7:
		return "HP";
	case 8:
		return "FALCON";
	case 9:
		return "SPECTRA";
	case 10:
		return "QUALSTAR";
	default:
		return "-";
	}	
}

char * getTapeDriveType(int drivetype)
{
	switch(drivetype)
	{
	case 0:
		return "GENERIC";
	case 1:
		return "LTO";
	case 2:
		return "DLT";
	case 3:
		return "SDLT";
	case 4:
		return "AIT";
	case 5:
		return "SAIT";
	case 6:
		return "DDS";
	case 7:
		return "3590";
	case 8:
		return "3592";
	case 9:
		return "T9840";
	case 10:
		return "T9940";
	default:
		return "-";
	}	
}

char * getStorageStatus(int status)
{
	switch(status)
	{
	case 0:
		return "OFFLINE";
	case 1:
		return "ONLINE";
	case 2:
		return "DOWN";
	default:
		return "-";
	}	
}
char * getStorageOwnerStatus(int ownerstatus)
{
	switch(ownerstatus)
	{
	case 0:
		return "OFFLINE";
	case 1:
		return "ONLINE";
	case 2:
		return "DOWN";
	default:
		return "-";
	}	
}
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

// 입력받아야하는정보
int RecvReply(va_sock_t sock, char * data, int size, int flag);
char * getLibraryType(int librarytype);
char * getTapeDriveType(int drivetype);
char * getStorageStatus(int status);
char * getStorageOwnerStatus(int ownerstatus);
int InitProcess(char * filename);
int GetArgument(int argc, char ** argv);
int SendCommand();
void PrintUsage(char * cmd);
int countDelimiter(char *searchObject, char delimiter);
int setArg(int argc, char **argv, int *cnt, int option);
char *strsub(char *input, int i_begin, int i_end);
int countOptionValue(int argc, char **argv, int *countArgument, int option);
void Exit();
#endif