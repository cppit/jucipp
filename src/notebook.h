#ifndef JUCI_NOTEBOOK_H_
#define JUCI_NOTEBOOK_H_

#include <iostream>
#include "gtkmm.h"
#include "source.h"
#include "source_clang.h"
#include <type_traits>
#include <map>
#include <sigc++/sigc++.h>

class Notebook : public Gtk::HPaned {
  class TabLabel : public Gtk::EventBox {
    Gtk::HBox hbox;
    Gtk::Button button;
  public:
    TabLabel(const boost::filesystem::path &path, std::function<void()> on_close);
    Gtk::Label label;
  };
  
private:
  Notebook();
public:
  static Notebook &get() {
    static Notebook singleton;
    return singleton;
  }
  
  //Source::View* get_view(int page);
  size_t size();
  Source::View* get_view(size_t index);
  Source::View* get_current_view();
  std::vector<Source::View*> &get_views();
  
  void open(const boost::filesystem::path &file_path, size_t notebook_index=-1);
  void configure(size_t index);
  bool save(size_t index);
  bool save_current();
  void save_session();
  bool close(size_t index);
  bool close_current();
  void next();
  void previous();
  void toggle_split();
  boost::filesystem::path get_current_folder();

  Gtk::Label info;
  Gtk::Label status;
  
  std::function<void(Source::View*)> on_change_page;
  std::function<void(Source::View*)> on_close_page;
private:
  size_t get_index(Source::View *view);
  Source::View *get_view(size_t notebook_index, int page);
  void focus_view(Source::View *view);
  std::pair<size_t, int> get_notebook_page(size_t index);
  
  std::vector<Gtk::Notebook> notebooks;
  std::vector<Source::View*> source_views; //Is NOT freed in destructor, this is intended for quick program exit.
  std::vector<std::unique_ptr<Gtk::Widget> > source_maps;
  std::vector<std::unique_ptr<Gtk::ScrolledWindow> > scrolled_windows;
  std::vector<std::unique_ptr<Gtk::HBox> > hboxes;
  std::vector<std::unique_ptr<TabLabel> > tab_labels;
  
  bool split=false;
  size_t last_index=-1;
  Source::View* current_view_pre_focused=nullptr;
  Source::View* current_view_focused=nullptr;
  
  bool save_modified_dialog(size_t index);
};
#endif  // JUCI_NOTEBOOK_H_
