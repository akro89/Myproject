#include "require.h"
#include "abio.h"
#include "lib.h"
#include "vendor.h"

extern char ABIOMASTER_DATA_FOLDER[MAX_PATH_LENGTH];
extern char ABIOMASTER_CATALOG_DB_FOLDER[MAX_PATH_LENGTH];
extern char ABIOMASTER_VOLUME_DB_FOLDER[MAX_PATH_LENGTH];
extern char ABIOMASTER_FILE_LIST_FOLDER[MAX_PATH_LENGTH];

extern char masterIP[IP_LENGTH];
extern char masterPort[PORT_LENGTH];
extern char masterNodename[NODE_NAME_LENGTH];
extern char masterLogPort[PORT_LENGTH];

extern struct portRule * portRule;		// VIRBAK ABIO IP Table 목록
extern int portRuleNumber;				// VIRBAK ABIO IP Table 개수

int va_retry_tape_drive_error(struct ck * command, char * moduleName)
{	
	time_t current_t;

	struct ck newCommand;
	struct ckBackup newCommandData;

	struct ckBackup commandData;

	current_t = time(NULL);

	memset(&newCommand, 0, sizeof(struct ck));
	memset(&newCommandData, 0, sizeof(struct ckBackup));

	memcpy(&commandData, command->dataBlock, sizeof(struct ckBackup));


	newCommand.request = 0;
	newCommand.reply = 1;
	strcpy(newCommand.requestCommand, "RETRY_BACKUP_DIF_DRIVE");
	strcpy(newCommand.executeCommand, moduleName);
	strcpy(newCommand.sourceIP, masterIP);
	strcpy(newCommand.sourcePort, masterPort);
	strcpy(newCommand.sourceNodename, masterNodename);
	strcpy(newCommand.destinationIP, masterIP);
	strcpy(newCommand.destinationPort, masterPort);
	strcpy(newCommand.destinationNodename, masterNodename);

	newCommandData.version = CURRENT_VERSION_INTERNAL;
	strcpy(newCommandData.client.nodename, commandData.client.nodename);

	newCommandData.startTime = current_t;
	newCommandData.endTime = current_t;

	strcpy(newCommandData.backupset, commandData.backupset);
	strcpy(newCommandData.jobID, commandData.jobID);
	newCommandData.state = STATE_TEMP;


	newCommandData.writeDetailedLog = commandData.writeDetailedLog;
	strcpy(newCommandData.policy, commandData.policy);
	strcpy(newCommandData.schedule, commandData.schedule);

	newCommandData.backupData = commandData.backupData;
	newCommandData.jobMethod = commandData.jobMethod;
	newCommandData.jobType = BACKUP_JOB;
	
	newCommandData.retention[0] = commandData.retention[0];
	newCommandData.retention[1] = commandData.retention[1];
	
	newCommandData.multiPlexing = commandData.multiPlexing;
	newCommandData.multiSession = commandData.multiSession;
	newCommandData.saveIncompletedBackup = commandData.saveIncompletedBackup;
	newCommandData.checkPoint = commandData.checkPoint;
	

	newCommandData.compress = commandData.compress;
	newCommandData.checkDeletedFile = commandData.checkDeletedFile;
	
	strcpy(newCommandData.pool, commandData.pool);

	newCommandData.snapShot = commandData.snapShot;
	
	strcpy(newCommandData.filelist, commandData.filelist);
	
	newCommandData.mountStatus.driveAddress = commandData.mountStatus.driveAddress;

	newCommandData.retryCount = commandData.retryCount;
	newCommandData.retryInterval = commandData.retryInterval;
	newCommandData.encryption = commandData.encryption;

	//#issue414
	newCommandData.MaxOutboundBandwidth = commandData.MaxOutboundBandwidth;
	newCommandData.LimitCPU = commandData.LimitCPU;
	newCommandData.LimitDiskIO = commandData.LimitDiskIO;

	
	// client setting
	if (SetNodeInfo(newCommandData.client.nodename, &newCommandData.client, &newCommandData.ndmp) == 0)
	{
		// client가 살아있는지 확인한다.
		if (CheckNodeAlive(newCommandData.client.ip, newCommandData.client.port) < 0)
		{
			return ERROR_NETWORK_CLIENT_DOWN;
		}
	}
	else
	{
		return ERROR_CONFIG_OPEN_NODE_LIST_FILE;
	}
	
	
	// 지정한 storage pool이 system pool인지 확인한다.
	if (!strcmp(newCommandData.pool, ABIOMASTER_CATALOG_POOL_NAME))
	{
		return ERROR_VOLUME_POOL_SYSTEM_POOL;
	}

	memcpy(newCommand.dataBlock, &newCommandData, sizeof(struct ckBackup));
	memcpy(command, &newCommand, sizeof(struct ck));
	memcpy(&commandData, command->dataBlock,sizeof(struct ckBackup));

	return 0;
}

