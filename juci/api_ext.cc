#include "api.h"

BOOST_PYTHON_MODULE(juci_to_python_api) {
      using namespace boost::python;
      // text editing
      def("replaceLine", &libjuci::ReplaceLine);
      def("replaceWord", &libjuci::ReplaceWord);
      def("getWord", &libjuci::GetWord);
      //something more
    }// module::juci_to_python_api
