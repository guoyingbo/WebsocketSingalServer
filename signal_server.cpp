#include "signal_server.h"
#include <map>
#include <boost/log/trivial.hpp>
#include <json/reader.h>

namespace {
  const char kSignal[] = "signal";
  const char kName[] = "name";
  const char kMessage[] = "message";
  const char kSignIn[] = "sign_in";
  const char kSignOut[] = "sign_out";
  const char kID[] = "id";
}

SignalServer::SignalServer()
  :m_last_id(-1)
{

}

void SignalServer::OnReceive(connection_hdl hdl, const std::string& message)
{
//  BOOST_LOG_TRIVIAL(info) << "RECV:" << message;
  Json::Reader reader;
  Json::Value jinput;
  if (reader.parse(message, jinput) && jinput.isMember(kSignal))
  {
    std::string type = jinput[kSignal].asString();

    if (type == kSignIn)
    {
      ProcessSignIn(hdl, jinput);
    }
    else if(type == kSignOut)
    {
      ProcessSignOut(hdl, jinput);
    }
    else if (type == kMessage)
    {
      ProcessMessage(hdl, jinput);
    }
    else if (type == "exist")
    {
      ProcessExist(hdl, jinput);
    }
  }



}

void SignalServer::OnClose(connection_hdl hdl)
{
  int pid = -1;
  std::string text;
  {
    std::lock_guard<std::mutex> lock(m_mutex_peers);
    if (m_map_peers.count(hdl))
    {
      Peer p = m_map_peers[hdl];
      Json::Value jreturn;
      jreturn[kSignal] = kSignOut;
      jreturn[kID] = p.id;
      BOOST_LOG_TRIVIAL(info) <<"--disconnect:"<<p.id<<" "<< p.name;
      m_map_peers.erase(hdl);
      pid = p.id;
      text = jreturn.toStyledString();
      PrintPeers();
    }
  }

  if (pid != -1)
  {
    int id = RemovePairID(pid);
    if (id != -1)
    {
      connection_hdl hdl_p = GetConnectionFromID(id);
      if (!hdl_p.expired())
        this->Send(text, hdl_p);
    }
  }

}

#ifdef WIN32
#include "versionhelpers.h"
#endif // WIN32


int SignalServer::NextID()
{
  int id = m_last_id + 1;
  if (id >= INT_MAX)
    id = 0;

  while (IsExist(id))
  {
    id += 1;
    if (id >= INT_MAX)
      id = 0;
  }

  m_last_id = id;
  return id;
}

void SignalServer::PrintPeers()
{

  bool bShortSegment = true;
  std::streamsize sz1 = 5;
  std::streamsize sz2 = 43;
#ifdef WIN32
  bShortSegment = IsWindows8OrGreater();
#endif
 

  std::stringstream ss_out;
  if (bShortSegment)
  {
    ss_out << "┌──────┬──────────────────────────────────────────────┐\n";
    ss_out << "│ id   │ name                                         │\n";
    ss_out << "├──────┼──────────────────────────────────────────────┤\n";
  }
  else
  { 
    ss_out <<"┌───┬────────────────────────┐\n";
    ss_out <<"│ id   │ name                                           │\n";
    ss_out <<"├───┼────────────────────────┤\n";
    sz2 = 45;
    sz1 = 5;
  }

  std::map<int, Peer> map_sort_peers;

  for (auto p : m_map_peers)
  {
    map_sort_peers[p.second.id] = p.second;
  }

  for (auto p: map_sort_peers)
  {
    ss_out << std::left << "│ " << std::setw(sz1) << p.second.id << "│ "
      << std::setw(sz2) << p.second.name << "  "<< "│\n";
  }

  if (bShortSegment)
  {
    ss_out << "└──────┴──────────────────────────────────────────────┘\n";
  }
  else
    ss_out <<"└───┴────────────────────────┘\n";

  BOOST_LOG_TRIVIAL(info) <<"peer list\n"<< ss_out.str() <<"  \n";
  boost::log::core::get()->flush();
}

void SignalServer::Broadcast(const std::string& text)
{
  for (auto& p : m_map_peers)
  {
    this->Send(text, p.first);
  }
}

