#include "debug_lldb.h"
#include <stdio.h>
#ifdef __APPLE__
#include <stdlib.h>
#endif
#include <boost/filesystem.hpp>
#include <iostream>
#include "terminal.h"
#include "filesystem.h"
#include "process.hpp"
#include "config.h"

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
  std::cout << "debugger log: " << msg << std::endl;
}

Debug::LLDB::LLDB(): state(lldb::StateType::eStateInvalid), buffer_size(131072) {
#ifdef __APPLE__
  if(!getenv("LLDB_DEBUGSERVER_PATH")) {
    std::string debug_server_path("/usr/local/opt/llvm/bin/debugserver");
    if(boost::filesystem::exists(debug_server_path))
      setenv("LLDB_DEBUGSERVER_PATH", debug_server_path.c_str(), 0);
  }
#elif __linux
  if(!getenv("LLDB_DEBUGSERVER_PATH")) {
    std::string debug_server_path("/usr/bin/lldb-server");
    if(!boost::filesystem::exists(debug_server_path)) {
      auto versions={"4.0", "3.9", "3.8", "3.7", "3.6", "3.5"};
      for(auto &version: versions) {
        auto corrected_debug_server_path=debug_server_path+'-'+version;
        if(boost::filesystem::exists(corrected_debug_server_path)) {
          setenv("LLDB_DEBUGSERVER_PATH", corrected_debug_server_path.c_str(), 0);
          break;
        }
      }
    }
  }
#endif
}

