class TestClass {
public:
  TestClass();
  template<class T>
  TestClass(T t) {++t;}
  ~TestClass() {}
  void function();
};

TestClass::TestClass() {}

void TestClass::function() {}

int main() {
  TestClass test;
  test.function();
}
