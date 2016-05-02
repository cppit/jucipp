/**
   \mainpage
   juCi++ is a lightweight C++ IDE written in C++
   (<a href="https://github.com/cppit/jucipp">Github page</a>).

  \section sec_overview Overview
  The application entry point is the class Application.

  \section sec_dependencies Dependencies
  juCi++ is based on boost, gtkmm and libclang (among others).
  \startuml
   left to right direction
   component [juCi++] #LightGreen
   component [libclangmm] #Cyan
   component [tiny-process-library] #Cyan
   [juCi++] --> [boost-filesystem] : use 
   [juCi++] --> [boost-regex] : use
   [juCi++] --> [gtkmm-3.0] : use
   [juCi++] --> [gtksourceviewmm-3.0] : use
   [juCi++] --> [aspell] : use
   [juCi++] --> [lbclang] : use
   [juCi++] --> [lbdb] : use
   [juCi++] --> [libclangmm] : use
   [juCi++] --> [tiny-process-library] : use
  \enduml
*/