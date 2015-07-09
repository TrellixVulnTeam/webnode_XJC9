/**
 *  Copyright APPEX, 2015, 
 *  <HFTS-CLIENT>
 *  <xyfeng>
 *
 *  @defgroup <configure>
 *  @ingroup  <hfts-client>
*
*	<Ϊ����ģ���ṩ�ļ����ƽӿ�>
*
 *  @{
 */
 
#ifndef APX_FILE_H_20150210
#define APX_FILE_H_20150210
         
/*-----------------------------------------------------------*/
/*                          Include File Header                               */
/*-----------------------------------------------------------*/
/*---Include ANSI C .h File---*/
#include <stddef.h>

/*---Include Local.h File---*/
#include "../include/apx_list.h"
#include "../include/apx_type.h"
        
#ifdef __cplusplus
extern "C" {
#endif /*end __cplusplus */
        
/*------------------------------------------------------------*/
/*                          Macros Defines                                      */
/*------------------------------------------------------------*/
#define	APX_FILE_BLK_FULL		( -0x1A1B1C1D )       /** blockд���� */
#define	APX_FILE_FULL			( -0x2A2B2C2D )       /** �ļ���������*/

/*------------------------------------------------------------*/
/*                    Exported Variables                                        */
/*------------------------------------------------------------*/
        
        
/*------------------------------------------------------------*/
/*                         Data Struct Define                                      */
/*------------------------------------------------------------*/
typedef	void	FILE_T;

/** �ṩ�������ļ��ķ�����*/
typedef	enum	_apx_file_e_
{
	APX_FILE_SUCESS = 0,
	APX_FILE_DOWNLOADING = -5,	/** �ļ�δ�������*/
	APX_FILE_DOWNLOADED,	/** �ļ����������*/
	APX_FILE_NO_MEM,		/** �ڴ治��*/
	APX_FILE_PARAM_ERR, /** ��������*/
	APX_FILE_UNKOWN, /** ��������*/
	
	APX_FILE_MAX
}apx_file_e;

/** �ļ����ݿ�*/
typedef	struct	_apx_fblock_s_
{
	u64	u64Start;		/** ���ļ���Ŀ�ʼ�ֽ�ƫ�� */
	u64	u64End;			/** �����ֽ�ƫ��*/
	u64	u64Offset;		/** ����ڿ�ʼ�ֽڵ�ƫ�� */
	s32	s32Idx;
	u8	sign[41];
	char padding[8];
}apx_fblk_st;
        
/*------------------------------------------------------------*/
/*                          Exported Functions                                  */
/*------------------------------------------------------------*/
/**
	@brief	apx_file_init
 		�ļ�����ģ���ʼ��
 
	@param[in]	 void	
	@return
		( void  )
	history
		       Author                Date              Modification
		   ----------       ----------       ------------
		      xyfeng     	  	2015-3-19        Initial Draft 
*/
void apx_file_init(void );

/**
	@brief	apx_file_exit
 		�ļ�����ģ����Դ����
 
	@param[in]	 void	
	@return
		( void  )
	history
		       Author                Date              Modification
		   ----------       ----------       ------------
		      xyfeng     	  	2015-3-19        Initial Draft 
*/
void apx_file_exit(void );


/**
	@brief	apx_file_create
			�½��ļ�
 
	@param[in]	 pUrl			url
	@param[in]	 pFName		�ļ�����,����·��
	@param[in]	 u64FSize		�ļ���С
	@param[in]	 s32BlkCnt	�ֿ����
	@param[out]	 pError		�����룬��axp_file_e
	@return
		pError = APX_FILE_SUCESS or APX_FILE_DOWNLOADING��
						�����ļ���Ϣ�ṹ
		pError = APX_FILE_DOWNLOADED or  APX_FILE_NO_MEM 
					or APX_FILE_PARAM_ERR, ����NULL
	history
		       Author                Date              Modification
		   ----------       ----------       ------------
		      xyfeng     	  	2015-3-19        Initial Draft 
*/
FILE_T* apx_file_create( s8 *ps8Url, s8 *ps8FName, u64 u64FSize,
							s32 s32BlkCnt, s32 *ps32Err, u32 u32Upload );

/**
	@brief	apx_file_reset
		�����ļ�
 
	@param[in]	 pFileInfo 	�ļ���Ϣ�ṹ
	@return
		return 0 if success, else return -1
	history
		       Author                Date              Modification
		   ----------       ----------       ------------
		      xyfeng     	  	2015-3-19        Initial Draft 
*/
int apx_file_reset( FILE_T *pFileInfo );

/**
	@brief	apx_file_release
 		�ͷ��ļ�
 
	@param[in]	 pFileInfo		�ļ���Ϣ�ṹ	
	@return
		( void  )
	history
		       Author                Date              Modification
		   ----------       ----------       ------------
		      xyfeng     	  	2015-3-19        Initial Draft 
*/
void apx_file_release( FILE_T *pFileInfo );

/**
	@brief	apx_file_destroy
 		�����ļ�
 
	@param[in]	 pFileInfo		�ļ���Ϣ�ṹ
	@return
		( void  )
	history
		       Author                Date              Modification
		   ----------       ----------       ------------
		      xyfeng     	  	2015-3-19        Initial Draft 
*/
void apx_file_destroy( FILE_T *pFileInfo);

/**
	@brief	apx_file_read
 		��ȡ�ļ�����
 
	@param[in]	 pBuf		�ļ�����
	@param[in]	 szSize		��ȡ�����ݴ�С
	@param[in]	 s32Idx		��������Ӧ�����ݿ�����
	@param[in]	 pFileInfo 	�ļ���Ϣ�ṹ
	@return
		return size of buf if success, other return -1
	history
		       Author                Date              Modification
		   ----------       ----------       ------------
		      xyfeng     	  	2015-3-19        Initial Draft 
*/
ssize_t apx_file_read( void *pBuf, size_t szSize, s32 s32Idx, FILE_T *pFileInfo );

/**
	@brief	apx_file_write
 		д���ļ�����
 
	@param[in]	 pBuf	�ļ�����
	@param[in]	 szSize	д������ݴ�С
	@param[in]	 s32Idx	��������Ӧ�����ݿ�����
	@param[in]	 pFileInfo �ļ���Ϣ�ṹ	
	@return
		return size of buf if success, 
		return APX_FILE_BLK_FULL, if block is full
		return APX_FILE_FULL, if file is full
		other return -1
	history
		       Author                Date              Modification
		   ----------       ----------       ------------
		      xyfeng     	  	2015-3-19        Initial Draft 
*/
ssize_t apx_file_write( void *pBuf, size_t szSize, s32 s32Idx, FILE_T *pFileInfo );

/**
	@brief	apx_file_size
 		�ļ���С
 
	@param[in]	 pFileInfo 	�ļ���Ϣ�ṹ
	@return
		�ļ���С
	history
		       Author                Date              Modification
		   ----------       ----------       ------------
		      xyfeng     	  	2015-3-19        Initial Draft 
*/
u64 apx_file_size( FILE_T *pFileInfo );

/**
	@brief	apx_file_cur_size
 		�������ļ����ļ���С
 
	@param[in]	 pFileInfo 	�ļ���Ϣ�ṹ
	@return
		�������ļ����ļ���С
	history
		       Author                Date              Modification
		   ----------       ----------       ------------
		      xyfeng     	  	2015-3-19        Initial Draft 
*/
u64 apx_file_cur_size( FILE_T *pFileInfo );

/**
	@brief	apx_file_pause
 		��ͣ���أ��رմ򿪵��ļ�
 
	@param[in]	 pFileInfo 	�ļ���Ϣ�ṹ
	@return
		( void  )
	history
		       Author                Date              Modification
		   ----------       ----------       ------------
		      xyfeng     	  	2015-3-19        Initial Draft 
*/
void apx_file_pause( FILE_T *pFileInfo );

/**
	@brief	apx_file_resume
 		�ָ����أ����´��ļ�
 
	@param[in]	 pFileInfo 	�ļ���Ϣ�ṹ
	@return
		( void  )
	history
		       Author                Date              Modification
		   ----------       ----------       ------------
		      xyfeng     	  	2015-3-19        Initial Draft 
*/
void apx_file_resume( FILE_T *pFileInfo );

/**
	@brief	apx_file_blk_info
 		��ȡδ�������block
	
	@param[in]	 pFileInfo 	�ļ���Ϣ�ṹ
	@return
		����δ����������ݿ�
		NULL��ʾû��δ�������block
	history
		       Author                Date              Modification
		   ----------       ----------       ------------
		      xyfeng     	  	2015-3-19        Initial Draft 
*/
apx_fblk_st* apx_file_blk_info( FILE_T *pFileInfo );
void apx_file_blk_reset( FILE_T *pFileInfo, apx_fblk_st *pstblk );
s32 apx_file_blk_cnt( FILE_T *pFileInfo );
s32 apx_file_is_exist( s8 *path, off_t *pSize );
s32 apx_file_mkdir( s8* path );
s32 apx_file_divide_cnt( u64 u64Size, u32 upload );
void* apx_block_point( FILE_T *pFileInfo );
void apx_block_reset( FILE_T *pFileInfo );
void apx_block_set_status( void* pBlk, int index, int status );
void apx_block_set_sign( void* pBlk, int index, char* pSign );
void apx_file_set_cur_size( FILE_T *pFileInfo, u64 u64Size );
void apx_file_update_upsize( FILE_T *pFileInfo );

#ifdef __cplusplus
 }       
#endif /*end __cplusplus */
         
#endif /*end APX_FILE_H_20150210 */       
/** @} */
 
