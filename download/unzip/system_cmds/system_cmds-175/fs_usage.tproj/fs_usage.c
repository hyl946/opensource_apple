/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
cc -I. -DKERNEL_PRIVATE -O -o fs_usage fs_usage.c
*/

#define	Default_DELAY	1	/* default delay interval */

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <strings.h>
#include <nlist.h>
#include <fcntl.h>
#include <string.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>

#include <libc.h>
#include <termios.h>
#include <bsd/curses.h>
#include <sys/ioctl.h>

#ifndef KERNEL_PRIVATE
#define KERNEL_PRIVATE
#include <sys/kdebug.h>
#undef KERNEL_PRIVATE
#else
#include <sys/kdebug.h>
#endif /*KERNEL_PRIVATE*/

#include <sys/sysctl.h>
#include <errno.h>
#import <mach/clock_types.h>
#include <err.h>

extern int errno;


struct th_info {
        int  in_filemgr;
        int  thread;
        int  type;
        int  arg1;
        int  arg2;
        int  arg3;
        int  arg4;
        int  vfslookup;
        int  child_thread;
        int  waited;
        double stime;
        char pathname[32];
};

#define MAX_THREADS 512
struct th_info th_state[MAX_THREADS];


int  cur_max = 0;
int  need_new_map = 1;
int  bias_secs;
int  wideflag = 0;
int  select_pid_mode = 0;  /* Flag set indicates that output is restricted
			      to selected pids or commands */

int  one_good_pid = 0;    /* Used to fail gracefully when bad pids given */


#define DBG_ZERO_FILL_FAULT   1
#define DBG_PAGEIN_FAULT      2
#define DBG_COW_FAULT         3
#define DBG_CACHE_HIT_FAULT   4

#define TRACE_DATA_NEWTHREAD   0x07000004
#define TRACE_STRING_NEWTHREAD 0x07010004
#define TRACE_STRING_EXEC      0x07010008

#define MACH_vmfault    0x01300000
#define MACH_sched      0x01400000
#define MACH_stkhandoff 0x01400008
#define VFS_LOOKUP      0x03010090
#define BSC_exit        0x040C0004

#define MSC_map_fd   0x010c00ac
#define	BSC_recvmsg  0x040C006C
#define	BSC_sendmsg  0x040C0070
#define	BSC_recvfrom 0x040C0074
#define	BSC_sendto   0x040C0214

#define BSC_read     0x040C000C
#define BSC_write    0x040C0010
#define BSC_open     0x040C0014
#define BSC_close    0x040C0018
#define BSC_link     0x040C0024
#define BSC_unlink   0x040C0028
#define BSC_mknod    0x040C0038	
#define BSC_chmod    0x040C003C	
#define BSC_chown    0x040C0040	
#define BSC_access   0x040C0084	
#define BSC_chflags  0x040C0088	
#define BSC_fchflags 0x040C008C
#define BSC_sync     0x040C0090	
#define BSC_symlink  0x040C00E4	
#define BSC_readlink 0x040C00E8	
#define BSC_fsync    0x040C017C	
#define BSC_readv    0x040C01E0	
#define BSC_writev   0x040C01E4	
#define BSC_fchown   0x040C01EC	
#define BSC_fchmod   0x040C01F0	
#define BSC_rename   0x040C0200	
#define BSC_mkdir    0x040C0220	
#define BSC_rmdir    0x040C0224	
#define BSC_statfs   0x040C0274	
#define BSC_fstatfs  0x040C0278	
#define BSC_stat     0x040C02F0	
#define BSC_fstat    0x040C02F4	
#define BSC_lstat    0x040C02F8	
#define BSC_pathconf 0x040C02FC	
#define BSC_fpathconf     0x040C0300
#define BSC_getdirentries 0x040C0310
#define BSC_mmap     0x040c0314
#define BSC_lseek    0x040c031c
#define BSC_truncate 0x040C0320
#define BSC_ftruncate     0x040C0324	
#define BSC_statv    0x040C0364	
#define BSC_lstatv   0x040C0368	
#define BSC_fstatv   0x040C036C	
#define BSC_mkcomplex     0x040C0360	
#define BSC_getattrlist 0x040C0370	
#define BSC_setattrlist 0x040C0374	
#define BSC_getdirentriesattr 0x040C0378	
#define BSC_exchangedata  0x040C037C	
#define BSC_checkuseraccess   0x040C0380	
#define BSC_searchfs    0x040C0384	

// Carbon File Manager support
#define FILEMGR_PBGETCATALOGINFO		 0x1e000020
#define FILEMGR_PBGETCATALOGINFOBULK	 0x1e000024
#define FILEMGR_PBCREATEFILEUNICODE		 0x1e000028
#define FILEMGR_PBCREATEDIRECTORYUNICODE 0x1e00002c
#define FILEMGR_PBCREATEFORK			 0x1e000030
#define FILEMGR_PBDELETEFORK			 0x1e000034
#define FILEMGR_PBITERATEFORK			 0x1e000038
#define FILEMGR_PBOPENFORK				 0x1e00003c
#define FILEMGR_PBREADFORK				 0x1e000040
#define FILEMGR_PBWRITEFORK				 0x1e000044
#define FILEMGR_PBALLOCATEFORK			 0x1e000048
#define FILEMGR_PBDELETEOBJECT			 0x1e00004c
#define FILEMGR_PBEXCHANGEOBJECT		 0x1e000050
#define FILEMGR_PBGETFORKCBINFO			 0x1e000054
#define FILEMGR_PBGETVOLUMEINFO			 0x1e000058
#define FILEMGR_PBMAKEFSREF				 0x1e00005c
#define FILEMGR_PBMAKEFSREFUNICODE		 0x1e000060
#define FILEMGR_PBMOVEOBJECT			 0x1e000064
#define FILEMGR_PBOPENITERATOR			 0x1e000068
#define FILEMGR_PBRENAMEUNICODE			 0x1e00006c
#define FILEMGR_PBSETCATALOGINFO		 0x1e000070
#define FILEMGR_PBSETVOLUMEINFO			 0x1e000074
#define FILEMGR_FSREFMAKEPATH			 0x1e000078
#define FILEMGR_FSPATHMAKEREF			 0x1e00007c

