#include "terminal.h"

Terminal::View::View(){
  textview_.add(buffer_);
  view_.add(textview_);
}


 
