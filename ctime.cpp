#include <iostream>
#include <chrono>
#include <thread>
#include <random>
#include <algorithm>
#include <mutex>
#define CONSOLE_OUT std::cout
#define CONSOLE_IN std::cin
std::chrono::steady_clock MASTER_CLOCK; // global monotonic clock for multi-threaded app
std::mutex LOCK_CONSOLE_OUT;

struct thread_timer
{
    std::chrono::time_point<std::chrono::steady_clock> start_time;
    thread_timer() : start_time {MASTER_CLOCK.now()} {};
    ~thread_timer()
    { 
        std::chrono::time_point<std::chrono::steady_clock>end_time {MASTER_CLOCK.now()};
        std::chrono::duration<long long, std::nano> dur_elapsed = end_time-start_time;
        LOCK_CONSOLE_OUT.lock();
        CONSOLE_OUT << "Thread " << std::this_thread::get_id()
            << " timed-out after... " << dur_elapsed.count()
            << " seconds.\n"; 
        LOCK_CONSOLE_OUT.unlock();
    }
};

int randomInt(const int max_duration)
{
    static std::random_device rdevice; // hardware alternative to get random seed
    static std::minstd_rand0 rng;
    static std::uniform_int_distribution<> ldist(1, max_duration);
    rng.seed(rdevice()); // seed rng
    
    static int rnum{};
    return rnum = ldist(rng);
}

int main()
{
    std::vector<std::thread> thread_p(0);
    CONSOLE_OUT << "Enter number of threads to time-out & max duration time-out duration:\n";
    CONSOLE_OUT.flush();
    size_t tnum {};
    int mdur{};
    CONSOLE_IN >> tnum >> mdur;

     // allocate enough space but don't resize - this allows push_back() to work
     // must allocate space to prevent mem reallocation (realloc will cause issues with iterators)
    thread_p.reserve(tnum); 
    auto l_run_thread = [=]()
            {
                thread_timer t1;
                std::this_thread::sleep_for
                    (std::chrono::seconds(randomInt(mdur)));
            };
    
    for (size_t i {0}; i < tnum; ++i)
        thread_p.push_back(std::thread (l_run_thread));

    for (std::thread& thread_to_close : thread_p)
    {
        thread_to_close.join();
    }

    return 0;
}