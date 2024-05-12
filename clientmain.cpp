#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <locale>
#include <sys/time.h>
#include <sys/shm.h>

#define SA struct sockaddr
#include "protocol.h"
#define NOCL 100

int countColons(const char *str)
{
    int count = 0;
    while (*str)
    { 
        if (*str == ':')
        {
            count++; 
        }
        str++; 
    }
    return count;
}
bool containsDotAndAlpha(const char *str)
{
    bool hasDot = false;
    bool hasAlpha = false;

    while (*str)
    {
        if (*str == '.')
        {
            hasDot = true;
        }
        else if (std::isalpha(*str))
        {
            hasAlpha = true;
        }
        ++str;
    }

    return hasDot && hasAlpha;
}
bool doesNotContainDot(const char *str)
{
    while (*str)
    {
        if (*str == '.')
        {
            return false; // Dot found, so the string does contain a dot
        }
        ++str;
    }
    return true; // Dot not found, so the string does not contain a dot
}
char *nslookup(char *&hostname)
{
    int retval;
    struct addrinfo *result = NULL;

    struct addrinfo hints;
    bzero(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = 0;
    retval = getaddrinfo(hostname, NULL, &hints, &result);
    if (retval != 0)
    {
        return NULL;
    }

    struct sockaddr_in *sockaddr_ipv4;
    //struct sockaddr_in6 *sockaddr_ipv6;
    //char ipstringbuffer[46];
    for (struct addrinfo *ptr = result; ptr != NULL; ptr = ptr->ai_next)
    {
        switch (ptr->ai_family)
        {
        case AF_INET:
            sockaddr_ipv4 = (struct sockaddr_in *)ptr->ai_addr;
            return (char *)inet_ntoa(sockaddr_ipv4->sin_addr);
            break;
        case AF_INET6:
            // sockaddr_ipv6 = (struct sockaddr_in6 *)ptr->ai_addr;
            break;
        default:
            break;
        }
    }

    if (result != NULL)
    {
        freeaddrinfo(result);
    }
}
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int CAP = 2000;
int main(int argc, char *argv[])
{

    int sockfd[NOCL]; // Max 100 clients...
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;
    //int numread;
    char buffer[1450];
    int DEBUGv;
    int timeout_in_seconds = 1;

    DEBUGv = 0;

    struct timeval ct1, ct2;

    //char **tokens;
    FILE *fptr=NULL;

    if (argc < 4 || argc > 5)
    {

        fprintf(stderr, "usage: %s <HOSTNAME:PORT> <CLIENTs> <prob> <resultfile> [debug] \n", argv[0]);
        if (fptr != NULL)
            fprintf(fptr, "ERROR OCCURED");
        exit(1);
    }
    int delimCounter = countColons(argv[1]);
    char *start = argv[1]; // 根据 ： 进行分割
    char *end = strrchr(argv[1], ':');
    int partLen = end - start;
    // 判断是ipv6还是v4
    char *Desthost = (char *)malloc((partLen + 1) * sizeof(char));
    strncpy(Desthost, start, partLen);
    char *Destport = NULL;
    Destport = ++end;
    int port;                           
    //int serverfd;                       
    struct sockaddr_in clientAddress;   
    struct sockaddr_in6 clientAddress6; 
    memset(&clientAddress, 0, sizeof(clientAddress));
    memset(&clientAddress6, 0, sizeof(clientAddress6));
    socklen_t len;
    //char server_message[CAP]; 
    port = atoi(Destport);    // 将字符串转换成int

    int noClients = atoi(argv[2]);
    int prob = atoi(argv[3]);

    if (noClients >= NOCL)
    {
        printf("Too many clients..Max is %d.\n", NOCL);
        printf("If you want more, change NOCL and recompile.\n");
        exit(1);
    }

    printf("Probability = %d \n", prob);

    if (argc == 6)
    {
        printf("DEBUG ON\n");
        DEBUGv = 1;
    }
    else
    {
        printf("DEBUG OFF\n");
        DEBUGv = 0;
    }
    socklen_t addr_len;
    struct sockaddr_storage their_addr;
    addr_len = sizeof(their_addr);

    printf("Connecting %d clients %s on port=%s \n", noClients, Desthost, Destport);
    printf("Saving to %s \n", argv[4]);
    fptr = fopen(argv[4], "w+");
    if (fptr == NULL)
    {
        printf("Cant write to %s, %s.\n", argv[4], strerror(errno));
    }
    // 以上是变量范围
    //------------------------------------------------

    memset(&hints, 0, sizeof hints);
    memset(&buffer, 0, sizeof(buffer));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    // 清空用于连接的结构和用于接受的缓冲区
    //----------------------------------------------

    if ((rv = getaddrinfo(Desthost, Destport, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    else
        printf("Getaddrinfo success\n");
    int ip_version = delimCounter == 1 ? 4 : 6;
    p = servinfo;

    if (ip_version == 4)
    {
        if (containsDotAndAlpha(Desthost))
        {
            Desthost = nslookup(Desthost);
        }
        else if (doesNotContainDot(Desthost))
        {
            Desthost = nslookup(Desthost);
        }
        clientAddress.sin_family = AF_INET;
    }
    else
    {
        clientAddress6.sin6_family = AF_INET6;
    }

    // 测试连接
    //------------------------------------
    if (ip_version == 4)
    {
        memset(&clientAddress, 0, sizeof(clientAddress));
        clientAddress.sin_family = AF_INET;
        clientAddress.sin_port = htons(port);                // 没用上
        clientAddress.sin_addr.s_addr = inet_addr(Desthost); // 没用上
        len = sizeof(clientAddress);
        for (int i = 0; i < noClients; i++)
        {
            if ((sockfd[i] = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
            {
                perror("socket");
            }
        }
    }
    else if (ip_version == 6)
    {
        memset(&clientAddress6, 0, sizeof(clientAddress6));
        clientAddress6.sin6_family = AF_INET6;
        inet_pton(AF_INET6, Desthost, &clientAddress6.sin6_addr);
        clientAddress6.sin6_port = htons(port);
        len = sizeof(clientAddress);
        for (int i = 0; i < noClients; i++)
        {
            if ((sockfd[i] = socket(AF_INET6, SOCK_DGRAM, 0)) == -1)
            {
                perror("socket");
            }
        }
    }
    printf("IPv%d, Host %s, and port %d.\n", ip_version, Desthost, port);

    // 逐个创建套接字，创建多个套接字准备用于连接
    //------------------------------------

    int s;
    struct sockaddr_in sa;
    struct sockaddr_in6 sa6;
    socklen_t sa_len = sizeof(sa);
    socklen_t sa_len6 = sizeof(sa6);

    char localIP[100];
    const char *myAdd;
    memset(&localIP, 0, sizeof(localIP));
    int bobsMother;
    if (ip_version == 4)
    {
        bobsMother = socket(AF_INET, SOCK_DGRAM, 0);
    }
    else
    {
        bobsMother = socket(AF_INET6, SOCK_DGRAM, 0);
    }

    if (bobsMother == -1)
    {
        perror("Socket cant do nr2");
    }
    else
    {
        rv = connect(bobsMother, p->ai_addr, p->ai_addrlen);
        if (rv == -1)
        {
            perror("Cant connect to socket..");
        }
        else
        {
            if (ip_version == 4)
            {
                if ((s = getsockname(bobsMother, (struct sockaddr *)&sa, &sa_len) == -1))
                {
                    perror("getsockname failed.");
                }
                else
                {
                    myAdd = inet_ntop(sa.sin_family, &sa.sin_addr, localIP, sizeof(localIP));
                    printf("BobsMother (%s:%d) \n", localIP, ntohs(sa.sin_port));
                }
            }
            else
            {
                if ((s = getsockname(bobsMother, (struct sockaddr *)&sa6, &sa_len6) == -1))
                {
                    perror("getsockname failed.");
                }
                else
                {
                    myAdd = inet_ntop(sa6.sin6_family, &sa6.sin6_addr, localIP, sizeof(localIP));
                    printf("BobsMother (%s:%d) \n", localIP, ntohs(sa6.sin6_port));
                }
            }
        }
    }

    close(bobsMother);

    // 测试连接 使用了connect
    //------------------------------------

    typedef struct calcMessage cMessage;
    cMessage CM;
    CM.type = htons(22);
    CM.message = htons(0);
    CM.major_version = htons(1);
    CM.minor_version = htons(0);
    CM.protocol = htons(17);

    typedef struct calcProtocol cProtocol;
    //cProtocol *ptrCM;
    cProtocol CP[NOCL];
    cMessage CMs[NOCL];
    int droppedClient[NOCL];

    // 创建两个用于交互的信息结构体
    //---------------------------------------------

    for (int i = 0; i < NOCL; i++)
    {
        droppedClient[i] = 0;
    }
    //int myRand;
    int dropped = 0;

    int OKresults = 0;
    int ERRORresults = 0;
    int ERRORsignup = 0;

    printf("Sending Requests.\n");
    printf(" CM size = %lu \n", sizeof(struct calcMessage)); // 使用 %lu 来打印无符号长整型值
    printf("uint16_t = %lu \n", sizeof(uint16_t));           // 同样使用 %lu
    printf("uint32_t = %lu \n", sizeof(uint32_t));           // 同样使用 %lu

    // 共享内存
    size_t size = sizeof(int) * 10; // 10个int的大小

    // 1. 创建共享内存段
    int shmid = shmget(IPC_PRIVATE, size, IPC_CREAT | 0666);
    if (shmid == -1)
    {
        perror("shmget");
        return 1;
    }

    // 2. 将共享内存段附加到进程地址空间
    int *shared_memory = (int *)shmat(shmid, NULL, 0);
    if (shared_memory == (void *)-1)
    {
        perror("shmat");
        return 1;
    }

    // 初始化共享内存中的变量
    for (int i = 0; i < 10; i++)
    {
        shared_memory[i] = 0; // 这里只是示例，可以根据需要进行初始化
    }

    // ——————————————————————————————————

    struct timeval tv;

    tv.tv_sec = 2;
    tv.tv_usec = 0;

    for (int i = 0; i < noClients; i++)
    {

        pid_t pid = fork();

        if (pid == 0)
        {
            if (droppedClient[i] == -1)
            {
                pthread_mutex_lock(&mutex);
                shared_memory[0] = shared_memory[0] + 1;
                shared_memory[1] = shared_memory[1] + 1;
                shared_memory[2] = shared_memory[2] + 1;
                shared_memory[3] = shared_memory[3] + 1;
                pthread_mutex_unlock(&mutex);
                exit(0);
            }
            // 信号量处理
            //----------------------------step 1-------------------------------
            dropped = 0;
            setsockopt(sockfd[i], SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv);
            while (dropped != 3)
            {
                if (ip_version == 4)
                {
                    if ((numbytes = sendto(sockfd[i], &CM, sizeof(CM), 0, (struct sockaddr *)&clientAddress, sizeof(clientAddress))) == -1)
                    {
                        perror("talker: sendto");
                        exit(1);
                    }
                    else
                    {
                        if ((s = getsockname(sockfd[i], (struct sockaddr *)&sa, &sa_len) == -1))
                        {
                            perror("getsockname failed.");
                        }
                        else
                        {
                            // 为什么这里的port不是唯一的
                            if (dropped == 0)
                            {
                                printf("Client[%d] (%s:%d) registered, sent %d bytes\n", i, localIP, ntohs(sa.sin_port), numbytes);
                            }
                            printf("Client[%d] Try %d times to send calcMessage\n", i, dropped + 1);
                        }
                    }
                    if ((numbytes = recvfrom(sockfd[i], buffer, sizeof(buffer), 0, (struct sockaddr *)&clientAddress, &len)) == -1)
                    {
                        dropped++;
                        if (dropped == 3)
                        {
                            perror("recvfrom");
                            //                            if (errno == ETIMEDOUT ) {
                            //                                printf("Client %d timedout.\n", i);
                            //                            }
                            printf("Client %d timedout.\n", i);
                            droppedClient[i] = -1;
                            pthread_mutex_lock(&mutex);
                            shared_memory[0] = shared_memory[0] + 1;
                            shared_memory[1] = shared_memory[1] + 1;
                            shared_memory[2] = shared_memory[2] + 1;
                            shared_memory[3] = shared_memory[3] + 1;
                            pthread_mutex_unlock(&mutex);
                            exit(1);
                            break;
                        }
                    }
                    else
                    {
                        printf("Client[%d] ", i);
                        break;
                    }
                }
                else
                {
                    if ((numbytes = sendto(sockfd[i], &CM, sizeof(CM), 0, (struct sockaddr *)&clientAddress6, sizeof(clientAddress6))) == -1)
                    {
                        perror("talker6: sendto");
                        exit(1);
                    }
                    else
                    {
                        if ((s = getsockname(sockfd[i], (struct sockaddr *)&sa6, &sa_len6) == -1))
                        {
                            perror("getsockname failed.");
                        }
                        else
                        {
        
                            if (dropped == 0)
                            {
                                printf("Client[%d] (%s:%d) registered, sent %d bytes\n", i, localIP, ntohs(sa6.sin6_port), numbytes);
                            }
                            printf("Client[%d] Try %d times to send calcMessage\n", i, dropped + 1);
                        }
                    }
                    if ((numbytes = recvfrom(sockfd[i], buffer, sizeof(buffer), 0, (struct sockaddr *)&clientAddress6, &len)) == -1)
                    {
                        dropped++;
                        if (dropped == 3)
                        {
                            perror("recvfrom");
            
                            printf("Client %d timedout.\n", i);
                            droppedClient[i] = -1;
                            pthread_mutex_lock(&mutex);
                            shared_memory[0] = shared_memory[0] + 1;
                            shared_memory[1] = shared_memory[1] + 1;
                            shared_memory[2] = shared_memory[2] + 1;
                            shared_memory[3] = shared_memory[3] + 1;
                            pthread_mutex_unlock(&mutex);
                            exit(1);
                            break;
                        }
                    }
                    else
                    {
                        printf("Client[%d] ", i);
                        break;
                    }
                }
            }
            pthread_mutex_lock(&mutex);
            shared_memory[0] = shared_memory[0] + 1;
            pthread_mutex_unlock(&mutex);
            while (shared_memory[1] != 1)
            {
            }

            if (numbytes == sizeof(cProtocol))
            {
                /*拷贝一份*/
                memcpy(&CP[i], buffer, sizeof(cProtocol));
                /* 输出一些里面的协议参数 */
                printf("| calcProtocol type=%d version=%d.%d id=%d arith=%d ", ntohs(CP[i].type), ntohs(CP[i].major_version), ntohs(CP[i].minor_version), ntohl(CP[i].id), ntohl(CP[i].arith));
                // 开始判断是什么算数
                switch (ntohl(CP[i].arith))
                {
                case 1:
                    printf(" add \n");
                    break;
                case 2:
                    printf(" sub \n");
                    break;
                case 3:
                    printf(" mul \n");
                    break;
                case 4:
                    printf(" div \n");
                    break;
                case 5:
                    printf(" fadd \n");
                    break;
                case 6:
                    printf(" fsub \n");
                    break;
                case 7:
                    printf(" fmul \n");
                    break;
                case 8:
                    printf(" fdiv \n");
                    break;
                }

                printf("\t  | inVal1=%d inVal2=%d inRes=%d inFloat1=%g inFloat2=%g flValue=%g \n", ntohl(CP[i].inValue1), ntohl(CP[i].inValue2), ntohl(CP[i].inResult), CP[i].flValue1, CP[i].flValue2, CP[i].flResult);
            }
            else
            {
                // 如果不是相同的结构体大小则报错
                printf("\t  | ODD SIZE MESSAGE. Got %d bytes, expected %lu bytes (sizeof(cProtocol)) . \n", numbytes, sizeof(cProtocol));
                droppedClient[i] = -1; // Signal that this client is busted.
                ERRORresults++;
                ERRORsignup++;
            }

            // 信号量处理
            // 等待4秒
            // 下面开始计算
            dropped = 0;

            // 以下只是计算
            switch (ntohl(CP[i].arith))
            {
            case 1: /*add */
                CP[i].inResult = htonl(ntohl(CP[i].inValue1) + ntohl(CP[i].inValue2));
                printf("[%d] %d + %d => %d ", i, ntohl(CP[i].inValue1), ntohl(CP[i].inValue2), ntohl(CP[i].inResult));
                break;
            case 2: /*sub */
                CP[i].inResult = htonl(ntohl(CP[i].inValue1) - ntohl(CP[i].inValue2));
                printf("[%d] %d - %d => %d ", i, ntohl(CP[i].inValue1), ntohl(CP[i].inValue2), ntohl(CP[i].inResult));
                break;
            case 3: /*mul */
                CP[i].inResult = htonl(ntohl(CP[i].inValue1) * ntohl(CP[i].inValue2));
                printf("[%d] %d * %d => %d ", i, ntohl(CP[i].inValue1), ntohl(CP[i].inValue2), ntohl(CP[i].inResult));
                break;
            case 4: /*div */
                CP[i].inResult = htonl(ntohl(CP[i].inValue1) / ntohl(CP[i].inValue2));
                printf("[%d] %d / %d => %d ", i, ntohl(CP[i].inValue1), ntohl(CP[i].inValue2), ntohl(CP[i].inResult));
                break;
            case 5: /*fadd */
                CP[i].flResult = CP[i].flValue1 + CP[i].flValue2;
                printf("[%d] %g + %g => %g ", i, CP[i].flValue1, CP[i].flValue2, CP[i].flResult);
                break;
            case 6: /*fsub */
                CP[i].flResult = CP[i].flValue1 - CP[i].flValue2;
                printf("[%d] %g - %g => %g ", i, CP[i].flValue1, CP[i].flValue2, CP[i].flResult);
                break;
            case 7: /*fmul */
                CP[i].flResult = CP[i].flValue1 * CP[i].flValue2;
                printf("[%d] %g * %g => %g ", i, CP[i].flValue1, CP[i].flValue2, CP[i].flResult);
                break;
            case 8: /*fdiv */
                CP[i].flResult = CP[i].flValue1 / CP[i].flValue2;
                printf("[%d] %g / %g => %g ", i, CP[i].flValue1, CP[i].flValue2, CP[i].flResult);
                break;
            default:
                printf(" ** SHIT unkown arithm. %d ** \n", ntohl(CP[i].arith));
                ERRORresults++;
                break;
            }

            CP[i].type = htons(2);
            if (droppedClient[i] == -1)
            {
                pthread_mutex_lock(&mutex);
                shared_memory[2] = shared_memory[2] + 1;
                shared_memory[3] = shared_memory[3] + 1;
                pthread_mutex_unlock(&mutex);
                exit(1);
            }
            while (dropped != 3)
            {
                if (ip_version == 4)
                {
                    if ((numbytes = sendto(sockfd[i], &CP[i], sizeof(cProtocol), 0, (struct sockaddr *)&clientAddress, sizeof(clientAddress))) == -1)
                    {
                        perror("talker: sendto");
                    }
                    else
                    {
                        if ((s = getsockname(sockfd[i], (struct sockaddr *)&sa, &sa_len) == -1))
                        {
                            perror("getsockname failed.");
                        }
                        else
                        {
                            // 为什么这里的port不是唯一的
                            if (dropped == 0)
                            {
                                printf(" (%s:%d) sent %d bytes\n", localIP, ntohs(sa.sin_port), numbytes);
                            }
                            printf("Client[%d] Try %d times to send calcMessage\n", i, dropped + 1);
                        }
                    }
                }
                else
                {
                    if ((numbytes = sendto(sockfd[i], &CP[i], sizeof(cProtocol), 0, (struct sockaddr *)&clientAddress6, sizeof(clientAddress6))) == -1)
                    {
                        perror("talker6: sendto");
                        // exit(1);
                    }
                    else
                    {
                        if ((s = getsockname(sockfd[i], (struct sockaddr *)&sa6, &sa_len6) == -1))
                        {
                            perror("getsockname failed.");
                        }
                        else
                        {
                            // 为什么这里的port不是唯一的
                            if (dropped == 0)
                            {
                                printf(" (%s:%d) sent %d bytes\n", localIP, ntohs(sa.sin_port), numbytes);
                            }
                            printf("Client[%d] Try %d times to send calcMessage\n", i, dropped + 1);
                        }
                    }
                }
                pthread_mutex_lock(&mutex);
                shared_memory[2] = shared_memory[2] + 1;
                pthread_mutex_unlock(&mutex);
                while (shared_memory[2] != noClients)
                {
                }

                // 设置超时
                setsockopt(sockfd[i], SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv);
                // 等待接收信息
                if ((numbytes = recvfrom(sockfd[i], buffer, sizeof(buffer), 0, (struct sockaddr *)&their_addr, &addr_len)) == -1)
                {
                    dropped++;
                    if (dropped == 3)
                    {
                        printf("Client %d (id = %d ) : (%d) %s \n", i, ntohl(CP[i].id), errno, strerror(errno));
                        //                        if (errno == ETIMEDOUT ) {
                        //                            printf("Client %d timedout.\n", i);
                        //                        }
                        printf("Client %d timedout.\n", i);
                        droppedClient[i] = -1;
                        pthread_mutex_lock(&mutex);
                        shared_memory[3] = shared_memory[3] + 1;
                        pthread_mutex_unlock(&mutex);
                        exit(0);
                        break;
                    }
                }
                else
                {
                    printf("Client[%d] | ", i);
                    if (ip_version == 4)
                    {
                        printf("(port=%d) ", ntohs(sa.sin_port));
                    }
                    else
                    {
                        printf("(port=%d) ", ntohs(sa6.sin6_port));
                    }
                    printf(" Got %d bytes ", numbytes);
                    break;
                }
            }

            // 接收到了消息
            /* read info */
            /* Copy to internal structure */
            memcpy(&CMs[i], buffer, sizeof(cMessage));
            // 输出一下信息类型
            switch (ntohs(CMs[i].type))
            {
            case 1:
                printf("S->C [ascii] ");
                break;
            case 2:
                printf("S->C [binary] ");
                break;
            case 3:
                printf("S->C [N/A] ");
                break;
            case 4:
                printf("C->S [ascii] ");
                break;
            case 5:
                printf("C->S [binary] ");
                break;
            case 6:
                printf("C->S [N/A] ");
                break;
            default:
                printf(" unknown type=%d ", ntohs(CMs[i].type));
                break;
            }
            // 输出版本号
            printf("version=%d.%d ", ntohs(CMs[i].major_version), ntohs(CMs[i].minor_version));
            /*
                 0 = Not applicable/availible (N/A or NA)
                 1 = OK   // Accept
                 2 = NOT OK  // Reject
             * */
            switch (ntohl(CMs[i].message))
            {
            case 0:
                printf(" N/A ");
                ERRORresults++;
                shared_memory[9] = shared_memory[9] + 1;
                break;
            case 1:
                printf(" OK ");
                OKresults++;
                shared_memory[8] = shared_memory[8] + 1;
                break;
            case 2:
                printf(" Not OK ");
                ERRORresults++;
                shared_memory[9] = shared_memory[9] + 1;
                break;
            default:
                printf("Unknown msg = %d ", ntohl(CMs[i].message));
                ERRORresults++;
                shared_memory[9] = shared_memory[9] + 1;
                break;
            }
            // 以下只是计算
            switch (ntohl(CP[i].arith))
            {
            case 1: /*add */
                CP[i].inResult = htonl(ntohl(CP[i].inValue1) + ntohl(CP[i].inValue2));
                printf("[ %d + %d => %d ] ", ntohl(CP[i].inValue1), ntohl(CP[i].inValue2), ntohl(CP[i].inResult));
                break;
            case 2: /*sub */
                CP[i].inResult = htonl(ntohl(CP[i].inValue1) - ntohl(CP[i].inValue2));
                printf("[ %d - %d => %d ] ", ntohl(CP[i].inValue1), ntohl(CP[i].inValue2), ntohl(CP[i].inResult));
                break;
            case 3: /*mul */
                CP[i].inResult = htonl(ntohl(CP[i].inValue1) * ntohl(CP[i].inValue2));
                printf("[ %d * %d => %d ] ", ntohl(CP[i].inValue1), ntohl(CP[i].inValue2), ntohl(CP[i].inResult));
                break;
            case 4: /*div */
                CP[i].inResult = htonl(ntohl(CP[i].inValue1) / ntohl(CP[i].inValue2));
                printf("[ %d / %d => %d ] ", ntohl(CP[i].inValue1), ntohl(CP[i].inValue2), ntohl(CP[i].inResult));
                break;
            case 5: /*fadd */
                CP[i].flResult = CP[i].flValue1 + CP[i].flValue2;
                printf("[ %g + %g => %g ] ", CP[i].flValue1, CP[i].flValue2, CP[i].flResult);
                break;
            case 6: /*fsub */
                CP[i].flResult = CP[i].flValue1 - CP[i].flValue2;
                printf("[ %g - %g => %g ] ", CP[i].flValue1, CP[i].flValue2, CP[i].flResult);
                break;
            case 7: /*fmul */
                CP[i].flResult = CP[i].flValue1 * CP[i].flValue2;
                printf("[ %g * %g => %g ] ", CP[i].flValue1, CP[i].flValue2, CP[i].flResult);
                break;
            case 8: /*fdiv */
                CP[i].flResult = CP[i].flValue1 / CP[i].flValue2;
                printf("[  %g / %g => %g ] ", CP[i].flValue1, CP[i].flValue2, CP[i].flResult);
                break;
            default:
                printf(" ** SHIT unkown arithm. %d ** ", ntohl(CP[i].arith));
                break;
            }

            printf("\n");

            pthread_mutex_lock(&mutex);
            shared_memory[3] = shared_memory[3] + 1;
            pthread_mutex_unlock(&mutex);

            exit(0);
        }
        else if (pid < 0)
        {
            // 出错
            perror("fork failed");
            exit(0);
        }
        else
        {
            // 父进程
            continue;
        }
    }

    // printf("\n-----RESPONSES to calcMessage (registration) ----- \n");
    while (shared_memory[0] != noClients)
    {
    }

    // 可优化弄成 setsockopt
    printf("\nWaiting 4s \n");
    sleep(4);

    pthread_mutex_lock(&mutex);
    shared_memory[1] = 1;
    pthread_mutex_unlock(&mutex);

    printf("Doing Calculations .\n\n");
    while (shared_memory[3] != noClients)
    {
    }
    // 循环结束
    printf("Reading server response, expecting %d replies .\n", noClients - dropped);

    printf("Setting a timeout of %d seconds on reads.\n", timeout_in_seconds);

    // ——————————————————————————————————————————————————————————————
    // 以下似乎是第二次运算

    // 得到结果
    printf("Done, with good clients.\n");

    // ————————————————————————————————————————————————————————————
    // 以下是测试连接准备发送吗？？

    bobsMother = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (bobsMother == -1)
    {
        perror("Socket cant do nr2");
    }
    else
    {
        rv = connect(bobsMother, p->ai_addr, p->ai_addrlen);
        if (rv == -1)
        {
            perror("Cant connect to socket..");
        }
        else
        {
            if (ip_version == 4)
            {
                if ((s = getsockname(bobsMother, (struct sockaddr *)&sa, &sa_len) == -1))
                {
                    perror("getsockname failed.");
                }
                else
                {
                    myAdd = inet_ntop(sa.sin_family, &sa.sin_addr, localIP, sizeof(localIP));
                    printf("BobsMother (%s:%d) \n", localIP, ntohs(sa.sin_port));
                }
            }
            else
            {
                if ((s = getsockname(bobsMother, (struct sockaddr *)&sa6, &sa_len6) == -1))
                {
                    perror("getsockname failed.");
                }
                else
                {
                    myAdd = inet_ntop(sa6.sin6_family, &sa6.sin6_addr, localIP, sizeof(localIP));
                    printf("BobsMother (%s:%d) \n", localIP, ntohs(sa6.sin6_port));
                }
            }
        }
    }

    char myMsg[] = "TEXT UDP 1.0";

    //  printf("bob\n");
    gettimeofday(&ct1, NULL);
    //  printf("alice\n");

    printf("Client will 'connect', and send a text string (rubbish). \n");
    printf("Server should reply with an ERROR indicated.\n");

    printf("Client[X] ");
    /*
    printf("Client[X] 10.81.182.234:39347 sent 12 bytes\n");
    printf("Client[X] expecting 12 or 50 bytes.\n");
    printf("Client[X] got 12 bytes, within 305729 [us].\n");
    printf("S->C [binary] (correct)version=8241.11824  Not OK (correct)\n");
    printf("Done with BAD clients.\n");
    printf("SUMMARY Tested:5 Dropped:0 OK:5 ERROR:0 BAD:0 ErrorRatio is fine (0 < 0.1)\n");
    printf("SUMMARY: PASSED!\n");
*/
    if (ip_version == 4)
    {
        if ((numbytes = sendto(bobsMother, myMsg, strlen(myMsg), 0, (struct sockaddr *)&clientAddress, sizeof(clientAddress))) == -1)
        {
            perror("talker: sendto");
            exit(1);
        }
        else
        {
            if ((s = getsockname(bobsMother, (struct sockaddr *)&sa, &sa_len) == -1))
            {
                perror("getsockname failed.");
            }
            else
            {
                myAdd = inet_ntop(sa.sin_family, &sa.sin_addr, localIP, sizeof(localIP));
                printf("BobsMother (%s:%d) \n", localIP, ntohs(sa.sin_port));
            }
        }
    }
    else
    {
        if ((numbytes = sendto(bobsMother, myMsg, strlen(myMsg), 0, (struct sockaddr *)&clientAddress6, sizeof(clientAddress6))) == -1)
        {
            perror("talker6: sendto");
            // exit(1);
        }
        else
        {
            if ((s = getsockname(bobsMother, (struct sockaddr *)&sa6, &sa_len6) == -1))
            {
                perror("getsockname failed.");
            }
            else
            {
                myAdd = inet_ntop(sa6.sin6_family, &sa6.sin6_addr, localIP, sizeof(localIP));
                printf("BobsMother (%s:%d) \n", localIP, ntohs(sa6.sin6_port));
            }
        }
    }
    setsockopt(bobsMother, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv);
    if ((numbytes = recvfrom(bobsMother, buffer, sizeof(buffer), 0, (struct sockaddr *)&their_addr, &addr_len)) == -1)
    {
        printf("Client[X] Error (%d) %s \n", errno, strerror(errno));
        if (errno == ETIMEDOUT)
        {
            printf("Client[X] timedout.\n");
        }
    }
    else
    {
        printf("Client[X] expecting %d or %lu bytes.\n", sizeof(cMessage), sizeof(cProtocol));
        printf("Client[X] got %d bytes, ", numbytes);
    }
    gettimeofday(&ct2, 0);
    //  printf("seconds : %ld\nmicro seconds : %ld", ct2.tv_sec, ct2.tv_usec);
    double tv1, tv2;
    tv2 = (double)ct2.tv_sec + (double)(ct2.tv_usec) / 1000000;
    tv1 = (double)ct1.tv_sec + (double)(ct1.tv_usec) / 1000000;
    printf("within %g [us].\n", (tv2 - tv1) * 1000 * 1000);

    int badCproblem = 0;

    if (numbytes == sizeof(cMessage))
    {
        memcpy(&CM, buffer, sizeof(cMessage));
        switch (ntohs(CM.type))
        {
        case 1:
            printf("S->C [ascii] (wrong)");
            badCproblem++;
            break;
        case 2:
            printf("S->C [binary] (correct)");
            break;
        case 3:
            printf("S->C [N/A] (wrong)");
            badCproblem++;
            break;
        case 4:
            printf("C->S [ascii] (wrong)");
            badCproblem++;
            break;
        case 5:
            printf("C->S [binary] (wrong)");
            badCproblem++;
            break;
        case 6:
            printf("C->S [N/A] (wrong)");
            badCproblem++;
            break;
        default:
            printf(" unknown type=%d ", ntohs(CM.type));
            badCproblem++;
            break;
        }

        printf("version=%d.%d ", ntohs(CM.major_version), ntohs(CM.minor_version));
        switch (ntohl(CM.message))
        {
        case 0:
            printf(" N/A  (wrong)");
            badCproblem++;
            break;
        case 1:
            printf(" OK (wrong)");
            badCproblem++;
            break;
        case 2:
            printf(" Not OK (correct)");
            break;
        default:
            printf("Unknown msg = %d (wrong)", ntohl(CM.message));
            badCproblem++;
            break;
        }
    }
    else if (numbytes == sizeof(cProtocol))
    {
        printf("Client[X] got a cProtocol, ");
        memcpy(&CP[0], buffer, sizeof(cProtocol));
        printf(" type = %d \n", ntohs(CP[0].type));
        badCproblem++;
    }
    else
    {
        printf("Client[X] got not the size that I expected.\n");
    }
    printf("\nDone with BAD clients.\n");
    if (badCproblem > 0)
    {
        printf("%d issues with bad clients.\n", badCproblem);
        printf("see the log above if it was type, message or both.\n");
    }
    ERRORresults += badCproblem;
    shared_memory[9] = shared_memory[9] + badCproblem;

    close(bobsMother);

    printf("SUMMARY Tested:%d Dropped:%d OK:%d ERROR:%d BAD:%d", noClients, dropped, shared_memory[8], shared_memory[9], badCproblem);

    double errorRatio = 100.0;
    if (ERRORresults > 0)
    {
        errorRatio = (double)(ERRORresults) / (double)(noClients - dropped);
        //    printf("Calcs ratio = %8.8g \n", errorRatio);
    }
    else
    {
        errorRatio = 0;
    }

    if (errorRatio > 0.1)
    {
        printf(" ErrorRatio is to high (%g > 0.1) \n", errorRatio);
    }
    else
    {
        printf(" ErrorRatio is fine (%g < 0.1) \n", errorRatio);
    }

    if (fptr != NULL)
        fprintf(fptr, "Tested:%d Dropped:%d OK:%d ERROR:%d ErrorRatio:%g BAD:%d\n", noClients, dropped, shared_memory[8], shared_memory[9], errorRatio, badCproblem);

    if (fptr != NULL)
        fclose(fptr);

    if (ERRORresults == 0)
    {
        printf("SUMMARY: PASSED!\n");
    }
    else
    {
        printf("SUMMARY: FAILED!\n");
    }

    // 删除共享内存段
    if (shmctl(shmid, IPC_RMID, NULL) == -1)
    {
        perror("shmctl failed");
        return 1;
    }
    // Close socket and quit program
    // TODO
    // return 0;
}