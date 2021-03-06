#!/bin/sh

NLS_LANG=ENGLISH
export NLS_LANG

NLS_DATE_FORMAT=YYYY-MM-DD:hh24:mi:ss
export NLS_DATE_FORMAT

ORACLE_HOME=$1
RMAN=$ORACLE_HOME/bin/rman

ORACLE_SID=$2
ORACLE_ACCOUNT=$3
ORACLE_PASSWORD=$4

RESTORE_PORT_NUM=$5

RESTORE_TABLESPACE_NAME=$6
RESTORE_UNTIL_TIME=$7
RESTORE_FILELIST_PATH=$8

LOG_PATH=$9

echo "RUN {
SET UNTIL TIME '$RESTORE_UNTIL_TIME';
ALLOCATE CHANNEL ch00 TYPE 'SBT_TAPE';
SEND CHANNEL 'ch00' \"JOB_TYPE=RESTORE\";
SEND CHANNEL 'ch00' \"PORT_NUM=$RESTORE_PORT_NUM\";
SEND CHANNEL 'ch00' \"FILELIST_PATH=$RESTORE_FILELIST_PATH\";
RESTORE TABLESPACE $RESTORE_TABLESPACE_NAME;
RECOVER TABLESPACE $RESTORE_TABLESPACE_NAME;
RELEASE CHANNEL ch00;
}" | $RMAN target $ORACLE_ACCOUNT/$ORACLE_PASSWORD@$ORACLE_SID nocatalog log "$LOG_PATH"
