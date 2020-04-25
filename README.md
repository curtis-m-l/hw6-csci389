# Homework 6: Crank It Up
## By Casey Harris and Maxx Curtis

## Part 1: Multithreaded Client

- One important change for this project is that, instead of the client making a new
connection to the server for every request, it now connects on creation of a client
cache object and disconnects on destruction. This produced a five or six time speed
increase even before the multithreading started.

- Similarly, the previous structure of our code means that it was easiest for us to
make a new client cache for each thread, rather than trying to multithread a single
client cache object. Since these objects all connect to the same host and port, and
the server only uses one cache, this does not affect performance.

- We made a set of global mutexes in test_generate_workload.cc that manage different
resources shared between threads. key_mutex is used whenever we append data to the
global list keys_in_use (in essence, whenever we set a brand new key in the cache).
get_mutex locks access to the global variables tracking get requests and get hits,
which determine the cache's get hit ratio. duration_vector_mutex is used to prevent
overlap when caches append their individual request timing vectors to the global one,
which occurs at the end of a thread.

- The last mutex, time_mutex, is used to determine the timings of individual requests
and how many requests took a certain length of time. However, this functionality
is never used in this project, since we are now graphing different parameters.

- Our random distributions that determine which key we get or delete do use the size
of keys_in_use into account, but race conditions would only ever result in an
underestimation of the size of the vector (since the vector is strictly appended to,
never deleted from). This means that, even if it reports an inaccurate size, the
function will never attempt to access a non-existant element, so the functionality
is preserved and no mutex lock is needed.

- Warming up the cache is done using a dummy client cache, which is destructed
immediately after. This is to avoid repeatedly warming up the cache as a byproduct
of multithreading. Since all client caches pull their keys from a global list, as
the size of that list grows, the hit rate drops. In addition, repeated warmups are
slower and prone to competition, since appending to the global list is mutex locked.

- Aside from changes to enable connections on construction, nothing has changed in
cache_client.cc -- all multithreading was done in test_generate_workload.

### Graphs

- We fixed server at one thread, then increased the number of client threads (maximum
8). 

- We found that requests per second leveled off pretty fast, since server was the
bottleneck. Interestingly, two client threads were faster than one, which we attribute
to the client having computation to do between requests, while the server doesn't.

- The 95th percentile latency grew noticably as the number of client threads increased.
This makes sense, given that all of the requests are queueing for the response of
one server thread, meaning some might be in that queue for a longer time when there
are more clients making simultaneous requests.

- For 95% latency, threads = 7 is somewhat of an outlier. We ran a few trials, but
every time it was much higher than any of the others. We're not sure if this is due
to actual causes or just random variation.

## Part 2: Multithreaded Server

- Since we used Beast for this project, the server already had most of a multithreading
framework in place. We've added a mutex cache_mutex to ensure that all communication
with the server-side cache is serialized, but we've made no other changes.

### Graphs

- This time, we fixed client threads at 4 (since the test machine was 4-core) and
varied the number of server threads between 1 and 8. Also, we did end up using the
same machine for the client and server, which might have slowed the process down
somewhat.

- The requests per second measurement leveled off at 4 threads, which makes sense.
I believe that, given the performance of the single-threaded server, the saturation
point for a server thread is slightly more than one client thread produces, so when
the server thread count is greater than or equal to the client thread count, client
threads become the bottleneck for requests per second.

- The 95th percentile latency was noisier this time around, with notable outliers at
threads = 3 and threads = 7. I don't think that increasing the server thread count
affected the highest end of request times all that much, since those requests might
be bottlenecked by other factors (like retrieving the data from the computer's actual
memory).