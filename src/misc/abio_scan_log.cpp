#include "require.h"
#include "abio.h"
#include "lib.h"
#include "vendor.h"
#include "abio_scan.h"


extern enum volumeType mountedVolumeType;

extern char driveDeviceFile[STORAGE_DEVICE_FILE_LENGTH];
extern char diskVolumeFile[MAX_PATH_LENGTH];

extern char mountedVolume[VOLUME_NAME_LENGTH];
extern int volumeIdentify;		// 0 : didn't read yet. 1 : virbak abio type. 2 : cannot identify
extern int volumeBlockSize;
extern int dataBufferSize;

extern struct backupsetInfo * backupsetList;
extern int backupsetListNumber;

extern char restoreBackupset[BACKUPSET_ID_LENGTH];
extern char restoreClient[NODE_NAME_LENGTH];
extern char restoreDirectory[MAX_PATH_LENGTH];
extern int * restoreFileList;
extern int restoreFileNumber;

extern int successFileNumber;
extern int failFileNumber;

extern char currentDirectory[MAX_PATH_LENGTH];

void PrintRestoreStartLog()
{
	char topSplitBarLine[1024];
	char jobTypeLine[1024];
	char clientLine[1024];
	char backupsetLine[1024];
	char backupTimeLine[1024];
	char restoreFileNumberLine[1024];
	char bottomSplitBarLine[1024];
	
	char restoreLogFile[MAX_PATH_LENGTH];
	char ** lines;
	int lineNumber;
	
	#ifdef __ABIO_WIN
		char * convertLine;
		int convertLineSize;
	#endif
	
	time_t current_t;
	char logTime[TIME_STAMP_LENGTH];
	
	time_t backup_t;
	char backupTime[TIME_STAMP_LENGTH];
	
	int i;
	
	
	
	// initialize variables
	memset(topSplitBarLine, 0, sizeof(topSplitBarLine));
	memset(jobTypeLine, 0, sizeof(jobTypeLine));
	memset(clientLine, 0, sizeof(clientLine));
	memset(backupsetLine, 0, sizeof(backupsetLine));
	memset(backupTimeLine, 0, sizeof(backupTimeLine));
	memset(restoreFileNumberLine, 0, sizeof(restoreFileNumberLine));
	memset(bottomSplitBarLine, 0, sizeof(bottomSplitBarLine));
	
	memset(restoreLogFile, 0, sizeof(restoreLogFile));
	memset(logTime, 0, sizeof(logTime));
	memset(backupTime, 0, sizeof(backupTime));
	
	
	current_t = time(NULL);
	va_make_time_stamp(current_t, logTime, TIME_STAMP_TYPE_EXTERNAL_EN);
	
	backup_t = (time_t)atoi(restoreBackupset);
	va_make_time_stamp(backup_t, backupTime, TIME_STAMP_TYPE_EXTERNAL_EN);
	
	
	sprintf(topSplitBarLine,						"%s ====================================================================================================", logTime);
	sprintf(jobTypeLine,							"%s Job Type                          : RESTORE", logTime);
	sprintf(clientLine,								"%s Client                            : %s", logTime, restoreClient);
	sprintf(backupsetLine,							"%s Backup Set ID                     : %s", logTime, restoreBackupset);
	sprintf(backupTimeLine,							"%s Backup Time                       : %s", logTime, backupTime);
	
	if (restoreFileNumber > 0)
	{
		sprintf(restoreFileNumberLine,				"%s Request Restore File Number       : %d", logTime, restoreFileNumber);
	}
	else
	{
		for (i = 0; i < backupsetListNumber; i++)
		{
			if (!strcmp(backupsetList[i].backupset, restoreBackupset))
			{
				sprintf(restoreFileNumberLine,		"%s Request Restore File Number       : %d", logTime, backupsetList[i].fileNumber);
				
				break;
			}
		}
	}
	
	sprintf(bottomSplitBarLine,						"%s ----------------------------------------------------------------------------------------------------", logTime);
	
	
	lineNumber = 7;
	lines = (char **)malloc(sizeof(char *) * lineNumber);
	
	lines[0] = topSplitBarLine;
	lines[1] = jobTypeLine;
	lines[2] = clientLine;
	lines[3] = backupsetLine;
	lines[4] = backupTimeLine;
	lines[5] = restoreFileNumberLine;
	lines[6] = bottomSplitBarLine;
	
	
	sprintf(restoreLogFile, "%s.log", restoreBackupset);
	//ky88 2012.03.22 
	va_append_text_file_lines(currentDirectory, restoreLogFile, lines, NULL, lineNumber);
	//va_append_text_file_lines(NULL, restoreLogFile, lines, NULL, lineNumber);
	
	for (i = 0; i < lineNumber; i++)
	{
		#ifdef __ABIO_UNIX
			printf("%s\n", lines[i]);
		#elif __ABIO_WIN
			if (va_convert_string_from_utf8(ENCODING_UNKNOWN, lines[i], (int)strlen(lines[i]), (void **)&convertLine, &convertLineSize) > 0)
			{
				printf("%s\n", convertLine);
				va_free(convertLine);
			}
		#endif
	}
	
	va_free(lines);
}

