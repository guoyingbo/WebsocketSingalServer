// WebsocketSignalServer.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include "signal_server.h"

int main(int argc,char** argv)
{
  int port = 8888;
  if (argc == 2){
    port = atoi(argv[1]);
  }
  
  SignalServer server;
  server.Listen(port);
 
  return 0;
}


