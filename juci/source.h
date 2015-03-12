#ifndef JUCI_SOURCE_H_
#define JUCI_SOURCE_H_

#include <unordered_map>
#include <vector>
#include <string>
#include "gtkmm.h"
#include <TranslationUnit.h>

using std::string;

namespace Source {
  class Theme {
  public:
    const std::unordered_map<string, string>& tagtable() const;
    const std::unordered_map<string, string>& typetable() const;
    void SetTagTable(const std::unordered_map<string, string> &tagtable);
    void InsertTag(const string &key, const string &value);
    void SetTypeTable(const std::unordered_map<string, string> &tagtable);
    void InsertType(const string &key, const string &value);

  private:
    std::unordered_map<string, string> tagtable_;
    std::unordered_map<string, string> typetable_;
    string background_;
  };  // class Theme

  class View : public Gtk::TextView {
  public:
    View();
    string UpdateLine();
    void ApplyTheme(const Theme &theme);
    void OnOpenFile(std::vector<Clang::SourceLocation> &locations,
                    const Theme &theme);
  private:
    string GetLine(const Gtk::TextIter &begin);
  };  // class View

  class Model{
  public:
    Model();
    Theme& theme();
    const string filepath();
    void SetFilePath(const string &filepath);
    void SetSourceLocations(
                     const std::vector<Clang::SourceLocation> &locations);
    std::vector<Clang::SourceLocation>& getSourceLocations() {
      return locations_;
    }

  private:
    Theme theme_;
    string filepath_;
    std::vector<Clang::SourceLocation> locations_;
  };

  class Controller {
  public:
    Controller();
    View& view();
    Model& model();
    void OnNewEmptyFile();
    void OnOpenFile(const string &filename);
    Glib::RefPtr<Gtk::TextBuffer> buffer();

  private:
    void OnLineEdit();
    void OnSaveFile();

  protected:
    View view_;
    Model model_;
  };  // class Controller
}  // namespace Source
#endif  // JUCI_SOURCE_H_