#define FILEMGR_PBGETCATINFO			 0x1e010000
#define FILEMGR_PBGETCATINFOLITE		 0x1e010004
#define FILEMGR_PBHGETFINFO				 0x1e010008
#define FILEMGR_PBXGETVOLINFO			 0x1e01000c
#define FILEMGR_PBHCREATE				 0x1e010010
#define FILEMGR_PBHOPENDF				 0x1e010014
#define FILEMGR_PBHOPENRF				 0x1e010018
#define FILEMGR_PBHGETDIRACCESS			 0x1e01001c
#define FILEMGR_PBHSETDIRACCESS			 0x1e010020
#define FILEMGR_PBHMAPID				 0x1e010024
#define FILEMGR_PBHMAPNAME				 0x1e010028
#define FILEMGR_PBCLOSE					 0x1e01002c
#define FILEMGR_PBFLUSHFILE				 0x1e010030
#define FILEMGR_PBGETEOF				 0x1e010034
#define FILEMGR_PBSETEOF				 0x1e010038
#define FILEMGR_PBGETFPOS				 0x1e01003c
#define FILEMGR_PBREAD					 0x1e010040
#define FILEMGR_PBWRITE					 0x1e010044
#define FILEMGR_PBGETFCBINFO			 0x1e010048
#define FILEMGR_PBSETFINFO				 0x1e01004c
#define FILEMGR_PBALLOCATE				 0x1e010050
#define FILEMGR_PBALLOCCONTIG			 0x1e010054
#define FILEMGR_PBSETFPOS				 0x1e010058
#define FILEMGR_PBSETCATINFO			 0x1e01005c
#define FILEMGR_PBGETVOLPARMS			 0x1e010060
#define FILEMGR_PBSETVINFO				 0x1e010064
#define FILEMGR_PBMAKEFSSPEC			 0x1e010068
#define FILEMGR_PBHGETVINFO				 0x1e01006c
#define FILEMGR_PBCREATEFILEIDREF		 0x1e010070
#define FILEMGR_PBDELETEFILEIDREF		 0x1e010074
#define FILEMGR_PBRESOLVEFILEIDREF		 0x1e010078
#define FILEMGR_PBFLUSHVOL				 0x1e01007c
#define FILEMGR_PBHRENAME				 0x1e010080
#define FILEMGR_PBCATMOVE				 0x1e010084
#define FILEMGR_PBEXCHANGEFILES			 0x1e010088
#define FILEMGR_PBHDELETE				 0x1e01008c
#define FILEMGR_PBDIRCREATE				 0x1e010090
#define FILEMGR_PBCATSEARCH				 0x1e010094
#define FILEMGR_PBHSETFLOCK				 0x1e010098
#define FILEMGR_PBHRSTFLOCK				 0x1e01009c
#define FILEMGR_PBLOCKRANGE				 0x1e0100a0
#define FILEMGR_PBUNLOCKRANGE			 0x1e0100a4


#define FILEMGR_CLASS   0x1e

#define MAX_PIDS 32
int    pids[MAX_PIDS];

int    num_of_pids = 0;
int    exclude_pids = 0;

struct kinfo_proc *kp_buffer = 0;
int kp_nentries = 0;

#define SAMPLE_SIZE 60000

#define DBG_ZERO_FILL_FAULT   1
#define DBG_PAGEIN_FAULT      2
#define DBG_COW_FAULT         3
#define DBG_CACHE_HIT_FAULT   4

#define DBG_FUNC_ALL	(DBG_FUNC_START | DBG_FUNC_END)
#define DBG_FUNC_MASK	0xfffffffc

/* Default divisor */
#define DIVISOR 16.6666        /* Trace divisor converts to microseconds */
double divisor = DIVISOR;

int mib[6];
size_t needed;
char  *my_buffer;

kbufinfo_t bufinfo = {0, 0, 0, 0};

int total_threads = 0;
kd_threadmap *mapptr = 0;

int trace_enabled = 0;
int set_remove_flag = 1;

void set_numbufs();
void set_init();
void set_enable();
void sample_sc();
int quit();

/*
 *  signal handlers
 */

void leave()			/* exit under normal conditions -- INT handler */
{
        int i;
	void set_enable();
	void set_pidcheck();
	void set_pidexclude();
	void set_remove();

	set_enable(0);

	if (exclude_pids == 0) {
	        for (i = 0; i < num_of_pids; i++)
		        set_pidcheck(pids[i], 0);
	}
	else {
	        for (i = 0; i < num_of_pids; i++)
		        set_pidexclude(pids[i], 0);
	}
	set_remove();
	exit(0);
}


void sigwinch()
{
	initscr();
}

int
exit_usage(myname) {

        fprintf(stderr, "Usage: %s [-e] [-w] [pid | cmd [pid | cmd]....]\n", myname);
	fprintf(stderr, "  -e    exclude the pids specified from the sample\n");
	fprintf(stderr, "  -w    force wider, detailed, output\n");
	fprintf(stderr, "  pid   selects process(s) to sample\n");
	fprintf(stderr, "  cmd   selects process(s) matching command string to sample\n");

	exit(1);
}


main(argc, argv)
	int	argc;
	char	*argv[];
{
	char	*myname = "fs_usage";
	int     i;
	char    ch;
	void getdivisor();
	void argtopid();
	void set_remove();
	void set_pidcheck();
	void set_pidexclude();
	int quit();

        if ( geteuid() != 0 ) {
            printf("'fs_usage' must be run as root...\n");
            exit(1);
        }

	initscr();

	/* get our name */
	if (argc > 0) {
		if ((myname = rindex(argv[0], '/')) == 0) {
			myname = argv[0];
		}
		else {
			myname++;
		}
	}
	
       while ((ch = getopt(argc, argv, "ew")) != EOF) {
               switch(ch) {
                case 'e':
		    exclude_pids = 1;
		    break;
                case 'w':
		    wideflag = 1;
		    break;
	       default:
		 exit_usage(myname);		 
	       }
       }

        argc -= optind;
        argv += optind;

	while (argc > 0 && num_of_pids < (MAX_PIDS - 1)) {
	  select_pid_mode++;
	  argtopid(argv[0]);
	  argc--;
	  argv++;
	}

	if (exclude_pids)
	        pids[num_of_pids++] = getpid();

#if 0
	for (i = 0; i < num_of_pids; i++)
	  {
	    if (exclude_pids)
	      printf("exclude pid %d\n", pids[i]);
	    else
	      printf("pid %d\n", pids[i]);
	  }
#endif

	/* set up signal handlers */
	signal(SIGINT, leave);
	signal(SIGQUIT, leave);
	signal(SIGTERM, leave);
	signal(SIGWINCH, sigwinch);

	if ((my_buffer = malloc(SAMPLE_SIZE * sizeof(kd_buf))) == (char *)0)
	    quit("can't allocate memory for tracing info\n");

	set_remove();
	set_numbufs(SAMPLE_SIZE);
	set_init();

	if (exclude_pids == 0) {
	        for (i = 0; i < num_of_pids; i++)
		        set_pidcheck(pids[i], 1);
	} else {
	        for (i = 0; i < num_of_pids; i++)
		        set_pidexclude(pids[i], 1);
	}

	if (select_pid_mode && !one_good_pid)
	  {
	    /* 
	       An attempt to restrict output to a given
	       pid or command has failed. Exit gracefully
	    */
	    set_remove();
	    exit_usage(myname);
	  }

	set_enable(1);
	getdivisor();


	/* main loop */

	while (1) {
	        usleep(1000 * 50);

		sample_sc();
	}
}

