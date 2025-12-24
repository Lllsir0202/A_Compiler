class Test {
 private:
  int value_;

 private:
  explicit Test(int value) : value_(value) {}

 public:
  static auto GetTest(int value) -> Test { return Test(value); }
  [[maybe_unused]] auto GetValue() -> int { return value_; }
};