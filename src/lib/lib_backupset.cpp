#include "require.h"
#include "abio.h"
#include "lib.h"
#include "vendor.h"


extern char ABIOMASTER_DATA_FOLDER[MAX_PATH_LENGTH];
extern char ABIOMASTER_CATALOG_DB_FOLDER[MAX_PATH_LENGTH];
extern char ABIOMASTER_VOLUME_DB_FOLDER[MAX_PATH_LENGTH];
extern char ABIOMASTER_FILE_LIST_FOLDER[MAX_PATH_LENGTH];
extern char ABIOMASTER_LOG_FOLDER[MAX_PATH_LENGTH];

extern char masterIP[IP_LENGTH];
extern char masterPort[PORT_LENGTH];
extern char masterNodename[NODE_NAME_LENGTH];
extern char masterLogPort[PORT_LENGTH];

extern struct portRule * portRule;		// VIRBAK ABIO IP Table ���
extern int portRuleNumber;				// VIRBAK ABIO IP Table ����


static void va_expire_latest_catalog_db(char * client, enum backupDataType backupData, char * backupset);
static void va_update_catalog_db_backupset_copy(char * backupset, int newRestoreBackupsetCopyNumber, char * media);
static void va_update_latest_catalog_db_backupset_copy(char * client, enum backupDataType backupData, char * backupset, int newRestoreBackupsetCopyNumber, char * media);

static void va_get_disk_volume_repository(char * storageName, char * nodename, char * repository, struct networkDriveConfig * networkDrive);
static int va_remove_disk_volume(char * nodename, char * repository, char * volumeName, struct networkDriveConfig * networkDrive);
static int va_volume_list_comparator(const void * a1, const void * a2);

static int CheckCoordinationCatalogDB(struct catalogDB * catalogDB);

