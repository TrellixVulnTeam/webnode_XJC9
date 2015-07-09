/**
*	Copyright (c)  AppexNetworks, All Rights Reserved.
*	High speed Files Transport
*	Author:	xyfeng
*
*	@defgroup	libcurl��װ
*	@ingroup		
*
*	Ϊ�������ģ���ṩ�ϴ����ؽӿ�
*
*	@{
*/
 
#ifndef _APX_TRANSFER_CL_H_
#define _APX_TRANSFER_CL_H_
         
/*-----------------------------------------------------------*/
/*                          Include File Header                               */
/*-----------------------------------------------------------*/
/*---Include ANSI C .h File---*/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif        
/*---Include Local.h File---*/
#include "../include/apx_list.h"
#include "../include/apx_type.h"

#ifdef __cplusplus
extern "C" {
#endif /*end __cplusplus */
        
/*------------------------------------------------------------*/
/*                          Macros Defines                                      */
/*------------------------------------------------------------*/
        
        
/*------------------------------------------------------------*/
/*                    Exported Variables                                        */
/*------------------------------------------------------------*/
        
        
/*------------------------------------------------------------*/
/*                         Data Struct Define                                      */
/*------------------------------------------------------------*/
        
        
/*------------------------------------------------------------*/
/*                          Exported Functions                                  */
/*------------------------------------------------------------*/
/**
	@brief	apx_trans_init
		ģ���ʼ��
 
	@param[in]	nu	 nu	����������	
	@return
		return 0 if success, else return -1
	history
		      xyfeng     	  	2015-4-14        Initial Draft 
*/
int apx_trans_init_cl( u32 nu );

/**
	@brief	apx_trans_exit
		ģ��ȥ��ʼ��
 
	@param[in]	 ( void )	
	@return
		( void )
	history
		      xyfeng     	  	2015-4-14        Initial Draft 
*/
void apx_trans_exit_cl( void );


/**
	@brief	apx_trans_create
		����һ������/�ϴ�����
 
	@param[in]	 ( void )	
	@return
		return 0 if success, else return -1
	history
		      xyfeng     	  	2015-4-14        Initial Draft 
*/
int apx_trans_create_cl( void );

/**
	@brief	apx_trans_release
		�ͷ�����
 
	@param[in]	nu		��������
	@param[in]	flags		no used
	@return
		return 0 if success, else return -1
	history
		      xyfeng     	  	2015-4-14        Initial Draft 
*/
int apx_trans_release_cl( u32 nu, int flags );


/**
	@brief	apx_trans_start
		��ʼ����
 
	@param[in]	nu		��������
	@param[in]	glb_opt	ȫ������
	@param[in]	task_opt	��������
	@return
		return 0 if success, else return -1
	history
		      xyfeng     	  	2015-4-14        Initial Draft 
*/
int apx_trans_start_cl( u32 nu, struct apx_trans_glboptions* glb_opt, struct apx_trans_opt* task_opt );

/**
	@brief	apx_trans_stop
		��������
 
	@param[in]	nu	��������
	@return
		return 0 if success, else return -1
	history
		      xyfeng     	  	2015-4-14        Initial Draft 
*/
int apx_trans_stop_cl( u32 nu);

/**
	@brief	apx_trans_delete_cl
		ɾ������
 
	@param[in]	nu	��������
	@return
		( void )
	history
		      xyfeng     	  	2015-4-14        Initial Draft 
*/
int apx_trans_delete_cl( u32 nu );

/**
	@brief	apx_trans_recv
	
 
	@param[in]	nu	
	@return
		int :
	history
		      xyfeng     	  	2015-4-14        Initial Draft 
*/
int apx_trans_recv_cl( u32 nu );

//int apx_trans_setopt(u32 nu, struct apx_trans_glboptions* glb_opt, struct apx_trans_opt* task_opt);
/**
	@brief	apx_trans_getopt
		��ȡ����
 
	@param[in]	nu		��������
	@param[in]	glb_opt	ȫ������
	@param[in]	task_opt	��������
	@return
		return 0 if success, else return -1
	history
		      xyfeng     	  	2015-4-14        Initial Draft 
*/
int apx_trans_getopt_cl( u32 nu, struct apx_trans_glboptions* glb_opt, struct apx_trans_opt* task_opt );

/**
	@brief	apx_trans_getstat
		��ȡͳ����Ϣ
 
	@param[in]	nu		��������
	@param[out]	task_stat	ͳ����Ϣ
	@return
		return 0 if success, else return -1
	history
		      xyfeng     	  	2015-4-14        Initial Draft 
*/
int apx_trans_getstat_cl( u32 nu, struct apx_trans_stat* task_stat );


/**
	@brief	apx_trans_get_btfile_cl
	
 
	@param[in]	nu	
	@param[in]	task_opt	
	@param[in]	bt_file	
	@return
		return 0 if success, else return -1
	history
		      xyfeng     	  	2015-4-14        Initial Draft 
*/
int apx_trans_get_btfile_cl( u32 nu, struct apx_trans_opt* task_opt, struct btfile* bt_file );

/**
	@brief	apx_trans_precreate_cl
		ͨ��url ��ȡ�ļ���Ϣ:
			�ļ���
			�ļ���С
			�Ƿ�֧�ֶ��߳�����
 
	@param[in]	task_opt		����url��proto, ����fname , fsize bp_continue
	@return
		return 0 if success, else return -1
	history
		      xyfeng     	  	2015-4-14        Initial Draft 
*/
int apx_trans_precreate_cl( struct apx_trans_opt* task_opt );

int apx_trans_del_file_cl( struct apx_trans_glboptions* glb_opt, struct apx_trans_opt* task_opt );
         
#ifdef __cplusplus
 }       
#endif /*end __cplusplus */
         
#endif /*end _APX_TRANSFER_CL_H_ */       
         
        

/** @} */
 
