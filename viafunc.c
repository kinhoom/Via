/* Via functions file 
 * viafunc.c
 * Author:alex kinhoom
 */
#include "viafunc.h"

//Unix error
void unix_error(char *msg) {
    fprintf(stderr,"\033[31m%s:\033[0m %s\n",  msg, strerror(errno));
    exit(0);
}

//Gai error
void gai_error(int code, char *msg) 
{
    fprintf(stderr, "\033[31m%s:\033[0m %s\n", msg, gai_strerror(code));
    exit(0);
}
void Getaddrinfo(const char *node, const char *service,const struct addrinfo *hints, struct addrinfo **res) {
    int rc;
    if((rc = getaddrinfo(node, service, hints, res)) != 0) {
	gai_error(rc, "Getaddrinfo error");
    }
}

//Set socket option 
void Setsockopt(int s, int level, int optname, const void *optval, int optlen) { 
    int rc;
    if ((rc = setsockopt(s, level, optname, optval, optlen)) < 0) {
	unix_error("Setsockopt error");
    }
    return rc;
}
//Free mem
void Freeaddrinfo(struct addrinfo *res)
{
    freeaddrinfo(res);
}
//Accept the connection of client
int Accept(int s, struct sockaddr *addr, socklen_t *addrlen) 
{
    int rc;

    if ((rc = accept(s, addr, addrlen)) < 0) {
	unix_error("Accept error");
    }
    return rc;
}
//Get the nameinfo of the client
void Getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host, size_t hostlen, char *serv, size_t servlen, int flags) {
    int rc;
    if ((rc = getnameinfo(sa, salen, host, hostlen, serv, servlen, flags)) != 0) {
        gai_error(rc, "Getnameinfo error");
    }
}
//Close fd
void Close(int fd) {
    int rc;
    if ((rc = close(fd)) < 0) {
	unix_error("Close error");
    }
}
//Init robust io
void rio_readinitb(rio_t *rp, int fd) {
    rp->rio_fd = fd;  
    rp->rio_cnt = 0;
    rp->rio_bufptr = rp->rio_buf;
}
void Rio_readinitb(rio_t *rp, int fd) {
    rio_readinitb(rp, fd);
} 

/* $begin rio_read */
static ssize_t rio_read(rio_t *rp, char *usrbuf, size_t n)
{
    int cnt;

    while (rp->rio_cnt <= 0) {  /* Refill if buf is empty */
	rp->rio_cnt = read(rp->rio_fd, rp->rio_buf, sizeof(rp->rio_buf));
	if (rp->rio_cnt < 0) {
	    if (errno != EINTR) /* Interrupted by sig handler return */
		return -1;
	}
	else if (rp->rio_cnt == 0)  /* EOF */
	    return 0;
	else 
	    rp->rio_bufptr = rp->rio_buf; /* Reset buffer ptr */
    }

    /* Copy min(n, rp->rio_cnt) bytes from internal buf to user buf */
    cnt = n;          
    if (rp->rio_cnt < n)   
	cnt = rp->rio_cnt;
    memcpy(usrbuf, rp->rio_bufptr, cnt);
    rp->rio_bufptr += cnt;
    rp->rio_cnt -= cnt;
    return cnt;
}
/* 
 *   rio_readlineb - Robustly read a text line (buffered)
 *   
 */
/* $begin rio_readlineb */
ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen) 
{
    int n, rc;
    char c, *bufp = usrbuf;

    for (n = 1; n < maxlen; n++) { 
        if ((rc = rio_read(rp, &c, 1)) == 1) {
	    *bufp++ = c;
	    if (c == '\n') {
                n++;
     		break;
            }
	} else if (rc == 0) {
	    if (n == 1) {
		return 0; /* EOF, no data read */
            }
	    else {
		break;    /* EOF, some data was read */
            }
	} else
	    return -1;	  /* Error */
    }
    *bufp = '\0';
    return n-1;
}
ssize_t Rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen) 
{
    ssize_t rc;

    if ((rc = rio_readlineb(rp, usrbuf, maxlen)) < 0) {
	unix_error("Rio_readlineb error");
    }
    return rc;
} 

