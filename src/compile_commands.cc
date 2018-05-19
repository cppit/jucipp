#include "compile_commands.h"
#include "clangmm.h"
#include <boost/property_tree/json_parser.hpp>
#include <regex>

std::vector<std::string> CompileCommands::Command::parameter_values(const std::string &parameter_name) const {
  std::vector<std::string> parameter_values;
  
  bool found_argument=false;
  for(auto &parameter: parameters) {
    if(found_argument) {
      parameter_values.emplace_back(parameter);
      found_argument=false;
    }
    else if(parameter==parameter_name)
      found_argument=true;
  }
  
  return parameter_values;
}

CompileCommands::CompileCommands(const boost::filesystem::path &build_path) {
  try {
    boost::property_tree::ptree root_pt;
    boost::property_tree::json_parser::read_json((build_path/"compile_commands.json").string(), root_pt);
    
    auto commands_pt=root_pt.get_child("");
    for(auto &command: commands_pt) {
      boost::filesystem::path directory=command.second.get<std::string>("directory");
      auto parameters_str=command.second.get<std::string>("command");
      boost::filesystem::path file=command.second.get<std::string>("file");
      
      std::vector<std::string> parameters;
      bool backslash=false;
      bool single_quote=false;
      bool double_quote=false;
      size_t parameter_start_pos=std::string::npos;
      size_t parameter_size=0;
      auto add_parameter=[&parameters, &parameters_str, &parameter_start_pos, &parameter_size] {
        auto parameter=parameters_str.substr(parameter_start_pos, parameter_size);
        // Remove escaping
        for(size_t c=0;c<parameter.size()-1;++c) {
          if(parameter[c]=='\\')
            parameter.replace(c, 2, std::string()+parameter[c+1]);
        }
        parameters.emplace_back(parameter);
      };
      for(size_t c=0;c<parameters_str.size();++c) {
        if(backslash)
          backslash=false;
        else if(parameters_str[c]=='\\')
          backslash=true;
        else if((parameters_str[c]==' ' || parameters_str[c]=='\t') && !backslash && !single_quote && !double_quote) {
          if(parameter_start_pos!=std::string::npos) {
            add_parameter();
            parameter_start_pos=std::string::npos;
            parameter_size=0;
          }
          continue;
        }
        else if(parameters_str[c]=='\'' && !backslash && !double_quote) {
          single_quote=!single_quote;
          continue;
        }
        else if(parameters_str[c]=='\"' && !backslash && !single_quote) {
          double_quote=!double_quote;
          continue;
        }
        
        if(parameter_start_pos==std::string::npos)
          parameter_start_pos=c;
        ++parameter_size;
      }
      if(parameter_start_pos!=std::string::npos)
        add_parameter();
      
      commands.emplace_back(Command{directory, parameters, boost::filesystem::absolute(file, build_path)});
    }
  }
  catch(...) {}
}

std::vector<std::string> CompileCommands::get_arguments(const boost::filesystem::path &build_path, const boost::filesystem::path &file_path) {
  std::string default_std_argument="-std=c++1y";
  
  std::vector<std::string> arguments;
  if(!build_path.empty()) {
    clangmm::CompilationDatabase db(build_path.string());
    if(db) {
      clangmm::CompileCommands commands(file_path.string(), db);
      auto cmds = commands.get_commands();
      for (auto &cmd : cmds) {
        auto cmd_arguments = cmd.get_arguments();
        bool ignore_next=false;
        for (size_t c = 1; c < cmd_arguments.size(); c++) {
          if(ignore_next) {
            ignore_next=false;
            continue;
          }
          else if(cmd_arguments[c]=="-o" || cmd_arguments[c]=="-c") {
            ignore_next=true;
            continue;
          }
          arguments.emplace_back(cmd_arguments[c]);
        }
      }
    }
    else
      arguments.emplace_back(default_std_argument);
  }
  else
    arguments.emplace_back(default_std_argument);
  
  auto clang_version_string=clangmm::to_string(clang_getClangVersion());
  const static std::regex clang_version_regex(R"(^[A-Za-z ]+([0-9.]+).*$)");
  std::smatch sm;
  if(std::regex_match(clang_version_string, sm, clang_version_regex)) {
    auto clang_version=sm[1].str();
    arguments.emplace_back("-I/usr/lib/clang/"+clang_version+"/include");
    arguments.emplace_back("-I/usr/lib64/clang/"+clang_version+"/include"); // For Fedora
#if defined(__APPLE__) && CINDEX_VERSION_MAJOR==0 && CINDEX_VERSION_MINOR<32 // TODO: remove during 2018 if llvm3.7 is no longer in homebrew (CINDEX_VERSION_MINOR=32 equals clang-3.8 I think)
    arguments.emplace_back("-I/usr/local/Cellar/llvm/"+clang_version+"/lib/clang/"+clang_version+"/include");
    arguments.emplace_back("-I/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/../include/c++/v1");
    arguments.emplace_back("-I/Library/Developer/CommandLineTools/usr/bin/../include/c++/v1"); //Added for OS X 10.11
#endif
#ifdef _WIN32
    auto env_msystem_prefix=std::getenv("MSYSTEM_PREFIX");
    if(env_msystem_prefix!=nullptr)
      arguments.emplace_back("-I"+(boost::filesystem::path(env_msystem_prefix)/"lib/clang"/clang_version/"include").string());
#endif
  }
  arguments.emplace_back("-fretain-comments-from-system-headers");
  
  auto extension=file_path.extension().string();
  if(extension==".h" ||  //TODO: temporary fix for .h-files (parse as c++)
     extension!=".c")
    arguments.emplace_back("-xc++");
  
  if(extension.empty() || (1<extension.size() && extension[1]=='h') || extension==".tcc" || extension==".cuh") {
    arguments.emplace_back("-Wno-pragma-once-outside-header");
    arguments.emplace_back("-Wno-pragma-system-header-outside-header");
    arguments.emplace_back("-Wno-include-next-outside-header");
  }
  
  if(extension==".cu" || extension==".cuh") {
    arguments.emplace_back("-include");
    arguments.emplace_back("cuda_runtime.h");
  }
  
  if(extension==".cl") {
    arguments.emplace_back("-xcl");
    arguments.emplace_back("-cl-std=CL2.0");
    arguments.emplace_back("-Xclang");
    arguments.emplace_back("-finclude-default-header");
    arguments.emplace_back("-Wno-gcc-compat");
  }
  
  if(!build_path.empty()) {
    arguments.emplace_back("-working-directory");
    arguments.emplace_back(build_path.string());
  }

  return arguments;
}
