// WebsocketSignalServer.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <fstream>
#include "signal_server.h"
#include "message_queue.h"
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/exceptions.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/support/date_time.hpp>

#define DAEMON "daemon"
#define STOP "stop"
#define START "start"
#define SERVICE "service"

#define FILTER_DEBUG "debug"
#define FILTER_INFO "info"
#define FILTER_WARNING "warning"
#define FILTER_ERROR "error"
#define FILTER_FATAL "fatal"


#define LOG_CONSOLE 0
#define LOG_FILE_USER 1
#define LOG_FILE_SERVICE 2

void init_log(int type,int filter)
{
  namespace keyword = boost::log::keywords;
  namespace sinks = boost::log::sinks;
  namespace expr = boost::log::expressions;
  if (type == LOG_CONSOLE) {
    boost::log::add_console_log(
            std::clog,
            keyword::format =
                    (
                            expr::stream
                                    << expr::format_date_time< boost::posix_time::ptime >("TimeStamp", "[%m/%d %H:%M:%S]")
                                    << "[" << boost::log::trivial::severity
                                    << "] " << expr::smessage
                    )
    );

  }else {
    char tbuffer[128];
    auto t = std::time(nullptr);

    std::strftime(tbuffer,sizeof(tbuffer),(type==LOG_FILE_USER)?"log/wsSignalServer_%Y%m%d.%H%M%S":
    "/root/log/wsSignalServer_%Y%m%d.%H%M%S",
                  std::localtime(&t));
    boost::log::add_file_log(
            keyword::file_name = strcat(tbuffer,"_%N.log"),
            keyword::rotation_size = 10*1024*1024,
            keyword::time_based_rotation = sinks::file::rotation_at_time_point(0,0,0),
            keyword::format =
                    (
                            expr::stream
                                    << expr::format_date_time< boost::posix_time::ptime >("TimeStamp", "[%m/%d %H:%M:%S]")
                                    << "[" << boost::log::trivial::severity
                                    << "] " << expr::smessage
                    )
    );

  }

  boost::log::core::get()->set_filter(
          boost::log::trivial::severity >= filter
  );
  boost::log::add_common_attributes();
}


int listen(int port)
{
  SignalServer server;
  server.Listen(port);
  return 0;
}

class arg_option {
public:
  arg_option(int argc,char** argv){
    for (int i = 0; i < argc; ++i) {
      m_arg_list.emplace_back(argv[i]);
    }
  }

  std::string get(const std::string& command,const char* def) {
    for (int i = 0; i < m_arg_list.size(); i++) {
      if (m_arg_list[i] == command && i+1 < m_arg_list.size()) {
        return m_arg_list[i+1];
      }
    }
    return def;
  }

private:
  std::vector<std::string> m_arg_list;
};


int main(int argc,char* argv[])
{
  arg_option opt(argc,argv);
  int port = atoi(opt.get("-p","2000").data());

  std::string command = opt.get("-c",START);
  std::string filter_string = opt.get("-f",FILTER_INFO);

  boost::log::trivial::severity_level filter = boost::log::trivial::info;
  if (filter_string == FILTER_DEBUG)
    filter = boost::log::trivial::debug;
  else if(filter_string == FILTER_INFO)
    filter = boost::log::trivial::info;
  else if(filter_string == FILTER_WARNING)
    filter = boost::log::trivial::warning;
  else if(filter_string == FILTER_ERROR)
    filter = boost::log::trivial::error;
  else if(filter_string == FILTER_FATAL)
    filter = boost::log::trivial::fatal;

  if (command == DAEMON) {
#ifndef WIN32
    if (-1 == daemon(1, 1)) {
      std::cout << "daemon error\n";
      exit(-1);
    } else {
      init_log(LOG_FILE_USER,filter);
      return listen(port);
    };
#else
    init_log(false);
    return listen(port);
#endif
  }else if(command == START) {
    init_log(LOG_CONSOLE,filter);
    BOOST_LOG_TRIVIAL(info) << "";
    return listen(port);
  }else if(command == STOP) {
    MessageQueue queue(false);
    queue.SendExitMessage();
    return 0;
  } else if(command == SERVICE){
#ifndef WIN32
    if (-1 == daemon(1, 1)) {
      std::cout << "daemon error\n";
      exit(-1);
    } else {
    init_log(LOG_FILE_SERVICE,filter);

    return listen(port);
    }
#else
  return -1;
#endif
  } else {
    return  1;
  }
}


