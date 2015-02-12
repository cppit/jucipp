#include "iostream"
#include "gtkmm.h"

namespace Keybindings {

    class Model {
    public:
      Model();
      virtual ~Model();
      std::string menu_ui_string(){return menu_ui_string_;}
      std::string hidden_ui_string(){return hidden_ui_string_;}
    private:
      std::string menu_ui_string_;
      std::string hidden_ui_string_;
    };

    class Controller {
    public:
      Controller();

      virtual ~Controller();

      Glib::RefPtr<Gtk::ActionGroup> action_group_menu() {
        return action_group_menu_;
      };

      Glib::RefPtr<Gtk::UIManager> ui_manager_menu() {
        return ui_manager_menu_;
      };

      Glib::RefPtr<Gtk::ActionGroup> action_group_hidden() {
        return action_group_hidden_;
      };

      Glib::RefPtr<Gtk::UIManager> ui_manager_hidden() {
        return ui_manager_hidden_;
      };

      void BuildMenu();
      void BuildHiddenMenu();


    protected:
      Glib::RefPtr<Gtk::UIManager> ui_manager_menu_;
      Glib::RefPtr<Gtk::ActionGroup> action_group_menu_;
      Glib::RefPtr<Gtk::UIManager> ui_manager_hidden_;
      Glib::RefPtr<Gtk::ActionGroup> action_group_hidden_;
    private:
      Keybindings::Model model_;
    };

}