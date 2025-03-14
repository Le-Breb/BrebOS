#ifndef THREAD_H
#define THREAD_H
#include "process.h"


class Process;

enum thread_state
{
    READY,
    RUNNING,
    BLOCKED
};
typedef uint tid_t;

//k_stack_top(-1),
class Thread
{
public:
    Thread(Process* parent, tid_t tid);

    void terminate();

    /** Checks whether the thread is terminated */
    [[nodiscard]] bool is_terminated() const;

    /** Checks whether the thread is waiting for a key press */
    [[nodiscard]] bool is_waiting_key() const;

    /** Get the parent process */
    [[nodiscard]] Process* get_process() const;

    /** Get the tid(identifier) of this thread */
    [[nodiscard]] tid_t get_tid();

    void set_flag(uint flag);



    uint quantum;

    //Store the state of the stack and the cpu
    stack_state_t k_stack_state{};
    cpu_state_t k_cpu_state{};

    cpu_state_t cpu_state{};
    stack_state_t stack_state{};

    uint k_stack_top{}; // Top of syscall handlers' stack
    uint flags{}; // thread state

private:

    tid_t tid;


    Process* parent;

    enum thread_state state;
};


#endif //THREAD_H
