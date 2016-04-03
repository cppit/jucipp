#include "cmake.h"
#include "filesystem.h"
#include "dialogs.h"
#include "config.h"
#include "terminal.h"
#include <boost/regex.hpp>

CMake::CMake(const boost::filesystem::path &path) {
  const auto find_cmake_project=[this](const boost::filesystem::path &cmake_path) {
    for(auto &line: filesystem::read_lines(cmake_path)) {
      const boost::regex project_regex("^ *project *\\(.*$", boost::regex::icase);
      boost::smatch sm;
      if(boost::regex_match(line, sm, project_regex))
        return true;
    }
    return false;
  };
  
  auto search_path=boost::filesystem::is_directory(path)?path:path.parent_path();
  while(true) {
    auto search_cmake_path=search_path/"CMakeLists.txt";
    if(boost::filesystem::exists(search_cmake_path)) {
      paths.emplace(paths.begin(), search_cmake_path);
      if(find_cmake_project(search_cmake_path)) {
        project_path=search_path;
        break;
      }
    }
    if(search_path==search_path.root_directory())
      break;
    search_path=search_path.parent_path();
  }
}

bool CMake::update_default_build(const boost::filesystem::path &default_build_path, bool force) {
  if(project_path.empty())
    return false;
  
  if(!boost::filesystem::exists(project_path/"CMakeLists.txt"))
    return false;
  
  if(default_build_path.empty())
    return false;
  if(!boost::filesystem::exists(default_build_path)) {
    boost::system::error_code ec;
    boost::filesystem::create_directories(default_build_path, ec);
    if(ec) {
      Terminal::get().print("Error: could not create "+default_build_path.string()+": "+ec.message()+"\n", true);
      return false;
    }
  }
  
  if(!force && boost::filesystem::exists(default_build_path/"compile_commands.json"))
    return true;
  
  auto compile_commands_path=default_build_path/"compile_commands.json";
  Dialog::Message message("Creating/updating default build");
  auto exit_status=Terminal::get().process(Config::get().project.cmake_command+" "+
                                           filesystem::escape_argument(project_path)+" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON", default_build_path);
  message.hide();
  if(exit_status==EXIT_SUCCESS) {
#ifdef _WIN32 //Temporary fix to MSYS2's libclang
    auto compile_commands_file=filesystem::read(compile_commands_path);
    size_t pos=0;
    while((pos=compile_commands_file.find("-I/", pos))!=std::string::npos) {
      if(pos+3<compile_commands_file.size()) {
        std::string drive;
        drive+=compile_commands_file[pos+3];
        compile_commands_file.replace(pos, 4, "-I"+drive+":");
      }
      else
        break;
    }
    filesystem::write(compile_commands_path, compile_commands_file);
#endif
    return true;
  }
  return false;
}

bool CMake::update_debug_build(const boost::filesystem::path &debug_build_path, bool force) {
  if(project_path.empty())
    return false;
  
  if(!boost::filesystem::exists(project_path/"CMakeLists.txt"))
    return false;
  
  if(debug_build_path.empty())
    return false;
  if(!boost::filesystem::exists(debug_build_path)) {
    boost::system::error_code ec;
    boost::filesystem::create_directories(debug_build_path, ec);
    if(ec) {
      Terminal::get().print("Error: could not create "+debug_build_path.string()+": "+ec.message()+"\n", true);
      return false;
    }
  }
  
  if(!force && boost::filesystem::exists(debug_build_path/"CMakeCache.txt"))
    return true;
  
  std::unique_ptr<Dialog::Message> message;
  message=std::unique_ptr<Dialog::Message>(new Dialog::Message("Creating/updating debug build"));
  auto exit_status=Terminal::get().process(Config::get().project.cmake_command+" "+
                                           filesystem::escape_argument(project_path)+" -DCMAKE_BUILD_TYPE=Debug", debug_build_path);
  if(message)
    message->hide();
  if(exit_status==EXIT_SUCCESS)
    return true;
  return false;
}