void va_expire_parents_backupset(char * media, char * backupset, int copyNumber, char * jobID)
{
	va_fd_t fdVolume;
	struct volume volume;

	va_fd_t fdVolumeDB;
	struct volumeDB volumeDB;

	char ** expiredBackupsetList;
	int expiredBackupsetListNumber;

	expiredBackupsetList = NULL;
	expiredBackupsetListNumber = 0;

	int idx;

	if ((fdVolume = va_open(ABIOMASTER_DATA_FOLDER, ABIOMASTER_VOLUME_READ_LIST, O_RDONLY, 0, 0)) != (va_fd_t)-1)
	{
		expiredBackupsetList = (char **)malloc(sizeof(char *) * DEFAULT_MEMORY_FRAGMENT);
		memset(expiredBackupsetList , 0, sizeof(char *) * DEFAULT_MEMORY_FRAGMENT);
		
		while (va_read(fdVolume, &volume, sizeof(struct volume), DATA_TYPE_VOLUME) > 0)
		{
			if ((fdVolumeDB = va_open(ABIOMASTER_VOLUME_DB_FOLDER, volume.name, O_RDONLY, 1, 0)) != (va_fd_t)-1)
			{
				while (va_read(fdVolumeDB, &volumeDB, sizeof(struct volumeDB), DATA_TYPE_VOLUME_DB) > 0)
				{	
					if(!strcmp(volumeDB.jobBackupset,backupset) && volumeDB.backupsetCopyNumber == copyNumber)
					{
						for(idx = 0 ; idx < expiredBackupsetListNumber ; idx++)
						{
							if(!strcmp(expiredBackupsetList[idx],volumeDB.backupset))
							{
								break;
							}
						}

						if (expiredBackupsetListNumber % DEFAULT_MEMORY_FRAGMENT == 0)
						{
							expiredBackupsetList = (char **)realloc(expiredBackupsetList, sizeof(char *) * (expiredBackupsetListNumber + DEFAULT_MEMORY_FRAGMENT));
							memset(expiredBackupsetList + expiredBackupsetListNumber, 0, sizeof(char *) * DEFAULT_MEMORY_FRAGMENT);
						}

						if( expiredBackupsetListNumber <= idx)
						{
							expiredBackupsetList[expiredBackupsetListNumber] = (char *)malloc(sizeof(char) * BACKUPSET_ID_LENGTH);
							memset(expiredBackupsetList[expiredBackupsetListNumber], 0, sizeof(char) * BACKUPSET_ID_LENGTH);
							strcpy(expiredBackupsetList[expiredBackupsetListNumber], volumeDB.backupset);							

							// ���� backup job���� ������� backupset�� expire��Ŵ
							va_expire_backupset(NULL, expiredBackupsetList[expiredBackupsetListNumber], 1, NULL);
							va_remove(ABIOMASTER_CATALOG_DB_FOLDER, expiredBackupsetList[expiredBackupsetListNumber]);	

							expiredBackupsetListNumber++;
						}
					}
				}
			}
			
		}
	}


}
void va_expire_backupset(char * media, char * backupset, int copyNumber, char * jobID)
{
	char mediaName[VOLUME_NAME_LENGTH];
	char backupsetJobID[JOB_ID_LENGTH];
	
	va_fd_t fdVolume;
	struct volume volume;
	
	va_fd_t fdVolumeDB;
	struct volumeDB volumeDB;
	
	va_fd_t fdVolumeDBPre;
	struct volumeDB volumeDBPre;
	
	va_fd_t fdVolumeDBNext;
	struct volumeDB volumeDBNext;
	
	struct volumeDB * expiredVolumeDB;
	int expiredVolumeDBNumber;
	
	int expireBackupset;
	
	va_fd_t fdBackupsetList;
	struct backupsetList backupsetList;
	char backupsetListFile[MAX_PATH_LENGTH];
	
	struct backupsetList * newBackupsetList;
	int newBackupsetListNumber;
	
	int isValidBackupsetCopy;
	int newRestoreBackupsetCopyNumber;
	char filelist[MAX_PATH_LENGTH];
	
	struct ckBackup commandDataBackupset;
	struct ck commandBackupset;
	
	va_fd_t fdNode;
	struct abioNode node;
	
	va_fd_t fdDB;

	va_sock_t clientSock;
	va_sock_t masterSock;
	va_sock_t commandSock;
	
	struct expiredClient
	{
		char nodename[NODE_NAME_LENGTH];
		
		char ** backupset;
		int backupsetNumber;
	};
	
	struct expiredClient * expiredClientList;
	int expiredClientListNumber;
	
	struct expiredOracleRman
	{
		char nodename[NODE_NAME_LENGTH];
		struct oracleRmanDB oracleRman;
		
		char ** backupset;
		int backupsetNumber;
	};
	
	struct expiredOracleRman * expiredOracleRmanList;
	int expiredOracleRmanListNumber;
	struct dbBackupInfo dbInfo;
	struct oracleRmanDB oracleRman;

	time_t current_t;
	
	int i;
	int j;
	
	
	
	// initialize variables
	memset(mediaName, 0, sizeof(mediaName));
	
	expiredVolumeDB = NULL;
	expiredVolumeDBNumber = 0;
	
	expiredClientList = NULL;
	expiredClientListNumber = 0;
	
	expiredOracleRmanList = NULL;
	expiredOracleRmanListNumber = 0;
	
	current_t = time(NULL);
	
	
	// check exeception
	if ((media == NULL || media[0] == '\0') && (backupset == NULL || backupset[0] == '\0'))
	{
		return;
	}
	
	
	if (media != NULL)
	{
		strcpy(mediaName, media);
	}
	// expire�� backupset�� ����ִ� media�� �������� �ʾ��� ���, backupset�� ��� media�� ����ִ��� ã�´�.
	else
	{
		// volume list���� expire�� backupset�� ã�´�.
		if ((fdVolume = va_open(ABIOMASTER_DATA_FOLDER, ABIOMASTER_VOLUME_READ_LIST, O_RDONLY, 0, 0)) != (va_fd_t)-1)
		{
			while (va_read(fdVolume, &volume, sizeof(struct volume), DATA_TYPE_VOLUME) > 0)
			{
				if ((fdVolumeDB = va_open(ABIOMASTER_VOLUME_DB_FOLDER, volume.name, O_RDONLY, 1, 0)) != (va_fd_t)-1)
				{
					while (va_read(fdVolumeDB, &volumeDB, sizeof(struct volumeDB), DATA_TYPE_VOLUME_DB) > 0)
					{
						memset(backupsetJobID, 0, sizeof(backupsetJobID));
						if (jobID == NULL || jobID[0] == '\0')
						{
							strcpy(backupsetJobID, volumeDB.jobID);
						}
						else
						{
							strcpy(backupsetJobID, jobID);
						}
						
						if (!strcmp(volumeDB.backupset, backupset) && volumeDB.backupsetCopyNumber == copyNumber && !strcmp(volumeDB.jobID, backupsetJobID))
						{
							strcpy(mediaName, volume.name);
							
							break;
						}
					}
					
					va_close(fdVolumeDB);
				}
				
				if (mediaName[0] != '\0')
				{
					break;
				}
			}
			
			va_close(fdVolume);
		}
		
		// expire�� backupset�� ����ִ� media�� ã�� ���� ��� expire���� �ʴ´�.
		if (mediaName[0] == '\0')
		{
			return;
		}
	}
	
	
	// backupset�� volume db�� expire�Ѵ�.
	if ((fdVolumeDB = va_open(ABIOMASTER_VOLUME_DB_FOLDER, mediaName, O_RDWR, 1, 0)) != (va_fd_t)-1)
	{
		while (va_read(fdVolumeDB, &volumeDB, sizeof(struct volumeDB), DATA_TYPE_VOLUME_DB) > 0)
		{
			// backupset�� expire���� �����Ѵ�.
			expireBackupset = 0;
			
			// expire�� backupset�� �������� �ʾ�����, media�� ����ִ� ��� backupset�� expire�Ѵ�.
			if (backupset == NULL || backupset[0] == '\0')
			{
				// �̹� expire�� backupset���� Ȯ���ؼ�, expire�Ǿ����� �ٽ� expire���� �ʴ´�.
				if (volumeDB.backupsetStatus != ABIO_BACKUPSET_STATUS_EXPIRED)
				{
					expireBackupset = 1;
				}
			}
			// expire�� backupset�� ������ ���, ������ backupset�� �´��� Ȯ���Ѵ�.
			else
			{
				memset(backupsetJobID, 0, sizeof(backupsetJobID));
				if (jobID == NULL || jobID[0] == '\0')
				{
					strcpy(backupsetJobID, volumeDB.jobID);
				}
				else
				{
					strcpy(backupsetJobID, jobID);
				}
				
				if (!strcmp(volumeDB.backupset, backupset) && volumeDB.backupsetCopyNumber == copyNumber && !strcmp(volumeDB.jobID, backupsetJobID))
				{
					expireBackupset = 1;
					
					// �̹� expire�� backupset���� Ȯ���ؼ�, expire�Ǿ����� �ٽ� expire���� �ʴ´�.
					if (volumeDB.backupsetStatus == ABIO_BACKUPSET_STATUS_EXPIRED)
					{
						break;
					}
				}
			}
			
			// backupset�� expire�Ѵ�.
			if (expireBackupset == 1)
			{
				// expire�� backupset ����� �߰��Ѵ�.
				if (expiredVolumeDBNumber % DEFAULT_MEMORY_FRAGMENT == 0)
				{
					expiredVolumeDB = (struct volumeDB *)realloc(expiredVolumeDB, sizeof(struct volumeDB) * (expiredVolumeDBNumber + DEFAULT_MEMORY_FRAGMENT));
					memset(expiredVolumeDB + expiredVolumeDBNumber, 0, sizeof(struct volumeDB) * DEFAULT_MEMORY_FRAGMENT);
				}
				memcpy(expiredVolumeDB + expiredVolumeDBNumber, &volumeDB, sizeof(struct volumeDB));
				expiredVolumeDBNumber++;
				
				
				// backupset�� expire�Ѵ�.
				volumeDB.backupsetStatus = ABIO_BACKUPSET_STATUS_EXPIRED;
				volumeDB.expirationTime = current_t;
				
				va_lseek(fdVolumeDB, -(va_offset_t)sizeof(struct volumeDB), SEEK_CUR);
				va_write(fdVolumeDB, &volumeDB, sizeof(struct volumeDB), DATA_TYPE_VOLUME_DB);
				
				
				// previous media�� split backupset�� expire�Ѵ�.
				memcpy(&volumeDBPre, &volumeDB, sizeof(struct volumeDB));
				while (volumeDBPre.previousVolume[0] != '\0')
				{
					if ((fdVolumeDBPre = va_open(ABIOMASTER_VOLUME_DB_FOLDER, volumeDBPre.previousVolume, O_RDWR, 1, 1)) != (va_fd_t)-1)
					{
						while (va_read(fdVolumeDBPre, &volumeDBPre, sizeof(struct volumeDB), DATA_TYPE_VOLUME_DB) > 0)
						{
							if (!strcmp(volumeDBPre.backupset, volumeDB.backupset) && volumeDBPre.backupsetCopyNumber == volumeDB.backupsetCopyNumber && 
								!strcmp(volumeDBPre.jobID, volumeDB.jobID) && volumeDBPre.backupsetStatus != ABIO_BACKUPSET_STATUS_EXPIRED)
							{
								volumeDBPre.backupsetStatus = ABIO_BACKUPSET_STATUS_EXPIRED;
								volumeDBPre.expirationTime = volumeDB.expirationTime;
								
								va_lseek(fdVolumeDBPre, -(va_offset_t)sizeof(struct volumeDB), SEEK_CUR);
								va_write(fdVolumeDBPre, &volumeDBPre, sizeof(struct volumeDB), DATA_TYPE_VOLUME_DB);
								
								break;
							}
						}
						
						va_close(fdVolumeDBPre);
					}
					else
					{
						break;
					}
				}
				
				// next media�� split backupset�� expire�Ѵ�.
				memcpy(&volumeDBNext, &volumeDB, sizeof(struct volumeDB));
				while (volumeDBNext.nextVolume[0] != '\0')
				{
					if ((fdVolumeDBNext = va_open(ABIOMASTER_VOLUME_DB_FOLDER, volumeDBNext.nextVolume, O_RDWR, 1, 1)) != (va_fd_t)-1)
					{
						while (va_read(fdVolumeDBNext, &volumeDBNext, sizeof(struct volumeDB), DATA_TYPE_VOLUME_DB) > 0)
						{
							if (!strcmp(volumeDBNext.backupset, volumeDB.backupset) && volumeDBNext.backupsetCopyNumber == volumeDB.backupsetCopyNumber && 
								!strcmp(volumeDBNext.jobID, volumeDB.jobID) && volumeDBNext.backupsetStatus != ABIO_BACKUPSET_STATUS_EXPIRED)
							{
								volumeDBNext.backupsetStatus = ABIO_BACKUPSET_STATUS_EXPIRED;
								volumeDBNext.expirationTime = volumeDB.expirationTime;
								
								va_lseek(fdVolumeDBNext, -(va_offset_t)sizeof(struct volumeDB), SEEK_CUR);
								va_write(fdVolumeDBNext, &volumeDBNext, sizeof(struct volumeDB), DATA_TYPE_VOLUME_DB);
								
								break;
							}
						}
						
						va_close(fdVolumeDBNext);
					}
					else
					{
						break;
					}
				}
			}
		}
		
		va_close(fdVolumeDB);
	}
	
	
	
	
	// client�� the latest catalog db�� expire�Ѵ�.
	// client�� the latest catalog db�� backupset�� primary copy�� expire�Ǿ��� ���� expire�Ѵ�.
	// backupset�� copy version�� �����ִٰ� �ص�, the latest catalog db�� primary copy�� �����ϴ� ���� 
	// ������ ������ �� �� �ֱ� ������(�Ϲ������� ����� backupset copy�� �һ������ ����ų� 
	// secondary storage�� ����� ������ ��������� �� backupset copy�� ����ִ� media�� ���� ���� �ִ�)
	// backupset�� primary copy�� expire�Ǿ��ٸ� client�� the latest catalog db�� expire�Ѵ�.
	
	// client�� ������ backupset ����� client���� �����.
	for (i = 0; i < expiredVolumeDBNumber; i++)
	{
		// primary copy�� expire�Ѵ�.
		// client�� the latest catalog db�� �����ϴ� ���� regular file ����� �ش��ϹǷ�, 
		// regular file�� ����� backupset�� expire�� ���� client�� the latest catalog db�� expire�϶�� ��û�� ������.
		if (expiredVolumeDB[i].backupsetCopyNumber == 1 && expiredVolumeDB[i].backupData == ABIO_BACKUP_DATA_REGULAR_FILE)
		{
			// expire�� backupset�� client�� expired client list���� ã�´�.
			for (j = 0; j < expiredClientListNumber; j++)
			{
				if (!strcmp(expiredClientList[j].nodename, expiredVolumeDB[i].client))
				{
					break;
				}
			}
			
			// expire�� backupset�� client�� expired client list���� ã�� ���� ��� client�� �߰��Ѵ�.
			if (j == expiredClientListNumber)
			{
				// expired client list�� client�� �߰��Ѵ�.
				if (j % DEFAULT_MEMORY_FRAGMENT == 0)
				{
					expiredClientList = (struct expiredClient *)realloc(expiredClientList, sizeof(struct expiredClient) * (j + DEFAULT_MEMORY_FRAGMENT));
					memset(expiredClientList + j, 0, sizeof(struct expiredClient) * DEFAULT_MEMORY_FRAGMENT);
				}
				
				// client�� ����Ѵ�.
				strcpy(expiredClientList[j].nodename, expiredVolumeDB[i].client);
				
				// expired client list ������ ������Ų��.
				expiredClientListNumber++;
			}
			
			// expire�� backupset�� �߰��Ѵ�.
			if (expiredClientList[j].backupsetNumber % DEFAULT_MEMORY_FRAGMENT == 0)
			{
				expiredClientList[j].backupset = (char **)realloc(expiredClientList[j].backupset, sizeof(char *) * (expiredClientList[j].backupsetNumber + DEFAULT_MEMORY_FRAGMENT));
				memset(expiredClientList[j].backupset + expiredClientList[j].backupsetNumber, 0, sizeof(char *) * DEFAULT_MEMORY_FRAGMENT);
			}
			expiredClientList[j].backupset[expiredClientList[j].backupsetNumber] = (char *)malloc(sizeof(char) * BACKUPSET_ID_LENGTH);
			memset(expiredClientList[j].backupset[expiredClientList[j].backupsetNumber], 0, sizeof(char) * BACKUPSET_ID_LENGTH);
			strcpy(expiredClientList[j].backupset[expiredClientList[j].backupsetNumber], expiredVolumeDB[i].backupset);
			
			// expire�� backupset ������ ������Ų��.
			expiredClientList[j].backupsetNumber++;
		}
	}
	
	// client���� expire�� backupset ����� client�� �����Ѵ�.
	for (i = 0; i < expiredClientListNumber; i++)
	{
		memset(&commandBackupset, 0, sizeof(struct ck));
		memset(&commandDataBackupset, 0, sizeof(struct ckBackup));
		
		// client setting
		if ((fdNode = va_open(ABIOMASTER_DATA_FOLDER, ABIOMASTER_NODE_LIST, O_RDONLY, 1, 1)) != (va_fd_t)-1)
		{
			while (va_read(fdNode, &node, sizeof(struct abioNode), DATA_TYPE_NODE) > 0)
			{
				if (!strcmp(node.name, expiredClientList[i].nodename))
				{
					commandDataBackupset.client.platformType = node.platformType;
					commandDataBackupset.client.nodeType = node.nodeType;
					strcpy(commandDataBackupset.client.nodename, node.name);
					strcpy(commandDataBackupset.client.ip, node.ip);
					strcpy(commandDataBackupset.client.port, node.port);
					strcpy(commandDataBackupset.client.masterIP, masterIP);
					strcpy(commandDataBackupset.client.masterPort, masterPort);
					strcpy(commandDataBackupset.client.masterLogPort, masterLogPort);
					
					break;
				}
			}
			
			va_close(fdNode);
		}
		
		if (commandDataBackupset.client.nodeType == NODE_UNKNOWN)
		{
			continue;
		}
		
		// client�� ����ִ��� Ȯ���Ѵ�.
		if ((clientSock = va_connect(commandDataBackupset.client.ip, commandDataBackupset.client.port, 0)) != -1)
		{
			va_close_socket(clientSock, ABIO_SOCKET_CLOSE_CLIENT);
		}
		else
		{
			continue;
		}
		
		
		commandBackupset.request = 1;
		commandBackupset.reply = 0;
		strcpy(commandBackupset.requestCommand, MODULE_NAME_CLIENT_HTTPD);
		strcpy(commandBackupset.subCommand, "<EXPIRE_BACKUPSET>");
		strcpy(commandBackupset.executeCommand, MODULE_NAME_MASTER_HTTPD);
		strcpy(commandBackupset.sourceIP, masterIP);
		strcpy(commandBackupset.sourcePort, masterPort);
		strcpy(commandBackupset.sourceNodename, masterNodename);
		strcpy(commandBackupset.destinationIP, commandDataBackupset.client.ip);
		strcpy(commandBackupset.destinationPort, commandDataBackupset.client.port);
		strcpy(commandBackupset.destinationNodename, commandDataBackupset.client.nodename);
		
		
		if ((masterSock = va_make_server_socket_iptable(commandBackupset.destinationIP, commandBackupset.sourceIP, portRule, portRuleNumber, commandDataBackupset.catalogPort)) != -1)
		{
			// send commandBackupset to client
			if ((commandSock = va_connect("127.0.0.1", masterPort, 1)) != -1)
			{
				memcpy(commandBackupset.dataBlock, &commandDataBackupset, sizeof(struct ckBackup));
				
				va_send(commandSock, &commandBackupset, sizeof(struct ck), 0, DATA_TYPE_CK);
				va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
				
				
				// wait connection from client
				if ((clientSock = va_wait_connection(masterSock, TIME_OUT_JOB_CONNECTION, NULL)) >= 0)
				{
					// client�� expire�� backupset ����� �����Ѵ�.
					for (j = 0; j < expiredClientList[i].backupsetNumber; j++)
					{
						va_send(clientSock, expiredClientList[i].backupset[j], BACKUPSET_ID_LENGTH, 0, DATA_TYPE_NOT_CHANGE);
					}
					
					va_close_socket(clientSock, ABIO_SOCKET_CLOSE_CLIENT);
				}
			}
			
			va_close_socket(masterSock, 0);
		}
	}
	
	
	// client�� ������ oracle rman backupset ����� client���� �����.
	for (i = 0; i < expiredVolumeDBNumber; i++)
	{
		// primary copy�� expire�Ѵ�.
		// oracle rman �� �����ϴ� backupset list �� ABIO �� expire ������ �ݿ� �Ǿ�� �Ѵ�.
		if (expiredVolumeDB[i].backupsetCopyNumber == 1 && expiredVolumeDB[i].backupData == ABIO_BACKUP_DATA_ORACLE_RMAN)
		{
			// expire�� backupset�� client�� expired client list���� ã�´�.
			for (j = 0; j < expiredOracleRmanListNumber; j++)
			{
				if (!strcmp(expiredOracleRmanList[j].nodename, expiredVolumeDB[i].client))
				{
					break;
				}
			}
			
			// expire�� backupset�� client�� expired client list���� ã�� ���� ��� client�� �߰��Ѵ�.
			if (j == expiredOracleRmanListNumber)
			{
				// expire�Ҷ� backupset�� client���� ����ϴ� db info �� �ʿ��ϴ�.
				// ���� ���ٸ� expire list ���� �����Ѵ�.
				if ((fdDB = va_open(ABIOMASTER_DATA_FOLDER, ABIOMASTER_DB_BACKUP_LIST, O_RDONLY, 1, 0)) != (va_fd_t)-1)
				{
					memset(&oracleRman, 0, sizeof(struct oracleRmanDB));

					while (va_read(fdDB, &dbInfo, sizeof(struct dbBackupInfo), DATA_TYPE_DB_BACKUP_INFO) > 0)
					{
						if (!strcmp(expiredVolumeDB[i].client, dbInfo.client) && !strcmp(dbInfo.db, ABIO_BACKUP_DATA_TYPE_INTERNAL_ORACLE_RMAN))
						{
							memcpy(&oracleRman, dbInfo.dbData, sizeof(struct oracleRmanDB));
							
							break;
						}
					}
					
					va_close(fdDB);
					
					// db info �� ã�´�. ã�� ���� ��� �����Ѵ�.
					if (oracleRman.home[0] != '\0')
					{
						// expired client list�� client�� �߰��Ѵ�.
						if (j % DEFAULT_MEMORY_FRAGMENT == 0)
						{
							expiredOracleRmanList = (struct expiredOracleRman *)realloc(expiredOracleRmanList, sizeof(struct expiredOracleRman) * (j + DEFAULT_MEMORY_FRAGMENT));
							memset(expiredOracleRmanList + j, 0, sizeof(struct expiredOracleRman) * DEFAULT_MEMORY_FRAGMENT);
						}
						
						// client�� ����Ѵ�.
						strcpy(expiredOracleRmanList[j].nodename, expiredVolumeDB[i].client);
						
						// client�� DB Info �� ����Ѵ�.
						memcpy(&expiredOracleRmanList[j].oracleRman, &oracleRman, sizeof(struct oracleRmanDB));
						
						// expired client list ������ ������Ų��.
						expiredOracleRmanListNumber++;
					}
					else
					{
						continue;
					}
				}
				else
				{
					WriteDebugData(ABIOMASTER_DATA_FOLDER, "httpd_start_expired.log","db.dat open error");
					continue;
				}
			}


			// expire�� backupset�� �߰��Ѵ�.
			if (expiredOracleRmanList[j].backupsetNumber % DEFAULT_MEMORY_FRAGMENT == 0)
			{
				expiredOracleRmanList[j].backupset = (char **)realloc(expiredOracleRmanList[j].backupset, sizeof(char *) * (expiredOracleRmanList[j].backupsetNumber + DEFAULT_MEMORY_FRAGMENT));
				memset(expiredOracleRmanList[j].backupset + expiredOracleRmanList[j].backupsetNumber, 0, sizeof(char *) * DEFAULT_MEMORY_FRAGMENT);
			}
			expiredOracleRmanList[j].backupset[expiredOracleRmanList[j].backupsetNumber] = (char *)malloc(sizeof(char) * BACKUPSET_ID_LENGTH);
			memset(expiredOracleRmanList[j].backupset[expiredOracleRmanList[j].backupsetNumber], 0, sizeof(char) * BACKUPSET_ID_LENGTH);
			strcpy(expiredOracleRmanList[j].backupset[expiredOracleRmanList[j].backupsetNumber], expiredVolumeDB[i].backupset);

			// expire�� backupset ������ ������Ų��.
			expiredOracleRmanList[j].backupsetNumber++;
		}
	}
	
	// client���� expire�� backupset ����� client�� �����Ѵ�.
	for (i = 0; i < expiredOracleRmanListNumber; i++)
	{
		memset(&commandBackupset, 0, sizeof(struct ck));
		memset(&commandDataBackupset, 0, sizeof(struct ckBackup));
		
		// client setting
		if ((fdNode = va_open(ABIOMASTER_DATA_FOLDER, ABIOMASTER_NODE_LIST, O_RDONLY, 1, 1)) != (va_fd_t)-1)
		{
			while (va_read(fdNode, &node, sizeof(struct abioNode), DATA_TYPE_NODE) > 0)
			{
				if (!strcmp(node.name, expiredOracleRmanList[i].nodename))
				{
					commandDataBackupset.client.platformType = node.platformType;
					commandDataBackupset.client.nodeType = node.nodeType;
					strcpy(commandDataBackupset.client.nodename, node.name);
					strcpy(commandDataBackupset.client.ip, node.ip);
					strcpy(commandDataBackupset.client.port, node.port);
					strcpy(commandDataBackupset.client.masterIP, masterIP);
					strcpy(commandDataBackupset.client.masterPort, masterPort);
					strcpy(commandDataBackupset.client.masterLogPort, masterLogPort);
					
					break;
				}
			}
			
			va_close(fdNode);
		}
		
		if (commandDataBackupset.client.nodeType == NODE_UNKNOWN)
		{
			continue;
		}
		
		// client�� ����ִ��� Ȯ���Ѵ�.
		if ((clientSock = va_connect(commandDataBackupset.client.ip, commandDataBackupset.client.port, 0)) != -1)
		{
			va_close_socket(clientSock, ABIO_SOCKET_CLOSE_CLIENT);
		}
		else
		{
			continue;
		}
		
		
		commandBackupset.request = 1;
		commandBackupset.reply = 0;
		strcpy(commandBackupset.requestCommand, MODULE_NAME_DB_ORACLE_RMAN_HTTPD);
		strcpy(commandBackupset.subCommand, "<EXPIRE_ORACLE_RMAN_BACKUPSET_SILENT>");
		strcpy(commandBackupset.executeCommand, MODULE_NAME_MASTER_HTTPD);
		strcpy(commandBackupset.sourceIP, masterIP);
		strcpy(commandBackupset.sourcePort, masterPort);
		strcpy(commandBackupset.sourceNodename, masterNodename);
		strcpy(commandBackupset.destinationIP, commandDataBackupset.client.ip);
		strcpy(commandBackupset.destinationPort, commandDataBackupset.client.port);
		strcpy(commandBackupset.destinationNodename, commandDataBackupset.client.nodename);

		memcpy(commandDataBackupset.db.dbData, &expiredOracleRmanList[i].oracleRman, sizeof(struct oracleRmanDB));		
		
		// client�� expire�� backupset���� ������ expire ��û�� ������.
		for (j = 0; j < expiredOracleRmanList[i].backupsetNumber; j++)
		{
			memset(commandDataBackupset.backupset, 0, sizeof(commandDataBackupset.backupset));
			strcpy(commandDataBackupset.backupset, expiredOracleRmanList[i].backupset[j]);			
			//OracleRmanSyncBackupsetList(commandBackupset, commandDataBackupset);
			// send commandBackupset to client
			if ((commandSock = va_connect("127.0.0.1", masterPort, 1)) != -1)
			{
				memcpy(commandBackupset.dataBlock, &commandDataBackupset, sizeof(struct ckBackup));
				
				va_send(commandSock, &commandBackupset, sizeof(struct ck), 0, DATA_TYPE_CK);
				va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
			}
		}
	}
	// backupset�� client backupset list�� catalog db�� expire�Ѵ�.
	for (i = 0; i < expiredVolumeDBNumber; i++)
	{
		memset(backupsetListFile, 0, sizeof(backupsetListFile));
		va_get_backupset_list_file_name(expiredVolumeDB[i].client, expiredVolumeDB[i].backupData, backupsetListFile);
		
		if (backupsetListFile[0] == '\0')
		{
			// volume db�� backup data type�� �߸� �� �ִ�. �� ����� ���� ó���� ��� ������ ���Ŀ� �����ϵ��� �Ѵ�.
			continue;
		}
		
		if ((fdBackupsetList = va_open(ABIOMASTER_CATALOG_DB_FOLDER, backupsetListFile, O_RDWR, 1, 1)) != (va_fd_t)-1)
		{
			// backupset list���� backupset�� ã�Ƽ� expire�ϰ�, 
			// backupset�� primary copy��� ����� backupset copy�� ã�Ƽ� ��å�� ���� expire�Ѵ�.
			while (va_read(fdBackupsetList, &backupsetList, sizeof(struct backupsetList), DATA_TYPE_BACKUPSET_LIST) > 0)
			{
				if (!strcmp(backupsetList.backupset, expiredVolumeDB[i].backupset))
				{
					// backupset list���� backupset�� expire�Ѵ�.
					memset(backupsetList.volume[expiredVolumeDB[i].backupsetCopyNumber - 1], 0, sizeof(backupsetList.volume[expiredVolumeDB[i].backupsetCopyNumber - 1]));
					
					// backupset�� primary copy�� ��� ����� backupset copy�� ã�Ƽ� ��å�� ���� expire�Ѵ�.
					if (expiredVolumeDB[i].backupsetCopyNumber == 1)
					{
						for (j = 1; j < BACKUPSET_COPY_NUMBER; j++)
						{
							// ����� backupset copy�� ������ ��å�� ���� expire�Ѵ�.
							if (backupsetList.volume[j][0] != '\0')
							{
								if ((fdVolumeDB = va_open(ABIOMASTER_VOLUME_DB_FOLDER, backupsetList.volume[j], O_RDWR, 1, 1)) != (va_fd_t)-1)
								{
									// ����� backupset copy�� ã�Ƽ� ��å�� ���� expire�Ѵ�.
									while (va_read(fdVolumeDB, &volumeDB, sizeof(struct volumeDB), DATA_TYPE_VOLUME_DB) > 0)
									{
										if (!strcmp(volumeDB.backupset, expiredVolumeDB[i].backupset) && (volumeDB.backupsetCopyNumber == j + 1) && (volumeDB.backupsetStatus != ABIO_BACKUPSET_STATUS_EXPIRED))
										{
											// ����� backupset copy�� retention ������ primary copy�� ���󰡵��� �����Ǿ������� expire�Ѵ�.
											if (volumeDB.retention[2] == 0)
											{
												// ����� backupset copy�� expire�Ѵ�.
												volumeDB.backupsetStatus = ABIO_BACKUPSET_STATUS_EXPIRED;
												volumeDB.expirationTime = current_t;
												
												va_lseek(fdVolumeDB, -(va_offset_t)sizeof(struct volumeDB), SEEK_CUR);
												va_write(fdVolumeDB, &volumeDB, sizeof(struct volumeDB), DATA_TYPE_VOLUME_DB);
												
												
												// next media�� split backupset�� expire�Ѵ�.
												memcpy(&volumeDBNext, &volumeDB, sizeof(struct volumeDB));
												while (volumeDBNext.nextVolume[0] != '\0')
												{
													if ((fdVolumeDBNext = va_open(ABIOMASTER_VOLUME_DB_FOLDER, volumeDBNext.nextVolume, O_RDWR, 1, 1)) != (va_fd_t)-1)
													{
														while (va_read(fdVolumeDBNext, &volumeDBNext, sizeof(struct volumeDB), DATA_TYPE_VOLUME_DB) > 0)
														{
															if (!strcmp(volumeDBNext.backupset, volumeDB.backupset) && volumeDBNext.backupsetCopyNumber == volumeDB.backupsetCopyNumber && 
																!strcmp(volumeDBNext.jobID, volumeDB.jobID) && volumeDBNext.backupsetStatus != ABIO_BACKUPSET_STATUS_EXPIRED)
															{
																volumeDBNext.backupsetStatus = ABIO_BACKUPSET_STATUS_EXPIRED;
																volumeDBNext.expirationTime = volumeDB.expirationTime;
																
																va_lseek(fdVolumeDBNext, -(va_offset_t)sizeof(struct volumeDB), SEEK_CUR);
																va_write(fdVolumeDBNext, &volumeDBNext, sizeof(struct volumeDB), DATA_TYPE_VOLUME_DB);
																
																break;
															}
														}
														
														va_close(fdVolumeDBNext);
													}
													else
													{
														break;
													}
												}
												
												// backupset list���� ����� backupset copy�� expire�Ѵ�.
												memset(backupsetList.volume[j], 0, sizeof(backupsetList.volume[j]));
											}
											
											break;
										}
									}
									
									va_close(fdVolumeDB);
								}
							}
						}
					}
					
					break;
				}
			}
			
			
			// backupset�� ��� copy�� expire�Ǿ����� Ȯ���ϰ�, new restore backupset copy number�� ���Ѵ�.
			isValidBackupsetCopy = 0;
			for (j = 0; j < BACKUPSET_COPY_NUMBER; j++)
			{
				if (backupsetList.volume[j][0] != '\0')
				{
					isValidBackupsetCopy = 1;
					newRestoreBackupsetCopyNumber = j + 1;
					
					break;
				}
			}
			
			
			// ��� copy�� expire�Ǿ��ٸ� backupset list���� expire�� backupset�� �����Ѵ�.
			// ���� backupset�� catalog db�� backup file list�� �����ϰ�, the latest catalog db���� expire�� backupset�� ����ִ� ���ϵ��� expire�Ѵ�.
			if (isValidBackupsetCopy == 0)
			{
				// backupset list���� expire�� backupset�� �����Ѵ�.
				newBackupsetList = NULL;
				newBackupsetListNumber = 0;
				
				va_lseek(fdBackupsetList, 0, SEEK_SET);
				while (va_read(fdBackupsetList, &backupsetList, sizeof(struct backupsetList), DATA_TYPE_BACKUPSET_LIST) > 0)
				{
					if (strcmp(backupsetList.backupset, expiredVolumeDB[i].backupset) != 0)
					{
						if (newBackupsetListNumber % DEFAULT_MEMORY_FRAGMENT == 0)
						{
							newBackupsetList = (struct backupsetList *)realloc(newBackupsetList, sizeof(struct backupsetList) * (newBackupsetListNumber + DEFAULT_MEMORY_FRAGMENT));
							memset(newBackupsetList + newBackupsetListNumber, 0, sizeof(struct backupsetList) * DEFAULT_MEMORY_FRAGMENT);
						}
						va_change_byte_order_backupsetList(&backupsetList);		// host to network
						memcpy(newBackupsetList + newBackupsetListNumber, &backupsetList, sizeof(struct backupsetList));
						newBackupsetListNumber++;
						va_change_byte_order_backupsetList(&backupsetList);		// network to host
					}
				}
				
				va_lseek(fdBackupsetList, 0, SEEK_SET);
				va_write(fdBackupsetList, newBackupsetList, sizeof(struct backupsetList) * newBackupsetListNumber, DATA_TYPE_NOT_CHANGE);
				va_ftruncate(fdBackupsetList, (va_offset_t)(sizeof(struct backupsetList) * newBackupsetListNumber));
				
				va_free(newBackupsetList);
				

				// expire�� backupset�� catalog db�� �����Ѵ�.
				if(expiredVolumeDB[i].backupsetCopyNumber != 1)
				{
					va_remove(ABIOMASTER_CATALOG_DB_FOLDER, va_strcpy("%s.%d",expiredVolumeDB[i].backupset,expiredVolumeDB[i].backupsetCopyNumber));
				}
				va_remove(ABIOMASTER_CATALOG_DB_FOLDER, expiredVolumeDB[i].backupset);
				
				// expire�� backupset�� backup file list�� �����Ѵ�.
				memset(filelist, 0, sizeof(filelist));
				sprintf(filelist, "%s_%s.list", expiredVolumeDB[i].client, expiredVolumeDB[i].backupset);
				va_remove(ABIOMASTER_FILE_LIST_FOLDER, filelist);
				
				// the latest catalog db���� expire�� backupset�� ����ִ� ���ϵ��� expire�Ѵ�.
				va_expire_latest_catalog_db(expiredVolumeDB[i].client, expiredVolumeDB[i].backupData, expiredVolumeDB[i].backupset);
			}
			// valid backupset copy�� ������ backupset list�� ������Ʈ�Ѵ�.
			// ���� backupset�� catalog db�� the latest catalog db���� restore backupset copy number�� �����Ѵ�.
			// restore backupset copy number�� backupset���� ������ ��������� �� ��� copy���� ������������� �����Ѵ�.
			// restore backupset copy number�� valid backupset copy �߿��� copy number�� ���� ���� copy�� �����ȴ�.
			else
			{
				// expire�� backupset�� catalog db�� �����Ѵ�.
				if(expiredVolumeDB[i].backupsetCopyNumber != 1)
				{
					va_remove(ABIOMASTER_CATALOG_DB_FOLDER, va_strcpy("%s.%d",expiredVolumeDB[i].backupset,expiredVolumeDB[i].backupsetCopyNumber));
				}
				// backupset list�� ������Ʈ�Ѵ�.
				va_lseek(fdBackupsetList, -(va_offset_t)sizeof(struct backupsetList), SEEK_CUR);
				va_write(fdBackupsetList, &backupsetList, sizeof(struct backupsetList), DATA_TYPE_BACKUPSET_LIST);
				
				
				// restore backupset copy number�� valid backupset copy �߿��� copy number�� ���� ���� copy�̱� ������
				// expire�� backupset copy�� copy number�� new restore backupset copy number���� ������ restore backupset copy�� expire�Ǿ��� ������
				// backupset�� restore backupset copy number�� new restore backupset copy number�� �����Ѵ�.
				if (expiredVolumeDB[i].backupsetCopyNumber < newRestoreBackupsetCopyNumber)
				{
					// backupset�� catalog db���� restore backupset copy�� �����Ѵ�.
					va_update_catalog_db_backupset_copy(expiredVolumeDB[i].backupset, newRestoreBackupsetCopyNumber, backupsetList.volume[newRestoreBackupsetCopyNumber - 1]);
					
					// the latest catalog db���� restore backupset copy�� �����Ѵ�.
					va_update_latest_catalog_db_backupset_copy(expiredVolumeDB[i].client, expiredVolumeDB[i].backupData, expiredVolumeDB[i].backupset, newRestoreBackupsetCopyNumber, backupsetList.volume[newRestoreBackupsetCopyNumber - 1]);
				}
			}
			
			va_close(fdBackupsetList);
			
			
			// backupset list�� �����ִ� backupset�� ������ backupset list file�� �����Ѵ�.
			if (isValidBackupsetCopy == 0 && newBackupsetListNumber == 0)
			{
				va_remove(ABIOMASTER_CATALOG_DB_FOLDER, backupsetListFile);
			}
		}
		// backupset list file�� �����Ǿ����� ��� copy�� expire�Ǿ��ٰ� �Ǵ��Ѵ�.
		// backupset�� catalog db�� backup file list�� �����ϰ�, the latest catalog db���� expire�� backupset�� ����ִ� ���ϵ��� expire�Ѵ�.
		else
		{
			// expire�� backupset�� catalog db�� �����Ѵ�.
			va_remove(ABIOMASTER_CATALOG_DB_FOLDER, expiredVolumeDB[i].backupset);
			
			// expire�� backupset�� backup file list�� �����Ѵ�.
			memset(filelist, 0, sizeof(filelist));
			sprintf(filelist, "%s_%s.list", expiredVolumeDB[i].client, expiredVolumeDB[i].backupset);
			va_remove(ABIOMASTER_FILE_LIST_FOLDER, filelist);
			
			// the latest catalog db���� expire�� backupset�� ����ִ� ���ϵ��� expire�Ѵ�.
			va_expire_latest_catalog_db(expiredVolumeDB[i].client, expiredVolumeDB[i].backupData, expiredVolumeDB[i].backupset);
		}
	}
	
	
	// media ������ ������Ʈ�Ѵ�.
	va_expire_backupset_media();
	
	// ����� �ڿ����� �ݳ��Ѵ�.
	va_free(expiredVolumeDB);
	
	for (i = 0; i < expiredClientListNumber; i++)
	{
		for (j = 0; j < expiredClientList[i].backupsetNumber; j++)
		{
			va_free(expiredClientList[i].backupset[j]);
		}
		va_free(expiredClientList[i].backupset);
	}
	va_free(expiredClientList);

	for (i = 0; i < expiredOracleRmanListNumber; i++)
	{
		for (j = 0; j < expiredOracleRmanList[i].backupsetNumber; j++)
		{
			va_free(expiredOracleRmanList[i].backupset[j]);
		}
		va_free(expiredOracleRmanList[i].backupset);
	}
	va_free(expiredOracleRmanList);
}

