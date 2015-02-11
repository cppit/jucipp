#include "plugin.h"
#include <boost/python.hpp>
using namespace boost::python;

BOOST_PYTHON_MODULE(plugintest_ext)
{
    def("get_test_value2", &Plugin::get_test_value2);
}
