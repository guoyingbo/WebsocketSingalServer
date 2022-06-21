//
// Created by gyb on 6/15/22.
//

#include "message_queue.h"
#include <boost/interprocess/ipc/message_queue.hpp>
#include <iostream>
#include <boost/log/trivial.hpp>
using namespace boost::interprocess;

#define MS_NAME_TASK "message_queue_signal_server"

struct message_queue_pack{
  message_queue *mq = nullptr;
  ~message_queue_pack(){
    if  (mq) delete mq;
  }
};

#define mq_ pack_->mq

struct task_msg
{
  int exit = 0;
};


MessageQueue::MessageQueue(bool create): create_(create),pack_(new message_queue_pack){
  try {
    if (create_) {
      message_queue::remove(MS_NAME_TASK);
      mq_ = new message_queue(create_only,MS_NAME_TASK,10,sizeof(task_msg));;
    } else {
      mq_ = new message_queue(open_only,MS_NAME_TASK);
    }
  } catch (interprocess_exception& ie) {
    mq_ = nullptr;
    BOOST_LOG_TRIVIAL(info) << "MessageQueue()" << ie.what();
  }

}

MessageQueue::~MessageQueue() {
  if (create_)
    message_queue::remove(MS_NAME_TASK);
  delete pack_;
}

bool MessageQueue::WaitExitMessage() {
  task_msg msg;
  size_t size;
  unsigned int p;

  while(mq_ != nullptr) {
    try {
      mq_->receive(&msg, sizeof(msg), size, p);
      if (msg.exit){
        return true;
      }

    }
    catch (interprocess_exception&ie) {
      return false;
    }
  }
  return  false;
}

bool MessageQueue::SendExitMessage() {
  if (mq_) {
    try {
      task_msg msg;
      msg.exit = 1;
      return mq_->try_send(&msg,sizeof(msg),1);
    } catch (interprocess_exception&ie) {
      BOOST_LOG_TRIVIAL(info) << "SendExitMessage()" << ie.what();
      return false;
    }
  }
  return false;
}