void Debug::LLDB::start(const std::string &command, const boost::filesystem::path &path,
                  const std::vector<std::pair<boost::filesystem::path, int> > &breakpoints,
                  std::function<void(int exit_status)> callback,
                  std::function<void(const std::string &status)> status_callback,
                  std::function<void(const boost::filesystem::path &file_path, int line_nr, int line_index)> stop_callback,
                  const std::string &remote_host) {
  if(!debugger) {
    lldb::SBDebugger::Initialize();
    debugger=std::make_unique<lldb::SBDebugger>(lldb::SBDebugger::Create(true, log, nullptr));
    listener=std::make_unique<lldb::SBListener>("juCi++ lldb listener");
  }
  
  //Create executable string and argument array
  std::string executable;
  std::vector<std::string> arguments;
  size_t start_pos=std::string::npos;
  bool quote=false;
  bool double_quote=false;
  bool symbol=false;
  for(size_t c=0;c<=command.size();c++) { 
    if(c==command.size() || (!quote && !double_quote && !symbol && command[c]==' ')) {
      if(c>0 && start_pos!=std::string::npos) {
        if(executable.empty()) {
          executable=filesystem::unescape_argument(command.substr(start_pos, c-start_pos));
#ifdef _WIN32
          if(remote_host.empty())
            executable+=".exe";
#endif
        }
        else
          arguments.emplace_back(filesystem::unescape_argument(command.substr(start_pos, c-start_pos)));
        start_pos=std::string::npos;
      }
    }
    else if(command[c]=='\\' && !quote && !double_quote)
      symbol=true;
    else if(symbol)
      symbol=false;
    else if(command[c]=='\'' && !double_quote)
      quote=!quote;
    else if(command[c]=='"' && !quote)
      double_quote=!double_quote;
    if(c<command.size() && start_pos==std::string::npos && command[c]!=' ')
      start_pos=c;
  }
  const char *argv[arguments.size()+1];
  for(size_t c=0;c<arguments.size();c++)
    argv[c]=arguments[c].c_str();
  argv[arguments.size()]=nullptr;
  
  auto target=debugger->CreateTarget(executable.c_str());
  if(!target.IsValid()) {
    Terminal::get().async_print("Error (debug): Could not create debug target to: "+executable+'\n', true);
    if(callback)
      callback(-1);
    return;
  }
  
  //Set breakpoints
  for(auto &breakpoint: breakpoints) {
    if(!(target.BreakpointCreateByLocation(breakpoint.first.string().c_str(), breakpoint.second)).IsValid()) {
      Terminal::get().async_print("Error (debug): Could not create breakpoint at: "+breakpoint.first.string()+":"+std::to_string(breakpoint.second)+'\n', true);
      if(callback)
        callback(-1);
      return;
    }
  }
  
  lldb::SBError error;
  if(!remote_host.empty()) {
    auto connect_string="connect://"+remote_host;
    process = std::make_unique<lldb::SBProcess>(target.ConnectRemote(*listener, connect_string.c_str(), "gdb-remote", error));
    if(error.Fail()) {
      Terminal::get().async_print(std::string("Error (debug): ")+error.GetCString()+'\n', true);
      if(callback)
        callback(-1);
      return;
    }
    lldb::SBEvent event;
    while(true) {
      if(listener->GetNextEvent(event)) {
        if((event.GetType() & lldb::SBProcess::eBroadcastBitStateChanged)>0) {
          auto state=process->GetStateFromEvent(event);
          this->state=state;
          if(state==lldb::StateType::eStateConnected)
            break;
        }
      }
    }
    const char *empty_parameter[1];
    empty_parameter[0]=nullptr;
    process->RemoteLaunch(argv, empty_parameter, nullptr, nullptr, nullptr, nullptr, lldb::eLaunchFlagNone, false, error);
    if(!error.Fail())
      process->Continue();
  }
  else
    process = std::make_unique<lldb::SBProcess>(target.Launch(*listener, argv, const_cast<const char**>(environ), nullptr, nullptr, nullptr, path.string().c_str(), lldb::eLaunchFlagNone, false, error));
  if(error.Fail()) {
    Terminal::get().async_print(std::string("Error (debug): ")+error.GetCString()+'\n', true);
    if(callback)
      callback(-1);
    return;
  }
  if(debug_thread.joinable())
    debug_thread.join();
  debug_thread=std::thread([this, callback, status_callback, stop_callback]() {
    lldb::SBEvent event;
    while(true) {
      std::unique_lock<std::mutex> lock(event_mutex);
      if(listener->GetNextEvent(event)) {
        if((event.GetType() & lldb::SBProcess::eBroadcastBitStateChanged)>0) {
          auto state=process->GetStateFromEvent(event);
          this->state=state;
          
          if(state==lldb::StateType::eStateStopped) {
            for(uint32_t c=0;c<process->GetNumThreads();c++) {
              auto thread=process->GetThreadAtIndex(c);
              if(thread.GetStopReason()>=2) {
                process->SetSelectedThreadByIndexID(thread.GetIndexID());
                break;
              }
            }
          }
          
          //Update debug status
          lldb::SBStream stream;
          event.GetDescription(stream);
          std::string event_desc=stream.GetData();
          event_desc.pop_back();
          auto pos=event_desc.rfind(" = ");
          if(status_callback && pos!=std::string::npos) {
            auto status=event_desc.substr(pos+3);
            if(state==lldb::StateType::eStateStopped) {
              char buffer[100];
              auto thread=process->GetSelectedThread();
              auto n=thread.GetStopDescription(buffer, 100);
              if(n>0)
                status+=" ("+std::string(buffer, n<=100?n:100)+")";
              auto line_entry=thread.GetSelectedFrame().GetLineEntry();
              if(line_entry.IsValid()) {
                lldb::SBStream stream;
                line_entry.GetFileSpec().GetDescription(stream);
                status +=" "+boost::filesystem::path(stream.GetData()).filename().string()+":"+std::to_string(line_entry.GetLine());
              }
            }
            status_callback(status);
          }
          
          if(state==lldb::StateType::eStateStopped) {
            if(stop_callback) {
              auto line_entry=process->GetSelectedThread().GetSelectedFrame().GetLineEntry();
              if(line_entry.IsValid()) {
                lldb::SBStream stream;
                line_entry.GetFileSpec().GetDescription(stream);
                auto column=line_entry.GetColumn();
                if(column==0)
                  column=1;
                stop_callback(filesystem::get_normal_path(stream.GetData()), line_entry.GetLine(), column);
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
      lock.unlock();
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
  });
}

void Debug::LLDB::continue_debug() {
  std::unique_lock<std::mutex> lock(event_mutex);
  if(state==lldb::StateType::eStateStopped)
    process->Continue();
}

void Debug::LLDB::stop() {
  std::unique_lock<std::mutex> lock(event_mutex);
  if(state==lldb::StateType::eStateRunning) {
    auto error=process->Stop();
    if(error.Fail())
      Terminal::get().async_print(std::string("Error (debug): ")+error.GetCString()+'\n', true);
  }
}

void Debug::LLDB::kill() {
  std::unique_lock<std::mutex> lock(event_mutex);
  if(process) {
    auto error=process->Kill();
    if(error.Fail())
      Terminal::get().async_print(std::string("Error (debug): ")+error.GetCString()+'\n', true);
  }
}

void Debug::LLDB::step_over() {
  std::unique_lock<std::mutex> lock(event_mutex);
  if(state==lldb::StateType::eStateStopped) {
    process->GetSelectedThread().StepOver();
  }
}

void Debug::LLDB::step_into() {
  std::unique_lock<std::mutex> lock(event_mutex);
  if(state==lldb::StateType::eStateStopped) {
    process->GetSelectedThread().StepInto();
  }
}

void Debug::LLDB::step_out() {
  std::unique_lock<std::mutex> lock(event_mutex);
  if(state==lldb::StateType::eStateStopped) {
    process->GetSelectedThread().StepOut();
  }
}

std::pair<std::string, std::string> Debug::LLDB::run_command(const std::string &command) {
  std::pair<std::string, std::string> command_return;
  std::unique_lock<std::mutex> lock(event_mutex);
  if(state==lldb::StateType::eStateStopped || state==lldb::StateType::eStateRunning) {
    lldb::SBCommandReturnObject command_return_object;
    debugger->GetCommandInterpreter().HandleCommand(command.c_str(), command_return_object, true);
    command_return.first=command_return_object.GetOutput();
    command_return.second=command_return_object.GetError();
  }
  return command_return;
}

std::vector<Debug::LLDB::Frame> Debug::LLDB::get_backtrace() {
  std::vector<Frame> backtrace;
  std::unique_lock<std::mutex> lock(event_mutex);
  if(state==lldb::StateType::eStateStopped) {
    auto thread=process->GetSelectedThread();
    for(uint32_t c_f=0;c_f<thread.GetNumFrames();c_f++) {
      Frame backtrace_frame;
      auto frame=thread.GetFrameAtIndex(c_f);
      
      backtrace_frame.index=c_f;
      
      if(frame.GetFunctionName()!=nullptr)
        backtrace_frame.function_name=frame.GetFunctionName();
      
      auto module_filename=frame.GetModule().GetFileSpec().GetFilename();
      if(module_filename!=nullptr) {
        backtrace_frame.module_filename=module_filename;
      }
      
      auto line_entry=frame.GetLineEntry();
      if(line_entry.IsValid()) {
        lldb::SBStream stream;
        line_entry.GetFileSpec().GetDescription(stream);
        auto column=line_entry.GetColumn();
        if(column==0)
          column=1;
        backtrace_frame.file_path=filesystem::get_normal_path(stream.GetData());
        backtrace_frame.line_nr=line_entry.GetLine();
        backtrace_frame.line_index=column;
      }
      backtrace.emplace_back(backtrace_frame);
    }
  }
  return backtrace;
}

std::vector<Debug::LLDB::Variable> Debug::LLDB::get_variables() {
  std::vector<Debug::LLDB::Variable> variables;
  std::unique_lock<std::mutex> lock(event_mutex);
  if(state==lldb::StateType::eStateStopped) {
    for(uint32_t c_t=0;c_t<process->GetNumThreads();c_t++) {
      auto thread=process->GetThreadAtIndex(c_t);
      for(uint32_t c_f=0;c_f<thread.GetNumFrames();c_f++) {
        auto frame=thread.GetFrameAtIndex(c_f);
        auto values=frame.GetVariables(true, true, true, false);
        for(uint32_t value_index=0;value_index<values.GetSize();value_index++) {
          lldb::SBStream stream;
          auto value=values.GetValueAtIndex(value_index);
        
          Debug::LLDB::Variable variable;
          variable.thread_index_id=thread.GetIndexID();
          variable.frame_index=c_f;
          if(value.GetName()!=nullptr)
            variable.name=value.GetName();
          value.GetDescription(stream);
          variable.value=stream.GetData();
          
          auto declaration=value.GetDeclaration();
          if(declaration.IsValid()) {
            variable.declaration_found=true;
            variable.line_nr=declaration.GetLine();
            variable.line_index=declaration.GetColumn();
            if(variable.line_index==0)
              variable.line_index=1;
            
            auto file_spec=declaration.GetFileSpec();
            variable.file_path=filesystem::get_normal_path(file_spec.GetDirectory());
            variable.file_path/=file_spec.GetFilename();
          }
          else {
            variable.declaration_found=false;
            auto line_entry=frame.GetLineEntry();
            if(line_entry.IsValid()) {
              variable.line_nr=line_entry.GetLine();
              variable.line_index=line_entry.GetColumn();
              if(variable.line_index==0)
                variable.line_index=1;
              
              auto file_spec=line_entry.GetFileSpec();
              variable.file_path=filesystem::get_normal_path(file_spec.GetDirectory());
              variable.file_path/=file_spec.GetFilename();
            }
          }
          variables.emplace_back(variable);
        }
      }
    }
  }
  return variables;
}

void Debug::LLDB::select_frame(uint32_t frame_index, uint32_t thread_index_id) {
  std::unique_lock<std::mutex> lock(event_mutex);
  if(state==lldb::StateType::eStateStopped) {
    if(thread_index_id!=0)
      process->SetSelectedThreadByIndexID(thread_index_id);
    process->GetSelectedThread().SetSelectedFrame(frame_index);;
  }
}

void Debug::LLDB::cancel() {
  kill();
  if(debug_thread.joinable())
    debug_thread.join();
}

std::string Debug::LLDB::get_value(const std::string &variable, const boost::filesystem::path &file_path, unsigned int line_nr, unsigned int line_index) {
  std::string variable_value;
  std::unique_lock<std::mutex> lock(event_mutex);
  if(state==lldb::StateType::eStateStopped) {
    auto frame=process->GetSelectedThread().GetSelectedFrame();
    
    auto values=frame.GetVariables(true, true, true, false);
    //First try to find variable based on name, file and line number
    for(uint32_t value_index=0;value_index<values.GetSize();value_index++) {
      lldb::SBStream stream;
      auto value=values.GetValueAtIndex(value_index);

      if(value.GetName()!=nullptr && value.GetName()==variable) {
        auto declaration=value.GetDeclaration();
        if(declaration.IsValid()) {
          if(declaration.GetLine()==line_nr && (declaration.GetColumn()==0 || declaration.GetColumn()==line_index)) {
            auto file_spec=declaration.GetFileSpec();
            auto value_decl_path=filesystem::get_normal_path(file_spec.GetDirectory());
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
      auto value=frame.GetValueForVariablePath(variable.c_str());
      if(value.IsValid()) {
        lldb::SBStream stream;
        value.GetDescription(stream);
        variable_value=stream.GetData();
      }
    }
  }
  return variable_value;
}

std::string Debug::LLDB::get_return_value(const boost::filesystem::path &file_path, unsigned int line_nr, unsigned int line_index) {
  std::string return_value;
  std::unique_lock<std::mutex> lock(event_mutex);
  if(state==lldb::StateType::eStateStopped) {
    auto thread=process->GetSelectedThread();
    auto thread_return_value=thread.GetStopReturnValue();
    if(thread_return_value.IsValid()) {
      auto line_entry=thread.GetSelectedFrame().GetLineEntry();
      if(line_entry.IsValid()) {
        lldb::SBStream stream;
        line_entry.GetFileSpec().GetDescription(stream);
        if(filesystem::get_normal_path(stream.GetData())==file_path && line_entry.GetLine()==line_nr &&
           (line_entry.GetColumn()==0 || line_entry.GetColumn()==line_index)) {
          lldb::SBStream stream;
          thread_return_value.GetDescription(stream);
          return_value=stream.GetData();
        }
      }
    }
  }
  return return_value;
}

bool Debug::LLDB::is_invalid() {
  std::unique_lock<std::mutex> lock(event_mutex);
  return state==lldb::StateType::eStateInvalid;
}

bool Debug::LLDB::is_stopped() {
  std::unique_lock<std::mutex> lock(event_mutex);
  return state==lldb::StateType::eStateStopped;
}

bool Debug::LLDB::is_running() {
  std::unique_lock<std::mutex> lock(event_mutex);
  return state==lldb::StateType::eStateRunning;
}

void Debug::LLDB::add_breakpoint(const boost::filesystem::path &file_path, int line_nr) {
  std::unique_lock<std::mutex> lock(event_mutex);
  if(state==lldb::eStateStopped || state==lldb::eStateRunning) {
    if(!(process->GetTarget().BreakpointCreateByLocation(file_path.string().c_str(), line_nr)).IsValid())
      Terminal::get().async_print("Error (debug): Could not create breakpoint at: "+file_path.string()+":"+std::to_string(line_nr)+'\n', true);
  }
}

void Debug::LLDB::remove_breakpoint(const boost::filesystem::path &file_path, int line_nr, int line_count) {
  std::unique_lock<std::mutex> lock(event_mutex);
  if(state==lldb::eStateStopped || state==lldb::eStateRunning) {
    auto target=process->GetTarget();
    for(int line_nr_try=line_nr;line_nr_try<line_count;line_nr_try++) {
      for(uint32_t b_index=0;b_index<target.GetNumBreakpoints();b_index++) {
        auto breakpoint=target.GetBreakpointAtIndex(b_index);
        for(uint32_t l_index=0;l_index<breakpoint.GetNumLocations();l_index++) {
          auto line_entry=breakpoint.GetLocationAtIndex(l_index).GetAddress().GetLineEntry();
          if(line_entry.GetLine()==static_cast<uint32_t>(line_nr_try)) {
            auto file_spec=line_entry.GetFileSpec();
            auto breakpoint_path=filesystem::get_normal_path(file_spec.GetDirectory());
            breakpoint_path/=file_spec.GetFilename();
            if(breakpoint_path==file_path) {
              if(!target.BreakpointDelete(breakpoint.GetID()))
                Terminal::get().async_print("Error (debug): Could not delete breakpoint at: "+file_path.string()+":"+std::to_string(line_nr)+'\n', true);
              return;
            }
          }
        }
      }
    }
  }
}

void Debug::LLDB::write(const std::string &buffer) {
  std::unique_lock<std::mutex> lock(event_mutex);
  if(state==lldb::StateType::eStateRunning) {
    process->PutSTDIN(buffer.c_str(), buffer.size());
  }
}
