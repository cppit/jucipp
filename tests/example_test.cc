#include "terminal.h"
#include "info.h"
#include <gtkmm.h>

//In case one has to test functions that include Terminal::print or Info::print
//Requires display server to work
//However, it is possible to use the Broadway backend if the test is run in a pure terminal environment
//One also has to include the source stubs/dispatcher.cc in CMakeLists.txt for at least Terminal

//To run the test using broadway backend:
//broadwayd&
//make test

int main() {
  auto app=Gtk::Application::create();
  Terminal::get().print("some message");
  Info::get().print("some message");
  g_assert(true);
}
