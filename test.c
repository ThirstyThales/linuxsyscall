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
#include<limits.h>
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

/*
    28/341
    symlink readlink
*/
void test9(int argc, char* argv[])
{
    int ret = symlink("file","file_softlink");
    assert(0 == ret, "symlink error!\n");
    wait_any_button();
    char buf[123];
    ssize_t ss = readlink("file_softlink", buf, sizeof(buf));
    assert(-1 != ss, "readlink error!\n");
    printf("readlink: %.*s\n", ss, buf);
    wait_any_button();
    ret = unlink("file_softlink");
    assert(0 == ret, "unlink error!\n");
}

/*
    42/341
    setresuid setresgid
    getresuid getresgid
    setreuid setregid
    geteuid getuid getegid getgid 
    setgid setegid setuid seteuid
*/
void test10_printid()
{
    uid_t r,e,s;
    gid_t gr,ge,gs;
    int ret = getresuid(&r, &e, &s);
    assert(0 == ret, "getresuid error!\n");
    printf("read uid: %x, effective uid: %x, seted uid: %x\n",
        r, e, s);
    
    ret = getresgid(&gr, &ge, &gs);
    assert(0 == ret, "getresgid error!\n");
    printf("read gid: %x, effective gid: %x, seted gid: %x\n",
        gr, ge, gs);
}
void test10(int argc, char* argv[])
{
    int ret = 0;
    test10_printid();

    uid_t now_user = geteuid();
    if(now_user != getuid())
        printf("euid not equal ruid!\n");
    gid_t now_group = geteuid();
    if(now_group != getgid())
        printf("egid not equal rgid!\n");
    
    assert(0 == now_user, "not is root user!please run with sudo!\n");
    
    const uid_t usr = 1000;
    const uid_t grp = 1000;

    //ret = setgid(grp);
    //assert(0 == ret, "setgid(1000) error!\n");
    ret = setegid(grp);
    assert(0 == ret, "setegid(1000) error!\n");

    //ret = setuid(usr);//Can't regain root privileges
    //assert(0 == ret, "setuid(1000) error!\n");
    ret = seteuid(usr);
    assert(0 == ret, "seteuid(1000) error!\n");

    int nf = creat("fileNotRoot",S_IRWXU|S_IRWXG|S_IRWXO);
    assert(-1 != nf, "creat file failed!\n");

    printf("root created a file!\n");
    wait_any_button();
    ret = unlink("fileNotRoot");
    assert(0 == ret, "unlink error!\n");

    test10_printid();
    printf("try regain root privileges...\n");
    //ret = setregid(0,0);
    ret = setresgid(0, 0, -1);
    assert(0 == ret, "setresgid error!\n");
    //ret = setreuid(0,0);
    ret = setresuid(0, 0, -1);
    assert(0 == ret, "setresuid error!\n");

    test10_printid();
}
/*
    45/341
    access
    alarm pause
*/
void test11(int argc, char* argv[])
{
    if(0 == access("file", F_OK))
    {
        printf("file is existd.\n");
    }
    if(0 == access("file", R_OK))
    {
        printf("file can be read.\n");
    }
    printf("pause.wait 5s or other sigaln.\n");
    alarm(5);
    fflush(stdout);
    pause();
    printf("THIS TEXT WILL NOT SHOW!\n");
}
/*
    47/341
    tee splice
*/
void test12(int argc, char* argv[])
{
    int fd = creat("test12_tmp.log", S_IRWXU|S_IRWXG|S_IRWXO);
    int len = 0;
    int slen = 0;
    do
    {
        len = tee(STDIN_FILENO, STDOUT_FILENO, 
            INT_MAX, SPLICE_F_NONBLOCK);
        if(len < 0)
        {
            if(errno == EAGAIN)
                continue;
            perror("tee");
            printf("example: date | ./a.out | cat\n");
            exit(EXIT_FAILURE);
        }
        else if(len == 0)
        {
            printf("over\n");
            break;
        }
        while(len > 0)
        {
            slen = splice(STDIN_FILENO, NULL, fd, NULL,
                            len, SPLICE_F_MOVE);
            if (slen < 0) {
                perror("splice");
                break;
            }
            len -= slen;
        }
    }while(1);
}
/*
    50/341
    dup dup2 dup3
*/
void test13(int argc, char* argv[])
{
    int ret = 0;
    int fd = open("file", O_RDONLY);
    assert(fd != -1, "open error\n" );

    int fd1 = dup(fd);
    assert(fd1 != -1, "dup error\n" );

    int fd2 = dup2(fd, 666);
    assert(fd2 != -1, "dup2 error\n" );

    int fd3 = dup3(fd, 888, O_CLOEXEC);
    assert(fd3 != -1, "dup3 error\n" );

    printf("file: %d %d %d %d\n",
        fd, fd1, fd2, fd3);
    
    close(STDIN_FILENO);
    int nin = dup2(fd, STDIN_FILENO);
    assert(nin == STDIN_FILENO, "dup2 STDIN error\n" );

    const char d[256] = {};
    scanf("%s", d);
    printf("%s\n", d);
}
int main(int argc, char* argv[])
{
    for(int i = 0; i < argc; i++)
    {
        printf("Arg%d: %s\n", i, argv[i]);
    }
    fflush(stdout);
    test13(argc, argv);
    return 0;
}