/*--------------------------------------------------------------------*/
/* functions to connect clients and server */

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <errno.h>

#include <stdlib.h>

#include <unistd.h>

#define MAXNAMELEN 256
/*--------------------------------------------------------------------*/

/*----------------------------------------------------------------*/
/* prepare server to accept requests
 returns file descriptor of socket
 returns -1 on error
 */
int startserver() {
    int sd; /* socket descriptor */
    
    char * servhost; /* full name of this host */
    ushort servport; /* port assigned to this server */
    
    /*
     FILL HERE
     create a TCP socket using socket()
     */
    if((sd = socket(AF_INET, SOCK_STREAM, 0))==-1){
        perror("socket");
        exit(1);
    }
    
    /*
     FILL HERE
     bind the socket to some port using bind()
     let the system choose a port
     */
    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(0);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if(bind(sd, (struct sockaddr *)&server_addr, sizeof(server_addr))==-1){
        perror("bind");
        exit(1);
    }
    
    
    /* we are ready to receive connections */
    if(listen(sd, 5)==-1){
        perror("listen");
        exit(1);
    }
    
    /*
     FILL HERE
     figure out the full host name (servhost)
     use gethostname() and gethostbyname()
     full host name is remote**.cs.binghamton.edu
     */
    char tmpname[MAXNAMELEN+1];
    if(gethostname(tmpname,MAXNAMELEN)==-1){
        perror("gethostname");
        exit(1);
    }
    struct hostent *tmp = gethostbyname(tmpname);
    servhost = malloc(MAXNAMELEN+1);
    int i;
    for(i=0;tmp->h_name[i];i++) servhost[i] = tmp->h_name[i];
    servhost[i] = 0;
    
    /*
     FILL HERE
     figure out the port assigned to this server (servport)
     use getsockname()
     */
    struct sockaddr_in ret;
    unsigned int len = sizeof(struct sockaddr_in);
    if(getsockname(sd, (struct sockaddr*)&ret, &len)==-1){
        perror("getsockname");
        exit(0);
    }
    servport = ntohs(ret.sin_port);
    
    /* ready to accept requests */
    printf("admin: started server on '%s' at '%hu'\n", servhost, servport);
    free(servhost);
    return sd;
}
/*----------------------------------------------------------------*/

/*----------------------------------------------------------------*/
/*
 establishes connection with the server
 returns file descriptor of socket
 returns -1 on error
 */
int hooktoserver(char *servhost, ushort servport) {
    int sd; /* socket descriptor */
    
    ushort clientport; /* port assigned to this client */
    
    /*
     FILL HERE
     create a TCP socket using socket()
     */
    if((sd = socket(AF_INET, SOCK_STREAM, 0))==-1){
        perror("socket");
        exit(1);
    }

    /*
     FILL HERE
     connect to the server on 'servhost' at 'servport'
     use gethostbyname() and connect()
     */
    struct hostent *h = gethostbyname(servhost);
    struct sockaddr_in my_addr;
	//

    memcpy(&my_addr.sin_addr.s_addr, h->h_addr, h->h_length);//

    my_addr.sin_port = htons(servport);
    my_addr.sin_family = AF_INET;
    if(my_addr.sin_family!=AF_INET){
        printf("not equal\n");
        printf("%d,expected: %d",my_addr.sin_family,AF_INET);
    }

    char **addr_list = NULL;
    char addr_p[INET_ADDRSTRLEN];
    for(addr_list = h->h_addr_list; *addr_list; addr_list++)
    {
        inet_ntop(AF_INET, *(addr_list), addr_p, INET_ADDRSTRLEN);
//        printf("server address: %s\n", addr_p);
    }
    
    if(connect(sd, (struct sockaddr*)&my_addr, sizeof(my_addr))==-1){
        perror("connect");
        exit(1);
    }
    
    /*
     FILL HERE
     figure out the port assigned to this client
     use getsockname()
     */
    struct sockaddr_in ret;
    unsigned int len = sizeof(struct sockaddr_in);
    if(getsockname(sd, (struct sockaddr*)&ret, &len)==-1){
        perror("getsockname");
        exit(0);
    }
    clientport = ntohs(ret.sin_port);
    
    /* succesful. return socket descriptor */
//    printf("admin: connected to server on '%s' at '%hu' thru '%hu'\n", servhost, servport, clientport);
    return sd;
}
/*----------------------------------------------------------------*/

/*----------------------------------------------------------------*/
int readn(int sd, char *buf, int n) {
    int toberead;
    char * ptr;
    
    toberead = n;
    ptr = buf;
    while (toberead > 0) {
        int byteread;
        
        byteread = read(sd, ptr, toberead);
        if (byteread <= 0) {
            if (byteread == -1)
                perror("read");
            return (0);
        }
        
        toberead -= byteread;
        ptr += byteread;
    }
    return (1);
}

char *recvtext(int sd) {
    char *msg;
    long len;
    /* read the message length */
    if (!readn(sd, (char *) &len, sizeof(len))) {
        return (NULL);
    }
    len = ntohl(len);
    /* allocate space for message text */
    msg = NULL;
    if (len > 0) {
        msg = (char *) malloc(len);
        if (!msg) {
            fprintf(stderr, "error : unable to malloc\n");
            return (NULL);
        }
        
        /* read the message text */
        if (!readn(sd, msg, len)) {
            free(msg);
            return (NULL);
        }
    }
    
    /* done reading */
    return (msg);
}

int sendtext(int sd, char *msg) {
    long len;
    
    /* write lent */
    len = (msg ? strlen(msg) + 1 : 0);
    len = htonl(len);
    write(sd, (char *) &len, sizeof(len));
    
    /* write message text */
    len = ntohl(len);
    if (len > 0)
        write(sd, msg, len);
    return (1);
}
/*----------------------------------------------------------------*/

