#define _XOPEN_SOURCE 700
#define FUSE_USE_VERSION 35

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <signal.h>

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#include <sys/shm.h>
#include <sys/ipc.h>

#include <fuse.h>

#include "nvstd/rtypes.h"
#include "nvstd/delay.h"

//#define FUSE_IS_PARENT
#define SHOW_FULL_STORAGE

#define SIZE 4096
#define PERM 0666

#define SKIPROOT(path) if (strcmp(path, "/") != 0) return -ENOENT;
#define MOUNTPOINT "./ftest"

u8* storage;

// File stats
u64 acc_time;
u64 upd_time;
u64 init_time;
uid_t cur_uid;
gid_t cur_gid;

i32 fork_pid;

u32 tick = 0;

// Shared memory
#define SHM_DATA_SIZE 256
#define SHM_DATA_START 16
#define SHM_BUF_SIZE (SHM_DATA_SIZE + SHM_DATA_START)
key_t shm_key;
i32 shm_id;
u8* shm_buf;

// https://stackoverflow.com/questions/2745074
#define CEILDIV(x,y) (x / y) + ((x % y) != 0)


//////// FUSE functions
static void *test_init(
    struct fuse_conn_info *conn,
	struct fuse_config *cfg
    )
{
    cfg->direct_io = 1;
    return NULL; // All the examples do this so I guess I will too
}

static int test_getattr(
    const char* path, struct stat* stbuf, 
    struct fuse_file_info* fi
    )
{
    SKIPROOT(path);
    stbuf->st_mode = S_IFREG | PERM;
    stbuf->st_nlink = 1;
    stbuf->st_size = SIZE;
    stbuf->st_blocks = 0;
    stbuf->st_uid = cur_uid;
    stbuf->st_gid = cur_gid;
    stbuf->st_atime = acc_time;
    stbuf->st_mtime = upd_time;
    stbuf->st_ctime = init_time;
    return 0;
}

static int test_chown(
    const char *path, uid_t uid, gid_t gid,
    struct fuse_file_info *fi
    )
{
    SKIPROOT(path);
    cur_uid = uid;
    cur_gid = gid;
    return 0;
}

static int test_open(
    const char* path, 
    struct fuse_file_info* fi
    )
{
    SKIPROOT(path);
    return 0;
}

static int test_truncate(
    const char* path, off_t size, 
    struct fuse_file_info* fi
    )
{
    SKIPROOT(path);
    return 0;
}

void wait_for_rw()
{
    u32 timeout = 5000;
    do { delay_ms(1); timeout--; }
    while (shm_buf[0] != 'f' && timeout > 0);
    if (timeout <= 0)
    {
        printf("R/W timed out!\n");
        exit(-1);
    }
}

static int test_read(
    const char* path, char* buf, 
    size_t size, off_t offset, 
    struct fuse_file_info* fi
    )
{
    SKIPROOT(path);
    printf("READ: ofs %d / len %d\n", (i32)offset, (i32)size);
    if (offset >= SIZE || offset < 0) return 0;
    
    i32 size_delta = (offset + size) - SIZE;

    if (size_delta > 0)
    {
        if (((i32)size - size_delta) <= 0)
            return 0;
        size -= size_delta;
    }

    u32 num_blocks = CEILDIV(size, SHM_DATA_SIZE);
    u32 size_remaining = (u32)size;
    for (u32 i = 0; i < num_blocks; i++)
    {
        u32 block_start = i * SHM_DATA_SIZE;

        u32 cur_len = SHM_DATA_SIZE;
        if (size_remaining < SHM_DATA_SIZE)
            cur_len = size_remaining;

        size_remaining -= SHM_DATA_SIZE;

        u32 cur_ofs = offset + block_start;

        memcpy(shm_buf + 1, &cur_len, 4);
        memcpy(shm_buf + 5, &cur_ofs, 4);
        shm_buf[0] = 'r';
        
        wait_for_rw();

        u32 from_addr = block_start;
        u32 to_addr = SHM_DATA_START;
        for (u32 j = 0; j < cur_len; j++)
        {  
            buf[from_addr] = shm_buf[to_addr];
            from_addr++;
            to_addr++;
        }
    }

    acc_time = time(NULL);

    return size;
}

