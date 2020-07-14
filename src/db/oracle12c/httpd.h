#ifndef __ORA12CHTTPD_H__
#define __ORA12CHTTPD_H__


struct tableSpaceItem
{
	char tableSpace[ABIO_FILE_LENGTH];
	char database[ABIO_FILE_LENGTH];
	char ** dataFile;
	int fileNumber;
	time_t mtime;
	__uint64 size;
};


int InitProcess(char * filename);
int GetModuleConfiguration();
void LoadLogDescription();
void GetEntireDatabases();
void ResponseRequest();
void GetOracle12cFileList();

int GetTableSpaceList();
int GetArchiveLogSize();
int GetControlFileSize();

int ExecuteSql(char * query);
int MakeQueryFile(char * query);
int CheckQueryError(char * query);
void PrintQueryFileError(char * logMsgID, ...);

void Exit();


/**************************************************************************************************
 * function prototype : 
 * DESCRIPTION
 *
 * ARGUMENTS
 *
 * RETURN VALUES
 *************************************************************************************************/


#endif
