#ifndef __HTTPD_H__
#define __HTTPD_H__


#define SUSPENDED_JOB_MANAGEMENT_JOB_SCHEDULING_INTERVAL		600			// 10 minutes
#define LOG_RETENTION_MANAGEMENT_JOB_SCHEDULING_INTERVAL		3600		// 1 hour
#define LICENSE_SUMMARY_LOADING_INTERVAL		120		// 2 minutes

//#issue 191
#define MSSQL_SEPARATOR_DATABASE	"[Database] "
#define MSSQL_SEPARATOR_FILEGROUP	"[Filegroup] "
#define MSSQL_SEPARATOR_FILE		"[File] "

struct httpdRequest
{
	char * request;
	int requestSize;
	
	struct httpdRequest * next;
};

struct jobStatusFront
{
	int completeJobNumber;
	int failJobNumber;
	int cancelJobNumber;
	int suspendJobNumber;
};

/* httpd.c */
#ifdef __ABIO_UNIX
	int StartHttpd(char * filename);
#elif __ABIO_WIN
	int StartHttpd();
#endif

#ifdef __ABIO_UNIX
	int InitProcess(char * filename);
#elif __ABIO_WIN
	int InitProcess();
#endif

int GetModuleConfiguration();
int CheckConfigurationValid();
int InitHttpd();

int Httpd();
int CheckConnectionRestriction(char * ip);
void CheckInterfaceConf(va_sock_t sock);

#ifdef __ABIO_UNIX
	va_thread_return_t ProcessMessageUnix(void * arg);
#elif __ABIO_WIN
	void ProcessMessageWin(void * arg);
#endif
int RecvCommand(va_sock_t sock, char * cmd);
int ResponseCommand(char * cmd, va_sock_t sock);
int ResponseCommandBackup(char * cmd, va_sock_t sock);
int ResponseCommandBackupset(char * cmd, va_sock_t sock);
int ResponseCommandDevice(char * cmd, va_sock_t sock);
int ResponseCommandEtc(char * cmd, va_sock_t sock);
int ResponseCommandHost(char * cmd, va_sock_t sock);
int ResponseCommandJob(char * cmd, va_sock_t sock);
int ResponseCommandMedia(char * cmd, va_sock_t sock);
int ResponseCommandSchedule(char * cmd, va_sock_t sock);
int ResponseCommandUser(char * cmd, va_sock_t sock);
int ResponseCommandFrontPage(char * cmd, va_sock_t sock);

int RegisterMasterServer();
int RegisterMasterUser();
int RegisterMasterUserGroup();

enum platformType GetPlatformType();
int RegisterSystemVolumePool();

// #issue 170 : Retry Backup
/*
int SetNodeInfo(char * nodename, struct ckClient * ckClient, struct ndmpNode * ndmp);
int CheckNodeAlive(char * ip, char * port);
*/

int ComparatorString(const void * a1, const void * a2);

int GetTapeLibraryInventory(char * nodename, char * device, struct tapeInventory * inventory);
int MoveMedium(char * nodename, char * device, int source, int destination, enum elementType sourceElementType, enum elementType destinationElementType, struct ckBackup * commandData);

void CloseThread();
void Exit();

#ifdef __ABIO_UNIX	
	va_thread_return_t WaitChild(void * arg);
#endif

#ifdef __ABIO_WIN
	void Httpd_service_handler(DWORD fdwcontrol);
	void Httpd_set_status(DWORD dwstate, DWORD dwaccept);
#endif

void PrintDaemonLog(char * message, ...);
void initCommandDataSetCommandIDIP(struct ckBackup * pCommandData, char value[][1024]);

/* httpd_backup.c */
int FsBackup(char * cmd, va_sock_t sock);
int DBBackup(char * cmd, va_sock_t sock);
int FsRestore(char * cmd, va_sock_t sock);
int DBRestore(char * cmd, va_sock_t sock);
int GetFileList(char * cmd, va_sock_t sock);
int GetDirectoryList(char * cmd, va_sock_t sock);
int GetFilesystemList(char * cmd, va_sock_t sock);
int GetVmwareDatastoreList(char * cmd, va_sock_t sock);
int GetVmwareFileList(char * cmd, va_sock_t sock);
int GetVmwareDiskList(char * cmd, va_sock_t sock);
int GetVmwareLatestList(char * cmd, va_sock_t sock);
int GetVmwareBackupIDList(char * cmd, va_sock_t sock);
int GetVmwareCatalogDB(char * cmd, va_sock_t sock);
int GetVmwareBackupList(char * cmd, va_sock_t sock);
int GetVmwareBackupPath(char * cmd, va_sock_t sock);
int GetNdmpFileList(char * cmd, va_sock_t sock);
int GetOracleFileList(char * cmd, va_sock_t sock);
int GetOracleRmanFileList(char * cmd, va_sock_t sock);
int GetMssqlFileList(char * cmd, va_sock_t sock);
int GetExchangeFileList(char * cmd, va_sock_t sock);
int GetExchangeMailboxFileList(char * cmd, va_sock_t sock);
int GetExchangeMailboxDirectoryList(char * cmd, va_sock_t sock);
int GetExchangeMailboxStoreList(char * cmd, va_sock_t sock);
int GetSybaseFileList(char * cmd, va_sock_t sock);
int GetSybaseIqFileList(char * cmd, va_sock_t sock);
int GetDB2DatabaseList(char * cmd, va_sock_t sock);
int GetDB2TablespaceList(char * cmd, va_sock_t sock);
int GetAltibaseFileList(char * cmd, va_sock_t sock);
int GetNotesFileList(char * cmd, va_sock_t sock);
int PutFileList(char * cmd, va_sock_t sock);
int GetDBInfo(char * cmd, va_sock_t sock);
int GetBackupsetList(char * cmd, va_sock_t sock);
int GetOracle12cFileList(char * cmd, va_sock_t sock);
int GetSQLBackTrackFileList(char * cmd, va_sock_t sock);
int GetMysqlFileList(char * cmd, va_sock_t sock);
int GetCatalogDB(char * cmd, va_sock_t sock);
int GetCatalogDirectoryDB(char * cmd, va_sock_t sock);
int GetCatalogFileDB(char * cmd, va_sock_t sock);
//sharepoint
int GetSharePointFarmList(char * cmd, va_sock_t sock);
int GetSharePointSiteList(char * cmd, va_sock_t sock);
int GetSharepointBackupsetList(char * cmd, va_sock_t sock);
int GetSharepointCatalogDB(char * cmd, va_sock_t sock);
//#issue164
int GetSmartRestoreFileList(char * cmd, va_sock_t sock);
//#issue344
int GetOSFilesystemList(char * cmd, va_sock_t sock);

