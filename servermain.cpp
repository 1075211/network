#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>

/* You will to add includes here */
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <mutex>
#include <sys/types.h>
#include <sys/socket.h>
#include <math.h>
#include <unordered_map>
// Included to get the support library
#include "calcLib.h"
#include "calcLib.c"

#include "protocol.h"

using namespace std;
// longest length of datagram
#define MAXLENGTH 1024
// the working means the server is hanldling the datagram
#define WORKING 1
// the waiting means the server is now available for clients
#define WAITING 0
// make bzero() works normally
#define bzero(a, b) memset(a, 0, b)
// set a port number
#define MYPORT 4950
/* Needs to be global, to be rechable by callback and main */
int loopCount = 0;
int Ter = 0;
int id = 0;                               // id starts from 0
int work = WAITING;                       // not working at the start
unordered_map<int, int> communication_ID; // the map stores the clients' datagrams id and the waiting time
mutex map_lock;                           // The lock protecting communication_id

// This function is used to get the result of a calc protocol.
double getResult(calcProtocol *ptc)
{
  int type = 0;
  const char *a;
  switch (ntohl(ptc->arith))
  {
  case 1:
    ptc->inResult = htonl(ntohl(ptc->inValue1) + ntohl(ptc->inValue2));
    a = "add";
    break;
  case 2:
    ptc->inResult = htonl(ntohl(ptc->inValue1) - ntohl(ptc->inValue2));
    a = "sub";
    break;
  case 3:
    ptc->inResult = htonl(ntohl(ptc->inValue1) * ntohl(ptc->inValue2));
    a = "mul";
    break;
  case 4:
    ptc->inResult = htonl(ntohl(ptc->inValue1) / ntohl(ptc->inValue2));
    a = "div";
    break;
  case 5:
    ptc->flResult = ptc->flValue1 + ptc->flValue2;
    type = 1;
    a = "fadd";
    break;
  case 6:
    ptc->flResult = ptc->flValue1 - ptc->flValue2;
    type = 1;
    a = "fsub";
    break;
  case 7:
    ptc->flResult = ptc->flValue1 * ptc->flValue2;
    type = 1;
    a = "fmul";
    break;
  case 8:
    ptc->flResult = ptc->flValue1 / ptc->flValue2;
    type = 1;
    a = "fdiv";
    break;
  }
  if (type == 0)
  {
    printf("server calculate %u %s %u result is %u\n", ntohl(ptc->inValue1), a, ntohl(ptc->inValue2), ntohl(ptc->inResult));
    return ntohl(ptc->inResult);
  }
  else
  {
    printf("server calculate %f %s %f result is %f\n", ptc->flValue1, a, ptc->flValue2, ptc->flResult);
    return ptc->flResult;
  }
}

/* Call back function, will be called when the SIGALRM is raised when the timer expires. */
void checkJobbList(int signum)
{
  // As anybody can call the handler, its good coding to check the signal number that called it.
  if (work == WAITING)
  { // the server is available, just record the time.
    loopCount++;
  }
  else
  {
    printf("Let me be, I want to sleep.\n");
  }
  // check the communication_id
  // 1. lock the map
  map_lock.lock();
  // 2. check in turn
  for (unordered_map<int, int>::iterator it = communication_ID.begin(); it != communication_ID.end();)
  {
    it->second++;
    if (it->second >= 10)
    {
      printf("Client %d waits more than 10s.\n", it->first);
      it = communication_ID.erase(it);
    }
    else
      ++it;
  }
  map_lock.unlock();

  if (loopCount > 20)
  {
    printf("Wait for a client.\n");
    Ter = 1;
  }
  return;
}

