//
// Created by gyb on 6/15/22.
//

#ifndef WSSIGNALSERVER_MESSAGE_QUEUE_H
#define WSSIGNALSERVER_MESSAGE_QUEUE_H


struct message_queue_pack;

class MessageQueue {
public:
  explicit MessageQueue(bool create);
  ~MessageQueue();

  bool WaitExitMessage();
  bool SendExitMessage();

private:
  bool create_;
  message_queue_pack *pack_;
};


#endif //WSSIGNALSERVER_MESSAGE_QUEUE_H
