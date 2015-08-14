/*-----------------------------------------------------------
*      Copyright (c)  AppexNetworks, All Rights Reserved.
*
*FileName:     apx_transfer_cl.c 
*
*Description:  libcurl��װ�ϴ�����ģ��,֧��HTTP(S)��FTP
* 
*History: 
*      Author              Date        	Modification 
*  ----------      ----------  	----------
* 	xyfeng   		2015-4-7     	Initial Draft 
*------------------------------------------------------------*/
/*-----------------------------------------------------------*/
/*                          Include File Header                               */
/*-----------------------------------------------------------*/
/*---Include ANSI C .h File---*/
/** support for strcasestr */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif     
#include <sys/types.h>
#include <unistd.h>
#include <strings.h>

#include <time.h>
#include <ctype.h>
#include "../include/curl/curl.h"

/*---Include Local.h File---*/
#include "../include/apx_hftsc_api.h"
#include "apx_transfer_a2.h"
#include "apx_file.h"
#include "apx_transfer_cl.h"
#include "apx_proto_ctl.h"

/*------------------------------------------------------------*/
/*                          Private Macros Defines                          */
/*------------------------------------------------------------*/
#define	TASK_DEFAULT_NUM		( 50 )
#define	MAX_THREAD_NUM		( 30)
#define	MAX_THREAD_NUM_HTTPS		( 15 )
#define	DEFAULT_THREAD_NUM	( 5 )
#define	MAX_HTTP_HEADERS_NUM		( 30 )

#define	HTTP_RET_OK			( 200 )
#define	HTTP_HEAD_CONTENT_LEN	"Content-Length:"
#define	HTTP_HEAD_ACCEPT_RANGE	"Accept-Ranges:"
#define	HTTP_HEAD_ACCEPT_RANGE_BYTES	"bytes"
#define	HTTP_HEAD_CONTENT_DISPOS	"Content-Disposition:"
#define	HTTP_HEAD_LOCATION			"Location:"
#define	HTTP_HEAD_CONN_CLOSE		"Connection: close"
#define	HTTP_HEAD_ETAGS				"ETag:"
#define	HTTP_HEAD_IF_MATCH			"If-Match:"
#define	HTTP_HEAD_USER_AGENT		"User-Agent: "

#define __set_state( _pHandle_, _state_ )	\
do {										\
	_pHandle_->status = _state_;				\
}while( 0 )

/** ���ô�����صı�־λ */
#define	SET_FLAG( _var_, _bit_ )		\
do {									\
	_var_ |= _bit_;					\
}while( 0 )

/** ���������صı�־λ */
#define	CLEAR_FLAG( _var_, _bit_ )		\
do {										\
	_var_ &= ~( _bit_ );					\
}while( 0 )

#define	GET_FLAG( _var_, _bit_ )		( _var_ & _bit_ )

/** ������� */
#define __check_params( _nu_, _handle_, _ret_ )	\
do {												\
	if( _nu_ >= g_trans_head.u32Limits ) { 			\
		tlog( "index exceed limit num( nu = %u, limit = %u).", _nu_, g_trans_head.u32Limits  );	\
		return _ret_;								\
	}											\
	_handle_ =  g_trans_head.pHandle[_nu_];		\
	if( NULL == _handle_ ) {						\
		tlog( "handle is null( nu = %u ).", _nu_ );		\
		return _ret_;								\
	}											\
}while( 0 )
/** �ڴ��ͷ� */
#define	CL_FREE( _ptr_ )							\
do{												\
	if( _ptr_ != NULL ) {							\
		free( _ptr_ );								\
		_ptr_ = NULL;							\
	}											\
}while( 0 )

/** HTTP ͷ��Ϣ�ͷ� */
#define	CURL_SLIST_FREE( _headers_ )			\
do{ 												\
	if( _headers_ != NULL ) {						\
		curl_slist_free_all( _headers_ );				\
		_headers_ = NULL;						\
	}											\
}while( 0 )


#define HF_DEBUG

#undef tlog
#ifdef HF_DEBUG

