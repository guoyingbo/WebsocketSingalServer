#include "websocket_server.h"
#include <boost/log/trivial.hpp>
#include <utility>

typedef websocketpp::lib::shared_ptr<boost::asio::ssl::context> context_ptr;


void on_http(server_tls* s, websocketpp::connection_hdl hdl) {
    server_tls::connection_ptr con = s->get_con_from_hdl(hdl);
    
    con->set_body("Hello World!");
    con->set_status(websocketpp::http::status_code::ok);
}

// No change to TLS init methods from echo_server_tls
std::string get_password() {
    return "Tlw(e2`s7_)";
}

// See https://wiki.mozilla.org/Security/Server_Side_TLS for more details about
// the TLS modes. The code below demonstrates how to implement both the modern
enum tls_mode {
    MOZILLA_INTERMEDIATE = 1,
    MOZILLA_MODERN = 2
};

context_ptr on_tls_init(tls_mode mode, websocketpp::connection_hdl hdl) {
    namespace asio = websocketpp::lib::asio;

    //BOOST_LOG_TRIVIAL(info) << "on_tls_init called with hdl: " << hdl.lock().get() << std::endl;
    BOOST_LOG_TRIVIAL(info) << "using TLS mode: " << (mode == MOZILLA_MODERN ? "Mozilla Modern" : "Mozilla Intermediate") << std::endl;

    context_ptr ctx = websocketpp::lib::make_shared<asio::ssl::context>(asio::ssl::context::sslv23);

    try {
        if (mode == MOZILLA_MODERN) {
            // Modern disables TLSv1
            ctx->set_options(asio::ssl::context::default_workarounds |
                             asio::ssl::context::no_sslv2 |
                             asio::ssl::context::no_sslv3 |
                             asio::ssl::context::no_tlsv1 |
                             asio::ssl::context::single_dh_use);
        } else {
            ctx->set_options(asio::ssl::context::default_workarounds |
                             asio::ssl::context::no_sslv2 |
                             asio::ssl::context::no_sslv3 |
                             asio::ssl::context::single_dh_use);
        }
        ctx->set_password_callback(bind(&get_password));
        ctx->use_certificate_chain_file("cert.pem");
        ctx->use_private_key_file("key.pem", asio::ssl::context::pem);
        
        // Example method of generating this file:
        // `openssl dhparam -out dh.pem 2048`
        // Mozilla Intermediate suggests 1024 as the minimum size to use
        // Mozilla Modern suggests 2048 as the minimum size to use.
        ctx->use_tmp_dh_file("dh.pem");
        
        std::string ciphers;
        
        if (mode == MOZILLA_MODERN) {
            ciphers = "ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES256-GCM-SHA384:DHE-RSA-AES128-GCM-SHA256:DHE-DSS-AES128-GCM-SHA256:kEDH+AESGCM:ECDHE-RSA-AES128-SHA256:ECDHE-ECDSA-AES128-SHA256:ECDHE-RSA-AES128-SHA:ECDHE-ECDSA-AES128-SHA:ECDHE-RSA-AES256-SHA384:ECDHE-ECDSA-AES256-SHA384:ECDHE-RSA-AES256-SHA:ECDHE-ECDSA-AES256-SHA:DHE-RSA-AES128-SHA256:DHE-RSA-AES128-SHA:DHE-DSS-AES128-SHA256:DHE-RSA-AES256-SHA256:DHE-DSS-AES256-SHA:DHE-RSA-AES256-SHA:!aNULL:!eNULL:!EXPORT:!DES:!RC4:!3DES:!MD5:!PSK";
        } else {
            ciphers = "ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES256-GCM-SHA384:DHE-RSA-AES128-GCM-SHA256:DHE-DSS-AES128-GCM-SHA256:kEDH+AESGCM:ECDHE-RSA-AES128-SHA256:ECDHE-ECDSA-AES128-SHA256:ECDHE-RSA-AES128-SHA:ECDHE-ECDSA-AES128-SHA:ECDHE-RSA-AES256-SHA384:ECDHE-ECDSA-AES256-SHA384:ECDHE-RSA-AES256-SHA:ECDHE-ECDSA-AES256-SHA:DHE-RSA-AES128-SHA256:DHE-RSA-AES128-SHA:DHE-DSS-AES128-SHA256:DHE-RSA-AES256-SHA256:DHE-DSS-AES256-SHA:DHE-RSA-AES256-SHA:AES128-GCM-SHA256:AES256-GCM-SHA384:AES128-SHA256:AES256-SHA256:AES128-SHA:AES256-SHA:AES:CAMELLIA:DES-CBC3-SHA:!aNULL:!eNULL:!EXPORT:!DES:!RC4:!MD5:!PSK:!aECDH:!EDH-DSS-DES-CBC3-SHA:!EDH-RSA-DES-CBC3-SHA:!KRB5-DES-CBC3-SHA";
        }
        
        if (SSL_CTX_set_cipher_list(ctx->native_handle() , ciphers.c_str()) != 1) {
            std::cout << "Error setting cipher list" << std::endl;
        }
    } catch (std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
    }
    return ctx;
}


