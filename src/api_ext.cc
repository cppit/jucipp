#include "api.h"

BOOST_PYTHON_MODULE(juci_to_python_api) {
      using namespace boost::python;
      // plugin inclusion
      def("addMenuElement", &libjuci::AddMenuElement);
      def("addSubMenuElement", &libjuci::AddSubMenuElement);
      def("loadPlugin", &libjuci::LoadPlugin);
      def("initPlugin", &libjuci::InitPlugin);

      // text editing
      def("replaceLine", &libjuci::ReplaceLine);
      def("replaceWord", &libjuci::ReplaceWord);
      def("getWord", &libjuci::GetWord);
    }  // module::juci_to_python_api