#define    tlog( fmt, args... )     \
        do { \
            fprintf( stderr, "[%us-%03d-%s] "fmt"\n",  __get_clock(), __LINE__, __FUNCTION__, ##args ); \
        }while(0)
        
#else
#define    tlog( fmt, args... )
#endif


/*------------------------------------------------------------*/
/*                          Private Type Defines                            */
/*------------------------------------------------------------*/
/** �ƶ˴���������ر�־λ */
enum
{
	PROXY_ENABLE	=	0x01, /** ���ô��� */
	THREAD_PROXY	=	0x02, /** ��ǰ�߳̽��д������� */
	DLOAD_FAILED	=	0x04, /** ��ͨ����ʧ�ܱ�־ */
	PROXY_FAILED	=	0x08, /** ��������ʧ�ܱ�־ */
};

/** �ϴ����ؿ��� ���ݽṹ */
typedef struct _handle_s_
{
	s8*	url;
	s8*	filename;
	s8*	ftp_acct;
	s8 *cookies;  /** support for cookies */
	s8 *referer;  /** support for Referer */
	u64	size;
	u64 ticks_size[2];
	u32 ticks[2]; /** 0: 5s  1: 3s */
	u16	speed_limit; /** �ϴ�������������� */
	u16	conn_limit; /** ����̸߳��� */
	u16	active_conns; /** ����̸߳��� */
	u16	conns; /**  �̸߳��� */
	u8 	proto; /** ����Э��: HTTP(S) FTP */
	u8	range:1, /** ������Ƿ�֧�ֶϵ����� */
		load_type:2, /** 1: download 2: server download   3: serve upload */
		status:1, /** ����״̬ */
		keep_close:1, /** conntent: keep_close */
		checksum:1, /** �ļ�У�� */
		cld_proxy: 1; /** �ƶ˴������� */
	u16 proxy_active:8,
		proxy_num:8; /** �����̸߳��� */
	u64 proxy_size; /** �������ش�С */
	s8 *proxy_url;
	FILE_T *pFile; /** �ļ����ƽṹ */
	s8 *if_match;
	pthread_t thread_id[MAX_THREAD_NUM];
	pthread_rwlock_t rwLock;
	u8 fileId[APX_ID_LEN];
	u8 sign[APX_SIGN_LEN];
}handle_st;        

/** �ļ����ݿ� */
typedef struct _gb_file_s_
{
	handle_st *pHandle;
	apx_fblk_st *pBlk;/** ���ݿ���Ϣ */
}gb_file_st;        

/** �ϴ�����ȫ������ */
typedef struct _trans_head_s_
{	
	u32 u32Cnt; /** ������� */
	u32 u32Limits; /** ���������� */
	handle_st **pHandle; /** ������������ָ�� */
	CURLSH *pDnsShare; /** dns�������ݽṹ */
	struct list_head stHeadList;
	pthread_rwlock_t rwLock;
}trans_head_st;

         
/*------------------------------------------------------------*/
/*                         Global Variables                                */
/*------------------------------------------------------------*/
/*---Extern Variables---*/
        
/*---Local Variables---*/
static s32 g_trans_init;
static trans_head_st g_trans_head;
        
/*------------------------------------------------------------*/
/*                          Local Function Prototypes                       */
/*------------------------------------------------------------*/
        
         
/*------------------------------------------------------------*/
/*                        Functions                                               */
/*------------------------------------------------------------*/

/*---------------------------------------------------------
*Function:    __get_clock
*Description:   
*           ��ȡ�������������ʱ��
*Parameters: 
*	 ( void )	
*Return:
*       ������������ʱ��
*History:
*      xyfeng     	  2015-4-10        Initial Draft 
*-----------------------------------------------------------*/
static u32 __get_clock( void )
{
    u64   now;
    struct timespec tsp; 
    static u32 __inited = 0 ;
    static u64 __startup = 0 ;

    if( 0 == clock_gettime( CLOCK_MONOTONIC, &tsp ) )
    {
        if( 0 == __inited )
        {
            __startup = tsp.tv_sec;
            __inited  = 1;
        }
        now = tsp.tv_sec;

        return ( u32 )( now - __startup );
    }

    return 0 ;
}


/*---------------------------------------------------------
*Function:    __share_dns
*Description:   
*           ����dns����
*Parameters: 
*	 ( void )	
*Return:
*       ָ��dns����ṹ
*History:
*      xyfeng     	  2015-4-10        Initial Draft 
*-----------------------------------------------------------*/
static CURLSH* __share_dns( void )
{
	CURLSH *pShare = NULL;

	pShare = curl_share_init();
	if( pShare != NULL )
	{
		curl_share_setopt( pShare, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS );
	}

	return pShare;
}

/** ��ͨ���� �� �ƶ���ת �໥ת�� */
u8 __check_proxy_num( u16 conns )
{
	if( 1 == conns )
	{
		return 0;
	}
	else if( conns < 5 )
	{
		return 1;
	}
	else if( conns < 10 )
	{
		return 2;
	}

	return 3;
}

s8 *__build_proxy_json( handle_st *pstHandle, s8 *pRange, size_t sLen )
{
	size_t size = 0;
	s8* pJson = NULL;
	s8* pHeaders[MAX_HTTP_HEADERS_NUM] = { NULL };
	s8 s8IfMatch[BUF_LEN_MAX ];
	s8 *pTmp = NULL;
	
	if( sLen > 0 && pRange != NULL )
	{
		pHeaders[size++] = pRange;
	}
	
	if( pstHandle->cookies != NULL )
	{
		pHeaders[size++] = pstHandle->cookies;
	}

	if( pstHandle->referer != NULL )
	{
		pHeaders[size++] = pstHandle->referer;
	}

	if( pstHandle->if_match != NULL )
	{
		pTmp = strchr( pstHandle->if_match, '\"' );
		if( pTmp != NULL )
		{
			pTmp++;
			snprintf( s8IfMatch, sizeof( s8IfMatch ), HTTP_HEAD_IF_MATCH" \\\"%s", pTmp );
				
			pTmp = strrchr( s8IfMatch, '\"' );
			snprintf( pTmp, sizeof( s8IfMatch ) - ( pTmp - s8IfMatch ), "\\\"");
			pHeaders[size++] = s8IfMatch;
		}
		else
		{
			pHeaders[size++] =  pstHandle->if_match;
		}
	}

	return apx_build_proxy_json( pstHandle->url, pHeaders, size );
}

/*-------------------------------------------------
*Function:    __if_switch_download
*Description:   
*	����ʧ��ʱ��
*	��ͨ���غ��ƶ˴��������໥�л�
*Parameters: 
*	pstHandle[IN]	
*	pu8Flag[IN]	
*Return:
*       return 1 if switch to the other download mode, else return 0
*History:
*      xyfeng     	  2015-7-23        Initial Draft 
*---------------------------------------------------*/
s32 __if_switch_download(  handle_st *pstHandle, u8 *pu8Flag )
{
	u8 u8Flag = *pu8Flag;
	s32 s32Ret = 0;
		
	if( GET_FLAG( u8Flag, THREAD_PROXY ) )
	{/** ��ͨ���� */
		SET_FLAG( u8Flag, PROXY_FAILED );
		if( !GET_FLAG( u8Flag, DLOAD_FAILED ) )
		{
			CLEAR_FLAG( u8Flag , THREAD_PROXY);
			pthread_rwlock_wrlock( &pstHandle->rwLock );
			pstHandle->proxy_active--;
			pthread_rwlock_unlock( &pstHandle->rwLock );
			s32Ret = 1;
		}
	}
	else 
	{/** �ƶ˴������� */
		SET_FLAG( u8Flag, DLOAD_FAILED );
		if( GET_FLAG( u8Flag, PROXY_ENABLE )
			&& !GET_FLAG( u8Flag, PROXY_FAILED ) )
		{
			SET_FLAG( u8Flag , THREAD_PROXY);
			pthread_rwlock_wrlock( &pstHandle->rwLock );
			pstHandle->proxy_active++;
			pthread_rwlock_unlock( &pstHandle->rwLock );
			s32Ret = 1;
		}
	}

	*pu8Flag = u8Flag;
	return s32Ret;
}

/*---------------------------------------------------------
*Function:    __get_fname_from_url
*Description:   
*           ��url �н����ļ���
*Parameters: 
*	url		
*	filename[out]	�ļ���
*Return:
*       return 0 if success, other return -1
*History:
*      xyfeng     	  2015-4-10        Initial Draft 
*-----------------------------------------------------------*/
static int __get_fname_from_url( s8 *url, s8 **filename )
{
	size_t k = 0;
	size_t size = 0;
	s8 *pstr = NULL;
	const s8 *pos = NULL;
	const s8 *tmp = NULL;

	*filename = NULL;

	pos = strstr( url, "://" );
	pos = ( pos ) ? ( pos + 3 ) : url;

	pos = strrchr( pos, '/' );
	if( NULL == pos )
	{
		return -1;
	}
	pos++;

	tmp = strrchr( pos, '?' );
	size = ( tmp ) ? ( size_t )( tmp - pos ): strlen( pos );
	*filename = calloc( 1, size + 1 );
	if( !*filename )
	{
		return -1;
	}
	strncpy( *filename, pos, size );

	pstr = *filename;
	for( k = 0; k <  size; k++ )
	{
		if( pstr[k] == '\r' ||pstr[k] == '\n')
		{
			pstr[k] = 0;
			break;
		}
	}

	return 0;
}

static char *__parse_filename( const char *ptr, size_t len )
{
	char *copy;
	char *p;
	char *q;
	char  stop = '\0';

	/* simple implementation of strndup() */
	copy = malloc(len+1);
	if(!copy)
		return NULL;
	memcpy(copy, ptr, len);
	copy[len] = '\0';

	p = copy;
	if(*p == '\'' || *p == '"')
	{
		/* store the starting quote */
		stop = *p;
		p++;
	}
	else
		stop = ';';

	/* if the filename contains a path, only use filename portion */
	q = strrchr(copy, '/');
	if(q)
	{
		p = q + 1;
		if(!*p)
		{
			free(copy);
			return NULL;
		}
	}

	/* If the filename contains a backslash, only use filename portion. The idea
	is that even systems that don't handle backslashes as path separators
	probably want the path removed for convenience. */
	q = strrchr(p, '\\');
	if(q)
	{
		p = q + 1;
		if(!*p)
		{
			free(copy);
			return NULL;
		}
	}

	/* scan for the end letter and stop there */
	q = p;
	while(*q) 
	{
		if(q[1] && (q[0] == '\\'))
			q++;
		else if(q[0] == stop)
			break;
		q++;
	}
	*q = '\0';

	/* make sure the file name doesn't end in \r or \n */
	q = strchr(p, '\r');
	if(q)
		*q = '\0';

	q = strchr(p, '\n');
	if(q)
		*q = '\0';

	if(copy != p)
		memmove(copy, p, strlen(p) + 1);

	return copy;
}


/*---------------------------------------------------------
*Function:    __head_filename
*Description:   
*           ��http ��Ϣͷ�н����ļ���
*Parameters: 
*	ptr		http header buf
*	size		buf size
*	ppfilename[out]	file name	
*Return:
*       void 
*History:
*      xyfeng     	  2015-4-10        Initial Draft 
*-----------------------------------------------------------*/
static void __head_filename(void *ptr, size_t size, s8** ppfilename  )
{
	const char *end = (char*)ptr + size;
	const char *p = ptr;

	*ppfilename = NULL;
	/* look for the 'filename=' parameter
	(encoded filenames (*=) are not supported) */
	for(;;)
	{
		char *filename;
		size_t len;

		while( *p && ( p < end ) && !isalpha( *p ) )
			p++;
		if( p > end - 9 )//strlen( "filename=" )
			break;

		if( memcmp( p, "filename=", 9 ) )
		{
			/* no match, find next parameter */
			while( ( p < end ) && ( *p != ';' ) )
				p++;
			continue;
		}
		p += 9;
		/* this expression below typecasts 'cb' only to avoid
		warning: signed and unsigned type in conditional expression
		*/
		len = (ssize_t)size - (p - (char*)ptr);
		filename = __parse_filename(p, len);
		if(filename)
		{
			*ppfilename = filename;
			break;
		}
		else
		return;
	}

	return;
}

static size_t __head_null( void *buffer, size_t size,
								size_t nmemb, void *stream )
{
	return 0;
}
/*---------------------------------------------------------
*Function:    __head_response
*Description:   
*           parsr response header message for HEAD request
*Parameters: 
*	buffer	
*	size	
*Return:
*       buffer size
*History:
*      xyfeng     	  2015-4-10        Initial Draft 
*-----------------------------------------------------------*/
static size_t __head_response( void *buffer, size_t size,
								size_t nmemb, void *stream )
{
	s8 *pStr = NULL;
	size_t sLen = 0;
	handle_st *pstHandle = ( handle_st* )stream;

	pStr = (s8*)strcasestr( buffer, HTTP_HEAD_CONTENT_LEN );
	if( pStr != NULL )
	{
		long long llSize = atoll( pStr + 15 );//strlen( HTTP_HEAD_CONTENT_LEN )
		if( llSize > 0 )
		{
			pstHandle->size = llSize;
		}
	}
	else if( ( pStr = strcasestr( buffer, HTTP_HEAD_ACCEPT_RANGE ) ) != NULL )
	{	
		pStr += 14;//strlen( HTTP_HEAD_ACCEPT_RANGE )
		if( pStr != NULL && strcasestr( pStr, HTTP_HEAD_ACCEPT_RANGE_BYTES ) )
		{
			pstHandle->range = 1;
		}
	}
	else if( ( pStr = strcasestr( buffer, HTTP_HEAD_CONTENT_DISPOS ) ) != NULL )
	{
		pStr += 20;//strlen( HTTP_HEAD_CONTENT_DISPOS )
		if( pStr != NULL )
		{
			CL_FREE( pstHandle->filename );
			__head_filename( pStr, strlen( pStr ), &pstHandle->filename );
		}
	}
	else if( ( pStr = strcasestr( buffer, HTTP_HEAD_LOCATION ) ) != NULL )
	{
		pStr += 9;//strlen( HTTP_HEAD_LOCATION )
		if( pStr != NULL )
		{
			CL_FREE( pstHandle->filename );
			__get_fname_from_url( pStr, &pstHandle->filename );
		}
	}
	else if( ( pStr = strcasestr( buffer, HTTP_HEAD_CONN_CLOSE ) ) != NULL )
	{
		pstHandle->keep_close = 1;
	}
	else if( NULL == pstHandle->if_match  
		&& ( pStr = strcasestr( buffer, HTTP_HEAD_ETAGS ) ) != NULL )
	{
		s8 *pTmp = NULL;
		s8 *pEnd = NULL;

		pStr += 6;//strlen( HTTP_HEAD_ETAGS )
		if( pStr != NULL )
		{
			sLen =  strlen( pStr );
			if( pStr[sLen - 1] == '\n' )
			{
				pStr[sLen - 1] = 0;
				if( pStr[sLen - 2] == '\r' )
				{
					pStr[sLen - 2] = 0;
					
				}
			}
			
			pTmp = strchr( pStr, '\n' );
			if( pTmp != NULL )
			{
				*pTmp = 0;
			}
			pTmp = strchr( pStr, '\r' );
			if( pTmp != NULL )
			{
				*pTmp = 0;
			}

			sLen =  strlen( pStr ) + 14;//  strlen( HTTP_HEAD_IF_MATCH ) + 5
			pstHandle->if_match = calloc( 1, sLen );
			if( pstHandle->if_match )
			{
				snprintf( pstHandle->if_match, sLen, HTTP_HEAD_IF_MATCH" %s", pStr );
			}
		}
	}

	return size * nmemb;
}

static void __curl_setopt_common( CURL *pstCurl )
{
	curl_easy_setopt( pstCurl, CURLOPT_NOPROGRESS, 1L );
	curl_easy_setopt( pstCurl, CURLOPT_CONNECTTIMEOUT, 30L );
	//curl_easy_setopt( pstCurl, CURLOPT_TIMEOUT, 30L );
	curl_easy_setopt( pstCurl, CURLOPT_FOLLOWLOCATION, 1L );
	curl_easy_setopt( pstCurl, CURLOPT_SSL_VERIFYHOST, 0L );
	curl_easy_setopt( pstCurl, CURLOPT_SSL_VERIFYPEER, 0L );
	curl_easy_setopt( pstCurl, CURLOPT_NOSIGNAL, 1L );
	//curl_easy_setopt( pstCurl, CURLOPT_FORBID_REUSE, 1L );
	//curl_easy_setopt( pstCurl, CURLOPT_VERBOSE, 1);
	if( g_trans_head.pDnsShare != NULL )
	{
		curl_easy_setopt( pstCurl, CURLOPT_SHARE, g_trans_head.pDnsShare );
		curl_easy_setopt( pstCurl, CURLOPT_DNS_CACHE_TIMEOUT, 60 * 5);
	}
	return;
}

static struct curl_slist* __curl_set_headers(  CURL *pstCurl, handle_st *pstHandle )
{
#ifndef SIGN_DISABLE
	int ret = 0,
		sLen = 0;
	char userAgent[BUF_LEN_MAX] = {0};
#endif
	struct curl_slist *pstHeaders = NULL;

	if( pstHandle->cookies != NULL )
	{
		pstHeaders = curl_slist_append( pstHeaders, pstHandle->cookies );
	}
	
	if( pstHandle->referer != NULL )
	{
		pstHeaders = curl_slist_append( pstHeaders, pstHandle->referer );
	}
	
	if( pstHandle->if_match )
	{
		pstHeaders = curl_slist_append( pstHeaders, pstHandle->if_match );
	}

#ifndef SIGN_DISABLE
{
	sLen = snprintf( userAgent, sizeof( userAgent ), HTTP_HEAD_USER_AGENT );
	ret = apx_conf_useragent_get( &userAgent[sLen], sizeof( userAgent ) - sLen);
	if( 0 == ret )
	{
		if( strlen( &userAgent[sLen] ) > 0 )
		{
			pstHeaders = curl_slist_append( pstHeaders, userAgent );
		}
	}
}
#endif

	if( pstHeaders != NULL )
	{
		curl_easy_setopt( pstCurl, CURLOPT_HTTPHEADER, pstHeaders );
	}
	
	if( APX_TASK_PROTO_FTP == pstHandle->proto )
	{
		if( pstHandle->ftp_acct != NULL )
		{
			curl_easy_setopt( pstCurl, CURLOPT_USERPWD, pstHandle->ftp_acct );
		}
	}
	else
	{
		curl_easy_setopt( pstCurl, CURLOPT_HTTPGET, 1L );
	}

	return pstHeaders;
}

static void __reset_thread_id(  handle_st *pstHandle, pthread_t id )
{
	int k = 0;
	
	for( k < 0; k < MAX_THREAD_NUM; k++ )
	{
		if( pstHandle->thread_id[k] == id )
		{
			pstHandle->thread_id[k] = 0;
			break;
		}
	}
	return;
}

/*---------------------------------------------------------
*Function:    __request_head
*Description:   
*          perform HEAD request
*Parameters: 
*	pUrl	
*	pstHandle	
*Return:
*       return 0 if success, other return -1
*History:
*      xyfeng     	  2015-4-10        Initial Draft 
*-----------------------------------------------------------*/
static s32 __request_head( s8* pUrl, handle_st *pstHandle )
{
	long lHttpCode = 0;
	double filesize = 0.0;
	CURLcode eRes;
	CURL *pstCurl = NULL;
	struct curl_slist *pstHeaders = NULL;
	size_t sLen = 0;
	s8 s8Url[BUF_LEN_MAX * 2];

	pstCurl = curl_easy_init();
	if( NULL == pstCurl )
	{
		return -1;
	}
	
	__curl_setopt_common( pstCurl );
	pstHeaders = __curl_set_headers( pstCurl, pstHandle );
	if( APX_TASK_TYPE_SERVER_DOWN != pstHandle->load_type
		&& APX_TASK_PROTO_FTP != pstHandle->proto )
	{
		curl_easy_setopt( pstCurl, CURLOPT_URL, pUrl ); 
		curl_easy_setopt( pstCurl, CURLOPT_BUFFERSIZE, 1L );
		
		curl_easy_setopt( pstCurl, CURLOPT_WRITEFUNCTION, __head_null );
		curl_easy_setopt( pstCurl, CURLOPT_WRITEDATA, pstHandle );
		
		curl_easy_setopt( pstCurl, CURLOPT_HEADERFUNCTION, __head_response );
		curl_easy_setopt( pstCurl, CURLOPT_HEADERDATA, pstHandle );
	}
	else
	{
		if( APX_TASK_TYPE_SERVER_DOWN == pstHandle->load_type )
		{
			sLen = snprintf( s8Url, sizeof( s8Url ), "%s", pUrl );
			apx_cloud_sign( &s8Url[sLen], sizeof( s8Url ) - sLen );
			curl_easy_setopt( pstCurl, CURLOPT_URL, s8Url ); 
		}
		else
		{
			curl_easy_setopt( pstCurl, CURLOPT_URL, pUrl ); 
		}
		
		curl_easy_setopt( pstCurl, CURLOPT_HEADER, 1L );
		curl_easy_setopt( pstCurl, CURLOPT_NOBODY, 1L );
		
		curl_easy_setopt( pstCurl, CURLOPT_WRITEFUNCTION, __head_response );
		curl_easy_setopt( pstCurl, CURLOPT_WRITEDATA, pstHandle );
		
	}

	eRes = curl_easy_perform(  pstCurl );
	if( eRes != CURLE_OK && eRes != CURLE_GOT_NOTHING && eRes != CURLE_WRITE_ERROR ) {
		tlog( "perfrom failed( Res:%d, %s )%s.", eRes, curl_easy_strerror( eRes ), pUrl );
		CURL_SLIST_FREE( pstHeaders );
		curl_easy_cleanup( pstCurl );
		return -2;
	}
	eRes = curl_easy_getinfo( pstCurl, CURLINFO_HTTP_CODE, &lHttpCode);
	CURL_SLIST_FREE( pstHeaders );
	curl_easy_cleanup( pstCurl );
	if( eRes != CURLE_OK || HTTP_403 == lHttpCode || HTTP_404 == lHttpCode )
	{
		tlog( "curl_easy_getinfo failed( retCode: %ld, Res:%d, %s ).", lHttpCode, eRes, curl_easy_strerror( eRes ) );
		return -3;
	}

	if( APX_TASK_PROTO_FTP == pstHandle->proto
		&& 350 == lHttpCode )
	{
		lHttpCode = HTTP_RET_OK;
	}
	
	return ( HTTP_RET_OK == lHttpCode ) ? 0 : -4;
}

static s32 __request_size( handle_st* pHandle )
{
	s32 s32Err;
	handle_st stHandle;

	memset( &stHandle, 0, sizeof( handle_st ) );
	stHandle.load_type	=	pHandle->load_type;
	stHandle.proto	=	pHandle->proto;
	stHandle.url		=	pHandle->url;
	stHandle.ftp_acct	=	pHandle->ftp_acct;
	stHandle.cookies	=	pHandle->cookies;
	stHandle.referer	=	pHandle->referer;
	
	s32Err = __request_head( pHandle->url, &stHandle );
	if( s32Err < 0 )
	{
		tlog( "http head failed(url: %s ).", pHandle->url );
		return -1;
	}
	
	pHandle->size = stHandle.size;
	pHandle->range = stHandle.range;
	if( !pHandle->keep_close )
	{
		pHandle->keep_close = stHandle.keep_close;
	}
	if( NULL == pHandle->if_match )
	{
		pHandle->if_match = stHandle.if_match;
	}
	else if( stHandle.if_match != NULL )
	{
		CL_FREE( stHandle.if_match );
	}
	CL_FREE( stHandle.filename );
	
	return 0;
}

void __set_ftp_acct( handle_st *pstHandle, s8* user, s8* passwd )
{
	s32 size = 0;
	size_t sLen1 = 0,
		   sLen2 = 0;
	
	if( user && passwd && APX_TASK_PROTO_FTP == pstHandle->proto )
	{
		sLen1 = strlen( user );
		sLen2 = strlen( passwd );
		if( sLen1 > 0 && sLen2 > 0 )
		{
			/** user:passwd */
			pstHandle->ftp_acct = calloc( 1, sLen1 + sLen2 + 2 );
			if( pstHandle->ftp_acct != NULL )
			{
				size = snprintf( pstHandle->ftp_acct, sLen1 + sLen2 + 2,
						"%s:%s", user, passwd );
				pstHandle->ftp_acct[size] = '\0';
			}
		}
	}
	return;
}

s8 *__joint_filename( s8* pPath, s8* pName )
{
	s32 size = 0;
	size_t sLen1 = strlen( pPath ),
		   sLen2 = strlen( pName );
	s8 *pFileName = NULL;

	/** path/file */
	pFileName = calloc( 1, sLen1 + sLen2 + 5 );
	if( pFileName != NULL )
	{
		if( pPath[sLen1 - 1] == '/' )
		{
			size = snprintf( pFileName, sLen1 + sLen2 + 5, "%s%s", pPath, pName );
		}
		else
		{
			size= snprintf( pFileName, sLen1 + sLen2 + 5, "%s/%s", pPath, pName );
		}
		pFileName[size] = '\0';
	}

	return pFileName;
}

/*---------------------------------------------------------
*Function:    __options_assign
*Description:   
*           
*Parameters: 
*	pstHandle	
*	glb	
*	opt	
*Return:
*       static void :
*History:
*      xyfeng     	  2015-4-10        Initial Draft 
*-----------------------------------------------------------*/
static void __options_assign( handle_st *pstHandle, struct apx_trans_glboptions* glb, struct apx_trans_opt* opt )
{	
	pstHandle->proto 	= 	opt->proto;
	pstHandle->range 	= 	opt->bp_continue;
	pstHandle->size 		= 	opt->fsize;
	pstHandle->load_type = 	opt->type;
	pstHandle->url		=	strdup( opt->uri );
	pstHandle->cookies	= 	opt->cookie;
	pstHandle->referer	= 	opt->referer;
	pstHandle->keep_close=	opt->keep_close;
	pstHandle->cld_proxy	=	opt->cloud_proxy;
	pstHandle->conn_limit = DEFAULT_THREAD_NUM;

	if( strlen( opt->if_match ) > 0 )
	{
		pstHandle->if_match =	strdup( opt->if_match );
	}
	else
	{
		pstHandle->if_match =	NULL;
	}

	if( APX_TASK_TYPE_SERVER_UP == pstHandle->load_type
		|| APX_TASK_TYPE_SERVER_DOWN == pstHandle->load_type )
	{	
		strncpy( pstHandle->fileId, opt->fileId, sizeof( pstHandle->fileId ) - 1 );
		if(  APX_TASK_TYPE_SERVER_UP == pstHandle->load_type )
		{
			pstHandle->range = 1;
		}
		else
		{
			pstHandle->checksum = opt->checksum;
		}
	}
	else if( NULL != apx_cloud_get_url() )
	{
		pstHandle->cld_proxy = opt->cloud_proxy;
	}
	
	if( glb )
	{
		if( pstHandle->proto != APX_TASK_PROTO_HTTPS )
		{
			pstHandle->conn_limit = ( glb->connections > MAX_THREAD_NUM ) ? MAX_THREAD_NUM : glb->connections;
		}
		else
		{
			pstHandle->conn_limit = ( glb->connections > MAX_THREAD_NUM_HTTPS ) ? MAX_THREAD_NUM_HTTPS : glb->connections;
		}
		
		if( pstHandle->load_type == APX_TASK_TYPE_SERVER_UP )
		{
			pstHandle->speed_limit = glb->max_limit_uploadspeed;
		}
		else
		{
			pstHandle->speed_limit = glb->max_limit_downspeed;
		}
	}
	
	if( opt->concurr > 0 )
	{
		if( pstHandle->proto != APX_TASK_PROTO_HTTPS )
		{
			pstHandle->conn_limit = ( opt->concurr > MAX_THREAD_NUM ) ? MAX_THREAD_NUM : opt->concurr;
		}
		else
		{
			pstHandle->conn_limit = ( opt->concurr > MAX_THREAD_NUM_HTTPS ) ? MAX_THREAD_NUM_HTTPS : opt->concurr;
		}
	}
	
	if( pstHandle->proto != APX_TASK_PROTO_HTTPS )
	{
		pstHandle->conn_limit = pstHandle->conn_limit > MAX_THREAD_NUM? MAX_THREAD_NUM : pstHandle->conn_limit;
		
	}
	else
	{
		pstHandle->conn_limit = pstHandle->conn_limit > MAX_THREAD_NUM_HTTPS ? MAX_THREAD_NUM_HTTPS : pstHandle->conn_limit;
	}
	
	if( pstHandle->load_type == APX_TASK_TYPE_SERVER_UP )
	{
		if( opt->up_splimit > 0 )
		{
			pstHandle->speed_limit = opt->up_splimit;
		}
	}
	else
	{
		if( opt->down_splimit > 0 )
		{
			pstHandle->speed_limit = opt->down_splimit;
		}
	}
	
	pstHandle->filename = __joint_filename( opt->fpath, opt->fname );
	if( APX_TASK_PROTO_FTP == pstHandle->proto )
	{
		__set_ftp_acct( pstHandle,  opt->ftp_user, opt->ftp_passwd );
	}

	return;
}
s32 __dir_check( struct apx_trans_opt* task_opt )
{
	int ret = 0;
	
	ret = apx_file_is_exist( task_opt->fpath, NULL ); 
	return ret ? 0 : apx_file_mkdir( task_opt->fpath );
}

s32 __options_check( handle_st *pstHandle )
{
	
	if( NULL == pstHandle->url 
		|| NULL == pstHandle->filename )
	{
		tlog( "parameters err( url or filename  is null )." );
		return -1;
	}
	
	if( APX_TASK_PROTO_HTTP != pstHandle->proto
		&& APX_TASK_PROTO_HTTPS != pstHandle->proto
		&& APX_TASK_PROTO_FTP != pstHandle->proto )
	{
		tlog( "unkown protocol type( proto = %u ).",  pstHandle->proto );
		return -1;
	}

	if( APX_TASK_TYPE_DOWN != pstHandle->load_type
		&& APX_TASK_TYPE_SERVER_DOWN != pstHandle->load_type
		&& APX_TASK_TYPE_SERVER_UP != pstHandle->load_type )
	{
		tlog( "unkown task type( upload = %d ).", pstHandle->load_type );
		return -1;
	}

	return 0;
}

static size_t __file_callback( void *buffer, size_t size,
								size_t nmemb, void *stream )
{
	ssize_t slen = 0;
	apx_fblk_st* pstblk = (( gb_file_st* )stream)->pBlk; 
	handle_st *pstHandle = (( gb_file_st* )stream)->pHandle;

	if( APX_TASK_TYPE_SERVER_UP  != pstHandle->load_type )
	{
		slen = apx_file_write( buffer, size * nmemb, pstblk->s32Idx, pstHandle->pFile );
	}
	else
	{
		slen = apx_file_read( buffer, size * nmemb, pstblk->s32Idx, pstHandle->pFile );
	}
	
	return slen < 0 ? 0 : slen;
}

static void *__download_thread( void *arg )
{
	u8 u8CldProxy = 0;
	s8 *pProxyJson = NULL;
	
	u8 u8FirstFailed = 0;
	u16 u16ActConns = 0;
	s32 s32Ret = 0;
	
	s8 range[BUF_LEN_MAX];
	size_t sLen = 0;
	size_t sRgLen = sizeof( range );
	s8 s8Url[BUF_LEN_MAX * 2];
	
	long lHttpCode = 0;
	CURLcode eRes;
	CURL *pstCurl = NULL;
	struct curl_slist *pstHeaders = NULL;
	
	apx_fblk_st* pstblk = NULL; 
	handle_st *pstHandle = ( handle_st *)arg;
	gb_file_st stGbFile;
	pthread_t thread_id = pthread_self( );
	
	pthread_detach( thread_id);
	pstCurl = curl_easy_init();
	if( NULL == pstCurl )
	{
		return NULL;
	}

	pthread_rwlock_wrlock( &pstHandle->rwLock );
	pstHandle->active_conns++;
	if( pstHandle->cld_proxy
		&& pstHandle->proxy_active < pstHandle->proxy_num )
	{
		SET_FLAG( u8CldProxy, PROXY_ENABLE );
		SET_FLAG( u8CldProxy, THREAD_PROXY );
		pstHandle->proxy_active++;
	}
	pthread_rwlock_unlock( &pstHandle->rwLock );

	while( pstHandle->status
		&& ( pstblk = apx_file_blk_info( pstHandle->pFile ) ) != NULL )
	{
		stGbFile.pHandle = pstHandle;
		stGbFile.pBlk = pstblk;

		retry:
		pstblk->u8Proxy = 0;
		__curl_setopt_common( pstCurl );
		curl_easy_setopt( pstCurl, CURLOPT_WRITEDATA, &stGbFile );
		curl_easy_setopt( pstCurl, CURLOPT_WRITEFUNCTION, __file_callback );
		sLen = 0;
		if( pstHandle->range )
		{
			if( GET_FLAG( u8CldProxy, THREAD_PROXY ) )
			{
				sLen = snprintf( range, sRgLen, "Range: bytes=" );
			}

			sLen += snprintf( range + sLen, sRgLen - sLen, "%llu-",
					pstblk->u64Start + pstblk->u64Offset );
			if( pstblk->u64End != 0 )
			{
				sLen += snprintf( range + sLen, sRgLen - sLen, "%llu", pstblk->u64End );
			}
		}
		
		if( GET_FLAG( u8CldProxy, THREAD_PROXY ) )
		{
			pProxyJson = __build_proxy_json( pstHandle,range, sLen );
			if( NULL == pProxyJson )
			{
				tlog( "fatal: build post content failed." );
				
				CLEAR_FLAG( u8CldProxy , THREAD_PROXY);
				pthread_rwlock_wrlock( &pstHandle->rwLock );
				pstHandle->proxy_active--;
				pthread_rwlock_unlock( &pstHandle->rwLock );
				curl_easy_reset( pstCurl );
				goto retry;
			}

			pstblk->u8Proxy = 1;
			curl_easy_setopt( pstCurl, CURLOPT_POSTFIELDS, pProxyJson );
			curl_easy_setopt( pstCurl, CURLOPT_URL, pstHandle->proxy_url ); 
			pstHeaders = curl_slist_append( pstHeaders, "Content-type: application/json" );
			curl_easy_setopt( pstCurl, CURLOPT_HTTPHEADER, pstHeaders );
		}
		else
		{
			if( APX_TASK_TYPE_SERVER_DOWN != pstHandle->load_type )
			{
				curl_easy_setopt( pstCurl, CURLOPT_URL, pstHandle->url );
			}
			else
			{
				sLen = snprintf( s8Url, sizeof( s8Url ), "%s", pstHandle->url );
				apx_cloud_sign( &s8Url[sLen], sizeof( s8Url ) - sLen );
				curl_easy_setopt( pstCurl, CURLOPT_URL, s8Url ); 
			}
			if( pstHandle->range )
			{
				curl_easy_setopt( pstCurl, CURLOPT_RANGE, range );
			}
			
			if(pstHandle->speed_limit > 0 )
			{
				pthread_rwlock_rdlock( &pstHandle->rwLock );
				u16ActConns = ( 0 == u16ActConns ) ? pstHandle->conns : pstHandle->active_conns;
				pthread_rwlock_unlock( &pstHandle->rwLock );

				curl_easy_setopt( pstCurl, CURLOPT_MAX_RECV_SPEED_LARGE,
						( curl_off_t )( pstHandle->speed_limit * 1024 /u16ActConns ) );
			}
			
			pstHeaders = __curl_set_headers( pstCurl, pstHandle );
		}
		
		
		eRes = curl_easy_perform(  pstCurl );
		if( eRes != CURLE_OK ) {
			CL_FREE( pProxyJson );
			CURL_SLIST_FREE( pstHeaders );
			curl_easy_reset( pstCurl );

			if( u8FirstFailed < 2 && pstHandle->status )
			{/** ͬʱ�������̣߳�����������־ܾ������ */
				u8FirstFailed++;
				sleep( u8FirstFailed + 1 );
				goto retry;
			}
			tlog( "perfrom failed( Res:%d, %s ).", eRes, curl_easy_strerror( eRes ) );

			u8FirstFailed = 0;
			s32Ret = __if_switch_download( pstHandle, &u8CldProxy );
			if( s32Ret )
			{
				goto retry;
			}
			
			apx_file_blk_reset( pstHandle->pFile, pstblk );
			break;
		}

		if( APX_TASK_PROTO_FTP != pstHandle->proto )
		{
			eRes = curl_easy_getinfo( pstCurl, CURLINFO_HTTP_CODE, &lHttpCode);
			if( eRes != CURLE_OK || ( lHttpCode != HTTP_OK && lHttpCode != HTTP_206 ) )
			{
				tlog( "curl_easy_getinfo failed( Res:%d, code: %ld, %s ).", eRes, lHttpCode, curl_easy_strerror( eRes ) );
				apx_file_blk_reset( pstHandle->pFile, pstblk );
				if( HTTP_412 == lHttpCode || HTTP_410 == lHttpCode )
				{ /** �ļ��Ѹı� */
					u8FirstFailed = 0;
					CL_FREE( pProxyJson );
					CURL_SLIST_FREE( pstHeaders );
					break;
				}
			}
		}
		u8FirstFailed = 0;
		CL_FREE( pProxyJson );
		CURL_SLIST_FREE( pstHeaders );
		curl_easy_reset( pstCurl );
		
		if( APX_TASK_TYPE_SERVER_DOWN == pstHandle->load_type
			&& 1 == pstHandle->checksum )
		{
			s32Ret = apx_block_checksum( pstHandle->pFile, pstblk->s32Idx );
			if( -4 == s32Ret )
			{
				tlog( "blcok[%d] checksum err, retry it.", pstblk->s32Idx );
				apx_file_blk_reset( pstHandle->pFile, pstblk );
			}
		}
	}
	
	curl_easy_cleanup( pstCurl );
	pthread_rwlock_wrlock( &pstHandle->rwLock );
	pstHandle->active_conns--;
	if( GET_FLAG( u8CldProxy, THREAD_PROXY ) )
	{
		pstHandle->proxy_active--;
	}
	__reset_thread_id( pstHandle, thread_id );
	pthread_rwlock_unlock( &pstHandle->rwLock );
	
	/** file download done, if file size = 0 */
	if( !pstHandle->size  && !pstHandle->active_conns 
		|| ( APX_TASK_TYPE_SERVER_DOWN == pstHandle->load_type 
			&& pstHandle->size == apx_file_cur_size( pstHandle->pFile )) )
	{
		apx_file_write( NULL, 0,0, pstHandle->pFile );
	}

	return NULL;
}

static size_t __upload_response( void *buffer, size_t size, size_t nmemb, void *stream )
{
	tlog( "\n\n%s\n\n", ( char* )buffer );
	return size * nmemb;
}
static void *__upload_thread( void *arg )
{
	u64 u64BlkSize = 0;
	u8 u8FirstFailed = 0;
	u16 u16ActConns = 0;
	s8 tmp[BUF_LEN_MAX];
	CURLcode eRes;
	CURL *pstCurl = NULL;
	struct curl_httppost *pForm = NULL;
	struct curl_httppost *pLast = NULL;
	struct curl_slist *pstHeaders = NULL;
	apx_fblk_st* pstblk = NULL; 
	handle_st *pstHandle = ( handle_st *)arg;
	gb_file_st stGbFile;
	pthread_t thread_id = pthread_self( );
	s8* pFileName = NULL;
	long lHttpCode = 0;
	size_t sLen = 0;
	s8 s8Url[BUF_LEN_MAX * 2];

	pthread_detach( thread_id);
	pstCurl = curl_easy_init();
	if( NULL == pstCurl )
	{
		return NULL;
	}

	/** last name */
	pFileName = strrchr( pstHandle->filename, '/');
	pFileName = pFileName ? ( pFileName + 1 ) :  pstHandle->filename;

	pthread_rwlock_wrlock( &pstHandle->rwLock );
	pstHandle->active_conns++;
	pthread_rwlock_unlock( &pstHandle->rwLock );
	
	while( pstHandle->status
		&& ( pstblk = apx_file_blk_info( pstHandle->pFile ) ) != NULL )
	{
		stGbFile.pHandle = pstHandle;
		stGbFile.pBlk = pstblk;
		u64BlkSize = pstblk->u64End - pstblk->u64Start -  pstblk->u64Offset + 1;

		retry:
		curl_formadd( &pForm,
					&pLast,
					CURLFORM_COPYNAME, "file",
					CURLFORM_STREAM, &stGbFile,
					CURLFORM_CONTENTSLENGTH, (long)u64BlkSize,
					CURLFORM_FILENAME, pFileName,
					CURLFORM_END );

		curl_formadd( &pForm,
					&pLast,
					CURLFORM_COPYNAME, "fileId",
					CURLFORM_COPYCONTENTS, pstHandle->fileId,
					CURLFORM_END );
		
		snprintf( tmp, sizeof( tmp ), "%d", pstblk->s32Idx + 1 );
		curl_formadd( &pForm,
					&pLast,
					CURLFORM_COPYNAME, "index",
					CURLFORM_COPYCONTENTS, tmp,
					CURLFORM_END );
		
		__curl_setopt_common( pstCurl );
		curl_easy_setopt( pstCurl, CURLOPT_HTTPPOST, pForm );
		curl_easy_setopt( pstCurl, CURLOPT_READFUNCTION, __file_callback );
		curl_easy_setopt( pstCurl, CURLOPT_WRITEFUNCTION, __upload_response  );
		curl_easy_setopt( pstCurl, CURLOPT_WRITEDATA, NULL  );

		sLen = snprintf( s8Url, sizeof( s8Url ), "%s", pstHandle->url );
		apx_cloud_sign( &s8Url[sLen], sizeof( s8Url ) - sLen );
		curl_easy_setopt( pstCurl, CURLOPT_URL, s8Url ); 

		// Disable "Expect: 100-continue"
		pstHeaders = curl_slist_append( pstHeaders, "Expect:" );
		curl_easy_setopt( pstCurl, CURLOPT_HTTPHEADER, pstHeaders );
		
		if(pstHandle->speed_limit > 0 )
		{
			pthread_rwlock_rdlock( &pstHandle->rwLock );
			u16ActConns = ( 0 == u16ActConns ) ? pstHandle->conns : pstHandle->active_conns;
			pthread_rwlock_unlock( &pstHandle->rwLock );

			curl_easy_setopt( pstCurl, CURLOPT_MAX_SEND_SPEED_LARGE,
					( curl_off_t )( pstHandle->speed_limit * 1024 /u16ActConns ) );
		}
		
		eRes = curl_easy_perform(  pstCurl );
		if( eRes != CURLE_OK ) {
			curl_formfree( pForm );
			pForm = pLast = NULL;
			CURL_SLIST_FREE( pstHeaders );

			if( u8FirstFailed < 2 && pstHandle->status )
			{/** ͬʱ�������̣߳�����������־ܾ������ */
				u8FirstFailed++;
				curl_easy_reset( pstCurl );
				sleep( u8FirstFailed + 1 );
				goto retry;
			}
			tlog( "perfrom failed( Res:%d, %s ).", eRes, curl_easy_strerror( eRes ) );
			apx_file_blk_reset( pstHandle->pFile, pstblk );
			break;
		}
		
		eRes = curl_easy_getinfo( pstCurl, CURLINFO_HTTP_CODE, &lHttpCode);
		if( eRes != CURLE_OK || lHttpCode != HTTP_OK )
		{
			tlog( "curl_easy_getinfo failed( Res:%d, code: %ld, %s ).", eRes, lHttpCode, curl_easy_strerror( eRes ) );
			apx_file_blk_reset( pstHandle->pFile, pstblk );
		}

		u8FirstFailed = 0;
		curl_formfree( pForm );
		pForm = pLast = NULL;
		
		CURL_SLIST_FREE( pstHeaders );
		curl_easy_reset( pstCurl );
	}
	
	curl_easy_cleanup( pstCurl );
	pthread_rwlock_wrlock( &pstHandle->rwLock );
	pstHandle->active_conns--;
	__reset_thread_id( pstHandle, thread_id );
	pthread_rwlock_unlock( &pstHandle->rwLock );
	
	return NULL;
}

static s32 __request_fileinfo( handle_st *pstHandle )
{
	s32 s32Err = 0;
	cld_fileinfo_st fileinfo;

	memset( &fileinfo, 0, sizeof( cld_fileinfo_st ) );
	if( APX_TASK_TYPE_SERVER_UP == pstHandle->load_type )
	{
		fileinfo.eReqType = FREQ_UP;
	}
	else
	{
		fileinfo.eReqType = FREQ_DOWN;
	}
	
	fileinfo.blkCnt = apx_file_blk_cnt( pstHandle->pFile );
	fileinfo.pblk = apx_block_point( pstHandle->pFile );
	if( fileinfo.pblk == NULL )
	{
		tlog( "pblk null." );
		return -1;
	}
	strncpy( fileinfo.fileId, pstHandle->fileId, sizeof(  fileinfo.fileId ) - 1 );
	
	s32Err =  apx_cloud_fileinfo( &fileinfo);
	if( s32Err < 0 )
	{
		tlog( "get fileinnfo failed." );
		return -2;
	}
	
	strncpy( pstHandle->sign, fileinfo.sign, sizeof( pstHandle->sign ) - 1 );
	tlog( "fileStatus: %d, eReqType: %d, s32BlkCnt: %d\nsign: %s",
		fileinfo.eStatus, fileinfo.eReqType,
		fileinfo.blkCnt, fileinfo.sign );
	
	extern void apx_file_dump( FILE_T * pFile );
	apx_file_dump( pstHandle->pFile );
	
	if( APX_TASK_TYPE_SERVER_UP == pstHandle->load_type
		&& FILE_UP_DONE == fileinfo.eStatus )
	{
		apx_file_set_cur_size( pstHandle->pFile, pstHandle->size );
		return -4;
	}
	
	return 0;
}

static void __release_handle( handle_st *pstHandle )
{
	pthread_rwlock_wrlock( &pstHandle->rwLock );
	CL_FREE( pstHandle->filename  );
	CL_FREE( pstHandle->url );
	CL_FREE( pstHandle->ftp_acct );
	CL_FREE( pstHandle->if_match );
	if( pstHandle->cld_proxy )
	{
		CL_FREE( pstHandle->proxy_url );
	}
	
	if( pstHandle->pFile )
	{
		apx_file_release( pstHandle->pFile );
		pstHandle->pFile = NULL;
	}
	pthread_rwlock_unlock( &pstHandle->rwLock );
	pthread_rwlock_destroy( &pstHandle->rwLock );
	CL_FREE( pstHandle );
}

/*---------------------------------------------------------
*Function:    apx_trans_init_cl
*Description:   
*           
*Parameters: 
*	nu	
*Return:
*       int :
*History:
*      xyfeng     	  2015-4-10        Initial Draft 
*-----------------------------------------------------------*/
int apx_trans_init_cl( u32 nu )
{
	handle_st **ppstHandle = NULL;

	if( g_trans_init )
	{
		return 0;
	}
	
	apx_file_init();

	g_trans_head.u32Limits = nu > 0 ? nu : TASK_DEFAULT_NUM;
	INIT_LIST_HEAD( &g_trans_head.stHeadList );
	pthread_rwlock_init( &g_trans_head.rwLock, NULL );

	ppstHandle = calloc( g_trans_head.u32Limits, sizeof( handle_st * ) );
	if( NULL == ppstHandle )
	{
		tlog( "calloc failed." );
		return -1;
	}

	g_trans_head.pHandle = ppstHandle;
	g_trans_head.pDnsShare = __share_dns();
	g_trans_init = 1;
	return 0;
}

void apx_trans_exit_cl( void )
{
	u32 k = 0;
	
	if( !g_trans_init )
		return;
	
	if( g_trans_head.pDnsShare != NULL )
	{
		curl_share_cleanup( g_trans_head.pDnsShare );
		g_trans_head.pDnsShare = NULL;
	}

	pthread_rwlock_wrlock( &g_trans_head.rwLock );
	if( g_trans_head.pHandle != NULL )
	{
		for( k = 0; k < g_trans_head.u32Limits; k++ )
		{
			if( g_trans_head.pHandle[k] != NULL )
			{
				if( g_trans_head.pHandle[k]->status )
				{
					apx_trans_stop_cl( k );
				}
				__release_handle( g_trans_head.pHandle[k] );
				g_trans_head.pHandle[k] = NULL;
			}
		}

		CL_FREE( g_trans_head.pHandle );
	}
	pthread_rwlock_unlock( &g_trans_head.rwLock );
	pthread_rwlock_destroy( &g_trans_head.rwLock );
	apx_file_exit();
	g_trans_init = 0;
	
	return;
}


/*---------------------------------------------------------
*Function:    apx_trans_create_cl
*Description:   
*           
*Parameters: 
*	 ( void )	
*Return:
*       int :
*History:
*      xyfeng     	  2015-4-10        Initial Draft 
*-----------------------------------------------------------*/
int apx_trans_create_cl( void )
{
	u32 k = 0;

	if( !g_trans_init )
	{
		tlog( "create err: do not call init function." );
		return -1;
	}
	
	pthread_rwlock_wrlock( &g_trans_head.rwLock );
	for( k = 0; k < g_trans_head.u32Limits; k++ )
	{
		if( NULL == g_trans_head.pHandle[k] )
		{
			break;
		}
	}
	
	if( k < g_trans_head.u32Limits )
	{
		g_trans_head.pHandle[k] = calloc( 1, sizeof( handle_st ) );
		if( g_trans_head.pHandle[k] != NULL )
		{
			__set_state( g_trans_head.pHandle[k], 0 );
			pthread_rwlock_init( &g_trans_head.pHandle[k]->rwLock, NULL );
			pthread_rwlock_unlock( &g_trans_head.rwLock );
			return k;
		}
	}
	pthread_rwlock_unlock( &g_trans_head.rwLock );
	tlog( "create err. not found free handle." );
	return -1;
}

int apx_trans_release_cl( u32 nu, int flags )
{
	handle_st *pstHandle = NULL;

	__check_params( nu, pstHandle, -1 );

	if( pstHandle->status )
	{
		apx_trans_stop_cl( nu );
		//tlog( "release alert, do not call stop befor release." );
		//return -1;
	}	
	__release_handle( pstHandle );
	g_trans_head.pHandle[nu] = NULL;
	
	return 0;
}

int apx_trans_del_file_cl( struct apx_trans_glboptions* glb_opt, struct apx_trans_opt* task_opt )
{
	s8 filename[BUF_LEN_MAX * 2] = { 0  };

	if( NULL == task_opt 
	|| 0 == strlen( task_opt->uri )
	|| 0 == strlen( task_opt->fname ) )
	{
		tlog( "parameters err." );
		return -1;
	}

	if( APX_TASK_TYPE_SERVER_UP == task_opt->type )
	{
		tlog( "no permit to call apx_trans_del_file_cl for uploading." );
		return -2;
	}
	
	if( 0 == strlen( task_opt->fpath ) )
	{
		strncpy( filename, task_opt->fname, sizeof( filename ) - 1 );
		filename[sizeof( filename ) - 1] = 0;
	}
	else
	{
		if( task_opt->fpath[strlen( task_opt->fpath ) - 1] == '/' )
		{
			snprintf( filename, sizeof( filename ), "%s%s", task_opt->fpath, task_opt->fname );
		}
		else
		{
			snprintf( filename, sizeof( filename ), "%s/%s", task_opt->fpath, task_opt->fname );
		}
	}

	apx_file_delete_all( filename );
	return 0;
}

int apx_trans_delete_cl( u32 nu )
{
	handle_st *pstHandle = NULL;

	__check_params( nu, pstHandle,  -1 );
	if( pstHandle->status )
	{
		apx_trans_stop_cl( nu );
		tlog( "delete alert, do not call stop befor delete." );
		//return -1;
	}	
	if( pstHandle->pFile != NULL )
	{
		apx_file_destroy( pstHandle->pFile );
		pstHandle->pFile = NULL;
	}
	__release_handle( pstHandle );
	g_trans_head.pHandle[nu] = NULL;

	return 0;
}
int apx_trans_get_btfile_cl( u32 nu, struct apx_trans_opt* task_opt, struct btfile* bt_file )
{
	return -1;
}

/*---------------------------------------------------------
*Function:    apx_trans_start_cl
*Description:   
*           
*Parameters: 
*	nu	
*	glb_opt	
*	task_opt	
*Return:
*       int :
*History:
*      xyfeng     	  2015-4-10        Initial Draft 
*-----------------------------------------------------------*/
int apx_trans_start_cl(u32 nu, struct apx_trans_glboptions* glb_opt, struct apx_trans_opt* task_opt)
{
	s32 k = 0,
		blkCnt = 0;
	s32 s32Err = 0;
	u32 m = 0;
	u32 u32FileFlag = 0;
	handle_st *pstHandle = NULL;

	__check_params( nu, pstHandle, -1 );
	if( NULL == task_opt )
	{
		tlog( "task_opt parameter is null." );
		return -2;
	}

	if( task_opt->type != APX_TASK_TYPE_SERVER_UP )
	{
		s32Err = __dir_check( task_opt );
		if( s32Err < 0 )
		{
			tlog( "create dir failed( dir: %s).", task_opt->fpath );
			return -3;
		}
	}
	else if( 0 == strlen( task_opt->fileId ))
	{
		tlog( "trans[%u] fileId is null.", nu  );
		return -3;
	}

	__options_assign( pstHandle, glb_opt, task_opt );
	s32Err = __options_check( pstHandle );
	if( s32Err < 0 )
	{
		tlog( "__options_check failed" );
		return -4;
	}

	if(( pstHandle->load_type != APX_TASK_TYPE_SERVER_UP )
		&& ( !pstHandle->size || !pstHandle->range
			|| !pstHandle->if_match ))
	{
		s32Err = __request_size( pstHandle );
		if( !s32Err )
		{
			task_opt->bp_continue = pstHandle->range;
			task_opt->fsize = pstHandle->size;
			task_opt->keep_close	= pstHandle->keep_close;
			if( pstHandle->if_match != NULL )
			{
				strncpy( task_opt->if_match, pstHandle->if_match, sizeof( task_opt->if_match ) - 1 );
			}
		}
	}
	tlog( "fname:%s, url: %s fsize: %llu %d ", task_opt->fname, task_opt->uri, task_opt->fsize, task_opt->bp_continue );

	/** already exist? */
	pthread_rwlock_rdlock( &g_trans_head.rwLock );
	for( m = 0; m < g_trans_head.u32Limits; m++ )
	{
		if(  m != nu && g_trans_head.pHandle[m] != NULL
			&& g_trans_head.pHandle[m]->url != NULL
			&& g_trans_head.pHandle[m]->filename != NULL )
		{
			if( 0 == strcmp( pstHandle->url, g_trans_head.pHandle[m]->url )
			     && 0 == strcmp( pstHandle->filename, g_trans_head.pHandle[m]->filename ) )
			{
				tlog( "the task already start." );
				pthread_rwlock_unlock( &g_trans_head.rwLock );
				return -6;
			}
		}
	}
	pthread_rwlock_unlock( &g_trans_head.rwLock );

	u32FileFlag = ( APX_TASK_TYPE_SERVER_UP == pstHandle->load_type ) ? 1 :( \
		APX_TASK_TYPE_SERVER_DOWN == pstHandle->load_type ? 2 : 0 );
	blkCnt = ( pstHandle->range ) ? ( ( pstHandle->keep_close ) ? pstHandle->conn_limit : 0 ) : 1;
	pstHandle->pFile = apx_file_create( pstHandle->url,
									pstHandle->filename, 
									pstHandle->size, 
									blkCnt,
									&s32Err,
									u32FileFlag );
	if( NULL ==pstHandle->pFile )
	{
		tlog( "apx_file_create failed( err = %d ).", s32Err );
		return -7;
	}	

	if( 0 == pstHandle->size )
	{
		pstHandle->size = apx_file_size( pstHandle->pFile );
	}
	blkCnt = apx_file_blk_cnt( pstHandle->pFile );
	pstHandle->conns = pstHandle->range ? pstHandle->conn_limit : 1;
	pstHandle->conns = ( pstHandle->conns > blkCnt ) ?  blkCnt : pstHandle->conns;
	if( pstHandle->cld_proxy )
	{
		s32Err = apx_get_proxy_url( &pstHandle->proxy_url );
		if( s32Err < 0 || NULL == pstHandle->proxy_url )
		{
			pstHandle->cld_proxy = 0;
		}
		else
		{
			pstHandle->proxy_num = __check_proxy_num( pstHandle->conns );
		}
	}
	
	if( APX_TASK_TYPE_SERVER_UP == pstHandle->load_type
		|| ( APX_TASK_TYPE_SERVER_DOWN == pstHandle->load_type 
		&& 1 == pstHandle->checksum ) )
	{/** �������ϴ����ݴ�С */
		s32Err = __request_fileinfo( pstHandle );
		if( APX_TASK_TYPE_SERVER_UP == pstHandle->load_type
			&& -4 == s32Err )
		{/** upload done */
			return 0;
		}

		if( s32Err < 0 )
		{
			tlog( "alert: __request_fileinfo failed( Err: %d ).", s32Err );
			//return -8;
		}
		if( APX_TASK_TYPE_SERVER_UP == pstHandle->load_type )
		{
			apx_file_update_upsize( pstHandle->pFile );
		}
	}

	if( APX_TASK_TYPE_SERVER_UP != pstHandle->load_type
		&& 0 == pstHandle->range
		&& apx_file_cur_size( pstHandle->pFile ) != 0 )
	{/** ��֧�ֶ��߳�����ʱ�������ļ���С */
		apx_file_set_cur_size( pstHandle->pFile, 0 );
		apx_block_reset( pstHandle->pFile );
	}
	
	tlog( "Size: %llu, curSize: %llu, Range: %u, file: %s, conns: %u, "\
		"limits: %u, speed limit: %u, fileId: %s, keepclose: %d, checksum: %d, cld_proxy: %d, proxy_url: %s ",
		pstHandle->size, apx_file_cur_size( pstHandle->pFile ), 
		pstHandle->range, pstHandle->filename,
		pstHandle->conns, pstHandle->conn_limit,
		pstHandle->speed_limit, pstHandle->fileId,
		pstHandle->keep_close, pstHandle->checksum,
		pstHandle->cld_proxy, 
		pstHandle->proxy_url != NULL ? (char*)pstHandle->proxy_url : "null" );
	
	//extern void apx_file_dump( FILE_T * pFile );
	//apx_file_dump( pstHandle->pFile );

	__set_state( pstHandle, 1 );
	pstHandle->ticks[0] = pstHandle->ticks[1] = __get_clock();
	pstHandle->ticks_size[0] = pstHandle->ticks_size[1] =  apx_file_cur_size( pstHandle->pFile );
	if( APX_TASK_PROTO_HTTPS == pstHandle->proto )
	{
		//init_locks();
	}
	memset( pstHandle->thread_id, 0, sizeof( pstHandle->thread_id ) );
	if( pstHandle->load_type != APX_TASK_TYPE_SERVER_UP )
	{		
		while( k < pstHandle->conns )
		{
			pthread_create( &(pstHandle->thread_id[k] ), NULL, __download_thread, pstHandle );
			k++;
		}
	}
	else
	{
		while( k < pstHandle->conns )
		{
			pthread_create( &(pstHandle->thread_id[k] ), NULL, __upload_thread, pstHandle );
			k++;
		}
	}

	return 0;	
}

int apx_trans_stop_cl( u32 nu )
{
	int k = 0;
	handle_st *pstHandle = NULL;

	if( nu >= g_trans_head.u32Limits  )
	{
		tlog( "patameters err." );
		return -1;
	}
	
	pstHandle = g_trans_head.pHandle[nu];
	if( pstHandle != NULL && pstHandle->status )
	{
		__set_state( pstHandle, 0 );
		if( pstHandle->active_conns )
		{
			pthread_rwlock_rdlock( &pstHandle->rwLock );
			for( k < 0; k < MAX_THREAD_NUM; k++ )
			{
				if( pstHandle->thread_id[k] != 0 )
				{
					pthread_cancel( pstHandle->thread_id[k] );
				}
			}
			pthread_rwlock_unlock( &pstHandle->rwLock );
		}
		if( APX_TASK_PROTO_HTTPS == pstHandle->proto )
		{
			//kill_locks();
		}
		return 0;
	}
	
	return 0;
}


int apx_trans_recv_cl( u32 nu )
{

	return 0;
}

int apx_trans_setopt_cl( u32 nu, struct apx_trans_glboptions* glb_opt, struct apx_trans_opt* task_opt )
{
	handle_st *pstHandle = NULL;
	
	__check_params( nu, pstHandle, -1 );
	
	return 0;
}

int apx_trans_getopt_cl( u32 nu, struct apx_trans_glboptions* glb_opt, struct apx_trans_opt* task_opt )
{
	handle_st *pstHandle = NULL;
	
	__check_params( nu, pstHandle, -1 );

	return 0;
}

int apx_trans_getstat_cl( u32 nu, struct apx_trans_stat* task_stat )
{
	u32 u32Ticks = 0;
	u64 u64CurSize = 0;
	handle_st *pstHandle = NULL;
	
	__check_params( nu, pstHandle, -1 );

	if( task_stat != NULL )
	{
		pthread_rwlock_rdlock( &pstHandle->rwLock );
		task_stat->total_size = pstHandle->size;
		task_stat->connections = pstHandle->active_conns;
		task_stat->trans_errno = 0;
		if( 0 == task_stat->total_size )
		{
			task_stat->total_size =  apx_file_size( pstHandle->pFile );
		}
		if( pstHandle->status )
		{
			task_stat->state_event = task_stat->state = APX_TASK_STATE_ACTIVE;
			//task_stat->state_event = task_stat->state = ( pstHandle->size == u64CurSize ) ? APX_TASK_STATE_FINISHED : APX_TASK_STATE_ACTIVE;
		}
		else
		{
			task_stat->state_event = task_stat->state =  APX_TASK_STATE_STOP;
		}

		u32Ticks = __get_clock();
		u64CurSize = apx_file_cur_size( pstHandle->pFile );
		//u32Ticks = ( u32Ticks > pstHandle->ticks ) ? u32Ticks - pstHandle->ticks : 0;
		
		if( pstHandle->load_type == APX_TASK_TYPE_SERVER_UP )
		{
			task_stat->up_size = u64CurSize;
			if( u32Ticks <= pstHandle->ticks[0] ||u64CurSize < pstHandle->ticks_size[0] )
			{
				task_stat->up_speed = 0;
			}
			else
			{
				task_stat->up_speed = ( u64CurSize - pstHandle->ticks_size[0] ) / ( u32Ticks - pstHandle->ticks[0] );
			}
		}
		else
		{
			task_stat->down_size = u64CurSize;
			if( u32Ticks <= pstHandle->ticks[0] ||u64CurSize < pstHandle->ticks_size[0] )
			{
				task_stat->down_speed = 0;
			}
			else
			{
				task_stat->down_speed = ( u64CurSize - pstHandle->ticks_size[0] ) / ( u32Ticks - pstHandle->ticks[0] );
			}
		}

		if( u32Ticks >= pstHandle->ticks[0] + 5 )
		{
			pstHandle->ticks[0] = pstHandle->ticks[1];
			pstHandle->ticks_size[0] = pstHandle->ticks_size[1];
		}

		if( u32Ticks >= pstHandle->ticks[1] + 3 )
		{
			pstHandle->ticks[1] = u32Ticks;
			pstHandle->ticks_size[1] = u64CurSize;
		}
		pthread_rwlock_unlock( &pstHandle->rwLock );
		return 0;
	}
	
	return -1;
}
int __upload_precreate_cl( struct apx_trans_opt* task_opt )
{
	s32 s32Err = 0;
	u32 u32BlkCnt = 0;
	u64 u64Size = 0;
	s8* pfileName = __joint_filename( task_opt->fpath, task_opt->fname );
	cld_fileinfo_st fileinfo;
	
	s32Err = apx_file_is_exist( pfileName, ( off_t* )&u64Size );
	u32BlkCnt = apx_file_divide_cnt( u64Size, 1);
	
	memset( &fileinfo, 0, sizeof( cld_fileinfo_st ) );
	s32Err = apx_cloud_upload_proload( pfileName, u32BlkCnt, &fileinfo );
	if( s32Err < 0 )
	{
		tlog( "preload request failed." );
		CL_FREE( pfileName );
		return -1;
	}

	CL_FREE( pfileName );
	task_opt->fsize = u64Size;
	strncpy( task_opt->fileId, fileinfo.fileId, sizeof( task_opt->fileId ) -1 );
	tlog( " precreate ok( fileId: %s, url: %s ).", task_opt->fileId, task_opt->uri );
	
	return 0;
}

int __download_precreate_cl( struct apx_trans_opt* task_opt )
{
	s8 *ps8Url = NULL;
	s32 s32Err = 0;
	size_t len = 0;
	handle_st stHandle;

	len = strlen( task_opt->fname );
	if( len != 0 )
	{
		tlog( "file name already specified( fname: %s ), skip precreate.", task_opt->fname );
		return 0;
	}
	
	len = strlen( task_opt->uri );
	if( 0 == len )
	{
		tlog( "uri is null." );
		return -3;
	}
	ps8Url = task_opt->uri;

	memset( &stHandle, 0, sizeof( handle_st ) );	
	stHandle.load_type		=	task_opt->type;
	stHandle.proto		=	task_opt->proto;
	stHandle.cookies		=	task_opt->cookie;
	stHandle.referer		=	task_opt->referer;
	if( APX_TASK_PROTO_FTP == stHandle.proto )
	{
		__set_ftp_acct( &stHandle, task_opt->ftp_user, task_opt->ftp_passwd );
	}
	
	s32Err = __request_head( ps8Url, &stHandle );
	if( -4 ==  s32Err )
	{
		s32Err = 0;
		tlog( "server not support for  head request." );
		goto out;
	}
	else if( s32Err < 0 )
	{
		s32Err = -4;
		tlog( "http head failed(url: %s ).", ps8Url );
		goto out;
	}

	if( NULL == stHandle.filename )
	{
		__get_fname_from_url( ps8Url, &stHandle.filename );
		if( NULL == stHandle.filename )
		{
			s32Err = -5;
			tlog( "no file name( url: %s )", ps8Url );
			goto out;
		}
	}
	
	task_opt->bp_continue	=	stHandle.range;
	task_opt->fsize			=	stHandle.size;
	task_opt->keep_close		=	stHandle.keep_close;
	strncpy( task_opt->fname, stHandle.filename, sizeof( task_opt->fname ) - 1 );
	memset( task_opt->if_match, 0, sizeof( task_opt->if_match ) );
	if( stHandle.if_match != NULL )
	{
		strncpy( task_opt->if_match, stHandle.if_match, sizeof( task_opt->if_match ) - 1 );
	}

out:
	CL_FREE( stHandle.filename );
	CL_FREE( stHandle.ftp_acct );
	return s32Err;
}


int apx_trans_precreate_cl( struct apx_trans_opt* task_opt )
{
	if( NULL == task_opt )
	{
		tlog( "task_opt is null." );
		return -1;
	}

	if( APX_TASK_PROTO_HTTP != task_opt->proto
		&& APX_TASK_PROTO_HTTPS != task_opt->proto
		&& APX_TASK_PROTO_FTP != task_opt->proto )
	{
		tlog( "unkown protocol type( proto = %u ).",  task_opt->proto );
		return -2;
	}

	if( APX_TASK_TYPE_SERVER_UP == task_opt->type )
	{
		return __upload_precreate_cl( task_opt );
	}
	else if( APX_TASK_TYPE_DOWN == task_opt->type
		|| APX_TASK_TYPE_SERVER_DOWN == task_opt->type )
	{
		return __download_precreate_cl( task_opt );
	}

	tlog( "unkown task type( type: %d ).", task_opt->type );
	return -10;
}