void
find_proc_names()
{
	size_t			bufSize = 0;
	struct kinfo_proc       *kp;
	int quit();

	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_ALL;
	mib[3] = 0;

	if (sysctl(mib, 4, NULL, &bufSize, NULL, 0) < 0)
		quit("trace facility failure, KERN_PROC_ALL\n");

	if((kp = (struct kinfo_proc *)malloc(bufSize)) == (struct kinfo_proc *)0)
	    quit("can't allocate memory for proc buffer\n");
	
	if (sysctl(mib, 4, kp, &bufSize, NULL, 0) < 0)
		quit("trace facility failure, KERN_PROC_ALL\n");

        kp_nentries = bufSize/ sizeof(struct kinfo_proc);
	kp_buffer = kp;
}


struct th_info *find_thread(int thread, int type) {
       struct th_info *ti;

       for (ti = th_state; ti < &th_state[cur_max]; ti++) {
	       if (ti->thread == thread) {
		       if (type == ti->type)
			       return(ti);
		       if (ti->in_filemgr) {
			       if (type == -1)
				       return(ti);
			       continue;
		       }
		       if (type == 0)
			       return(ti);
	       }
       }
       return ((struct th_info *)0);
}

void
set_enable(int val) 
{
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDENABLE;		/* protocol */
	mib[3] = val;
	mib[4] = 0;
	mib[5] = 0;		        /* no flags */
	if (sysctl(mib, 4, NULL, &needed, NULL, 0) < 0)
		quit("trace facility failure, KERN_KDENABLE\n");

	if (val)
	  trace_enabled = 1;
	else
	  trace_enabled = 0;
}

void
set_numbufs(int nbufs) 
{
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDSETBUF;
	mib[3] = nbufs;
	mib[4] = 0;
	mib[5] = 0;		        /* no flags */
	if (sysctl(mib, 4, NULL, &needed, NULL, 0) < 0)
		quit("trace facility failure, KERN_KDSETBUF\n");

	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDSETUP;		
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;		        /* no flags */
	if (sysctl(mib, 3, NULL, &needed, NULL, 0) < 0)
		quit("trace facility failure, KERN_KDSETUP\n");
}

void
set_pidcheck(int pid, int on_off) 
{
        kd_regtype kr;

	kr.type = KDBG_TYPENONE;
	kr.value1 = pid;
	kr.value2 = on_off;
	needed = sizeof(kd_regtype);
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDPIDTR;
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;

	if (sysctl(mib, 3, &kr, &needed, NULL, 0) < 0) {
	        if (on_off == 1)
		        printf("pid %d does not exist\n", pid);
	}
	else {
	  one_good_pid++;
	}
}

/* 
   on_off == 0 turns off pid exclusion
   on_off == 1 turns on pid exclusion
*/
void
set_pidexclude(int pid, int on_off) 
{
        kd_regtype kr;

	one_good_pid++;

	kr.type = KDBG_TYPENONE;
	kr.value1 = pid;
	kr.value2 = on_off;
	needed = sizeof(kd_regtype);
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDPIDEX;
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;

	if (sysctl(mib, 3, &kr, &needed, NULL, 0) < 0) {
	        if (on_off == 1)
		          printf("pid %d does not exist\n", pid);
	}
}

void
get_bufinfo(kbufinfo_t *val)
{
        needed = sizeof (*val);
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDGETBUF;		
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;		/* no flags */

	if (sysctl(mib, 3, val, &needed, 0, 0) < 0)
		quit("trace facility failure, KERN_KDGETBUF\n");  

}

void
set_remove() 
{
        extern int errno;

        errno = 0;

	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDREMOVE;		/* protocol */
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;		/* no flags */
	if (sysctl(mib, 3, NULL, &needed, NULL, 0) < 0)
	  {
	    set_remove_flag = 0;

	    if (errno == EBUSY)
		quit("the trace facility is currently in use...\n          fs_usage, sc_usage, and latency use this feature.\n\n");
	    else
		quit("trace facility failure, KERN_KDREMOVE\n");
	  }
}

void
set_init() 
{       kd_regtype kr;

	kr.type = KDBG_RANGETYPE;
	kr.value1 = 0;
	kr.value2 = -1;
	needed = sizeof(kd_regtype);
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDSETREG;		
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;		/* no flags */

	if (sysctl(mib, 3, &kr, &needed, NULL, 0) < 0)
		quit("trace facility failure, KERN_KDSETREG\n");

	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDSETUP;		
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;		/* no flags */

	if (sysctl(mib, 3, NULL, &needed, NULL, 0) < 0)
		quit("trace facility failure, KERN_KDSETUP\n");
}

