#include "gtkmm.h"
#include "logging.h"
#include "source.h"

class SelectionDialog : public Gtk::Dialog {
public:
  SelectionDialog(Source::View& view);
  void show(const std::map<std::string, std::string>& rows);
  bool close(GdkEventFocus*);
  
  std::function<void(Gtk::ListViewText& list_view_text)> on_select;
  
private:
  void adjust(int current_x, int current_y);
  
  Source::View& view;
  Gtk::ScrolledWindow scrolled_window;
  Gtk::ListViewText list_view_text;
};