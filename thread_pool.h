#pragma once

// Simple thread pool implementation.
// This implementation works effectively if the amount of threads in a pool is small, < 50.

#include <functional>
#include <thread>
#include <atomic>
#include <vector>
#include <stdexcept>

#include "common/latch.h"

class thread_pool
{

public:

	thread_pool(unsigned pool_size)
		: threads(pool_size)
	{
	}

	void perform(std::function<void()>&& routine)
	{

		for(auto& t : threads)		
		{
			if(t.is_not_busy())
			{
				t.perform(std::move(routine));
				return;
			}
		}
	
		throw out_of_threads();

	}

	class thread;

	thread* get_free_thread(){	
		for(auto& t : threads)		
			if(t.is_not_busy())
				return &t;
		return 0;
	}

	unsigned busy_threads_count()
	{

		unsigned r = 0;
		for(auto& t : threads)		
		{
			if(!t.is_not_busy())
				r++;
		}
	
		return r;

	}

	void wait_until_all_are_finished()
	{
		for(auto& t : threads)		
			t.wait_until_finished();
	}

	// exception
	class out_of_threads : public std::runtime_error
	{
	public:
		out_of_threads() : std::runtime_error("all threads of thread pool are busy") {}
	};

	class thread
	{
	public:

		thread()
			: //log("tpool"), 
			latch(0)
		
		{

			time_to_stop = false;
			std::unique_lock<std::mutex> lock(mutex);
			the_thread = std::thread(std::bind(&thread::thread_func, this));
			condition.wait(lock); // wait until the thread is started		
		}

		~thread()
		{
			time_to_stop = true;
			condition.notify_one();
			the_thread.join();		
		}

		void perform(const std::function<void()>& proutine)
		{
			std::unique_lock<std::mutex> lock(mutex);
			latch.reset(1);
			routine = proutine;
			condition.notify_one();		
		}

		bool is_not_busy()
		{
			return latch.try_wait();
		}

		void wait_until_finished()
		{
			latch.wait();
		}

	private:
		
		//log_channel log;

		// disallow copying
		thread(const thread&) = delete;
		thread& operator=(const thread&) = delete;

		void thread_func()
		{
			try
			{

				std::unique_lock<std::mutex> lock(mutex);

				condition.notify_one();

				while (!time_to_stop)
				{

					condition.wait(lock);
					
					try_drop_a_latch drop_latch(latch); // notify that we're done on scope exit

					if (time_to_stop) break;

					try
					{
						routine();
					}
					catch(std::exception& e)
					{
						//log.error() << "error while performing routine in thread pool: " << e.what();
					}

				}

			}
			catch(std::exception& e)
			{
				//log.error() << "thread exception: " << e.what();
			}
			catch(...)
			{
				//log.error() << "unknown exception in the thread";
			}
		
		}

		class latch latch;

		std::function<void()> routine;

		std::atomic<bool> time_to_stop;
		std::mutex mutex;
		std::condition_variable condition;

		std::thread the_thread;

	};

private:

	std::vector<thread> threads;

};


