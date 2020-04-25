#include <iostream>
#include <cassert>
#include <time.h>
#include <variant>
#include <random>
#include <chrono>
#include <algorithm>
#include <string>
#include <fstream>
#include <tuple>
#include <mutex>
#include <thread>
#include <map>
#include "cache.hh"
#include "evictor.hh"

//Source: https://en.cppreference.com/w/cpp/chrono/treat_as_floating_point
using FpMilliseconds = std::chrono::duration<float, std::chrono::milliseconds::period>;

/*
  Created by: Maxx Curtis and Casey Harris, for 
  CSCI 389 HW #5: 'Lies, Benchmarks, and Statistics'

  Contains workload generator and latency measurement test for cache_client.cc
*/

// CONSTANTS
//const int WORKLOAD_REQUEST_COUNT = 10000;         //Used for workload, which isn't currently present.
const int NTHREAD = 4;
const int NREQ_COUNT = 100000 / NTHREAD;             //Integer division is fine
const int CACHE_SIZE = 1024;
const int GETPROB = 67;
const int SETPROB = 98;   // SETPROB is 100 <-> (desired prob). It's the top of the range.
                    // Don't need a DELPROB, it's just in an else statement.
const std::string HOST = "127.0.0.1";
const std::string PORT = "3618";

std::mutex key_mutex;
std::mutex get_mutex;
std::mutex time_mutex;
std::mutex duration_vector_mutex;

// Global Variables
std::vector<std::string> keys_in_use;
std::vector<double> request_durations;
double total_gets = 0;          //Effectively an int, but typed for division purposes.
int get_hits = 0;



//-------------------------------------------------------------Cache Calling Functions------------------------------------------------//

void
cache_set(Cache& items, key_type name, Cache::val_type data, Cache::size_type size)
{
    // Used to handle type conversion for 'data' and assert results, 
    // but now it's just a wrapper for cache.set()

    items.set(name, data, size);
}

void 
cache_get(Cache& items, key_type key)
{
    // Performs cache.get and records its hit ratio

    total_gets += 1;
    Cache::size_type got_item_size = 0;
    Cache::val_type got_item = items.get(key, got_item_size);
    if (got_item != nullptr) {
        get_hits += 1;
        //std::cout << key << " gotten successfully\n";
    }
    else {
        //std::cout << "Falied to get " << key << "\n";
        // TODO: Simulate backend server refilling cache?
    }
}

void 
cache_del(Cache& items, key_type key)
{
    // Used to assert the results, but is now just a wrapper for cache.del()

    bool delete_success = items.del(key);
    if (delete_success) {
        std::cout << "Deleted " << key << " from the cache.\n";
    }
    else {
        std::cout << "Invalid delete on item " << key << "\n";
    }
}

//-----------------------------------------------------Helper Functions for Generator-------------------------------------------------//

key_type
select_key_get()
{
    // Uses an exponential distribution of Lambda 6 to select keys for get()
    // This is meant to imitate the high temporal locality of certain keys in large data caches

    std::minstd_rand0 generator(std::chrono::system_clock::now().time_since_epoch().count());
    std::exponential_distribution<double> distribution(6);
    auto rand_num = std::min((distribution(generator) / 2), 1.);   // Min to bound our output between 0 and 1
    int corr_index = static_cast<int>(rand_num * keys_in_use.size());
    //This prevents out of bounds indexing.
    if(rand_num == 1.){
        corr_index -= 1;
    }
    assert(corr_index < static_cast<int>(keys_in_use.size()) && corr_index >= 0 && "Invalid index generated!\n");
    return keys_in_use[corr_index];
}

key_type
select_key_del()
{
    // Uses an inverse exponential distribution of Lambda 6 to select keys for del()
    // This is meant to account for the fact that very popular keys cannot be replenished once they are
    // deleted, which would drastically decrease hit rate, so we make it very unlikely for that to happen

    std::minstd_rand0 generator(std::chrono::system_clock::now().time_since_epoch().count());
    std::exponential_distribution<double> distribution(6);
    auto rand_num = 1. - (std::min((distribution(generator) / 2), 1.));   // Min to bound our output between 0 and 1
    int corr_index = static_cast<int>(rand_num * keys_in_use.size());
    assert(corr_index < static_cast<int>(keys_in_use.size()) && corr_index >= 0 && "Invalid index generated!\n");
    return keys_in_use[corr_index];
}

bool 
useRealKey(int fail_prob)
{
    // Helper for generate_request() that returns 'true'
    // if a key from the cache should be used in the request

    int use_real_key_prob = rand() % 100;
    if (use_real_key_prob > fail_prob) {
        return true;
    }
    else {
        return false;
    }
}