int OracleRmanSyncBackupsetList(struct ck syncCommand, struct ckBackup syncCommandData)
{
	va_fd_t fdBackupsetList;
	char backupsetListFile[MAX_PATH_LENGTH];
	struct backupsetList backupsetList;

	char msg[DSIZ * DSIZ];
	char msgValue[DSIZ][DSIZ];
	char * recvMsg;


	char ** oracleRmanBackupsetList;
	int oracleRmanBackupsetListNumber;

	char temp[1024];

	va_sock_t commandSock;
	va_sock_t masterSock;
	va_sock_t clientSock;

	int recvSize;
	int i;


	oracleRmanBackupsetList = NULL;
	oracleRmanBackupsetListNumber = 0;

	

	recvSize = 0;

	

	// oracle_rman type backupset ���� sid �� ���� ���� ���� �����Ѵ�.
	memset(backupsetListFile, 0, sizeof(backupsetListFile));
	va_get_backupset_list_file_name(syncCommandData.client.nodename, ABIO_BACKUP_DATA_ORACLE_RMAN, backupsetListFile);
	
	if ((fdBackupsetList = va_open(ABIOMASTER_CATALOG_DB_FOLDER, backupsetListFile, O_RDONLY, 1, 1)) != (va_fd_t)-1)
	{
		while (va_read(fdBackupsetList, &backupsetList, sizeof(struct backupsetList), DATA_TYPE_BACKUPSET_LIST) > 0)
		{
			if (!strcmp(backupsetList.instance, ((struct oracleRmanDB *)syncCommandData.db.dbData)->sid))
			{
				if (oracleRmanBackupsetListNumber % DEFAULT_MEMORY_FRAGMENT == 0)
				{
					oracleRmanBackupsetList = (char **)realloc(oracleRmanBackupsetList, sizeof(char *) * (oracleRmanBackupsetListNumber + DEFAULT_MEMORY_FRAGMENT));
					memset(oracleRmanBackupsetList + oracleRmanBackupsetListNumber, 0, sizeof(char *) * DEFAULT_MEMORY_FRAGMENT);
				}
				oracleRmanBackupsetList[oracleRmanBackupsetListNumber] = (char *)malloc(BACKUPSET_ID_LENGTH);

				
				memset(oracleRmanBackupsetList[oracleRmanBackupsetListNumber], 0, BACKUPSET_ID_LENGTH);
				strcpy(oracleRmanBackupsetList[oracleRmanBackupsetListNumber], backupsetList.backupset);
				oracleRmanBackupsetListNumber++;
			}
		}

		va_close(fdBackupsetList);
	}
	else
	{
		return 2;
	}
	// Oracle RMAN client httpd �� ���� backupset �� sync �Ѵ�.
	if (!GET_ERROR(syncCommandData.errorCode))
	{	
		
		// client�κ��� ������ ���� server socket�� �����.
		if ((masterSock = va_make_server_socket_iptable(syncCommand.destinationIP, syncCommand.sourceIP, portRule, portRuleNumber, syncCommandData.catalogPort)) != -1)
		{
			// ����� client�� ������.
			if ((commandSock = va_connect("127.0.0.1", masterPort, 1)) != -1)
			{
				memcpy(syncCommand.dataBlock, &syncCommandData, sizeof(struct ckBackup));
				va_send(commandSock, &syncCommand, sizeof(struct ck), 0, DATA_TYPE_CK);
				va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
				
				// client�κ��� ������ ��ٸ���.
		
				if ((clientSock = va_wait_connection(masterSock, TIME_OUT_JOB_CONNECTION, NULL)) >= 0)
				{
					// client �� Ready ���¸� ��ٸ���.
					if ((recvSize = va_recv(clientSock, msg, sizeof(msg), 0, DATA_TYPE_NOT_CHANGE)) > 0)
					{
					
						for (i = 0; i < (int)strlen(msg)+1; i++)
						{
							recvMsg = msg + i;
							
							if (recvMsg[0] != '\0')
							{
								va_parser(recvMsg, msgValue);
								
								if (strcmp(msgValue[0],"SYNC_ORACLE_RMAN_BACKUPSET_LIST_READY") == 0)
								{
									break;
								}
								else if (strcmp(msgValue[0],"<SYNC_ORACLE_RMAN_BACKUPSET_LIST>") == 0)
								{
									va_close_socket(clientSock, ABIO_SOCKET_CLOSE_CLIENT);
									va_close_socket(masterSock, 0);

									return 2;
								}

								i += (int)strlen(recvMsg);
							}
						}
				
					}
					memset(temp,0,1024);
					va_itoa(oracleRmanBackupsetListNumber,temp);
					
					va_send(clientSock,temp,10,0,DATA_TYPE_NOT_CHANGE);					
					// ABIO �� Backupset ������ �����Ѵ�.
					for (i = 0; i < oracleRmanBackupsetListNumber; i++)
					{
						if (va_send(clientSock, oracleRmanBackupsetList[i], BACKUPSET_ID_LENGTH, 0, DATA_TYPE_NOT_CHANGE) < 0)
						{								
							break;
						}
					}
					memset(msg, 0, sizeof(msg));
					// client �� ó�� ����� �޴´�.
					if ((recvSize = va_recv(clientSock, msg, sizeof(msg), 0, DATA_TYPE_NOT_CHANGE)) > 0)
					{
						for (i = 0; i < recvSize; i++)
						{							
							recvMsg = msg + i;
							if (recvMsg[0] != '\0')
							{
								va_parser(recvMsg, msgValue);	
								if (strcmp(msgValue[0],"SYNC_ORACLE_RMAN_BACKUPSET_LIST_END") == 0)
								{
									if (strcmp(msgValue[1], "ok") != 0)
									{
										va_close_socket(clientSock, ABIO_SOCKET_CLOSE_CLIENT);
										va_close_socket(masterSock, 0);

										return 2;
									}
								}
								i += (int)strlen(recvMsg);	
							}
						}
					}
					va_close_socket(clientSock, ABIO_SOCKET_CLOSE_CLIENT);
				}

				va_close_socket(masterSock, 0);
			}
		}
	}

	
	for (i = 0; i < oracleRmanBackupsetListNumber; i++)
	{
		va_free(oracleRmanBackupsetList[i]);
	}

	va_free(oracleRmanBackupsetList);
	return 0;
}

