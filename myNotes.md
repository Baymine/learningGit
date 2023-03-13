# 协程模块：实现协程创建、协程切换等功能，提供给外部用户协程接口。
   1. 实现的是非对称协程，其中的一个主协程用于帮助协程切换
   2. 主协程不需要申请堆空间，直接使用的是线程栈
## 协程
- 本项目中实现的是有栈非对称协程
  - 协程拥有自己的调用栈
- 基于非对称协程的设计，一般都需要一个主协程。通常主协程就直接使用线程所在的栈，而新的协程需要额外申请从堆中申请内存空间来作为其执行时的栈空间。
- 任何切换到新协程的操作都必须由主协程来执行。
  - 必须在当前协程是主协程的情况下才行唤醒其他目标协程。（Resume）
  - 挂起协程的目的就是为了切换到主协程。（Yield）
## 实现
- 成员
  - 协程id
  - 协程寄存器上下文
  - 协程栈空间大小
    - 以字节为单位。在创建协程时会调用 malloc 函数从堆上申请该大小的空间作为协程的栈空间。
  - 协程需要执行的回调函数
- 构造
  - 非对称协程需要一个主协程帮助协程切换
    - 主协程只需要构造一个Coroutine对象即可
  - 一个线程一般有两种协程，一个是主协程，一个是当前正在执行的协程
    - 用户协程
      - 申请堆内存，并且赋值给指针sp（堆底，指向这段空间的最低地址）
      - 将栈顶指针8字节对齐之后，将对应的地址赋值给结构体中对应的寄存器即可
      - 设置回调函数
        - 调用完回调函数之后将当前协程挂起
- 切换
  - 在主协程中进行切换，主要是将对应的上下文进行交换
- 挂起
  - 将主协程的上下文切换回来

# 协程HOOK模块：hook网络函数如read、write、accept、connect等。
## Hook函数
- 在hook之后，我们自己提供了一份 read函数，如何保证程序加载的是我们Hook后的read函数，而不是系统自带的read函数呢。
   > 在linux下，如果多个动态库有同名的全局符号的情况下，会加载第一个动态库中的符号，而忽略后面的动态库中的同名符号。因此，我们需要修改动态链接顺序，只要保证自己的库在glibc库之前被加载就可以了，最好把自己的库放在最前面。

