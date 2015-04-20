#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "user/syscall.h"
#include <stdbool.h>
#include <debug.h>
#include "devices/shutdown.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "threads/malloc.h"
#include "devices/input.h"
#include "filesys/filesys.h"
#include "filesys/file.h"


static int add_file(struct file *f);
static struct fd_mapper* get_file(int fd);
static void syscall_handler(struct intr_frame *);
static void extract_args(void* p, int argv[], int argc);
static bool check_userPointer(const void* p);


void syscall_init(void) {
	lock_init(&fs_lock);
	intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}


static void syscall_handler(struct intr_frame *f UNUSED) {

	if(!check_userPointer((const void*) f->esp)){
		exit(-1);
	}
	void* p = f->esp; /* because esp points to the last element on stack */

	int sys_call_num = *((int*) p); /*first thing on the stack is the System Call Number*/
	p +=4; /*move pointer to next element to be popped*/
	if(!check_userPointer((const void*) p)){
		exit(-1);
	}
	int argv[3]; /*max number of argumnets in all syscall methods is = 3*/


	switch (sys_call_num) { /* switch on type int */
	case SYS_HALT: {
		halt();
	}break;
	case SYS_EXIT: {
		extract_args(p, argv, 1);
		exit(argv[0]);
	}break;
	case SYS_EXEC: {
		extract_args(p, argv, 1);

		if(!check_userPointer((const void*) argv[0])){
			exit(-1);
		}

		f->eax = exec((const char*) argv[0]);
	}break;
	case SYS_WAIT: {
		extract_args(p, argv, 1);
		f->eax = wait(argv[0]);
	}break;
	case SYS_CREATE: {
		extract_args(p, argv, 2);
		if(!check_userPointer((const void*) argv[0]))
			exit(-1);
		f->eax = create((const char*) argv[0], (unsigned)argv[1]);
	}break;
	case SYS_REMOVE: {
		extract_args(p, argv, 1);
		if(!check_userPointer((const void*) argv[0]))
			exit(-1);
		f->eax = remove((const char*) argv[0]);
	}break;
	case SYS_OPEN: {
		extract_args(p, argv, 1);
		if(!check_userPointer((const void*) argv[0]))
			exit(-1);
		f->eax = open((const char*)argv[0]);
	}break;
	case SYS_FILESIZE: {
		extract_args(p, argv, 1);
		f->eax = filesize(argv[0]);
	}break;
	case SYS_READ: {
		extract_args(p, argv, 3);
		if(!check_userPointer((const void*) argv[1]))
			exit(-1);
		/*check buffer*/
		void* tempBuf = ((void*) argv[1])+ argv[2];
		if(!check_userPointer((const void*) tempBuf))
			exit(-1);

		f->eax = read(argv[0], (void*)argv[1], (unsigned)argv[2]);
	}break;
	case SYS_WRITE: {
		extract_args(p, argv, 3);
		if(!check_userPointer((const void*) argv[1]))
			exit(-1);
		/*check buffer*/
		void* tempBuf = ((void*) argv[1])+ argv[2];
		if(!check_userPointer((const void*) tempBuf))
			exit(-1);

		f->eax = write(argv[0], (void*)argv[1], (unsigned)argv[2]);
	}break;
	case SYS_SEEK: {
		extract_args(p, argv, 2);
		seek(argv[0], (unsigned)argv[1]);
	}break;
	case SYS_TELL: {
		extract_args(p, argv, 1);
		f->eax = tell(argv[0]);
	}break;
	case SYS_CLOSE: {
		extract_args(p, argv, 1);
		close(argv[0]);
	}break;
	default: {
		printf("INVALID system call!\n");
		thread_exit();
	}break;
	}
}

void exit(int status) {
	struct thread* current_thread = thread_current();
	/*handle if parent is waiting for me and if I am waiting on a child*/
	printf ("%s: exit(%d)\n", current_thread->name, status);

	struct list_elem* e;
	if(thread_current()->papa!=NULL){
		for (e = list_begin (&thread_current()->papa->children); e != list_end (&thread_current()->papa->children); e = list_next (e))
		{
			struct child *c = list_entry (e, struct child, elem);
			if(c->pid_ofChild == thread_current()->tid){
				c->status = status;
				switch(status){
				case -1:
					c->exs = KILLED;
					break;
				default:
					c->exs = NORMAL_EXIT;
					break;
				}
				break;
			}
		}
	}
	thread_exit();
}

pid_t exec(const char *cmd_line) {
	struct thread* parent_thread = thread_current();
	pid_t pid = (pid_t) process_execute(cmd_line); /*create thread with given cmd_line*/

	if(pid==PID_ERROR){
		return PID_ERROR;
	}
	struct list_elem* e;

	for (e = list_begin (&parent_thread->children); e != list_end (&parent_thread->children); e = list_next (e))
	{
		struct child *c = list_entry (e, struct child, elem);
		if(c->pid_ofChild==pid){
			sema_down(&c->child_t->exec_sema);
			if(!c->loaded){
				return -1;
			}
			break;
		}
	}


	return pid;
}

int wait(pid_t pid) {
	return process_wait(pid);
}

void halt(void) {
	shutdown_power_off();
}

bool create(const char *file, unsigned initial_size) {
	lock_acquire(&fs_lock);

	bool state = filesys_create(file, initial_size);

	lock_release(&fs_lock);

	return state;
}

