//
// Created by aurelien on 3/5/25.
//

#include "thread.h"
#include "../core/fb.h"


Thread::Thread(Process* parent, tid_t tid): quantum(0), k_stack_top(-1),flags(P_READY), state(READY)
{
    if(parent == nullptr)
        printf_error("Thread parent is null");
    this->parent = parent;
    this->tid = tid;
}

void Thread::terminate()
{
    flags |= P_TERMINATED;
}


bool Thread::is_terminated() const
{
    return flags & P_TERMINATED;
}

bool Thread::is_waiting_key() const
{
    return flags & P_WAITING_KEY;
}

tid_t Thread::get_tid()
{
    return tid;
}

void Thread::set_flag(uint flag)
{
    flags |= flag;
}

Process *Thread::get_process() const
{
    return this->parent;
}



