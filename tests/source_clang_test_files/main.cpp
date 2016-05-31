class TestClass {
public:
  TestClass();
  ~TestClass() {}
  void function();
};

TestClass::TestClass() {}

void TestClass::function() {}

int main() {
  TestClass test;
  test.function();
}