/*
 *  rio_writen - Robustly write n bytes (unbuffered)
 *   
 */
ssize_t rio_writen(int fd, void *usrbuf, size_t n) 
{
    size_t nleft = n;
    ssize_t nwritten;
    char *bufp = usrbuf;

    while (nleft > 0) {
	if ((nwritten = write(fd, bufp, nleft)) <= 0) {
	    if (errno == EINTR)  /* Interrupted by sig handler return */
		nwritten = 0;    /* and call write() again */
	    else
		return -1;       /* errno set by write() */
	}
	nleft -= nwritten;
	bufp += nwritten;
    }
    return n;
}
//Robustly write
void Rio_writen(int fd, void *usrbuf, size_t n) 
{
    if (rio_writen(fd, usrbuf, n) != n) {
	unix_error("Rio_writen error");
    }
}
//Open listening fd
int open_listenfd(char *port) {
    struct addrinfo hints, *listp, *p;
    int listenfd, optval=1;
    /* Get a list of potential server addresses */
    //Fills the specified mem block with 0 (init mem block)
    memset(&hints,0,sizeof(struct addrinfo));
    //Accept TCP connections(only tcp conn)
    hints.ai_socktype = SOCK_STREAM;
    //Any IP address
    hints.ai_flags = AI_PASSIVE;
    //Using a numeric port arg.
    hints.ai_flags |= AI_NUMERICSERV;
    //Recommended for connections
    hints.ai_flags |= AI_ADDRCONFIG;
    //Get the ptr that points to the linked list with  hints
    Getaddrinfo(NULL, port, &hints, &listp);
    /* Travel the linked list for one that we can successfully bind to */
    for(p = listp; p; p = p->ai_next) {
	/* Create the socket descriptor */
	if((listenfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) {
	    continue; //create socket fail,try the next one
	}
	//Set socket option to avoid "Address already in use" error from bind
	Setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,(const void *)&optval , sizeof(int));
	//Bind the descriptor to the address
	if(bind(listenfd, p->ai_addr, p->ai_addrlen) == 0) {
	    break; //success
	}
	//Bind failed ,try next one
	Close(listenfd);
    }
    //Recover mem
    Freeaddrinfo(listp); 
    //No address used
    if(!p) {
	return -1;
    }
    /* Make it a listening socket ready to accept connection requests */
    if(listen(listenfd, LISTENQ) < 0) {
	return -1;
    }
    return listenfd;
}
//Wrapper for open_listenfd
int Open_listenfd(char *port) {
    int rc;
    if((rc = open_listenfd(port)) < 0) {
	unix_error("Open_listenfd error");
    }
    return rc;  
} 

//Wrapper for unix open
int Open(const char *pathname, int flags, mode_t mode) 
{
    int rc;

    if ((rc = open(pathname, flags, mode))  < 0) {
	unix_error("Open error");
    }
    return rc;
}

//Wrapper for memory mapping functions
void *Mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset) 
{
    void *ptr;

    if ((ptr = mmap(addr, len, prot, flags, fd, offset)) == ((void *) -1))
	unix_error("mmap error");
    return(ptr);
}

//Wrapper for munmap
void Munmap(void *start, size_t length) 
{
    if (munmap(start, length) < 0) {
	unix_error("munmap error");
    }
}

//Wrapper for fork
pid_t Fork(void) 
{
    pid_t pid;

    if ((pid = fork()) < 0) {
	unix_error("Fork error");
    }
    return pid;
}


//Redirect stdout to client
int Dup2(int fd1, int fd2) 
{
    int rc;

    if ((rc = dup2(fd1, fd2)) < 0)
	unix_error("Dup2 error");
    return rc;
}
//Wrapper for execve
void Execve(const char *filename, char *const argv[], char *const envp[]) 
{
    if (execve(filename, argv, envp) < 0)
	unix_error("Execve error");
}
//Wrapper for wait
pid_t Wait(int *status) 
{
    pid_t pid;

    if ((pid  = wait(status)) < 0)
	unix_error("Wait error");
    return pid;
}
