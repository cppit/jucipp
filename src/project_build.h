#ifndef JUCI_PROJECT_BUILD_H_
#define JUCI_PROJECT_BUILD_H_

#include <boost/filesystem.hpp>
#include "cmake.h"

namespace Project {
  class Build {
  public:
    Build() {}
    virtual ~Build() {}
    
    boost::filesystem::path project_path;
    
    boost::filesystem::path get_default_build_path();
    virtual bool update_default_build(bool force=false) {return false;}
    boost::filesystem::path get_debug_build_path();
    virtual bool update_debug_build(bool force=false) {return false;}
    
    virtual boost::filesystem::path get_executable(const boost::filesystem::path &path) {return boost::filesystem::path();}
  };
  
  class CMake : public Build {
    ::CMake cmake;
  public:
    CMake(const boost::filesystem::path &path);
    
    bool update_default_build(bool force=false) override;
    bool update_debug_build(bool force=false) override;
    
    boost::filesystem::path get_executable(const boost::filesystem::path &path) override;
  };
  
  std::unique_ptr<Build> get_build(const boost::filesystem::path &path);
}

#endif // JUCI_PROJECT_BUILD_H_
