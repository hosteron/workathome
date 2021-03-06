/*
2018-1-19 14 41
modify by xiang.zhou
*/
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <cf_timer.h>
#include <cf_buffer.h>
#include <cf_cmd.h>
#include "medusa.h"
#include "plug_cmd.h"
#include "mds_log.h"
#include "mds_msg_ctl.h"
#include "mds_msg_json_cmd.h"
#include "iksemel.h"


#define MDS_FW_PLUG_NAME    "fw"
#define MDS_FW_ELEM_CLASS_NAME    "FW"

#define MDS_FW_CK_FW	"/etc/conf/medusa/check_fw_hook.sh &>/data/fw_check.log"
#define MDS_FW_SH_DLD_FW    "/etc/medusa/mds_download_svr_fw.sh &> /data/fw_dld_fw.log"
#define MDS_FW_PLAY_SOUND   "/etc/medusa/plays_sound.sh &> /data/plays_sound.log"
#define MDS_FW_SH_DO_UPDATE "/etc/rc.d/fw.sh upgrade &> /data/fw_update.log"
#define MDS_FW_FILE_NAME     "/tmp/fw.bin"
#define LANGUAGE_IN_USE_PATH  "/data/log/language_in_use"
#define MDS_MAX_PATH_LEN            95

#define FW_SUCCESS     0
#define FW_BATTERY_LOW      10
#define FW_OUT_OF_CHARGE      11
#define FW_DOWNLOAD_ERROR 20
#define FW_FW_ERROR       30
#define FW_UPGRADE_FAIL     31

#define FW_IN_UPGRADING_SOUND_NUM     		 90
#define FW_UPGRADING_SUCCESS_SOUND_NUM     	 56
#define FW_UPGRADING_FAIL_SOUND_NUM    		 76

#define DOMAIN_NAME_CHINESE_MAINLAND 		"dl.ecouser.net"
#define DOMAIN_NAME_OTHER_AREAS 			"dl-ww.ecouser.net"
#define CHINESE_MAINLAND 					"ZH"

#define FW_CHECKHANDLE_REQ 0
#define FW_CHECKHANDLE_BEAT 1
#define FW_CHECKHANDLE_QUICK 2

#define FW_UPGRADE_NORMAL 0
#define FW_UPGRADE_REQUEST 1

typedef struct {
	MDS_ELEM_MEMBERS;
	enum {
		MDS_FW_ST_IDLE=0,
		MDS_FW_ST_DL_FW,
		MDS_FW_ST_UPDATING
	}status;

	char checkFwHook[MDS_MAX_PATH_LEN+1];
	char doUpdateHook[MDS_MAX_PATH_LEN+1]; 
	char downloadFwHook[MDS_MAX_PATH_LEN+1]; 
	char playSoundHook[MDS_MAX_PATH_LEN+1]; 
	char fwFileName[MDS_MAX_PATH_LEN+1];
	char language_in_use_path[MDS_MAX_PATH_LEN+1];
	int soundNumInUpgrading;
	int soundNumUpgradSuccess;
	int soundNumUpgradFail;

	char domainChina[128];
	char domainOthers[128];

	int interval; // min
	int fw_timer;
	CFFdevent fw_event;
	char version[16];
}MdsFwElem;
static MDSElem* _FwElemRequested(MDSServer* svr, CFJson* jConf);
static int _FwElemReleased(MDSElem* elem);
static int _FwAddedAsGuest(MDSElem* self, MDSElem* vendor);
static int _FwAddedAsVendor(MDSElem* self, MDSElem* guestElem);
static int _FwRemoveAsGuest(MDSElem* self, MDSElem* vendor);
static int _FwRemoveAsVendor(MDSElem* self, MDSElem* guestElem);
static int _FwProcess(MDSElem* self, MDSElem* vendor, MdsMsg* msg);

static MdsElemClass _fwCls = {
	.name = MDS_FW_ELEM_CLASS_NAME,
	.request = _FwElemRequested,
	.release = _FwElemReleased,
	.process = _FwProcess,
	.addedAsGuest = _FwAddedAsGuest,
	.addedAsVendor = _FwAddedAsVendor,
	.removeAsGuest = _FwRemoveAsGuest,
	.removeAsVendor = _FwRemoveAsVendor
};
static int _MdsFwPlugInit(MDSPlugin* plg, MDSServer* svr)
{
	MDS_MSG("Initiating plugin: "MDS_FW_PLUG_NAME"\n");
	return MDSServerRegistElemClass(svr, &_fwCls);
}

