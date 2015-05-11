#ifndef JUCI_LOGGING_H_
#define JUCI_LOGGING_H_

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>


using namespace boost::log;

#define TRACE(x) BOOST_LOG_TRIVIAL(trace)     << __func__ << "(" << __LINE__ << "): " << #x << "=<" << x << ">\n";
#define DEBUG(x) BOOST_LOG_TRIVIAL(debug)     << __func__ << "(" << __LINE__ << "): " << #x << "=<" << x << ">\n";
#define INFO(x) BOOST_LOG_TRIVIAL(info)       << __func__ << "(" << __LINE__ << "): " << #x << "=<" << x << ">\n";
#define WARNING(x) BOOST_LOG_TRIVIAL(warning) << __func__ << "(" << __LINE__ << "): " << #x << "=<" << x << ">\n";
#define ERROR(x) BOOST_LOG_TRIVIAL(error)     << __func__ << "(" << __LINE__ << "): " << #x << "=<" << x << ">\n";
#define FATAL(x) BOOST_LOG_TRIVIAL(fatal)     << __func__ << "(" << __LINE__ << "): " << #x << "=<" << x << ">\n";

void init() {
  add_file_log("juci.log");
}


#endif  // JUCI_LOGGING_H_
