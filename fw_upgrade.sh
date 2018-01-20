#!/bin/sh
# version: v0.0.2
# do upgrade fw from /tmp/fw.bin
#
set -x

NORMAL_FS_MNT=/tmp/normal_fs
FW_FILE=/tmp/fw.bin             
LOG_FILE=/data/log/fw.log
FW_MAKR_FILE=/data/log/fw_mark
BOOT_FLAG_PART=/dev/block/mtd/by-name/boot_flag	

source /usr/bin/factory_fun


mkdir -p /data/log/

echo 31 > ${FW_MAKR_FILE} && sync

if [ -f ${FW_START_MAKR_FILE} ];then
	mv ${FW_START_MAKR_FILE} ${FW_START_MAKR_FILE}.tmp && rm ${FW_START_MAKR_FILE}.tmp
fi

cat /proc/cmdline | grep root=/dev/block/mtd/by-name/boot1_fs

if [ $? -eq 0 ]
then	

	BOOT_PART=/dev/block/mtd/by-name/boot2
	BOOT_FS_PART=/dev/block/mtd/by-name/boot2_fs 
	BOOT_FLAG=boot_mode2 

else
	BOOT_PART=/dev/block/mtd/by-name/boot1
	BOOT_FS_PART=/dev/block/mtd/by-name/boot1_fs  
	BOOT_FLAG=boot_mode1

fi



if [ -f ${FW_FILE} ]
then

	RET=`fw_tools -p -S ${FW_FILE}`
	FW_PRODUCT=`cjp "${RET}" string product`

	RET=`cat /etc/fw.manifest`
	DEV_PRODUCT=`cjp "${RET}" string product`

	if [ "${FW_PRODUCT}" != "${DEV_PRODUCT}" ]
	then
		echo 30 > ${FW_MAKR_FILE}
		reboot
	fi

	fw_tools -p -Snormal_boot ${FW_FILE} > /tmp/normal_boot.img && \
	dd if=/tmp/normal_boot.img of=${BOOT_PART} && \
	fw_tools -p -Snormal_fs ${FW_FILE} > /tmp/root_fs.img && \
	dd if=/tmp/root_fs.img of=${BOOT_FS_PART}

	if [ $? -eq 0 ]
	then						
		dd if=/dev/zero of=${BOOT_FLAG_PART} bs=512 count=1
		echo ${BOOT_FLAG} > ${BOOT_FLAG_PART} && echo 0 > ${FW_MAKR_FILE}
		echo `date`  fw_update success! >> ${LOG_FILE}
		mdsctl "fw" '{"todo": "progress","value":20}'
		mdsctl "fw" '{"todo": "progress","value":40}'
		mdsctl "fw" '{"todo": "progress","value":60}'
		mdsctl "fw" '{"todo": "progress","value":80}'
		mdsctl "fw" '{"todo": "progress","value":100}'
		mdsctl "fw" '{"todo": "progress","value":100}'
		mdsctl "fw" '{"todo": "progress","value":100}'
		sleep 1s
		sync
		reboot
	else
		echo 30 > ${FW_MAKR_FILE}
		reboot
	fi  

	
else
	echo `date`  fw_update fail: no fw.bin found! >> ${LOG_FILE}
fi