std::string
generate_random_data(int& data_length_return)
{
    // Helper for generate_request(), only called during a 'set'
    // Produces a string of variable length comprised of letters 'a' - 'z'

    // Size Ranges:
    //  2-8: 95%
    //  10-45: 4%
    //  100-115: 1%

    int data_length = 1;
    int size_prob = rand() % 100;
    if (size_prob < 95) {
        data_length = rand() % 6 + 2;
    }
    else if (size_prob < 99) {
        data_length = rand() % 35 + 10; 
    }
    else {
        data_length = rand() % 15 + 100; 
    }
    data_length_return = data_length;

    std::string data = "";
    while(data_length > 1) {                         
        char char_ascii = rand() % 26 + 97;     //Outputs something between 'a' and 'z'.
        data.push_back(char_ascii);
        data_length -= 1;
    }
    return data;
}

key_type
generate_random_key(int length)
{
    // Helper function only called during a 'set'
    // Generates a key of len 'length' comprised of letters 'A' - 'Z'

    std::string key = "";
    while(length > 1) {                          
        char char_ascii = rand() % 26 + 65;     //Outputs something between 'A' and 'Z'.
        key.push_back(char_ascii);
        length = length - 1;
    }
    key_type return_key(key);
    return return_key;
}

/*
    NOTE: Original helper function for generate_request()
    Randomly selects an existing key using uniform probability

key_type
select_key() 
{

    int rand_index = rand() % keys_in_use.size();
    key_type active_key = keys_in_use[rand_index];
    key_touch_count[rand_index] += 1;
    return active_key;
}
*/

//------------------------------------------------------------Generator/Warmup--------------------------------------------------------//

// Generate a random request for the cache
std::vector<std::variant<std::string, int>> 
generate_request()
{
    // Returns a vector of variants, which can be either a string or an int
    // The first element is always the name of the request to be called
    // The following elements of the vector match the function call for the desired request

    std::vector<std::variant<std::string, int>> random_request;
    int op_prob = rand() % 100;

    /*
    Probability Details:
    Total range: 0-99
    0 <-> (GETPROB - 1): Get
    GETPROB <-> (SETPROB - 1): Delete
    SETPROB <-> 99: Set
    */

    ///////////////////// GET /////////////////////
    if (op_prob < GETPROB) {
        
        random_request.push_back("get");
        int fail_prob = 10;

        if (useRealKey(fail_prob)) {
            key_type active_key = select_key_get();
            random_request.push_back(active_key);
        }
        else {
            // Return a request to get a dummy key (which will never exist in the cache)
            std::string dummy_key = "-A";
            random_request.push_back(dummy_key);
        }
        return random_request;
    }
    //////////////////// SET //////////////////////
    else if (op_prob > SETPROB) { // Note that this is >, not <
        
        //std::cout << "Making a set request!\n";
        int data_length;
        std::string rand_data = generate_random_data (data_length); // NOTE: Takes a reference variable for data_length
        
        random_request.push_back("set");
        random_request.push_back(rand_data);

        int fail_prob = 2;
        key_type set_key;

        if (useRealKey(fail_prob)) {
            // Select a key from our list to set the value of
            set_key = select_key_get();           
        }
        else {
            // Return a request to set a brand new key, and add that key to our list.
            // Note: Keys created by this function are 8 characters, whereas keys
            //      created during warmup are length 10. This is a debugging tool to
            //      distinguish different key origins.
            set_key = generate_random_key(8);
            key_mutex.lock();
            keys_in_use.push_back(set_key);
            key_mutex.unlock();
        }

        random_request.push_back(set_key);
        random_request.push_back(data_length);
        return random_request;
    }
    ///////////////// DEL ///////////////////////
    else {
        int fail_prob = 90;
        random_request.push_back("delete");

        if (useRealKey(fail_prob)) {
            // Select a key from our list to delete
            key_type active_key = select_key_del();
            random_request.push_back(active_key);
        }
        else {
            // Return a request to delete a non-existant key
            std::string dummy_key = "-A";
            random_request.push_back(dummy_key);
        }
        return random_request;
    }
}

