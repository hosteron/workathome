#!/bin/sh

#version:0.0.1
#release data:2017-11-15
#modified by xiang.zhou

#set -x 

PLATFORM=QJ2077
MODULE=fw0
FW_VERSION=VERSION
FW_STATUS=STATUS
FW_PATH=/tmp/fw
#BASE_URL=http://dl.ecouser.net:8005/products/wukong/class
#BASE_URL=http://${DOMAIN_NAME}:8005/products/wukong/class
BASE_URL=http://192.168.36.193:8000
MDS_SVR_FW_META_FILE=${FW_PATH}/srv_fw_meta.json

source /usr/bin/factory_fun

#step 1:download server meta file

mkdir -p ${FW_PATH}
mret=`mdsctl sys0 "{\"pid_get\":\"get_all\"}"`
SN=`cjp "${mret}" string sn`
#MODEL=`cjp "${mret}" string dev_class`
MODEL=161
CURRENT_FW_VER=`cjp "${mret}" string fw_arm`
MAC_ADDR=`ifconfig |grep wlan0|awk '{print $5}'`
#MDS_SVR_FW_META_URI=${BASE_URL}/${MODEL}/firmware/latest.json?sn=${SN}\&ver=${CURRENT_FW_VER}\&mac=${MAC_ADDR}\&plat=${PLATFORM}\&module=${MODULE}
MDS_SVR_FW_META_URI=${BASE_URL}/latest.json
rm -f ${MDS_SVR_FW_META_FILE}
wget -T 60 -O ${MDS_SVR_FW_META_FILE}_ ${MDS_SVR_FW_META_URI} && mv ${MDS_SVR_FW_META_FILE}_ ${MDS_SVR_FW_META_FILE}
if [ $? -ne 0 ]; then
#    mdsctl "fw" '{"todo": "status", "status": "MDS_FW_DL_FW_FINISHED", "result": -1, "error_code":20,"error_msg": "download fw failed"}'
    exit 1
fi

#setp 2: parse server meta file,and check fw version
SERV_META_CONTEXT=`cat ${MDS_SVR_FW_META_FILE}`

NEW_FW_VER=`cjp "${SERV_META_CONTEXT}" string version`
NEW_FW_SIZE=`cjp "${SERV_META_CONTEXT}" int "${MODULE}".size`
NEW_FW_CHECKSUM=`cjp "${SERV_META_CONTEXT}" string "${MODULE}".checkSum`
NEW_FW_URL=`cjp "${SERV_META_CONTEXT}" string "${MODULE}".url`
if [ -z $NEW_FW_VER ] || [ -z $NEW_FW_SIZE ] || [ -z $NEW_FW_CHECKSUM ] || [ -z $NEW_FW_URL ];then
#    mdsctl "fw" '{"todo": "status", "status": "MDS_FW_DL_FW_FINISHED", "result": -1, "error_code":20,"error_msg": "download fw failed"}'
#    mv ${FW_MAKR_FILE} ${FW_MAKR_FILE}.tmp && rm ${FW_MAKR_FILE}.tmp
    exit 1	
fi

#step 3:check fw version
if [ _"$CURRENT_FW_VER" != _"$NEW_FW_VER" ];then
	if [ -f ${FW_PATH}/${FW_VERSION} ];then
		TMP_VERSION=`cat ${FW_PATH}/${FW_VERSION}`
		if [ _"$TMP_VERSION" != _"$NEW_FW_VER" ];then
		mdsctl "fw" "{\"todo\": \"update_version\", \"version\": \"$NEW_FW_VER\"}"
		echo "$NEW_FW_VER" > ${FW_PATH}/${FW_VERSION}
		else
		TMP_STATUS=`cat ${FW_PATH}/${FW_STATUS}`
		if [ _"$TMP_STATUS" != _3 ];then #if not load success
		mdsctl "fw" "{\"todo\": \"update_version\", \"version\": \"$NEW_FW_VER\"}"
		fi
		exit 1
		fi
	else
	mdsctl "fw" "{\"todo\": \"update_version\", \"version\": \"$NEW_FW_VER\"}"
	echo "$NEW_FW_VER" > ${FW_PATH}/${FW_VERSION}
	fi
else
exit 0
fi

#step 4:if request from APP,trigger it to report msg || if QUICK is defined,meas quick upgrade
if [ -n "$REQUEST" ];then
    mdsctl "fw" '{"todo": "trigger"}' 
fi

if [ -n "$QUICK" ];then
    mdsctl "fw" '{"todo": "qcheckok"}'
fi
exit 0


