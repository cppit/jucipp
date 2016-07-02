#include "ctags.h"
#include "config.h"
#include "terminal.h"

//Temporary fix for current Arch Linux boost linking problem
#ifdef __GNUC_PREREQ
#if __GNUC_PREREQ(5,1)
#include <regex>
#define REGEX_NS std
#endif
#endif
#ifndef REGEX_NS
#include <boost/regex.hpp>
#define REGEX_NS boost
#endif

std::unique_ptr<std::stringstream> Ctags::get_result(const boost::filesystem::path &path, std::vector<boost::filesystem::path> exclude_paths) {
  std::string exclude;
  for(auto &path: exclude_paths) {
    if(!path.empty())
      exclude+=" --exclude="+path.string();
  }
  
  std::stringstream stdin_stream;
  //TODO: when debian stable gets newer g++ version that supports move on streams, remove unique_ptr below
  std::unique_ptr<std::stringstream> stdout_stream(new std::stringstream());
  auto command=Config::get().project.ctags_command+exclude+" --fields=n --sort=foldcase -f- -R *";
  Terminal::get().process(stdin_stream, *stdout_stream, command, path);
  return stdout_stream;
}

Ctags::Data Ctags::parse_line(const std::string &line) {
  Data data;
  
  const static REGEX_NS::regex regex("^([^\t]+)\t([^\t]+)\t(?:/\\^)?([ \t]*)(.+);\"\tline:([0-9]+)$");
  REGEX_NS::smatch sm;
  if(REGEX_NS::regex_match(line, sm, regex)) {
    data.source=sm[4].str();
    if(data.source.size()>1 && data.source[data.source.size()-1]=='/' && data.source[data.source.size()-2]!='\\') {
      data.source.pop_back();
      if(data.source.size()>1 && data.source[data.source.size()-1]=='$' && data.source[data.source.size()-2]!='\\')
        data.source.pop_back();
      auto symbol=sm[1].str();
      data.path=sm[2].str();
      data.index=sm[3].str().size();
      try {
        data.line=std::stoul(sm[5].str())-1;
      }
      catch(const std::exception&) {
        data.line=0;
      }
      
      size_t pos=data.source.find(symbol);
      if(pos!=std::string::npos)
        data.index+=pos;
      
      data.source=Glib::Markup::escape_text(data.source);
      pos=-1;
      while((pos=data.source.find(symbol, pos+1))!=std::string::npos) {
        data.source.insert(pos+symbol.size(), "</b>");
        data.source.insert(pos, "<b>");
        pos+=7+symbol.size();
      }
    }
    else {
      data.path=sm[2].str();
      data.index=0;
      data.source=sm[1].str();
      try {
        data.line=std::stoul(sm[3].str())-1;
      }
      catch(const std::exception&) {
        data.line=0;
      }
      data.source="<b>"+Glib::Markup::escape_text(data.source)+"</b>";
    }
  }
  else {
    Terminal::get().print("Warning (ctags): please report to the juCi++ project that the following line was not parsed:\n"+
                          line+'\n', true);
  }
  
  return data;
}
