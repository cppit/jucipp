#ifndef JUCI_DIRECTORIES_H_
#define JUCI_DIRECTORIES_H_

#include <gtkmm.h>
#include <glib.h>
#include "boost/filesystem.hpp"
#include <utility>
#include <algorithm>
#include <iostream>
#include <fstream>


namespace Directories {
  
  class Config {
  public:
    Config(Config &original);
    Config();
    std::vector<std::string> ignore_list() { return ignore_list_; }
    std::vector<std::string> exception_list() { return exception_list_; }
    void AddIgnore(std::string filter);
    void AddException(std::string filter);
    bool IsException(std::string path);
    bool IsIgnored(std::string path);
  private:
    std::vector<std::string> ignore_list_;
    std::vector<std::string> exception_list_;
  };
  class View : public Gtk::TreeModel::ColumnRecord {
  public:
    View() {
      add(m_col_id);
      add(m_col_name);
      add(m_col_path);
    }
    Gtk::TreeModelColumn<Glib::ustring> m_col_id;
    Gtk::TreeModelColumn<Glib::ustring> m_col_name;
    Gtk::TreeModelColumn<Glib::ustring> m_col_path;
  };

  class Model {
  };
  
  class Controller {
  public:
    Controller();
    Controller(Directories::Config& cfg);
    View& view() { return view_;}
    Model& model() { return model_;}
    Directories::Config& config() { return config_;}
    Gtk::ScrolledWindow& widget() {return m_ScrolledWindow;}
    bool open_folder (const boost::filesystem::path& dir_path);
    void list_dirs (const boost::filesystem::path& dir_path,
                    Gtk::TreeModel::Row &row, unsigned depth);
    std::string get_project_name(const boost::filesystem::path& dir_path);
    int count (const std::string path);

    //Child widgets:
    Gtk::Box m_VBox;
    Gtk::ScrolledWindow m_ScrolledWindow;
    Gtk::TreeView m_TreeView;
    Glib::RefPtr<Gtk::TreeStore> m_refTreeModel;
    bool IsIgnored(std::string path);
  private:
    View view_;
    Model model_;
    Directories::Config config_;
  protected:
    void on_treeview_row_activated(const Gtk::TreeModel::Path& path,
                                   Gtk::TreeViewColumn* column);
  };
}  // namespace Directories

#endif  // JUCI_DIRECTORIES_H_