void va_expire_backupset_media()
{
	va_fd_t fdVolume;
	struct volume volume;
	
	char nodename[NODE_NAME_LENGTH];
	char repository[ABIO_FILE_LENGTH];
	struct networkDriveConfig networkDrive;
	
	struct volume * volumeList;
	int volumeListNumber;
	
	va_fd_t fdVolumeDB;
	struct volumeDB volumeDB;
	struct abio_file_stat volumeDBFileStatus;
	
	char volumePoolName[VOLUME_POOL_NAME_LENGTH];
	int expiredBackupsetNumber;
	int totalBackupsetNumber;
	enum volumeUsage volumeUsage;
	__uint64 volumeSize;
	__uint64 backupsetEndPosition;
	
	time_t current_t;
	
	
	
	// initialize variables
	volumeList = NULL;
	volumeListNumber = 0;
	
	current_t = time(NULL);
	
	
	if ((fdVolume = va_open(ABIOMASTER_DATA_FOLDER, ABIOMASTER_VOLUME_LIST, O_RDWR, 1, 0)) != (va_fd_t)-1)
	{
		while (va_read(fdVolume, &volume, sizeof(struct volume), DATA_TYPE_VOLUME) > 0)
		{
			// volume�� ������̸� expire �˻縦 ���� �ʴ´�.
			if (volume.use == 1)
			{
				if (volumeListNumber % DEFAULT_MEMORY_FRAGMENT == 0)
				{
					volumeList = (struct volume *)realloc(volumeList, sizeof(struct volume) * (volumeListNumber + DEFAULT_MEMORY_FRAGMENT));
					memset(volumeList + volumeListNumber, 0, sizeof(struct volume) * DEFAULT_MEMORY_FRAGMENT);
				}
				va_change_byte_order_volume(&volume);		// host to network
				memcpy(volumeList + volumeListNumber, &volume, sizeof(struct volume));
				volumeListNumber++;
				va_change_byte_order_volume(&volume);		// network to host
				
				continue;
			}
			
			
			// volume�� expire�Ǿ����� �˻��Ѵ�.
			memset(volumePoolName, 0, sizeof(volumePoolName));
			expiredBackupsetNumber = 0;
			totalBackupsetNumber = 0;
			volumeUsage = VOLUME_USAGE_EMPTY;
			volumeSize = 0;
			backupsetEndPosition = 0;
			
			// volume db ������ ������ �ִ��� Ȯ���Ѵ�.
			va_stat(ABIOMASTER_VOLUME_DB_FOLDER, volume.name, &volumeDBFileStatus);
			
			// tape media�� volume db�� ��� backupset�� ��� �ִ��� Ȯ���Ѵ�.
			if ((fdVolumeDB = va_open(ABIOMASTER_VOLUME_DB_FOLDER, volume.name, O_RDONLY, 1, 0)) != (va_fd_t)-1)
			{
				while (va_read(fdVolumeDB, &volumeDB, sizeof(struct volumeDB), DATA_TYPE_VOLUME_DB) > 0)
				{
					totalBackupsetNumber++;
					volumeSize += volumeDB.fileSize;
					
					if (volumeDB.backupsetStatus == ABIO_BACKUPSET_STATUS_EXPIRED)
					{
						expiredBackupsetNumber++;
					}
					
					if (backupsetEndPosition < volumeDB.backupsetEndPosition)
					{
						backupsetEndPosition = volumeDB.backupsetEndPosition;
					}
					
					if (volumeDB.backupsetEndPosition == 0)
					{
						volumeUsage = VOLUME_USAGE_FULL;
					}
					else
					{
						volumeUsage = VOLUME_USAGE_FILLING;
					}
					
					memset(volumePoolName, 0, sizeof(volumePoolName));
					strcpy(volumePoolName, volumeDB.pool);
				}
				
				va_close(fdVolumeDB);
			}
			
			// volume db�� ���ų�, �� backupset ������ expire�� backupset ������ ������ expire�Ǿ��ٰ� �Ǵ��Ѵ�.
			if ((totalBackupsetNumber == expiredBackupsetNumber) && 
				((volumeDBFileStatus.size > 0 && totalBackupsetNumber > 0) || (volumeDBFileStatus.size == 0 && totalBackupsetNumber == 0)))
			{
				// delete volume db of the volume
				va_remove(ABIOMASTER_VOLUME_DB_FOLDER, volume.name);
				
				// disk volume�� expire�Ǿ����� disk volume file�� �����Ѵ�.
				if (volume.type == VOLUME_DISK)
				{
					memset(nodename, 0, sizeof(nodename));
					memset(repository, 0, sizeof(repository));
					va_get_disk_volume_repository(volume.storage, nodename, repository, &networkDrive);
					
					// remove the physical disk media
					if (repository[0] != '\0')
					{
						va_remove_disk_volume(nodename, repository, volume.name, &networkDrive);
						if(strlen(volume.sourceDedupNodeName))
						{
							va_remove_disk_volume(volume.sourceDedupNodeName, "DEDUP_DATABASE", volume.name, &networkDrive);
						}
					}
				}
				else
				{
					if (volume.volumePoolAccess == 0 && strcmp(volume.pool, ABIOMASTER_CLEANING_POOL_NAME))
					{
						volume.assignTime = 0;
						memset(volume.pool, 0, sizeof(volume.pool));
						strcpy(volume.pool, ABIOMASTER_SCRATCH_POOL_NAME);
					}
					
					volume.usage = VOLUME_USAGE_EMPTY;
					volume.backupsetNumber = 0;
					volume.expiredBackupsetNumber = 0;
					volume.volumeSize = 0;
					volume.backupsetPosition = 0;
					if (totalBackupsetNumber > 0)
					{
						volume.expirationTime = current_t;
					}
					
					if (volumeListNumber % DEFAULT_MEMORY_FRAGMENT == 0)
					{
						volumeList = (struct volume *)realloc(volumeList, sizeof(struct volume) * (volumeListNumber + DEFAULT_MEMORY_FRAGMENT));
						memset(volumeList + volumeListNumber, 0, sizeof(struct volume) * DEFAULT_MEMORY_FRAGMENT);
					}
					va_change_byte_order_volume(&volume);		// host to network
					memcpy(volumeList + volumeListNumber, &volume, sizeof(struct volume));
					volumeListNumber++;
					va_change_byte_order_volume(&volume);		// network to host
				}
			}
			// volume�� expire���� ���� ��쿡�� volume�� backupset ���� ������ ������Ʈ�Ѵ�.
			else
			{
				volume.usage = volumeUsage;
				volume.backupsetNumber = totalBackupsetNumber;
				volume.expiredBackupsetNumber = expiredBackupsetNumber;
				volume.volumeSize = volumeSize;
				volume.backupsetPosition = backupsetEndPosition;
				
				if (!strcmp(volume.pool, ABIOMASTER_SCRATCH_POOL_NAME) && volumePoolName[0] != '\0')
				{
					memset(volume.pool, 0, sizeof(volume.pool));
					strcpy(volume.pool, volumePoolName);
				}
				
				if (volumeListNumber % DEFAULT_MEMORY_FRAGMENT == 0)
				{
					volumeList = (struct volume *)realloc(volumeList, sizeof(struct volume) * (volumeListNumber + DEFAULT_MEMORY_FRAGMENT));
					memset(volumeList + volumeListNumber, 0, sizeof(struct volume) * DEFAULT_MEMORY_FRAGMENT);
				}
				va_change_byte_order_volume(&volume);		// host to network
				memcpy(volumeList + volumeListNumber, &volume, sizeof(struct volume));
				volumeListNumber++;
				va_change_byte_order_volume(&volume);		// network to host
			}
		}
		
		qsort(volumeList, volumeListNumber, sizeof(struct volume), va_volume_list_comparator);
		
		va_lseek(fdVolume, 0, SEEK_SET);
		va_write(fdVolume, volumeList, sizeof(struct volume) * volumeListNumber, DATA_TYPE_NOT_CHANGE);
		va_ftruncate(fdVolume, (va_offset_t)(sizeof(struct volume) * volumeListNumber));
		va_free(volumeList);
		
		va_close(fdVolume);
		va_copy_file(ABIOMASTER_DATA_FOLDER, ABIOMASTER_VOLUME_LIST,ABIOMASTER_DATA_FOLDER, ABIOMASTER_VOLUME_READ_LIST);
	}
}

