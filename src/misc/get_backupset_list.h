#ifndef __MISC_GET_BACKUPSET_LIST_H__
#define __MISC_GET_BACKUPSET_LIST_H__

void PrintUsage(char * cmd);
void Exit();
int GetArgument(int argc, char ** argv);
int InitProcess(char * filename);
int SendCommand();
char * GetBackupData(int number);
char * GetBackupMethod(int number);
int RecvReply(va_sock_t sock, char * data, int size, int flag);

struct backupsetitem
	{
		char volume[VOLUME_NAME_LENGTH];
		char policy[POLICY_NAME_LENGTH];	
		char schedule[SCHEDULE_NAME_LENGTH];
		char client[NODE_NAME_LENGTH];
		char backupset[BACKUPSET_ID_LENGTH];	
		char btime[TIME_STAMP_LENGTH];
		char etime[TIME_STAMP_LENGTH];	
		char retention[NUMBER_STRING_LENGTH];
		char backupsetCopyNumber[DSIZ];
		char backupData[DSIZ];
		char fileNumber[DSIZ];
		char fileSize[DSIZ];
		char backupsetStatus[DSIZ];
		char backupMethod[DSIZ];
	};
char * GetBackupData(int number)
{

	switch(number)
	{
		case 11: return "File";
		case 12: return "Raw Device";
		case 13: return "Catalog DB";
		case 50: return "NDMP";
		case 250: return "VMWare";
		case 101: return "Oracle";
		case 102: return "MSSQL";
		case 103: return "Exchange";
		case 104: return "SAP";
		case 105: return "DB2";
		case 106: return "Informix";
		case 107: return "Sybase";					
		case 108: return "MySQL";
		case 109: return "Oracle Rman";
		case 110: return "Notes";
		case 111: return "UniSQL";
		case 112: return "Altibase";
		case 113: return "Exchange MailBox";
		case 114: return "Informix Onbar";
		case 115: return "Tibero";
		case 116: return "Oracle12C";
		case 117: return "SQLBackTrack";
        case 120: return "SharePoint";                
	}
	return NULL;
}
char * GetBackupMethod(int number)
{
	switch(number)
	{
		case 11: 
		case 51:
		case 53:
		case 201:
		case 218:
		case 221:
		case 261:
		case 271:
		case 301:
		case 311:
		case 321:
		case 331:
		case 341:
		case 351:
		case 361:
		case 371:			
			return "Full Backup";
		case 12:
		case 52:
		case 54:
		case 222:
		case 273:
		case 302:
		case 322:
		case 332:
		case 342:
		case 352:
		case 363:
			return "Incremental Backup";
		case 13:
		case 202:
		case 272:
		case 362:
			return "Differential Backup";
		case 14:
			return "Archive Backup";
		case 203:
			return "Log (Truncate) Backup";
		case 204:
			return "Log (Not Truncate) Backup";
		case 205:
			return "File Group Backup";
		case 206:
			return "File Backup";
		case 211:
			return "Storage Group Full Backup";
		case 212:
			return "Storage Group Incremental Backup";
		case 213:
			return "Storage Group Differential Backup";
		case 214:
			return "Storage Group Copy Backup";
		case 215:
			return "Store Full Backup";
		case 219:
			return "SIS Backup";
		case 231:
			return "Database Offline Full Backup";
		case 232:
			return "Database Offline Incremental Backup";
		case 233:
			return "Database Offline Incremental Delta Backup";
		case 234:
			return "Database Online Full Backup";
		case 235:
			return "Database Online Incremental Backup";
		case 236:
			return "Database Online Incremental Delta Backup";
		case 237:
			return "Tablespace Offline Full Backup";
		case 238:
			return "Tablespace Offline Incremental Backup";
		case 239:
			return "Tablespace Offline Incremental Delta Backup";
		case 240:
			return "Tablespace Online Full Backup";
		case 241:
			return "Tablespace Online Incremental Backup";
		case 242:
			return "Tablespace Online Incremental Delta Backup";
		case 243:
			return "Log Backup";
		case 251:
			return "Database Backup";
		case 252:
			return "Transaction Backup";
	}

	return NULL;
}

#endif