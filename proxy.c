/*--------------------------------------------------------------------*/
/* conference server */

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#define MAX_SIZE 1024

extern char * recvtext(int sd);
extern int sendtext(int sd, char *msg);
extern int hooktoserver(char *servhost, ushort servport);
extern int startserver();

/*--------------------------------------------------------------------*/

struct arg{
    int fd;
    int* clients;
};

struct mapping{
    char* key;
    char** content;
    int total;
    int chunkNum;
    int* chunkSize;
};

typedef struct mapping map;

pthread_mutex_t lock;
map* maps;
int mapInd;

int getHeaderLength(char* input){
    int i;
    for(i=0;i<MAX_SIZE;i++){
        if(input[i]=='\r' && input[i+1]=='\n' && input[i+2]=='\r' && input[i+3]=='\n' ){
            return i+4;
        }
    }
    return -1;
}

int getContentLength(char* input){
    char* tmp = malloc(MAX_SIZE);
    int i;
    for(i=0;input[i];i++) tmp[i] = input[i];
    tmp[i] = 0;
    char *p;
    strtok(tmp, "\n");
    int ret = -1;
    while((p = strtok(NULL, "\n"))){
        char b1[50],b2[50];
        sscanf(p,"%s %s",b1,b2);
        if(!strcmp(b1,"Content-Length:")){
            ret = atoi(b2);
            break;
        }
    }
    free(tmp);
    return ret;
}

int* proxy(int fd){
    int* ret = malloc(sizeof(int)*2);
    ret[0] = 0;
    char* inBuffer = malloc(MAX_SIZE);	//malloc
    int byte_count = (int)recv(fd,inBuffer,MAX_SIZE-1,0);
    inBuffer[byte_count] = 0;
    //	printf("recv()'d %d bytes of data in buf\n",byte_count);
    //	printf("%s\n",inBuffer);
    char b1[5],b2[500];
    sscanf(inBuffer,"%s %s",b1,b2);
    char* sendRequest = malloc(MAX_SIZE);	//malloc
    int i = 0;
    int ind = 0;
    for(i=0;b1[i];i++) sendRequest[ind++] = b1[i];
    sendRequest[ind++] = ' ';
    char hostStr[500];
    char portStr[6];
    int pind = 0;
    int hostStrLen = 0;
    int hasPort = 0;
    
    for(i=7;b2[i]!='/';i++){
        if(b2[i]==':') {
            hasPort = 1;
            hostStr[i-7] = 0;
        }else{
            if(!hasPort) {
                hostStr[i-7] = b2[i];
                hostStrLen++;
            }else portStr[pind++] = b2[i];
        }
    }
    int j;
    for(j=0;j<mapInd;j++){
        if(!strcmp(maps[j].key,b2)){
            ret[0] = 1;
            ret[1] = j;
            int k;
//            printf("%d\n",maps[j].chunkNum);
            for(k=0;k<maps[j].chunkNum;k++){
                int csize = maps[j].chunkSize[k];
                //                printf("%s\n",maps[j].content[k]);
                send(fd,maps[j].content[k],csize,0);
            }

            return ret;
        }
    }

    char* sn = malloc(hostStrLen+1);
    for(j=0;j<hostStrLen;j++) sn[j] = hostStr[j];
    sn[j] = 0;
    char* key = malloc(strlen(b2)+1);
    for(j=0;j<strlen(b2);j++) key[j] = b2[j];
    key[j] = 0;
    //
    portStr[pind] = 0;
    ushort port = 80;
    if(hasPort) port = (ushort)atoi(portStr);

    for(i+=1+strlen(b1);inBuffer[i];i++) sendRequest[ind++] = inBuffer[i];

    sendRequest[ind] = 0;

    //		printf("%s\n",sendRequest);
    //		printf("%s:%d\n",hostStr,port);
    int sockfd = hooktoserver(sn,port);
    free(sn);
    send(sockfd,sendRequest,strlen(sendRequest),0);
    
    free(inBuffer); 	//free InBuffer
    free(sendRequest);	//free sendRequest
    //
    char* outBuffer = malloc(MAX_SIZE);	//malloc
    int bc = (int)recv(sockfd,outBuffer,MAX_SIZE-1,0);
    outBuffer[bc] = 0;
    //    printf("recv()'d %d bytes of data in buf\n",bc);
    //	printf("%s\n",outBuffer);
    int len = getContentLength(outBuffer);

    int reclen = bc-getHeaderLength(outBuffer);

    char** entireInput = malloc(sizeof(char*)*len);	//malloc
    ret[1] = mapInd;
    maps[mapInd].key = key;

    maps[mapInd].content = entireInput;
    maps[mapInd].chunkNum = 1;
    maps[mapInd].chunkSize = malloc(sizeof(int)*len);

    maps[mapInd].chunkSize[0] = bc;

    maps[mapInd].total = len;
    entireInput[0] = outBuffer;

    send(fd,outBuffer,bc,0);

    while(reclen!=len){
        char* tmpBuffer = malloc(MAX_SIZE);
        bc = (int)recv(sockfd,tmpBuffer,MAX_SIZE-1,0);
        //        printf("recv()'d %d bytes of data in buf\n",bc);
        tmpBuffer[bc] = 0;
        reclen+=bc;
        int chunkInd = maps[mapInd].chunkNum;
        maps[mapInd].chunkSize[chunkInd] = bc;
        entireInput[chunkInd] = tmpBuffer;
        maps[mapInd].chunkNum++;
        send(fd,tmpBuffer,bc,0);
    }

//    printf("%d\n",maps[mapInd].chunkNum);
    mapInd++;
//    printf("%d\n",mapInd);
    return ret;
}