int main(int argc, char *argv[])
{
  if (argc < 2)
  {
    fprintf(stderr, "Usage: %s <hostname>:<port>\n", argv[0]);
    exit(1);
  }
  // 初始化calcLib库
  int init_result = initCalcLib();
  if (init_result != 0)
  {
    fprintf(stderr, "Failed to initialize calcLib library\n");
    return 1;
  }
  char *p = argv[1];
  int colonCount = 0;

  // 统计冒号的数量
  while (*p)
  {
    if (*p == ':')
    {
      colonCount++;
    }
    p++;
  }
  char *Desthost;
  char *Destport;
  int ip_type = 0;
  // 根据冒号的数量判断是IPv4还是IPv6
  if (colonCount == 1)
  { // IPv4
    ip_type = 4;
    char delim[] = ":";
    Desthost = strtok(argv[1], delim); // 提取IP地址
    Destport = strtok(NULL, delim);    // 提取端口号
    if (Destport == NULL)
    {
      printf("Invalid format.\n");
      return 1;
    }
    // 如果主机名是 "localhost"，将其解析为 "127.0.0.1"
    if (strcmp(Desthost, "localhost") == 0)
    {
      char local[] = "127.0.0.1";
      Desthost = local;
    }
    int port = atoi(Destport); // 将端口号字符串转换为整数
    printf("IPv4 Address: %s Port: %d\n", Desthost, port);
  }
  else if (colonCount > 1)
  {
    ip_type = 6;                      // IPv6
    Desthost = argv[1];               // IPv6地址
    Destport = strrchr(argv[1], ':'); // 查找最后一个冒号，即端口号的位置
    if (Destport == NULL || Destport == argv[1])
    {
      printf("Invalid format.\n");
      return 1;
    }
    *Destport = '\0';          // 将冒号替换为字符串结束符
    Destport++;                // 移动指针到端口号的起始位置
    int port = atoi(Destport); // 将端口号字符串转换为整数
    printf("IPv6 Address: %s Port: %d\n", Desthost, port);
  }
  else
  {
    printf("Invalid format.\n");
    return 1;
  }

  /* Do more magic */
  /*
     Prepare to setup a reoccurring event every 1s. If it_interval, or it_value is omitted, it will be a single alarm 10s after it has been set.
  */
  struct itimerval alarmTime;
  alarmTime.it_interval.tv_sec = 1;
  alarmTime.it_interval.tv_usec = 0;
  alarmTime.it_value.tv_sec = 1;
  alarmTime.it_value.tv_usec = 0;

  /* Regiter a callback function, associated with the SIGALRM signal, which will be raised when the alarm goes of */
  signal(SIGALRM, checkJobbList);
  setitimer(ITIMER_REAL, &alarmTime, NULL); // Start/register the alarm.
  // register id stores current id number.
  int register_id;
  // create socket
  int servfd, rvsdlen;
  struct sockaddr_in clitAddr;
  struct sockaddr_in6 clitAddr_6;

  socklen_t address_length;
  socklen_t address6_length;
  address_length = sizeof(clitAddr);
  address6_length = sizeof(clitAddr_6);
  if (ip_type == 4)
  {
    struct sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress)); // 清空地址结构体

    if ((servfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
      perror("socket");
    }

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(atoi(Destport)); // 将字符串类型的端口号转换为网络字节序
    if (inet_pton(AF_INET, Desthost, &serverAddress.sin_addr) <= 0)
    {
      perror("inet_pton");
    }

    if (bind(servfd, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) == -1)
    {
      perror("bind");
    }
  }
  else
  {
    struct sockaddr_in6 serverAddress6;
    memset(&serverAddress6, 0, sizeof(serverAddress6)); // 清空地址结构体

    if ((servfd = socket(AF_INET6, SOCK_DGRAM, 0)) == -1)
    {
      perror("socket");
    }

    serverAddress6.sin6_family = AF_INET6;
    serverAddress6.sin6_port = htons(atoi(Destport)); // 将字符串类型的端口号转换为网络字节序
    if (inet_pton(AF_INET6, Desthost, &serverAddress6.sin6_addr) <= 0)
    {
      perror("inet_pton");
    }

    if (bind(servfd, (struct sockaddr *)&serverAddress6, sizeof(serverAddress6)) == -1)
    {
      perror("bind");
    }
  }
  // prepare for receiving and sending messages
  char *ope; // ope is the string to save the operation message
  char rvsdbuf[MAXLENGTH] = {0};
  calcProtocol ptc, respondePtc;
  calcMessage msg;
  char myMsg[] = "TEXT UDP 1.0";
  // start communicating
  if (ip_type == 4)
  {
    while ((rvsdlen = recvfrom(servfd, rvsdbuf, MAXLENGTH, 0, (struct sockaddr *)&clitAddr, &address_length)))
    {
      // if receive something error
      if (rvsdlen < 0)
      {
        printf("Client error!\n");
        break;
      }
      // receive normally works
      else
      {
        work = WORKING;
        if (rvsdlen == sizeof(msg))
        {
          getpeername(servfd, (struct sockaddr *)&clitAddr, &address_length); // getpeername is used to get client address and port.
          memcpy(&msg, rvsdbuf, rvsdlen);
          if (ntohs(msg.type) == 22 && ntohl(msg.message) == 0)
          {
            register_id = id;
            printf("\n|client come\nclient[%d]:PORT:%d\n", register_id, ntohs(clitAddr.sin_port));
            // receive an clac message, copy it to the msg.
            printf("Server received a calcMessage from client.\n");
            printf("  Type: %hu message: %u\n", ntohs(msg.type), ntohl(msg.message));
          }
          else
          {
            register_id = id;
            printf("\n|client come\nclient[%d]:PORT:%d\n", register_id, ntohs(clitAddr.sin_port));
            // receive an clac message, copy it to the msg.
            printf("Server received a calcMessage from client.\n");
            printf("  Type: %hu message: %u\n", ntohs(msg.type), ntohl(msg.message));
            msg.type = htons(2);
            msg.message = htonl(2);
            msg.major_version = htons(1);
            msg.minor_version = htons(0);
            sendto(servfd, (char *)&msg, sizeof(calcMessage), 0, (struct sockaddr *)&clitAddr, address_length);
            printf("|client finished\n");
            work = WAITING;
            loopCount = 0;
            continue;
          }
          // put register id into communication_id map
          communication_ID[register_id] = 0;
          // generate clacProtocol
          // initCalcLib();
          loopCount = 0;
          ope = randomType();
          if (ope[0] == 102)
          {
            ptc.arith = htonl(rand() % 4 + 5);
            ptc.flValue1 = randomFloat();
            ptc.flValue2 = randomFloat();
            ptc.inValue1 = htonl(0);
            ptc.inValue2 = htonl(0);
            ptc.inResult = htonl(0);
            ptc.flResult = 0.0f;
          }
          else
          {
            ptc.arith = htonl(1 + rand() % 4);
            ptc.inValue1 = htonl(randomInt());
            ptc.inValue2 = htonl(randomInt());
            ptc.flValue1 = 0.0f;
            ptc.flValue2 = 0.0f;
            ptc.inResult = htonl(0);
            ptc.flResult = 0.0f;
          }
          ptc.major_version = htons(1);
          ptc.minor_version = htons(0);
          ptc.type = htons(1);
          ptc.id = htonl(id++);
          // send to client
          sendto(servfd, (char *)&ptc, sizeof(ptc), 0, (struct sockaddr *)&clitAddr, address_length);
          printf("Server has generated a clacProtocol and sent to client.\n");
          printf("  Protocol: type: %hu arith: %u inValue1: %u  inValue2: %u  flValue1: %f  flValue2: %f\n",
                 ntohs(ptc.type), ntohl(ptc.arith), ntohl(ptc.inValue1), ntohl(ptc.inValue2), ptc.flValue1, ptc.flValue2);
        }
        // if a calcProtocol has been returned
        else if (rvsdlen == sizeof(respondePtc))
        {
          getpeername(servfd, (struct sockaddr *)&clitAddr, &address_length); // getpeername is used to get client address and port.
          printf("\n|client continued\nGet a responde from :PORT:%d\n", ntohs(clitAddr.sin_port));
          memcpy(&respondePtc, rvsdbuf, sizeof(respondePtc));
          register_id = ntohl(respondePtc.id);
          if (communication_ID.count(register_id) == 0)
          {
            // the client is out of the map because its message is out of time.
            printf("This clinet has been deleted.\n");
            work = WAITING;
            loopCount = 0;
            continue;
          }
          // receive a responde normally
          communication_ID[register_id] = 0; // reset the time.
          int32_t client_intresult = ntohl(respondePtc.inResult);
          double client_flresult = respondePtc.flResult;
          double server_result = getResult(&respondePtc); // obtain the result of calcProtocol
          printf("get result from client: in: %u  fl:%f\n", client_intresult, client_flresult);
          // 比较客户端和服务器端的结果
          double diff = 0;
          if (ntohl(respondePtc.arith) <= 4)
          {
            diff = fabs(server_result - client_intresult);
          }
          else
          {
            diff = fabs(server_result - client_flresult);
          }
          if (diff < 0.0001)
          {
            msg.type = htons(2);
            msg.message = htonl(1);
            msg.major_version = htons(1);
            msg.minor_version = htons(0);
            msg.protocol = htons(17);
            sendto(servfd, (char *)&msg, sizeof(calcMessage), 0, (struct sockaddr *)&clitAddr, address_length);
            printf("Succeeded!\n");
          }
          else
          {
            msg.type = htons(2);
            msg.message = htonl(2);
            msg.major_version = htons(1);
            msg.minor_version = htons(0);
            msg.protocol = htons(17);
            sendto(servfd, (char *)&msg, sizeof(calcMessage), 0, (struct sockaddr *)&clitAddr, address_length);
            printf("Failed!\n");
          }
          // erase the register id
          map_lock.lock();
          communication_ID.erase(register_id);
          map_lock.unlock();
          printf("|client  finished\n\n");
          work = WAITING;
          loopCount = 0;
        }
        // can't handle this type of message
        else
        {
          msg.type = htons(2);
          msg.message = htonl(0);
          msg.major_version = htons(1);
          msg.minor_version = htons(0);
          msg.protocol = htons(17);
          sendto(servfd, (char *)&msg, sizeof(calcMessage), 0, (struct sockaddr *)&clitAddr, address_length);
          printf("Can't handle this type of message! Rejected!\n");
          work = WAITING;
          loopCount = 0;
        }
      }
      work = WAITING;
      loopCount = 0;
    }
  }
  else{
    while ((rvsdlen = recvfrom(servfd, rvsdbuf, MAXLENGTH, 0,(struct sockaddr*)& clitAddr_6, &address6_length)))
    {
      // if receive something error
      if (rvsdlen < 0)
      {
        printf("Client error!\n");
        break;
      }
      // receive normally works
      else
      {
        work = WORKING;
        if (rvsdlen == sizeof(msg))
        {
          getpeername(servfd, (struct sockaddr*)&clitAddr_6, &address6_length); // getpeername is used to get client address and port.
          memcpy(&msg, rvsdbuf, rvsdlen);
          if (ntohs(msg.type) == 22 && ntohl(msg.message) == 0)
          {
            register_id = id;
            printf("\n|client come\nclient[%d]:PORT:%d\n", register_id, ntohs(clitAddr_6.sin6_port));
            // receive an clac message, copy it to the msg.
            printf("Server received a calcMessage from client.\n");
            printf("  Type: %hu message: %u\n", ntohs(msg.type), ntohl(msg.message));
          }
          else
          {
            register_id = id;
            printf("\n|client come\nclient[%d]:PORT:%d\n", register_id, ntohs(clitAddr_6.sin6_port));
            // receive an clac message, copy it to the msg.
            printf("Server received a calcMessage from client.\n");
            printf("  Type: %hu message: %u\n", ntohs(msg.type), ntohl(msg.message));
            msg.type = htons(2);
            msg.message = htonl(2);
            msg.major_version = htons(1);
            msg.minor_version = htons(0);
            sendto(servfd, (char *)&msg, sizeof(calcMessage), 0, (struct sockaddr*)&clitAddr_6, address6_length);
            printf("|client finished\n");
            work = WAITING;
            loopCount = 0;
            continue;
          }
          // put register id into communication_id map
          communication_ID[register_id] = 0;
          // generate clacProtocol
          // initCalcLib();
          loopCount = 0;
          ope = randomType();
          if (ope[0] == 102)
          {
            ptc.arith = htonl(rand() % 4 + 5);
            ptc.flValue1 = randomFloat();
            ptc.flValue2 = randomFloat();
            ptc.inValue1 = htonl(0);
            ptc.inValue2 = htonl(0);
            ptc.inResult = htonl(0);
            ptc.flResult = 0.0f;
          }
          else
          {
            ptc.arith = htonl(1 + rand() % 4);
            ptc.inValue1 = htonl(randomInt());
            ptc.inValue2 = htonl(randomInt());
            ptc.flValue1 = 0.0f;
            ptc.flValue2 = 0.0f;
            ptc.inResult = htonl(0);
            ptc.flResult = 0.0f;
          }
          ptc.major_version = htons(1);
          ptc.minor_version = htons(0);
          ptc.type = htons(1);
          ptc.id = htonl(id++);
          // send to client
          sendto(servfd, (char *)&ptc, sizeof(ptc), 0, (struct sockaddr*)&clitAddr_6, address6_length);
          printf("Server has generated a clacProtocol and sent to client.\n");
          printf("  Protocol: type: %hu arith: %u inValue1: %u  inValue2: %u  flValue1: %f  flValue2: %f\n",
                 ntohs(ptc.type), ntohl(ptc.arith), ntohl(ptc.inValue1), ntohl(ptc.inValue2), ptc.flValue1, ptc.flValue2);
        }
        // if a calcProtocol has been returned
        else if (rvsdlen == sizeof(respondePtc))
        {
          getpeername(servfd, (struct sockaddr*)&clitAddr_6, &address6_length); // getpeername is used to get client address and port.
          printf("\n|client continued\nGet a responde from :PORT:%d\n", ntohs(clitAddr_6.sin6_port));
          memcpy(&respondePtc, rvsdbuf, sizeof(respondePtc));
          register_id = ntohl(respondePtc.id);
          if (communication_ID.count(register_id) == 0)
          {
            // the client is out of the map because its message is out of time.
            printf("This clinet has been deleted.\n");
            work = WAITING;
            loopCount = 0;
            continue;
          }
          // receive a responde normally
          communication_ID[register_id] = 0; // reset the time.
          int32_t client_intresult = ntohl(respondePtc.inResult);
          double client_flresult = respondePtc.flResult;
          double server_result = getResult(&respondePtc); // obtain the result of calcProtocol
          printf("get result from client: in: %u  fl:%f\n", client_intresult, client_flresult);
          // 比较客户端和服务器端的结果
          double diff = 0;
          if (ntohl(respondePtc.arith) <= 4)
          {
            diff = fabs(server_result - client_intresult);
          }
          else
          {
            diff = fabs(server_result - client_flresult);
          }
          if (diff < 0.0001)
          {
            msg.type = htons(2);
            msg.message = htonl(1);
            msg.major_version = htons(1);
            msg.minor_version = htons(0);
            msg.protocol = htons(17);
            sendto(servfd, (char *)&msg, sizeof(calcMessage), 0, (struct sockaddr*)&clitAddr_6, address6_length);
            printf("Succeeded!\n");
          }
          else
          {
            msg.type = htons(2);
            msg.message = htonl(2);
            msg.major_version = htons(1);
            msg.minor_version = htons(0);
            msg.protocol = htons(17);
            sendto(servfd, (char *)&msg, sizeof(calcMessage), 0, (struct sockaddr*)&clitAddr_6, address6_length);
            printf("Failed!\n");
          }
          // erase the register id
          map_lock.lock();
          communication_ID.erase(register_id);
          map_lock.unlock();
          printf("|client  finished\n\n");
          work = WAITING;
          loopCount = 0;
        }
        // can't handle this type of message
        else
        {
          msg.type = htons(2);
          msg.message = htonl(0);
          msg.major_version = htons(1);
          msg.minor_version = htons(0);
          msg.protocol = htons(17);
          sendto(servfd, (char *)&msg, sizeof(calcMessage), 0, (struct sockaddr*)&clitAddr_6, address6_length);
          printf("Can't handle this type of message! Rejected!\n");
          work = WAITING;
          loopCount = 0;
        }
      }
      work = WAITING;
      loopCount = 0;
    }
  }

  while (Ter == 0)
  {
    printf("This is the main loop, %d time.\n", loopCount);
    sleep(1);
    loopCount++;
  }
  printf("done.\n");
  return 0;
}

