#ifndef JUCI_SOURCE_H_
#define JUCI_SOURCE_H_

#include <unordered_map>
#include <vector>
#include <string>
#include "gtkmm.h"

using std::string;

namespace Source {
  class Theme {
  public:
    const std::unordered_map<string, string>& tagtable() const;
    void SetTagTable(const std::unordered_map<string, string> &tagtable);
  private:
    std::unordered_map<string, string> tagtable_;
    string background_;
  };  // class Theme

  class View : public Gtk::TextView {
  public:
    View();
    string UpdateLine();
    void ApplyTheme(const Theme &theme);
  private:
    string GetLine(const Gtk::TextIter &begin);
  };  // class View

  class Model{
  public:
    Model();
    Theme theme();
    const string filepath();
  private:
    Theme theme_;
    string filepath_;
  };

  class Controller {
  public:
    Controller();
    View& view();
    Model& model();

  private:
    void OnLineEdit();
    void OnOpenFile();
    void OnSaveFile();
    
  protected:
    View view_;
    Model model_;
  };  // class Controller
}  // namespace Source
#endif  // JUCI_SOURCE_H_