void
cache_warmup(Cache& items) {
    // Set a bunch of values to the cache, to fill it up
    int size_of_data_generated = 0;
    while (size_of_data_generated < CACHE_SIZE) { // Fills the cache
        
        int set_chance = rand() % 100; // 10% chance to 'get' instead of 'set'
        if (set_chance > 10 || keys_in_use.size() == 0) {
            int data_length;
            std::string test_data = generate_random_data (data_length); // NOTE: Takes a reference variable for data_length
            key_type new_key = generate_random_key(10);
            Cache::val_type rand_data = test_data.c_str();

            cache_set(items, new_key, rand_data, data_length);

            size_of_data_generated += data_length;
            keys_in_use.push_back(new_key);
        }
        else {
            key_type key = select_key_get();
            cache_get(items, key);
        }
    }
    std::cout << "\n----------------------Warmup Complete-----------------------\n\n";
    return;
}

//-----------------------------------------------------Latency Test Helper Functions (PART II)----------------------------------------//

void
make_text_file( std::map<double, int>& timings )
{
    // Takes a reference to an ordered map containing:
    //  The duration that a request took (key)
    //  and the number of requests that took precisely that long (value)
    // Records these values in a .dat file to be used in a histogram/scatter plot

    std::ofstream outfile ("timing_data.dat");
    for (auto i : timings) {
        outfile << i.first << "\t" << i.second << std::endl;
    }
    outfile.close();
}

std::tuple<double, double>
baseline_performance(std::map<double, int>& times_map, double avg_time) 
{
    // Takes a map of latency times : request counts
    // and the recorded average time per request

    // Returns the 95th Percentile Latency
    // and the Mean Throughput (requests per second)

    int dist_from_back = times_map.size() * 0.05;
    auto it = times_map.rbegin();
    for (int i = 0; i < dist_from_back; i++) {
        ++it;
    }
    double top_five_percentile = it->first;
    double reqs_per_second = 1000. / avg_time;      // Avg time is in milliseconds
    std::tuple <double, double> results (top_five_percentile, reqs_per_second);
    return results;
}

//-----------------------------------------------------------------Latency test-------------------------------------------------------//

void
baseline_latencies(int nreq, double& total_time, std::map<double, int>& times_map)
{
    Cache items(HOST, PORT);
    // Returns a vector of latency times, one per request
    // Takes a reference variable that records the total latency time across all requests
        // (used to later calculate mean time per request)

    std::cout << "Beginning latency test\n";

    // total_gets and get_hits are now totals for individual threads.
    //double total_gets = 0.;
    //int get_hits = 0;

    std::vector<double> nreq_timings;

    for (int i = 0; i < nreq; i++) {

        std::vector<std::variant<std::string, int>> new_req = generate_request();

        auto start = std::chrono::high_resolution_clock::now();
        auto stop = std::chrono::high_resolution_clock::now();

        if (std::get<std::string>(new_req[0]) == "set") {
            // Vector contents: {"set", key_type key, val_type data, size_type size}
            Cache::val_type data = std::get<std::string>(new_req[1]).c_str();
            key_type key = std::get<std::string>(new_req[2]);
            Cache::size_type size = std::get<int>(new_req[3]);

            start = std::chrono::high_resolution_clock::now();
            items.set(key, data, size);
            stop = std::chrono::high_resolution_clock::now();

        }
        else if (std::get<std::string>(new_req[0]) == "get") {
            // Vector contents: {"get", key_type key}

            key_type key = std::get<std::string>(new_req[1]);
            Cache::size_type val_size;
            Cache::val_type get_result;
            
            start = std::chrono::high_resolution_clock::now();
            get_result = items.get(key, val_size);
            stop = std::chrono::high_resolution_clock::now();

            get_mutex.lock();

            total_gets += 1;
            if (get_result != nullptr) {
                get_hits += 1;
            }

            get_mutex.unlock();
            
        }
        else if (std::get<std::string>(new_req[0]) == "delete") {
            // Vector contents: {"get", key_type key}

            key_type key = std::get<std::string>(new_req[1]);

            start = std::chrono::high_resolution_clock::now();
            items.del(key);
            stop = std::chrono::high_resolution_clock::now();
        }

        // Note that implicit conversion is allowed here
		auto time_raw = FpMilliseconds(stop - start).count();
		int time_cutoff = static_cast<int>(time_raw * 100 + .5);
        double time = static_cast<double>(time_cutoff) / 100;
        // std::cout << time << "\n";
        nreq_timings.push_back(time);

        //Mutex locking for output parameter modification (which are monotonically increasing)
        time_mutex.lock();

        total_time += time;

        auto find_key = times_map.find(time);
        if (find_key == times_map.end()) {
            times_map.emplace(time, 1);
        }
        else {
            times_map [find_key->first] += 1;
        }

        time_mutex.unlock();
        
    }

    //When the thread finishes, append its results to the global results list
    duration_vector_mutex.lock();
    request_durations.insert(request_durations.end(), nreq_timings.begin(), nreq_timings.end());
    duration_vector_mutex.unlock();

	items.~Cache();
}