static int _MdsFwPlugExit(MDSPlugin* plg, MDSServer* svr)
{
	MDS_MSG("Exiting plugin: "MDS_FW_PLUG_NAME"\n");

	return MDSServerAbolishElemClass(svr, &_fwCls);
}

MDS_PLUGIN("xiang.zhou", 
		MDS_FW_PLUG_NAME, 
		"firmware upgrade",
		_MdsFwPlugInit,
		_MdsFwPlugExit,
		0, 0, 1);
int PlaySound(MdsFwElem* fwEm,int file_number)
{
	int ret = 0;
	char cmdBuf[128] = {0};

	snprintf(cmdBuf,sizeof(cmdBuf)," FILE_NUMBER=%d %s ",file_number,fwEm->playSoundHook); 

	MDS_DBG("cmdBuf=%s\n", cmdBuf);
	ret = system(cmdBuf);
	
	MDS_DBG("ret=%d\n", ret);
	if (ret == -1) {
		MDS_ERR("system(PlaySound_hook)\n");
	}
	return ret;
}
int write_language_in_use_to_file(MdsFwElem *fwEm,char *p_language_in_use)
{
	int len = 0;
	int ret = 0;
	char file_path[128];
	len = strlen(p_language_in_use) + 1;
	snprintf(file_path, sizeof(file_path), "%s_tmp",fwEm->language_in_use_path);
	if (len !=CFBufToFile(file_path,p_language_in_use,len))
	{
		MDS_ERR("write %s failed\n", file_path);	
		return -1;
	}

	ret = rename(file_path, fwEm->language_in_use_path);

	if(ret <0){	
		MDS_ERR("Can not rename file:%s_tmp", fwEm->language_in_use_path);
	}
	
	return ret;
}
int MdsFwElemPubMsg(MdsFwElem *fwEm, int is_successed, int error_code)
{
	iks *resp;
	MdsCtlMsg msg;
	struct timeval tv;
	uint64_t ts;
	char pTmpValue[64] = { 0 };

	gettimeofday(&tv, NULL);

	ts = tv.tv_sec;
	ts *= 1000;	
	ts += tv.tv_usec/1000;

	resp = iks_new("ctl");
	if(resp){
		memset(pTmpValue, 0, sizeof(pTmpValue));
		snprintf(pTmpValue, sizeof(pTmpValue), "%lld", ts);
		iks_insert_attrib(resp, "ts", pTmpValue);
		iks_insert_attrib(resp, "td", "UpdateResult");

		if(is_successed == 1)
		{
			iks_insert_attrib(resp, "ret", "ok");
		}
		else
		{
			iks_insert_attrib(resp, "ret", "fail");

			memset(pTmpValue, 0, sizeof(pTmpValue));
			snprintf(pTmpValue, sizeof(pTmpValue), "%d", error_code);
			iks_insert_attrib(resp, "errno", pTmpValue);
		}

		msg.from = NULL;
		msg.to = NULL;
		msg.cmd = resp;

		MDSElemCastMsg((MDSElem*)fwEm, MDS_MSG_TYPE_CTL, &msg);

		iks_delete(resp);
	}

	return 0;
}
static int _FwCheckHandle(MdsFwElem *fwEm, int isBeat)
{
	char *p = NULL;
	char domain_name[128],tmpBuf[256];
	if((p=getenv("DEFAULT_COUNTRY"))  !=  NULL){
		MDS_DBG("DEFAULT_COUNTRY =%s\n",p);
		if (strcmp(p,CHINESE_MAINLAND) == 0){
			strncpy(domain_name,fwEm->domainChina,sizeof(domain_name));
		}else{
			strncpy(domain_name,fwEm->domainOthers,sizeof(domain_name));	
		}
	}else{
		strncpy(domain_name,fwEm->domainChina,sizeof(domain_name));
	}

	if(isBeat == FW_CHECKHANDLE_BEAT)
		snprintf(tmpBuf,sizeof(tmpBuf),"DOMAIN_NAME=%s %s ",domain_name,fwEm->checkFwHook);
	else if(isBeat == FW_CHECKHANDLE_REQ)
		snprintf(tmpBuf,sizeof(tmpBuf),"DOMAIN_NAME=%s REQUEST=YES %s ",domain_name,fwEm->checkFwHook);
	else
		snprintf(tmpBuf,sizeof(tmpBuf),"DOMAIN_NAME=%s QUICK=YES %s ",domain_name,fwEm->checkFwHook);
	MDS_DBG("tmpBuf =%s\n",tmpBuf);
	system(tmpBuf);

	return 0;
}
static int _FwDoUpgrade(MdsFwElem *fwEm, int flag)
{
	char *p = NULL;
	char tmpBuf[256];
	PlaySound(fwEm,fwEm->soundNumInUpgrading);

	if((p=getenv("LANGUAGE_IN_USE"))  !=  NULL){
		MDS_DBG("LANGUAGE_IN_USE =%s\n",p);
		write_language_in_use_to_file(fwEm,p);
	}
	if(FW_UPGRADE_NORMAL == flag)
		snprintf(tmpBuf,sizeof(tmpBuf),"MDS_FW_FILE_NAME=%s NORMAL=YES %s ",fwEm->fwFileName,fwEm->downloadFwHook);
	else
		snprintf(tmpBuf,sizeof(tmpBuf),"MDS_FW_FILE_NAME=%s %s ",fwEm->fwFileName,fwEm->downloadFwHook);
	MDS_DBG("tmpBuf =%s\n",tmpBuf);
	system(tmpBuf);

	fwEm->status = MDS_FW_ST_DL_FW;
	return 0;
}
int fw_TimerHandlerfunc(CFFdevents* events, CFFdevent* event, int fd,void* arg)
{
	MdsFwElem *elem = (MdsFwElem *)arg;
	uint64_t          exp;
	read(fd, &exp, sizeof(uint64_t));
	//printf("###[%s:%d]####\n", __func__,__LINE__);
	_FwCheckHandle(elem, FW_CHECKHANDLE_BEAT);
	return 0;
}
static int TimerfdInit(unsigned int expiration_ms,unsigned int periodic_ms)

