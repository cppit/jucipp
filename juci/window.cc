#include "window.h"

Window::Window() :
  window_box_(Gtk::ORIENTATION_VERTICAL),
  notebook_(keybindings_),
  menu_(keybindings_){
  set_title("juCi++");
  set_default_size(600, 400);
  add(window_box_);
  keybindings_.action_group_menu()->add(Gtk::Action::create("FileQuit",
							    Gtk::Stock::QUIT),
					[this]() {
					  OnWindowHide();
					});
  keybindings_.action_group_menu()->add(Gtk::Action::create("FileOpenFile",
							    Gtk::Stock::OPEN),
					[this]() {
					  OnOpenFile();
					});
  libjuci::ApiServiceProvider::menu_ =
    std::shared_ptr<Menu::Controller>(&menu_);

  libjuci::ApiServiceProvider::notebook_ =
    std::shared_ptr<Notebook::Controller>(&notebook_);

  libjuci::ApiServiceProvider::AddKeybinding();  

  add_accel_group(keybindings_.ui_manager_menu()->get_accel_group());
  add_accel_group(keybindings_.ui_manager_hidden()->get_accel_group());

  //moved here from menu.cc by forgie
  keybindings_.BuildMenu();

  window_box_.pack_start(menu_.view(), Gtk::PACK_SHRINK);
  window_box_.pack_start(notebook_.entry_view(), Gtk::PACK_SHRINK);
  window_box_.pack_start(notebook_.view());
  show_all_children();
  } // Window constructor

void Window::OnWindowHide(){
  //TODO forgie: find out how to 'remove' the pointers
  //TODO forgie: Make shared_ptr
  //libjuci::ApiServiceProvider::notebook_ =
  //   std::shared_ptr<Notebook::Controller>(nullptr);
  // libjuci::ApiServiceProvider::menu_ =
  //  std::shared_ptr<Menu::Controller>(nullptr);
  hide();
}

void Window::OnOpenFile() {
  Gtk::FileChooserDialog dialog("Please choose a file",
            Gtk::FILE_CHOOSER_ACTION_OPEN);
    dialog.set_transient_for(*this);
    dialog.set_position(Gtk::WindowPosition::WIN_POS_CENTER_ALWAYS);

    //Add response buttons the the dialog:
    dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    dialog.add_button("_Open", Gtk::RESPONSE_OK);

    //Add filters, so that only certain file types can be selected:
    Glib::RefPtr<Gtk::FileFilter> filter_text = Gtk::FileFilter::create();
    filter_text->set_name("Text files");
    filter_text->add_mime_type("text/plain");
    dialog.add_filter(filter_text);

    Glib::RefPtr<Gtk::FileFilter> filter_cpp = Gtk::FileFilter::create();
    filter_cpp->set_name("C/C++ files");
    filter_cpp->add_mime_type("text/x-c");
    filter_cpp->add_mime_type("text/x-c++");
    filter_cpp->add_mime_type("text/x-c-header");
    dialog.add_filter(filter_cpp);

    Glib::RefPtr<Gtk::FileFilter> filter_any = Gtk::FileFilter::create();
    filter_any->set_name("Any files");
    filter_any->add_pattern("*");
    dialog.add_filter(filter_any);

    int result = dialog.run();

    switch (result) {
        case(Gtk::RESPONSE_OK): {
            std::cout << "Open clicked." << std::endl;
            std::string path = dialog.get_filename();
            std::cout << "File selected: " << path << std::endl;
	    notebook_.OnOpenFile(path);
            break;
        }
        case(Gtk::RESPONSE_CANCEL): {
            std::cout << "Cancel clicked." << std::endl;
            break;
        }
        default: {
            std::cout << "Unexpected button clicked." << std::endl;
            break;
        }
    }

}
