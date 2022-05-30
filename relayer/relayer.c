#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include "relayer.h"
#define BUFSIZE 1024

enum
{
	STATE_R = 1,
	STATE_W,
	STATE_Ex,
	STATE_T
};

struct rel_fsm_st
{
	int state;   //状态
	int sfd;   //source fd
	int dfd;   //destination fd
	int len;   //保存每次读的长度
	int pos;   //保存没写完时的位置，下一次从这开始写
	char buf[BUFSIZE];
	char* errstr;    //记录出错的原因
	int64_t count;    //记录传输的个数
};

struct rel_job_st
{
	int fd1, fd2;
	int job_state;	//任务的状态
	struct rel_fsm_st fsm12, fsm21;
	int fd1_save, fd2_save;
};

static struct rel_job_st* rel_job[REL_JOBMAX];
static pthread_mutex_t mut_rel_job = PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t init_once = PTHREAD_ONCE_INIT;

static void fsm_driver(struct rel_fsm_st* fsm)
{
	int ret;
	switch(fsm->state)
	{
		case STATE_R:
			fsm->len = read(fsm->sfd, fsm->buf, BUFSIZE);
			if(fsm->len == 0)
				fsm->state = STATE_T;
			else if(fsm->len < 0)
			{
				if(errno == EAGAIN)
					fsm->state = STATE_R;
				else
				{	
					fsm->state = STATE_Ex;
					fsm->errstr = "read()";
				}
			}
			else
			{
				fsm->state = STATE_W;
				fsm->pos = 0;
			}
			break;

		case STATE_W:
			ret = write(fsm->dfd, fsm->buf + fsm->pos, fsm->len);
			if(ret < 0)
			{
				if(errno == EAGAIN)
					fsm->state = STATE_W;
				else
				{
					fsm->state = STATE_Ex;
					fsm->errstr = "write()";
				}
			}
			else
			{
				fsm->pos += ret;
				fsm->len -= ret;
				if(fsm->len == 0)
					fsm->state = STATE_R;
				else
					fsm->state = STATE_W;
			}			
            break;

		case STATE_Ex:
			perror(fsm->errstr);
			fsm->state = STATE_T;
            break;

		case STATE_T:
			/* do something 结束 */
            break;
		
		default:
			abort();
			break;
	}	


}

static void* thr_relayer(void* p)
{
	int i; 
	while(1)
	{
		pthread_mutex_lock(&mut_rel_job);
		for(i = 0; i < REL_JOBMAX; i++)
		{
			if(rel_job[i] != NULL)
			{
				if(rel_job[i]->job_state == STATE_RUNNING)
				{
					fsm_driver(&rel_job[i]->fsm12);
					fsm_driver(&rel_job[i]->fsm21);

					if(rel_job[i]->fsm12.state == STATE_T && \
						rel_job[i]->fsm21.state == STATE_T)
						rel_job[i]->job_state = STATE_OVER;
				}
			}
		}	
		pthread_mutex_unlock(&mut_rel_job);
	}
}

//static void module_unload()
static void module_load()
{
	pthread_t tid_relayer;
	int err;
	pthread_create(&tid_relayer, NULL, thr_relayer, NULL);
	if(err)
	{
		fprintf(stderr, "pthread_create():%s\n", strerror(err));
		exit(1);
	}
}

int get_free_pos_unlocked()
{
	int i;
	for(i = 0; i <REL_JOBMAX; i++)
	{
		if(rel_job[i] == NULL)
			return i;
	}
	return -1;
}
int rel_addjob(int fd1, int fd2)
{
	struct rel_job_st* me;
		
	pthread_once(&init_once, module_load);

	me = malloc(sizeof(*me));
	if(me == NULL)
		return -ENOMEM;
	
	me->fd1 = fd1;
	me->fd2 = fd2;
	me->job_state = STATE_RUNNING;	
	
	me->fd1_save = fcntl(me->fd1, F_GETFL);
	fcntl(me->fd1, F_SETFL, me->fd1_save|O_NONBLOCK);
	
	me->fd2_save = fcntl(me->fd2, F_GETFL);
    fcntl(me->fd2, F_SETFL, me->fd2_save|O_NONBLOCK);

	me->fsm12.sfd = me->fd1;
	me->fsm12.dfd = me->fd2;
	me->fsm12.state = STATE_R;

	me->fsm21.sfd = me->fd2;
    me->fsm21.dfd = me->fd1;
    me->fsm21.state = STATE_R;

	pthread_mutex_lock(&mut_rel_job);
	int pos = get_free_pos_unlocked();
	if(pos < 0)
	{	
		pthread_mutex_unlock(&mut_rel_job);
		fcntl(me->fd1, F_SETFL, me->fd1_save);
		fcntl(me->fd2, F_SETFL, me->fd2_save);
		free(me);
		return -ENOSPC;
	}

	rel_job[pos] = me;
	pthread_mutex_unlock(&mut_rel_job);	
	return pos;
}
#if 0
int rel_canceljob(int id)
{

}

int rel_waitjob(int id, struct rel_state_st*)
{


}

int rel_statjob(int id, struct rel_state_st*)
{

}
#endif
