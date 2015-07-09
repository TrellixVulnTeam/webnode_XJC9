/**
*	Copyright (c)  AppexNetworks, All Rights Reserved.
*	High speed Files Transport
*	Author:	xyfeng
*
*	@defgroup	Э�����ģ��
*	@ingroup		
*
*	Ϊ�ƶ�ͨ���ṩЭ����ƽӿ�
*
*	@{
*Function List: 
*/
 
#ifndef _APX_PRORO_CTL_H_
#define _APX_PRORO_CTL_H_
         
/*-----------------------------------------------------------*/
/*                          Include File Header                               */
/*-----------------------------------------------------------*/
/*---Include ANSI C .h File---*/
        
/*---Include Local.h File---*/
#include "../include/apx_hftsc_api.h"

#ifdef __cplusplus
extern "C" {
#endif /*end __cplusplus */
        
/*------------------------------------------------------------*/
/*                          Macros Defines                                      */
/*------------------------------------------------------------*/
#define	HTTP_OK	( 200 )
#define	ERR_LEN_MAX 	( 256 )
        
/*------------------------------------------------------------*/
/*                    Exported Variables                                        */
/*------------------------------------------------------------*/
        
        
/*------------------------------------------------------------*/
/*                         Data Struct Define                                      */
/*------------------------------------------------------------*/
/** restful api type */
typedef enum _restful_e_
{
	RESTFUL_LOGIN = 0, /** ��½ */
	RESTFUL_LOGOUT, /** ע�� */
	
	RESTFUL_TASK_CRT_HTTP, /** ����http�������� */
	RESTFUL_TASK_CRT_BT, /** ����bt�������� */
	RESTFUL_TASK_START, /** ��ʼ���� */
	RESTFUL_TASK_STOP, /** ��ͣ���� */
	RESTFUL_TASK_DEL,	/** ɾ���������� */
	RESTFUL_TASK_STATUS, /** ��ȡ����״̬ */
	RESTFUL_TASK_LIST, /** ��ȡ�����б� */
	RESTFUL_UPLOAD_PRELOAD, /** �ļ��ϴ�Ԥȡ */
	RESTFUL_FILEINFO, /** �ļ�״̬*/
	RESTFUL_FAULT_CHK, /** �ƶ�״̬��� */
	
	RESTFUL_MAX
}restful_e;

/** ��½: �ƶ˷��ص��û���Ϣ */
typedef struct _cloud_userinfo_s_
{
	u8	admin;
	u16	status;
	u16	quota;
	
	u32	usrId;
	u32	groupId;
	
	u16	upLimit;
	u16	downLimit;
	u16	activeTaskLimit;
	
	char name[ERR_LEN_MAX];
	char passwd[ERR_LEN_MAX];
	char email[ERR_LEN_MAX];
	
	char createTm[ERR_LEN_MAX];
	char loginTm[ERR_LEN_MAX];
	char lastLoginTm[ERR_LEN_MAX];
	
	char remark[ERR_LEN_MAX];
}cld_userinfo_st;

typedef struct _cloud_btfile_s_
{
	int size;
	int download_size;
	char name[ERR_LEN_MAX];
	char hash[48];
}cld_btfile_st;

/** ���� �ƶ˷��ص�������Ϣ */
typedef struct _cloud_taskinfo_s_
{
	int priority;
	int conns;
	char url[ERR_LEN_MAX*4];
	char description[ERR_LEN_MAX*2];
	int next_idx;
	cld_btfile_st btfile[0];
}cld_taskinfo_st;

typedef struct _cloud_task_s_
{
	int id;
	int status;
	cld_taskinfo_st *info;
}cld_task_st;

/** �ϴ�״̬ */
typedef enum
{
	FILE_UP_NONE = 0, /** δ�ϴ� */
	FILE_UP_DONE, /** ���ϴ� */
	FILE_UP_HALF, /** �����ϴ� */

	FILE_UP_MAX
}fileup_e;

/** �ļ��ϴ�Ԥȡ�� �ƶ˷��ص��ļ��ֶ���Ϣ */
typedef struct _cloud_fileinfo_s_
{
	fileup_e eStatus;
	u32	preload; /** 1: preload, other: fileinfo */
	s32 blkCnt;
	u8 fileId[64];
	u8 sign[41];
	void *pblk;
}cld_fileinfo_st;

/** �ƶ�״̬��� */
typedef struct _cloud_fault_s_
{
	u32 serverCon:2, /** �洢������ */
		storageSpace:2, /** �洢�ռ� */
		dbCon:2, /** ���ݿ������ */
		dbStorage:2, /** ���ݿ�洢�ռ� */
		dnsResolve:2; /** DNS���� */
}cld_fault_st;

typedef struct _cloud_taskdetail_s_
{
	int id;
	int status;
	int priority;
	int conns;
	char url[ERR_LEN_MAX*2];
	char description[ERR_LEN_MAX];
	int next_idx;
	cld_btfile_st btfile[0];
}cld_taskdetail_st;

typedef struct _cloud_filedetail_s_
{
	u64 size;
	u64 cur_size;
	fileup_e eStatus;
	char name[ERR_LEN_MAX];
	char fileId[64];
}cld_filedetail_st;

/** �ƶ������б� */
typedef struct  _cloud_list_s_
{
	u8	type; /** 0: http  1: bt 2: upload file*/
	union {
		cld_taskdetail_st taskinfo;
		cld_filedetail_st fileinfo;
	}u;
}cld_list_st;

/** �ƶ������б� */
typedef struct  _cloud_tasklist_s_
{
	u16 total;
	u16 cnt;
	cld_list_st* plist;
}cld_tasklist_st;

/*------------------------------------------------------------*/
/*                          Exported Functions                                  */
/*------------------------------------------------------------*/
char* apx_cloud_get_url( void);
void apx_cloud_set_url( char* ps8RootUrl );
int apx_cloud_login( char *user, char *passwd, int *http_code, cld_userinfo_st *userinfo );
int apx_cloud_logout();
int apx_cloud_task_create( struct apx_trans_opt *trans_opt, int *task_status );
int apx_cloud_task_start( int task_id, int *http_code, int *task_status );
int apx_cloud_task_stop( int task_id, int *http_code, int *task_status );
int apx_cloud_task_del( int task_id, int *http_code, int *task_status );
int apx_cloud_task_status( int task_id, int *http_code, cld_task_st *taskinfo  );
int apx_cloud_task_list( int *http_code, int start,  int limit, cld_tasklist_st *ptask_list );
void apx_cloud_task_freelist( cld_tasklist_st *ptask_list );
int apx_cloud_upload_proload( s8* fname, u32 blkcnt,  int *http_code, cld_fileinfo_st *fileinfo  );
int apx_cloud_fileinfo( int *http_code, cld_fileinfo_st *fileinfo  );
int apx_cloud_fault_check( int *http_code, cld_fault_st *faultinfo );

#ifdef __cplusplus
 }       
#endif /*end __cplusplus */
         
#endif /*end _APX_PRORO_CTL_H_ */       
         
        

/** @} */
 