bool remove(const char *file) {
	lock_acquire(&fs_lock);

	bool state = filesys_remove(file);

	lock_release(&fs_lock);

	return state;
}

int open(const char *file) {
	lock_acquire(&fs_lock);

	struct file* f_opened = filesys_open(file);

	if (f_opened == NULL) {
		lock_release(&fs_lock);
		return -1;
	}

	int fd = add_file(f_opened);

	lock_release(&fs_lock);
	return fd;
}

int filesize(int fd) {
	lock_acquire(&fs_lock);
	struct fd_mapper* fd_map = get_file(fd);
	if (fd_map == NULL) {
		lock_release(&fs_lock);
		return -1;
	}
	struct file* f_opened = fd_map->file;
	if(f_opened==NULL){
		lock_release(&fs_lock);
		return -1;
	}
	int size = file_length(f_opened);
	lock_release(&fs_lock);
	return size;
}

int read(int fd, void *buffer, unsigned length) {
	lock_acquire(&fs_lock);
	if (fd == 0) {
		unsigned i=0;
		while (i < length) {
			*((uint8_t *) buffer) = input_getc();
			buffer = ((uint8_t*) buffer) + 1;
			i++;
		}
		lock_release(&fs_lock);
		return length;
	}


	struct fd_mapper* fd_map = get_file(fd);
	if (fd_map == NULL) {
		lock_release(&fs_lock);
		return -1;
	}

	struct file* f_opened = fd_map->file;
	if(f_opened==NULL){
		lock_release(&fs_lock);
		return -1;
	}

	int actually_read = file_read(f_opened, buffer, length);

	lock_release(&fs_lock);

	return actually_read;
}

int write(int fd, const void *buffer, unsigned length) {
	lock_acquire(&fs_lock);
	if (fd == 1) {
		putbuf(buffer, length);
		lock_release(&fs_lock);
		return length;
	}


	struct fd_mapper* fd_map = get_file(fd);
	if (fd_map == NULL) {
		lock_release(&fs_lock);
		return -1;
	}

	struct file* f_opened = fd_map->file;
	if(f_opened==NULL){
		lock_release(&fs_lock);
		return -1;
	}

	int actually_written = file_write(f_opened, buffer, length);

	lock_release(&fs_lock);

	return actually_written;
}

void seek(int fd, unsigned position) {
	lock_acquire(&fs_lock);

	struct fd_mapper* fd_map = get_file(fd);

	if (fd_map == NULL) {
		lock_release(&fs_lock);
		return;
	}

	struct file* f_opened = fd_map->file;
	if(f_opened==NULL){
		lock_release(&fs_lock);
		return;
	}

	file_seek(f_opened, position);

	lock_release(&fs_lock);
}

unsigned tell(int fd) {
	lock_acquire(&fs_lock);

	struct fd_mapper* fd_map = get_file(fd);

	if (fd_map == NULL) {
		lock_release(&fs_lock);
		return -1;
	}

	struct file* f_opened = fd_map->file;
	if(f_opened==NULL){
		lock_release(&fs_lock);
		return -1;
	}

	off_t position = file_tell(f_opened);

	lock_release(&fs_lock);

	return position;
}

void close(int fd) {
	lock_acquire(&fs_lock);
	struct fd_mapper* fd_map = get_file(fd);
	if(fd_map == NULL){
		lock_release(&fs_lock);
		return;
	}

	struct file* f_to_close = fd_map->file;
	if(f_to_close == NULL){
		lock_release(&fs_lock);
		return;
	}

	file_close(f_to_close);
	hash_delete(&thread_current()->mapper, &fd_map->hash_elem);

	free(fd_map);
	lock_release(&fs_lock);
}

/* Adds the opened file "f" to the current thread's mapper
 * giving it fd as file discriptor, return fd
 * */
int add_file(struct file *f) {
	if (f == NULL)
		return -1;

	struct thread* t = thread_current();

	struct fd_mapper* newMapper = (struct fd_mapper*) malloc(sizeof(struct fd_mapper));
	newMapper->file = f;
	newMapper->fd = t->num_fd_mappers;
	t->num_fd_mappers++;

	hash_insert(&t->mapper, &(newMapper->hash_elem));
	int f_d = newMapper->fd;
	return f_d;
}

/* gets the file with "fd" from the current thread's mapper */
struct fd_mapper* get_file(int fd) {
	struct thread* t = thread_current();

	struct fd_mapper dummyMap;
	dummyMap.fd = fd;

	struct hash_elem* h_e = hash_find(&t->mapper, &(dummyMap.hash_elem));

	if (h_e == NULL)
		return NULL;

	struct fd_mapper* foundMap = hash_entry(h_e, struct fd_mapper, hash_elem);

	return foundMap;
}

static void extract_args(void* p, int argv[], int argc){
	int i = 0;
	while(i < argc){
		argv[i] = *((int*)p);
		p += 4;
		i++;
	}
}

static bool check_userPointer(const void* p){ /*checks that given address to on any type is within range*/
	if(p<((void *) 0x08048000)/*start of code segment*/ || p>=PHYS_BASE)
		return false;

	void* kernalAddr = pagedir_get_page(thread_current()->pagedir, p);
	if(kernalAddr==NULL)
		return false;

	return true;
}
