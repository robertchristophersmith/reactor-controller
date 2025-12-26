#ifndef WEIGHTED_AVERAGE_H
#define WEIGHTED_AVERAGE_H

#include <Arduino.h>

#define WA_BUFFER_SIZE 60 // 60 samples at 1Hz = 1 minute

class WeightedAverage {
public:
  WeightedAverage() { reset(); }

  void reset() {
    _head = 0;
    _count = 0;
    _sum = 0.0;
    for (int i = 0; i < WA_BUFFER_SIZE; i++) {
      _buffer[i] = 0.0;
    }
  }

  void add(float value) {
    if (isnan(value))
      return; // Ignore NaNs

    // Subtract old value at head
    _sum -= _buffer[_head];

    // Overwrite head
    _buffer[_head] = value;
    _sum += value;

    // Move head
    _head++;
    if (_head >= WA_BUFFER_SIZE)
      _head = 0;

    if (_count < WA_BUFFER_SIZE)
      _count++;
  }

  float getAverage() {
    if (_count == 0)
      return 0.0;
    return _sum / _count;
  }

private:
  float _buffer[WA_BUFFER_SIZE];
  uint8_t _head;
  uint8_t _count;
  float _sum;
};

#endif
