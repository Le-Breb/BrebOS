#pragma once

#include "syscalls.h"

// Mostly taken from linux

namespace mlibc {
	// C++ wrappers for the extern "C" functions.
	inline sc_word_t do_nargs_syscall(int sc) {
		return __do_syscall0(sc);
	}
	inline sc_word_t do_nargs_syscall(int sc, sc_word_t arg1) {
		return __do_syscall1(sc, arg1);
	}
	inline sc_word_t do_nargs_syscall(int sc, sc_word_t arg1, sc_word_t arg2) {
		return __do_syscall2(sc, arg1, arg2);
	}
	inline sc_word_t do_nargs_syscall(int sc, sc_word_t arg1, sc_word_t arg2, sc_word_t arg3) {
		return __do_syscall3(sc, arg1, arg2, arg3);
	}
	inline sc_word_t do_nargs_syscall(int sc, sc_word_t arg1, sc_word_t arg2, sc_word_t arg3,
			sc_word_t arg4) {
		return __do_syscall4(sc, arg1, arg2, arg3, arg4);
	}
	inline sc_word_t do_nargs_syscall(int sc, sc_word_t arg1, sc_word_t arg2, sc_word_t arg3,
			sc_word_t arg4, sc_word_t arg5) {
		return __do_syscall5(sc, arg1, arg2, arg3, arg4, arg5);
	}
	inline sc_word_t do_nargs_syscall(int sc, sc_word_t arg1, sc_word_t arg2, sc_word_t arg3,
			sc_word_t arg4, sc_word_t arg5, sc_word_t arg6) {
		return __do_syscall6(sc, arg1, arg2, arg3, arg4, arg5, arg6);
	}


	// Type-safe syscall result type.
	enum class sc_result_t : sc_word_t { };

	// Cast to the argument type of the extern "C" functions.
	inline sc_word_t sc_cast(long x) { return x; }
	inline sc_word_t sc_cast(const void *x) { return reinterpret_cast<sc_word_t>(x); }

	template<typename... T>
	sc_result_t do_syscall(int sc, T... args) {
		return static_cast<sc_result_t>(do_nargs_syscall(sc, sc_cast(args)...));
	}

	inline int sc_error(sc_result_t ret) {
		auto v = static_cast<sc_word_t>(ret);
		if(static_cast<unsigned long>(v) > -4096UL)
			return -v;
		return 0;
	}

	// Cast from the syscall result type.
	template<typename T>
	T sc_int_result(sc_result_t ret) {
		auto v = static_cast<sc_word_t>(ret);
		return v;
	}

	template<typename T>
	T *sc_ptr_result(sc_result_t ret) {
		auto v = static_cast<sc_word_t>(ret);
		return reinterpret_cast<T *>(v);
	}
} // namespace mlibc