{
	int fd;
	struct timespec now;
	struct itimerspec new_value;
	if (clock_gettime(CLOCK_MONOTONIC, &now) == -1)
	{
		perror("clock_gettime error");
		return -1;
	}
	new_value.it_value.tv_sec = now.tv_sec + expiration_ms/1000;
	new_value.it_value.tv_nsec = now.tv_nsec + (expiration_ms%1000)*1000*1000;
	if(new_value.it_value.tv_nsec >= 1000000000)
	{
		new_value.it_value.tv_nsec -= 1000000000;
		new_value.it_value.tv_sec ++;
	}
	new_value.it_interval.tv_sec = periodic_ms/1000;
	new_value.it_interval.tv_nsec = (periodic_ms%1000)*1000*1000;
	printf("[%s:%d]####%d,%d\n", __func__, __LINE__,new_value.it_interval.tv_sec,new_value.it_interval.tv_nsec);
	fd = timerfd_create(CLOCK_MONOTONIC, 0);
	if (fd <0)
	{
		perror("timerfd_create:"); 
		return fd;  
	}

	if (timerfd_settime(fd, TFD_TIMER_ABSTIME, &new_value, NULL) == -1)
	{ 
		perror("timerfd_settime:");
		close(fd);
		return -1;
	}

	return fd;
}
static MDSElem* _FwElemRequested(MDSServer* svr, CFJson* jConf)
{
	MdsFwElem* fwEm;
	const char *tmpCStr,*ptr;
	int tmpInt,len;
#define GET_HOOK_INFO(__JCONF, __KEY, __TO, __DEFAULT,__IS_HOOK) 	\
	if (!(tmpCStr = CFJsonObjectGetString(__JCONF, __KEY))) {	\
		ptr = __DEFAULT;										\
		len = strlen(__DEFAULT) + 1;							\
	}else{														\
		ptr = tmpCStr;											\
		len = strlen(tmpCStr) + 1;								\
	}															\
	if(len < MDS_MAX_PATH_LEN){									\
		if(__IS_HOOK){											\
			if(memrchr((const void*)ptr,(int)'&',len)){			\
				strncpy(__TO,ptr,len);							\
			}else{												\
				snprintf(__TO, MDS_MAX_PATH_LEN,"%s &",ptr);	\
			}													\
		}else{													\
			strncpy(__TO,ptr,len);								\
		}														\
	}else{														\
		MDS_ERR_OUT(ERR_FREE_FEM, "MDSElem init failed: %s is overlong [%d/%d]\n", ptr,len,MDS_MAX_PATH_LEN);	\
	}

	if (!svr || !jConf) {
		MDS_ERR_OUT(ERR_OUT, "\n");
	}
	if (!(fwEm = (MdsFwElem*)calloc(1,sizeof(MdsFwElem)))) {
		MDS_ERR_OUT(ERR_OUT, "malloc for MdsFwElem failed\n");
	}

	GET_HOOK_INFO(jConf, "check_fw_hook",fwEm->checkFwHook,MDS_FW_CK_FW,1) ;
	GET_HOOK_INFO(jConf, "do_update_hook",fwEm->doUpdateHook,MDS_FW_SH_DO_UPDATE,1) ;
	GET_HOOK_INFO(jConf, "download_fw_hook",fwEm->downloadFwHook,MDS_FW_SH_DLD_FW,1) ;
	GET_HOOK_INFO(jConf, "PlaySound_hook",fwEm->playSoundHook,MDS_FW_PLAY_SOUND,1) ;
	GET_HOOK_INFO(jConf, "fw_file_name",fwEm->fwFileName,MDS_FW_FILE_NAME,0);

	GET_HOOK_INFO(jConf, "domain_china",fwEm->domainChina,DOMAIN_NAME_CHINESE_MAINLAND,0);
	GET_HOOK_INFO(jConf, "domain_others",fwEm->domainOthers,DOMAIN_NAME_OTHER_AREAS,0);
	
	GET_HOOK_INFO(jConf, "language_in_use_path",fwEm->language_in_use_path,LANGUAGE_IN_USE_PATH,0);
	fwEm->status  = MDS_FW_ST_IDLE;

	fwEm->soundNumInUpgrading	= FW_IN_UPGRADING_SOUND_NUM;
	fwEm->soundNumUpgradSuccess = FW_UPGRADING_SUCCESS_SOUND_NUM;
	fwEm->soundNumUpgradFail 	= FW_UPGRADING_FAIL_SOUND_NUM;
	CFJsonObjectGetInt(jConf,"sound_num_in_upgrading",&fwEm->soundNumInUpgrading);
	CFJsonObjectGetInt(jConf,"sound_num_upgrade_success",&fwEm->soundNumUpgradSuccess);
	CFJsonObjectGetInt(jConf,"sound_num_upgrade_fail",&fwEm->soundNumUpgradFail);
	fwEm->interval = 10;
	CFJsonObjectGetInt(jConf,"interval",&fwEm->interval);
	printf("[%s:%d]####%d\n", __func__, __LINE__, fwEm->interval);
	//fwEm->fw_timer = TimerfdInit(1000, fwEm->interval*60*1000);
	fwEm->fw_timer = TimerfdInit(10, 5*1000);
	if(fwEm->fw_timer <= 0)
	{
		MDS_ERR_OUT(ERR_FREE_FEM, "TimerfdInit error\n");
	}
	if (CFFdeventInit(&fwEm->fw_event, fwEm->fw_timer, "fw timer fd event",
				fw_TimerHandlerfunc, (void*)fwEm, NULL, NULL, NULL, NULL)) {
		MDS_ERR_OUT(ERR_EVENT_INIT, "CFFdeventInit timer error\n");
	}

	if (CFFdeventsAdd(MDSServerGetFdevents(svr), &fwEm->fw_event))
	{
		MDS_ERR_OUT(ERR_EVENT_ADD, "CFFdeventInit timer error\n");
	}
	/* init medusa element */
	if (!(tmpCStr=CFJsonObjectGetString(jConf, "name"))
			|| MDSElemInit((MDSElem*)fwEm, svr, &_fwCls, tmpCStr, _FwProcess,
				_FwAddedAsGuest, _FwAddedAsVendor,
				_FwRemoveAsGuest, _FwRemoveAsVendor)) {
		MDS_ERR_OUT(ERR_FREE_INIT, "MDSElem init failed: for %s\n", tmpCStr);
	}
	return (MDSElem*)fwEm;
ERR_FREE_INIT:
	CFFdeventsDel(fwEm->server->fdevents, &fwEm->fw_event);
ERR_EVENT_ADD:
	CFFdeventExit(&fwEm->fw_event);
ERR_EVENT_INIT:
	close(fwEm->fw_timer);
	fwEm->fw_timer = -1;
ERR_FREE_FEM:
	free(fwEm);
ERR_OUT:
	return NULL;
}
static int _FwElemReleased(MDSElem* elem)
{	
	MdsFwElem *fwEm= (MdsFwElem*)elem;

	MDSElemExit(elem);
	close(fwEm->fw_timer);
	fwEm->fw_timer = -1;
	free(fwEm);
	return 0;
}
static int _FwAddedAsGuest(MDSElem* self, MDSElem* vendor)
{
	return 0;
}

