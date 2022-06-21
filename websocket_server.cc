#include "websocket_server.h"
#include <boost/log/trivial.hpp>
#include <utility>

WebsocketServer::WebsocketServer():m_exit_signal(false),m_message_queue(true)
{
  // Initialize Asio Transport
  m_server.clear_access_channels(websocketpp::log::alevel::all);
  m_server.set_access_channels(websocketpp::log::alevel::none);
  
  m_server.init_asio();
  // Register handler callbacks
  m_server.set_open_handler(bind(&WebsocketServer::on_open, this, ::_1));
  m_server.set_close_handler(bind(&WebsocketServer::on_close, this, ::_1));
  m_server.set_message_handler(bind(&WebsocketServer::on_message, this, ::_1, ::_2));
  m_server.set_pong_timeout(5000);
  m_server.set_pong_timeout_handler(bind(&WebsocketServer::on_pong_timeout, this,::_1,::_2));
}

void WebsocketServer::run(uint16_t port)
{
  // listen on specified port
  m_server.listen(port);

  // Start the server accept loop
  m_server.start_accept();
  // Start the ASIO io_service run loop
  try {
    BOOST_LOG_TRIVIAL(info) << "server run at:" << port;
    m_server.run();
  }
  catch (const std::exception & e) {
    BOOST_LOG_TRIVIAL(info) << e.what();
  }
}

void WebsocketServer::on_open(connection_hdl hdl)
{
  {
    lock_guard<mutex> guard(m_action_lock);
    m_actions.push(action(SUBSCRIBE, std::move(hdl)));
  }
  m_action_cond.notify_one();
}

void WebsocketServer::on_close(connection_hdl hdl)
{
  {
    lock_guard<mutex> guard(m_action_lock);
    m_actions.push(action(UNSUBSCRIBE, std::move(hdl)));
  }
  m_action_cond.notify_one();
}

void WebsocketServer::on_message(connection_hdl hdl, server::message_ptr msg)
{
  // queue message up for sending by processing thread
  {
    lock_guard<mutex> guard(m_action_lock);
    m_actions.push(action(MESSAGE, std::move(hdl), std::move(msg)));
  }
  m_action_cond.notify_one();
}

void WebsocketServer::process_messages()
{
  while (true)
  {
    unique_lock<mutex> lock(m_action_lock);

    while (m_actions.empty()) 
    {
      m_action_cond.wait(lock);
    }

    action a = m_actions.front();
    m_actions.pop();
    lock.unlock();

    if (a.type == SUBSCRIBE) 
    {
      lock_guard<mutex> guard(m_connection_lock);
      m_connections.insert(a.hdl);
    }
    else if (a.type == UNSUBSCRIBE)
    {
      lock_guard<mutex> guard(m_connection_lock);
      m_connections.erase(a.hdl);
      OnClose(a.hdl);
    }
    else if (a.type == MESSAGE) 
    {
      lock_guard<mutex> guard(m_connection_lock);
      if (a.msg->get_opcode() == websocketpp::frame::opcode::text)
      {
        BOOST_LOG_TRIVIAL(debug) << "-->RECV:\n" << a.msg->get_payload();
        OnReceive(a.hdl, a.msg->get_payload());
      }

    }
    else if(a.type == EXIT)
    {
      BOOST_LOG_TRIVIAL(info) << "message_process loop return";
      return;
    }
    else {
      // undefined
    }

  }
}

void WebsocketServer::Listen(int port)
{
  try
  {
    // Start a thread to run the processing loop
    thread t1(bind(&WebsocketServer::process_messages, this));
    thread t2(bind(&WebsocketServer::loop_ping, this));
    thread t3(bind(&WebsocketServer::wait_exit_message,this));
    // Run the asio loop with the main thread
    run(port);
    t3.join();
    t2.join();
    t1.join();
    BOOST_LOG_TRIVIAL(info) << "Exit.";
  }
  catch (websocketpp::exception const & e)
  {
	  BOOST_LOG_TRIVIAL(error) << "---main loop exit---" << e.what();
  }
}

bool WebsocketServer::Send(void * data, int len, connection_hdl hdl)
{
	try {
	 m_server.send(std::move(hdl), data, len, websocketpp::frame::opcode::BINARY);
	 return true;
	}
	catch (std::exception& e)
	{
		BOOST_LOG_TRIVIAL(error) << "send error:"<< e.what();
	}
	return false;
}

bool WebsocketServer::Send(const std::string& text, connection_hdl hdl)
{
	try
	{
		m_server.send(std::move(hdl), text, websocketpp::frame::opcode::TEXT);
    BOOST_LOG_TRIVIAL(debug) << "<--SEND:\n" << text << "\n";
		return true;
	}
	catch (std::exception& e)
	{
		BOOST_LOG_TRIVIAL(error) <<"send error:"<< e.what();
	}
	return false;
}

void WebsocketServer::Broadcast(const std::string& text)
{
  lock_guard<mutex> guard(m_connection_lock);
  for (const auto& hdl: m_connections)
  {
    Send(text, hdl);
  }
}

void WebsocketServer::Broadcast(void* data, int len)
{
  lock_guard<mutex> guard(m_connection_lock);
  for (const auto& hdl : m_connections)
  {
    Send(data,len, hdl);
  }
}

void WebsocketServer::loop_ping() {
  while (true)
  {
    if (m_mutex_exit.wait(5000)){
      BOOST_LOG_TRIVIAL(info) << "loop_ping exit.";
      return;
    }
    lock_guard<mutex> guard(m_connection_lock);
    for (const auto& hdl : m_connections)
    {
      std::error_code er;
      m_server.ping(hdl,"",er);
      if (er)
      {
        BOOST_LOG_TRIVIAL(error) << er.message();
      }
    }
  }

}

void WebsocketServer::on_pong_timeout(connection_hdl hdl, std::string s) {
  std::error_code er;
  m_server.close(hdl,websocketpp::close::status::normal,"pong timeout",er);
  BOOST_LOG_TRIVIAL(info) << "pong timeout";
}

void WebsocketServer::wait_exit_message() {
  if (m_message_queue.WaitExitMessage()){
    BOOST_LOG_TRIVIAL(info) << "receive exit message";
    m_mutex_exit.notify();
    m_server.stop();
    {
      lock_guard<mutex> guard(m_action_lock);
      m_actions.push(action(EXIT, connection_hdl()));
    }
    m_action_cond.notify_one();
  } else {
    BOOST_LOG_TRIVIAL(error) << "error wait_exit_message";
  }
}