int GetDBBackupInstance(char * cmd, va_sock_t sock);
int GetNdmpBackupIDList(char * cmd, va_sock_t sock);
int GetNdmpCatalogDB(char * cmd, va_sock_t sock);
int GetOracleBackupTablespaceList(char * cmd, va_sock_t sock);
int GetOracle12cBackupTablespaceList(char * cmd, va_sock_t sock);
int GetOracleCatalogDB(char * cmd, va_sock_t sock);
int GetOracle12cCatalogDB(char * cmd, va_sock_t sock);
int GetOracleInstance(char * cmd, va_sock_t sock);
int GetOracle12cInstance(char * cmd, va_sock_t sock);
int GetOracleTableSpace(char * cmd, va_sock_t sock);
int GetOracle12cTableSpace(char * cmd, va_sock_t sock);
int GetOracleRmanBackupTablespaceList(char * cmd, va_sock_t sock);
int GetOracleRmanCatalogDB(char * cmd, va_sock_t sock);
int GetOracleRmanRestoreBackupset(char * cmd, va_sock_t sock);
int GetOracleRmanArchiveList(char * cmd, va_sock_t sock);
int GetOracleTableSpaceCatalog(char * cmd, va_sock_t sock);
int GetOracle12cTableSpaceCatalog(char * cmd, va_sock_t sock);
int GetMssqlBackupDatabaseList(char * cmd, va_sock_t sock);
int GetMssqlCatalogDB(char * cmd, va_sock_t sock);
int GetExchangeBackupStorageGroupList(char * cmd, va_sock_t sock);
int GetExchangeCatalogDB(char * cmd, va_sock_t sock);
int GetExchangeMailboxCatalogDB(char * cmd, va_sock_t sock);
int GetSapBackupIDList(char * cmd, va_sock_t sock);
int GetSapCatalogDB(char * cmd, va_sock_t sock);
int GetSybaseBackupDatabaseList(char * cmd, va_sock_t sock);
int GetSybaseCatalogDB(char * cmd, va_sock_t sock);
int GetSybaseIqBackupDatabaseList(char * cmd, va_sock_t sock);
int GetSybaseIqCatalogDB(char * cmd, va_sock_t sock);
int GetDB2CatalogDB(char * cmd, va_sock_t sock);
int GetDB2BackupDatabaseList(char * cmd, va_sock_t sock);
int GetDB2BackupTablespaceList(char * cmd, va_sock_t sock);
int GetDB2CatalogDBLog(char * cmd, va_sock_t sock);
int GetDB2BackupLogDatabaseList(char * cmd, va_sock_t sock);
int GetAltibaseBackupTablespaceList(char * cmd, va_sock_t sock);
int GetAltibaseCatalogDB(char * cmd, va_sock_t sock);
int GetNotesCatalogDB(char * cmd, va_sock_t sock);
int GetInformixList(char * cmd, va_sock_t sock);
int GetInformixBackupTablespaceList(char * cmd, va_sock_t sock);
int GetInformixCatalogDB(char * cmd, va_sock_t sock);
int GetInformixInstance(char * cmd, va_sock_t sock);
int GetInformixTablespace(char* cmd, va_sock_t sock);
int GetInformixTablespaceCatalog(char* cmd, va_sock_t sock);
int GetInformixOnbarList(char* cmd, va_sock_t sock);
int GetInformixBackupDBspaceList(char * cmd, va_sock_t sock);
int GetInformixOnbarCatalogDB(char * cmd, va_sock_t sock);
int GetInformixOnbarInstance(char * cmd, va_sock_t sock);
int GetInformixOnbarDBSPACE(char * cmd, va_sock_t sock);
int GetInformixOnbarDBSPACECatalog(char * cmd, va_sock_t sock);
int GetTiberoFileList(char * cmd, va_sock_t sock);
int GetTiberoBackupTablespaceList(char * cmd, va_sock_t sock);
int GetTiberoCatalogDB(char * cmd, va_sock_t sock);
int GetTiberoInstance(char * cmd, va_sock_t sock);
int GetTiberoTableSpace(char * cmd, va_sock_t sock);
int GetTiberoTableSpaceCatalog(char * cmd, va_sock_t sock);
int GetOracleArchiveList(char * cmd, va_sock_t sock);
int GetSQLBackTrackstance(char * cmd, va_sock_t sock);
int GetSQLBackTrackTableSpace(char * cmd, va_sock_t sock);
int GetSQLBackTrackTableSpaceCatalog(char * cmd, va_sock_t sock);
int GetMySQLBackupDatabaseList(char * cmd, va_sock_t sock);
int GetMySQLCatalogDB(char * cmd, va_sock_t sock);

