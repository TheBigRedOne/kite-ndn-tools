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

#include "rv.hpp"

#include <boost/asio/signal_set.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/positional_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include <iostream>

namespace ndn::kite::rv {

namespace po = boost::program_options;

static const std::string DEFAULT_VALIDATOR_CONFIG = "/usr/local/etc/ndn/rv.conf";

class Runner : noncopyable
{
public:
  explicit
  Runner(const Options& options)
    : m_options(options)
    , m_rv(m_face, m_keyChain, options)
    , m_signalSet(m_face.getIoContext(), SIGINT)
  {
    m_signalSet.async_wait([this] (const auto& ec, auto) {
      if (ec != boost::asio::error::operation_aborted) {
        m_rv.stop();
      }
    });
  }

  int
  run()
  {
    std::cout << "KITE RV serving:";
    for (const auto& prefix : m_options.prefixes) {
      std::cout << ' ' << prefix;
    }
    std::cout << std::endl;

    try {
      m_rv.start();
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
  Rv m_rv;
  boost::asio::signal_set m_signalSet;
};

static void
usage(std::ostream& os, std::string_view programName, const po::options_description& options)
{
  os << "Usage: " << programName << " [options] <prefix>\n"
     << "\n"
     << "Starts a KITE Rendezvous server at <prefix>. Incoming KITE Requests are\n"
     << "verified against the trust schema loaded from --validator-config and\n"
     << "answered with KITE Acks signed by the RV's identity at <prefix>.\n"
     << "\n"
     << options;
}

static int
main(int argc, char* argv[])
{
  Options options;
  options.validatorConfigFile = DEFAULT_VALIDATOR_CONFIG;
  std::string prefix;

  po::options_description visibleDesc("Options");
  visibleDesc.add_options()
    ("help,h",             "print this help message and exit")
    ("validator-config,c", po::value<std::string>(&options.validatorConfigFile)
                              ->default_value(options.validatorConfigFile),
                           "path to ValidatorConfig file")
    ("version,V",          "print program version and exit")
    ;

  po::options_description hiddenDesc;
  hiddenDesc.add_options()
    ("prefix", po::value<std::string>(&prefix));

  po::options_description optDesc;
  optDesc.add(visibleDesc).add(hiddenDesc);

  po::positional_options_description posDesc;
  posDesc.add("prefix", -1);

  po::variables_map vm;
  try {
    po::store(po::command_line_parser(argc, argv).options(optDesc).positional(posDesc).run(), vm);
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
    std::cout << "kiterv " << tools::VERSION << std::endl;
    return 0;
  }

  if (prefix.empty()) {
    std::cerr << "ERROR: no RV prefix specified\n\n";
    usage(std::cerr, argv[0], visibleDesc);
    return 2;
  }

  try {
    options.prefixes.emplace_back(prefix);
  }
  catch (const Name::Error& e) {
    std::cerr << "ERROR: invalid prefix '" << prefix << "': " << e.what() << std::endl;
    return 2;
  }

  return Runner(options).run();
}

} // namespace ndn::kite::rv

int
main(int argc, char* argv[])
{
  return ndn::kite::rv::main(argc, argv);
}