void handle_thread(void* argument){
    time_t c_start;
    time_t c_end;
    c_start = clock();
    
    /*
     FILL HERE
     wait using select() for
     messages from existing clients and
     connect requests from new clients
     */
    struct arg* tmp = (struct arg*)argument;
    int fd = tmp->fd;
    int i;
    
    pthread_mutex_lock(&lock);
    for(i=0;i<FD_SETSIZE;i++){
        if(tmp->clients[i]==-1){
            tmp->clients[i]=fd;
            break;
        }
    }
    pthread_mutex_unlock(&lock);
    
    /* look for messages from live clients */
    char * clienthost; /* host name of the client */
    ushort clientport; /* port number of the client */
    
    
    /*
     FILL HERE:
     figure out client's host name and port
     using getpeername() and gethostbyaddr()
     */
    
    struct sockaddr_in sa;
    unsigned int salen = sizeof(struct sockaddr_in);
    if(getpeername(fd, (struct sockaddr*)&sa, &salen)==-1){
        perror("getpeername");
        exit(1);
    }
    
    struct hostent* ret = gethostbyaddr((void*)&sa.sin_addr.s_addr, 4, AF_INET);
    
    clienthost = malloc(strlen(ret->h_name)+1);
    strcpy(clienthost,ret->h_name);
    clienthost[strlen(ret->h_name)] = 0;
    clientport = ntohs(sa.sin_port);
    
    
    char **addr_list = NULL;
    char addr_p[INET_ADDRSTRLEN];
    for(addr_list = ret->h_addr_list; *addr_list; addr_list++){
        inet_ntop(AF_INET, *(addr_list), addr_p, INET_ADDRSTRLEN);
        break;
    }

    /* read the message */
    int* r = proxy(fd);

    c_end = clock();
    
    printf("%s|%s|",addr_p,maps[r[1]].key);
    if(r[0]) printf("CACHE_HIT|");
    else printf("CACHE_MISS|");
    printf("%d|%d\n",maps[r[1]].total,(int)difftime(c_end,c_start));
    free(r);
    
//    printf("End of thread\n");
    pthread_exit(NULL);
}



/*--------------------------------------------------------------------*/
int fd_isset(int fd, fd_set *fsp) {
    return FD_ISSET(fd, fsp);
}
/* main routine */
int main(int argc, char *argv[]) {
    pthread_mutex_init(&lock, NULL);
    maps = malloc(sizeof(map)*1000);
    mapInd = 0;
    
    int servsock; /* server socket descriptor */
    
    /* check usage */
    if (argc != 1) {
        fprintf(stderr, "usage : %s\n", argv[0]);
        exit(1);
    }
    
    /* get ready to receive requests */
    servsock = startserver();
    if (servsock == -1) {
        perror("Error on starting server: ");
        exit(1);
    }
    
    /*
     FILL HERE:
     init the set of live clients
     */
    int client[FD_SETSIZE];
    int i;
    for (i=0;i<FD_SETSIZE;i++) client[i] = -1;
    
    /* receive requests and process them */
    while (1) {
        pthread_t thread_id;
        /*
         FILL HERE:
         accept a new connection request
         */
        struct sockaddr_in sa;
        unsigned int salen = sizeof(struct sockaddr);
        int csd = accept(servsock,(struct sockaddr*)&sa,&salen);
        
        /* if accept is fine? */
        if (csd != -1) {
            char * clienthost; /* host name of the client */
            ushort clientport; /* port number of the client */
            /*
             FILL HERE:
             figure out client's host name and port
             using gethostbyaddr() and without using getpeername().
             */
            
            struct hostent* ret = gethostbyaddr((const void*)&sa.sin_addr, sizeof(struct in_addr), AF_INET);
            if(ret==NULL){
                perror("gethostbyaddr");
                exit(1);
            }
            clienthost = ret->h_name;
            clientport = ntohs(sa.sin_port);
//            printf("admin: connect from '%s' at '%hu'\n", clienthost, clientport);
            
            
            
            struct arg argument;
            argument.fd = csd;
            argument.clients = client;
            
            if(pthread_create(&thread_id,NULL,(void *)(&handle_thread),(void *)(&argument)) == -1){
                fprintf(stderr,"pthread_create error!\n");
                break;
            }
        } else {
            perror("accept");
            exit(0);
        }
    }
    return 0;
}
/*--------------------------------------------------------------------*/