static void va_get_disk_volume_repository(char * storageName, char * nodename, char * repository, struct networkDriveConfig * networkDrive)
{
	va_fd_t fdStorage;
	struct storage storage;
	struct tapeLibrary tapeLibrary;
	struct tapeDrive tapeDrive;
	struct diskStorage diskStorage;
	struct storageOwner storageOwner;
	
	int i;
	
	
	
	if ((fdStorage = va_open(ABIOMASTER_DATA_FOLDER, ABIOMASTER_STORAGE_READ_LIST, O_RDONLY, 0, 1)) != (va_fd_t)-1)
	{
		while (va_read(fdStorage, &storage, sizeof(struct storage), DATA_TYPE_STORAGE) > 0)
		{
			if (!strcmp(storage.name, storageName))
			{
				if (storage.type == STORAGE_DISK)
				{
					va_read(fdStorage, &diskStorage, sizeof(struct diskStorage), DATA_TYPE_DISK_STORAGE);
					va_read(fdStorage, &storageOwner, sizeof(struct storageOwner), DATA_TYPE_STORAGE_OWNER);
					
					strcpy(nodename, storageOwner.nodename);
					strcpy(repository, storageOwner.repository);
					memcpy(networkDrive, &diskStorage.networkDrive, sizeof(struct networkDriveConfig));
				}
				
				break;
			}
			else if (storage.type == STORAGE_TAPE_LIBRARY)
			{
				va_read(fdStorage, &tapeLibrary, sizeof(struct tapeLibrary), DATA_TYPE_TAPE_LIBRARY);
				va_lseek(fdStorage, (va_offset_t)sizeof(struct storageOwner), SEEK_CUR);
				
				for (i = 0; i < tapeLibrary.driveNumber; i++)
				{
					va_read(fdStorage, &tapeDrive, sizeof(struct tapeDrive), DATA_TYPE_TAPE_DRIVE);
					va_lseek(fdStorage, (va_offset_t)(sizeof(struct storageOwner) * tapeDrive.driveOwnerNumber), SEEK_CUR);
				}
			}
			else if (storage.type == STORAGE_STANDALONE_TAPE_DRIVE)
			{
				va_lseek(fdStorage, (va_offset_t)sizeof(struct tapeDrive), SEEK_CUR);
				va_lseek(fdStorage, (va_offset_t)sizeof(struct storageOwner), SEEK_CUR);
			}
			else if (storage.type == STORAGE_DISK)
			{
				va_lseek(fdStorage, (va_offset_t)sizeof(struct diskStorage), SEEK_CUR);
				va_lseek(fdStorage, (va_offset_t)sizeof(struct storageOwner), SEEK_CUR);
			}
			else
			{
				break;
			}
		}
		
		va_close(fdStorage);
	}
}

void va_remove_disk_volume_event_log(char * nodename, char * repository, char * volumeName)
{
	struct ckBackup commandData;
	
	memset(&commandData, 0, sizeof(struct ckBackup));
	
	strcpy(commandData.loginID, nodename);
	strcpy(commandData.loginIP, repository);
	strcpy(commandData.object3, volumeName);

	//������ Job ID�� ǥ��
	sprintf(commandData.client.nodename, "%s", masterNodename);

	va_get_job_id(&commandData);
	
	commandData.version = CURRENT_VERSION_INTERNAL;
	commandData.jobType = DELETE_VOLUME_JOB;
	commandData.state = STATE_END;

	va_make_log(&commandData, STATE_END, MODULE_MASTER_HTTPD);

	//log
	WriteDebugData(ABIOMASTER_DATA_FOLDER,"REMOVE_DISK_VOLUME.log", "--------------------------------------");	
	WriteDebugData(ABIOMASTER_DATA_FOLDER,"REMOVE_DISK_VOLUME.log", "nodename : %s", nodename);
	WriteDebugData(ABIOMASTER_DATA_FOLDER,"REMOVE_DISK_VOLUME.log", "repository : %s", repository);
	WriteDebugData(ABIOMASTER_DATA_FOLDER,"REMOVE_DISK_VOLUME.log", "volumeName : %s", volumeName);
}

static int va_remove_disk_volume(char * nodename, char * repository, char * volumeName, struct networkDriveConfig * networkDrive)
{
	struct ck removeCommand;
	struct ckBackup removeCommandData;
	
	va_fd_t fdNode;
	struct abioNode node;
	
	va_sock_t commandSock;
	va_sock_t masterSock;
	va_sock_t clientSock;
	
	int removeDiskVolumeResult;
	
	// initialize variables
	removeDiskVolumeResult = -1;
	memset(&removeCommand, 0, sizeof(struct ck));
	memset(&removeCommandData, 0, sizeof(struct ckBackup));

	// client setting
	if ((fdNode = va_open(ABIOMASTER_DATA_FOLDER, ABIOMASTER_NODE_LIST, O_RDONLY, 1, 1)) != (va_fd_t)-1)
	{
		while (va_read(fdNode, &node, sizeof(struct abioNode), DATA_TYPE_NODE) > 0)
		{
			if (!strcmp(node.name, nodename))
			{
				removeCommandData.client.platformType = node.platformType;
				removeCommandData.client.nodeType = node.nodeType;
				strcpy(removeCommandData.client.nodename, node.name);
				strcpy(removeCommandData.client.ip, node.ip);
				strcpy(removeCommandData.client.port, node.port);
				strcpy(removeCommandData.client.masterIP, masterIP);
				strcpy(removeCommandData.client.masterPort, masterPort);
				strcpy(removeCommandData.client.masterLogPort, masterLogPort);
				
				break;
			}
		}
		
		va_close(fdNode);
	}
	
	if (removeCommandData.client.nodeType == NODE_UNKNOWN)
	{
		return -1;
	}
	
	// client�� ����ִ��� Ȯ���Ѵ�.
	if ((clientSock = va_connect(removeCommandData.client.ip, removeCommandData.client.port, 0)) != -1)
	{
		va_close_socket(clientSock, ABIO_SOCKET_CLOSE_CLIENT);
	}
	else
	{
		return -1;
	}
	
	
	removeCommand.request = 1;
	removeCommand.reply = 0;
	strcpy(removeCommand.requestCommand, MODULE_NAME_CLIENT_HTTPD);
	strcpy(removeCommand.subCommand, "<REMOVE_FILE>");
	strcpy(removeCommand.executeCommand, MODULE_NAME_MASTER_HTTPD);
	strcpy(removeCommand.sourceIP, masterIP);
	strcpy(removeCommand.sourcePort, masterPort);
	strcpy(removeCommand.sourceNodename, masterNodename);
	strcpy(removeCommand.destinationIP, removeCommandData.client.ip);
	strcpy(removeCommand.destinationPort, removeCommandData.client.port);
	strcpy(removeCommand.destinationNodename, removeCommandData.client.nodename);
	
	sprintf(removeCommandData.backupFile, "%s%c%s", repository, DIRECTORY_IDENTIFIER, volumeName);
	memcpy(&removeCommandData.mountStatus.networkDrive, networkDrive, sizeof(struct networkDriveConfig));
	
	
	// ��� ó�� ����� ���� server socket�� �����.
	if ((masterSock = va_make_server_socket_iptable(removeCommand.destinationIP, removeCommand.sourceIP, portRule, portRuleNumber, removeCommandData.catalogPort)) != -1)
	{
		// ����� storage node�� ������.
		if ((commandSock = va_connect("127.0.0.1", masterPort, 1)) != -1)
		{
			memcpy(removeCommand.dataBlock, &removeCommandData, sizeof(struct ckBackup));
			
			va_send(commandSock, &removeCommand, sizeof(struct ck), 0, DATA_TYPE_CK);
			va_close_socket(commandSock, ABIO_SOCKET_CLOSE_CLIENT);
			
			
			// ��� ó�� ����� ��ٸ���.
			if ((clientSock = va_wait_connection(masterSock, TIME_OUT_JOB_CONNECTION, NULL)) >= 0)
			{
				if (va_recv(clientSock, &removeCommandData, sizeof(struct ckBackup), 0, DATA_TYPE_CK_BACKUP) == sizeof(struct ckBackup))
				{
					if (!GET_ERROR(removeCommandData.errorCode))
					{
						removeDiskVolumeResult = 0;
					}
				}
				
				va_close_socket(clientSock, ABIO_SOCKET_CLOSE_SERVER);
			}
		}
		
		va_close_socket(masterSock, 0);
	}

	//Event Log�� �����Ǵ� �����̸� ǥ��
	va_remove_disk_volume_event_log(nodename, repository, volumeName);

	if (removeDiskVolumeResult == 0)
	{
		return 0;
	}
	else
	{
		return -1;
	}
}

