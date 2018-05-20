#include "ctags.h"
#include "config.h"
#include "terminal.h"
#include "project_build.h"
#include "filesystem.h"
#include <iostream>
#include <vector>
#include <regex>
#include <climits>

std::pair<boost::filesystem::path, std::unique_ptr<std::stringstream> > Ctags::get_result(const boost::filesystem::path &path) {
  auto build=Project::Build::create(path);
  auto run_path=build->project_path;
  std::string exclude;
  if(!run_path.empty()) {
    auto relative_default_path=filesystem::get_relative_path(build->get_default_path(), run_path);
    if(!relative_default_path.empty())
      exclude+=" --exclude="+relative_default_path.string();
    
    auto relative_debug_path=filesystem::get_relative_path(build->get_debug_path(), run_path);
    if(!relative_debug_path.empty())
      exclude+=" --exclude="+relative_debug_path.string();
  }
  else {
    boost::system::error_code ec;
    if(boost::filesystem::is_directory(path, ec) || ec)
      run_path=path;
    else
      run_path=path.parent_path();
  }
  
  std::stringstream stdin_stream;
  //TODO: when debian stable gets newer g++ version that supports move on streams, remove unique_ptr below
  auto stdout_stream=std::make_unique<std::stringstream>();
  auto command=Config::get().project.ctags_command+exclude+" --fields=ns --sort=foldcase -I \"override noexcept\" -f - -R *";
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

  const static std::regex regex(R"(^([^\t]+)\t([^\t]+)\t(?:/\^)?([ \t]*)(.+?)(\$/)?;"\tline:([0-9]+)\t?[a-zA-Z]*:?(.*)$)");
  std::smatch sm;
  if(std::regex_match(line_fixed, sm, regex)) {
    location.symbol=sm[1].str();
    //fix location.symbol for operators
    if(9<location.symbol.size() && location.symbol[8]==' ' && location.symbol.compare(0, 8, "operator")==0) {
      auto &chr=location.symbol[9];
      if(!((chr>='a' && chr<='z') || (chr>='A' && chr<='Z') || (chr>='0' && chr<='9') || chr=='_'))
        location.symbol.erase(8, 1);
    }
    
    location.file_path=sm[2].str();
    location.source=sm[4].str();
    try {
      location.line=std::stoul(sm[6])-1;
    }
    catch(const std::exception&) {
      location.line=0;
    }
    location.scope=sm[7].str();
    if(!sm[5].str().empty()) {
      location.index=sm[3].str().size();
      
      size_t pos=location.source.find(location.symbol);
      if(pos!=std::string::npos)
        location.index+=pos;
      
      if(markup) {
        location.source=Glib::Markup::escape_text(location.source);
        auto symbol=Glib::Markup::escape_text(location.symbol);
        pos=-1;
        while((pos=location.source.find(symbol, pos+1))!=std::string::npos) {
          location.source.insert(pos+symbol.size(), "</b>");
          location.source.insert(pos, "<b>");
          pos+=7+symbol.size();
        }
      }
    }
    else {
      location.index=0;
      location.source=location.symbol;
      if(markup)
        location.source="<b>"+Glib::Markup::escape_text(location.source)+"</b>";
    }
  }
  else
    std::cerr << "Warning (ctags): please report to the juCi++ project that the following line was not parsed:\n" << line << std::endl;
  
  return location;
}

///Split up a type into its various significant parts
std::vector<std::string> Ctags::get_type_parts(const std::string &type) {
  std::vector<std::string> parts;
  size_t text_start=-1;
  for(size_t c=0;c<type.size();++c) {
    auto &chr=type[c];
    if((chr>='0' && chr<='9') || (chr>='a' && chr<='z') || (chr>='A' && chr<='Z') || chr=='_' || chr=='~') {
      if(text_start==static_cast<size_t>(-1))
        text_start=c;
    }
    else {
      if(text_start!=static_cast<size_t>(-1)) {
        parts.emplace_back(type.substr(text_start, c-text_start));
        text_start=-1;
      }
      if(chr=='*' || chr=='&')
        parts.emplace_back(std::string()+chr);
    }
  }
  return parts;
}

std::vector<Ctags::Location> Ctags::get_locations(const boost::filesystem::path &path, const std::string &name, const std::string &type) {
  auto result=get_result(path);
  result.second->seekg(0, std::ios::end);
  if(result.second->tellg()==0)
    return std::vector<Location>();
  result.second->seekg(0, std::ios::beg);
  
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
  
  auto parts=get_type_parts(full_type);
  
  std::string line;
  long best_score=LONG_MIN;
  std::vector<Location> best_locations;
  while(std::getline(*result.second, line)) {
    if(line.size()>2048)
      continue;
    auto location=Ctags::get_location(line, false);
    if(!location.scope.empty()) {
      if(location.scope+"::"+location.symbol!=name)
        continue;
    }
    else if(location.symbol!=name)
      continue;
    
    location.file_path=result.first/location.file_path;
    
    auto source_parts=get_type_parts(location.source);
    
    //Find match score
    long score=0;
    size_t source_index=0;
    for(auto &part: parts) {
      bool found=false;
      for(auto c=source_index;c<source_parts.size();++c) {
        if(part==source_parts[c]) {
          source_index=c+1;
          ++score;
          found=true;
          break;
        }
      }
      if(!found)
        --score;
    }
    size_t index=0;
    for(auto &source_part: source_parts) {
      bool found=false;
      for(auto c=index;c<parts.size();++c) {
        if(source_part==parts[c]) {
          index=c+1;
          ++score;
          found=true;
          break;
        }
      }
      if(!found)
        --score;
    }
    
    if(score>best_score) {
      best_score=score;
      best_locations.clear();
      best_locations.emplace_back(location);
    }
    else if(score==best_score)
      best_locations.emplace_back(location);
  }
  
  return best_locations;
}
