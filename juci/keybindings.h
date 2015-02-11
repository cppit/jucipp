#include "iostream"
#include "gtkmm.h"

namespace Keybindings {
    class Controller {
    public:
      Controller();

      virtual ~Controller();

      Glib::RefPtr<Gtk::ActionGroup> action_group() {
        return action_group_;
      };

      Glib::RefPtr<Gtk::UIManager> ui_manager() {
        return ui_manager_;
      };

      void set_ui_manger_string(std::string ui_string);

      void set_ui_manager_action_group(Glib::RefPtr<Gtk::ActionGroup> action_group);

    protected:
      Glib::RefPtr<Gtk::UIManager> ui_manager_;
      Glib::RefPtr<Gtk::ActionGroup> action_group_;
    };

}