static int va_volume_list_comparator(const void * a1, const void * a2)
{
	struct volume * l1;
	struct volume * l2;
	
	
	
	l1 = (struct volume *)a1;
	l2 = (struct volume *)a2;
	
	
	if (!strcmp(l1->storage, l2->storage))
	{
		// storage�� ���� �� disk storage�̸� volume �̸� ������ �����Ѵ�.
		if (l1->type == VOLUME_DISK)
		{
			return strcmp(l1->name, l2->name);
		}
		// storage�� ���� �� tape volume�̸� slot address ������ �����Ѵ�.
		else
		{
			return l1->address - l2->address;
		}
	}
	else
	{
		// �Ѵ� standalone tape drive���� ����ϴ� volume�̸� volume �̸� ������ �����Ѵ�.
		if (l1->storage[0] == '\0' && l2->storage[0] == '\0')
		{
			return strcmp(l1->name, l2->name);
		}
		// standalone tape drive���� ����ϴ� volume�� �ڷ� ������.
		else if (l1->storage[0] == '\0')
		{
			return 1;
		}
		// standalone tape drive���� ����ϴ� volume�� �ڷ� ������.
		else if (l2->storage[0] == '\0')
		{
			return -1;
		}
		// storage �̸��� �ٸ����� storage �̸� ������ �����Ѵ�.
		else
		{
			return strcmp(l1->storage, l2->storage);
		}
	}
}

static void va_expire_latest_catalog_db(char * client, enum backupDataType backupData, char * backupset)
{
	va_fd_t fdCatalogLatest;
	struct catalogDB catalogDB;
	
	char catalogLatestFile[MAX_PATH_LENGTH];
	int isValidBackupset;
	
	
	
	memset(catalogLatestFile, 0, sizeof(catalogLatestFile));
	if (va_get_latest_catalog_db_file_name(client, backupData, catalogLatestFile) != 0)
	{
		return;
	}
	
	// expire latest catalog db
	if ((fdCatalogLatest = va_open(ABIOMASTER_CATALOG_DB_FOLDER, catalogLatestFile, O_RDWR, 1, 0)) != (va_fd_t)-1)
	{
		// expire catalog of backupset in latest catalog db
		while (va_read(fdCatalogLatest, &catalogDB, sizeof(struct catalogDB), DATA_TYPE_CATALOG_DB) > 0)
		{
			if (!strcmp(catalogDB.backupset, backupset))
			{
				catalogDB.expire = 1;
				
				va_lseek(fdCatalogLatest, -(va_offset_t)sizeof(struct catalogDB), SEEK_CUR);
				va_write(fdCatalogLatest, &catalogDB, sizeof(struct catalogDB), DATA_TYPE_CATALOG_DB);
			}
			
			va_lseek(fdCatalogLatest, (va_offset_t)(catalogDB.length + catalogDB.length2 + catalogDB.length3), SEEK_CUR);
		}
		
		
		// check whether the latest catalog db is expired or not.
		// the latest catalog db�� �ִ� ��� file�� expire�Ǿ����� the latest catalog db�� expire�Ǿ��ٰ� �Ǵ��Ѵ�.
		isValidBackupset = 0;
		
		va_lseek(fdCatalogLatest, 0, SEEK_SET);
		while (va_read(fdCatalogLatest, &catalogDB, sizeof(struct catalogDB), DATA_TYPE_CATALOG_DB) > 0)
		{
			if (catalogDB.expire == 0)
			{
				isValidBackupset = 1;
				
				break;
			}
			
			va_lseek(fdCatalogLatest, (va_offset_t)(catalogDB.length + catalogDB.length2 + catalogDB.length3), SEEK_CUR);
		}
		
		va_close(fdCatalogLatest);
		
		
		// if the latest catalog db is expired, remove it
		if (isValidBackupset == 0)
		{
			va_remove(ABIOMASTER_CATALOG_DB_FOLDER, catalogLatestFile);
		}
	}
}

static void va_update_catalog_db_backupset_copy(char * backupset, int newRestoreBackupsetCopyNumber, char * media)
{
	va_fd_t fdCatalog;
	struct catalogDB catalogDB;
	
	
	
	if ((fdCatalog = va_open(ABIOMASTER_CATALOG_DB_FOLDER, backupset, O_RDWR, 1, 1)) != (va_fd_t)-1)
	{
		while (va_read(fdCatalog, &catalogDB, sizeof(struct catalogDB), DATA_TYPE_CATALOG_DB) > 0)
		{
			// restore backupset copy number�� �����Ѵ�.
			catalogDB.backupsetCopyNumber = newRestoreBackupsetCopyNumber;
			
			// restore backupset copy�� media�� �����Ѵ�.
			memset(catalogDB.volume, 0, sizeof(catalogDB.volume));
			strcpy(catalogDB.volume, media);
			
			// file�� media������ ��ġ�� ���õ� ������ ��� �����Ѵ�.
			catalogDB.backupsetPosition = 0;
			catalogDB.startBlockNumber = 0;
			catalogDB.endBlockNumber = 0;
			memset(catalogDB.endVolume, 0, sizeof(catalogDB.endVolume));
			
			va_lseek(fdCatalog, -(va_offset_t)sizeof(struct catalogDB), SEEK_CUR);
			va_write(fdCatalog, &catalogDB, sizeof(struct catalogDB), DATA_TYPE_CATALOG_DB);
			
			va_lseek(fdCatalog, (va_offset_t)(catalogDB.length + catalogDB.length2 + catalogDB.length3), SEEK_CUR);
		}
		
		va_close(fdCatalog);
	}
}

static void va_update_latest_catalog_db_backupset_copy(char * client, enum backupDataType backupData, char * backupset, int newRestoreBackupsetCopyNumber, char * media)
{
	va_fd_t fdCatalogLatest;
	struct catalogDB catalogDB;
	
	char catalogLatestFile[MAX_PATH_LENGTH];
	
	
	
	memset(catalogLatestFile, 0, sizeof(catalogLatestFile));
	if (va_get_latest_catalog_db_file_name(client, backupData, catalogLatestFile) != 0)
	{
		return;
	}
	
	if ((fdCatalogLatest = va_open(ABIOMASTER_CATALOG_DB_FOLDER, catalogLatestFile, O_RDWR, 1, 0)) != (va_fd_t)-1)
	{
		// expire�� backupset�� �ִ� ������ ã�Ƽ� restore backupset copy�� �����Ѵ�.
		while (va_read(fdCatalogLatest, &catalogDB, sizeof(struct catalogDB), DATA_TYPE_CATALOG_DB) > 0)
		{
			if (!strcmp(catalogDB.backupset, backupset))
			{
				// restore backupset copy number�� �����Ѵ�.
				catalogDB.backupsetCopyNumber = newRestoreBackupsetCopyNumber;
				
				// restore backupset copy�� media�� �����Ѵ�.
				memset(catalogDB.volume, 0, sizeof(catalogDB.volume));
				strcpy(catalogDB.volume, media);
				
				// file�� media������ ��ġ�� ���õ� ������ ��� �����Ѵ�.
				catalogDB.backupsetPosition = 0;
				catalogDB.startBlockNumber = 0;
				catalogDB.endBlockNumber = 0;
				memset(catalogDB.endVolume, 0, sizeof(catalogDB.endVolume));
				
				va_lseek(fdCatalogLatest, -(va_offset_t)sizeof(struct catalogDB), SEEK_CUR);
				va_write(fdCatalogLatest, &catalogDB, sizeof(struct catalogDB), DATA_TYPE_CATALOG_DB);
			}
			
			va_lseek(fdCatalogLatest, (va_offset_t)(catalogDB.length + catalogDB.length2 + catalogDB.length3), SEEK_CUR);
		}
		
		va_close(fdCatalogLatest);
	}
}

int va_get_backupset_list_file_name(char * client, enum backupDataType backupData, char * backupsetListFile)
{
	switch (backupData)
	{
		case ABIO_BACKUP_DATA_REGULAR_FILE :
			sprintf(backupsetListFile, "%s%s", ABIOMASTER_BACKUPSET_LIST_PREFIX, client);
			break;
			
		case ABIO_BACKUP_DATA_RAW_DEVICE :
			sprintf(backupsetListFile, "%s%s.%s", ABIOMASTER_BACKUPSET_LIST_PREFIX, client, ABIO_BACKUP_DATA_TYPE_INTERNAL_RAW_DEVICE);
			break;
			
		case ABIO_BACKUP_DATA_CATALOG_DB :
			sprintf(backupsetListFile, "%s%s.%s", ABIOMASTER_BACKUPSET_LIST_PREFIX, client, ABIO_BACKUP_DATA_TYPE_INTERNAL_CATALOG_DB);
			break;
			
		case ABIO_BACKUP_DATA_NDMP :
			sprintf(backupsetListFile, "%s%s.%s", ABIOMASTER_BACKUPSET_LIST_PREFIX, client, ABIO_BACKUP_DATA_TYPE_INTERNAL_NDMP);
			break;
			
		case ABIO_BACKUP_DATA_ORACLE :
			sprintf(backupsetListFile, "%s%s.%s", ABIOMASTER_BACKUPSET_LIST_PREFIX, client, ABIO_BACKUP_DATA_TYPE_INTERNAL_ORACLE);
			break;
			
		case ABIO_BACKUP_DATA_MSSQL :
			sprintf(backupsetListFile, "%s%s.%s", ABIOMASTER_BACKUPSET_LIST_PREFIX, client, ABIO_BACKUP_DATA_TYPE_INTERNAL_MSSQL);
			break;
			
		case ABIO_BACKUP_DATA_EXCHANGE :
			sprintf(backupsetListFile, "%s%s.%s", ABIOMASTER_BACKUPSET_LIST_PREFIX, client, ABIO_BACKUP_DATA_TYPE_INTERNAL_EXCHANGE);
			break;
			
		case ABIO_BACKUP_DATA_SAP :
			sprintf(backupsetListFile, "%s%s.%s", ABIOMASTER_BACKUPSET_LIST_PREFIX, client, ABIO_BACKUP_DATA_TYPE_INTERNAL_SAP);
			break;
			
		case ABIO_BACKUP_DATA_DB2 :
			sprintf(backupsetListFile, "%s%s.%s", ABIOMASTER_BACKUPSET_LIST_PREFIX, client, ABIO_BACKUP_DATA_TYPE_INTERNAL_DB2);
			break;
			
		case ABIO_BACKUP_DATA_INFORMIX :
			sprintf(backupsetListFile, "%s%s.%s", ABIOMASTER_BACKUPSET_LIST_PREFIX, client, ABIO_BACKUP_DATA_TYPE_INTERNAL_INFORMIX);
			break;
			
		case ABIO_BACKUP_DATA_SYBASE :
			sprintf(backupsetListFile, "%s%s.%s", ABIOMASTER_BACKUPSET_LIST_PREFIX, client, ABIO_BACKUP_DATA_TYPE_INTERNAL_SYBASE);
			break;

		case ABIO_BACKUP_DATA_SYBASE_IQ :
			sprintf(backupsetListFile, "%s%s.%s", ABIOMASTER_BACKUPSET_LIST_PREFIX, client, ABIO_BACKUP_DATA_TYPE_INTERNAL_SYBASE_IQ);
			break;
			
		case ABIO_BACKUP_DATA_MYSQL :
			sprintf(backupsetListFile, "%s%s.%s", ABIOMASTER_BACKUPSET_LIST_PREFIX, client, ABIO_BACKUP_DATA_TYPE_INTERNAL_MYSQL);
			break;
			
		case ABIO_BACKUP_DATA_ORACLE_RMAN :
			sprintf(backupsetListFile, "%s%s.%s", ABIOMASTER_BACKUPSET_LIST_PREFIX, client, ABIO_BACKUP_DATA_TYPE_INTERNAL_ORACLE_RMAN);
			break;
			
		case ABIO_BACKUP_DATA_NOTES :
			sprintf(backupsetListFile, "%s%s.%s", ABIOMASTER_BACKUPSET_LIST_PREFIX, client, ABIO_BACKUP_DATA_TYPE_INTERNAL_NOTES);
			break;
			
		case ABIO_BACKUP_DATA_UNISQL :
			sprintf(backupsetListFile, "%s%s.%s", ABIOMASTER_BACKUPSET_LIST_PREFIX, client, ABIO_BACKUP_DATA_TYPE_INTERNAL_UNISQL);
			break;
			
		case ABIO_BACKUP_DATA_ALTIBASE :
			sprintf(backupsetListFile, "%s%s.%s", ABIOMASTER_BACKUPSET_LIST_PREFIX, client, ABIO_BACKUP_DATA_TYPE_INTERNAL_ALTIBASE);
			break;
			
		case ABIO_BACKUP_DATA_EXCHANGE_MAILBOX :
			sprintf(backupsetListFile, "%s%s.%s", ABIOMASTER_BACKUPSET_LIST_PREFIX, client, ABIO_BACKUP_DATA_TYPE_INTERNAL_EXCHANGE_MAILBOX);
			break;

		case ABIO_BACKUP_DATA_VMWARE :
			sprintf(backupsetListFile, "%s%s.%s", ABIOMASTER_BACKUPSET_LIST_PREFIX, client, ABIO_BACKUP_DATA_TYPE_INTERNAL_VMWARE);
			break;

		case ABIO_BACKUP_DATA_INFORMIX_ONBAR :
			sprintf(backupsetListFile, "%s%s.%s", ABIOMASTER_BACKUPSET_LIST_PREFIX, client, ABIO_BACKUP_DATA_TYPE_INTERNAL_INFORMIX_ONBAR);

		case ABIO_BACKUP_DATA_TIBERO :
			sprintf(backupsetListFile, "%s%s.%s", ABIOMASTER_BACKUPSET_LIST_PREFIX, client, ABIO_BACKUP_DATA_TYPE_INTERNAL_TIBERO);
			break;

		case ABIO_BACKUP_DATA_ORACLE12C :
			sprintf(backupsetListFile, "%s%s.%s", ABIOMASTER_BACKUPSET_LIST_PREFIX, client, ABIO_BACKUP_DATA_TYPE_INTERNAL_ORACLE12C);
			break;

		case ABIO_BACKUP_DATA_SQLBACKTRACK :
			sprintf(backupsetListFile, "%s%s.%s", ABIOMASTER_BACKUPSET_LIST_PREFIX, client, ABIO_BACKUP_DATA_TYPE_INTERNAL_SQLBACKTRACK);
			break;

		case ABIO_BACKUP_DATA_SHAREPOINT :
			sprintf(backupsetListFile, "%s%s.%s", ABIOMASTER_BACKUPSET_LIST_PREFIX, client, ABIO_BACKUP_DATA_TYPE_INTERNAL_SHAREPOINT);
			break;

		case ABIO_BACKUP_DATA_OS_BACKUP :
			sprintf(backupsetListFile, "%s%s.%s", ABIOMASTER_BACKUPSET_LIST_PREFIX, client, ABIO_BACKUP_DATA_TYPE_INTERNAL_PHYSICAL);
			break;

		default :
			return -1;
	}
	
	return 0;
}

