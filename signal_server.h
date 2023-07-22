#pragma once
#include "websocket_server.h"
#include <map>
#include <json/value.h>
#include <mutex>
struct ICE {
  std::string uri;
  std::string username;
  std::string password;
  ICE() {
    uri = "turn:115.231.220.242:8101?transport=tcp";
    username = "ts1";
    password = "12345678";
  }
};

extern ICE g_ice_server;

class SignalServer : public WebsocketServer
{
public:

  struct Peer
  {
    int id;
    connection_hdl hdl;
    std::string name;
  };

  struct Pair
  {
    int from;
    int to;
  };
  SignalServer();

  void OnReceive(connection_hdl hdl, const std::string& message) override;
  void OnClose(connection_hdl hdl) override;


private:

  int NextID();
  void PrintPeers();

  void Broadcast(const std::string& text);

  void ProcessSignIn(connection_hdl hdl, Json::Value& value);
  void ProcessSignOut(connection_hdl hdl, Json::Value& value);
  void ProcessMessage(connection_hdl hdl, Json::Value& value);
  void ProcessExist(connection_hdl hdl, Json::Value& value);
  bool IsExist(int id);
  int IsExist(const std::string& name);

  int RemovePairID(int id);

  std::map<connection_hdl, Peer, std::owner_less<connection_hdl>> m_map_peers;
  connection_hdl GetConnectionFromID(int id);

  int m_last_id;

  std::vector<Pair> m_vPairID;
  std::mutex m_mutex_peers;
};