void SignalServer::ProcessSignIn(connection_hdl hdl, Json::Value& value)
{
    Peer p;
     p.name = value[kName].asString();
     p.hdl = hdl;
     p.id = NextID();
    
     Json::Value jreturn;
     jreturn[kID] = p.id;
     jreturn[kName] = p.name;
 //    this->Broadcast(jreturn.toStyledString());

     int same_id = IsExist(p.name);

     jreturn[kSignal] = "return";
     jreturn["request"] = "sign_in";
     jreturn["status"] = "ok";
     jreturn["repeat"] = (same_id != -1);
     if (!g_ice_server.uri.empty())
     {
       Json::Value ice;
       ice["uri"] = g_ice_server.uri;
       ice["username"] = g_ice_server.username;
       ice["password"] = g_ice_server.password;
       jreturn["ice"] = ice;
     }
     
     if (value.isMember("nolist"))
     {
       std::lock_guard<std::mutex> lock(m_mutex_peers);
       m_map_peers[hdl] = p;
     }
     else
     {
       std::lock_guard<std::mutex> lock(m_mutex_peers);
       Json::Value peers;
       for (auto &pa : m_map_peers)
       {
           Json::Value pv;
           pv["name"] = pa.second.name;
           pv["id"] = pa.second.id;
           peers.append(pv);
       }
       m_map_peers[hdl] = p;
       jreturn["peers"] = peers;
     }

     this->Send(jreturn.toStyledString(), hdl);

     //printf("--sign in:%d %s\n", p.id,p.name.data());
     BOOST_LOG_TRIVIAL(info) << "--sign in:" << p.id<<" "<<p.name;
     PrintPeers();
}

void SignalServer::ProcessSignOut(connection_hdl hdl, Json::Value& value)
{
  int id = value[kID].asInt();

  {
      std::lock_guard<std::mutex> lock(m_mutex_peers);
      m_map_peers.erase(hdl);
  }

  Json::Value jreturn;
  jreturn[kSignal] = "return";
  jreturn["request"] = "sign_out";
  jreturn["status"] = "ok";

  this->Send(jreturn.toStyledString(),hdl);

  jreturn.clear();
  jreturn[kSignal] = kSignOut;
  jreturn[kID] = id;

  int pid = RemovePairID(id);
  if (pid != -1)
  {
    connection_hdl hdl = GetConnectionFromID(pid);
    if (!hdl.expired())
      this->Send(jreturn.toStyledString(), hdl);
  }
//  this->Broadcast(jreturn.toStyledString());
//  printf("--sign out:%d\n", id);
  BOOST_LOG_TRIVIAL(info) << "--sign out:"<<id;
  PrintPeers();
}

void SignalServer::ProcessMessage(connection_hdl hdl, Json::Value& value)
{
  int id = value["to"].asInt();
  connection_hdl hdl_to = GetConnectionFromID(id);
  if (!hdl_to.expired())
  this->Send(value.toStyledString(), hdl_to);
  
  if (value.isMember("type") && value["type"].asString() == "offer")
  {
    Pair p;
    p.from = value["from"].asInt();
    p.to = id;
    m_vPairID.push_back(p);
  }
}

void SignalServer::ProcessExist(connection_hdl hdl, Json::Value& value)
{
  std::string name = value["name"].asString();
  int id = IsExist(name);

  Json::Value jreturn;
  jreturn[kSignal] = "return";
  jreturn["request"] = "exist";

  if (id >= 0)
  {
    jreturn["id"] = id;
    jreturn["exist"] = true;
  }
  else
  {
    jreturn["exist"] = false;
  }

  this->Send(jreturn.toStyledString(), hdl);
}

bool SignalServer::IsExist(int id)
{
  std::lock_guard<std::mutex> lock(m_mutex_peers);
  for (auto& p : m_map_peers)
  {
    if (p.second.id == id)
    {
      return true;
    }
  }
  return false;
}



int SignalServer::IsExist(const std::string& name)
{
  std::lock_guard<std::mutex> lock(m_mutex_peers);
  for (auto& p : m_map_peers)
  {
    if (p.second.name == name)
    {
      return p.second.id;
    }
  }
  return -1;
}

int SignalServer::RemovePairID(int id)
{
  auto itb = m_vPairID.begin();
  auto ite = m_vPairID.end();
  for (; itb != ite; itb++)
  {
    if (itb->from == id || itb->to == id)
    {
      int pid = itb->from == id ? itb->to : itb->from;
      BOOST_LOG_TRIVIAL(info) << "remove pair:" << itb->from << ":" << itb->to;
      m_vPairID.erase(itb);
      return pid;
    }
  }

  return -1;
}

connection_hdl SignalServer::GetConnectionFromID(int id)
{
  std::lock_guard<std::mutex> lock(m_mutex_peers);
  for (auto& p : m_map_peers)
  {
    if (p.second.id == id)
    {
      return p.first;
    }
  }

  return connection_hdl();
}

ICE g_ice_server;