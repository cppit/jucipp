#include "debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include "terminal.h"

#include <lldb/API/SBTarget.h>
#include <lldb/API/SBProcess.h>
#include <lldb/API/SBEvent.h>
#include <lldb/API/SBBreakpoint.h>
#include <lldb/API/SBThread.h>
#include <lldb/API/SBStream.h>
#include <lldb/API/SBDeclaration.h>
#include <lldb/API/SBCommandInterpreter.h>
#include <lldb/API/SBCommandReturnObject.h>
#include <lldb/API/SBBreakpointLocation.h>

using namespace std; //TODO: remove

extern char **environ;

void log(const char *msg, void *) {
  cout << "debugger log: " << msg << endl;
}

Debug::Debug(): listener("juCi++ lldb listener"), state(lldb::StateType::eStateInvalid), buffer_size(131072) {
#ifdef __APPLE__
  setenv("LLDB_DEBUGSERVER_PATH", "/usr/local/opt/llvm/bin/debugserver", 0);
#endif
}

void Debug::start(std::shared_ptr<std::vector<std::pair<boost::filesystem::path, int> > > breakpoints, const boost::filesystem::path &executable,
                  const boost::filesystem::path &path, std::function<void(int exit_status)> callback,
                  std::function<void(const std::string &status)> status_callback,
                  std::function<void(const boost::filesystem::path &file_path, int line_nr, int line_index)> stop_callback) {
  if(!debugger.IsValid()) {
    lldb::SBDebugger::Initialize();
    debugger=lldb::SBDebugger::Create(true, log, nullptr);
  }
  auto target=debugger.CreateTarget(executable.string().c_str());
  
  if(!target.IsValid()) {
    Terminal::get().async_print("Error (debug): Could not create debug target to: "+executable.string()+'\n', true);
    return;
  }
  
  for(auto &breakpoint: *breakpoints) {
    if(!(target.BreakpointCreateByLocation(breakpoint.first.string().c_str(), breakpoint.second)).IsValid()) {
      Terminal::get().async_print("Error (debug): Could not create breakpoint at: "+breakpoint.first.string()+":"+std::to_string(breakpoint.second)+'\n', true);
      return;
    }
  }
  
  lldb::SBError error;
  process = std::unique_ptr<lldb::SBProcess>(new lldb::SBProcess(target.Launch(listener, nullptr, (const char**)environ, nullptr, nullptr, nullptr, path.string().c_str(), lldb::eLaunchFlagNone, false, error)));
  if(error.Fail()) {
    Terminal::get().async_print(std::string("Error (debug): ")+error.GetCString()+'\n', true);
    return;
  }
  if(debug_thread.joinable())
    debug_thread.join();
  debug_thread=std::thread([this, callback, status_callback, stop_callback]() {
    lldb::SBEvent event;
    while(true) {
      event_mutex.lock();
      if(listener.GetNextEvent(event)) {
        if((event.GetType() & lldb::SBProcess::eBroadcastBitStateChanged)>0) {
          auto state=process->GetStateFromEvent(event);
          this->state=state;
          
          //Update debug status
          lldb::SBStream stream;
          event.GetDescription(stream);
          std::string event_desc=stream.GetData();
          event_desc.pop_back();
          auto pos=event_desc.rfind(" = ");
          if(status_callback && pos!=std::string::npos)
            status_callback(event_desc.substr(pos+3));
          
          if(state==lldb::StateType::eStateStopped) {
            auto line_entry=process->GetSelectedThread().GetSelectedFrame().GetLineEntry();
            if(stop_callback) {
              if(line_entry.IsValid()) {
                lldb::SBStream stream;
                line_entry.GetFileSpec().GetDescription(stream);
                auto column=line_entry.GetColumn();
                if(column==0)
                  column=1;
                stop_callback(stream.GetData(), line_entry.GetLine(), column);
              }
              else
                stop_callback("", 0, 0);
            }
          }
          else if(state==lldb::StateType::eStateRunning) {
            stop_callback("", 0, 0);
          }
          else if(state==lldb::StateType::eStateExited) {
            auto exit_status=process->GetExitStatus();
            if(callback)
              callback(exit_status);
            if(status_callback)
              status_callback("");
            if(stop_callback)
              stop_callback("", 0, 0);
            process.reset();
            this->state=lldb::StateType::eStateInvalid;
            event_mutex.unlock();
            return;
          }
          else if(state==lldb::StateType::eStateCrashed) {
            if(callback)
              callback(-1);
            if(status_callback)
              status_callback("");
            if(stop_callback)
              stop_callback("", 0, 0);
            process.reset();
            this->state=lldb::StateType::eStateInvalid;
            event_mutex.unlock();
            return;
          }
        }
        if((event.GetType() & lldb::SBProcess::eBroadcastBitSTDOUT)>0) {
          char buffer[buffer_size];
          size_t n;
          while((n=process->GetSTDOUT(buffer, buffer_size))!=0)
            Terminal::get().async_print(std::string(buffer, n));
        }
        //TODO: for some reason stderr is redirected to stdout
        if((event.GetType() & lldb::SBProcess::eBroadcastBitSTDERR)>0) {
          char buffer[buffer_size];
          size_t n;
          while((n=process->GetSTDERR(buffer, buffer_size))!=0)
            Terminal::get().async_print(std::string(buffer, n), true);
        }
      }
      event_mutex.unlock();
      this_thread::sleep_for(std::chrono::milliseconds(200));
    }
  });
}