//------------------------------------------------------------------MAIN--------------------------------------------------------------//

int main(int argc, char** argv)
{
    Cache disposable_cache(HOST, PORT); //Used exclusively for warmup
    cache_warmup(disposable_cache);
    disposable_cache.~Cache();
    /*
        Main takes a single parameter, which determines what kind of test the program runs

        "work" produces a basic workload test (Part 1), which generates WORKLOAD_REQUEST_COUNT
            requests for cache_client.cc and records the hit rate for gets.

        "measure" runs a performance test (Parts 2 & 3) comprised of NREQ_COUNT requests,
            and prints the mean throughput (in requests per second), the 95th percentile
            latency for requests (ms), hit rate for gets, and the average time per request (ms)
    */
    
    if (argc < 2) {
        std::cout << "No parameter given!\n";
        std::cout << "'work' == run a workload test\n";
        std::cout << "'measure' == run a latency test\n";
    }

    srand (time(NULL));
    std::cout << "Beginning request generation!\n";

    if (std::string(argv[1]) == "work") {
        std::cout << "Currently commented out; try \"measure\".\n";
        /*
        for (int i = 0; i < WORKLOAD_REQUEST_COUNT; i++) {

            std::vector<std::variant<std::string, int>> new_req = generate_request();

            if (std::get<std::string>(new_req[0]) == "set") {
                assert(new_req.size() == 4 && "'set' request generated with the wrong number of elements!\n");
                // Vector contents: {"set", key_type key, val_type data, size_type size}
                Cache::val_type data = std::get<std::string>(new_req[1]).c_str();
                key_type key = std::get<std::string>(new_req[2]);
                Cache::size_type size = std::get<int>(new_req[3]);
                cache_set(items, key, data, size);
            }
            else if (std::get<std::string>(new_req[0]) == "get") {
                assert(new_req.size() == 2 && "'get' request generated with the wrong number of elements!\n");
                // Vector contents: {"get", key_type key}

                key_type key = std::get<std::string>(new_req[1]);
                cache_get(items, key);
            }
            else if (std::get<std::string>(new_req[0]) == "delete") {
                assert(new_req.size() == 2 && "'del' request generated with the wrong number of elements!\n");
                // Vector contents: {"get", key_type key}

                key_type key = std::get<std::string>(new_req[1]);
                cache_del(items, key);
            }
        }
        */
    }

    //---------------------------------------------------------Multithreading Stuff---------------------------------------------------//

    else if (std::string(argv[1]) == "measure") {
        double total_time = 0.;
        std::map<double, int> times_map;
        std::vector<std::thread> thread_vector;

        // auto thread_lambda = [&items, &total_time, &times_map]()
        //  {baseline_latencies(items, NREQ_COUNT, total_time, times_map);};
        auto start = std::chrono::high_resolution_clock::now();
        
        //Threading start
        for (int i = 0; i < NTHREAD; i++) {

            thread_vector.emplace_back(std::thread(
                baseline_latencies, NREQ_COUNT, std::ref(total_time), std::ref(times_map)));
            //std::vector<double> latency_times = baseline_latencies(items, NREQ_COUNT, total_time);
        }
        //Join threads
        
        for (int i = 0; i < NTHREAD; i++) {
            thread_vector[i].join();
        }

        auto stop = std::chrono::high_resolution_clock::now();
        total_time = FpMilliseconds(stop - start).count();
        
        //Threading end
        thread_vector.clear();
        make_text_file(times_map);  //Create output text file
		std::cout << "Total time taken: " << total_time << "\n";
		double avg_time = total_time / (NREQ_COUNT * NTHREAD);
        std::tuple<double, double> performance_stats = baseline_performance(times_map, avg_time);
        std::cout << "95th% latency: " << std::get<0>(performance_stats) << "\n";
        std::cout << "Reqs per second: " << std::get<1>(performance_stats) << "\n";
        std::cout << "Average time per request: (Comparison) " << avg_time << "\n";
        std::cout << "Handled " << NTHREAD << " threads...\n";
    }
    else {
        std::cout << "No parameters given!\n";
        std::cout << "Try 'work' for workload test or 'measure' for latency test";
    }

    if (total_gets == 0) {
        std::cout << "There were no gets\n";
    }
    else {
        double get_hit_ratio = get_hits / total_gets;
        std::cout << "get() hit ratio: " << get_hit_ratio << "\n";
    }
    return 0;
}