static int _FwAddedAsVendor(MDSElem* self, MDSElem* guestElem)
{
	return 0;
}

static int _FwRemoveAsGuest(MDSElem* self, MDSElem* vendor)
{
	return 0;
}

static int _FwRemoveAsVendor(MDSElem* self, MDSElem* guestElem)
{
	return 0;
}
static int _FwProcess(MDSElem* self, MDSElem* vendor, MdsMsg* msg)
{
	MdsFwElem *fwEm = NULL;
	fwEm = (MdsFwElem*)self;
	int ret=0,error=0xf;
	int error_code = 0;
	char *pret=NULL,*perror=NULL;
	if (!strcmp(msg->type, MDS_MSG_TYPE_CMD)) 
	{
		const char *jMsgStr;
		CFBuffer *respBuf;
		CFJson *jMsg;
		const char* tmpCStr;
		int result=-1;

		char tmpBuf[128];

		jMsgStr = ((MdsCmdMsg*)msg->data)->cmd;
		respBuf = ((MdsCmdMsg*)msg->data)->respBuf;
		MDS_DBG("jMsgStr:%s\n", jMsgStr);
		jMsg = CFJsonParse(jMsgStr);
		if (!jMsg) {
			CFBufferCp(respBuf, CF_CONST_STR_LEN("Wrong cmd format\n"));
		} else {
			if ((tmpCStr = CFJsonObjectGetString(jMsg, "todo"))) {
				if (!strcmp(tmpCStr, "fw")) {
					if ((tmpCStr = CFJsonObjectGetString(jMsg, "cmd"))) {
						if (!strcmp(tmpCStr, "query_update_status")){
							if (fwEm->status==MDS_FW_ST_IDLE ){
								CFBufferCp(respBuf, CF_CONST_STR_LEN("{\"ret\": \"ok\",\"status\": \"idle\"}"));  
							}else{
								CFBufferCp(respBuf, CF_CONST_STR_LEN("{\"ret\": \"ok\",\"status\": \"updating\"}"));
							}
						}
					}
					error = 0;
				}else if(!strcmp(tmpCStr, "update_version")) {
					if((tmpCStr = CFJsonObjectGetString(jMsg, "version"))) {
						strncpy(fwEm->version, tmpCStr, sizeof(fwEm->version));
					}

					CFBufferCp(respBuf, CF_CONST_STR_LEN("{\"ret\": \"ok\"}"));
					error = 0;
					_FwDoUpgrade(fwEm,FW_UPGRADE_NORMAL);
				}else if(!strcmp(tmpCStr, "trigger")){
					//_TriggerHilink(fwEm);
					CFBufferCp(respBuf, CF_CONST_STR_LEN("{\"ret\": \"ok\"}"));
					error = 0;
				}else if(!strcmp(tmpCStr, "qcheckok")) {
					_FwDoUpgrade(fwEm,FW_UPGRADE_REQUEST);
					CFBufferCp(respBuf, CF_CONST_STR_LEN("{\"ret\": \"ok\"}"));
					error = 0;
				}else if (!strcmp(tmpCStr, "status")) {
					if ((tmpCStr = CFJsonObjectGetString(jMsg, "status"))) {
						if (CFJsonObjectGetInt(jMsg, "result", &result)	) {
							result = 1;
						}

						if (result != 0){
							CFJsonObjectGetInt(jMsg, "error_code", &error_code);
						}


						if (!strcmp(tmpCStr, "MDS_FW_DL_FW_FINISHED")) {
							CFBufferCp(respBuf, CF_CONST_STR_LEN("{\"ret\": \"ok\"}"));

							if(strlen(fwEm->doUpdateHook) && result==0){
								snprintf(tmpBuf,sizeof(tmpBuf),"%s",fwEm->doUpdateHook);
								system(tmpBuf);
								fwEm->status = MDS_FW_ST_UPDATING;
							}

							error = result;
						}else if (!strcmp(tmpCStr, "MDS_FW_UPDATE_FINISHED")) {	

							if (fwEm->status != MDS_FW_ST_IDLE ){
								
								error = 0;
								CFBufferCp(respBuf, CF_CONST_STR_LEN("{\"ret\": \"fail\"}")); 
								MDS_DBG("SEND UPDATE MSG FAIL!!!!");
								return -1;
							}			
						
							switch (result){
								case FW_DOWNLOAD_ERROR:
									//download error
									MdsFwElemPubMsg(fwEm,0,FW_DOWNLOAD_ERROR);
									break;
								case FW_FW_ERROR:
									//firmware error
									MdsFwElemPubMsg(fwEm,0,FW_FW_ERROR);
									break;
								case FW_UPGRADE_FAIL:
									//upgrade fail
									MdsFwElemPubMsg(fwEm,0,FW_UPGRADE_FAIL);
									break;
								default:
									//success
									MdsFwElemPubMsg(fwEm,1,0);
									break;
							}
							error = 0;
							CFBufferCp(respBuf, CF_CONST_STR_LEN("{\"ret\": \"ok\"}"));                          
						} else {
							error = 4;
							CFBufferCp(respBuf, CF_CONST_STR_LEN("{\"ret\": \"fail\", \"error\":\"4\"}"));
						}
					} else {
						error = 3;
						CFBufferCp(respBuf, CF_CONST_STR_LEN("{\"ret\": \"fail\", \"error\":\"3\"}"));
					}                  
				} else if (!strcmp(tmpCStr, "QuickUpgrade")) {
					if((fwEm->status==MDS_FW_ST_IDLE) && strlen(fwEm->checkFwHook)){
						_FwCheckHandle(fwEm,FW_CHECKHANDLE_QUICK);
						error = 0;
						CFBufferCp(respBuf, CF_CONST_STR_LEN("{\"ret\": \"ok\"}"));
					}else{
						error = -1;
						CFBufferCp(respBuf, CF_CONST_STR_LEN("{\"ret\": \"fail\", \"error\":\"busy now!\"}"));
					}				                       
				} else {
					error = 2;
					CFBufferCp(respBuf, CF_CONST_STR_LEN("{\"ret\": \"fail\", \"error\":\"2\"}"));
				}
			} else {
				error = 1;
				CFBufferCp(respBuf, CF_CONST_STR_LEN("{\"ret\": \"fail\", \"error\":\"1\"}"));
			}
			CFJsonPut(jMsg);

			if(error!=0){

				MdsFwElemPubMsg(fwEm,0,error_code);
				PlaySound(fwEm,fwEm->soundNumUpgradFail );
				fwEm->status = MDS_FW_ST_IDLE;
			}

		}
	}
	else if (!strcmp(msg->type, MDS_MSG_TYPE_JSON_CMD))
	{
		const char *pTmpStr = NULL;
		MdsJsonCmdMsg *jCmd = (MdsJsonCmdMsg *)msg->data;
		if(!jCmd || !jCmd->cmd) return 0;

		if((pTmpStr = CFJsonObjectGetString(jCmd->cmd, "todo")))
		{
			if(!strcmp(pTmpStr, "check_upgrade"))
			{
				_FwCheckHandle(fwEm, FW_CHECKHANDLE_REQ);
			}
			else if(!strcmp(pTmpStr, "do_upgrage"))
			{
				_FwDoUpgrade(fwEm, FW_UPGRADE_REQUEST);
			}
			else if(!strcmp(pTmpStr,"get_version"))
			{
				if(strlen(fwEm->version) && jCmd->resp) CFJsonObjectAddString(jCmd->resp, "version", fwEm->version);
			}
		}
	}

	return 0;
}