WebsocketServer::WebsocketServer():m_exit_signal(false),m_message_queue(true)
{
  // Initialize Asio Transport
  m_server_plain.clear_access_channels(websocketpp::log::alevel::all);
  m_server_plain.set_access_channels(websocketpp::log::alevel::none);
  
  m_server_plain.init_asio(&m_ios);
  // Register handler callbacks
  m_server_plain.set_open_handler(bind(&WebsocketServer::on_open, this, ::_1));
  m_server_plain.set_close_handler(bind(&WebsocketServer::on_close, this, ::_1));
  m_server_plain.set_message_handler(bind(&WebsocketServer::on_message, this, ::_1, ::_2));
  m_server_plain.set_pong_timeout(15000);
  m_server_plain.set_pong_timeout_handler(bind(&WebsocketServer::on_pong_timeout, this,::_1,::_2));

  m_server_tls.clear_access_channels(websocketpp::log::alevel::all);
  m_server_tls.set_access_channels(websocketpp::log::alevel::none);
  
  m_server_tls.init_asio(&m_ios);
  m_server_tls.set_open_handler(bind(&WebsocketServer::on_open_tls, this, ::_1));
  m_server_tls.set_close_handler(bind(&WebsocketServer::on_close, this, ::_1));
  m_server_tls.set_message_handler(bind(&WebsocketServer::on_message,this, ::_1, ::_2));
  m_server_tls.set_http_handler(bind(&on_http, &m_server_tls, ::_1));
  m_server_tls.set_tls_init_handler(bind(&on_tls_init, MOZILLA_INTERMEDIATE, ::_1));
  m_server_tls.set_pong_timeout(15000);
  m_server_tls.set_pong_timeout_handler(bind(&WebsocketServer::on_pong_timeout, this,::_1,::_2));

}

void WebsocketServer::run(uint16_t port,uint16_t port_tls)
{
  // listen on specified port
  m_server_plain.listen(port);

  // Start the server accept loop
  m_server_plain.start_accept();
  // Start the ASIO io_service run loop
  BOOST_LOG_TRIVIAL(info) << "server run at:" << port;
  
  if (port_tls > 0){
    m_server_tls.listen(port_tls);
    m_server_tls.start_accept();   
    BOOST_LOG_TRIVIAL(info) << "stl server run at:" << port;
  }


  try {
    
    m_ios.run();
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

void WebsocketServer::on_open_tls(connection_hdl hdl)
{
  {
    lock_guard<mutex> guard(m_action_lock);
    m_actions.push(action(TLS_SUBSCRIBE, std::move(hdl)));
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

void WebsocketServer::on_message(connection_hdl hdl, server_plain::message_ptr msg)
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
      m_con_list_plain.insert(a.hdl);
    }
    else if (a.type == TLS_SUBSCRIBE) 
    {
      lock_guard<mutex> guard(m_connection_lock);
      m_con_list_tls.insert(a.hdl);
    }
    else if (a.type == UNSUBSCRIBE)
    {
      lock_guard<mutex> guard(m_connection_lock);
      m_con_list_plain.erase(a.hdl);
      m_con_list_tls.erase(a.hdl);
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

void WebsocketServer::Listen(int port,int port_tls)
{
  try
  {
    // Start a thread to run the processing loop
    thread t1(bind(&WebsocketServer::process_messages, this));
    thread t2(bind(&WebsocketServer::loop_ping, this));
    thread t3(bind(&WebsocketServer::wait_exit_message,this));
    // Run the asio loop with the main thread
    run(port,port_tls);
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
    if (m_con_list_plain.count(hdl))
	    m_server_plain.send(std::move(hdl), data, len, websocketpp::frame::opcode::BINARY);
    else
      m_server_tls.send(std::move(hdl), data, len, websocketpp::frame::opcode::BINARY);
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
    if (m_con_list_plain.count(hdl))
		  m_server_plain.send(std::move(hdl), text, websocketpp::frame::opcode::TEXT);
    else 
      m_server_tls.send(std::move(hdl), text, websocketpp::frame::opcode::TEXT);
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
  for (const auto& hdl: m_con_list_plain)
  {
    Send(text, hdl);
  }
  for (const auto& hdl: m_con_list_tls)
  {
    Send(text, hdl);
  }
}

void WebsocketServer::Broadcast(void* data, int len)
{
  lock_guard<mutex> guard(m_connection_lock);
  for (const auto& hdl : m_con_list_plain)
  {
    Send(data,len, hdl);
  }
  for (const auto& hdl : m_con_list_tls)
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
    for (const auto& hdl : m_con_list_plain)
    {
      std::error_code er;
      m_server_plain.ping(hdl,"",er);
      if (er)
      {
        BOOST_LOG_TRIVIAL(error) << er.message();
      }
    }
    
    for (const auto& hdl : m_con_list_tls)
    {
      std::error_code er;
      m_server_tls.ping(hdl,"",er);
      if (er)
      {
        BOOST_LOG_TRIVIAL(error) << er.message();
      }
    }

  }

}

void WebsocketServer::on_pong_timeout(connection_hdl hdl, std::string s) {
  std::error_code er;
  if (m_con_list_plain.count(hdl))
    m_server_plain.close(hdl,websocketpp::close::status::normal,"pong timeout",er);
  else
    m_server_tls.close(hdl,websocketpp::close::status::normal,"pong timeout",er);
  BOOST_LOG_TRIVIAL(info) << "pong timeout " << er.message();
}

void WebsocketServer::wait_exit_message() {
  if (m_message_queue.WaitExitMessage()){
    BOOST_LOG_TRIVIAL(info) << "receive exit message";
    m_mutex_exit.notify();
    m_server_plain.stop();
    m_server_tls.stop();
    {
      lock_guard<mutex> guard(m_action_lock);
      m_actions.push(action(EXIT, connection_hdl()));
    }
    m_action_cond.notify_one();
  } else {
    BOOST_LOG_TRIVIAL(error) << "error wait_exit_message";
  }
}

