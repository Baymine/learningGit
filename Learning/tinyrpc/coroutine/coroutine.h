#ifndef TINYRPC_COROUTINE_COROUTINE_H
#define TINYRPC_COROUTINE_COROUTINE_H

#include<memory>
#include<functional>

namespace tinyrpc{
    int getCoroutineIndex();

    class Coroutine{
    public:
        typedef std::shared_ptr<Coroutine> ptr;

    private: // 私有化默认构造函数  
        Coroutine();
    
    public:
        Coroutine(int size, char* stack_ptr);

        Coroutine(int size, char* stack_ptr, std::function<void()> cb);

        ~Coroutine();
    }
}

#endif