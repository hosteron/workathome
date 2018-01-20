#!/bin/sh

#version:0.0.1
#release data:2017-11-15
#modified by xiang.zhou

#set -x

MODULE=fw0
FW_PATH=/data/fw
MDS_SVR_FW_META_FILE=${FW_PATH}/srv_fw_meta.json

source /usr/bin/factory_fun

mkdir -p /data/log/
echo `date`  : start > ${FW_START_MAKR_FILE} && sync

#step 1:check battery status

source /etc/conf/medusa/get_battery_status_hook.sh

#setp 2: parse server meta file,and check fw version
SERV_META_CONTEXT=`cat ${MDS_SVR_FW_META_FILE}`

NEW_FW_VER=`cjp "${SERV_META_CONTEXT}" string version`
NEW_FW_SIZE=`cjp "${SERV_META_CONTEXT}" int "${MODULE}".size`
NEW_FW_CHECKSUM=`cjp "${SERV_META_CONTEXT}" string "${MODULE}".checkSum`
NEW_FW_URL=`cjp "${SERV_META_CONTEXT}" string "${MODULE}".url`
if [ -z $NEW_FW_VER ] || [ -z $NEW_FW_SIZE ] || [ -z $NEW_FW_CHECKSUM ] || [ -z $NEW_FW_URL ];then
    mdsctl "fw" '{"todo": "status", "status": "MDS_FW_DL_FW_FINISHED", "result": -1, "error_code":20,"error_msg": "download fw failed"}'
#    mv ${FW_MAKR_FILE} ${FW_MAKR_FILE}.tmp && rm ${FW_MAKR_FILE}.tmp
    exit 1	
fi

#step 3: download new fw
rm -f ${MDS_FW_FILE_NAME}
wget -T 60 -O ${MDS_FW_FILE_NAME}_ ${NEW_FW_URL} && mv ${MDS_FW_FILE_NAME}_ ${MDS_FW_FILE_NAME}
if [ $? -ne 0 ]; then
    mdsctl "fw" '{"todo": "status", "status": "MDS_FW_DL_FW_FINISHED", "result": -1, "error_code":20,"error_msg": "download fw failed"}'
#    mv ${FW_MAKR_FILE} ${FW_MAKR_FILE}.tmp && rm ${FW_MAKR_FILE}.tmp
    exit 1
else
	#checksum
	FW_MD5SUM=`md5sum ${MDS_FW_FILE_NAME} | cut -d ' ' -f1`

	if [[ ${FW_MD5SUM} == ${NEW_FW_CHECKSUM} ]];then
	        echo "md5sum check success!!!";
	else
	        echo "md5sum check fail!!!";
		mdsctl "fw" '{"todo": "status", "status": "MDS_FW_DL_FW_FINISHED", "result": -1, "error_code":20,"error_msg": "download fw failed"}'
#		mv ${FW_MAKR_FILE} ${FW_MAKR_FILE}.tmp && rm ${FW_MAKR_FILE}.tmp
    		exit 1
	fi
		
	
	mdsctl "fw" '{"todo": "status", "status": "MDS_FW_DL_FW_FINISHED", "result": 0}'	
	exit 0
fi
