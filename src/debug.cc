#include "debug.h"

#include <thread>

#include "lldb/API/SBTarget.h"
#include "lldb/API/SBProcess.h"
#include "lldb/API/SBListener.h"
#include "lldb/API/SBEvent.h"
#include "lldb/API/SBBreakpoint.h"
#include "lldb/API/SBThread.h"
#include "lldb/API/SBStream.h"
#include "lldb/API/SBDeclaration.h"

#include <iostream> //TODO: remove
using namespace std;

void log(const char *msg, void *) {
  cout << "debugger log: " << msg << endl;
}

Debug::Debug(): stopped(false) {
  lldb::SBDebugger::Initialize();
  
  debugger=lldb::SBDebugger::Create(true, log, nullptr);
}

void Debug::start(std::shared_ptr<std::vector<std::pair<boost::filesystem::path, int> > > breakpoints, const boost::filesystem::path &executable,
                  const boost::filesystem::path &path, std::function<void(int exit_status)> callback,
                  std::function<void(const std::string &status)> status_callback,
                  std::function<void(const boost::filesystem::path &file, int line)> stop_callback) {
  std::thread debug_thread([this, breakpoints, executable, path, callback, status_callback, stop_callback]() {
    auto target=debugger.CreateTarget(executable.string().c_str());
    auto listener=lldb::SBListener("juCi++ lldb listener");
    
    if(!target.IsValid()) {
      cerr << "Error: Could not create debug target to: " << executable << endl; //TODO: output to terminal instead
      return;
    }
    
    for(auto &breakpoint: *breakpoints) {
      if(!(target.BreakpointCreateByLocation(breakpoint.first.string().c_str(), breakpoint.second)).IsValid()) {
        cerr << "Error: Could not create breakpoint at: " << breakpoint.first << ":" << breakpoint.second << endl; //TODO: output to terminal instead
        return;
      }
    }
    
    lldb::SBError error;
    process = std::unique_ptr<lldb::SBProcess>(new lldb::SBProcess(target.Launch(listener, nullptr, nullptr, nullptr, nullptr, nullptr, path.string().c_str(), lldb::eLaunchFlagNone, false, error)));
    
    if(error.Fail()) {
      cerr << "Error (debug): " << error.GetCString() << endl; //TODO: output to terminal instead
      return;
    }

    lldb::SBEvent event;
    while(true) {
      if(listener.WaitForEvent(3, event)) {
        auto state=process->GetStateFromEvent(event);
        
        //Update debug status
        lldb::SBStream stream;
        event.GetDescription(stream);
        std::string event_desc=stream.GetData();
        event_desc.pop_back();
        auto pos=event_desc.rfind(" = ");
        if(status_callback && pos!=std::string::npos)
          status_callback(event_desc.substr(pos+3));
        
        bool expected=false;
        if(state==lldb::StateType::eStateStopped && stopped.compare_exchange_strong(expected, true)) {
          auto line_entry=process->GetSelectedThread().GetSelectedFrame().GetLineEntry();
          if(stop_callback) {
            lldb::SBStream stream;
            line_entry.GetFileSpec().GetDescription(stream);
            stop_callback(stream.GetData(), line_entry.GetLine());
          }
          
          /*lldb::SBStream stream;
          process->GetSelectedThread().GetDescription(stream);
          cout << stream.GetData() << endl;*/
          for(uint32_t thread_index=0;thread_index<process->GetNumThreads();thread_index++) {
            auto thread=process->GetThreadAtIndex(thread_index);
            for(uint32_t frame_index=0;frame_index<thread.GetNumFrames();frame_index++) {
              auto frame=thread.GetFrameAtIndex(frame_index);
              auto values=frame.GetVariables(false, true, true, false);
              for(uint32_t value_index=0;value_index<values.GetSize();value_index++) {
                /*cout << thread_index << ", " << frame_index << endl;
                lldb::SBStream stream;
                process->GetDescription(stream);
                cout << stream.GetData() << endl;
                
                stream.Clear();
                process->GetSelectedThread().GetSelectedFrame().GetLineEntry().GetDescription(stream);
                cout << stream.GetData() << endl;*/
                /*lldb::SBStream stream;
                auto value=values.GetValueAtIndex(value_index);
                
                cout << value.GetFrame().GetSymbol().GetName() << endl;
                
                auto declaration = value.GetDeclaration();
                if(declaration.IsValid())
                  cout << declaration.GetFileSpec().GetFilename() << ":" << declaration.GetLine() << ":" << declaration.GetColumn() << endl;
                
                value.GetDescription(stream);
                cout << "  " << stream.GetData() << endl;
                stream.Clear();
                
                value.GetData().GetDescription(stream);
                cout << "  " << stream.GetData() << endl;*/
              }
            }
          }
        }
        
        else if(state==lldb::StateType::eStateExited) {
          auto exit_status=process->GetExitStatus();
          if(callback)
            callback(exit_status);
          if(status_callback)
            status_callback("");
          if(stop_callback)
            stop_callback("", 0);
          process.reset();
          stopped=false;
          return;
        }
        
        else if(state==lldb::StateType::eStateCrashed) {
          if(callback)
            callback(-1);
          if(status_callback)
            status_callback("");
          if(stop_callback)
            stop_callback("", 0);
          process.reset();
          stopped=false;
          return;
        }
      }
      this_thread::sleep_for(std::chrono::milliseconds(200));
    }
  });
  debug_thread.detach();
}

void Debug::continue_debug() {
  bool expected=true;
  if(stopped.compare_exchange_strong(expected, false))
    process->Continue();
}

void Debug::stop() {
  auto error=process->Stop();
  if(error.Fail()) {
    cerr << "Error (debug): " << error.GetCString() << endl; //TODO: output to terminal instead
    return;
  }
}

void Debug::kill() {
  auto error=process->Kill();
  if(error.Fail()) {
    cerr << "Error (debug): " << error.GetCString() << endl; //TODO: output to terminal instead
    return;
  }
}

std::string Debug::get_value(const std::string &variable) {
  if(stopped) {
    auto frame=process->GetSelectedThread().GetSelectedFrame();
    auto values=frame.GetVariables(false, true, false, false);
    for(uint32_t value_index=0;value_index<values.GetSize();value_index++) {
      lldb::SBStream stream;
      auto value=values.GetValueAtIndex(value_index);

      if(value.GetName()==variable) {
        value.GetDescription(stream);
        return stream.GetData();
      }
    }
  }
  return std::string();
}