/* httpd_backupset.c */
int GetBackupsetLog(char * cmd, va_sock_t sock);
int GetBackupsetFilelist(char * cmd, va_sock_t sock);
int GetBackupsetCatalog(char * cmd, va_sock_t sock);
int GetBackupsetCataloglist(char * cmd, va_sock_t sock);
int GetLatestCataloglist(char * cmd, va_sock_t sock);
int GetBackupsetVolumeList(char * cmd, va_sock_t sock);
int ExpireBackupset(char * cmd, va_sock_t sock);
int VerifyBackupset(char * cmd, va_sock_t sock);
int MigrateBackupset(char * cmd, va_sock_t sock);
int DuplicateBackupset(char * cmd, va_sock_t sock);
int ChangeBackupsetRetention(char * cmd, va_sock_t sock);
int ShrinkClientCatalog(char *cmd,va_sock_t sock);
int ShrinkMasterCatalog(char *cmd,va_sock_t sock);
int DeleteSlaveCatalog(char *cmd,va_sock_t sock);
int DeleteSlaveLog(char *cmd,va_sock_t sock);
int FindFiles(char * cmd, va_sock_t sock);

/* httpd_device.c */
int GetStorageList(char * cmd, va_sock_t sock);
int GetStorageTapeLibrary(char * cmd, va_sock_t sock);
int GetTapeLibraryOwnerList(char * cmd, va_sock_t sock);
int GetTapeDriveList(char * cmd, va_sock_t sock);
int GetTapeDriveOwnerList(char * cmd, va_sock_t sock);
int GetStorageStandAloneTapeDrive(char * cmd, va_sock_t sock);
int GetStorageDisk(char * cmd, va_sock_t sock);
int GetStorageOwnerList(char * cmd, va_sock_t sock);
int GetScsiInventory(char * cmd, va_sock_t sock);
int GetTapeDriveAddress(char * cmd, va_sock_t sock);
int GetTapeLibrarySerial(char * cmd, va_sock_t sock);
int GetTapeDriveSerial(char * cmd, va_sock_t sock);
int GetTapeLibrarySlotNumber(char * cmd, va_sock_t sock);
int AddStorageTapeLibrary(char * cmd, va_sock_t sock);
int AddTapeDrive(char * cmd, va_sock_t sock);
int AddTapeDriveStorageOwner(char * cmd, va_sock_t sock);
int AddStorageStandAloneTapeDrive(char * cmd, va_sock_t sock);
int AddStorageDisk(char * cmd, va_sock_t sock);
int ResetStorageConfiguration(char * cmd, va_sock_t sock);
int UpdateStorageTapeLibrary(char * cmd, va_sock_t sock);
int UpdateTapeDrive(char * cmd, va_sock_t sock);
int UpdateTapeDriveStorageOwner(char * cmd, va_sock_t sock);
int UpdateStorageStandAloneTapeDrive(char * cmd, va_sock_t sock);
int UpdateStorageDisk(char * cmd, va_sock_t sock);
int DeleteStorageTapeLibrary(char * cmd, va_sock_t sock);
int DeleteTapeDrive(char * cmd, va_sock_t sock);
int DeleteTapeDriveStorageOwner(char * cmd, va_sock_t sock);
int DeleteStorageStandAloneTapeDrive(char * cmd, va_sock_t sock);
int DeleteStorageDisk(char * cmd, va_sock_t sock);
int GetStorage(char * cmd, va_sock_t sock);
/* httpd_etc.c */


/* httpd_host.c */
int GetNodeList(char * cmd, va_sock_t sock);
int GetSelectedNodeList(char * cmd, va_sock_t sock);
int GetModuleConf(char * cmd, va_sock_t sock);
int SetModuleConf(char * cmd, va_sock_t sock);
int GetCatalogInfo(char * cmd, va_sock_t sock);
int ServiceStart(char * cmd, va_sock_t sock);
int ServiceStop(char * cmd, va_sock_t sock);
int GetNodeNumber(char * cmd, va_sock_t sock);
int GetNodeInformation(char * cmd, va_sock_t sock);
int AddNode(char * cmd, va_sock_t sock);
int UpdateNode(char * cmd, va_sock_t sock);
int DeleteNode(char * cmd, va_sock_t sock);
int GetNodeModule(char * cmd, va_sock_t sock);
int CheckNodeModule(char * cmd, va_sock_t sock);
int CheckVMwareNodeModule(char * cmd, va_sock_t sock);