int va_get_latest_catalog_db_file_name(char * client, enum backupDataType backupData, char * catalogFile)
{
	switch (backupData)
	{
		case ABIO_BACKUP_DATA_REGULAR_FILE :
			sprintf(catalogFile, "%s%s", ABIOMASTER_CATALOG_DB_PREFIX, client);
			break;
			
		case ABIO_BACKUP_DATA_RAW_DEVICE :
			sprintf(catalogFile, "%s%s.%s", ABIOMASTER_CATALOG_DB_PREFIX, client, ABIO_BACKUP_DATA_TYPE_INTERNAL_RAW_DEVICE);
			break;
			
		case ABIO_BACKUP_DATA_CATALOG_DB :
			sprintf(catalogFile, "%s%s.%s", ABIOMASTER_CATALOG_DB_PREFIX, client, ABIO_BACKUP_DATA_TYPE_INTERNAL_CATALOG_DB);
			break;
			
		case ABIO_BACKUP_DATA_NDMP :
			sprintf(catalogFile, "%s%s.%s", ABIOMASTER_CATALOG_DB_PREFIX, client, ABIO_BACKUP_DATA_TYPE_INTERNAL_NDMP);
			break;
			
		case ABIO_BACKUP_DATA_SAP :
			sprintf(catalogFile, "%s%s.%s", ABIOMASTER_CATALOG_DB_PREFIX, client, ABIO_BACKUP_DATA_TYPE_INTERNAL_SAP);
			break;
			
		case ABIO_BACKUP_DATA_NOTES :
			sprintf(catalogFile, "%s%s.%s", ABIOMASTER_CATALOG_DB_PREFIX, client, ABIO_BACKUP_DATA_TYPE_INTERNAL_NOTES);
			break;

		case ABIO_BACKUP_DATA_SHAREPOINT :
			sprintf(catalogFile, "%s%s.%s", ABIOMASTER_CATALOG_DB_PREFIX, client, ABIO_BACKUP_DATA_TYPE_INTERNAL_SHAREPOINT);
			break;

		case ABIO_BACKUP_DATA_VMWARE :
			sprintf(catalogFile, "%s%s.%s", ABIOMASTER_CATALOG_DB_PREFIX, client, ABIO_BACKUP_DATA_TYPE_INTERNAL_VMWARE);
			break;

		case ABIO_BACKUP_DATA_OS_BACKUP :
			sprintf(catalogFile, "%s%s.%s", ABIOMASTER_CATALOG_DB_PREFIX, client, ABIO_BACKUP_DATA_TYPE_INTERNAL_PHYSICAL);
			break;
			
		default :
			return -1;
	}
	
	return 0;
}

// Catalog DB file���� �о�� ���� ������ Catalog DB�� �߰�
int va_add_catalog_tag(struct catalogTag * catalogTag, unsigned int index, struct catalogDB * catalogDB, char * path)
{
	catalogTag[index].version = catalogDB->version;
	catalogTag[index].length = catalogDB->length + catalogDB->length2;
	
	catalogTag[index].offset = catalogDB->offset;
	catalogTag[index].mtime = catalogDB->st.mtime;
	catalogTag[index].ctime = catalogDB->st.ctime;
	
	#ifdef __ABIO_NOTES_SUPPORTED
		if (catalogDB->st.filetype == NOTES_DATABASE || catalogDB->st.filetype == NOTES_TRANSACTION_LOG)
		{
			memcpy(&catalogTag[index].DbIID, &catalogDB->st.DbIID, sizeof(VA_NOTES_UNID));
			memcpy(&catalogTag[index].NotesMtime, &catalogDB->st.NotesMtime, sizeof(VA_NOTES_TIMEDATE));
		}
	#endif
	
	catalogTag[index].path = (char *)malloc(sizeof(char) * (catalogDB->length+catalogDB->length2 + 1));
	strcpy(catalogTag[index].path, path);
	catalogTag[index].path[catalogDB->length + catalogDB->length2] = '\0';
	
	return index;
}

// comparison function sorting catalog db
// Catalog DB���� key��(file name and table space name)�� ���� ������ ã�´�.
int va_search_catalog_tag(struct catalogTag * catalogTag, unsigned int index, char * path)
{
	int middle;
	int left;
	int right;
	
	int r;
	
	
	
	middle = 0;
	left = 0;
	right = index - 1;
	
	while (right >= left)
	{
		middle = (right + left) / 2;
		
		#ifdef __ABIO_UNIX
			// unix�� ��� file name�� ���� �� case sensitive�ϰ� ���ϰ�
			r = strcmp(path, catalogTag[middle].path);
		#elif __ABIO_WIN
			// windows�� ��� file name�� ���� �� case insensitive�ϰ� ���Ѵ�.
			r = _stricmp(path, catalogTag[middle].path);
		#endif
		
		if (r < 0)
		{
			right = middle - 1;
		}
		else if (r > 0)
		{
			left = middle + 1;
		}
		else
		{
			return middle;
		}
	}
	
	return -1;
}

// catalog tag sorting function
int va_compare_catalog_tag(const void * a1, const void * a2)
{
	struct catalogTag * c1;
	struct catalogTag * c2;
	
	int r;
	
	
	
	c1 = (struct catalogTag *)a1;
	c2 = (struct catalogTag *)a2;
	
	
	#ifdef __ABIO_UNIX
		// unix�� ��� file name�� ���� �� case sensitive�ϰ� ���ϰ�
		r = strcmp(c1->path, c2->path);
	#elif __ABIO_WIN
		// windows�� ��� file name�� ���� �� case insensitive�ϰ� ���Ѵ�.
		r = _stricmp(c1->path, c2->path);
	#endif
	
	if (r > 0)
	{
		return 1;
	}
	else if (r < 0)
	{
		return -1;
	}
	else
	{
		return 0;
	}
}

// catalog db ������ ���ռ� �˻縦 �Ѵ�.
int va_check_coordination_catalogdb(char * catalogPath, char * catalogFile)
{
	va_fd_t fdCatalog;
	struct catalogDB catalogDB;
	
	int r;
	
	
	
	if ((fdCatalog = va_open(catalogPath, catalogFile, O_RDONLY, 1, 1)) != (va_fd_t)-1)
	{
		while (va_read(fdCatalog, &catalogDB, sizeof(struct catalogDB), DATA_TYPE_CATALOG_DB) > 0)
		{
			if ((r = CheckCoordinationCatalogDB(&catalogDB)) < 0)
			{
				va_close(fdCatalog);
				
				return r;
			}
			else
			{
				va_lseek(fdCatalog, (va_offset_t)(catalogDB.length + catalogDB.length2 + catalogDB.length3), SEEK_CUR);
			}
		}
		
		va_close(fdCatalog);
	}
	else
	{
		return 1;
	}
	
	
	return 0;
}

// catalog db�� data�� ������ �������� �ִ��� Ȯ���Ѵ�.
static int CheckCoordinationCatalogDB(struct catalogDB * catalogDB)
{
	if (catalogDB->version != AM_1_2 && catalogDB->version != AM_1_4 && catalogDB->version != AM_2_0 && catalogDB->version != VA_2_2 && 
		catalogDB->version != VA_2_3 && catalogDB->version != VA_2_4 && catalogDB->version != VA_2_5 && catalogDB->version != VA_2_5_1 && catalogDB->version != VA_2_5_2 && 
		catalogDB->version != VA_3_0_0 && catalogDB->version != VA_3_0_1)
	{
		return -2;
	}
	
	if (catalogDB->backupsetCopyNumber < 0 || catalogDB->backupsetCopyNumber > BACKUPSET_COPY_NUMBER)
	{
		return -3;
	}
	
	if (catalogDB->length < 0 || catalogDB->length > ABIO_FILE_LENGTH * 2)
	{
		return -4;
	}
	
	if (catalogDB->length2 < 0 || catalogDB->length2 > ABIO_FILE_LENGTH)
	{
		return -5;
	}
	
	if (catalogDB->length3 < 0 || catalogDB->length3 > ABIO_FILE_LENGTH)
	{
		return -6;
	}
	
	if (catalogDB->expire != 0 && catalogDB->expire != 1)
	{
		return -7;
	}
	/*
	if (catalogDB->compress < 0 || catalogDB->compress > 9)
	{
		return -8;
	}
	*/
	switch (catalogDB->st.filetype)
	{
		case ORACLE_REGULAR_DATA_FILE : 
		case ORACLE_RAW_DEVICE_DATA_FILE : 
		case ORACLE_ARCHIVE_LOG : 
		case ORACLE_CONTROL_FILE : 
			if (catalogDB->dbBackupMethod < ABIO_DB_BACKUP_METHOD_ORACLE_FULL || catalogDB->dbBackupMethod > ABIO_DB_BACKUP_METHOD_ORACLE_INCREMENTAL)
			{
				return -9;
			}
			break;
		
		case MSSQL_DATABASE_FULL : 
		case MSSQL_DATABASE_DIFFERENTIAL : 
		case MSSQL_LOG : 
		case MSSQL_FILEGROUP : 
		case MSSQL_FILE : 
			if (catalogDB->dbBackupMethod < ABIO_DB_BACKUP_METHOD_MSSQL_DB_FULL || catalogDB->dbBackupMethod > ABIO_DB_BACKUP_METHOD_MSSQL_DB_FILE)
			{
				return -9;
			}
			break;
		
		case EXCHANGE_DATABASE_FILE : 
		case EXCHANGE_LOG_FILE : 
			if (catalogDB->dbBackupMethod < ABIO_DB_BACKUP_METHOD_EXCHANGE_STORAGE_GROUP_FULL || catalogDB->dbBackupMethod > ABIO_DB_BACKUP_METHOD_EXCHANGE_STORE_FULL)
			{
				return -9;
			}
			break;
		
		case SYBASE_DATABASE : 
		case SYBASE_TRANSACTION : 
			if (catalogDB->dbBackupMethod < ABIO_DB_BACKUP_METHOD_SYBASE_DATABASE || catalogDB->dbBackupMethod > ABIO_DB_BACKUP_METHOD_SYBASE_TRANSACTION)
			{
				return -9;
			}
			break;
		case SYBASE_IQ_DATABASE : 
		case SYBASE_IQ_TRANSACTION : 
			if (catalogDB->dbBackupMethod < ABIO_DB_BACKUP_METHOD_SYBASE_IQ_DATABASE || catalogDB->dbBackupMethod > ABIO_DB_BACKUP_METHOD_SYBASE_IQ_TRANSACTION)
			{
				return -9;
			}
			break;
		case DB2_DATABASE_OFFLINE_FULL:
		case DB2_DATABASE_OFFLINE_INCREMENTAL:
		case DB2_DATABASE_OFFLINE_INCREMENTAL_DELTA:
		case DB2_DATABASE_ONLINE_FULL:
		case DB2_DATABASE_ONLINE_INCREMENTAL:
		case DB2_DATABASE_ONLINE_INCREMENTAL_DELTA:
		case DB2_TABLESPACE_OFFLINE_FULL:
		case DB2_TABLESPACE_OFFLINE_INCREMENTAL:
		case DB2_TABLESPACE_OFFLINE_INCREMENTAL_DELTA:
		case DB2_TABLESPACE_ONLINE_FULL:
		case DB2_TABLESPACE_ONLINE_INCREMENTAL:
		case DB2_TABLESPACE_ONLINE_INCREMENTAL_DELTA:
		case DB2_LOG:
			if (catalogDB->dbBackupMethod < ABIO_DB_BACKUP_METHOD_DB2_DATABASE_OFFLINE_FULL || catalogDB->dbBackupMethod > ABIO_DB_BACKUP_METHOD_DB2_LOG)
			{
				return -9;
			}
			break;
			
		case ALTIBASE_DATA_FILE : 
		case ALTIBASE_ARCHIVE_LOG : 
		case ALTIBASE_LOG_ANCHOR : 
			if (catalogDB->dbBackupMethod < ABIO_DB_BACKUP_METHOD_ALTIBASE_FULL || catalogDB->dbBackupMethod > ABIO_DB_BACKUP_METHOD_ALTIBASE_INCREMENTAL)
			{
				return -9;
			}
			break;

		case EXCHANGE_MAILBOX_FOLDER : 
		case EXCHANGE_MAILBOX_MESSAGE :
			if (catalogDB->dbBackupMethod < ABIO_DB_BACKUP_METHOD_EXCHANGE_MAILBOX_FULL || catalogDB->dbBackupMethod > ABIO_DB_BACKUP_METHOD_EXCHANGE_MAILBOX_SIS)
			{
				return -9;
			}
			break;

		case NOTES_DATABASE_UNLOGGED :
		case NOTES_DATABASE_LOGGED_ARCHIVE :
		case NOTES_DATABASE_LOGGED_CIRCULAR :
		case NOTES_DATABASE_LOGGED_LINEAR :
		case NOTES_DATABASE_CHANGE_INFO :
		case NOTES_TRANSACTION_LOG :
		case NOTES_DATABASE_TEMPLATE :
		case NOTES_MAIL_BOX :
		case NOTES_SYMBOLIC_LINK : 
			if (catalogDB->dbBackupMethod < ABIO_DB_BACKUP_METHOD_NOTES_FULL || catalogDB->dbBackupMethod > ABIO_DB_BACKUP_METHOD_NOTES_INCREMENTAL)
			{
				return -9;
			}
			break;

		case ORACLE_RMAN_TABLESPACE :
		case ORACLE_RMAN_ARCHIVE_LOG :
		case ORACLE_RMAN_CONTROL_FILE :
		case ORACLE_RMAN_SPFILE :
			if (catalogDB->dbBackupMethod < ABIO_DB_BACKUP_METHOD_ORACLE_RMAN_FULL || catalogDB->dbBackupMethod > ABIO_DB_BACKUP_METHOD_ORACLE_RMAN_INCREMENTAL)
			{
				return -9;
			}
			break;

		default:
			break;
	}
	
	return 0;
}