boost::filesystem::path CMake::get_executable(const boost::filesystem::path &file_path) {
  auto executables = get_functions_parameters("add_executable");
  
  //Attempt to find executable based add_executable files and opened tab
  boost::filesystem::path executable_path;
  if(!file_path.empty()) {
    for(auto &executable: executables) {
      if(executable.second.size()>1) {
        for(size_t c=1;c<executable.second.size();c++) {
          if(executable.second[c]==file_path.filename()) {
            executable_path=executable.first.parent_path()/executable.second[0];
            break;
          }
        }
      }
      if(!executable_path.empty())
        break;
    }
  }
  if(executable_path.empty() && executables.size()>0 && executables[0].second.size()>0)
    executable_path=executables[0].first.parent_path()/executables[0].second[0];
  
  return executable_path;
}

void CMake::read_files() {
  for(auto &path: paths)
    files.emplace_back(filesystem::read(path));
}

void CMake::remove_tabs() {
  for(auto &file: files) {
    for(auto &chr: file) {
      if(chr=='\t')
        chr=' ';
    }
  }
}

void CMake::remove_comments() {
  for(auto &file: files) {
    size_t pos=0;
    size_t comment_start;
    bool inside_comment=false;
    while(pos<file.size()) {
      if(!inside_comment && file[pos]=='#') {
        comment_start=pos;
        inside_comment=true;
      }
      if(inside_comment && file[pos]=='\n') {
        file.erase(comment_start, pos-comment_start);
        pos-=pos-comment_start;
        inside_comment=false;
      }
      pos++;
    }
    if(inside_comment)
      file.erase(comment_start);
  }
}

void CMake::remove_newlines_inside_parentheses() {
  for(auto &file: files) {
    size_t pos=0;
    bool inside_para=false;
    bool inside_quote=false;
    char last_char=0;
    while(pos<file.size()) {
      if(!inside_quote && file[pos]=='"' && last_char!='\\')
        inside_quote=true;
      else if(inside_quote && file[pos]=='"' && last_char!='\\')
        inside_quote=false;

      else if(!inside_quote && file[pos]=='(')
        inside_para=true;
      else if(!inside_quote && file[pos]==')')
        inside_para=false;

      else if(inside_para && file[pos]=='\n')
        file.replace(pos, 1, 1, ' ');
      last_char=file[pos];
      pos++;
    }
  }
}

void CMake::find_variables() {
  for(auto &file: files) {
    size_t pos=0;
    while(pos<file.size()) {
      auto start_line=pos;
      auto end_line=file.find('\n', start_line);
      if(end_line==std::string::npos)
        end_line=file.size();
      if(end_line>start_line) {
        auto line=file.substr(start_line, end_line-start_line);
        boost::smatch sm;
        const boost::regex set_regex("^ *set *\\( *([A-Za-z_][A-Za-z_0-9]*) +(.*)\\) *$", boost::regex::icase);
        if(boost::regex_match(line, sm, set_regex)) {
          auto data=sm[2].str();
          while(data.size()>0 && data.back()==' ')
            data.pop_back();
          parse_variable_parameters(data);
          variables[sm[1].str()]=data;
        }
        else {
          const boost::regex project_regex("^ *project *\\( *([^ ]+).*\\) *$", boost::regex::icase);
          if(boost::regex_match(line, sm, project_regex)) {
            auto data=sm[1].str();
            parse_variable_parameters(data);
            variables["CMAKE_PROJECT_NAME"]=data; //TODO: is this variable deprecated/non-standard?
            variables["PROJECT_NAME"]=data;
          }
        }
      }
      pos=end_line+1;
    }
  }
}