/* httpd_job.c */
int GetRunningJobLog(char * cmd, va_sock_t sock);
int GetTerminalJobLog(char * cmd, va_sock_t sock);
int GetTerminalErrorJobLog(char * cmd, va_sock_t sock);
int GetJobLogRun(char * cmd, va_sock_t sock);
int GetJobLogDescription(char * cmd, va_sock_t sock);
int GetEventLog(char * cmd, va_sock_t sock);
int GetEventLogRun(char * cmd, va_sock_t sock);
int DeleteLog(char * cmd, va_sock_t sock);
int CancelJob(char * cmd, va_sock_t sock);
int StopJob(char * cmd, va_sock_t sock);
int PauseJob(char * cmd, va_sock_t sock);
int ResumeJob(char * cmd, va_sock_t sock);


/* httpd_mana_log.c */
va_thread_return_t GetLogRequest(void * arg);
va_thread_return_t ProcessLogRequest(void * arg);


/* httpd_mana_retention.c */
va_thread_return_t CheckRetention(void * arg);


/* httpd_mana_running.c */
va_thread_return_t CheckRunningJob(void * arg);
// #issue 170 : Retry Backup
void Retry_Backup(struct ckBackup * commandData);


/* httpd_media.c */
int GetVolumePoolList(char * cmd, va_sock_t sock);
int GetSelectedVolumePoolList(char * cmd, va_sock_t sock);
int GetVolumeList(char * cmd, va_sock_t sock);
int GetVolumePoolVolumeList(char * cmd, va_sock_t sock);
int GetStorageVolumeList(char * cmd, va_sock_t sock);
int AddVolumePool(char * cmd, va_sock_t sock);
int UpdateVolumePool(char * cmd, va_sock_t sock);
int DeleteVolumePool(char * cmd, va_sock_t sock);
int GetTapeLibraryInventoryVolumeList(char * cmd, va_sock_t sock);
int GetDiskInventoryVolumeList(char * cmd, va_sock_t sock);
int GetTapeLibraryVolumeHeader(char * cmd, va_sock_t sock);
int GetImportVolumeList(char * cmd, va_sock_t sock);
int AddTapeLibraryVolume(char * cmd, va_sock_t sock);
int AddStandAloneTapeDriveVolume(char * cmd, va_sock_t sock);
int ResetVolumeConfiguration(char * cmd, va_sock_t sock);
int UpdateVolume(char * cmd, va_sock_t sock);
int UpdateVolumeVolumePool(char * cmd, va_sock_t sock);
int MakeVolumeAccessReadWrite(char * cmd, va_sock_t sock);
int MakeVolumeAccessReadOnly(char * cmd, va_sock_t sock);
int MakeVolumePoolAccessNonPermanent(char * cmd, va_sock_t sock);
int MakeVolumePoolAccessPermanent(char * cmd, va_sock_t sock);
int DeleteVolume(char * cmd, va_sock_t sock);
int ImportVolume(char * cmd, va_sock_t sock);
int ExportVolume(char * cmd, va_sock_t sock);
int AutoExportVolume(char * cmd);
int AutoImportVolume(char * cmd);
int UnmountVolume(char * cmd, va_sock_t sock);
int ExpireVolume(char * cmd, va_sock_t sock);


/* httpd_schedule.c */
int GetPolicyList(char * cmd, va_sock_t sock);
int GetPolicy(char * cmd, va_sock_t sock);
int GetPolicyFileList(char * cmd, va_sock_t sock);
int GetScheduleList(char * cmd, va_sock_t sock);
int GetSchedule(char * cmd, va_sock_t sock);
int AddPolicy(char * cmd, va_sock_t sock);
int UpdatePolicy(char * cmd, va_sock_t sock);
int DeletePolicy(char * cmd, va_sock_t sock);
int CopyPolicy(char * cmd, va_sock_t sock);
int ActivatePolicy(char * cmd, va_sock_t sock);
int DeactivatePolicy(char * cmd, va_sock_t sock);
int AddSchedule(char * cmd, va_sock_t sock);
int UpdateSchedule(char * cmd, va_sock_t sock);
int DeleteSchedule(char * cmd, va_sock_t sock);
int CopySchedule(char * cmd, va_sock_t sock);
int ActivateSchedule(char * cmd, va_sock_t sock);
int DeactivateSchedule(char * cmd, va_sock_t sock);
int RunSchedule(char * cmd, va_sock_t sock);

void CopyScheduleOption(struct scheduleType * main, struct scheduleType * copy);

int GetOtherScheduleList(char * cmd, va_sock_t sock);
int GetOtherSchedule(char * cmd, va_sock_t sock);
int AddOtherSchedule(char * cmd, va_sock_t sock);
int UpdateOtherSchedule(char * cmd, va_sock_t sock);
int DeleteOtherSchedule(char * cmd, va_sock_t sock);
int CopyOtherSchedule(char * cmd, va_sock_t sock);
int ActivateOtherSchedule(char * cmd, va_sock_t sock);
int DeactivateOtherSchedule(char * cmd, va_sock_t sock);
int RunOtherSchedule(char * cmd, va_sock_t sock);
int SFCheckSchedule(char * cmd, va_sock_t sock);

/* httpd_user.c */
int GetUserList(char * cmd, va_sock_t sock);
int GetUnregisteredClientList(char * cmd, va_sock_t sock);
int AddUser(char * cmd, va_sock_t sock);
int ChangeUserPasswd(char * cmd, va_sock_t sock);
int DeleteUser(char * cmd, va_sock_t sock);
int ChangeUserPermission(char * cmd, va_sock_t sock);
int ChangeUserInfo(char * cmd, va_sock_t sock);

