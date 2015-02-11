#include "api.h"

BOOST_PYTHON_MODULE(jucy_to_python_api) {
      using namespace boost::python;
      // text editing
      def("replaceLine", &juci_api::cpp::ReplaceLine);
      def("replaceWord", &juci_api::cpp::ReplaceWord);
    }// module::juci