void CMake::parse_variable_parameters(std::string &data) {
  size_t pos=0;
  bool inside_quote=false;
  char last_char=0;
  while(pos<data.size()) {
    if(!inside_quote && data[pos]=='"' && last_char!='\\') {
      inside_quote=true;
      data.erase(pos, 1); //TODO: instead remove quote-mark if pasted into a quote, for instance: "test${test}test"<-remove quotes from ${test}
      pos--;
    }
    else if(inside_quote && data[pos]=='"' && last_char!='\\') {
      inside_quote=false;
      data.erase(pos, 1); //TODO: instead remove quote-mark if pasted into a quote, for instance: "test${test}test"<-remove quotes from ${test}
      pos--;
    }
    else if(!inside_quote && data[pos]==' ' && pos+1<data.size() && data[pos+1]==' ') {
      data.erase(pos, 1);
      pos--;
    }
    
    last_char=data[pos];
    pos++;
  }
  for(auto &var: variables) {
    auto pos=data.find("${"+var.first+'}');
    while(pos!=std::string::npos) {
      data.replace(pos, var.first.size()+3, var.second);
      pos=data.find("${"+var.first+'}');
    }
  }
  
  //Remove variables we do not know:
  pos=data.find("${");
  auto pos_end=data.find("}", pos+2);
  while(pos!=std::string::npos && pos_end!=std::string::npos) {
    data.erase(pos, pos_end-pos+1);
    pos=data.find("${");
    pos_end=data.find("}", pos+2);
  }
}

void CMake::parse() {
  read_files();
  remove_tabs();
  remove_comments();
  remove_newlines_inside_parentheses();
  find_variables();
  parsed=true;
}

std::vector<std::string> CMake::get_function_parameters(std::string &data) {
  std::vector<std::string> parameters;
  size_t pos=0;
  size_t parameter_pos=0;
  bool inside_quote=false;
  char last_char=0;
  while(pos<data.size()) {
    if(!inside_quote && data[pos]=='"' && last_char!='\\') {
      inside_quote=true;
      data.erase(pos, 1);
      pos--;
    }
    else if(inside_quote && data[pos]=='"' && last_char!='\\') {
      inside_quote=false;
      data.erase(pos, 1);
      pos--;
    }
    else if(!inside_quote && pos+1<data.size() && data[pos]==' ' && data[pos+1]==' ') {
      data.erase(pos, 1);
      pos--;
    }
    else if(!inside_quote && data[pos]==' ') {
      parameters.emplace_back(data.substr(parameter_pos, pos-parameter_pos));
      if(pos+1<data.size())
        parameter_pos=pos+1;
    }
    
    last_char=data[pos];
    pos++;
  }
  parameters.emplace_back(data.substr(parameter_pos));
  for(auto &var: variables) {
    for(auto &parameter: parameters) {
      auto pos=parameter.find("${"+var.first+'}');
      while(pos!=std::string::npos) {
        parameter.replace(pos, var.first.size()+3, var.second);
        pos=parameter.find("${"+var.first+'}');
      }
    }
  }
  return parameters;
}

std::vector<std::pair<boost::filesystem::path, std::vector<std::string> > > CMake::get_functions_parameters(const std::string &name) {
  if(!parsed)
    parse();
  std::vector<std::pair<boost::filesystem::path, std::vector<std::string> > > functions;
  size_t file_c=0;
  for(auto &file: files) {
    size_t pos=0;
    while(pos<file.size()) {
      auto start_line=pos;
      auto end_line=file.find('\n', start_line);
      if(end_line==std::string::npos)
        end_line=file.size();
      if(end_line>start_line) {
        auto line=file.substr(start_line, end_line-start_line);
        const boost::regex function_regex("^ *"+name+" *\\( *(.*)\\) *$", boost::regex::icase);
        boost::smatch sm;
        if(boost::regex_match(line, sm, function_regex)) {
          auto data=sm[1].str();
          while(data.size()>0 && data.back()==' ')
            data.pop_back();
          auto parameters=get_function_parameters(data);
          functions.emplace(functions.begin(), paths[file_c], parameters);
        }
      }
      pos=end_line+1;
    }
    file_c++;
  }
  return functions;
}
