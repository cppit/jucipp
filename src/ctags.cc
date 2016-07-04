#include "ctags.h"
#include "config.h"
#include "terminal.h"
#include "project_build.h"
#include "filesystem.h"
#include "directories.h"
#include <iostream>
#include <vector>
#include <regex>

std::pair<boost::filesystem::path, std::unique_ptr<std::stringstream> > Ctags::get_result(const boost::filesystem::path &path) {
  auto build=Project::Build::create(path);
  auto run_path=build->project_path;
  std::string exclude;
  if(!run_path.empty()) {
    boost::system::error_code ec;
    auto default_path=boost::filesystem::canonical(build->get_default_path(), ec);
    if(!ec) {
      auto path=filesystem::get_relative_path(default_path, build->project_path);
      if(!path.empty())
        exclude+=" --exclude="+path.string();
    }
    auto debug_path=boost::filesystem::canonical(build->get_debug_path(), ec);
    if(!ec) {
      auto path=filesystem::get_relative_path(debug_path, build->project_path);
      if(!path.empty())
        exclude+=" --exclude="+path.string();
    }
  }
  else {
    if(!Directories::get().path.empty())
      run_path=Directories::get().path;
    else
      run_path=path;
  }
  
  std::stringstream stdin_stream;
  //TODO: when debian stable gets newer g++ version that supports move on streams, remove unique_ptr below
  std::unique_ptr<std::stringstream> stdout_stream(new std::stringstream());
  auto command=Config::get().project.ctags_command+exclude+" --fields=n --sort=foldcase -I \"override noexcept\" -f - -R *";
  Terminal::get().process(stdin_stream, *stdout_stream, command, run_path);
  return {run_path, std::move(stdout_stream)};
}

Ctags::Location Ctags::get_location(const std::string &line, bool markup) {
  Location location;

#ifdef _WIN32
  auto line_fixed=line;
  if(!line_fixed.empty() && line_fixed.back()=='\r')
    line_fixed.pop_back();
#else
  auto &line_fixed=line;
#endif

  const static std::regex regex("^([^\t]+)\t([^\t]+)\t(?:/\\^)?([ \t]*)(.+)$");
  std::smatch sm;
  if(std::regex_match(line_fixed, sm, regex)) {
    location.source=sm[4].str();
    size_t pos=location.source.find(";\"\tline:");
    if(pos==std::string::npos)
      return location;
    try {
      location.line=std::stoul(location.source.substr(pos+8))-1;
    }
    catch(const std::exception&) {
      location.line=0;
    }
    location.source.erase(pos);
    if(location.source.size()>1 && location.source[location.source.size()-1]=='/' && location.source[location.source.size()-2]!='\\') {
      location.source.pop_back();
      if(location.source.size()>1 && location.source[location.source.size()-1]=='$' && location.source[location.source.size()-2]!='\\')
        location.source.pop_back();
      auto symbol=sm[1].str();
      location.file_path=sm[2].str();
      location.index=sm[3].str().size();
      
      size_t pos=location.source.find(symbol);
      if(pos!=std::string::npos)
        location.index+=pos;
      
      if(markup) {
        location.source=Glib::Markup::escape_text(location.source);
        pos=-1;
        while((pos=location.source.find(symbol, pos+1))!=std::string::npos) {
          location.source.insert(pos+symbol.size(), "</b>");
          location.source.insert(pos, "<b>");
          pos+=7+symbol.size();
        }
      }
    }
    else {
      location.file_path=sm[2].str();
      location.index=0;
      location.source=sm[1].str();
      if(markup)
        location.source="<b>"+Glib::Markup::escape_text(location.source)+"</b>";
    }
  }
  else
    std::cerr << "Warning (ctags): please report to the juCi++ project that the following line was not parsed:\n" << line << std::endl;
  
  return location;
}

Ctags::Location Ctags::get_location(const boost::filesystem::path &path, const std::string &name, const std::string &type) {
  //insert name into type
  size_t c=0;
  size_t bracket_count=0;
  for(;c<type.size();++c) {
    if(type[c]=='<')
      ++bracket_count;
    else if(type[c]=='>')
      --bracket_count;
    else if(bracket_count==0 && type[c]=='(')
      break;
  }
  auto full_type=type;
  full_type.insert(c, name);
  
  //Split up full_type
  std::vector<std::string> parts;
  size_t text_start=-1;
  for(size_t c=0;c<full_type.size();++c) {
    auto &chr=full_type[c];
    if((chr>='0' && chr<='9') || (chr>='a' && chr<='z') || (chr>='A' && chr<='Z') || chr=='_' || chr=='~') {
      if(text_start==static_cast<size_t>(-1))
        text_start=c;
    }
    else {
      if(text_start!=static_cast<size_t>(-1))
        parts.emplace_back(full_type.substr(text_start, c-text_start));
      text_start=-1;
      if(chr=='*' || chr=='&')
        parts.emplace_back(std::string()+chr);
    }
  }
  
  auto result=get_result(path);
  result.second->seekg(0, std::ios::end);
  if(result.second->tellg()==0)
    return Location();
  result.second->seekg(0, std::ios::beg);
  
  std::string line;
  size_t best_score=0;
  Location best_location;
  while(std::getline(*result.second, line)) {
    if(line.find(name)==std::string::npos)
      continue;
    auto location=Ctags::get_location(line, false);
    location.file_path=result.first/location.file_path;
    
    //Find match score
    size_t score=0;
    size_t pos=0;
    for(auto &part: parts) {
      auto find_pos=location.source.find(part, pos);
      if(find_pos!=std::string::npos) {
        pos=find_pos+1;
        ++score;
      }
    }
    if(score>best_score) {
      best_score=score;
      best_location=location;
    }
  }
  
  return best_location;
}
