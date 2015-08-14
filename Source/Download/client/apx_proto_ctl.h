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
#define	HTTP_OK		( 200 )
#define	HTTP_206		( 206 ) /** Partial Content */
#define	HTTP_403		( 403 ) /** Forbidden */
#define	HTTP_404		( 404 ) /** Not Found */
#define	HTTP_410		( 410 ) /** Gone */
#define	HTTP_412		( 412 ) /** Precondition Failed */

#define	APX_SIGN_LEN	( 41 )
#define	APX_ID_LEN		( 64 )
#define	BUF_LEN_MAX 	( 128 )
#define	URL_LEN_MAX 	( 512 )

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
	RESTFUL_TASK_CRT_FTP, /** ���� FTP �������� */
	RESTFUL_TASK_START, /** ��ʼ���� */
	RESTFUL_TASK_STOP, /** ��ͣ���� */
	RESTFUL_TASK_DEL,	/** ɾ���������� */
	RESTFUL_TASK_STATUS, /** ��ȡ����״̬ */
	RESTFUL_TASK_LIST, /** ��ȡ�����б� */
	RESTFUL_TASK_PROXY, /** ��ת����*/
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
	
	char name[BUF_LEN_MAX];
	char passwd[BUF_LEN_MAX];
	char email[BUF_LEN_MAX];
	
	char createTm[BUF_LEN_MAX];
	char loginTm[BUF_LEN_MAX];
	char lastLoginTm[BUF_LEN_MAX];
	
	char remark[BUF_LEN_MAX];
}cld_userinfo_st;

/** �ƶ��������� */
typedef enum
{
	CLD_HTTP = 0,
	CLD_BT, /** ���� */
	CLD_UPLOAD, /** ���� */
	CLD_FTP, /** ״̬*/
	
	CLDTASK_MAX
}cldtask_e;

/** ���� �ƶ˷��ص�������Ϣ */
typedef struct _cloud_taskinfo_s_
{
	cldtask_e	type; /** 0: http, 1: bt 2: upload 3: ftp*/
	int status;/** 0 �ǳ�ʼ������û���� 
				1����ʼ����
				2���������
				3������Ϊֹͣ
				4�����Ƴ�
				5	������ֹ
				6��������ɣ����ڽ����ļ�����
				*/
	int size;
	int download_size;
	int priority;
	int conns;
	int speed;	/** byte / s */
	int remain;	/** ʣ��ʱ�� */	
	u8 url[URL_LEN_MAX];
	u8 description[BUF_LEN_MAX];
	u8 fileId[APX_ID_LEN];
	u8 ctime[APX_SIGN_LEN]; /** ����ʱ�� */
	u8 name[BUF_LEN_MAX];
	u8 hash[APX_SIGN_LEN];
}cld_taskinfo_st;

/** �ϴ�״̬ */
typedef enum
{
	FILE_UP_NONE = 0, /** δ�ϴ� */
	FILE_UP_DONE, /** ���ϴ� */
	FILE_UP_HALF, /** �����ϴ� */

	FILE_UP_MAX
}fileup_e;

/** ������Ϣ���� */
typedef enum
{
	TREQ_NONE = 0,
	TREQ_CRT, /** ���� */
	TREQ_STOP, /** ���� */
	TREQ_ST, /** ״̬*/
	TREQ_LIST, /** �����б� */

	TREQ_MAX
}treqfrom_e;

/** �ļ���Ϣ���� */
typedef enum
{
	FREQ_NONE = 0,
	FREQ_PRE, /** Ԥȡ */
	FREQ_UP, /** �ϴ� */
	FREQ_DOWN, /** ����*/
	FREQ_ST, /** ״̬*/
	FREQ_LIST, /** �����б� */

	FREQ_MAX
}freqfrom_e;

/** �ļ��ϴ�Ԥȡ�� �ƶ˷��ص��ļ��ֶ���Ϣ */
typedef struct _cloud_fileinfo_s_
{
	int size;
	int cur_size;
	fileup_e eStatus;
	char name[BUF_LEN_MAX];
	char fileId[APX_ID_LEN];
	u8 sign[APX_SIGN_LEN];
	u8 ctime[APX_SIGN_LEN];/**  ����ʱ�� */
	
	freqfrom_e eReqType;
	s32 blkCnt;
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

/** �ƶ������б� */
typedef struct  _cloud_list_s_
{
	u8	type; /** 0: http  1: bt 2: upload file 3: ftp*/
	union {
		cld_taskinfo_st taskinfo;
		cld_fileinfo_st fileinfo;
	}u;
}cld_list_st;

/** �ƶ������б� */
typedef struct  _cloud_tasklist_s_
{
	u16 total; /** �ܵ�������� */
	u16 cnt; /** plist������� */
	cld_list_st* plist;
}cld_tasklist_st;

/*------------------------------------------------------------*/
/*                          Exported Functions                                  */
/*------------------------------------------------------------*/
char* apx_cloud_get_url( void);
void apx_cloud_set_url( char* ps8RootUrl );
int apx_cloud_login( char *user, char *passwd, cld_userinfo_st *userinfo );
int apx_cloud_logout();
int apx_cloud_task_create( struct apx_trans_opt *trans_opt, cld_taskinfo_st *taskinfo );
int apx_cloud_task_start( char *taskId );
int apx_cloud_task_stop( char *taskId, cld_list_st *info );
int apx_cloud_task_del( char *taskId );
int apx_cloud_task_status( char *taskId, cld_list_st *info );
int apx_cloud_task_list( int start,  int limit, cld_tasklist_st *ptask_list );
void apx_cloud_task_freelist( cld_tasklist_st *ptask_list );
int apx_cloud_upload_proload( s8* fname, u32 blkcnt, cld_fileinfo_st *fileinfo  );
int apx_cloud_fileinfo( cld_fileinfo_st *fileinfo  );
int apx_cloud_fault_check( cld_fault_st *faultinfo );
s8* apx_build_proxy_json( s8 *url, s8 *headers[], size_t size );
int apx_get_proxy_url( s8** ppUrl );
int apx_cloud_sign( s8 *pUrl, size_t sLen );

#ifdef __cplusplus
 }       
#endif /*end __cplusplus */
         
#endif /*end _APX_PRORO_CTL_H_ */       
         
        

/** @} */
 
