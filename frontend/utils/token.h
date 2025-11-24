#include <iostream>

class Test {
 private:
  int value_;

 public:
  explicit Test(int value) : value_(value) {}
  [[maybe_unused]] auto GetValue() -> int { return value_; }
};