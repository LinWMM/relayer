#ifndef RELAYER_H__
#define RELAYER_H__

#define REL_JOBMAX 1000
enum
{
	STATE_RUNNING=1,
	STATE_CANCELED,
	STATE_OVER
};

struct rel_state_st
{
	int state;
	int fd1;
	int fd2;
	int64_t count12;
	int64_t count21;  
	//struct timerval start, end
};

int rel_addjob(int fd1, int fd2);
/*
 * return >= 0   成功 返回当前任务id
 * 		 == -EINVAL  失败，参数非法
 * 		 == -ENOSPC  失败，任务数组满
 * 		 == -ENOMEM  失败，malloc失败
 * */


int rel_canceljob(int id);
/*
 * return == 0 成功
 *        == -EINVAL 失败，参数非法
 *		  == -EBUSY  失败，任务不能被重复取消
 * */


int rel_waitjob(int id, struct rel_state_st*);
/*
 *	return == 0    成功
 *		   == -EINVAL  失败，参数非法
 * */

int rel_statjob(int id, struct rel_state_st*);
/*  获取当前任务状态
 *  return == 0    成功
 *         == -EINVAL  失败，参数非法
 * */













#endif
