/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2015-2026, Harbin Institute of Technology,
 *                          Regents of the University of California.
 *
 * This file is part of ndn-tools (Named Data Networking Essential Tools).
 * See AUTHORS.md for complete list of ndn-tools authors and contributors.
 *
 * ndn-tools is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * ndn-tools is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * ndn-tools, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @author Zhongda Xia <xiazhongda@hit.edu.cn>
 */

#include "core/common.hpp"
#include "core/version.hpp"

#include "producer.hpp"

#include <boost/asio/signal_set.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

#include <iostream>

namespace ndn::kite::producer {

namespace po = boost::program_options;

class Runner : noncopyable
{
public:
  explicit
  Runner(const Options& options)
    : m_options(options)
    , m_producer(m_face, m_keyChain, options)
    , m_signalSet(m_face.getIoContext(), SIGINT)
  {
    m_signalSet.async_wait([this] (const auto& ec, auto) {
      if (ec != boost::asio::error::operation_aborted) {
        m_producer.stop();
      }
    });
  }

  int
  run()
  {
    const auto prefix = Name(m_options.rvPrefix).append(m_options.producerSuffix);
    std::cout << "KITE producer announcing " << prefix
              << " (interval=" << m_options.interval.count() << "ms"
              << ", lifetime=" << m_options.lifetime.count() << "ms)" << std::endl;

    try {
      m_producer.start();
      m_face.processEvents();
    }
    catch (const std::exception& e) {
      std::cerr << "ERROR: " << e.what() << std::endl;
      return 1;
    }
    return 0;
  }

private:
  const Options& m_options;
  Face m_face;
  KeyChain m_keyChain;
  Producer m_producer;
  boost::asio::signal_set m_signalSet;
};

static void
usage(std::ostream& os, std::string_view programName, const po::options_description& options)
{
  os << "Usage: " << programName << " [options]\n"
     << "\n"
     << "Starts a KITE mobile producer. The producer sends a KITE Request to the RV\n"
     << "(--rv-prefix) every --interval milliseconds, asking the RV to install a\n"
     << "PrefixAnnouncement for <rv-prefix>/<producer-suffix>. It also serves\n"
     << "consumer Interests under the same name with an echo-back Data signed by\n"
     << "its own identity (also <rv-prefix>/<producer-suffix>).\n"
     << "\n"
     << options;
}

static int
main(int argc, char* argv[])
{
  Options options;
  std::string rvPrefix;
  std::string producerSuffix;
  auto interval = options.interval.count();
  auto lifetime = options.lifetime.count();

  po::options_description visibleDesc("Options");
  visibleDesc.add_options()
    ("help,h",             "print this help message and exit")
    ("rv-prefix,r",        po::value<std::string>(&rvPrefix), "RV prefix (required)")
    ("producer-suffix,p",  po::value<std::string>(&producerSuffix), "producer suffix (required)")
    ("interval,i",         po::value(&interval)->default_value(interval),
                           "Request retransmission interval, in milliseconds")
    ("lifetime,l",         po::value(&lifetime)->default_value(lifetime),
                           "Request expiration period, in milliseconds")
    ("version,V",          "print program version and exit")
    ;

  po::options_description optDesc;
  optDesc.add(visibleDesc);

  po::variables_map vm;
  try {
    po::store(po::command_line_parser(argc, argv).options(optDesc).run(), vm);
    po::notify(vm);
  }
  catch (const po::error& e) {
    std::cerr << "ERROR: " << e.what() << "\n\n";
    usage(std::cerr, argv[0], visibleDesc);
    return 2;
  }
  catch (const boost::bad_any_cast& e) {
    std::cerr << "ERROR: " << e.what() << "\n\n";
    usage(std::cerr, argv[0], visibleDesc);
    return 2;
  }

  if (vm.count("help") > 0) {
    usage(std::cout, argv[0], visibleDesc);
    return 0;
  }

  if (vm.count("version") > 0) {
    std::cout << "kiteproducer " << tools::VERSION << std::endl;
    return 0;
  }

  if (rvPrefix.empty()) {
    std::cerr << "ERROR: --rv-prefix is required\n\n";
    usage(std::cerr, argv[0], visibleDesc);
    return 2;
  }
  if (producerSuffix.empty()) {
    std::cerr << "ERROR: --producer-suffix is required\n\n";
    usage(std::cerr, argv[0], visibleDesc);
    return 2;
  }

  try {
    options.rvPrefix = Name(rvPrefix);
    options.producerSuffix = PartialName(producerSuffix);
  }
  catch (const Name::Error& e) {
    std::cerr << "ERROR: invalid name: " << e.what() << std::endl;
    return 2;
  }

  if (interval <= 0) {
    std::cerr << "ERROR: --interval must be positive" << std::endl;
    return 2;
  }
  options.interval = time::milliseconds(interval);

  if (lifetime <= 0) {
    std::cerr << "ERROR: --lifetime must be positive" << std::endl;
    return 2;
  }
  options.lifetime = time::milliseconds(lifetime);

  return Runner(options).run();
}

} // namespace ndn::kite::producer

int
main(int argc, char* argv[])
{
  return ndn::kite::producer::main(argc, argv);
}
