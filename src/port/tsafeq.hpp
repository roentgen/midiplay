#if !defined(PORT_TSAFEQ_HPP__)
#define PORT_TSAFEQ_HPP__

#include <mutex>
#include <queue>
#include <semaphore.h>
#include <stdlib.h>
#include <assert.h>

namespace port {

	template < typename S > class signaling_t {
		S sig_obj_;
	public:
		signaling_t(int p = 0)
		{
			static_assert(false, "");
		}

		~signaling_t() {}
		
		int post()
		{
			static_assert(false, "");
			return 0;
		}

		int wait()
		{
			static_assert(false, "");
			return 0;
		}
	};

	template <> signaling_t< sem_t >::signaling_t(int p)
	{
		int err =sem_init(&sig_obj_, 0, p);
		if (err) {
			perror("sem_init filed");
			assert(false);
		}
	}
	
	template <> signaling_t< sem_t >::~signaling_t()
	{
		sem_destroy(&sig_obj_);
	}

	template <> int signaling_t< sem_t >::post()
	{
		int err = sem_post(&sig_obj_);
		if (err) {
			perror("sem_post filed");
			assert(false);
		}
		return err;
	}

	template <> int signaling_t< sem_t >::wait()
	{
		int err = sem_wait(&sig_obj_);
		if (err) {
			perror("sem_wait filed");
			assert(false);
		}
		return err;
	}

	template < typename L, typename S, typename E > class tsafeque_t {
		L lock_;
		signaling_t< S > wait_;
		std::queue< E > q_;
	public:

		size_t size()
		{
			std::lock_guard< L > lock(lock_);
			return q_.size();
		}

		template < typename L1, typename S1, typename E1 > struct wait_func {
			E1 operator()()
			{
				static_assert(false, "need specialise");
			}
		};

		template < typename L1, typename S1, typename E1 > struct send_func {
			void operator ()(E1 s)
			{
				static_assert(false, "need specialise");
			}
		};
		
		void send(E s) { send_func< L, S, E >()(std::move(this, s)); }
		E wait() { return wait_func< L, S, E >()(this); }


		template < typename E1 > struct wait_func< std::mutex, sem_t, E1 > {
			E operator ()(tsafeque_t* t)
			{
				t->wait_.wait();
				return t->pop();
			}
		};

		template < typename E1 > struct send_func< std::mutex, sem_t, E1 > {
			void operator ()(tsafeque_t* t, E1 e)
			{
				t->push(std::move(e));
				t->wait_.post();
			}
		};

		
	protected:
		E pop()
		{
			std::lock_guard< L > lock(lock_);
			return q_.pop_front();
		}

		void push(E&& e)
		{
			std::lock_guard< L > lock(lock_);
			q_.push_back(e);
		}


	};
		
	template < typename E > class tsafeque_t < std::mutex, sem_t, E > {
	public:

	};
}

#endif
