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

#define TRACE_VAR(x) BOOST_LOG_TRIVIAL(trace)      << "** Trace: "   << __FILE__ << "@" << __func__ << "(" << __LINE__ << "): " << #x << "=<" << x << ">";
#define DEBUG_VAR(x) BOOST_LOG_TRIVIAL(debug)      << "** Debug: "   << __FILE__ << "@" << __func__ << "(" << __LINE__ << "): " << #x << "=<" << x << ">";
#define INFO_VAR(x) BOOST_LOG_TRIVIAL(info)        << "** Info: "    << __FILE__ << "@" << __func__ << "(" << __LINE__ << "): " << #x << "=<" << x << ">";
#define WARNING_VAR(x) BOOST_LOG_TRIVIAL(warning)  << "** Warning: " << __FILE__ << "@" << __func__ << "(" << __LINE__ << "): " << #x << "=<" << x << ">";
#define FATAL_VAR(x) BOOST_LOG_TRIVIAL(fatal)      << "** Fatal: "   << __FILE__ << "@" << __func__ << "(" << __LINE__ << "): " << #x << "=<" << x << ">";
#define JERROR_VAR(x) BOOST_LOG_TRIVIAL(error)      << "** Error: "   << __FILE__ << "@" << __func__ << "(" << __LINE__ << "): " << #x << "=<" << x << ">";
#define JTRACE(x) BOOST_LOG_TRIVIAL(trace)          << "** Trace: "   << __FILE__ << "@" << __func__ << "(" << __LINE__ << "): \"" << x << "\"";
#define JDEBUG(x) BOOST_LOG_TRIVIAL(debug)          << "** Debug: "   << __FILE__ << "@" << __func__ << "(" << __LINE__ << "): \"" << x << "\"";
#define JINFO(x) BOOST_LOG_TRIVIAL(info)            << "** Info: "    << __FILE__ << "@" << __func__ << "(" << __LINE__ << "): \"" << x << "\"";
#define JWARNING(x) BOOST_LOG_TRIVIAL(warning)      << "** Warning: " << __FILE__ << "@" << __func__ << "(" << __LINE__ << "): \"" << x << "\"";
#define JFATAL(x) BOOST_LOG_TRIVIAL(fatal)          << "** Fatal: "   << __FILE__ << "@" << __func__ << "(" << __LINE__ << "): \"" << x << "\"";
#define JERROR(x) BOOST_LOG_TRIVIAL(error)          << "** Error: "   << __FILE__ << "@" << __func__ << "(" << __LINE__ << "): \"" << x << "\"";

#endif  // JUCI_LOGGING_H_
