#pragma once
#include "websocket_server.h"
#include <map>
#include "../jsoncpp/json/value.h"
#include <mutex>


class SignalServer : public WebsocketServer
{
public:

  struct Peer
  {
    int id;
    connection_hdl hdl;
    std::string name;
  };
  SignalServer();

  void OnReceive(connection_hdl hdl, const std::string& message) override;
  void OnClose(connection_hdl hdl) override;


private:

  void PrintPeers();

  void Broadcast(const std::string& text);

  void ProcessSignIn(connection_hdl hdl, Json::Value& value);
  void ProcessSignOut(connection_hdl hdl, Json::Value& value);
  void ProcessMessage(connection_hdl hdl, Json::Value& value);

  bool IsExist(int id);

  std::map<connection_hdl, Peer, std::owner_less<connection_hdl>> m_map_peers;
  connection_hdl GetConnectionFromID(int id);

  int m_last_id;
//  std::vector<Peer> m_peer_list;
  std::mutex m_mutex_peers;
};