int GetUserGroupList(char * cmd, va_sock_t sock);
int AddUserGroup(char * cmd, va_sock_t sock);
int DeleteUserGroup(char * cmd, va_sock_t sock);
int RenameUserGroup(char * cmd, va_sock_t sock);
int ChangeUserGroupPermission(char * cmd, va_sock_t sock);
int GetUserPoolList(char * cmd, va_sock_t sock);
int GetUserNodeList(char * cmd, va_sock_t sock);
int AddUserPool(char * cmd, va_sock_t sock);
int AddUserNode(char * cmd, va_sock_t sock);
int DeleteUserPool(char * cmd, va_sock_t sock);
int DeleteUserNode(char * cmd, va_sock_t sock);
int AddAllUserPool(char * cmd, va_sock_t sock);
int AddAllUserNode(char * cmd, va_sock_t sock);
int DeleteAllUserPool(char * cmd, va_sock_t sock);
int DeleteAllUserNode(char * cmd, va_sock_t sock);
/* httpd_login.c */
int LogIn(char * cmd, va_sock_t sock, char * connectionIP);
int LogInSelected(char * cmd, va_sock_t sock, char * connectionIP);
int LogIn_v5_uway(char * cmd, va_sock_t sock, char * connectionIP);


/* httpd_frontpage.c */
int GetJobStatusWeek(struct ckBackup commandData, struct jobStatusFront *jobStatus);
int GetFrontPageNodeInfo(char * cmd, va_sock_t sock);
int GetFrontPageInfo(char *cmd, va_sock_t sock);
int GetBackupJobs(char * cmd, va_sock_t sock);
int GetCurrentAndFutureJobs(char * cmd, va_sock_t sock);
int GetStorageInfo(char * cmd, va_sock_t sock);
int GetDataBaseAgentNumber(char * cmd, va_sock_t sock);
int GetMasterServerStatus(char * cmd, va_sock_t sock);
int GetCheckProcessUsage(char * cmd, va_sock_t sock);

/* httpd_license.c */
va_thread_return_t LoadLicenseSummary(void * arg);
int GetLicenseKeyList(char * cmd, va_sock_t sock);
int GetLicenseSummary(char * cmd, va_sock_t sock);
int AddLicenseKey(char * cmd, va_sock_t sock);
int RemoveLicenseKey(char * cmd, va_sock_t sock);
int GenerateEvaluationKey(char * cmd, va_sock_t sock);
void SetLicenseValidationDate();

/* httpd_web.c */
void response_not_found(char * file, int sock);
void response_ok(int sock , char * file , int status);

int PostCommand(char * cmd, va_sock_t sock);
int Web_LogIn(char * cmd, va_sock_t sock, char value[][1024], int valueNumber, char * connectionIP);
int WebGetMainPage(char * cmd, va_sock_t sock);
int WebGetFrontPageInfo(char * cmd, va_sock_t sock, char value[][1024], int valueNumber);
int WebGetStorageInfo(char * cmd, va_sock_t sock, char value[][1024], int valueNumber);
int WebGetFrontPageNodeInfo(char * cmd, va_sock_t sock, char value[][1024], int valueNumber);
int WebGetDataBaseAgentNumber(char * cmd, va_sock_t sock, char value[][1024], int valueNumber);
int WebGetTerminalJobLog(char * cmd, va_sock_t sock, char value[][1024], int valueNumber);
int WebGetRunningJobLog(char * cmd, va_sock_t sock, char value[][1024], int valueNumber);
int WebGetRunningJobThroughput(char * cmd, va_sock_t sock, char value[][1024], int valueNumber);
int SendLoginPage(va_sock_t sock);
int GetCommand(char * cmd, va_sock_t sock);

int WriteVolumeList(struct volumeList * head);
struct volumeList * AddVolumeList(struct volumeList * head, struct volume volume);
void FreeVolumeList(struct volumeList * p);
int CheckVolumeList(struct volumeList * oldVolumeList, struct volume newVolume);
void CheckVolumeFile();
int CheckTapeLibrary(int address, char* storage, struct volumeList * p);

enum functionNumber
{
	FUNCTION_StartHttpd = 1, 
	FUNCTION_InitProcess = 2, 
	FUNCTION_GetModuleConfiguration = 3, 
	FUNCTION_InitHttpd = 4, 
	
	FUNCTION_Httpd = 10, 
	FUNCTION_CheckConnectionRestriction = 11, 
	FUNCTION_CheckInterfaceConf = 12, 
	FUNCTION_ProcessMessageUnix = 13, 
	FUNCTION_ProcessMessageWin = 14, 
	FUNCTION_RecvCommand = 15, 
	FUNCTION_ResponseCommand = 16, 
	FUNCTION_ResponseCommandBackup = 17, 
	FUNCTION_ResponseCommandBackupset = 18, 
	FUNCTION_ResponseCommandDevice = 19, 
	FUNCTION_ResponseCommandEtc = 20, 
	FUNCTION_ResponseCommandHost = 21, 
	FUNCTION_ResponseCommandJob = 22, 
	FUNCTION_ResponseCommandMedia = 23, 
	FUNCTION_ResponseCommandSchedule = 24, 
	FUNCTION_ResponseCommandUser = 25, 
	FUNCTION_GetCommand = 26, 
	FUNCTION_get_argument = 27, 
	FUNCTION_response_not_found = 28, 
	FUNCTION_response_ok = 29, 
	
