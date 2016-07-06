#ifndef JUCI_GIT_H_
#define JUCI_GIT_H_
#include <git2.h>
#include <mutex>
#include <memory>
#include <iostream>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <giomm.h>
#include <boost/filesystem.hpp>

class Git {
  friend class Repository;
public:
  class Error {
    friend class Git;
    std::string message() noexcept;
  public:
    int code=0;
    Error() {}
    operator bool() noexcept {return code<0;}
  };
  
  class Repository {
  public:
    class Diff {
    public:
      class Lines {
      public:
        std::vector<std::pair<int, int> > added;
        std::vector<std::pair<int, int> > modified;
        std::vector<int> removed;
      };
    private:
      friend class Repository;
      Diff(const boost::filesystem::path &path, git_repository *repository);
      git_repository *repository;
      std::shared_ptr<git_blob> blob;
      git_diff_options options;
      static int hunk_cb(const git_diff_delta *delta, const git_diff_hunk *hunk, void *payload) noexcept;
      static int line_cb(const git_diff_delta *delta, const git_diff_hunk *hunk, const git_diff_line *line, void *payload) noexcept;
    public:
      Diff() : repository(nullptr), blob(nullptr) {}
      Lines get_lines(const std::string &buffer);
      std::string get_details(const std::string &buffer, int line_nr);
    };
    
    enum class STATUS {CURRENT, NEW, MODIFIED, DELETED, RENAMED, TYPECHANGE, UNREADABLE, IGNORED, CONFLICTED};
    class Status {
    public:
      std::unordered_set<std::string> added;
      std::unordered_set<std::string> modified;
    };
  private:
    friend class Git;
    Repository(const boost::filesystem::path &path);
    
    static int status_callback(const char *path, unsigned int status_flags, void *data) noexcept;
    
    std::unique_ptr<git_repository, std::function<void(git_repository *)> > repository;
    
    boost::filesystem::path work_path;
    sigc::connection monitor_changed_connection;
    Status saved_status;
    bool has_saved_status=false;
    std::mutex saved_status_mutex;
  public:
    ~Repository();
    
    static std::string status_string(STATUS status) noexcept;
    
    Status get_status();
    void clear_saved_status();
    
    boost::filesystem::path get_work_path() noexcept;
    boost::filesystem::path get_path() noexcept;
    static boost::filesystem::path get_root_path(const boost::filesystem::path &path);
    
    Diff get_diff(const boost::filesystem::path &path);
    
    Glib::RefPtr<Gio::FileMonitor> monitor;
  };
  
private:
  static bool initialized;
  
  ///Mutex for thread safe operations
  static std::mutex mutex;
  
  static std::unordered_map<std::string, std::pair<std::unique_ptr<Git::Repository>, size_t> > repositories;
  static std::mutex repositories_mutex;
  
  ///Call initialize in public static methods
  static void initialize() noexcept;
  
  static boost::filesystem::path path(const char *cpath, size_t cpath_length=static_cast<size_t>(-1)) noexcept;
public:
  static std::shared_ptr<Repository> get_repository(const boost::filesystem::path &path);
};
#endif //JUCI_GIT_H_
