/**
 *  Copyright APPEX, 2015, 
 *  <HFTS-CLIENT>
 *  <����>
 *
 *  @defgroup <configure>
 *  @ingroup  <hfts-client>
 *
 *  <�ļ�����>
 *
 *  @{
 */
#ifndef APX_USER_H_20150210
#define APX_USER_H_20150210


#include "../include/apx_list.h"
#include "../include/apx_type.h"
#include "../include/apx_hftsc_api.h"

#include "apx_proto_ctl.h"
#include "apx_task.h"

extern void thread_utask_scheduler(void * data);

struct apx_usertask_st
{
	pthread_mutex_t task_list_lock;
	u16 task_num[APX_TASK_STATE_END];
	struct list_head task_list[APX_TASK_STATE_END];
};

struct apx_userinfo_st
{
	struct list_head list;
	u32 userid;
	u32	groupid;
	char name[64];
	char passwd[64];
	char email[64];
	char user_dir[128];
	time_t register_time;
	time_t login_time;
	time_t last_login_time;
	u32 down_splimit;
	u32 up_splimit;
	u32 active_task_limit;
	
//	struct finfo_st *file_list;
	struct apx_usertask_st user_task;
	
	pthread_t	checker_tid;	
};


#endif
/** @} */