void PrintRestoreFileLog(int fileNumber, char * source, char * target)
{
	char restoreLogFile[MAX_PATH_LENGTH];
	
	time_t current_t;
	char logTime[TIME_STAMP_LENGTH];
	
	char logmsg[1024];
	
	#ifdef __ABIO_WIN
		char * convertLogmsg;
		int convertLogmsgSize;
	#endif
	
	
	
	// initialize variables
	memset(restoreLogFile, 0, sizeof(restoreLogFile));
	memset(logTime, 0, sizeof(logTime));
	memset(logmsg, 0, sizeof(logmsg));
	
	
	current_t = time(NULL);
	va_make_time_stamp(current_t, logTime, TIME_STAMP_TYPE_EXTERNAL_EN);
	
	
	sprintf(logmsg, "%s %d. [%s] ==> [%s] ..... ", logTime, fileNumber, source, target);
	
	
	sprintf(restoreLogFile, "%s.log", restoreBackupset);
	//ky88 2012.03.22
	va_append_text_file(currentDirectory, restoreLogFile, logmsg, (int)strlen(logmsg));
	
	#ifdef __ABIO_UNIX
		printf("%s", logmsg);
	#elif __ABIO_WIN
		if (va_convert_string_from_utf8(ENCODING_UNKNOWN, logmsg, (int)strlen(logmsg), (void **)&convertLogmsg, &convertLogmsgSize) > 0)
		{
			printf("%s", convertLogmsg);
			va_free(convertLogmsg);
		}
	#endif
}

void PrintRestoreFileResult(int result)
{
	char restoreLogFile[MAX_PATH_LENGTH];
	
	char logmsg[1024];
	
	#ifdef __ABIO_WIN
		char * convertLogmsg;
		int convertLogmsgSize;
	#endif
	
	
	
	// initialize variables
	memset(restoreLogFile, 0, sizeof(restoreLogFile));
	memset(logmsg, 0, sizeof(logmsg));
	
	
	#ifdef __ABIO_UNIX
		if (result == 0)
		{
			sprintf(logmsg, "ok\n");
		}
		else
		{
			sprintf(logmsg, "fail\n");
		}
	#elif __ABIO_WIN
		if (result == 0)
		{
			sprintf(logmsg, "ok\r\n");
		}
		else
		{
			sprintf(logmsg, "fail\r\n");
		}
	#endif
	
	
	sprintf(restoreLogFile, "%s.log", restoreBackupset);
	//ky88 2012.03.22
	va_append_text_file(currentDirectory, restoreLogFile, logmsg, (int)strlen(logmsg));
	
	#ifdef __ABIO_UNIX
		printf("%s", logmsg);
	#elif __ABIO_WIN
		if (va_convert_string_from_utf8(ENCODING_UNKNOWN, logmsg, (int)strlen(logmsg), (void **)&convertLogmsg, &convertLogmsgSize) > 0)
		{
			printf("%s", convertLogmsg);
			va_free(convertLogmsg);
		}
	#endif
}

void PrintRestoreResult()
{
	char topSplitBarLine[1024];
	char successFileNumberLine[1024];
	char failFileNumberLine[1024];
	
	char ** lines;
	int lineNumber;
	
	#ifdef __ABIO_WIN
		char * convertLine;
		int convertLineSize;
	#endif
	
	char restoreLogFile[MAX_PATH_LENGTH];
	
	time_t current_t;
	char logTime[TIME_STAMP_LENGTH];
	
	int i;
	
	
	
	// initialize variables
	memset(topSplitBarLine, 0, sizeof(topSplitBarLine));
	memset(successFileNumberLine, 0, sizeof(successFileNumberLine));
	memset(failFileNumberLine, 0, sizeof(failFileNumberLine));
	
	memset(restoreLogFile, 0, sizeof(restoreLogFile));
	memset(logTime, 0, sizeof(logTime));
	
	
	current_t = time(NULL);
	va_make_time_stamp(current_t, logTime, TIME_STAMP_TYPE_EXTERNAL_EN);
	
	
	sprintf(topSplitBarLine,						"%s ----------------------------------------------------------------------------------------------------", logTime);
	sprintf(successFileNumberLine,					"%s Completed Restore File Number     : %d", logTime, successFileNumber);
	sprintf(failFileNumberLine,						"%s Failed Restore File Number        : %d", logTime, failFileNumber);
	
	
	lineNumber = 5;
	lines = (char **)malloc(sizeof(char *) * lineNumber);
	
	lines[0] = topSplitBarLine;
	lines[1] = successFileNumberLine;
	lines[2] = failFileNumberLine;
	lines[3] = " ";
	lines[4] = " ";
	
	
	sprintf(restoreLogFile, "%s.log", restoreBackupset);
	//ky88 2012.03.22
	va_append_text_file_lines(currentDirectory, restoreLogFile, lines, NULL, lineNumber);
	
	for (i = 0; i < lineNumber; i++)
	{
		#ifdef __ABIO_UNIX
			printf("%s\n", lines[i]);
		#elif __ABIO_WIN
			if (va_convert_string_from_utf8(ENCODING_UNKNOWN, lines[i], (int)strlen(lines[i]), (void **)&convertLine, &convertLineSize) > 0)
			{
				printf("%s\n", convertLine);
				va_free(convertLine);
			}
		#endif
	}
	
	va_free(lines);
}
