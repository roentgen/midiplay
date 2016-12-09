#if !defined(PORT_TSAFEQ_HPP__)
#define PORT_TSAFEQ_HPP__

#include <mutex>
#include <deque>
#include <semaphore.h>
#include <stdlib.h>
#include <assert.h>

namespace port {

	template < typename S > class signaling_t {
		S sig_obj_;
	public:
		signaling_t(int p = 0)
		{
			assert(false);
		}

		~signaling_t() {}

		int post()
		{
			assert(false);
			return 0;
		}

		int wait()
		{
			assert(false);
			return 0;
		}
	};

	template <> class signaling_t< sem_t > {
		sem_t sig_obj_;
	public:
		signaling_t(int p=0)
		{
			int err = sem_init(&sig_obj_, 0, p);
			if (err) {
				perror("sem_init filed");
				assert(false);
			}
		}
	
		~signaling_t()
		{
			sem_destroy(&sig_obj_);
		}

		int post()
		{
			int err = sem_post(&sig_obj_);
			if (err) {
				perror("sem_post filed");
				assert(false);
			}
			return err;
		}
		
		int wait()
		{
			int err = sem_wait(&sig_obj_);
			if (err) {
				perror("sem_wait filed");
				assert(false);
			}
			return err;
		}
	};

#if defined(__OSX__)
	template <> class signaling_t< sem_t* > {
		sem_t* sig_obj_;
	public:
		signaling_t(int p=0)
		{
			sem_unlink("dead");
			sig_obj_ = sem_open("dead", O_CREAT, 0, p);
			if (!sig_obj_) {
				perror("sem_open filed");
				assert(false);
			}
		}
	
		~signaling_t()
		{
			sem_close(sig_obj_);
			sem_unlink("dead");
		}

		int post()
		{
			int err = sem_post(sig_obj_);
			if (err) {
				perror("sem_post filed");
				assert(false);
			}
			return err;
		}
		
		int wait()
		{
			int err = sem_wait(sig_obj_);
			if (err) {
				perror("sem_wait filed");
				assert(false);
			}
			return err;
		}
	};
#endif

	template < typename L, typename S, typename E > class tsafeque_t {
		L lock_;
		signaling_t< S > wait_;
		std::deque< E > q_;
	public:
		tsafeque_t() {}
		~tsafeque_t() {}
		
		size_t size()
		{
			std::lock_guard< L > lock(lock_);
			return q_.size();
		}

		template < typename L1, typename S1, typename E1 > struct wait_func {
			E1 operator()(tsafeque_t* t) { assert(false); }
		};

		template < typename L1, typename S1, typename E1 > struct send_func {
			void operator ()(tsafeque_t* t, E1 s) { assert(false); }
		};
		
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
		
		template < typename E1 > struct wait_func< std::mutex, sem_t*, E1 > {
			E operator ()(tsafeque_t* t)
			{
				t->wait_.wait();
				return t->pop();
			}
		};

		template < typename E1 > struct send_func< std::mutex, sem_t*, E1 > {
			void operator ()(tsafeque_t* t, E1 e)
			{
				t->push(std::move(e));
				t->wait_.post();
			}
		};

		void send(E s) { send_func< L, S, E >()(this, std::move(s)); }
		E wait() { return wait_func< L, S, E >()(this); }
		
	protected:
		E pop()
		{
			std::lock_guard< L > lock(lock_);
			E e = std::move(q_.front());
			q_.pop_front();
			return e;
		}

		void push(E&& e)
		{
			std::lock_guard< L > lock(lock_);
			q_.push_back(e);
		}

	};
		
}

#endif
