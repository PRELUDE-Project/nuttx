import pathlib,subprocess
b=pathlib.Path(__file__).resolve().parent
s=(b/'sched/wqueue/kwork_cancel.c').read_text()
start=s.index('static int work_qcancel('); end=s.index('\n/****************************************************************************',start)
fn=s[start:end]
pre=r'''
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#define FAR
typedef int irqstate_t;
struct work_s {void *worker;};
struct kworker_s {struct work_s *work; int pid; int wait_count; sem_t wait;};
struct kwork_wqueue_s {pthread_mutex_t lock; int nthreads; struct kworker_s threads[2];};
#define work_available(w) ((w)->worker==NULL)
static int spin_lock_irqsave(pthread_mutex_t *m){assert(pthread_mutex_lock(m)==0);return 0;}
static void spin_unlock_irqrestore(pthread_mutex_t *m,int f){(void)f;assert(pthread_mutex_unlock(m)==0);}
static bool work_remove(struct kwork_wqueue_s *q,struct work_s *w){(void)q;w->worker=NULL;return false;}
static void work_timer_reset(struct kwork_wqueue_s *q){(void)q;}
static int nxsched_gettid(void){return 99;}
static struct kworker_s *wq_get_worker(struct kwork_wqueue_s *q){return q->threads;}
static sem_t entered;
static int nxsem_wait_uninterruptible(sem_t *s){sem_post(&entered);while(sem_wait(s)&&errno==EINTR){}return 0;}
'''
post=r'''
static struct kwork_wqueue_s q={.lock=PTHREAD_MUTEX_INITIALIZER,.nthreads=2};
static struct work_s w;
static void *cancel_thread(void *unused){(void)unused;assert(work_qcancel(&q,true,&w)==0);return NULL;}
int main(void){
 sem_init(&entered,0,0);
 for(int i=0;i<2;i++){q.threads[i].pid=i+1;sem_init(&q.threads[i].wait,0,0);}
 for(int i=0;i<1000;i++){
  pthread_t t;
  q.threads[0].work=&w;q.threads[1].work=&w;w.worker=NULL;
  assert(pthread_create(&t,NULL,cancel_thread,NULL)==0);
  sem_wait(&entered);
  pthread_mutex_lock(&q.lock);
  assert(q.threads[0].wait_count==1);
  assert(q.threads[1].wait_count==0);
  q.threads[0].work=NULL;q.threads[0].wait_count--;
  sem_post(&q.threads[0].wait);
  pthread_mutex_unlock(&q.lock);
  pthread_join(t,NULL);
  assert(q.threads[1].work==&w);
  q.threads[1].work=NULL;
 }
 puts("PASS: actual work_qcancel returns after first matching worker, second remains active; 1000 iterations");
}
'''
(b/'cancel_two.c').write_text(pre+fn+post)
subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-pthread','-fsanitize=undefined','-fno-sanitize-recover=all',str(b/'cancel_two.c'),'-o',str(b/'cancel_two')],check=True)
r=subprocess.run([str(b/'cancel_two')],check=True,capture_output=True,text=True)
(b/'result.log').write_text(r.stdout);print(r.stdout)