	FUNCTION_RegisterMasterServer = 50, 
	FUNCTION_GetPlatformType = 51, 
	FUNCTION_RegisterSystemVolumePool = 52, 
	FUNCTION_SetNodeInfo = 53, 
	FUNCTION_CheckNodeAlive = 54, 
	FUNCTION_ComparatorString = 55, 
	
	FUNCTION_CloseThread = 100, 
	FUNCTION_Exit = 101, 
	FUNCTION_WaitChild = 102, 
	FUNCTION_Httpd_service_handler = 103, 
	FUNCTION_Httpd_set_status = 104, 
	
	FUNCTION_PrintHttpdCommandLog = 120, 
	FUNCTION_PrintDaemonLog = 121
};

enum functionNumberBackup
{
	FUNCTION_BACKUP_FsBackup = 1, 
	FUNCTION_BACKUP_DBBackup = 2, 
	FUNCTION_BACKUP_FsRestore = 3, 
	FUNCTION_BACKUP_DBRestore = 4, 
	FUNCTION_BACKUP_GetFileList = 5, 
	FUNCTION_BACKUP_GetDirectoryList = 6, 
	FUNCTION_BACKUP_GetFilesystemList = 7, 
	FUNCTION_BACKUP_GetOracleFileList = 8, 
	FUNCTION_BACKUP_GetMssqlFileList = 9, 
	FUNCTION_BACKUP_GetExchangeFileList = 10, 
	FUNCTION_BACKUP_PutFileList = 11, 
	FUNCTION_BACKUP_GetDBInfo = 12, 
	FUNCTION_BACKUP_GetBackupsetList = 13, 
	FUNCTION_BACKUP_GetCatalogDB = 14, 
	FUNCTION_BACKUP_GetDBBackupInstance = 15, 
	FUNCTION_BACKUP_GetOracleBackupTablespaceList = 16, 
	FUNCTION_BACKUP_GetOracleCatalogDB = 17, 
	FUNCTION_BACKUP_GetMssqlBackupDatabaseList = 18, 
	FUNCTION_BACKUP_GetMssqlCatalogDB = 19, 
	FUNCTION_BACKUP_GetExchangeBackupStorageGroupList = 20, 
	FUNCTION_BACKUP_GetExchangeCatalogDB = 21, 
	FUNCTION_BACKUP_GetSapBackupIDList = 22, 
	FUNCTION_BACKUP_GetSapCatalogDB = 23, 
	FUNCTION_BACKUP_GetNdmpCatalogDB = 24, 
	FUNCTION_BACKUP_GetOracleRmanFileList = 25, 
	FUNCTION_BACKUP_GetOracleRmanBackupTablespaceList = 26, 
	FUNCTION_BACKUP_GetOracleRmanCatalogDB = 27, 
	FUNCTION_BACKUP_GetNotesCatalogDB = 28, 
	
	FUNCTION_BACKUP_ComparatorBackupsetListDescending = 50, 
	FUNCTION_BACKUP_ComparatorMssqlBackupFileDescending = 51, 
	FUNCTION_BACKUP_ComparatorOracleArchiveLogDescending = 52, 
	FUNCTION_BACKUP_ComparatorOracleControlFileDescending = 53, 
	FUNCTION_BACKUP_ComparatorOracleTablespaceDescending = 54, 
	FUNCTION_BACKUP_ComparatorExchangeBackupObjectDescending = 55, 
	FUNCTION_BACKUP_ComparatorSapBackupIDDescending = 56, 
	FUNCTION_BACKUP_GetDepth = 57
};

enum functionNumberBackupset
{
	FUNCTION_BACKUPSET_GetBackupsetLog = 1, 
	FUNCTION_BACKUPSET_GetBackupsetFilelist = 2, 
	FUNCTION_BACKUPSET_GetBackupsetCatalog = 3, 
	FUNCTION_BACKUPSET_ExpireBackupset = 4, 
	FUNCTION_BACKUPSET_VerifyBackupset = 5, 
	FUNCTION_BACKUPSET_MigrateBackupset = 6, 
	FUNCTION_BACKUPSET_DuplicateBackupset = 7, 
	FUNCTION_BACKUPSET_ChangeBackupsetRetention = 8, 
	FUNCTION_BACKUPSET_GetLatestCataloglist = 9, 
	FUNCTION_BACKUPSET_GetBackupsetCataloglist = 10, 
	
	FUNCTION_BACKUPSET_ComparatorBackupsetDescending = 50
};

