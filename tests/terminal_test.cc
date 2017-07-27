#include "terminal.h"
#include <glib.h>

//Requires display server to work
//However, it is possible to use the Broadway backend if the test is run in a pure terminal environment:
//broadwayd&
//make test

int main() {
  auto app = Gtk::Application::create();
  
  {
    auto link=Terminal::get().find_link("~/test/test.cc:7:41: error: expected ';' after expression.");
    assert(std::get<0>(link)==0);
    assert(std::get<1>(link)==19);
    assert(std::get<2>(link)=="~/test/test.cc");
    assert(std::get<3>(link)=="7");
    assert(std::get<4>(link)=="41");
  }
  {
    auto link=Terminal::get().find_link("Assertion failed: (false), function main, file ~/test/test.cc, line 15.");
    assert(std::get<0>(link)==47);
    assert(std::get<1>(link)==70);
    assert(std::get<2>(link)=="~/test/test.cc");
    assert(std::get<3>(link)=="15");
  }
  {
    auto link=Terminal::get().find_link("test: ~/examples/main.cpp:17: int main(int, char**): Assertion `false' failed.");
    assert(std::get<0>(link)==6);
    assert(std::get<1>(link)==28);
    assert(std::get<2>(link)=="~/examples/main.cpp");
    assert(std::get<3>(link)=="17");
  }
  {
    auto link=Terminal::get().find_link("ERROR:~/test/test.cc:36:int main(): assertion failed: (false)");
    assert(std::get<0>(link)==6);
    assert(std::get<1>(link)==23);
    assert(std::get<2>(link)=="~/test/test.cc");
    assert(std::get<3>(link)=="36");
  }
}
