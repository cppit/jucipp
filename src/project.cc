#include "project.h"
#include "config.h"
#include "terminal.h"
#include "filesystem.h"

std::unordered_map<std::string, std::string> Project::run_arguments;
std::unordered_map<std::string, std::string> Project::debug_run_arguments;
std::atomic<bool> Project::compiling(false);
std::atomic<bool> Project::debugging(false);

std::unique_ptr<CMake> ProjectClang::get_cmake() {
  boost::filesystem::path path;
  if(!file_path.empty())
    path=file_path.parent_path();
  else
    path=Directories::get().current_path;
  if(path.empty())
    return nullptr;
  auto cmake=std::unique_ptr<CMake>(new CMake(path));
  if(cmake->project_path.empty())
    return nullptr;
  if(!CMake::create_default_build(cmake->project_path))
    return nullptr;
  return cmake;
}

std::pair<std::string, std::string> ProjectClang::get_run_arguments() {
  auto cmake=get_cmake();
  if(!cmake)
    return {"", ""};
  
  auto project_path=cmake->project_path.string();
  auto run_arguments_it=run_arguments.find(project_path);
  std::string arguments;
  if(run_arguments_it!=run_arguments.end())
    arguments=run_arguments_it->second;
  
  if(arguments.empty()) {
    auto executable=cmake->get_executable(file_path).string();
    
    if(executable!="") {
      auto project_path=cmake->project_path;
      auto default_build_path=CMake::get_default_build_path(project_path);
      if(!default_build_path.empty()) {
        size_t pos=executable.find(project_path.string());
        if(pos!=std::string::npos)
          executable.replace(pos, project_path.string().size(), default_build_path.string());
      }
      arguments=filesystem::escape_argument(executable);
    }
    else
      arguments=filesystem::escape_argument(CMake::get_default_build_path(cmake->project_path));
  }
  
  return {project_path, arguments};
}

void ProjectClang::compile() {    
  auto cmake=get_cmake();
  if(!cmake)
    return;
  
  auto default_build_path=CMake::get_default_build_path(cmake->project_path);
  if(default_build_path.empty())
    return;
  compiling=true;
  Terminal::get().print("Compiling project "+cmake->project_path.string()+"\n");
  Terminal::get().async_process(Config::get().terminal.make_command, default_build_path, [this](int exit_status) {
    compiling=false;
  });
}

void ProjectClang::compile_and_run() {
  auto cmake=get_cmake();
  if(!cmake)
    return;
  auto project_path=cmake->project_path;
  
  auto default_build_path=CMake::get_default_build_path(project_path);
  if(default_build_path.empty())
    return;
  
  auto run_arguments_it=run_arguments.find(project_path.string());
  std::string arguments;
  if(run_arguments_it!=run_arguments.end())
    arguments=run_arguments_it->second;
  
  if(arguments.empty()) {
    arguments=cmake->get_executable(file_path).string();
    if(arguments.empty()) {
      Terminal::get().print("Could not find add_executable in the following paths:\n");
      for(auto &path: cmake->paths)
        Terminal::get().print("  "+path.string()+"\n");
      Terminal::get().print("Solution: either use Project Set Run Arguments, or open a source file within a directory where add_executable is set.\n", true);
      return;
    }
    size_t pos=arguments.find(project_path.string());
    if(pos!=std::string::npos)
      arguments.replace(pos, project_path.string().size(), default_build_path.string());
    arguments=filesystem::escape_argument(arguments);
  }
  
  compiling=true;
  Terminal::get().print("Compiling and running "+arguments+"\n");
  Terminal::get().async_process(Config::get().terminal.make_command, default_build_path, [this, arguments, default_build_path](int exit_status){
    compiling=false;
    if(exit_status==EXIT_SUCCESS) {
      Terminal::get().async_process(arguments, default_build_path, [this, arguments](int exit_status){
        Terminal::get().async_print(arguments+" returned: "+std::to_string(exit_status)+'\n');
      });
    }
  });
}