enum functionNumberDevice
{
	FUNCTION_DEVICE_GetStorageList = 1, 
	FUNCTION_DEVICE_GetStorageTapeLibrary = 2, 
	FUNCTION_DEVICE_GetTapeLibraryOwnerList = 3, 
	FUNCTION_DEVICE_GetTapeDriveList = 4, 
	FUNCTION_DEVICE_GetTapeDriveOwnerList = 5, 
	FUNCTION_DEVICE_GetStorageStandAloneTapeDrive = 6, 
	FUNCTION_DEVICE_GetStorageDisk = 7, 
	FUNCTION_DEVICE_GetStorageOwnerList = 8, 
	FUNCTION_DEVICE_GetTapeDriveAddress = 9, 
	FUNCTION_DEVICE_GetTapeDriveSerial = 10, 
	FUNCTION_DEVICE_GetTapeLibrarySlotNumber = 11, 
	FUNCTION_DEVICE_AddStorageTapeLibrary = 12, 
	FUNCTION_DEVICE_AddTapeDrive = 13, 
	FUNCTION_DEVICE_AddTapeDriveStorageOwner = 14, 
	FUNCTION_DEVICE_AddStorageStandAloneTapeDrive = 15, 
	FUNCTION_DEVICE_AddStorageDisk = 16, 
	FUNCTION_DEVICE_ResetStorageConfiguration = 17, 
	FUNCTION_DEVICE_UpdateStorageTapeLibrary = 18, 
	FUNCTION_DEVICE_UpdateTapeDrive = 19, 
	FUNCTION_DEVICE_UpdateTapeDriveStorageOwner = 20, 
	FUNCTION_DEVICE_UpdateStorageStandAloneTapeDrive = 21, 
	FUNCTION_DEVICE_UpdateStorageDisk = 22, 
	FUNCTION_DEVICE_DeleteStorageTapeLibrary = 23, 
	FUNCTION_DEVICE_DeleteTapeDrive = 24, 
	FUNCTION_DEVICE_DeleteTapeDriveStorageOwner = 25, 
	FUNCTION_DEVICE_DeleteStorageStandAloneTapeDrive = 26, 
	FUNCTION_DEVICE_DeleteStorageDisk = 27, 
	
	FUNCTION_DEVICE_SortStorageList = 50, 
	FUNCTION_DEVICE_ComparatorStorage = 51, 
	FUNCTION_DEVICE_ComparatorTapeDrive = 52, 
	FUNCTION_DEVICE_MakeDiskStorageRepository = 53
};

enum functionNumberEtc
{
	FUNCTION_ETC_GetLicenseKeyList = 1,
	FUNCTION_ETC_AddLicenseKey = 2,
	FUNCTION_ETC_RemoveLicenseKey = 3
};

enum functionNumberHost
{
	FUNCTION_HOST_GetNodeList = 1, 
	FUNCTION_HOST_AddNode = 2, 
	FUNCTION_HOST_UpdateNode = 3, 
	FUNCTION_HOST_DeleteNode = 4, 
	FUNCTION_HOST_GetNodeModule = 5, 
	
	FUNCTION_HOST_GetWindowsSystem = 50
};

enum functionNumberJob
{
	FUNCTION_JOB_GetRunningJobLog = 1, 
	FUNCTION_JOB_GetTerminalJobLog = 2, 
	FUNCTION_JOB_GetJobLogRun = 3, 
	FUNCTION_JOB_GetEventLog = 4, 
	FUNCTION_JOB_GetEventLogRun = 5, 
	FUNCTION_JOB_DeleteLog = 7, 
	FUNCTION_JOB_CancelJob = 8, 
	FUNCTION_JOB_WriteAbioLog = 9, 
	FUNCTION_JOB_GetJobLogDescription = 10, 
	
	FUNCTION_JOB_CancelJobBackupServer = 50, 
	FUNCTION_JOB_CancelJobBackup = 51, 
	FUNCTION_JOB_CancelJobRestore = 52, 
	FUNCTION_JOB_CancelJobBackupset = 53, 
	FUNCTION_JOB_SendUnmountVolumeRequest = 54
};

enum functionNumberMedia
{
	FUNCTION_MEDIA_GetVolumePoolList = 1, 
	FUNCTION_MEDIA_GetVolumeList = 2, 
	FUNCTION_MEDIA_GetVolumePoolVolumeList = 3, 
	FUNCTION_MEDIA_GetStorageVolumeList = 4, 
	FUNCTION_MEDIA_AddVolumePool = 5, 
	FUNCTION_MEDIA_UpdateVolumePool = 6, 
	FUNCTION_MEDIA_DeleteVolumePool = 7, 
	FUNCTION_MEDIA_GetTapeLibraryInventoryVolumeList = 8, 
	FUNCTION_MEDIA_GetDiskInventoryVolumeList = 9, 
	FUNCTION_MEDIA_GetTapeLibraryVolumeHeader = 10, 
	FUNCTION_MEDIA_GetImportVolumeList = 11, 
	FUNCTION_MEDIA_AddTapeLibraryVolume = 12, 
	FUNCTION_MEDIA_AddStandAloneTapeDriveVolume = 13, 
	FUNCTION_MEDIA_ResetVolumeConfiguration = 14, 
	FUNCTION_MEDIA_UpdateVolume = 15, 
	FUNCTION_MEDIA_UpdateVolumeVolumePool = 16, 
	FUNCTION_MEDIA_MakeVolumeAccessReadWrite = 17, 
	FUNCTION_MEDIA_MakeVolumeAccessReadOnly = 18, 
	FUNCTION_MEDIA_MakeVolumePoolAccessNonPermanent = 19, 
	FUNCTION_MEDIA_MakeVolumePoolAccessPermanent = 20, 
	FUNCTION_MEDIA_DeleteVolume = 21, 
	FUNCTION_MEDIA_ImportVolume = 22, 
	FUNCTION_MEDIA_ExportVolume = 23, 
	FUNCTION_MEDIA_UnmountVolume = 24, 
	FUNCTION_MEDIA_ExpireVolume = 25, 
	