static int test_write(
    const char* path, const char* buf, 
    size_t size, off_t offset, 
    struct fuse_file_info* fi
    )
{
    SKIPROOT(path);
    printf("WRITE: ofs %d / len %d\n", (i32)offset, (i32)size);
    if (offset >= SIZE || offset < 0) return 0;

    i32 size_delta = (offset + size) - SIZE;

    if (size_delta > 0)
    {
        if (((i32)size - size_delta) <= 0)
            return 0;
        size -= size_delta;
    }

    u32 num_blocks = CEILDIV(size, SHM_DATA_SIZE);
    u32 size_remaining = (u32)size;
    for (u32 i = 0; i < num_blocks; i++)
    {
        u32 block_start = i * SHM_DATA_SIZE;

        u32 cur_len = SHM_DATA_SIZE;
        if (size_remaining < SHM_DATA_SIZE)
            cur_len = size_remaining;

        size_remaining -= SHM_DATA_SIZE;

        u32 cur_ofs = offset + block_start;

        memcpy(shm_buf + 1, &cur_len, 4);
        memcpy(shm_buf + 5, &cur_ofs, 4);

        u32 from_addr = block_start;
        u32 to_addr = SHM_DATA_START;
        for (u32 j = 0; j < cur_len; j++)
        {  
            shm_buf[to_addr] = buf[from_addr];
            from_addr++;
            to_addr++;
        }

        shm_buf[0] = 'w';

        wait_for_rw();
    }

    acc_time = time(NULL);
    upd_time = time(NULL);

    return size;
}

static const struct fuse_operations test_oper = {
    .init = test_init,
    .getattr = test_getattr,
    .chown = test_chown,
    .open = test_open,
    .read = test_read,
    .write = test_write,
    .truncate = test_truncate,
};


//////// Core functions
void do_rw()
{
    char first = (char)shm_buf[0];

    if (first == 'r' || first == 'w')
    {
        u32 len, ofs;
        memcpy(&len, shm_buf + 1, 4);
        memcpy(&ofs, shm_buf + 5, 4);

        if (first == 'r') 
        {
            //memcpy(shm_buf + SHM_DATA_START, storage + ofs, len);
            for (u32 i = 0; i < len; i++)
            {
                shm_buf[SHM_DATA_START + i] = storage[i + ofs];
            }
        }
        else if (first == 'w')
        {
            //memcpy(storage + ofs, shm_buf + SHM_DATA_START, len);
            for (u32 i = 0; i < len; i++)
            {
                storage[i + ofs] = shm_buf[SHM_DATA_START + i];
            }
        }

        shm_buf[0] = 'f';
    }
}

void cleanup()
{ 
    shmdt(shm_buf);
#ifndef FUSE_IS_PARENT
    if (fork_pid > 0)
#else
    if (fork_pid == 0)
#endif
    {
        printf("Storage exiting...\n");
        free(storage);
        shmctl(shm_id, IPC_RMID, NULL);
    }
    else
    {
        printf("FUSE exiting...\n");
    }
}

char* test_args[] = { 
    "", MOUNTPOINT, "-f", "-s",
    "-o", "auto_unmount,allow_other",
#ifdef FUSE_IS_PARENT
    "-d",
#endif
};

void clean_exit() { exit(0); }

int main()
{
    fclose(fopen(MOUNTPOINT, "w")); // (Re)initialize our mount file
    atexit(cleanup);

    cur_uid = getuid(); cur_gid = getgid();
    acc_time = upd_time = init_time = time(NULL); 

    // Setup shared memory
    shm_key = ftok("ftkey", 100);
    shm_id = shmget(shm_key, SHM_BUF_SIZE, 0666 | IPC_CREAT);
    shm_buf = (u8*)shmat(shm_id, (void*)0, 0);

    memset(shm_buf, 0, SHM_BUF_SIZE);

    fork_pid = fork();

#ifndef FUSE_IS_PARENT
    if (fork_pid == 0) 
        return fuse_main(6, test_args, &test_oper, NULL);
#else
    if (fork_pid > 0) 
        return fuse_main(7, test_args, &test_oper, NULL);
#endif
        
    // Wait until after forking as SIGINT is already handled by FUSE
    signal(SIGINT, clean_exit);
    storage = malloc(SIZE);

    // Print the contents of our storage array to the terminal
    for (;;)
    {
        do_rw();

#ifndef FUSE_IS_PARENT
        printf("\e[H\e[2J\e[3J");
        printf("Tick: %d\n", tick);
        
        printf("\nShared buffer - header:\n");
        for (u32 i = 0; i < SHM_DATA_START; i++)
        {
            printf("%02hhX ", shm_buf[i]);
        }

        printf("\n\nShared buffer - data:");
        for (u32 i = SHM_DATA_START; i < SHM_BUF_SIZE; i++)
        {
            // Wrap every 64 bytes
            if (((i - SHM_DATA_START) % 64) == 0) printf("\n");
            printf("%02hhX ", shm_buf[i]);
        }

#ifdef SHOW_FULL_STORAGE
        printf("\n\nStorage:");
        for (u32 i = 0; i < SIZE; i++)
        {
            if ((i % 64) == 0) printf("\n");
            printf("%02hhX ", storage[i]);
        }
#endif     

        printf("\n");
        tick++;
#endif

        delay_ms(30);
    }

    return 0;
}
