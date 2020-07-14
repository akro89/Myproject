#ifndef __ORALIB12C_H__
#define __ORALIB12C_H__


#define ORACLE12C_QUERY_LINE_NUMBER			5

#define ORACLE12C_LINESIZE_QUERY			"set linesize 1024"
#define ORACLE12C_SPOOL_ON_QUERY			"spool %s%c%s"
#define ORACLE12C_CONNECT_QUERY				"connect %s/%s"
#define ORACLE12C_SPOOL_OFF_QUERY			"spool off"
#define ORACLE12C_QUIT_QUERY				"quit"
#define GET_TABLE_SPACE_QUERY				"select c.name || '^' || a.name || '^' || b.name || '^' from v$tablespace a, v$datafile b, v$containers c where (c.con_id = b.con_id ) and (c.con_id = a.con_id) and (a.ts# = b.ts#);"
#define GET_ARCHIVE_LOG_DIR_QUERY			"select destination || '^' from v$archive_dest;"
#define GET_CONTROL_FILE_QUERY				"select name || '^' from v$controlfile;"
#define GET_ARCHIVE_LOG_MODE_QUERY			"select log_mode || '^' from v$database;"
#define GET_DATABASE_OPEN_MODE_QUERY		"select open_mode || '^' from v$containers where name='%s';"
#define BEGIN_BACKUP_QUERY					"ALTER TABLESPACE \"%s\" BEGIN BACKUP;"
#define END_BACKUP_QUERY					"ALTER TABLESPACE \"%s\" END BACKUP;"
#define SET_SESSION_CONTAINER				"ALTER SESSION SET CONTAINER=%s;"
#define SWITCH_LOG_FILE_QUERY				"ALTER SYSTEM SWITCH LOGFILE;"
#define ALTER_CONTROL_FILE_BINARY_QUERY		"ALTER DATABASE BACKUP CONTROLFILE TO '%s';"
#define ALTER_CONTROL_FILE_TRACE_QUERY		"ALTER DATABASE BACKUP CONTROLFILE TO TRACE AS '%s';"


#ifdef __ABIO_AIX
	__uint64 GetRawDeviceFileSizeAIX(char * file, struct abio_file_stat * st);
#elif __ABIO_SOLARIS
	__uint64 GetRawDeviceFileSizeSolaris(char * file, struct abio_file_stat * st);
#elif __ABIO_HP
	__uint64 GetRawDeviceFileSizeHP(char * file, struct abio_file_stat * st);
#elif __ABIO_Linux
	__uint64 GetRawDeviceFileSizeLinux(char * file, struct abio_file_stat * st);
#endif


/**************************************************************************************************
 * function prototype : 
 * DESCRIPTION
 *
 * ARGUMENTS
 *
 * RETURN VALUES
 *************************************************************************************************/


#endif
