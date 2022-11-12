#include <iostream>
#include <chrono>
#include <thread>
#include <random>
#include <algorithm>
#include <mutex>
#include <condition_variable>

/* 
 - This program combines combines knowledge of the chrono and threading libraries
 - The user enters # threads & # max seconds for a thread to run
 - App then randomly generates sleep duration for each thread
 - various locking mechanisms are used to prevent (1) console write race conditions and (2) deadlocks
*/

 // preprocessor substitution for readability
#define CONSOLE_OUT std::cout
#define CONSOLE_IN std::cin
#define CHRONO std::chrono

CHRONO::steady_clock MASTER_CLOCK; // global monotonic clock for multi-threaded app
std::mutex LOCK_CONSOLE_OUT; // used for condition_variable
std::condition_variable con_v;
std::vector<std::thread::id> g_timedout_threads; // holds timedout thread IDs to prevent race condition to console
bool g_bool_lock_aq{true}; // predicate control for wait_for()
std::mutex LOCK_BOOL; // locks predicate control variable used in wait_for()

struct thread_timer
{
    CHRONO::time_point<CHRONO::steady_clock> start_time; // time obj was created
    CHRONO::duration<int, std::milli> wait_time_out; // timeout duration to prevent deadlock

     // only ctor takes time duration used for condition_variable wait
    thread_timer(const CHRONO::duration<int, std::milli> wto) 
        : start_time {MASTER_CLOCK.now()}, wait_time_out {wto} 
            {}
    ~thread_timer()
    { 
         // determine current clock time -> compute time elapsed
        CHRONO::time_point<CHRONO::steady_clock>end_time {MASTER_CLOCK.now()};
        CHRONO::duration<long long, std::nano> dur_elapsed = end_time-start_time;
         // cast duration from nanoseconds -> milliseconds 
        auto c_dur_elapsed = CHRONO::duration_cast<CHRONO::milliseconds>(dur_elapsed);

         // all threads aquire unique_lock prior to race-condition block
        std::unique_lock<std::mutex> ul (LOCK_CONSOLE_OUT);
         // g_bool_lock_aq is defaulted to TRUE -> whatever thread is notified first, will NOT wait
         // as predicate will evaulate to true immediately upon initial call
        con_v.notify_one();
        if (con_v.wait_for(ul, wait_time_out, []()->bool{ return g_bool_lock_aq; }))
        {
             // if NO timeout occured -> lock global bool and set to false to prevent con_v wakeup
            {
                std::lock_guard<std::mutex> lg (LOCK_BOOL); // lock guard deleted at end of block
                g_bool_lock_aq = false;
            }
             // read out thread timer to console
            CONSOLE_OUT << "Thread " << std::this_thread::get_id()
                << " timed-out after... " << c_dur_elapsed.count()
                << " ms.\n"; 
        }
         // lock and set to true prior to notify
        {
            std::lock_guard<std::mutex> lg (LOCK_BOOL); 
            g_bool_lock_aq = true;
        }
        con_v.notify_one(); // notify a waiting thread
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
     // will manually allocate vec mem based on user input
    std::vector<std::thread> thread_p(0);
    CONSOLE_OUT << "Enter number of threads to time-out & max duration time-out duration:\n";
    CONSOLE_OUT.flush();
    size_t tnum {}; // number of threads
    int mdur{}; // max duration a thread could run 
    CONSOLE_IN >> tnum >> mdur;
     // allocate enough space but don't resize - this allows push_back() to work
     // must allocate space to prevent mem reallocation (realloc will cause issues with iterators)
    thread_p.reserve(tnum); 
    g_timedout_threads.reserve(tnum); // worst case - all threads fail
     // compute 120% of max thread time + round in milliseconds for wait_for time-out condition
    auto a_mdur {CHRONO::milliseconds(static_cast<int>(std::ceil(mdur*1.2)*1000))};
    CONSOLE_OUT << "MAX THREAD TIME ALLOCATED: " << a_mdur.count() << "ms" <<  std::endl;
     // lambda to start thread timer & sleep thread based on random int returned
    auto l_run_thread = [=]()
            {
                thread_timer branch_t(a_mdur);
                std::this_thread::sleep_for
                    (CHRONO::seconds(randomInt(mdur)));
            };
    
     // dedicated block to stop thread_timer
    {
        thread_timer main_t(a_mdur);
         // threads run predefined lambda
        for (size_t i {0}; i < tnum; ++i)
            thread_p.push_back(std::thread (l_run_thread));
    
         // join threads prior to main thread termination
        for (std::thread& thread_to_close : thread_p)
            {
                thread_to_close.join();
            }
    }
    
     // read out any threads that reached .wait_for() duration
    if (g_timedout_threads.empty())
    {
        CONSOLE_OUT << "No threads timed out.\n";
    }
    else
    {
        for (const std::thread::id& ids : g_timedout_threads)
            CONSOLE_OUT << "Thread " << ids << " , timed-out during mutex wait.\n";
    }

    return 0;
}