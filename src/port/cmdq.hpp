#if !defined(PORT_CMDQ_HPP__)
#define PORT_CMDQ_HPP__

#include "tsafeq.hpp"



namespace port {
	static const int CHANGE_PROG = 1;
	static const int STAT = 2;
	static const int STOP = 3;
	struct cmd_t {
		int tag;
		int data0;
		void* data1;
	};

	typedef tsafeque_t< std::mutex, sem_t, cmd_t > cmdq_t;
}

#endif