void Debug::continue_debug() {
  event_mutex.lock();
  if(state==lldb::StateType::eStateStopped)
    process->Continue();
  event_mutex.unlock();
}

void Debug::stop() {
  event_mutex.lock();
  if(state==lldb::StateType::eStateRunning) {
    auto error=process->Stop();
    if(error.Fail())
      Terminal::get().async_print(std::string("Error (debug): ")+error.GetCString()+'\n', true);
  }
  event_mutex.unlock();
}

void Debug::kill() {
  event_mutex.lock();
  if(process) {
    auto error=process->Kill();
    if(error.Fail())
      Terminal::get().async_print(std::string("Error (debug): ")+error.GetCString()+'\n', true);
  }
  event_mutex.unlock();
}

std::pair<std::string, std::string> Debug::run_command(const std::string &command) {
  std::pair<std::string, std::string> command_return;
  event_mutex.lock();
  if(state==lldb::StateType::eStateStopped) {
    lldb::SBCommandReturnObject command_return_object;
    debugger.GetCommandInterpreter().HandleCommand(command.c_str(), command_return_object, true);
    command_return.first=command_return_object.GetOutput();
    command_return.second=command_return_object.GetError();
  }
  event_mutex.unlock();
  return command_return;
}

void Debug::delete_debug() {
  kill();
  if(debug_thread.joinable())
    debug_thread.join();
  lldb::SBDebugger::Terminate();
}

std::string Debug::get_value(const std::string &variable, const boost::filesystem::path &file_path, unsigned int line_nr) {
  std::string variable_value;
  event_mutex.lock();
  if(state==lldb::StateType::eStateStopped) {
    auto frame=process->GetSelectedThread().GetSelectedFrame();
    
    auto values=frame.GetVariables(true, true, true, false);
    //First try to find variable based on name, file and line number
    for(uint32_t value_index=0;value_index<values.GetSize();value_index++) {
      lldb::SBStream stream;
      auto value=values.GetValueAtIndex(value_index);

      if(value.GetName()==variable) {
        auto declaration=value.GetDeclaration();
        if(declaration.IsValid()) {
          if(declaration.GetLine()==line_nr) {
            auto file_spec=declaration.GetFileSpec();
            boost::filesystem::path value_decl_path=file_spec.GetDirectory();
            value_decl_path/=file_spec.GetFilename();
            if(value_decl_path==file_path) {
              value.GetDescription(stream);
              variable_value=stream.GetData();
              break;
            }
          }
        }
      }
    }
    if(variable_value.empty()) {
      //In case a variable is missing file and line number, only do check on name
      auto value=frame.FindVariable(variable.c_str());
      if(value.IsValid()) {
        lldb::SBStream stream;
        value.GetDescription(stream);
        variable_value=stream.GetData();
      }
    }
  }
  event_mutex.unlock();
  return variable_value;
}

bool Debug::is_invalid() {
  bool invalid;
  event_mutex.lock();
  invalid=state==lldb::StateType::eStateInvalid;
  event_mutex.unlock();
  return invalid;
}

bool Debug::is_stopped() {
  bool stopped;
  event_mutex.lock();
  stopped=state==lldb::StateType::eStateStopped;
  event_mutex.unlock();
  return stopped;
}

bool Debug::is_running() {
  bool running;
  event_mutex.lock();
  running=state==lldb::StateType::eStateRunning;
  event_mutex.unlock();
  return running;
}

void Debug::add_breakpoint(const boost::filesystem::path &file_path, int line_nr) {
  event_mutex.lock();
  if(state==lldb::eStateStopped || state==lldb::eStateRunning) {
    if(!(process->GetTarget().BreakpointCreateByLocation(file_path.string().c_str(), line_nr)).IsValid())
      Terminal::get().async_print("Error (debug): Could not create breakpoint at: "+file_path.string()+":"+std::to_string(line_nr)+'\n', true);
  }
  event_mutex.unlock();
}

void Debug::remove_breakpoint(const boost::filesystem::path &file_path, int line_nr, int line_count) {
  event_mutex.lock();
  if(state==lldb::eStateStopped || state==lldb::eStateRunning) {
    auto target=process->GetTarget();
    for(int line_nr_try=line_nr;line_nr_try<line_count;line_nr_try++) {
      for(uint32_t b_index=0;b_index<target.GetNumBreakpoints();b_index++) {
        auto breakpoint=target.GetBreakpointAtIndex(b_index);
        for(uint32_t l_index=0;l_index<breakpoint.GetNumLocations();l_index++) {
          auto line_entry=breakpoint.GetLocationAtIndex(l_index).GetAddress().GetLineEntry();
          if(line_entry.GetLine()==static_cast<uint32_t>(line_nr_try)) {
            auto file_spec=line_entry.GetFileSpec();
            boost::filesystem::path breakpoint_path=file_spec.GetDirectory();
            breakpoint_path/=file_spec.GetFilename();
            if(breakpoint_path==file_path) {
              if(!target.BreakpointDelete(breakpoint.GetID()))
                Terminal::get().async_print("Error (debug): Could not delete breakpoint at: "+file_path.string()+":"+std::to_string(line_nr)+'\n', true);
              event_mutex.unlock();
              return;
            }
          }
        }
      }
    }
  }
  event_mutex.unlock();
}

void Debug::write(const std::string &buffer) {
  event_mutex.lock();
  if(state==lldb::StateType::eStateRunning) {
    process->PutSTDIN(buffer.c_str(), buffer.size());
  }
  event_mutex.unlock();
}
