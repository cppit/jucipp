#include "selection_dialog.h"

SelectionDialogBase::ListViewText::ListViewText(bool use_markup) {}

SelectionDialogBase::SelectionDialogBase(Gtk::TextView *text_view, const Glib::RefPtr<Gtk::TextBuffer::Mark> &start_mark, bool show_search_entry, bool use_markup):
  text_view(text_view), list_view_text(use_markup) {}

void SelectionDialogBase::show() {}

void SelectionDialogBase::hide() {}

void SelectionDialogBase::add_row(const std::string& row) {}

std::unique_ptr<SelectionDialog> SelectionDialog::instance;

SelectionDialog::SelectionDialog(Gtk::TextView *text_view, const Glib::RefPtr<Gtk::TextBuffer::Mark> &start_mark, bool show_search_entry, bool use_markup) :
  SelectionDialogBase(text_view, start_mark, show_search_entry, use_markup) {}

SelectionDialogBase::~SelectionDialogBase() {}

bool SelectionDialog::on_key_press(GdkEventKey* key) { return true; }

std::unique_ptr<CompletionDialog> CompletionDialog::instance;

CompletionDialog::CompletionDialog(Gtk::TextView *text_view, const Glib::RefPtr<Gtk::TextBuffer::Mark> &start_mark):
  SelectionDialogBase(text_view, start_mark, false, false) {}

bool CompletionDialog::on_key_press(GdkEventKey* key) { return true;}

bool CompletionDialog::on_key_release(GdkEventKey* key) {return true;}
