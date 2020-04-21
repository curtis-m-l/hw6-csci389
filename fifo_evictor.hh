#pragma once

#include <deque>
#include "evictor.hh"

/*
 Basic first-in-first-out (FIFO) eviction policy
 created for the Cache class defined in "cache.hh".
 Implemented in "fifo_evictor.cc".

 Created by Casey Harris
 for CSCI 389 Homework #2
 */

class FIFO_Evictor : public Evictor {
  private:
    // Deques are good for dual-ended operations (vectors have no push_front). 
    // https://en.cppreference.com/w/cpp/container/deque
    std::deque<key_type> keys;

  public:
    void touch_key(const key_type& touchedKey);
    const key_type evict();
};
