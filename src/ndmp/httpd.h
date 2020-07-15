#ifndef __NDMP_HTTPD_H__
#define __NDMP_HTTPD_H__



int InitProcess(char * filename);
int GetModuleConfiguration();

void ResponseRequest();

void GetNdmpFileList();
void PutFileList();
void PutBlockList();

void GetTapeLibraryInventory();
void MoveMedium();

void WriteLabel();
void GetLabel();
void UnloadTapeVolume();
void GetTapeDriveSerial();
void GetTapeLibrarySerial();
void GetScsiInventory();

void GetNodeModule();

int isalnumstr(char * str);

void Exit();
void CheckDevice();
//int va_ndmp_config_get_fs_info_debug(void * clientData, ndmpFileSystemList * fslist);
//int va_ndmp_library_make_inventory_debug(void * connection, char * device, enum elementType type, struct tapeInventory * inventory, int initialize);
enum functionNumber
{
	FUNCTION_InitProcess = 1,
	FUNCTION_GetModuleConfiguration = 2,
	FUNCTION_ResponseRequest = 3,
	
	FUNCTION_GetNdmpFileList = 10,
	FUNCTION_PutFileList = 11,
	FUNCTION_PutBlockList = 12,
	
	FUNCTION_GetTapeLibraryInventory = 20,
	FUNCTION_MoveMedium = 21,
	
	FUNCTION_WriteLabel = 30,
	FUNCTION_GetLabel = 31,
	FUNCTION_UnloadTapeVolume = 32,
	FUNCTION_GetTapeDriveSerial = 33,
	
	FUNCTION_GetNodeModule = 40,
	
	FUNCTION_Exit = 100,
};

int va_ndmp_config_get_fs_info_debug(void * clientData, ndmpFileSystemList * fslist);
#endif