int va_read_catalog_db(va_fd_t fdCatalog, struct catalogDB * readCatalogDB, char * readBackupFile1, int readBackupFile1Size, char * readBackupFile2, int readBackupFile2Size, char * readBackupFile3, int readBackupFile3Size)
{
	struct catalogDB catalogDB;
	char backupFile1[ABIO_FILE_LENGTH * 2];
	char backupFile2[ABIO_FILE_LENGTH * 2];
	char backupFile3[ABIO_FILE_LENGTH * 2];
	
	char * convertBuffer;
	int convertBufferSize;
	int convertSize;
	
	
	
	// initialize variables
	memset(backupFile1, 0, sizeof(backupFile1));
	memset(backupFile2, 0, sizeof(backupFile2));
	memset(backupFile3, 0, sizeof(backupFile3));
	
	if (readCatalogDB != NULL)
	{
		memset(readCatalogDB, 0, sizeof(struct catalogDB));
	}
	
	if (readBackupFile1 != NULL && readBackupFile1Size != 0)
	{
		memset(readBackupFile1, 0, readBackupFile1Size);
	}
	
	if (readBackupFile2 != NULL && readBackupFile2Size != 0)
	{
		memset(readBackupFile2, 0, readBackupFile2Size);
	}
	
	if (readBackupFile3 != NULL && readBackupFile3Size != 0)
	{
		memset(readBackupFile3, 0, readBackupFile3Size);
	}
	
	
	if (va_read(fdCatalog, &catalogDB, sizeof(struct catalogDB), DATA_TYPE_CATALOG_DB) == sizeof(struct catalogDB))
	{
		if (va_read(fdCatalog, backupFile1, catalogDB.length, DATA_TYPE_NOT_CHANGE) != catalogDB.length)
		{
			return -1;
		}
		
		if (va_read(fdCatalog, backupFile2, catalogDB.length2, DATA_TYPE_NOT_CHANGE) != catalogDB.length2)
		{
			return -1;
		}
		
		if (va_read(fdCatalog, backupFile3, catalogDB.length3, DATA_TYPE_NOT_CHANGE) != catalogDB.length3)
		{
			return -1;
		}
		
		if (catalogDB.version == VA_2_5_1 || catalogDB.version == VA_2_5_2)
		{
			if (readCatalogDB != NULL)
			{
				memcpy(readCatalogDB, &catalogDB, sizeof(struct catalogDB));
			}
			
			if (readBackupFile1 != NULL && backupFile1[0] != '\0')
			{
				if ((convertSize = va_convert_string_to_utf8(ENCODING_CP949, backupFile1, (int)strlen(backupFile1), (void **)&convertBuffer, &convertBufferSize)) > 0)
				{
					strcpy(readBackupFile1, convertBuffer);
					
					va_free(convertBuffer);
				}
				else
				{
					return -1;
				}
			}
			
			if (readBackupFile2 != NULL && backupFile2[0] != '\0')
			{
				if ((convertSize = va_convert_string_to_utf8(ENCODING_CP949, backupFile2, (int)strlen(backupFile2), (void **)&convertBuffer, &convertBufferSize)) > 0)
				{
					strcpy(readBackupFile2, convertBuffer);
					
					va_free(convertBuffer);
				}
				else
				{
					return -1;
				}
			}
			
			if (readBackupFile3 != NULL && backupFile3[0] != '\0')
			{
				if ((convertSize = va_convert_string_to_utf8(ENCODING_CP949, backupFile3, (int)strlen(backupFile3), (void **)&convertBuffer, &convertBufferSize)) > 0)
				{
					strcpy(readBackupFile3, convertBuffer);
					
					va_free(convertBuffer);
				}
				else
				{
					return -1;
				}
			}
		}
		else
		{
			if (readCatalogDB != NULL)
			{
				memcpy(readCatalogDB, &catalogDB, sizeof(struct catalogDB));
			}
			
			if (readBackupFile1 != NULL)
			{
				strcpy(readBackupFile1, backupFile1);
			}
			
			if (readBackupFile2 != NULL)
			{
				strcpy(readBackupFile2, backupFile2);
			}
			
			if (readBackupFile3 != NULL)
			{
				strcpy(readBackupFile3, backupFile3);
			}
		}
	}
	else
	{
		return -1;
	}
	
	
	return 0;
}

int va_Shrink_Catalog(char * catalogFolder , char * catalogFileName , int option, int * totalcnt , int * expirecnt , int * deletecnt , __uint64 * decreasefilesize)
{
	va_fd_t fdCatalogLatest;
	va_fd_t fdCatalogshrink;
	struct catalogDB catalogDB;
	char backupFilePath[ABIO_FILE_LENGTH * 2];
	char showbackupFilePath[ABIO_FILE_LENGTH * 2];
	char tmpbuf[sizeof(struct catalogDB) + ABIO_FILE_LENGTH * 2];
	
	int bufsize;

	char * ABIOMASTER_CATALOG_DB_NEW = "newcatalog.db";
	char * ABIOMASTER_CATALOG_DB_BAK = ".bak";

	//int totalcnt;
	//int expirecnt;
	//int deletecnt;
	//__uint64 decreasefilesize;
	int showlog;

	int flag = 0;

	char catalogDBFileName[MAX_PATH_LENGTH];
	char ABIOMASTER_CATALOG_DB_FILE_BAK[MAX_PATH_LENGTH];

	*totalcnt = 0;
	*expirecnt = 0;
	*deletecnt = 0;
	*decreasefilesize = 0;
	
	showlog = 0;
	
	/*memset(ABIOMASTER_CLIENT_CATALOG_DB_FOLDER, 0, sizeof(ABIOMASTER_CLIENT_CATALOG_DB_FOLDER));
	strncpy(ABIOMASTER_CLIENT_CATALOG_DB_FOLDER, ABIOMASTER_DEFATIL_CLIENT_CATALOG_DB_FOLDER, strlen(ABIOMASTER_DEFATIL_CLIENT_CATALOG_DB_FOLDER));*/
	
	memset(catalogDBFileName, 0, sizeof(catalogDBFileName));
	strncpy(catalogDBFileName, catalogFileName, strlen(catalogFileName));
	
	if ((fdCatalogLatest = va_open(catalogFolder, catalogDBFileName, O_RDONLY, 1, 0)) != (va_fd_t)-1)
	{
		// newcatalog.db ������ ������ �����.
		va_remove(catalogFolder, ABIOMASTER_CATALOG_DB_NEW);

		// newcatalog.db ���� ����	
		if ((fdCatalogshrink = va_open(catalogFolder, ABIOMASTER_CATALOG_DB_NEW, O_CREAT|O_RDWR, 1, 1)) == (va_fd_t)-1)
		{			
			return -1;
		}

		while (va_read(fdCatalogLatest, &catalogDB, sizeof(struct catalogDB), DATA_TYPE_CATALOG_DB) > 0)
		{
			(*totalcnt)++;

			if(!showlog)
			{
				if((*totalcnt)%100000 == 0)
				{
					//printf("Data processing... %d\n", totalcnt);
				}
			}

			memset(backupFilePath, 0, sizeof(backupFilePath));
			va_read(fdCatalogLatest, backupFilePath, catalogDB.length, DATA_TYPE_NOT_CHANGE);

			memset(showbackupFilePath, 0, sizeof(showbackupFilePath));
			memcpy(showbackupFilePath, backupFilePath, catalogDB.length);
			#ifdef __ABIO_WIN
				// catalog db���� unix file format���� ����Ǿ��ֱ� ������ windows file format���� ��ȯ�Ѵ�.
				va_slash_to_backslash(showbackupFilePath);
			#endif
		
			// expire���� ���� ���ϰ� delete���� ���� ���ϸ� �о�´�.
			if (catalogDB.expire == 0 && catalogDB.st.deletedFile != 1)
			{	
				memset(tmpbuf, 0, sizeof(tmpbuf));
				bufsize = 0;
				
				va_change_byte_order_catalogDB(&catalogDB);		// host to network
				memcpy(tmpbuf + bufsize, &catalogDB, sizeof(struct catalogDB));
				bufsize += sizeof(struct catalogDB);
				va_change_byte_order_catalogDB(&catalogDB);		// network to host
				
				memcpy(tmpbuf + bufsize, backupFilePath, catalogDB.length);
				bufsize += catalogDB.length;
				
				if (va_write(fdCatalogshrink, tmpbuf, bufsize, DATA_TYPE_NOT_CHANGE) < 0)
				{
					//printf("Can not write a new catalog database.\n");
					return -2;
				}

				//���ο� catalog�� write�ϴ� file catalog. ȭ���� ������������ �ּ�
				//printf("%s\n", showbackupFilePath);
			}
			else if(catalogDB.expire == 1)
			{
				(*expirecnt)++;
				(*decreasefilesize) += (sizeof(struct catalogDB) + catalogDB.length);

				if(showlog)
				{
					//WriteDebugData("c:\\","san.log","Expired catalog(%s)", showbackupFilePath);
				}
			}
			else if(catalogDB.st.deletedFile == 1)
			{
				(*deletecnt)++;
				(*decreasefilesize) += (sizeof(struct catalogDB) + catalogDB.length);
				
				if(showlog)
				{
					//WriteDebugData("c:\\","san.log","Deleted catalog(%s)", showbackupFilePath);
				}
			}
		}		
		va_close(fdCatalogLatest);
		va_close(fdCatalogshrink);
	}
	else	
	{
		//memset(ABIOMASTER_CLIENT_CATALOG_DB_FOLDER, 0, sizeof(ABIOMASTER_CATALOG_DB_FOLDER));		
		return -3;
	}

	if ((*expirecnt) + (*deletecnt) == 0)
	{
		va_remove(catalogFolder, ABIOMASTER_CATALOG_DB_NEW);
		return 1;
	}
		
	if (option == 1)
	{
		//catalog.db ����
		//printf("Delete File : \"%s\"\n", ABIOMASTER_CATALOG_DB_FILE);
		va_remove(catalogFolder, catalogDBFileName);
		//newcatalog.db -> catalog.db �̸�����
		//printf("Rename File : \"%s\" --> \"%s\"\n", ABIOMASTER_CATALOG_DB_NEW, ABIOMASTER_CATALOG_DB_FILE);
		va_rename(catalogFolder, ABIOMASTER_CATALOG_DB_NEW, catalogFolder, catalogDBFileName);		
	}
	else 
	{		
		memset(ABIOMASTER_CATALOG_DB_FILE_BAK, 0, MAX_PATH_LENGTH);
		sprintf(ABIOMASTER_CATALOG_DB_FILE_BAK, "%s%s", catalogDBFileName, ABIOMASTER_CATALOG_DB_BAK);

		//catalog.db -> catalog.db.bak �̸�����
		//printf("Rename File : \"%s\" --> \"%s\"\n", ABIOMASTER_CATALOG_DB_FILE, ABIOMASTER_CATALOG_DB_FILE_BAK);
		va_rename(catalogFolder, catalogDBFileName, catalogFolder, ABIOMASTER_CATALOG_DB_FILE_BAK);
		//newcatalog.db -> catalog.db �̸�����
		//printf("Rename File : \"%s\" --> \"%s\"\n", ABIOMASTER_CATALOG_DB_NEW, ABIOMASTER_CATALOG_DB_FILE);
		va_rename(catalogFolder, ABIOMASTER_CATALOG_DB_NEW, catalogFolder, catalogDBFileName);		
	}	

	return 1;
}
