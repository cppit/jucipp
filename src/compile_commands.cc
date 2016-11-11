#include "compile_commands.h"
#include <boost/property_tree/json_parser.hpp>

std::vector<std::string> CompileCommands::Command::paramter_values(const std::string &paramter_name) const {
  std::vector<std::string> parameter_values;
  
  bool found_argument=false;
  for(auto &parameter: parameters) {
    if(found_argument) {
      parameter_values.emplace_back(parameter);
      found_argument=false;
    }
    else if(parameter==paramter_name)
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
      size_t parameter_start_pos=-1;
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
          if(parameter_start_pos!=static_cast<size_t>(-1)) {
            add_parameter();
            parameter_start_pos=-1;
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
        
        if(parameter_start_pos==static_cast<size_t>(-1))
          parameter_start_pos=c;
        ++parameter_size;
      }
      if(parameter_start_pos!=static_cast<size_t>(-1))
        add_parameter();
      
      commands.emplace_back(Command{directory, parameters, boost::filesystem::absolute(file, build_path)});
    }
  }
  catch(...) {}
}
