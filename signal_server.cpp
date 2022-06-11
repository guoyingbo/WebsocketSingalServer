#include "signal_server.h"
#include <map>

#include <json/reader.h>

namespace {
  const char kSignal[] = "signal";
  const char kName[] = "name";
  const char kAdded[] = "added";
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
  Json::Reader reader;
  Json::Value jinput;
  reader.parse(message, jinput);

  std::string type = jinput[kSignal].asString();

  if (type == kSignIn)
  {
    ProcessSignIn(hdl, jinput);
  }
  else if(type == kSignOut)
  {
    ProcessSignOut(hdl, jinput);
  }
  else if (type == "message")
  {
    ProcessMessage(hdl, jinput);
  }

}

void SignalServer::OnClose(connection_hdl hdl)
{
  std::unique_lock<std::mutex> lock(m_mutex_peers);
  if (m_map_peers.count(hdl))
  {
    Peer p = m_map_peers[hdl];
    Json::Value jreturn;
    jreturn[kSignal] = kSignOut;
    jreturn[kID] = p.id;
    printf("--disconnect:%d %s\n", p.id, p.name.data());
    m_map_peers.erase(hdl);
    this->Broadcast(jreturn.toStyledString());
    PrintPeers();
    return;
  }
}


void SignalServer::PrintPeers()
{
  std::cout << "--------peer list----------\n";
  for (auto p : m_map_peers)
  {
    std::cout << p.second.id << ":" << p.second.name << std::endl;
  }
  std::cout << "---------------------------\n\n";
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
     p.id = m_last_id + 1;
     while (IsExist(p.id))
     {
       p.id = p.id + 1;
       if (p.id < 0)
         p.id = 0;
     }
     m_last_id = p.id;

     Json::Value jreturn;
     jreturn[kSignal] = kSignIn;
     jreturn[kID] = p.id;
     jreturn[kName] = p.name;
     this->Broadcast(jreturn.toStyledString());

     jreturn[kSignal] = "return";
     jreturn["request"] = "sign_in";
     jreturn["status"] = "ok";
     Json::Value peers;
     for (auto &pa : m_map_peers)
     {
       Json::Value pv;
       pv["name"] = pa.second.name;
       pv["id"] = pa.second.id;
       peers.append(pv);
     }
     jreturn["peers"] = peers;
     this->Send(jreturn.toStyledString(), hdl);

     m_map_peers[hdl] = p;
     printf("--sign in:%d %s\n", p.id,p.name.data());
     PrintPeers();
}

void SignalServer::ProcessSignOut(connection_hdl hdl, Json::Value& value)
{
  int id = value[kID].asUInt();
  m_map_peers.erase(hdl);

  Json::Value jreturn;
  jreturn[kSignal] = "return";
  jreturn["request"] = "sign_out";
  jreturn["status"] = "ok";

  jreturn.clear();
  jreturn[kSignal] = kSignOut;
  jreturn[kID] = id;

  this->Broadcast(jreturn.toStyledString());
  printf("--sign out:%d\n", id);
  PrintPeers();
}

void SignalServer::ProcessMessage(connection_hdl hdl, Json::Value& value)
{
  int id = value["to"].asUInt();
  connection_hdl hdl_to = GetConnectionFromID(id);

  this->Send(value.toStyledString(), hdl_to);
  

}

bool SignalServer::IsExist(int id)
{
  std::unique_lock<std::mutex> lock(m_mutex_peers);
  for (auto& p : m_map_peers)
  {
    if (p.second.id == id)
    {
      return true;
    }
  }
  return false;
}



connection_hdl SignalServer::GetConnectionFromID(int id)
{
  std::unique_lock<std::mutex> lock(m_mutex_peers);
  for (auto& p : m_map_peers)
  {
    if (p.second.id == id)
    {
      return p.first;
    }
  }

  return connection_hdl();
}
