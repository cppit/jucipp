#include <glib.h>
#include "debug_lldb.h"
#include <thread>
#include <boost/filesystem.hpp>
#include <atomic>

int main() {
  auto build_path=boost::filesystem::canonical(JUCI_BUILD_PATH);
  auto exec_path=build_path/"tests"/"lldb_test_files"/"lldb_test_executable";
  g_assert(boost::filesystem::exists(exec_path));
  
  auto tests_path=boost::filesystem::canonical(JUCI_TESTS_PATH);
  auto source_path=tests_path/"lldb_test_files"/"main.cpp";
  g_assert(boost::filesystem::exists(source_path));
  
  {
    auto parsed_run_arguments=Debug::LLDB::get().parse_run_arguments("\"~/test/te st\"");
    assert(std::get<0>(parsed_run_arguments).size()==0);
    
    assert(std::get<1>(parsed_run_arguments)=="~/test/te st");
    
    assert(std::get<2>(parsed_run_arguments).size()==0);
  }
  {
    auto parsed_run_arguments=Debug::LLDB::get().parse_run_arguments("~/test/te\\ st");
    assert(std::get<0>(parsed_run_arguments).size()==0);
    
    assert(std::get<1>(parsed_run_arguments)=="~/test/te st");
    
    assert(std::get<2>(parsed_run_arguments).size()==0);
  }
  {
    auto parsed_run_arguments=Debug::LLDB::get().parse_run_arguments("~/test/te\\ st Arg1\\\\ arg2");
    assert(std::get<0>(parsed_run_arguments).size()==0);
    
    assert(std::get<1>(parsed_run_arguments)=="~/test/te st");
    
    assert(std::get<2>(parsed_run_arguments).size()==2);
    assert(std::get<2>(parsed_run_arguments)[0]=="Arg1\\");
    assert(std::get<2>(parsed_run_arguments)[1]=="arg2");
  }
  {
    auto parsed_run_arguments=Debug::LLDB::get().parse_run_arguments("~/test/te\\ st Arg1\\\\\\ arg1");
    assert(std::get<0>(parsed_run_arguments).size()==0);
    
    assert(std::get<1>(parsed_run_arguments)=="~/test/te st");
    
    assert(std::get<2>(parsed_run_arguments).size()==1);
    assert(std::get<2>(parsed_run_arguments)[0]=="Arg1\\ arg1");
  }
  {
    auto parsed_run_arguments=Debug::LLDB::get().parse_run_arguments("\"~/test/te st\" Arg1 \"Ar g2\" Ar\\ g3");
    assert(std::get<0>(parsed_run_arguments).size()==0);
    
    assert(std::get<1>(parsed_run_arguments)=="~/test/te st");
    
    assert(std::get<2>(parsed_run_arguments).size()==3);
    assert(std::get<2>(parsed_run_arguments)[0]=="Arg1");
    assert(std::get<2>(parsed_run_arguments)[1]=="Ar g2");
    assert(std::get<2>(parsed_run_arguments)[2]=="Ar g3");
  }
  {
    auto parsed_run_arguments=Debug::LLDB::get().parse_run_arguments("ENV1=Test ENV2=Te\\ st ENV3=\"te ts\" ~/test/te\\ st Arg1 \"Ar g2\" Ar\\ g3");
    assert(std::get<0>(parsed_run_arguments).size()==3);
    assert(std::get<0>(parsed_run_arguments)[0]=="ENV1=Test");
    assert(std::get<0>(parsed_run_arguments)[1]=="ENV2=Te st");
    assert(std::get<0>(parsed_run_arguments)[2]=="ENV3=te ts");
    
    assert(std::get<1>(parsed_run_arguments)=="~/test/te st");
    
    assert(std::get<2>(parsed_run_arguments).size()==3);
    assert(std::get<2>(parsed_run_arguments)[0]=="Arg1");
    assert(std::get<2>(parsed_run_arguments)[1]=="Ar g2");
    assert(std::get<2>(parsed_run_arguments)[2]=="Ar g3");
  }
  
  std::vector<std::pair<boost::filesystem::path, int> > breakpoints;
  breakpoints.emplace_back(source_path, 2);
  
  std::atomic<bool> exited(false);
  int exit_status;
  std::atomic<int> line_nr(0);
  Debug::LLDB::get().on_exit.emplace_back([&](int exit_status_) {
    exit_status=exit_status_;
    exited=true;
  });
  Debug::LLDB::get().on_event.emplace_back([&](const lldb::SBEvent &event) {
    std::unique_lock<std::mutex> lock(Debug::LLDB::get().mutex);
    auto process=lldb::SBProcess::GetProcessFromEvent(event);
    auto state=lldb::SBProcess::GetStateFromEvent(event);
    if(state==lldb::StateType::eStateStopped) {
      auto line_entry=process.GetSelectedThread().GetSelectedFrame().GetLineEntry();
      if(line_entry.IsValid()) {
        lldb::SBStream stream;
        line_entry.GetFileSpec().GetDescription(stream);
        line_nr=line_entry.GetLine();
      }
    }
  });
  
  Debug::LLDB::get().start(exec_path.string(), "", breakpoints);
  
  for(;;) {
    if(exited) {
      g_assert_cmpint(exit_status, ==, 0);
      break;
    }
    else if(line_nr>0) {
      for(;;) {
        if(Debug::LLDB::get().is_stopped())
          break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
      g_assert_cmpint(line_nr, ==, 2);
      g_assert(Debug::LLDB::get().get_backtrace().size()>0);
      auto variables=Debug::LLDB::get().get_variables();
      g_assert_cmpstr(variables.at(0).name.c_str(), ==, "an_int");
      line_nr=0;
      Debug::LLDB::get().step_over();
      for(;;) {
        if(line_nr>0 && Debug::LLDB::get().is_stopped())
          break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
      g_assert_cmpint(line_nr, ==, 3);
      g_assert(Debug::LLDB::get().get_backtrace().size()>0);
      variables=Debug::LLDB::get().get_variables();
      g_assert_cmpstr(variables.at(0).name.c_str(), ==, "an_int");
      auto value=Debug::LLDB::get().get_value("an_int", source_path, 2, 7);
      g_assert_cmpuint(value.size(), >, 16);
      auto value_substr=value.substr(0, 16);
      g_assert_cmpstr(value_substr.c_str(), ==, "(int) an_int = 1");
      line_nr=0;
      Debug::LLDB::get().continue_debug();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  
  Debug::LLDB::get().cancel();
}
