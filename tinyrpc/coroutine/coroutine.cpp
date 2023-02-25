#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <atomic>
#include "tinyrpc/coroutine/coroutine.h"
#include "tinyrpc/comm/log.h"
#include "tinyrpc/comm/run_time.h"

namespace tinyrpc
{

    // main coroutine, every io thread have a main_coroutine
    static thread_local Coroutine *t_main_coroutine = NULL;

    // current thread is runing which coroutine
    static thread_local Coroutine *t_cur_coroutine = NULL;

    static thread_local RunTime *t_cur_run_time = NULL;

    // static thread_local bool t_enable_coroutine_swap = true;

    static std::atomic_int t_coroutine_count{0};

    static std::atomic_int t_cur_coroutine_id{1};

    int getCoroutineIndex()
    {
        return t_cur_coroutine_id;
    }

    RunTime *getCurrentRunTime()
    {
        return t_cur_run_time;
    }

    void setCurrentRunTime(RunTime *v)
    {
        t_cur_run_time = v;
    }

    // 直接调用协程的目标函数即可
    void CoFunction(Coroutine *co)
    {
        if (co != nullptr)
        {
            co->setIsInCoFunc(true);

            // 去执行协程回调函数
            co->m_call_back();

            co->setIsInCoFunc(false);
        }

        // here coroutine's callback function finished, that means coroutine's life is over. we should yiled main couroutine
        Coroutine::Yield();  // 当前协程完成任务，切换回主协程
    }

    // void Coroutine::SetCoroutineSwapFlag(bool value) {
    //   t_enable_coroutine_swap = value;
    // }

    // bool Coroutine::GetCoroutineSwapFlag() {
    //   return t_enable_coroutine_swap;
    // }

    // 主协程的构造函数
    // 因为主协程不需要申请堆空间，他直接使用的就是线程的栈。也不需要为主协程的m_coctx设置初值，
    // 因为后面协程切换的时候会保存当前寄存器上下文到主协程的m_coctx中的。
    Coroutine::Coroutine()
    {
        // main coroutine id is 0
        m_cor_id = 0;
        t_coroutine_count++;
        memset(&m_coctx, 0, sizeof(m_coctx));
        t_cur_coroutine = this; // 前正在执行的协程
        // DebugLog << "coroutine[" << m_cor_id << "] create";
    }

    Coroutine::Coroutine(int size, char *stack_ptr) : m_stack_size(size), m_stack_sp(stack_ptr)
    {
        assert(stack_ptr);

        if (!t_main_coroutine)
        {
            t_main_coroutine = new Coroutine();
        }

        m_cor_id = t_cur_coroutine_id++;
        t_coroutine_count++;
        // DebugLog << "coroutine[" << m_cor_id << "] create";
    }

    // 用户协程构造
    Coroutine::Coroutine(int size, char *stack_ptr, std::function<void()> cb)
        : m_stack_size(size), m_stack_sp(stack_ptr)
    {

        assert(m_stack_sp);

        if (!t_main_coroutine)
        {
            t_main_coroutine = new Coroutine();
        }

        setCallBack(cb);
        m_cor_id = t_cur_coroutine_id++;
        t_coroutine_count++;
        // DebugLog << "coroutine[" << m_cor_id << "] create";
    }

    bool Coroutine::setCallBack(std::function<void()> cb)
    {

        if (this == t_main_coroutine)
        {
            ErrorLog << "main coroutine can't set callback";
            return false;
        }
        if (m_is_in_cofunc)
        {
            ErrorLog << "this coroutine is in CoFunction";
            return false;
        }

        m_call_back = cb;

        // assert(m_stack_sp != nullptr);

        // 栈空间的生长方向一般是由高地址向低地址生长的！
        char *top = m_stack_sp + m_stack_size; // 堆最高地址
        // first set 0 to stack
        // memset(&top, 0, m_stack_size);

        // 申请堆起始地址
        // 字节对齐。 因为要作为栈底,所以地址必须是 8字节的整数倍(64位机一个字为8个字节)
        // -16LL其实就是最后4位为0，其余高位都是1
        /**
         * 1.casting the pointer "top" to an unsigned long. This allows us to perform bitwise operations on the
         *  address stored in the pointer.
         * 2.The number -16 is being used as a mask.
         *  This mask will set the last 4 bits of the address to 0, effectively aligning the address to a 16 byte boundary.
         */
        top = reinterpret_cast<char *>((reinterpret_cast<unsigned long>(top)) & -16LL);

        memset(&m_coctx, 0, sizeof(m_coctx));

        m_coctx.regs[kRSP] = top;
        m_coctx.regs[kRBP] = top;

        // This allows the coroutine to jump back to the CoFunction when it is resumed.
        m_coctx.regs[kRETAddr] = reinterpret_cast<char *>(CoFunction); // 函数指针
        m_coctx.regs[kRDI] = reinterpret_cast<char *>(this);           // 将寄存器RDI（第一个参数）设置为当前对象的地址

        m_can_resume = true;

        return true;
    }

    Coroutine::~Coroutine()
    {
        t_coroutine_count--;
        // DebugLog << "coroutine[" << m_cor_id << "] die";
    }

    Coroutine *Coroutine::GetCurrentCoroutine()
    {
        if (t_cur_coroutine == nullptr)
        {
            t_main_coroutine = new Coroutine();
            t_cur_coroutine = t_main_coroutine;
        }
        return t_cur_coroutine;
    }

    Coroutine *Coroutine::GetMainCoroutine()
    {
        if (t_main_coroutine)
        {
            return t_main_coroutine;
        }
        t_main_coroutine = new Coroutine();
        return t_main_coroutine;
    }

    bool Coroutine::IsMainCoroutine()
    {
        if (t_main_coroutine == nullptr || t_cur_coroutine == t_main_coroutine)
        {
            return true;
        }
        return false;
    }

    /********
    form target coroutine back to main coroutine
    ********/
    void Coroutine::Yield()
    {
        // if (!t_enable_coroutine_swap) {
        //   ErrorLog << "can't yield, because disable coroutine swap";
        //   return;
        // }
        if (t_main_coroutine == nullptr)
        {
            ErrorLog << "main coroutine is nullptr";
            return;
        }

        if (t_cur_coroutine == t_main_coroutine)
        {
            ErrorLog << "current coroutine is main coroutine";
            return;
        }
        Coroutine *co = t_cur_coroutine;
        t_cur_coroutine = t_main_coroutine;
        t_cur_run_time = NULL;
        coctx_swap(&(co->m_coctx), &(t_main_coroutine->m_coctx));
        // DebugLog << "swap back";
    }

    /********
    form main coroutine switch to target coroutine
    ********/
    void Coroutine::Resume(Coroutine *co)
    {
        if (t_cur_coroutine != t_main_coroutine)
        {
            ErrorLog << "swap error, current coroutine must be main coroutine";
            return;
        }

        if (!t_main_coroutine)
        {
            ErrorLog << "main coroutine is nullptr";
            return;
        }
        if (!co || !co->m_can_resume)
        {
            ErrorLog << "pending coroutine is nullptr or can_resume is false";
            return;
        }

        if (t_cur_coroutine == co)
        {
            DebugLog << "current coroutine is pending cor, need't swap";
            return;
        }

        t_cur_coroutine = co;
        t_cur_run_time = co->getRunTime();

        coctx_swap(&(t_main_coroutine->m_coctx), &(co->m_coctx));
        // DebugLog << "swap back";
    }

}
