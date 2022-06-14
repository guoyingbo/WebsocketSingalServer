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
  std::lock_guard<std::mutex> lock(m_mutex_peers);
  if (m_map_peers.count(hdl))
  {
    Peer p = m_map_peers[hdl];
    Json::Value jreturn;
    jreturn[kSignal] = kSignOut;
    jreturn[kID] = p.id;
    std::cout <<"--disconnect:"<<p.id<<" "<< p.name<<"\n";
    m_map_peers.erase(hdl);
    this->Broadcast(jreturn.toStyledString());
    PrintPeers();
    return;
  }
}


void SignalServer::PrintPeers()
{
  char same1 = '+';
  char same2 = '+';
  char tbuffer[128];
  auto t = std::time(nullptr);
  std::strftime(tbuffer,sizeof(tbuffer),"%Y/%m/%d %H:%M:%S\n",
          std::localtime(&t));
  std::cout << "------------------"<<tbuffer;
  for (auto p : m_map_peers)
  {
    bool have = true;
    if (!m_connections.count(p.first))
    {
      have = false;
      same1 = '-';
    }

    std::cout << p.second.id << ":" << p.second.name << " "<<(have?"+":"-")<< std::endl;
  }
  if (m_connections.size() != m_map_peers.size())
    same2 = '-';
  std::cout << "------------------------- "<<same2<<same1<<"\n\n";
  std::cout.flush();
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

    {
        std::lock_guard<std::mutex> lock(m_mutex_peers);
        for (auto &pa : m_map_peers)
        {
            Json::Value pv;
            pv["name"] = pa.second.name;
            pv["id"] = pa.second.id;
            peers.append(pv);
        }
        m_map_peers[hdl] = p;
    }

     jreturn["peers"] = peers;
     this->Send(jreturn.toStyledString(), hdl);

     //printf("--sign in:%d %s\n", p.id,p.name.data());
     std::cout << "--sign in:" << p.id<<" "<<p.name<<"\n";
     PrintPeers();
}

void SignalServer::ProcessSignOut(connection_hdl hdl, Json::Value& value)
{
  int id = value[kID].asUInt();

  {
      std::lock_guard<std::mutex> lock(m_mutex_peers);
      m_map_peers.erase(hdl);
  }

  Json::Value jreturn;
  jreturn[kSignal] = "return";
  jreturn["request"] = "sign_out";
  jreturn["status"] = "ok";

  jreturn.clear();
  jreturn[kSignal] = kSignOut;
  jreturn[kID] = id;

  this->Broadcast(jreturn.toStyledString());
//  printf("--sign out:%d\n", id);
  std::cout << "--sign out:"<<id<<"\n";
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
