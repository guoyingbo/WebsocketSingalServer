#include "websocket_server.h"

WebsocketServer::WebsocketServer()
{
  // Initialize Asio Transport
  m_server.clear_access_channels(websocketpp::log::alevel::all);
  m_server.set_access_channels(websocketpp::log::alevel::none);
  
  m_server.init_asio();
  // Register handler callbacks
  m_server.set_open_handler(bind(&WebsocketServer::on_open, this, ::_1));
  m_server.set_close_handler(bind(&WebsocketServer::on_close, this, ::_1));
  m_server.set_message_handler(bind(&WebsocketServer::on_message, this, ::_1, ::_2));
}

void WebsocketServer::run(uint16_t port)
{
  // listen on specified port
  m_server.listen(port);

  // Start the server accept loop
  m_server.start_accept();
  // Start the ASIO io_service run loop
  try {
    std::cout << "server run at:" << port << "\n";
    m_server.run();
  }
  catch (const std::exception & e) {
    std::cout << e.what() << std::endl;
  }
}

void WebsocketServer::on_open(connection_hdl hdl)
{
  {
    lock_guard<mutex> guard(m_action_lock);
    m_actions.push(action(SUBSCRIBE, hdl));
  }
  m_action_cond.notify_one();
}

void WebsocketServer::on_close(connection_hdl hdl)
{
  {
    lock_guard<mutex> guard(m_action_lock);
    m_actions.push(action(UNSUBSCRIBE, hdl));
  }
  m_action_cond.notify_one();
}

void WebsocketServer::on_message(connection_hdl hdl, server::message_ptr msg)
{
  // queue message up for sending by processing thread
  {
    lock_guard<mutex> guard(m_action_lock);
    m_actions.push(action(MESSAGE, hdl, msg));
 //   std::cout << "-->RECV:" << msg->get_payload() << "\n";
  }
  m_action_cond.notify_one();
}

void WebsocketServer::process_messages()
{
  while (1) 
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

      }
      OnReceive(a.hdl, a.msg->get_payload());
    }
    else 
    {
      // undefined.
    }
  }
}

void WebsocketServer::Listen(int port)
{
  try
  {
    // Start a thread to run the processing loop
    thread t(bind(&WebsocketServer::process_messages, this));

    // Run the asio loop with the main thread
    run(port);

    t.join();

  }
  catch (websocketpp::exception const & e)
  {
	  std::cout << "---main loop exit---\n";
    std::cout << e.what() << std::endl;
  }
}

bool WebsocketServer::Send(void * data, int len, connection_hdl hdl)
{
	try {
	 m_server.send(hdl, data, len, websocketpp::frame::opcode::BINARY);
	 return true;
	}
	catch (std::exception& e)
	{
		std::cout << "send error:"<< e.what() << std::endl;
	}
	return false;
}

bool WebsocketServer::Send(const std::string& text, connection_hdl hdl)
{
	try
	{
		m_server.send(hdl, text, websocketpp::frame::opcode::TEXT);
 //   std::cout << "<--SEND:" << text << "\n";
		return true;
	}
	catch (std::exception& e)
	{
		std::cout <<"send error:"<< e.what() << std::endl;
	}
	return false;
}

void WebsocketServer::Broadcast(const std::string& text)
{
  for (auto hdl: m_connections)
  {
    Send(text, hdl);
  }
}

void WebsocketServer::Broadcast(void* data, int len)
{
  for (auto hdl : m_connections)
  {
    Send(data,len, hdl);
  }
}

void WebsocketServer::OnReceive(connection_hdl hdl, const std::string& message)
{

}

