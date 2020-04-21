#include "fifo_evictor.hh"

/*
 Basic first-in-first-out (FIFO) eviction policy
 defined in "fifo_evictor.hh".
 Created for the Cache class defined in "cache.hh"

 Created by Casey Harris
 for CSCI 389 Homework #2
 */

  std::deque<key_type> keys;

  void FIFO_Evictor::touch_key(const key_type& touchedKey){
      keys.push_front(touchedKey);
  }

  const key_type FIFO_Evictor::evict(){
      // This code may cause problems if something is "double-touched", or
      // added (and evicted) twice. Unclear.
      auto lastVal = keys.back();
      keys.pop_back();
      return lastVal;
  }