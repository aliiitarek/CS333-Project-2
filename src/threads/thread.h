#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/synch.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "filesys/inode.h"
#include "lib/kernel/hash.h"

/* States in a thread's life cycle. */
enum thread_status
  {
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING        /* About to be destroyed. */
  };

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

/* Nice levels*/
#define NICE_MAX 20
#define NICE_MIN -20

/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */
struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Name (for debugging purposes). */
    uint8_t *stack;                     /* Saved stack pointer. */
    int priority;                       /* Priority. */
    struct list_elem allelem;           /* List element for all threads list. */

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */

#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */
#endif



    /*Alarm*/
    struct list_elem blocked_elem;  /* node of blocked priority queue */
    int64_t rem_ticks;    /* time to wake up */


    /*Advanced Scheduler*/
    int nice; /*value characterizing how "nice" the thread should be to other threads.*/
    int recent_cpu; /*time spent by this thread on the CPU*/ /*fixed point primitive*/

    /*Priority Scheduling*/
    struct list_elem sync_elem; /*node blocked sync primatives */

    /*Donation*/
    struct list donars; /*carries pointers to donar threads*/
    struct thread* waiting_on;
    int initial_priority; /*initial priority*/

    /*System Calls*/
    int num_fd_mappers; /*number of files open by this thread, starts at 2*/
    struct hash mapper; /*Hash map between fd and file*/

    /*Pointer to my Parent*/
    struct thread* papa;

    /*Exec() and Wait() System Call*/
    struct list children;
    struct semaphore wait_sema;
    struct semaphore exec_sema;
    struct file* myFile;

    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */
  };

enum exit_status{
	KILLED,
	NORMAL_EXIT,
	ALIVE
};

struct child
{
	struct thread* child_t; /*This thread is the one running start_process()*/
	tid_t pid_ofChild;
	struct list_elem elem;
	enum exit_status exs;
	int status;
	bool somethread_is_waiting_on_me;
	bool loaded;
};

struct fd_mapper
{
	int fd; /*key*/
	struct file* file; /*value*/
	struct hash_elem hash_elem; /*hash element*/
};

struct donar_elem
{
	struct list_elem don_elem;              /*List element.*/
	struct lock* lock_waiting_on;         /*lock that donar_thread is waiting on.*/
	struct thread* donar_thread; /*this is the thread that donated me*/
};

int get_highest_priority(void);

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func (struct thread *t, void *aux);
void thread_foreach (thread_action_func *, void *);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

/*mlfqs*/
void mlfqs_update_priority (struct thread *t, void* aux);
void mlfqs_recalculate_recent_cpu (struct thread *t, void* aux);
void mlfqs_update_load_avg(void);
int get_ready_threads(void);

/*donation*/
void update_max_donated_priority(struct thread * child, struct thread * parent);
void undonate(struct lock *lock);

/*file Hashmap*/
bool fileHashmap_comparable (const struct hash_elem *a, const struct hash_elem *b, void *aux);
unsigned fd_mapper_hash(const struct hash_elem* p_, void* aux);
void fd_mapper_dest(struct hash_elem* elem, void* aux);

int size(void);

#endif /* threads/thread.h */