int va_retry_backup(struct ck * command, char * moduleName)
{
	time_t current_t;

	struct ck newCommand;
	struct ckBackup newCommandData;

	struct ckBackup commandData;

	current_t = time(NULL);

	memset(&newCommand, 0, sizeof(struct ck));
	memset(&newCommandData, 0, sizeof(struct ckBackup));

	memcpy(&commandData, command->dataBlock, sizeof(struct ckBackup));

	newCommand.request = 1;
	newCommand.reply = 0;
	strcpy(newCommand.requestCommand, MODULE_NAME_MASTER_BACKUP);
	strcpy(newCommand.executeCommand, moduleName);
	strcpy(newCommand.sourceIP, command->sourceIP);
	strcpy(newCommand.sourcePort, command->sourcePort);
	strcpy(newCommand.sourceNodename, command->sourceNodename);
	strcpy(newCommand.destinationIP, command->destinationIP);
	strcpy(newCommand.destinationPort, command->destinationPort);
	strcpy(newCommand.destinationNodename, command->destinationNodename);
	
	
	newCommandData.version = CURRENT_VERSION_INTERNAL;
	strcpy(newCommandData.client.nodename, commandData.client.nodename);

	newCommandData.startTime = current_t;
	newCommandData.endTime = current_t;

	strcpy(newCommandData.backupset, commandData.backupset);
	strcpy(newCommandData.jobID, commandData.jobID);
	newCommandData.state = STATE_TEMP;


	newCommandData.writeDetailedLog = commandData.writeDetailedLog;
	strcpy(newCommandData.policy, commandData.policy);
	strcpy(newCommandData.schedule, commandData.schedule);

	newCommandData.backupData = commandData.backupData;
	newCommandData.jobMethod = commandData.jobMethod;
	newCommandData.jobType = BACKUP_JOB;
	
	newCommandData.retention[0] = commandData.retention[0];
	newCommandData.retention[1] = commandData.retention[1];
	
	newCommandData.multiPlexing = commandData.multiPlexing;
	newCommandData.multiSession = commandData.multiSession;
	newCommandData.saveIncompletedBackup = commandData.saveIncompletedBackup;
	newCommandData.checkPoint = commandData.checkPoint;
	newCommandData.multiBackup = commandData.multiBackup;
	

	newCommandData.compress = commandData.compress;
	newCommandData.checkDeletedFile = commandData.checkDeletedFile;
	
	strcpy(newCommandData.pool, commandData.pool);

	newCommandData.snapShot = commandData.snapShot;
	
	strcpy(newCommandData.filelist, commandData.filelist);
	
	newCommandData.mountStatus.driveAddress = commandData.mountStatus.driveAddress;
	strcpy(newCommandData.mountStatus.server, commandData.mountStatus.server);

	if(ABIO_BACKUP_DATA_DB < commandData.backupData && commandData.backupData < ABIO_BACKUP_DATA_DB_MAX)
	{
		strcpy(newCommandData.db.client, commandData.db.client);
		strcpy(newCommandData.db.db, commandData.db.db);
	
		memcpy(newCommandData.db.dbData, commandData.db.dbData, sizeof(commandData.db.dbData));
	}

	newCommandData.retryCount = commandData.retryCount;
	newCommandData.retryInterval = commandData.retryInterval;
	newCommandData.encryption = commandData.encryption;

	//#issue414
	newCommandData.MaxOutboundBandwidth = commandData.MaxOutboundBandwidth;
	newCommandData.LimitCPU = commandData.LimitCPU;
	newCommandData.LimitDiskIO = commandData.LimitDiskIO;
	
	// client setting
	if (SetNodeInfo(newCommandData.client.nodename, &newCommandData.client, &newCommandData.ndmp) == 0)
	{
		// client가 살아있는지 확인한다.
		if (CheckNodeAlive(newCommandData.client.ip, newCommandData.client.port) < 0)
		{
			return ERROR_NETWORK_CLIENT_DOWN;
		}
	}
	else
	{
		return ERROR_CONFIG_OPEN_NODE_LIST_FILE;
	}
	
	
	// 지정한 storage pool이 system pool인지 확인한다.
	if (!strcmp(newCommandData.pool, ABIOMASTER_CATALOG_POOL_NAME))
	{
		return ERROR_VOLUME_POOL_SYSTEM_POOL;
	}

	memcpy(newCommand.dataBlock, &newCommandData, sizeof(struct ckBackup));
	memcpy(command, &newCommand, sizeof(struct ck));
	memcpy(&commandData, command->dataBlock,sizeof(struct ckBackup));

	return 0;
}

int SetNodeInfo(char * nodename, struct ckClient * ckClient, struct ndmpNode * ndmp)
{
	va_fd_t fdNode;
	struct abioNode node;
	
	
	
	// client setting
	if ((fdNode = va_open(ABIOMASTER_DATA_FOLDER, ABIOMASTER_NODE_LIST, O_RDONLY, 1, 1)) != (va_fd_t)-1)
	{
		while (va_read(fdNode, &node, sizeof(struct abioNode), DATA_TYPE_NODE) > 0)
		{
			if (!strcmp(node.name, nodename))
			{
				ckClient->platformType = node.platformType;
				ckClient->nodeType = node.nodeType;
				strcpy(ckClient->nodename, node.name);
				strcpy(ckClient->ip, node.ip);
				strcpy(ckClient->port, node.port);
				strcpy(ckClient->masterIP, masterIP);
				strcpy(ckClient->masterPort, masterPort);
				strcpy(ckClient->masterLogPort, masterLogPort);
				
				if (node.nodeType == NODE_NDMP_CLIENT || node.nodeType == NODE_VMWARE_CLIENT)
				{
					strcpy(ndmp->account, node.ndmp.account);
					strcpy(ndmp->passwd, node.ndmp.passwd);
				}
				
				break;
			}
		}
		
		va_close(fdNode);
		
		if (strcmp(node.name, nodename) != 0)
		{
			return -1;
		}
	}
	else
	{
		return -1;
	}
	
	
	return 0;
}

int CheckNodeAlive(char * ip, char * port)
{
	va_sock_t sock;
	
	
	
	if ((sock = va_connect(ip, port, 0)) != -1)
	{
		va_close_socket(sock, ABIO_SOCKET_CLOSE_CLIENT);
	}
	else
	{
		return -1;
	}
	
	
	return 0;
}