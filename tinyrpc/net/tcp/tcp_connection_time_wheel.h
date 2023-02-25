#ifndef TINYRPC_NET_TCP_TCPCONNECTIONTIMEWHEEL_H
#define TINYRPC_NET_TCP_TCPCONNECTIONTIMEWHEEL_H

#include <queue>
#include <vector>
#include "tinyrpc/net/tcp/abstract_slot.h"
#include "tinyrpc/net/reactor.h"
#include "tinyrpc/net/timer.h"

namespace tinyrpc {

class TcpConnection; // 代表 TCP 连接的类，一个 TcpConnection 对象代表一个 Tcp 连接

class TcpTimeWheel {

 public:
  typedef std::shared_ptr<TcpTimeWheel> ptr;

  // 一个 AbstractSlot对象 与一个 TcpConnection 对象是一一对应的。
  typedef AbstractSlot<TcpConnection> TcpConnectionSlot;

  TcpTimeWheel(Reactor* reactor, int bucket_count, int invetal = 10);

  ~TcpTimeWheel();

  void fresh(TcpConnectionSlot::ptr slot);

  void loopFunc();


 private:
  Reactor* m_reactor {nullptr};
  int m_bucket_count {0};   // 槽数
  int m_inteval {0};    // 时间轮的周期(second)

  TimerEvent::ptr m_event;
  std::queue<std::vector<TcpConnectionSlot::ptr>> m_wheel;  // 时间轮队列
};


}

#endif