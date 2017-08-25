#include "dialogs.h"
#include <cmath>

Dialog::Message::Message(const std::string &text): Gtk::Window(Gtk::WindowType::WINDOW_POPUP) {
  auto g_application=g_application_get_default();
  auto gio_application=Glib::wrap(g_application, true);
  auto application=Glib::RefPtr<Gtk::Application>::cast_static(gio_application);
  set_transient_for(*application->get_active_window());
  
  set_position(Gtk::WindowPosition::WIN_POS_CENTER_ON_PARENT);
  set_modal(true);
  set_type_hint(Gdk::WindowTypeHint::WINDOW_TYPE_HINT_NOTIFICATION);
  property_decorated()=false;
  set_skip_taskbar_hint(true);
  
  auto box=Gtk::manage(new Gtk::Box(Gtk::Orientation::ORIENTATION_VERTICAL));
  auto label=Gtk::manage(new Gtk::Label(text));
  label->set_padding(10, 10);
  box->pack_start(*label);
  add(*box);
  
  show_all_children();
  show_now();
  
  while(Gtk::Main::events_pending())
    Gtk::Main::iteration(false);
}

bool Dialog::Message::on_delete_event(GdkEventAny *event) {
  return true;
}

std::string Dialog::gtk_dialog(const boost::filesystem::path &path, const std::string &title,
                        const std::vector<std::pair<std::string, Gtk::ResponseType>> &buttons,
                        Gtk::FileChooserAction action) {
  // Workaround for crash on MacOS when filtering files in file/folder dialogs.
  // See also https://github.com/cppit/jucipp/issues/259.
  // TODO 2018: check if this bug has been fixed
#ifdef __APPLE__
  class FileChooserDialog : public Gtk::FileChooserDialog {
    Gtk::FileChooserAction action;
  public:
    FileChooserDialog(const Glib::ustring& title, Gtk::FileChooserAction action) : Gtk::FileChooserDialog(title, action), action(action) {}
  protected:
    bool on_key_press_event(GdkEventKey *key_event) override {
      if(action==Gtk::FileChooserAction::FILE_CHOOSER_ACTION_OPEN || action==Gtk::FileChooserAction::FILE_CHOOSER_ACTION_SELECT_FOLDER) {
        auto unicode=gdk_keyval_to_unicode(key_event->keyval);
        if(unicode>31 && unicode!=127)
          return true;
      }
      return Gtk::FileChooserDialog::on_key_press_event(key_event);
    }
  };
  FileChooserDialog dialog(title, action);
#else
  Gtk::FileChooserDialog dialog(title, action);
#endif
  
  auto g_application=g_application_get_default();
  auto gio_application=Glib::wrap(g_application, true);
  auto application=Glib::RefPtr<Gtk::Application>::cast_static(gio_application);
  dialog.set_transient_for(*application->get_active_window());
  dialog.set_position(Gtk::WindowPosition::WIN_POS_CENTER_ON_PARENT);
  
  if(title=="Save File As")
    gtk_file_chooser_set_filename(reinterpret_cast<GtkFileChooser*>(dialog.gobj()), path.string().c_str());
  else if(!path.empty())
    gtk_file_chooser_set_current_folder(reinterpret_cast<GtkFileChooser*>(dialog.gobj()), path.string().c_str());
  else {
    boost::system::error_code ec;
    auto current_path=boost::filesystem::current_path(ec);
    if(!ec)
      gtk_file_chooser_set_current_folder(reinterpret_cast<GtkFileChooser*>(dialog.gobj()), current_path.string().c_str());
  }
  
  for (auto &button : buttons) 
    dialog.add_button(button.first, button.second);
  return dialog.run() == Gtk::RESPONSE_OK ? dialog.get_filename() : "";
}