![](https://pic2.zhimg.com/v2-8ae32235af862c1bc8c564ca758dae39_b.jpg)

# Reactor模块:基于epoll系列函数实现Reactor框架。
![主从Reactor](https://pic1.zhimg.com/v2-f8e630c4c2adf3ab6fae6b5e682c7d98_b.jpg)
- 结构（连接能够快速建立）
  - 主Reactor
    - 负责注册listenfd可可读事件，只负责新连接的创建，完成创建之后交给SubReactor
  - 从Reactor
    - 负责数据交换，监听可读、可写事件
## Reactor
  - TinyRPC 是多线程模型，根据`one loop per thread`的理念。主线程就是 MainReactor，而每个 IO 线程就是一个 SubReactor。
    - `one loop per thread`理念指的是以线程为单位进行编程，即每个线程负责一个循环，而不是将某些任务分配给不同的线程去执行。
  - 也就是说，对于 read 协程来看，它的执行流都是同步的，但是性能却是异步的！ 即同步的写法达到异步的性能，这也是协程的最大好处。有了这个，写代码就容易多了。就像写最简单的阻塞式编程一样，一路直接 accept、read、write 就行了，根本不要考虑注册什么回调。协程 hook 配合 Reactor 已经完全帮我们做好了。
    - 在原先实现异步的过程需要设置回调函数
> 潜在的难点：如何实现一个线程安全的Reactor
> 要分析可能出现线程不安全的地方
> - 获取回调函数的时候（加入到任务队列中）
>   - 监听到事件发生的时候，主epoll单纯地给事件设置回调函数（之后将读写事件添加到任务队列中）
- 读写回调函数的注册时机
  - 封装在函数`toEpoll`中
  - 回调内容--唤醒原先挂起的协程 `tinyrpc::Coroutine::Resume(cur_cor);`
  - 当epoll_wait返回时，Reactor会唤醒对应的协程
    - ![](https://pic2.zhimg.com/v2-8ae32235af862c1bc8c564ca758dae39_b.jpg)
    - 从read协程的视角来看，这个过程是同步的，但是性能缺失异步的（等待的时间并没有被浪费）

# 定时器模块：实现基本的定时器，结合reactor模块使用。
## **定时器**
- 作用
  - 事件循环是阻塞的
    - 引入定时器机制，当超时的时候就会调用对应的回调函数，然后事件循环再继续阻塞系统调用上
    - 检查长时间不活跃的连接
      - 为每个连接设置一个定时器（epoll会自动监听定时器任务）
        - 当操作事件发生的时候，为发生事件的连接添加事件
        - 当接收到超时事件的时候，
- 任务队列，队列中每个元素由两部分组成：执行时间点 + 执行函数。
- 如何在给定时间触发对应事件？
  - 将epoll_wait中的超时时间设置为任务队列中最早的执行时间
    - 进入epoll_wait之前，找出定时任务队列中时间最早的，将执行时间设置为epoll的超时时间，这样epoll_wait超时之后这个任务就会被执行了
  - 利用Linux的新增特征，timefd（**项目中使用的方法**）
    - 注册到epoll中
    - 当到时间之后，这个fd就会发生可读事件，
    - 之后epoll_wait返回，去执行其上绑定的定时任务
- 唤醒
  - 使用Linux中提供的eventfd， 直接创建一个这种 fd 并注册到 epoll_wait 上即可。当我们需要唤醒 epoll_wait 的时候，往这个 fd 里面写入数据即可。
  - > Eventfd是Linux内核中的一种用于通信和同步的机制，它可以让**内核和用户空间之间进行传递数据**。Eventfd通过一个文件描述符，允许应用程序使用文件I/O来监视和触发事件。


## 利用时间轮处理无效TCP连接
- 服务器维持一个 TCP 连接大约占用哪些资源: `文件描述符`、`内核收发缓冲区`、`CPU资源`、`内存资源`
  - 如果在应用层再在 TCP 连接做了一些封装，还可以会占用应用缓冲区等。
  - 如果注册到 EPOLL 上，也会加大 epoll 的监听负载。
- 分解问题
  - 服务器如何识别一个连接是无效连接
  - 服务器如何断开这个连接
    - 写半关闭，让服务器自动关闭TCP连接
      - 服务器主动断开进入到`CLOSE_WAIT`状态
        - > 处于 TCP_TIMEWAIT 状态的连接所消耗的资源可比一个处于 TCP_CONNECTED 的连接大多了
- 原理
  - 每隔一个固定的时间移动一次指针（**不需要显式的指针**，直接删除队头元素，加入的时候往队尾加入）
  - 将当前指向的指针的槽中对应的所有的元素全部初始化（删除原先的-触发回调函数，新建）
  - 结构
    - 一个队列，其中的元素为向量

# Tcp模块：包含基本的TcpServer、TcpClient、TcpConnection、TcpBuffer、NetAddress等类的封装。
## TCP server
- 作用
  - accept新连接
  - 将新连接分发给IO线程
  - 处理定时任务等
- TcpServer有两个协程，一个是主协程，执行reactor的循环，另一个accept的协程，会不断循环的去 accept，每当 accept 返回时就取出这个连接，交给某一个 IO 线程（也即 SubReactor）
  - ![TCPServer 执行流示意图](https://picx.zhimg.com/v2-c062128de509d5817742c19d15b556af_1440w.jpg?source=d16d100b)
  - TinyRPC 是主从 Reactor 架构。TcpServer 本身是 MainReactor, 每个 IO 线程是一个SubReactor.

## TCP Connection
### IO线程
本质上就是一个Reactor，一个 IO 线程会管理多个客户端连接对象，即 TcpConnection 对象
- mian
  - 创建Reactor，一个线程只能有一个Reactor对象(One thread per loop)
- addClient
  - `std::map<int, std::shared_ptr<TcpConnection>> m_clients;`  fd(accept返回的)-->connection

### TCP Connection
每一个 TcpConnection 对象代表一个客户端连接
- 分成两类，一种是RPC服务端，一种是RPC客户端
  - 服务端
    - `m_loop_cor`协程中绑定MainServerLoopCorFunc函数，执行读、处理、写（服务端的基本逻辑）
      - 读（先对缓存进行检查）
        - 调用read从socket的内核缓冲区中读取数据，然后放到自己的应用层缓冲区（vector）
        - 更新连接的时间轮
      - 处理
        - > 独立章节
      - 写
        - 调用 write 函数将写缓冲区的数据写入到 socket 上，然后由内核负责发送给对端。
      - ![](https://github.com/Gooddbird/tinyrpc/blob/main/imgs/input.drawio.png?raw=true)
  - 客户端
#### 处理
##### 过程
  - 根据不同的协议类型指定数据类型（多态，两种协议，，）
  - 解码 `m_codec->decode(m_read_buffer.get(), data.get())`;
  - 服务端则对数据进行分发
  - 客户端就将这个数据放到回复队列中
##### 步骤
  - 将请求数据包**解码**生成事件
  - 根据协议类型**分发**事件
  - 事件业务逻辑处理
  - **编码**生成响应数据包
##### 编码模块（TinnyPB）
TinyPB 协议是基于 protobuf 的一种自定义协议，主要是加了一些必要的字段如 错误码、RPC 方法名、起始结束标志位等
- 编码
  - 结构体(TinyPBStruct)-->字符串(TinyPB协议格式的字符串)
- 解码
  - 字符串-->结构体
- AbstractCodec----重写--->encode,decode(字符串解析操作)
##### 事件分发器
> **事件分发器**（Event Dispatcher）是一种常见的事件处理模型，它的核心思想是将事件的处理过程分离出来，由事件分发器负责管理事件的分发和处理。事件分发器通常包含一个事件循环，<u>它不断地从事件队列中取出事件，并将事件分发给相应的事件处理器进行处理。</u>  
> >将事件的处理过程与事件的产生过程分离开来

抽象类AbstractDispatcher(`virtual void dispatch(AbstractData* data, TcpConnection* conn) = 0;`)
- 流程
  - 解析服务器名称
    - 从map中，根据`string`得到`tinyrpc::TinyPbRpcDispacther::service_ptr`
  - 获取方法名称
    - 也是从string开始，得到的类别是`google::protobuf::MethodDescriptor*`
  - 根据 method 对象反射出 request 和 response 对象
    - `google::protobuf::Message* request = service->GetRequestPrototype(method).New();`
  - 设置`RPC_controller`(调用信息，方法名称，服务器全名)
  - 调用服务器中的方法
    - `service->CallMethod(method, &rpc_controller, request, response, &closure);`
    - 这里会调用原先注册到服务器中的方法
  - 将回应(response)序列化成string
  - 编码响应数据包，回送给 TcpConnection 对象
##### 事件处理机制
![](https://github.com/Gooddbird/tinyrpc/blob/main/imgs/execute.drawio.png?raw=true)
- 整个事件处理都在一个协程中的完成的，业务函数如query_age函数的执行也是在一个协程中运行的。
- 每一个TC连接持有一个协程
- 问题：
  - 读写操作都是网络IO操作，可以在协程中高效执行，但是业务处理在协程中执行的好处是什么？

## Servlet & CGI
> - CGI ，是通过 fork and exec 对应路径下的 CGI 程序并且启动后进行处理。不过 CGI 最大的诟病是每次请求要 fork 一次，尽管 fcgi 的出现一定程度解决了这个问题，但还是有一定的性能丢失。
> - Servlet 就是一段子程序，它接收 HTTP 请求，返回 HTTP 响应。
>   - it will listen for incoming HTTP requests and route them to the appropriate serverlet based on the requested URL.
>   - 它接收 HTTP 请求，返回 HTTP 响应。Servlet 需要被注册到一个路径下，如 /user，当浏览器访问这个路径时，就会找到对应的 Servlet 程序。

> the framework will listen for incoming HTTP requests and dispatch those requests to an appropriate handler based on the URL requested and the HTTP method used. The handler will then generate an appropriate HTTP response and send it back to the client.

- serverlet--自定义链接地址对应一些响应(callback function)  -- 抽象服务端
  - 虚拟接口
  - serverletDispatch
    - 管理和维护所有serverlet之间的关系(用URI区匹配对应的serverlet)
    - 成员
      - unordered_map（精准匹配） --- string ： serverlet::ptr
      - vector(模糊匹配) -- 所有相似的路径
        - "/sylar/*"
        - 使用的是fnmatch -- 检查模式参数是否匹配
      - default severlet: no match URL
    - 一种特殊的serverlet，重载的handler是为了确定使用哪一个serverlet
    - 添加serverlet（写入vector或者map ）
      - 存在线程安全的问题
        - 加上写锁
    - 删除serverlet
    - get serverlet
    - 404 NotfoundServerlet

- 项目中定义的Servlet
  - BlockCallTttpServlet
    - 直接`stub.query_age(&rpc_controller, &rpc_req, &rpc_res, NULL);`然后等待相应返回
  - NonBlockCallTttpServlet
  - BlockCallTttpServlet



# RPC模块：基于Tcp的进一步封装，预计使用Protobuf进行序列化实现RPC。

## 流程
### 定义TinyPB
> TinyPB 是 TinyRPC 框架自定义的一种轻量化协议类型，它是基于 google 的 protobuf 而定制的，读者可以按需自行对协议格式进行扩充。
```cpp
/*
**  min of package is: 1 + 4 + 4 + 4 + 4 + 4 + 4 + 1 = 26 bytes
*/
char start;                         // 代表报文的开始， 一般是 0x02
int32_t pk_len {0};                 // 整个包长度，单位 byte
int32_t msg_req_len {0};            // msg_req 字符串长度
std::string msg_req;                // msg_req,标识一个 rpc 请求或响应。 一般来说 请求 和 响应使用同一个 msg_req.
int32_t service_name_len {0};       // service_name 长度
std::string service_full_name;      // 完整的 rpc 方法名， 如 QueryService.query_name
int32_t err_code {0};               // 框架级错误代码. 0 代表调用正常，非 0 代表调用失败
int32_t err_info_len {0};           // err_info 长度
std::string err_info;               // 详细错误信息， err_code 非0时会设置该字段值
std::string pb_data;                // 业务 protobuf 数据，由 google 的 protobuf 序列化后得到
int32_t check_num {0};             // 包检验和，用于检验包数据是否有损坏
char end;                           // 代表报文结束，一般是 0x03
```
- 为什么需要这样一个协议
  - 防止粘包，当多个报文以数据流的形式发送的时候，可以通过这样的报头中的信息获取各个报文的边界

### 定义protobuf文件
### 生成pb桩文件
这两个文件包含了一些 RPC 主要类，如: queryAgeReq、QueryService、QueryService_Stub。
### 实现RPC方法
生成的QueryService是一个抽象类，需要重写业务函数（query_name...），实际上就是设置相关业务逻辑，然后设置返回状态和一些返回信息，然后调用其中的回调函数（原先的代码中需要利用request中的id到MySQL中查询用户信息，然后返回response对象）
### 配置文件
coroutine_stack_size = 256, coroutine_pool_size=1000
time_wheel, bucket_num=3

### 实现RPC服务
```cpp
tinyrpc::InitConfig(argv[1]);

tinyrpc::GetServer()->registerService(std::make_shared<QueryServiceImpl>());

tinyrpc::StartRpcServer();

```
### 修改 HTTP 服务实现异步 RPC 调用
调用的方式是阻塞的，但是在调用中，协程会进行切换，当调用完成之后，会resume当前协程然后继续运行，所以是以同步的写法实现了异步的性能

# 其他模块：如日志、线程池、互斥锁等模块。

## 使用
- 调用
  - 阻塞协程式异步调用
    - 同步的代码，实现异步的性能（不需要回调函数和新建线程）
    - 当请求远程方法的时候，线程不会阻塞在这里，而是去处理其他协程
      - 当前协程会切换到主协程
      - 但是对于当前协程是阻塞的，只有被唤醒之后才能继续下面的操作
  - 非阻塞协程式异步调用(使用于不依赖RPC调用结果的场景)
    - 类似`std::future`
    - 协程调用之后会立即返回，不会在这个调用中阻塞
      - 可以调用wait方法，这时候协程会阻塞，直到返回调用结果（遇到wait就阻塞）
    - 原理
      - 新生成一个协程处理这次调用
      - 把这个处理用的协程放到调度池任务中，原来的协程继续完成任务
    - 注意事项
      - 所有人RPC相关对象必须是堆上的对象而不是栈上的
        - 调用对象是在线程A中声明的，由于是异步RPC，整个调用过程是在线程B中执行的，所以需要保证在线程B中这些对象还存在
      - 调用RPC之前需要调用saveCalle，增加智能指针的引用次数，防止调用返回之后引用次数将为0而被销毁
        - 防止线程切换导致对象销毁？（上面的双重保险）
    - 具体实现
      - 继承类HttpServlet，重写handel方法
        - HttpServlet中包含对HTTP报头的处理(Not found, set HTTP code etc)
      - 将serverlet注册到对应的目录下
        - 使用的宏定义，用do...while循环，来将几个语句变成一个语句
- TinyPB 协议
  - err_code: 框架级别的错误码
  - service_full_name: 调用的完整方法名

- HTTP模块
  - 每来一个 HTTP 请求就会找到对应的 HttpServlet 对象，执行其提前注册好的业务逻辑函数，用于处理 Http 请求，并回执 Http 响应。


![非阻塞式异步RPC调用](https://raw.githubusercontent.com/Gooddbird/tinyrpc/main/imgs/nonblock_async_call.drawio.png)

- m:n 线程:协程模型
  - 1:n 模型可能会增加请求的时延。例如当某个 IO 线程在处理请求时，耗费了太多的时间，导致此 IO 线程的其他请求得不到及时处理，只能阻塞等待。
  - m:n 即 m 个线程共同调度 n 个协程。由于 m 个线程共用一个协程池，因此协程池里的就绪任务总会尽快的被 Resume。
    - 每一个客户端连接对象 TcpConnection, 对应一个协程。对客户端连接的 读数据、业务处理、写数据这三步，其实都在这个协程中完成的
    - 对于 m:n 协程模型 来说，一个 TcpConnection对象所持有的协程，可能会来回被多个不同的IO线程调度。
  - m:n 模型也引入了更强的线程竞争条件，所以对协程池加互斥锁是必须的。

## 支持的协议报文
1. 纯HTTP1.1
2. TinyPB 协议: 一种基于 Protobuf 的自定义协议，属于二进制协议。
### knowledge


- core dump
  - A core dump contains a snapshot of the program's memory, including any source code that was being executed at the time of the crash, as well as any data that may have been in memory.
  - Core dump是当程序出现异常时，系统将程序正在使用的内存内容储存到一个核心文件中，以便于诊断，调试故障的一个技术。

## questions
- 这里为什么要做内存对齐？因为这段空间会被当成新协程的执行栈空间来说，在栈空间进行 Push或者Pop操作时一般都是以机器字长为单位的，在64位下就是8个字节。也就是说每次Push或者Pop的数据都是8个字节，想一下，如果栈顶指针执行的地址不是8字节的整数倍，Push和Pop是不是很难执行？
- 为什么要用宏定义定义函数，而不是直接定义一个常规函数？
  - > 宏定义的函数用来执行某种特定任务，可提高代码的运行效率，允许在编译时完成大量的处理，而常规函数只能在运行时执行代码，不允许编译时做处理。另外，宏定义函数可以有效地减少重复代码量，提高代码的可读性，使程序更容易理解和维护。
## 项目特点
- 关于智能指针
  - 时间轮
    ```cpp
    // 注意这里并没有去寻找这个 AbstractSlot 的shared_ptr 指针是否已经存在时间轮里面了，而是直接新增了一个 
    // shared_ptr。这是有意为之的，没有必要再去时间轮遍历一下找到之前添加的 shared_ptr，然后再删除，再添加到队尾。既
    //然用了智能指针了，自然是通过 shard_ptr 的自动计数功能，反正当最后一个 shared_ptr 被删除后，这个 slot 对象自然
    //就析构了。这个特性解决了之前需要 O(N) 来遍历的问题。
    void TcpTimeWheel::fresh(TcpConnectionSlot::ptr slot) {
      DebugLog << "fresh connection";
      m_wheel.back().emplace_back(slot);
    }

    // 注意这里必须用 weak_ptr 而不能用 shread_ptr，否则不管时间轮那里有没有这个 slot 对象的 shared_ptr 指针，这个 
    //slot 永远不会销毁，因为 TcpConnection 始终持有一个 shared_ptr，导致引用计数一直大于0。这个就是典型的智能指针
    //循环引用的问题了。
    void TcpConnection::registerToTimeWheel() {
      auto cb = [] (TcpConnection::ptr conn) {
        conn->shutdownConnection();
      };
      TcpTimeWheel::TcpConnectionSlot::ptr tmp = std::make_shared<AbstractSlot<TcpConnection>>(shared_from_this(), cb);
      m_weak_slot = tmp;  // std::weak_ptr<tinyrpc::AbstractSlot<tinyrpc::TcpConnection>> tinyrpc::TcpConnection::m_weak_slot
      m_tcp_svr->freshTcpConnection(tmp);
    }
    ```
- acceptor中的listenfd是否需要设置成非阻塞？
  - > 不一定，在Reactor模型中有一个原则` Robot:宏定义的函数用来执行某种特定任务，可提高代码的运行效率，允许在编译时完成大量的处理，而常规函数只能在运行时执行代码，不允许编译时做处理。另外，宏定义函数可以有效地减少重复代码量，提高代码的可读性，使程序更容易理解和维护。`但是在使用accept的时候，只要有新连接，那就不会阻塞（也就是一般知道listenfd可读之后才调用accept），所以也不一定要设置成非阻塞
  - 为什么要用 SO_REUSEADDR
    - 当服务端意外重启之后，服务器会尝试重新连接原先的地址、端口，如果不可重用的话，这样的连接尝试会失败，因为the operating system would still think the port was in use


# 日志分析
## 启动服务器
`./bin/test_tinypb_server ./conf/test_tinypb_server.xml`
```log
	[DEBUG]	[17346]	[17346]	[0]	[tinyrpc/net/net_address.cc:38]	create ipv4 address succ [127.0.0.1:20000]
	[DEBUG]	[17346]	[17346]	[0]	[tinyrpc/net/tcp/io_thread.cc:35]	semaphore begin to wait until new thread frinish IOThread::main() to init
	[DEBUG]	[17346]	[17349]	[0]	[tinyrpc/net/reactor.cc:39]	thread[17349] succ create a reactor object
	[DEBUG]	[17346]	[17349]	[0]	[tinyrpc/net/reactor.cc:46]	m_epfd = 3
	[DEBUG]	[17346]	[17349]	[0]	[tinyrpc/net/reactor.cc:54]	wakefd = 4
	[DEBUG]	[17346]	[17349]	[0]	[tinyrpc/net/tcp/io_thread.cc:93]	finish iothread init, now post semaphore
	[DEBUG]	[17346]	[17346]	[0]	[tinyrpc/net/tcp/io_thread.cc:39]	semaphore wait end, finish create io thread
	[DEBUG]	[17346]	[17346]	[0]	[tinyrpc/net/tcp/io_thread.cc:35]	semaphore begin to wait until new thread frinish IOThread::main() to init
	[DEBUG]	[17346]	[17350]	[0]	[tinyrpc/net/reactor.cc:39]	thread[17350] succ create a reactor object
	[DEBUG]	[17346]	[17350]	[0]	[tinyrpc/net/reactor.cc:46]	m_epfd = 5
	[DEBUG]	[17346]	[17350]	[0]	[tinyrpc/net/reactor.cc:54]	wakefd = 6
	[DEBUG]	[17346]	[17350]	[0]	[tinyrpc/net/tcp/io_thread.cc:93]	finish iothread init, now post semaphore
	[DEBUG]	[17346]	[17346]	[0]	[tinyrpc/net/tcp/io_thread.cc:39]	semaphore wait end, finish create io thread
	[DEBUG]	[17346]	[17346]	[0]	[tinyrpc/net/tcp/io_thread.cc:35]	semaphore begin to wait until new thread frinish IOThread::main() to init
	[DEBUG]	[17346]	[17351]	[0]	[tinyrpc/net/reactor.cc:39]	thread[17351] succ create a reactor object
	[DEBUG]	[17346]	[17351]	[0]	[tinyrpc/net/reactor.cc:46]	m_epfd = 7
	[DEBUG]	[17346]	[17351]	[0]	[tinyrpc/net/reactor.cc:54]	wakefd = 8
	[DEBUG]	[17346]	[17351]	[0]	[tinyrpc/net/tcp/io_thread.cc:93]	finish iothread init, now post semaphore
	[DEBUG]	[17346]	[17346]	[0]	[tinyrpc/net/tcp/io_thread.cc:39]	semaphore wait end, finish create io thread
	[DEBUG]	[17346]	[17346]	[0]	[tinyrpc/net/tcp/io_thread.cc:35]	semaphore begin to wait until new thread frinish IOThread::main() to init
	[DEBUG]	[17346]	[17352]	[0]	[tinyrpc/net/reactor.cc:39]	thread[17352] succ create a reactor object
	[DEBUG]	[17346]	[17352]	[0]	[tinyrpc/net/reactor.cc:46]	m_epfd = 9
	[DEBUG]	[17346]	[17352]	[0]	[tinyrpc/net/reactor.cc:54]	wakefd = 10
	[DEBUG]	[17346]	[17352]	[0]	[tinyrpc/net/tcp/io_thread.cc:93]	finish iothread init, now post semaphore
	[DEBUG]	[17346]	[17346]	[0]	[tinyrpc/net/tcp/io_thread.cc:39]	semaphore wait end, finish create io thread
	[DEBUG]	[17346]	[17346]	[0]	[tinyrpc/net/reactor.cc:72]	Create new Reactor
	[DEBUG]	[17346]	[17346]	[0]	[tinyrpc/net/reactor.cc:39]	thread[17346] succ create a reactor object
	[DEBUG]	[17346]	[17346]	[0]	[tinyrpc/net/reactor.cc:46]	m_epfd = 11
	[DEBUG]	[17346]	[17346]	[0]	[tinyrpc/net/reactor.cc:54]	wakefd = 12
	[DEBUG]	[17346]	[17346]	[0]	[./tinyrpc/net/timer.h:27]	timeevent will occur at 1677245786758
	[DEBUG]	[17346]	[17346]	[0]	[tinyrpc/net/timer.cc:31]	m_timer fd = 13
	[DEBUG]	[17346]	[17346]	[0]	[tinyrpc/net/reactor.cc:181]	epoll_ctl add succ, fd[13]
	[DEBUG]	[17346]	[17346]	[0]	[tinyrpc/net/timer.cc:63]	need reset timer
	[DEBUG]	[17346]	[17346]	[0]	[./tinyrpc/net/timer.h:27]	timeevent will occur at 1677245786758
	[INFO]	[17346]	[17346]	[0]	[tinyrpc/net/tcp/tcp_server.cc:139]	TcpServer setup on [127.0.0.1:20000]
	[INFO]	[17346]	[17346]	[0]	[tinyrpc/comm/config.cc:255]	read config from file [./conf/test_tinypb_server.xml]: [log_path: ./], [log_prefix: test_tinypb_server], [log_max_size: 5 MB], [log_level: DEBUG], [coroutine_stack_size: 256 KB], [coroutine_pool_size: 1000], [msg_req_len: 20], [max_connect_timeout: 75 s], [iothread_num:4], [timewheel_bucket_num: 6], [timewheel_inteval: 10 s], [server_ip: 127.0.0.1], [server_Port: 20000], [server_protocal: TINYPB]

	[INFO]	[17346]	[17346]	[0]	[tinyrpc/net/tinypb/tinypb_rpc_dispatcher.cc:167]	succ register service[QueryService]!
	[DEBUG]	[17346]	[17346]	[0]	[./tinyrpc/net/timer.h:27]	timeevent will occur at 1677245777258
	[DEBUG]	[17346]	[17346]	[0]	[tinyrpc/net/timer.cc:63]	need reset timer
	[DEBUG]	[17346]	[17346]	[0]	[tinyrpc/net/tcp/tcp_server.cc:36]	create listenfd succ, listenfd=14
	[DEBUG]	[17346]	[17346]	[0]	[tinyrpc/net/tcp/tcp_server.cc:58]	set REUSEADDR succ
	[INFO]	[17346]	[17346]	[0]	[tinyrpc/coroutine/memory.cc:14]	succ mmap 262144000 bytes memory
	[INFO]	[17346]	[17346]	[0]	[tinyrpc/net/tcp/tcp_server.cc:149]	resume accept coroutine
	[DEBUG]	[17346]	[17346]	[1]	[tinyrpc/coroutine/coroutine_hook.cc:113]	this is hook accept
	[DEBUG]	[17346]	[17346]	[1]	[tinyrpc/net/fd_event.cc:133]	succ set o_nonblock
	[DEBUG]	[17346]	[17346]	[1]	[tinyrpc/coroutine/coroutine_hook.cc:43]	fd:[14], register read event to epoll
	[DEBUG]	[17346]	[17346]	[1]	[tinyrpc/net/reactor.cc:181]	epoll_ctl add succ, fd[14]
	[DEBUG]	[17346]	[17346]	[1]	[tinyrpc/coroutine/coroutine_hook.cc:140]	accept func to yield
	[DEBUG]	[17346]	[17349]	[0]	[tinyrpc/net/tcp/io_thread.cc:102]	IOThread 17349 begin to loop
	[DEBUG]	[17346]	[17351]	[0]	[tinyrpc/net/tcp/io_thread.cc:102]	IOThread 17351 begin to loop
	[DEBUG]	[17346]	[17350]	[0]	[tinyrpc/net/tcp/io_thread.cc:102]	IOThread 17350 begin to loop
	[DEBUG]	[17346]	[17352]	[0]	[tinyrpc/net/tcp/io_thread.cc:102]	IOThread 17352 begin to loop
```

## 发送请求
```log
	[DEBUG]	[17346]	[17346]	[1]	[tinyrpc/net/fd_event.cc:66]	delete succ
	[DEBUG]	[17346]	[17346]	[1]	[tinyrpc/net/reactor.cc:181]	epoll_ctl add succ, fd[14]
	[DEBUG]	[17346]	[17346]	[1]	[tinyrpc/coroutine/coroutine_hook.cc:146]	accept func yield back, now to call sys accept
	[INFO]	[17346]	[17346]	[1]	[tinyrpc/net/tcp/tcp_server.cc:93]	New client accepted succ! port:[50860
	[DEBUG]	[17346]	[17346]	[1]	[tinyrpc/net/net_address.cc:45]	ip[], port[50860
	[INFO]	[17346]	[17346]	[1]	[tinyrpc/net/tcp/tcp_server.cc:113]	New client accepted succ! fd:[17, addr:[127.0.0.1:44230]
	[DEBUG]	[17346]	[17346]	[1]	[tinyrpc/net/tcp/tcp_server.cc:238]	fd 17did't exist, new it
	[DEBUG]	[17346]	[17346]	[1]	[tinyrpc/net/tcp/tcp_connection.cc:30]	succ create tcp connection[2], fd=17
	[DEBUG]	[17346]	[17346]	[1]	[tinyrpc/net/tcp/tcp_server.cc:177]	tcpconnection address is 0x5568ddb17bb0, and fd is17
	[DEBUG]	[17346]	[17346]	[1]	[tinyrpc/net/tcp/tcp_server.cc:186]	current tcp connection count is [1]
	[DEBUG]	[17346]	[17346]	[1]	[tinyrpc/coroutine/coroutine_hook.cc:113]	this is hook accept
	[DEBUG]	[17346]	[17346]	[1]	[tinyrpc/net/fd_event.cc:126]	fd already set o_nonblock
	[DEBUG]	[17346]	[17346]	[1]	[tinyrpc/coroutine/coroutine_hook.cc:43]	fd:[14], register read event to epoll
	[DEBUG]	[17346]	[17346]	[1]	[tinyrpc/net/reactor.cc:181]	epoll_ctl add succ, fd[14]
	[DEBUG]	[17346]	[17346]	[1]	[tinyrpc/coroutine/coroutine_hook.cc:140]	accept func to yield
	[DEBUG]	[17346]	[17349]	[2]	[tinyrpc/net/tcp/tcp_connection.cc:122]	m_read_buffer size=128rd=0wd=0
	[DEBUG]	[17346]	[17349]	[2]	[tinyrpc/coroutine/coroutine_hook.cc:66]	this is hook read
	[DEBUG]	[17346]	[17349]	[2]	[tinyrpc/net/fd_event.cc:133]	succ set o_nonblock
	[DEBUG]	[17346]	[17349]	[2]	[tinyrpc/net/tcp/tcp_connection.cc:127]	m_read_buffer size=128rd=0wd=92
	[DEBUG]	[17346]	[17349]	[2]	[tinyrpc/net/tcp/tcp_connection.cc:129]	read data back, fd=17
	[DEBUG]	[17346]	[17349]	[2]	[tinyrpc/net/tcp/tcp_connection.cc:147]	read_count > rt
	[INFO]	[17346]	[17349]	[2]	[tinyrpc/net/tcp/tcp_connection.cc:169]	recv [92] bytes data from [127.0.0.1:44230], fd [17]
	[DEBUG]	[17346]	[17349]	[2]	[tinyrpc/net/tcp/tcp_connection.cc:191]	=====================================
	[DEBUG]	[17346]	[17349]	[2]	[tinyrpc/net/tcp/tcp_connection.cc:193]	buffer size=128
	[DEBUG]	[17346]	[17349]	[2]	[tinyrpc/net/tcp/tcp_connection.cc:199]	GET /nonblock?id=1 HTTP/1.1
Host: 127.0.0.1:20000
User-Agent: curl/7.81.0
Accept: */*
```

## 请求退出
```log
	[DEBUG]	[17346]	[17349]	[2]	[tinyrpc/net/fd_event.cc:66]	delete succ
	[DEBUG]	[17346]	[17349]	[2]	[tinyrpc/net/reactor.cc:181]	epoll_ctl add succ, fd[17]
	[DEBUG]	[17346]	[17349]	[2]	[tinyrpc/coroutine/coroutine_hook.cc:107]	read func yield back, now to call sys read
	[DEBUG]	[17346]	[17349]	[2]	[tinyrpc/net/tcp/tcp_connection.cc:127]	m_read_buffer size=128rd=0wd=92
	[DEBUG]	[17346]	[17349]	[2]	[tinyrpc/net/tcp/tcp_connection.cc:129]	read data back, fd=17
	[DEBUG]	[17346]	[17349]	[2]	[tinyrpc/net/tcp/tcp_connection.cc:136]	rt <= 0
	[ERROR]	[17346]	[17349]	[2]	[tinyrpc/net/tcp/tcp_connection.cc:137]	read empty while occur read event, because of peer close, fd= 17, sys error=Resource temporarily unavailable, now to clear tcp connection
	[DEBUG]	[17346]	[17349]	[2]	[tinyrpc/net/reactor.cc:203]	del succ, fd[17]
	[DEBUG]	[17346]	[17349]	[2]	[tinyrpc/net/tcp/tcp_connection.cc:156]	peer close, now yield current coroutine, wait main thread clear this TcpConnection
	[DEBUG]	[17346]	[17346]	[0]	[tinyrpc/net/tcp/tcp_server.cc:266]	TcpConection [fd:17] will delete, state=4
	[DEBUG]	[17346]	[17346]	[0]	[tinyrpc/net/tcp/tcp_connection.cc:77]	~TcpConnection, fd=17
```

## 服务器关闭
```log
	[ERROR]	[17346]	[17346]	[0]	[tinyrpc/comm/log.cc:41]	progress received invalid signal, will exit
```

## bug
- undefi undefined reference to `google::protobuf
```makefile
# 添加了 -ldl -lprotobuf -pthread 中的-lprotobuf，只在前三个中添加了
$(PATH_BIN)/test_tinypb_server: $(LIB_OUT)
	$(CXX) $(CXXFLAGS) $(PATH_TESTCASES)/test_tinypb_server.cc $(PATH_TESTCASES)/test_tinypb_server.pb.cc -o $@ $(LIB_OUT) $(LIBS) -ldl -lprotobuf -pthread $(PLUGIN_LIB)

$(PATH_BIN)/test_tinypb_server_client: $(LIB_OUT)
	$(CXX) $(CXXFLAGS) $(PATH_TESTCASES)/test_tinypb_server_client.cc $(PATH_TESTCASES)/test_tinypb_server.pb.cc -o $@ $(LIB_OUT) $(LIBS) -ldl -lprotobuf -pthread $(PLUGIN_LIB)

$(PATH_BIN)/test_http_server: $(LIB_OUT)
	$(CXX) $(CXXFLAGS) $(PATH_TESTCASES)/test_http_server.cc $(PATH_TESTCASES)/test_tinypb_server.pb.cc -o $@ $(LIB_OUT) $(LIBS) -ldl -lprotobuf -pthread $(PLUGIN_LIB)
```

## extra
- 使用一个weak_ptr指向当前类可以让你访问或者观察一个对象而不参与其的生命周期，这在观察者模式中非常有用

