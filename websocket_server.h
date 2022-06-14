#pragma once

#include <websocketpp/config/asio_no_tls.hpp>

#include <websocketpp/server.hpp>

#include <iostream>
#include <set>

#include <websocketpp/common/thread.hpp>



using websocketpp::connection_hdl;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

using websocketpp::lib::thread;
using websocketpp::lib::mutex;
using websocketpp::lib::lock_guard;
using websocketpp::lib::unique_lock;
using websocketpp::lib::condition_variable;

/* on_open insert connection_hdl into channel
 * on_close remove connection_hdl from channel
 * on_message queue send to all channels
 */

enum action_type {
  SUBSCRIBE,
  UNSUBSCRIBE,
  MESSAGE
};



class WebsocketServer {
public:
  
  typedef websocketpp::server<websocketpp::config::asio> server;
  
  struct action {
  action(action_type t, connection_hdl h) : type(t), hdl(h) {}
  action(action_type t, connection_hdl h, server::message_ptr m)
    : type(t), hdl(h), msg(m) {}

  action_type type;
  websocketpp::connection_hdl hdl;
  server::message_ptr msg;
};
  WebsocketServer();
  void Listen(int port);

  bool Send(void* data, int len,connection_hdl hdl);
  bool Send(const std::string& text,connection_hdl hdl);

  void Broadcast(const std::string& text);
  void Broadcast(void* data, int len);
  virtual void OnReceive(connection_hdl hdl, const std::string& message) = 0;
  virtual void OnClose(connection_hdl) = 0;
protected:
  void run(uint16_t port);

  void on_open(connection_hdl hdl);

  void on_close(connection_hdl hdl);

  void on_message(connection_hdl hdl, server::message_ptr msg);

  void on_pong_timeout(connection_hdl hdl, std::string s);

  [[noreturn]] void process_messages();

  [[noreturn]] void loop_ping();
  server m_server;

protected:
  typedef std::set<connection_hdl, std::owner_less<connection_hdl> > con_list;

 
  con_list m_connections;
  std::queue<action> m_actions;

  mutex m_action_lock;
  mutex m_connection_lock;
  condition_variable m_action_cond;
};
