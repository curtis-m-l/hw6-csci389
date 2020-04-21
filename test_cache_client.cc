#include <cassert>
#include <iostream>
#include "cache.hh"
#include "fifo_evictor.hh"

/*

 Test program for the Cache class defined in "cache.hh" and
 implemented in "cache_lib.cc".

 Created by Casey Harris and Maxx Curtis
 for CSCI 389 Homework #2

*/

// Cache::hash_func testHash = [](key_type key)->Cache::size_type {return static_cast<uint32_t>(key[4]);};

std::string host = "127.0.0.1";
std::string port = "3618";

// HELPER FUNCTIONS

void cache_set(Cache& items, Cache::val_type data, std::string name, Cache::size_type size)
{
    std::cout << "Attempting to add item of size " << size << "\n";
    /* Create an item with key 'name', value 'data', and size 'size'. Add it to the cache. */
    Cache::val_type val = data;
    items.set(name, val, size);
    // Can't use asserts in this function, would require get.
    // Asserted in main()
}

void cache_get(Cache& items, key_type key, Cache::size_type& itemSize, Cache::size_type target_size)
{
    Cache::val_type got_item = items.get(key, itemSize);
    // std::cout << "Retrieved Item: " << got_item << "\n";
    // std::cout << "Item size:" << itemSize << "\n";
    assert(got_item != nullptr && "Cache could not retrieve requested item!\n");
    assert(itemSize == target_size && "get() did not update size of its second param correctly!\n");
    std::cout << key << " gotten successfully.\n";
}

void cache_del(Cache& items, key_type key)
{
    bool delete_success = items.del(key);
    assert(delete_success);
    std::cout << "Deleted " << key << " from the cache.\n";
}

void cache_space_used(Cache& items, Cache::size_type target_size)
{
    Cache::size_type used_space = items.space_used();
    std::cout << "Current memory used: " << used_space << " | Expected: " << target_size << "\n";
    assert(used_space == target_size);
}

void cache_reset(Cache& items)
{
    items.reset();
    assert(items.space_used() == 0);
    std::cout << "Cache reset.\n";
}

void cache_get_failure(Cache& items, key_type key, Cache::size_type& itemSize)
{
    Cache::val_type got_item = items.get(key, itemSize);
    assert(got_item == nullptr);
}

// TEST CASES

void test_reduction() {
    // Checks that modifying an object does not prompt a rejection/eviction for some reason
    std::cout << "\nTesting 'reduction'...\n";
    Cache items(host, port);
    Cache::size_type gotItemSize = 0;
    // Fill the cache
    cache_set(items, "Abc", "ItemA", 4);
    //cache_set(items, "Bc", "ItemB", 3);
    //cache_set(items, "Cd", "ItemC", 3);
    // Make one of the existing values smaller
    // cache_set(items, "A", "ItemA", 2);
    // Verify that it was not rejected
    cache_get(items, "ItemA", gotItemSize, 4);
    cache_reset(items);
    items.~Cache();
}

void test_basic_operation() {
    /* Test basic functionality of a cache with no optional parameters */
    std::cout << "\nTesting basic operations...\n";
    Cache items(host, port);
    cache_space_used(items, 0);
    Cache::size_type gotItemSize = 0;
    // Set an item, verify that it's the right size
    cache_set(items, "Abcd", "ItemA", 5);
    cache_space_used(items, 5);
    // Get that item, check that it's size is updated correctly
    cache_get(items, "ItemA", gotItemSize, 5);
    // Delete item, check that the cache is now empty
    cache_del(items, "ItemA");
    cache_space_used(items, 0);
    // Set an item, reset the cache, an verify that the cache is empty
    cache_set(items, "Bc", "ItemB", 3);
    cache_reset(items);
    cache_space_used(items, 0);
    items.~Cache();
}

void test_modify_value() {
    /* Test that objects can be overwritten */
    std::cout << "\nTesting 'modify value'...\n";
    Cache items(host, port);
    Cache::size_type gotItemSize = 0;
    // Set an item, overwrite it, and check that the size has changed
    cache_set(items, "Abcd", "ItemA", 5);
    cache_set(items, "Ab", "ItemA", 3);
    cache_space_used(items, 3);
    cache_get(items, "ItemA", gotItemSize, 3);
    cache_reset(items);
    items.~Cache();
}

