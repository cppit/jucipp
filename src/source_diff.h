#pragma once
#include "source_base.h"
#include <boost/filesystem.hpp>
#include "dispatcher.h"
#include <set>
#include <map>
#include <thread>
#include <atomic>
#include <mutex>
#include "git.h"

namespace Source {
  class DiffView : virtual public Source::BaseView {
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
    DiffView(const boost::filesystem::path &file_path, const Glib::RefPtr<Gsv::Language> &language);
    ~DiffView() override;
    
    void configure() override;
    
    void rename(const boost::filesystem::path &path) override;
    
    void git_goto_next_diff();
    std::string git_get_diff_details();
    
    /// Use canonical path to follow symbolic links
    boost::filesystem::path canonical_file_path;
  private:
    std::mutex canonical_file_path_mutex;
    
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