void
sample_sc()
{
	kd_buf *kd;
	int i, count;
	void read_command_map();
	void create_map_entry();

        /* Get kernel buffer information */
	get_bufinfo(&bufinfo);

	if (need_new_map) {
	        read_command_map();
	        need_new_map = 0;
	}
	needed = bufinfo.nkdbufs * sizeof(kd_buf);
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDREADTR;		
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;		/* no flags */

	if (sysctl(mib, 3, my_buffer, &needed, NULL, 0) < 0)
                quit("trace facility failure, KERN_KDREADTR\n");
	count = needed;

	if (bufinfo.flags & KDBG_WRAPPED) {
	        printf("buffer wrapped  count = %d\n", count);

	        for (i = 0; i < cur_max; i++) {
			th_state[i].thread = 0;
			th_state[i].vfslookup = 0;
			th_state[i].pathname[0] = 0;
		}
		cur_max = 0;
		need_new_map = 1;
		
		set_enable(0);
		set_enable(1);
	}
	kd = (kd_buf *)my_buffer;
#if 0
	printf("READTR returned %d items\n", count);
#endif
	for (i = 0; i < count; i++) {
	        int debugid, thread;
		int type, n;
		long *sargptr;
		unsigned long long now;
		struct th_info *ti;
		void enter_syscall();
		void exit_syscall();
		void kill_thread_map();

		thread  = kd[i].arg5 & KDBG_THREAD_MASK;
		debugid = kd[i].debugid;
		type    = kd[i].debugid & DBG_FUNC_MASK;

		switch (type) {

		case TRACE_DATA_NEWTHREAD:
		   
		    for (n = 0, ti = th_state; ti < &th_state[MAX_THREADS]; ti++, n++) {
		        if (ti->thread == 0)
		           break;
		    }
		    if (ti == &th_state[MAX_THREADS])
		        continue;
		    if (n >= cur_max)
		        cur_max = n + 1;

		    ti->thread = thread;
		    ti->child_thread = kd[i].arg1;
		    continue;

		case TRACE_STRING_NEWTHREAD:
		    if ((ti = find_thread(thread, 0)) == (struct th_info *)0)
		            continue;
		    if (ti->child_thread == 0)
		            continue;
		    create_map_entry(ti->child_thread, (char *)&kd[i].arg1);

		    if (ti == &th_state[cur_max - 1])
		        cur_max--;
		    ti->child_thread = 0;
		    ti->thread = 0;
		    continue;

		case TRACE_STRING_EXEC:
		    create_map_entry(thread, (char *)&kd[i].arg1);
		    continue;

		case BSC_exit:
		    kill_thread_map(thread);
		    continue;

		case MACH_sched:
		case MACH_stkhandoff:
		    if (ti = find_thread(thread, 0))
		            ti->waited = 1;
		    continue;

		case VFS_LOOKUP:
		    if ((ti = find_thread(thread, 0)) == (struct th_info *)0)
		            continue;

		    if (ti->vfslookup == 0) {
			    ti->vfslookup = 1;
			    memset(&ti->pathname[0], 0, 32);
			    sargptr = (long *)&ti->pathname[0];
				
			    *sargptr++ = kd[i].arg2;
			    *sargptr++ = kd[i].arg3;
			    *sargptr++ = kd[i].arg4;

		    } else if (ti->vfslookup == 1) {
		            ti->vfslookup = 2;
	  
			    sargptr = (long *)&ti->pathname[12];
			    *sargptr++ = kd[i].arg1;
			    *sargptr++ = kd[i].arg2;
			    *sargptr++ = kd[i].arg3;
			    *sargptr++ = kd[i].arg4;
		    }
		    continue;
		}
		now = (((unsigned long long)kd[i].timestamp.tv_sec) << 32) |
		        (unsigned long long)((unsigned int)(kd[i].timestamp.tv_nsec));

		if (debugid & DBG_FUNC_START) {
		       char *p;

		       switch (type) {
			   case FILEMGR_PBGETCATALOGINFO:
					p = "GetCatalogInfo";
					break;
			   case FILEMGR_PBGETCATALOGINFOBULK:
					p = "GetCatalogInfoBulk";
					break;
			   case FILEMGR_PBCREATEFILEUNICODE:
					p = "CreateFileUnicode";
					break;
			   case FILEMGR_PBCREATEDIRECTORYUNICODE:
					p = "CreateDirectoryUnicode";
					break;
			   case FILEMGR_PBCREATEFORK:
					p = "PBCreateFork";
					break;
			   case FILEMGR_PBDELETEFORK:
					p = "PBDeleteFork";
					break;
			   case FILEMGR_PBITERATEFORK:
					p = "PBIterateFork";
					break;
			   case FILEMGR_PBOPENFORK:
					p = "PBOpenFork";
					break;
			   case FILEMGR_PBREADFORK:
					p = "PBReadFork";
					break;
			   case FILEMGR_PBWRITEFORK:
					p = "PBWriteFork";
					break;
			   case FILEMGR_PBALLOCATEFORK:
					p = "PBAllocateFork";
					break;
			   case FILEMGR_PBDELETEOBJECT:
					p = "PBDeleteObject";
					break;
			   case FILEMGR_PBEXCHANGEOBJECT:
					p = "PBExchangeObject";
					break;
			   case FILEMGR_PBGETFORKCBINFO:
					p = "PBGetForkCBInfo";
					break;
			   case FILEMGR_PBGETVOLUMEINFO:
					p = "PBGetVolumeInfo";
					break;
			   case FILEMGR_PBMAKEFSREF:
					p = "PBMakeFSRef";
					break;
			   case FILEMGR_PBMAKEFSREFUNICODE:
					p = "PBMakeFSRefUnicode";
					break;
			   case FILEMGR_PBMOVEOBJECT:
					p = "PBMoveObject";
					break;
			   case FILEMGR_PBOPENITERATOR:
					p = "PBOpenIterator";
					break;
			   case FILEMGR_PBRENAMEUNICODE:
					p = "PBRenameUnicode";
					break;
			   case FILEMGR_PBSETCATALOGINFO:
					p = "SetCatalogInfo";
					break;
			   case FILEMGR_PBSETVOLUMEINFO:
					p = "SetVolumeInfo";
					break;
			   case FILEMGR_FSREFMAKEPATH:
					p = "FSRefMakePath";
					break;
			   case FILEMGR_FSPATHMAKEREF:
					p = "FSPathMakeRef";
					break;
			   // SPEC based calls
			   case FILEMGR_PBGETCATINFO:
					p = "GetCatInfo";
					break;
			   case FILEMGR_PBGETCATINFOLITE:
					p = "GetCatInfoLite";
					break;
			   case FILEMGR_PBHGETFINFO:
					p = "PBHGetFInfo";
					break;
			   case FILEMGR_PBXGETVOLINFO:
					p = "PBXGetVolInfo";
					break;
			   case FILEMGR_PBHCREATE:
					p = "PBHCreate";
					break;
			   case FILEMGR_PBHOPENDF:
					p = "PBHOpenDF";
					break;
			   case FILEMGR_PBHOPENRF:
					p = "PBHOpenRF";
					break;
			   case FILEMGR_PBHGETDIRACCESS:
					p = "PBHGetDirAccess";
					break;
			   case FILEMGR_PBHSETDIRACCESS:
					p = "PBHSetDirAccess";
					break;
			   case FILEMGR_PBHMAPID:
					p = "PBHMapID";
					break;
			   case FILEMGR_PBHMAPNAME:
					p = "PBHMapName";
					break;
			   case FILEMGR_PBCLOSE:
					p = "PBClose";
					break;
			   case FILEMGR_PBFLUSHFILE:
					p = "PBFlushFile";
					break;
			   case FILEMGR_PBGETEOF:
					p = "PBGetEOF";
					break;
			   case FILEMGR_PBSETEOF:
					p = "PBSetEOF";
					break;
			   case FILEMGR_PBGETFPOS:
					p = "PBGetFPos";
					break;
			   case FILEMGR_PBREAD:
					p = "PBRead";
					break;
			   case FILEMGR_PBWRITE:
					p = "PBWrite";
					break;
			   case FILEMGR_PBGETFCBINFO:
					p = "PBGetFCBInfo";
					break;
			   case FILEMGR_PBSETFINFO:
					p = "PBSetFInfo";
					break;
			   case FILEMGR_PBALLOCATE:
					p = "PBAllocate";
					break;
			   case FILEMGR_PBALLOCCONTIG:
					p = "PBAllocContig";
					break;
			   case FILEMGR_PBSETFPOS:
					p = "PBSetFPos";
					break;
			   case FILEMGR_PBSETCATINFO:
					p = "PBSetCatInfo";
					break;
			   case FILEMGR_PBGETVOLPARMS:
					p = "PBGetVolParms";
					break;
			   case FILEMGR_PBSETVINFO:
					p = "PBSetVInfo";
					break;
			   case FILEMGR_PBMAKEFSSPEC:
					p = "PBMakeFSSpec";
					break;
			   case FILEMGR_PBHGETVINFO:
					p = "PBHGetVInfo";
					break;
			   case FILEMGR_PBCREATEFILEIDREF:
					p = "PBCreateFileIDRef";
					break;
			   case FILEMGR_PBDELETEFILEIDREF:
					p = "PBDeleteFileIDRef";
					break;
			   case FILEMGR_PBRESOLVEFILEIDREF:
					p = "PBResolveFileIDRef";
					break;
			   case FILEMGR_PBFLUSHVOL:
					p = "PBFlushVol";
					break;
			   case FILEMGR_PBHRENAME:
					p = "PBHRename";
					break;
			   case FILEMGR_PBCATMOVE:
					p = "PBCatMove";
					break;
			   case FILEMGR_PBEXCHANGEFILES:
					p = "PBExchangeFiles";
					break;
			   case FILEMGR_PBHDELETE:
					p = "PBHDelete";
					break;
			   case FILEMGR_PBDIRCREATE:
					p = "PBDirCreate";
					break;
			   case FILEMGR_PBCATSEARCH:
					p = "PBCatSearch";
					break;
			   case FILEMGR_PBHSETFLOCK:
					p = "PBHSetFlock";
					break;
			   case FILEMGR_PBHRSTFLOCK:
					p = "PBHRstFLock";
					break;
			   case FILEMGR_PBLOCKRANGE:
					p = "PBLockRange";
					break;
			   case FILEMGR_PBUNLOCKRANGE:
					p = "PBUnlockRange";
					break;
			   default:
			    		p = (char *)0;
					break;
			}
		        enter_syscall(thread, type, &kd[i], p, (double)now);
			continue;
		}
		switch (type) {

		case MACH_vmfault:
		    if (kd[i].arg2 == DBG_PAGEIN_FAULT)
		            exit_syscall("PAGE_IN", thread, type, 0, kd[i].arg1, 0, 2, (double)now);
		    else {
		            if (ti = find_thread(thread, type)) {
			            if (ti == &th_state[cur_max - 1])
				            cur_max--;
				    ti->thread = 0;
			    }
		    }
		    break;

		case MSC_map_fd:
		    exit_syscall("map_fd", thread, type, kd[i].arg1, kd[i].arg2, 1, 0, (double)now);
		    break;

		case BSC_mmap:
		    exit_syscall("mmap", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;

		case BSC_recvmsg:
		    exit_syscall("recvmsg", thread, type, kd[i].arg1, kd[i].arg2, 1, 1, (double)now);
		    break;

		case BSC_sendmsg:
		    exit_syscall("sendmsg", thread, type, kd[i].arg1, kd[i].arg2, 1, 1, (double)now);
		    break;

		case BSC_recvfrom:
		    exit_syscall("recvfrom", thread, type, kd[i].arg1, kd[i].arg2, 1, 1, (double)now);
		    break;

		case BSC_sendto:
		    exit_syscall("sendto", thread, type, kd[i].arg1, kd[i].arg2, 1, 1, (double)now);
		    break;

		case BSC_stat:
		    exit_syscall("stat", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;

		case BSC_open:
		    exit_syscall("open", thread, type, kd[i].arg1, kd[i].arg2, 2, 0, (double)now);
		    break;

		case BSC_close:
		    exit_syscall("close", thread, type, kd[i].arg1, kd[i].arg2, 1, 0, (double)now);
		    break;

		case BSC_read:
		    exit_syscall("read", thread, type, kd[i].arg1, kd[i].arg2, 1, 1, (double)now);
		    break;

		case BSC_write:
		    exit_syscall("write", thread, type, kd[i].arg1, kd[i].arg2, 1, 1, (double)now);
		    break;

		case BSC_fstat:
		    exit_syscall("fstat", thread, type, kd[i].arg1, kd[i].arg2, 1, 0, (double)now);
		    break;

		case BSC_lstat:
		    exit_syscall("lstat", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;

		case BSC_link:
		    exit_syscall("link", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;

		case BSC_unlink:
		    exit_syscall("unlink", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;

		case BSC_mknod:
		    exit_syscall("mknod", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;

		case BSC_chmod:
		    exit_syscall("chmod", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;

		case BSC_chown:
		    exit_syscall("chown", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;

		case BSC_access:
		    exit_syscall("access", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;

		case BSC_chflags:
		    exit_syscall("chflags", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;

		case BSC_fchflags:
		    exit_syscall("fchflags", thread, type, kd[i].arg1, kd[i].arg2, 1, 0, (double)now);
		    break;

		case BSC_sync:
		    exit_syscall("sync", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;

		case BSC_symlink:
		    exit_syscall("symlink", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;

		case BSC_readlink:
		    exit_syscall("readlink", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;

		case BSC_fsync:
		    exit_syscall("fsync", thread, type, kd[i].arg1, kd[i].arg2, 1, 0, (double)now);
		    break;

		case BSC_readv:
		    exit_syscall("readv", thread, type, kd[i].arg1, kd[i].arg2, 1, 1, (double)now);
		    break;

		case BSC_writev:
		    exit_syscall("writev", thread, type, kd[i].arg1, kd[i].arg2, 1, 1, (double)now);
		    break;

		case BSC_fchown:
		    exit_syscall("fchown", thread, type, kd[i].arg1, kd[i].arg2, 1, 0, (double)now);
		    break;

		case BSC_fchmod:
		    exit_syscall("fchmod", thread, type, kd[i].arg1, kd[i].arg2, 1, 0, (double)now);
		    break;

		case BSC_rename:
		    exit_syscall("rename", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;

		case BSC_mkdir:
		    exit_syscall("mkdir", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;

		case BSC_rmdir:
		    exit_syscall("rmdir", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;

		case BSC_statfs:
		    exit_syscall("statfs", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;

		case BSC_fstatfs:
		    exit_syscall("fstatfs", thread, type, kd[i].arg1, kd[i].arg2, 1, 0, (double)now);
		    break;

		case BSC_pathconf:
		    exit_syscall("pathconf", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;

		case BSC_fpathconf:
		    exit_syscall("fpathconf", thread, type, kd[i].arg1, kd[i].arg2, 1, 0, (double)now);
		    break;

		case BSC_getdirentries:
		    exit_syscall("getdirentries", thread, type, kd[i].arg1, kd[i].arg2, 1, 1, (double)now);
		    break;

		case BSC_lseek:
		    exit_syscall("lseek", thread, type, kd[i].arg1, kd[i].arg3, 1, 5, (double)now);
		    break;

		case BSC_truncate:
		    exit_syscall("truncate", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;

		case BSC_ftruncate:
		    exit_syscall("ftruncate", thread, type, kd[i].arg1, kd[i].arg2, 1, 3, (double)now);
		    break;

		case BSC_statv:
		    exit_syscall("statv", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;

		case BSC_lstatv:
		    exit_syscall("lstatv", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;

		case BSC_fstatv:
		    exit_syscall("fstatv", thread, type, kd[i].arg1, kd[i].arg2, 1, 0, (double)now);
		    break;

		case BSC_mkcomplex:
		    exit_syscall("mkcomplex", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;

		case BSC_getattrlist:
		    exit_syscall("getattrlist", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;

		case BSC_setattrlist:
		    exit_syscall("setattrlist", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;

		case BSC_getdirentriesattr:
		    exit_syscall("getdirentriesattr", thread, type, kd[i].arg1, kd[i].arg2, 0, 1, (double)now);
		    break;

		case BSC_exchangedata:
		    exit_syscall("exchangedata", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;

		case BSC_checkuseraccess:
		    exit_syscall("checkuseraccess", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;

		case BSC_searchfs:
		    exit_syscall("searchfs", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;

	   case FILEMGR_PBGETCATALOGINFO:
		    exit_syscall("GetCatalogInfo", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBGETCATALOGINFOBULK:
		    exit_syscall("GetCatalogInfoBulk", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBCREATEFILEUNICODE:
		    exit_syscall("CreateFileUnicode", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBCREATEDIRECTORYUNICODE:
		    exit_syscall("CreateDirectoryUnicode", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBCREATEFORK:
		    exit_syscall("PBCreateFork", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBDELETEFORK:
		    exit_syscall("PBDeleteFork", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBITERATEFORK:
		    exit_syscall("PBIterateFork", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBOPENFORK:
		    exit_syscall("PBOpenFork", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBREADFORK:
		    exit_syscall("PBReadFork", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBWRITEFORK:
		    exit_syscall("PBWriteFork", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBALLOCATEFORK:
		    exit_syscall("PBAllocateFork", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBDELETEOBJECT:
		    exit_syscall("PBDeleteObject", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBEXCHANGEOBJECT:
		    exit_syscall("PBExchangeObject", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBGETFORKCBINFO:
		    exit_syscall("PBGetForkCBInfo", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBGETVOLUMEINFO:
		    exit_syscall("PBGetVolumeInfo", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBMAKEFSREF:
		    exit_syscall("PBMakeFSRef", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBMAKEFSREFUNICODE:
		    exit_syscall("PBMakeFSRefUnicode", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBMOVEOBJECT:
		    exit_syscall("PBMoveObject", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBOPENITERATOR:
		    exit_syscall("PBOpenIterator", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBRENAMEUNICODE:
		    exit_syscall("PBRenameUnicode", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBSETCATALOGINFO:
		    exit_syscall("PBSetCatalogInfo", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBSETVOLUMEINFO:
		    exit_syscall("PBSetVolumeInfo", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_FSREFMAKEPATH:
		    exit_syscall("FSRefMakePath", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_FSPATHMAKEREF:
		    exit_syscall("FSPathMakeRef", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBGETCATINFO:
		    exit_syscall("GetCatInfo", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBGETCATINFOLITE:
		    exit_syscall("GetCatInfoLite", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBHGETFINFO:
		    exit_syscall("PBHGetFInfo", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBXGETVOLINFO:
		    exit_syscall("PBXGetVolInfo", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBHCREATE:
		    exit_syscall("PBHCreate", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBHOPENDF:
		    exit_syscall("PBHOpenDF", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBHOPENRF:
		    exit_syscall("PBHOpenRF", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBHGETDIRACCESS:
		    exit_syscall("PBHGetDirAccess", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBHSETDIRACCESS:
		    exit_syscall("PBHSetDirAccess", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBHMAPID:
		    exit_syscall("PBHMapID", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBHMAPNAME:
		    exit_syscall("PBHMapName", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBCLOSE:
		    exit_syscall("PBClose", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBFLUSHFILE:
		    exit_syscall("PBFlushFile", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBGETEOF:
		    exit_syscall("PBGetEOF", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBSETEOF:
		    exit_syscall("PBSetEOF", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBGETFPOS:
		    exit_syscall("PBGetFPos", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBREAD:
		    exit_syscall("PBRead", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBWRITE:
		    exit_syscall("PBWrite", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBGETFCBINFO:
		    exit_syscall("PBGetFCBInfo", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBSETFINFO:
		    exit_syscall("PBSetFInfo", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBALLOCATE:
		    exit_syscall("PBAllocate", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBALLOCCONTIG:
		    exit_syscall("PBAllocContig", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBSETFPOS:
		    exit_syscall("PBSetFPos", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBSETCATINFO:
		    exit_syscall("PBSetCatInfo", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBGETVOLPARMS:
		    exit_syscall("PBGetVolParms", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBSETVINFO:
		    exit_syscall("PBSetVInfo", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBMAKEFSSPEC:
		    exit_syscall("PBMakeFSSpec", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBHGETVINFO:
		    exit_syscall("PBHGetVInfo", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBCREATEFILEIDREF:
		    exit_syscall("PBCreateFileIDRef", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBDELETEFILEIDREF:
		    exit_syscall("PBDeleteFileIDRef", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBRESOLVEFILEIDREF:
		    exit_syscall("PBResolveFileIDRef", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBFLUSHVOL:
		    exit_syscall("PBFlushVol", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBHRENAME:
		    exit_syscall("PBHRename", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBCATMOVE:
		    exit_syscall("PBCatMove", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBEXCHANGEFILES:
		    exit_syscall("PBExchangeFiles", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBHDELETE:
		    exit_syscall("PBHDelete", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBDIRCREATE:
		    exit_syscall("PBDirCreate", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBCATSEARCH:
		    exit_syscall("PBCatSearch", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBHSETFLOCK:
		    exit_syscall("PBHSetFLock", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBHRSTFLOCK:
		    exit_syscall("PBHRstFLock", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBLOCKRANGE:
		    exit_syscall("PBLockRange", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
	   case FILEMGR_PBUNLOCKRANGE:
		    exit_syscall("PBUnlockRange", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, (double)now);
		    break;
		default:
		    break;
		}
	}
	fflush(0);
}

void
enter_syscall(int thread, int type, kd_buf *kd, char *name, double now)
{
       struct th_info *ti;
       int    i;
       int    secs;
       int    usecs;
       long long l_usecs;
       long curr_time;
	   kd_threadmap *map;
	   kd_threadmap *find_thread_map();

       switch (type) {

       case MACH_vmfault:
       case MSC_map_fd:
       case BSC_mmap:
       case BSC_recvmsg:
       case BSC_sendmsg:
       case BSC_recvfrom:
       case BSC_sendto:
       case BSC_stat:
       case BSC_open:
       case BSC_close:
       case BSC_read:
       case BSC_write:
       case BSC_fstat:
       case BSC_lstat:
       case BSC_link:
       case BSC_unlink:
       case BSC_mknod:
       case BSC_chmod:
       case BSC_chown:
       case BSC_access:
       case BSC_chflags:
       case BSC_fchflags:
       case BSC_sync:
       case BSC_symlink:
       case BSC_readlink:
       case BSC_fsync:
       case BSC_readv:
       case BSC_writev:
       case BSC_fchown:
       case BSC_fchmod:
       case BSC_rename:
       case BSC_mkdir:
       case BSC_rmdir:
       case BSC_statfs:
       case BSC_fstatfs:
       case BSC_pathconf:
       case BSC_fpathconf:
       case BSC_getdirentries:
       case BSC_lseek:
       case BSC_truncate:
       case BSC_ftruncate:
       case BSC_statv:
       case BSC_lstatv:
       case BSC_fstatv:
       case BSC_mkcomplex:
       case BSC_getattrlist:
       case BSC_setattrlist:
       case BSC_getdirentriesattr:
       case BSC_exchangedata:
       case BSC_checkuseraccess:
       case BSC_searchfs:
	   case FILEMGR_PBGETCATALOGINFO:
	   case FILEMGR_PBGETCATALOGINFOBULK:
	   case FILEMGR_PBCREATEFILEUNICODE:
	   case FILEMGR_PBCREATEDIRECTORYUNICODE:
	   case FILEMGR_PBCREATEFORK:
	   case FILEMGR_PBDELETEFORK:
	   case FILEMGR_PBITERATEFORK:
	   case FILEMGR_PBOPENFORK:
	   case FILEMGR_PBREADFORK:
	   case FILEMGR_PBWRITEFORK:
	   case FILEMGR_PBALLOCATEFORK:
	   case FILEMGR_PBDELETEOBJECT:
	   case FILEMGR_PBEXCHANGEOBJECT:
	   case FILEMGR_PBGETFORKCBINFO:
	   case FILEMGR_PBGETVOLUMEINFO:
	   case FILEMGR_PBMAKEFSREF:
	   case FILEMGR_PBMAKEFSREFUNICODE:
	   case FILEMGR_PBMOVEOBJECT:
	   case FILEMGR_PBOPENITERATOR:
	   case FILEMGR_PBRENAMEUNICODE:
	   case FILEMGR_PBSETCATALOGINFO:
	   case FILEMGR_PBSETVOLUMEINFO:
	   case FILEMGR_FSREFMAKEPATH:
	   case FILEMGR_FSPATHMAKEREF:

	   case FILEMGR_PBGETCATINFO:
	   case FILEMGR_PBGETCATINFOLITE:
	   case FILEMGR_PBHGETFINFO:
	   case FILEMGR_PBXGETVOLINFO:
	   case FILEMGR_PBHCREATE:
	   case FILEMGR_PBHOPENDF:
	   case FILEMGR_PBHOPENRF:
	   case FILEMGR_PBHGETDIRACCESS:
	   case FILEMGR_PBHSETDIRACCESS:
	   case FILEMGR_PBHMAPID:
	   case FILEMGR_PBHMAPNAME:
	   case FILEMGR_PBCLOSE:
	   case FILEMGR_PBFLUSHFILE:
	   case FILEMGR_PBGETEOF:
	   case FILEMGR_PBSETEOF:
	   case FILEMGR_PBGETFPOS:
	   case FILEMGR_PBREAD:
	   case FILEMGR_PBWRITE:
	   case FILEMGR_PBGETFCBINFO:
	   case FILEMGR_PBSETFINFO:
	   case FILEMGR_PBALLOCATE:
	   case FILEMGR_PBALLOCCONTIG:
	   case FILEMGR_PBSETFPOS:
	   case FILEMGR_PBSETCATINFO:
	   case FILEMGR_PBGETVOLPARMS:
	   case FILEMGR_PBSETVINFO:
	   case FILEMGR_PBMAKEFSSPEC:
	   case FILEMGR_PBHGETVINFO:
	   case FILEMGR_PBCREATEFILEIDREF:
	   case FILEMGR_PBDELETEFILEIDREF:
	   case FILEMGR_PBRESOLVEFILEIDREF:
	   case FILEMGR_PBFLUSHVOL:
	   case FILEMGR_PBHRENAME:
	   case FILEMGR_PBCATMOVE:
	   case FILEMGR_PBEXCHANGEFILES:
	   case FILEMGR_PBHDELETE:
	   case FILEMGR_PBDIRCREATE:
	   case FILEMGR_PBCATSEARCH:
	   case FILEMGR_PBHSETFLOCK:
	   case FILEMGR_PBHRSTFLOCK:
	   case FILEMGR_PBLOCKRANGE:
	   case FILEMGR_PBUNLOCKRANGE:


	   for (i = 0, ti = th_state; ti < &th_state[MAX_THREADS]; ti++, i++) {
	           if (ti->thread == 0)
		           break;
	   }
	   if (ti == &th_state[MAX_THREADS])
	           return;
	   if (i >= cur_max)
	           cur_max = i + 1;

	   if ((type >> 24) == FILEMGR_CLASS) {
		   ti->in_filemgr = 1;

		   l_usecs = (long long)(now / divisor);
		   secs = l_usecs / 1000000;

		   if (bias_secs == 0) {
		           curr_time = time((long *)0);
			   bias_secs = curr_time - secs;
		   }
		   curr_time = bias_secs + secs;
		   printf("%-8.8s", &(ctime(&curr_time)[11]));

		   if (COLS > 110 || wideflag) {
		           usecs = l_usecs - (long long)((long long)secs * 1000000);
			   printf(".%03ld", (long)usecs / 1000);
		   }
		   map = find_thread_map(thread);
		   if (map) {
		       char buf[128];
		       int  clen;

		       sprintf(buf, "  %s", name);
		       clen = strlen(buf);

		       if (clen > 25)
			   clen = 25;
		       memset(&buf[clen], ' ', 26 - clen);

		       sprintf(&buf[25], "(%d, 0x%x, 0x%x, 0x%x)", (short)kd->arg1, kd->arg2, kd->arg3, kd->arg4);
		       clen = strlen(&buf[25]) + 25;
		       memset(&buf[clen], ' ', 128 - clen);

		       if (COLS > 110 || wideflag)
			   sprintf(&buf[81], "%s\n", map->command); 
		       else
			   sprintf(&buf[60], "%.12s\n", map->command); 

		       printf(buf);

		   } else
			   printf("  %-24.24s (%5d, %#x, 0x%x, 0x%x)\n",         name, (short)kd->arg1, kd->arg2, kd->arg3, kd->arg4);
      
	   } else {
			   ti->in_filemgr = 0;
	   }
	   ti->thread = thread;
	   ti->vfslookup = 0;
	   ti->waited = 0;
	   ti->type   = type;
	   ti->stime  = now;
	   ti->arg1   = kd->arg1;
	   ti->arg2   = kd->arg2;
	   ti->arg3   = kd->arg3;
	   ti->arg4   = kd->arg4;
	   ti->pathname[0] = 0;

	   break;

       default:
	   break;
       }
}


void
exit_syscall(char *sc_name, int thread, int type, int error, int retval,
	                    int has_fd, int has_ret, double now)
{
       struct th_info *ti;
       int secs;
       int usecs;
       int nopadding;
       long long l_usecs;
       long curr_time;
       kd_threadmap *map;
       kd_threadmap *find_thread_map();
      
       if ((ti = find_thread(thread, type)) == (struct th_info *)0)
	       return;
       map = find_thread_map(thread);
       l_usecs = (long long)(now / divisor);
       secs = l_usecs / 1000000;

       if (bias_secs == 0) {
	       curr_time = time((long *)0);
	       bias_secs = curr_time - secs;
       }
       curr_time = bias_secs + secs;
       printf("%-8.8s", &(ctime(&curr_time)[11]));


       if (COLS > 110 || wideflag) {
	       nopadding = 0;
	       usecs = l_usecs - (long long)((long long)secs * 1000000);
	       printf(".%03ld", (long)usecs / 1000);

	       if ((type >> 24) != FILEMGR_CLASS) {
		   if (find_thread(thread, -1)) {
		       printf("   ");
		       nopadding = 1;
		   }
	       }
       } else
	       nopadding = 1;

       if (((type >> 24) == FILEMGR_CLASS) && (COLS > 110 || wideflag)) 
	       printf("  %-18.18s", sc_name);
       else
	       printf("  %-15.15s", sc_name);

       if (COLS > 110 || wideflag) {
	       if (has_fd == 2 && error == 0)
		       printf(" F=%-3d", retval);
	       else if (has_fd == 1)
		       printf(" F=%-3d", ti->arg1);
	       else if (has_ret != 2)
		       printf("      ");

	       if (error)
		       printf("[%3d]       ", error);
	       else if (has_ret == 3)
		       printf("O=0x%8.8x", ti->arg3);
	       else if (has_ret == 5)
		       printf("O=0x%8.8x", retval);
	       else if (has_ret == 2) 
		       printf(" A=0x%8.8x     ", retval);
	       else if (has_ret == 1)
		       printf("  B=0x%-6x", retval);
	       else if (has_ret == 4)
		       printf("R=0x%-8x", retval);
	       else
		       printf("            ");
       }
       printf(" %-28.28s ",  ti->pathname);

       usecs = (unsigned long)(((double)now - ti->stime) / divisor);
       secs = usecs / 1000000;
       usecs -= secs * 1000000;

       if ((type >> 24) != FILEMGR_CLASS && !nopadding)
	       printf("   ");
	       
       printf(" %2ld.%06ld", (long)secs, (long)usecs);
       if (ti->waited)
	       printf(" W");
       else
	       printf("  ");

       if (map) {
	       if (COLS > 110 || wideflag)
		       printf(" %s", map->command);
	       else
		       printf(" %.12s", map->command);
       }
       printf("\n");

       if (ti == &th_state[cur_max - 1])
	         cur_max--;
       ti->thread = 0;
}

int
quit(s)
char *s;
{
        if (trace_enabled)
	       set_enable(0);

	/* 
	   This flag is turned off when calling
	   quit() due to a set_remove() failure.
	*/
	if (set_remove_flag)
	        set_remove();

        printf("fs_usage: ");
	if (s)
		printf("%s", s);

	exit(1);
}


void getdivisor()
{

    unsigned int delta;
    unsigned int abs_to_ns_num;
    unsigned int abs_to_ns_denom;
    unsigned int proc_to_abs_num;
    unsigned int proc_to_abs_denom;

	extern void MKGetTimeBaseInfo(unsigned int *, unsigned int *, unsigned int *, unsigned int *, unsigned int *);

    MKGetTimeBaseInfo (&delta, &abs_to_ns_num, &abs_to_ns_denom,
		       &proc_to_abs_num,  &proc_to_abs_denom);

    divisor = ((double)abs_to_ns_denom / (double)abs_to_ns_num) * 1000;
}


void read_command_map()
{
    size_t size;
    int mib[6];
  
    if (mapptr) {
	free(mapptr);
	mapptr = 0;
    }
    total_threads = bufinfo.nkdthreads;
    size = bufinfo.nkdthreads * sizeof(kd_threadmap);

    if (size)
    {
        if (mapptr = (kd_threadmap *) malloc(size))
	     bzero (mapptr, size);
	else
	{
	    printf("Thread map is not initialized -- this is not fatal\n");
	    return;
	}
    }
 
    /* Now read the threadmap */
    mib[0] = CTL_KERN;
    mib[1] = KERN_KDEBUG;
    mib[2] = KERN_KDTHRMAP;
    mib[3] = 0;
    mib[4] = 0;
    mib[5] = 0;		/* no flags */
    if (sysctl(mib, 3, mapptr, &size, NULL, 0) < 0)
    {
        /* This is not fatal -- just means I cant map command strings */

        printf("Can't read the thread map -- this is not fatal\n");
	free(mapptr);
	mapptr = 0;
	return;
    }
    return;
}


void create_map_entry(int thread, char *command)
{
    int i, n;
    kd_threadmap *map;

    if (!mapptr)
        return;

    for (i = 0, map = 0; !map && i < total_threads; i++)
    {
        if (mapptr[i].thread == thread )	
	    map = &mapptr[i];   /* Reuse this entry, the thread has been reassigned */
    }

    if (!map)   /* look for invalid entries that I can reuse*/
    {
        for (i = 0, map = 0; !map && i < total_threads; i++)
	{
	    if (mapptr[i].valid == 0 )	
	        map = &mapptr[i];   /* Reuse this invalid entry */
	}
    }
  
    if (!map)
    {
        /* If reach here, then this is a new thread and 
	 * there are no invalid entries to reuse
	 * Double the size of the thread map table.
	 */

        n = total_threads * 2;
	mapptr = (kd_threadmap *) realloc(mapptr, n * sizeof(kd_threadmap));
	bzero(&mapptr[total_threads], total_threads*sizeof(kd_threadmap));
	map = &mapptr[total_threads];
	total_threads = n;
    }

    map->valid = 1;
    map->thread = thread;
    (void)strncpy (map->command, command, sizeof(map->command));
    map->command[sizeof(map->command)-1] = '\0';
}


kd_threadmap *find_thread_map(int thread)
{
    int i;
    kd_threadmap *map;

    if (!mapptr)
        return((kd_threadmap *)0);

    for (i = 0; i < total_threads; i++)
    {
        map = &mapptr[i];
	if (map->valid && (map->thread == thread))
	{
	    return(map);
	}
    }
    return ((kd_threadmap *)0);
}


void
kill_thread_map(int thread)
{
    kd_threadmap *map;

    if (map = find_thread_map(thread)) {
        map->valid = 0;
	map->thread = 0;
	map->command[0] = '\0';
    }
}

void
argtopid(str)
        char *str;
{
        char *cp;
        int ret;
	int i;

        ret = (int)strtol(str, &cp, 10);
        if (cp == str || *cp) {
	  /* Assume this is a command string and find matching pids */
	        if (!kp_buffer)
		        find_proc_names();

	        for (i=0; i < kp_nentries && num_of_pids < (MAX_PIDS - 1); i++) {
		      if(kp_buffer[i].kp_proc.p_stat == 0)
		          continue;
		      else {
			  if(!strcmp(str, kp_buffer[i].kp_proc.p_comm))
			    pids[num_of_pids++] = kp_buffer[i].kp_proc.p_pid;
		      }
		}
	}
	else if (num_of_pids < (MAX_PIDS - 1))
	        pids[num_of_pids++] = ret;

        return;
}