void test_set_object_cache_size() {
    /* Sets an object of size 'maxmem' and verifies that it was added properly */
    std::cout << "\nTesting 'set object of cache size'...\n";
    Cache items(host, port);
    Cache::size_type gotItemSize = 0;
    // Set an item that fills the entire cache
    cache_set(items, "Abcdefghi", "ItemA", 10);
    cache_get(items, "ItemA", gotItemSize, 10);
    // Check that it worked
    cache_space_used(items, 10);
    cache_reset(items);
    items.~Cache();
}

void test_cache_bounds() {
    std::cout << "\nTesting cache bounds without evictor...\n";
    /* Try adding an object to the cache that is greater than maxmem. Make sure it fails. */
    Cache items(host, port);
    cache_set(items, "Abcdefghij", "ItemA", 11);
    cache_space_used(items, 0);
    cache_reset(items);
    items.~Cache();
}

void test_overflow_no_evictor() {
    std::cout << "\nTesting 'overflow' without evictor...\n";
    Cache items(host, port);
    //Add a series of items, in which the last one will overflow the cache.
    cache_set(items, "Abcd", "ItemA", 5);
    cache_set(items, "Bc", "ItemB", 3);
    cache_set(items, "Cde", "ItemC", 4);
    cache_space_used(items, 8);
    cache_reset(items);
    items.~Cache();
}

