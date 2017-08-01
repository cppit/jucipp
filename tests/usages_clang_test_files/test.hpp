class Test {
public:
  Test() {}
  ~Test() {}
  
  int a=0;
  void b() {
    ++a;
  }
  void c() {
    b();
  }
};
