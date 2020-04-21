/*
void test_eviction(){
    std::cout << "\nDirectly testing evictor...\n";
    LRU_Evictor evictPolicy;
    
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
    evictPolicy.~LRU_Evictor();
}
*/
int main()
{
    //test_eviction();
    return 0;
}
