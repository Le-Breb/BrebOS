#pragma once

typedef long __sc_word_t;
using sc_word_t = __sc_word_t;

__sc_word_t __do_syscall0(long);
__sc_word_t __do_syscall1(long, __sc_word_t);
__sc_word_t __do_syscall2(long, __sc_word_t, __sc_word_t);
__sc_word_t __do_syscall3(long, __sc_word_t, __sc_word_t, __sc_word_t);
__sc_word_t __do_syscall4(long, __sc_word_t, __sc_word_t, __sc_word_t, __sc_word_t);
__sc_word_t __do_syscall5(long, __sc_word_t, __sc_word_t, __sc_word_t, __sc_word_t,
		__sc_word_t);
__sc_word_t __do_syscall6(long, __sc_word_t, __sc_word_t, __sc_word_t, __sc_word_t,
		__sc_word_t, __sc_word_t);