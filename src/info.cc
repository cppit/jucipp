#include "info.h"

namespace sigc {
#ifndef SIGC_FUNCTORS_DEDUCE_RESULT_TYPE_WITH_DECLTYPE
  template <typename Functor>
  struct functor_trait<Functor, false> {
    typedef decltype (::sigc::mem_fun(std::declval<Functor&>(),
                                      &Functor::operator())) _intermediate;
    typedef typename _intermediate::result_type result_type;
    typedef Functor functor_type;
  };
#else
  SIGC_FUNCTORS_DEDUCE_RESULT_TYPE_WITH_DECLTYPE
#endif
}

Info::Info() {
  set_hexpand(false);
  set_halign(Gtk::Align::ALIGN_END);
  
  auto content_area=dynamic_cast<Gtk::Container*>(get_content_area());
  label.set_max_width_chars(40);
  label.set_line_wrap(true);
  content_area->add(label);
    
  auto provider = Gtk::CssProvider::create();
  provider->load_from_data("* {border-radius: 5px;}");
  get_style_context()->add_provider(provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  
  //Workaround from https://bugzilla.gnome.org/show_bug.cgi?id=710888
  //Issue described at the same issue report
  //TODO: remove later
  auto revealer = gtk_widget_get_template_child (GTK_WIDGET (gobj()), GTK_TYPE_INFO_BAR, "revealer");
  if (revealer) {
    gtk_revealer_set_transition_type (GTK_REVEALER (revealer), GTK_REVEALER_TRANSITION_TYPE_NONE);
    gtk_revealer_set_transition_duration (GTK_REVEALER (revealer), 0);
  }
}

void Info::print(const std::string &text) {
  if(!enabled)
    return;
  
  timeout_connection.disconnect();
  timeout_connection=Glib::signal_timeout().connect([this]() {
    hide();
    return false;
  }, 3000);
  
  label.set_text(text);
  show();
}
