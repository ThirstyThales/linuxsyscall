#define _GNU_SOURCE 1
#include<fcntl.h>
#include<unistd.h>
#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<poll.h>
#include<sys/select.h>
#include<sys/mman.h>
#include<sys/stat.h>
#include<string.h>
#include<stdbool.h>
#include<stdint.h>
#include<sys/ioctl.h>
#include<time.h>
#include "ioctl_driver/dev/ioctl.h"

void wait_any_button()
{
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(0, &rfds); // fd 0 is stdin
    select(1, &rfds, NULL, NULL, NULL);
    getchar();
    return;
}

void assert(int cond, const char* err)
{
    if(!cond)
    {
        printf("error: %s\n",err);
        exit(EXIT_FAILURE);
    }
}


/*
    5/341
    read write open close lseek
*/
void test1(int argc, char* argv[])
{
    printf("test1\n");

    int fid = 
        open("file",O_CREAT|O_RDWR,S_IRWXU|S_IRWXG|S_IRWXO);
    printf("open:\t%d\n",fid);

    const char tried[] = "This is a tried.";
    if(-1 == write(fid, tried, sizeof(tried)-1))
    {
        printf("write failed:\t%d\n",errno);
    }
    printf("lseek:\t%d\n",lseek(fid, 1, SEEK_SET));
    char buf[20] = {};
    printf("read:\t%d = ",read(fid, buf,5));
    buf[6]='\0';
    printf("%s\n",buf);

    printf("close:\t%d\n",close(fid));

    return;
}
/*
    6/341
    poll
*/
void test2(int argc, char* argv[])
{
    int nfds;
    struct pollfd fds;

    fds.fd = open("myfifo", O_RDONLY);
    assert(-1 != fds.fd, "myfifo can't open.");

    fds.events = POLLIN;
    while(fds.fd >= 0)
    {
        printf("About to poll()\n");
        int ready = poll(&fds, 1, -1);
        assert(-1 != ready, "poll error\n");
        printf("Ready:\t%d\n", ready);
        char buf[20];
        if(0 != fds.revents)
        {
            printf("events: %s%s%s\n", 
                (fds.revents & POLLIN)  ? "POLLIN "  : "",
                (fds.revents & POLLHUP) ? "POLLHUP " : "",
                (fds.revents & POLLERR) ? "POLLERR " : "");
            if(POLLIN & fds.revents)
            {
                ssize_t s = read(fds.fd, buf, sizeof(buf));
                assert(-1 != s, "read error\n");
                printf("\tread %zd bytes: %.*s\n",
                    s, (int)s, buf);
            }
            if((POLLHUP | POLLERR) & fds.revents)
            {
                printf("\tover\n");
                close(fds.fd);
                break;
            }
        }
    }
    return;
}
/*
    7/341
    select
*/
void test3(int argc, char* argv[])
{
    fd_set rfds;
    struct timeval tv;
    int retval;

    FD_ZERO(&rfds);
    FD_SET(0, &rfds);

    tv.tv_sec = 5;
    tv.tv_usec = 0;

    retval = select(1, &rfds, NULL, NULL, &tv);

    if (retval == -1)
        perror("select()");
    else if (retval)
        printf("Data is available now.\n");
        /* FD_ISSET(0, &rfds) will be true. */
    else
        printf("No data within five seconds.\n");

    return;
}
/*
    12/341
    mmap munmap mremap mprotect msync
*/
void test4(int argc, char* argv[])
{
    int fd = open("file", O_RDONLY);
    assert(fd != -1,"can't open file\n");

    struct stat sb;
    fstat(fd, &sb);

    char * addr = mmap(NULL, sb.st_size, 
        PROT_READ, MAP_PRIVATE, fd, 0);
    assert(MAP_FAILED != addr, "mmap failed!\n");
    assert(0 == close(fd), "close file failed!\n");

    printf("file start, file size: %d\n",sb.st_size);
    write(STDOUT_FILENO, addr, sb.st_size);
    printf("file end \n");

    assert(0 == munmap(addr, sb.st_size), "unmap failed!\n");

    printf("start append \n");

    fd = open("file", O_RDWR);
    assert(fd != -1,"can't open file\n");

    addr = mmap(NULL, sb.st_size, 
        PROT_READ, MAP_SHARED, fd, 0);
    assert(MAP_FAILED != addr, "map failed!\n");   
    
    const char app[] = "[This text will not sync!]";
    size_t nlen = sb.st_size + sizeof(app);
    printf("nlen:%d\n",nlen);
    char * naddr = mremap((void*)addr, sb.st_size,
        nlen, MREMAP_MAYMOVE);
    assert(MAP_FAILED != naddr, "remap failed!\n"); 

    int ret = mprotect(naddr, nlen, PROT_READ|PROT_WRITE);
    assert(0 == ret, "mem protect modified failed!\n");

    memcpy(naddr+sb.st_size, app, sizeof(app));

    printf("new file:\n%s\n",naddr);

    ret = msync(naddr, nlen, MS_SYNC);
    assert(0 == ret, "sync file failed!\n");
    return;
}
/*
    13/341
    brk
*/
void test5(int argc, char* argv[])
{
    void *heap = sbrk(0);
    int *start = (int *)heap;

    brk(start + 0x2000000);

    int *p = start + 0x1000000;
    *p = 1;
    *(p+1) = 2;
    printf("%d %d\n", *p,*(p+1));

    brk(heap);

    return;
}
/*
    14/341
    ioctl
    Install ioctl_driver first!
    Then run with root.
*/
void test6(int argc, char* argv[])
{
    static const char dr_name[] = "/dev/ioctl";
    printf("Open Driver \n");
    int fd_driver = open(dr_name, O_RDWR);
    assert(-1 != fd_driver, "ERROR! Can't open driver.\n");

    uint32_t value = 0x0;
    int ret = ioctl(fd_driver, IOCTL_BASE_GET_MUIR, &value);
    assert(ret >= 0, "ERROR! ioctl error.\n");
    printf("Read Value: 0x%x\n", value);
    printf("Pause...\n");
    scanf("%x",&value);
    printf("Close Driver \n");
    assert(-1 != close(fd_driver), "ERROR! close failure.\n");
    return;
}
/*
    20/341
    time
    getcwd chdir fchdir mkdir rmdir
*/
void test7(int argc, char* argv[])
{
    const size_t size = 1024;
    char buf[size];
    char* ret = getcwd(buf, size);
    assert(ret == buf, "getcwd error\n");
    printf("start wd: %s\n",buf);

    assert(0 == chdir(".."), "chdir error\n");
    ret = getcwd(buf, size);
    assert(ret == buf, "getcwd error\n");  
    printf(".. wd: %s\n",buf);

    int fd = open("linuxsyscall", O_DIRECTORY | O_RDONLY);
    assert(0 <= fd, "open dir error\n");

    assert(0 == fchdir(fd), "fchdir error\n");

    ret = getcwd(buf, size);
    assert(ret == buf, "getcwd error\n");  
    printf("f wd: %s\n",buf);
    
    char name[32] = {};
    time_t  t = time(NULL);
    sprintf(name,"TMPDIR_%x\0",t);
    int iret = mkdir(name, S_IFDIR|S_IRWXU|S_IRWXG);
    assert(0 == iret, "mkdir error\n");
    printf("dir created.Press any button to delete\n");
    wait_any_button();
    iret = rmdir(name);
    assert(0 == iret, "rmdir error\n");
    return;
}

/*
    26/341
    umask creat chmod fchmod link unlink
*/
void test8(int argc, char* argv[])
{
    const char name[] = "test8_file";
    mode_t last = umask(0444);
    int fd = creat(name, S_IRWXU|S_IRWXG|S_IRWXO);
    assert(0 <= fd, "creat error\n");
    assert(0 == chmod(name, 0365), "chmod error\n");
    assert(0 == fchmod(fd, 0664), "fchmod error\n");
    printf("file created.Press any button to rename\n");
    wait_any_button();
    const char newname[] = "test8_newname";
    assert(0 == link(name, newname), "link error\n");
    printf("file renamed.Press any button to delete\n");
    wait_any_button();
    assert(0 == unlink(newname), "unlink error\n");
    assert(0 == unlink(name), "unlink error\n");
}


int main(int argc, char* argv[])
{
    for(int i = 0; i < argc; i++)
    {
        printf("Arg%d: %s\n", i, argv[i]);
    }
    test8(argc, argv);
    return 0;
}