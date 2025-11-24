#include <iostream>
#include "frontend/utils/token.h"

auto main() -> int {
  std::cout << "Hello, World!" << std::endl;
  Test test(42);
  std::cout << "Test value: " << test.GetValue() << std::endl;
  return 0;
}