void test_get_non_existant_item() {
    std::cout << "\nTesting non-existant item...\n";
    Cache items(host, port);
    Cache::size_type gotItemSize = 0;
    // Get something that never existed
    cache_get_failure(items, "ItemA", gotItemSize);
    cache_space_used(items, 0);
    cache_reset(items);
    // Set something, delete it, and try to get it
    cache_set(items, "Abcd", "ItemA", 5);
    cache_del(items, "ItemA");
    cache_get_failure(items, "ItemA", gotItemSize);
    cache_space_used(items, 0);
    cache_reset(items);
    items.~Cache();
}
/*
// TESTS WITH AN EVICTOR
void test_basic_evictor() {
    std::cout << "\nTesting basic operations with an evictor...\n";
    FIFO_Evictor evictPolicy = FIFO_Evictor();
    Cache::size_type gotItemSize = 0;
    Cache items(10, 0.75, &evictPolicy);
    // Add values to cache
    cache_set(items, "Abc", "ItemA", 4);
    cache_set(items, "D", "ItemD", 2);
    cache_set(items, "Bc", "ItemB", 3);
    cache_space_used(items, 9);
    cache_get(items, "ItemA", gotItemSize, 4);
    // Add value, overflowing cache and evicting A and D
    cache_set(items, "Cde", "ItemC", 7);
    cache_space_used(items, 10);
    cache_get_failure(items, "ItemA", gotItemSize);
    cache_get_failure(items, "ItemD", gotItemSize);
    evictPolicy.~FIFO_Evictor();
    items.~Cache();
}

void test_cache_bounds_with_evictor() {
    std::cout << "\nTesting cache bounds with evictor...\n";
    // Check that the cache doesn't evict items unnecessarily
    FIFO_Evictor evictPolicy = FIFO_Evictor();
    Cache::size_type gotItemSize = 0;
    Cache items(10, 0.75, &evictPolicy);
    // Add some items to the cache
    cache_set(items, "Bc", "ItemB", 3);
    cache_set(items, "Cde", "ItemC", 4);
    cache_space_used(items, 7);
    // Try to set an item with a size larger than maxmem
    cache_set(items, "Abcedfghijklmno", "ItemA", 16);
    cache_space_used(items, 7);
    // Check if any items were evicted unnecessarily
    cache_get(items, "ItemB", gotItemSize, 3);
    cache_get(items, "ItemC", gotItemSize, 4);
    evictPolicy.~FIFO_Evictor();
    items.~Cache();
}

void test_unnecessary_eviction()
{
    std::cout << "\nTesting unnecessary eviction...\n";
    FIFO_Evictor evictPolicy = FIFO_Evictor();
    Cache::size_type gotItemSize = 0;
    Cache items(10, 0.75, &evictPolicy);
    cache_set(items, "Abc", "ItemA", 4);
    cache_set(items, "Bc", "ItemB", 3);
    cache_space_used(items, 7);
    // Delete an item
    cache_del(items, "ItemA");
    cache_space_used(items, 3);
    // If the deleted item were still in the cache, this would prompt eviction.
    // However, since it no longer exists, it shouldn't do so.
    cache_set(items, "Cdefgh", "ItemC", 7);
    cache_space_used(items, 10);
    cache_get(items, "ItemB", gotItemSize, 3);
    evictPolicy.~FIFO_Evictor();
    items.~Cache();
}

void test_eviction() {
    std::cout << "\nDirectly testing evictor...\n";
    FIFO_Evictor evictPolicy;
    //Series of touchkeys/evicts to check FIFO ordering
    evictPolicy.touch_key("ItemA");
    evictPolicy.touch_key("ItemB");
    evictPolicy.touch_key("ItemA");
    evictPolicy.touch_key("ItemC");
    key_type evictedKey = evictPolicy.evict();
    assert(evictedKey == "ItemA" && "Evicted key did not match expectation!");
    evictedKey = evictPolicy.evict();
    assert(evictedKey == "ItemB" && "Evicted key did not match expectation!");
    evictedKey = evictPolicy.evict();
    assert(evictedKey == "ItemA" && "Evicted key did not match expectation!");
    evictPolicy.~FIFO_Evictor();
}

void test_evict_all() {
    std::cout << "\nTesting evict_all...\n";
    // Set an item large enough to remove all items in the cache
    FIFO_Evictor evictPolicy = FIFO_Evictor();
    Cache::size_type gotItemSize = 0;
    Cache items(10, 0.75, &evictPolicy);
    // Fill the cache with objects
    cache_set(items, "Abc", "ItemA", 4);
    cache_set(items, "Bc", "ItemB", 3);
    cache_set(items, "Cd", "ItemC", 3);
    cache_space_used(items, 10);
    // Set an object that fills the entire cache
    cache_set(items, "Defghijkl", "ItemD", 10);
    cache_space_used(items, 10);
    // Make sure all other items were evicted
    cache_get_failure(items, "ItemA", gotItemSize);
    cache_get_failure(items, "ItemB", gotItemSize);
    cache_get_failure(items, "ItemC", gotItemSize);
    cache_get(items, "ItemD", gotItemSize, 10);
    evictPolicy.~FIFO_Evictor();
    items.~Cache();
}

void test_size_zero_does_not_evict() {
    // Check that an item of size 0 does not prompt an eviction
    std::cout << "\nTesting size zero item does not evict...\n";
    FIFO_Evictor evictPolicy = FIFO_Evictor();
    Cache::size_type gotItemSize = 0;
    Cache items(10, 0.75, &evictPolicy);
    // Fill the cache with objects
    cache_set(items, "Abc", "ItemA", 4);
    cache_set(items, "Bc", "ItemB", 3);
    cache_set(items, "Cd", "ItemC", 3);
    cache_space_used(items, 10);
    // Add an item of size 0
    cache_set(items, "", "ItemD", 0);
    cache_space_used(items, 10);
    // Make sure nothing was evicted
    cache_get(items, "ItemA", gotItemSize, 4); // Our code fails at this line
    cache_get(items, "ItemB", gotItemSize, 3);
    cache_get(items, "ItemC", gotItemSize, 3);
    evictPolicy.~FIFO_Evictor();
    items.~Cache();
}
*/
int main()
{
    //test_reduction();
    test_basic_operation();
    test_modify_value();
    test_set_object_cache_size();
    test_cache_bounds();
    test_overflow_no_evictor();
    test_get_non_existant_item();
    
    //test_basic_evictor();
    //test_cache_bounds_with_evictor();
    //test_unnecessary_eviction();
    //test_eviction();
    //test_evict_all();
    //test_size_zero_does_not_evict();
    return 0;
}