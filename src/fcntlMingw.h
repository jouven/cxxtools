#ifndef FCNTLMINGW_H_
#define FCNTLMINGW_H_

#define F_GETFD 1
#define F_SETFD 2
#define F_GETFL 3
#define F_SETFL 4
#define O_NONBLOCK FIONBIO
#define FD_CLOEXEC 0x1

inline int fcntl(int fd, int cmd, ...)
{
    if (cmd == F_GETFD || cmd == F_SETFD)
    {
        return 0;
    }
    else
    {
        return -1;
    }
}

#endif
