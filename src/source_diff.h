#ifndef JUCI_SOURCE_DIFF_H_
#define JUCI_SOURCE_DIFF_H_
#include <gtksourceviewmm.h>
#include <boost/filesystem.hpp>
#include "dispatcher.h"
#include <set>
#include <map>
#include <thread>
#include <atomic>
#include <mutex>
#include "git.h"

namespace Source {
  class DiffView : virtual public Gsv::View {
    enum class ParseState {IDLE, STARTING, PREPROCESSING, PROCESSING, POSTPROCESSING};
    
    class Renderer : public Gsv::GutterRenderer {
    public:
      Renderer();
      
      Glib::RefPtr<Gtk::TextTag> tag_added;
      Glib::RefPtr<Gtk::TextTag> tag_modified;
      Glib::RefPtr<Gtk::TextTag> tag_removed;
      Glib::RefPtr<Gtk::TextTag> tag_removed_below;
      Glib::RefPtr<Gtk::TextTag> tag_removed_above;
      
    protected:
      void draw_vfunc(const Cairo::RefPtr<Cairo::Context> &cr, const Gdk::Rectangle &background_area,
                      const Gdk::Rectangle &cell_area, Gtk::TextIter &start, Gtk::TextIter &end,
                      Gsv::GutterRendererState p6) override;
    };
  public:
    DiffView(const boost::filesystem::path &file_path);
    ~DiffView();
    
    boost::filesystem::path file_path;
  protected:
    std::mutex file_path_mutex;
    std::time_t last_write_time;
    void check_last_write_time();
  public:
    virtual void configure();
    
    std::function<void(DiffView *view)> update_status_branch;
    std::string status_branch;
    
    Gtk::TextIter get_iter_at_line_end(int line_nr);
    
    void git_goto_next_diff();
    std::string git_get_diff_details();
    
  private:
    std::unique_ptr<Renderer> renderer;
    Dispatcher dispatcher;
    
    std::shared_ptr<Git::Repository> repository;
    std::unique_ptr<Git::Repository::Diff> diff;
    std::unique_ptr<Git::Repository::Diff> get_diff();
    
    std::thread parse_thread;
    std::atomic<ParseState> parse_state;
    std::mutex parse_mutex;
    std::atomic<bool> parse_stop;
    Glib::ustring parse_buffer;
    sigc::connection buffer_insert_connection;
    sigc::connection buffer_erase_connection;
    sigc::connection monitor_changed_connection;
    sigc::connection delayed_buffer_changed_connection;
    sigc::connection delayed_monitor_changed_connection;
    std::atomic<bool> monitor_changed;
    
    Git::Repository::Diff::Lines lines;
    void update_lines();
  };
}

#endif //JUCI_SOURCE_DIFF_H_
