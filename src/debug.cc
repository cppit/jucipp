#include "debug.h"

#include <thread>

#include "lldb/API/SBTarget.h"
#include "lldb/API/SBProcess.h"
#include "lldb/API/SBListener.h"
#include "lldb/API/SBEvent.h"
#include "lldb/API/SBBreakpoint.h"
#include "lldb/API/SBThread.h"
#include "lldb/API/SBStream.h"

#include <iostream> //TODO: remove
using namespace std;

void log(const char *msg, void *) {
  cout << "debugger log: " << msg << endl;
}

Debug::Debug() {
  lldb::SBDebugger::Initialize();
  
  debugger=lldb::SBDebugger::Create(true, log, nullptr);
}

void Debug::start(const boost::filesystem::path &project_path, const boost::filesystem::path &executable,
                  const boost::filesystem::path &path, std::function<void(int exit_status)> callback) {
  std::thread debug_thread([this, project_path, executable, path, callback]() {
    auto target=debugger.CreateTarget(executable.string().c_str());
    auto listener=lldb::SBListener("juCi++ lldb listener");
    
    if(!target.IsValid()) {
      cerr << "Error: Could not create debug target to: " << executable << endl; //TODO: output to terminal instead
      return;
    }
    
    for(auto &breakpoint: breakpoints[project_path.string()]) {
      if(!(target.BreakpointCreateByLocation(breakpoint.first.c_str(), breakpoint.second)).IsValid()) {
        cerr << "Error: Could not create breakpoint at: " << breakpoint.first << ":" << breakpoint.second << endl; //TODO: fix output to terminal instead
        return;
      }
      else
        cerr << "Created breakpoint at: " << breakpoint.first << ":" << breakpoint.second << endl;
    }
    
    lldb::SBError error;
    auto process = target.Launch(listener, nullptr, nullptr, nullptr, nullptr, nullptr, path.string().c_str(), lldb::eLaunchFlagNone, false, error);
    
    if(error.Fail()) {
      cerr << "Error (debug): " << error.GetCString() << endl; //TODO: output to terminal instead
      return;
    }
  
    //Wait till stopped. TODO: add check if process.GetStateFromEvent(event)==lldb::StateType::eStateStopped after
    lldb::SBEvent event;
    while(listener.WaitForEvent(3, event) && process.GetStateFromEvent(event)!=lldb::StateType::eStateStopped) {}
    
    cout << "NumThreads: " << process.GetNumThreads() << endl;
    for(uint32_t thread_index_id=0;thread_index_id<process.GetNumThreads();thread_index_id++) {
      auto thread=process.GetThreadAtIndex(thread_index_id);
      cout << "NumFrames: " << thread.GetNumFrames() << endl;
      for(uint32_t frame_index=0;frame_index<thread.GetNumFrames();frame_index++) {
        auto frame=thread.GetFrameAtIndex(frame_index);
        auto values=frame.GetVariables(true, true, true, true);
        cout << "variables.GetSize(): " << values.GetSize() << endl;
        for(uint32_t value_index=0;value_index<values.GetSize();value_index++) {
          auto value=values.GetValueAtIndex(value_index);
          //cout << value.GetName() << ": " << value.GetValue() << endl;
          lldb::SBStream stream;
          value.GetDescription(stream);
          cout << stream.GetData();
        }
      }
    }
    
    process.Continue();
  
    //Get exit status. TODO: add check if process.GetStateFromEvent(event)==lldb::StateType::eStateExited after
    while(listener.WaitForEvent(3, event) && process.GetStateFromEvent(event)!=lldb::StateType::eStateExited) {}
    auto exit_status=process.GetExitStatus();
    
    if(callback)
      callback(exit_status);
  });
  debug_thread.detach();
}