	FUNCTION_MEDIA_SortVolumeList = 50, 
	FUNCTION_MEDIA_ComparatorVolume = 51, 
	FUNCTION_MEDIA_GetLabel = 52, 
	FUNCTION_MEDIA_GetTapeVolumeCapacity = 53, 
	FUNCTION_MEDIA_GetStorageType = 54, 
	FUNCTION_MEDIA_GetTapeLibraryInventory = 55, 
	FUNCTION_MEDIA_MoveMedium = 56
};

enum functionNumberSchedule
{
	FUNCTION_SCHEDULE_GetPolicyList = 1, 
	FUNCTION_SCHEDULE_GetPolicy = 2, 
	FUNCTION_SCHEDULE_GetPolicyFileList = 3, 
	FUNCTION_SCHEDULE_GetScheduleList = 4, 
	FUNCTION_SCHEDULE_GetSchedule = 5, 
	FUNCTION_SCHEDULE_AddPolicy = 6, 
	FUNCTION_SCHEDULE_UpdatePolicy = 7, 
	FUNCTION_SCHEDULE_DeletePolicy = 8, 
	FUNCTION_SCHEDULE_AddSchedule = 9, 
	FUNCTION_SCHEDULE_UpdateSchedule = 10, 
	FUNCTION_SCHEDULE_DeleteSchedule = 11, 
	FUNCTION_SCHEDULE_RunSchedule = 12,
	FUNCTION_SCHEDULE_CopyPolicy = 13, 
	FUNCTION_SCHEDULE_CopySchedule = 14, 
	FUNCTION_SCHEDULE_ActivatePolicy = 15, 
	FUNCTION_SCHEDULE_DeactivatePolicy = 16, 
	FUNCTION_SCHEDULE_ActivateSchedule = 17, 
	FUNCTION_SCHEDULE_DeactivateSchedule = 18, 
};

enum functionNumberUser
{
	FUNCTION_USER_GetUserList = 1, 
	FUNCTION_USER_GetUnregisteredClientList = 2, 
	FUNCTION_USER_AddUser = 3, 
	FUNCTION_USER_ChangeUserPasswd = 4, 
	FUNCTION_USER_DeleteUser = 5,
	FUNCTION_USER_ChnageUserPermission = 6,
	FUNCTION_USER_ChangeUserInfo = 7,
	FUNCTION_USER_AddUserGroup = 8,
	FUNCTION_USER_DeleteUserGroup = 9,
	FUNCTION_USER_ChangeUserGroup = 10,
	FUNCTION_USER_RenameUserGroup = 11
};

enum functionNumberLogin
{
	FUNCTION_LOGIN_LogIn = 1, 
	FUNCTION_LOGIN_LogInUnix = 2, 
	FUNCTION_LOGIN_LogInWin = 3
};

enum functionNumberRetention
{
	FUNCTION_RETENTION_CheckRetention = 1, 
	FUNCTION_RETENTION_CheckBackupsetRetention = 2, 
	FUNCTION_RETENTION_CheckLogRetention = 3
};

enum functionNumberRunning
{
	FUNCTION_RUNNING_CheckRunningJob = 1, 

	// #issue 170 : Retry Backup
	FUNCTION_Retry_Backup = 41,
	
	FUNCTION_RUNNING_CancelJobBackupServer = 50, 
	FUNCTION_RUNNING_ClearJobBackup = 51, 
	FUNCTION_RUNNING_ClearJobRestore = 52, 
	FUNCTION_RUNNING_ClearJobBackupset = 53, 
	FUNCTION_RUNNING_RecoverBackupsetList = 54, 
	FUNCTION_RUNNING_RecoverSourceVolumeDB = 55, 
	FUNCTION_RUNNING_RecoverBackupsetCatalogDB = 56, 
	FUNCTION_RUNNING_RecoverLatestCatalogDB = 57, 
	FUNCTION_RUNNING_RecoverFileLatestCatalogDB = 58, 
	FUNCTION_RUNNING_RecoverSapLatestCatalogDB = 59, 
	FUNCTION_RUNNING_GetRestoreBackupsetCopyNumber = 60
};

enum functionNumberFrontpage
{
	FUNCTION_FRONTPAGE_GetFrontPageInfo = 1, 
	FUNCTION_FRONTPAGE_GetBackupJobs = 2, 
	FUNCTION_FRONTPAGE_GetCurrentAndFutureJobs = 3, 
	FUNCTION_FRONTPAGE_GetStorageInfo = 4, 
	FUNCTION_FRONTPAGE_GetDataBaseAgentNumber = 5, 
	FUNCTION_FRONTPAGE_GetMasterServerStatus = 6, 
	
	FUNCTION_FRONTPAGE_UpdateScheduleTime = 50, 
	FUNCTION_FRONTPAGE_GetLastDay = 51
};

enum functionNumberLicense
{
	FUNCTION_LICENSE_AddLicenseKey = 1,
	FUNCTION_LICENSE_RemoveLicenseKey = 2,
	FUNCTION_LICENSE_GenerateEvaluationKey = 3,
};

#endif
