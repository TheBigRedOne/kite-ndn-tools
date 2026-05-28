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

#ifndef NDN_TOOLS_KITE_RV_HPP
#define NDN_TOOLS_KITE_RV_HPP

#include "core/common.hpp"

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/security/validation-error.hpp>
#include <ndn-cxx/security/validator-config.hpp>
#include <ndn-cxx/util/signal.hpp>

namespace ndn::kite::rv {

/// Options for Rv.
struct Options
{
  std::vector<Name> prefixes;       ///< RV prefixes to register and announce under
  std::string validatorConfigFile;  ///< absolute path to the ValidatorConfig file
};

/**
 * @brief KITE Rendezvous server.
 *
 * Listens on each Name in Options::prefixes and answers KITE Requests with
 * KITE Acks that carry a PrefixAnnouncement of the form `<rvPrefix>/<producerSuffix>`
 * signed by the RV's identity (Options::prefixes[0]).
 *
 * KITE Request signatures are validated through a ValidatorConfig loaded from
 * Options::validatorConfigFile; validation failures are logged and dropped
 * silently.
 */
class Rv : noncopyable
{
public:
  Rv(Face& face, KeyChain& keyChain, const Options& options);

  /// Emitted on every received Interest under one of the registered prefixes,
  /// before signature validation.
  signal::Signal<Rv, Name> afterReceive;

  /**
   * @brief Register the RV prefix and start serving.
   * @note Non-blocking; caller must drive face.processEvents().
   */
  void
  start();

  /// Unregister the RV prefix.
  void
  stop();

private:
  void
  onInterest(const Interest& interest);

  void
  onValidationSuccess(const Interest& interest);

  void
  onValidationFailure(const Interest& interest, const security::ValidationError& error);

private:
  const Options& m_options;
  Face& m_face;
  KeyChain& m_keyChain;
  RegisteredPrefixHandle m_registeredPrefix;
  security::ValidatorConfig m_validator;
};

} // namespace ndn::kite::rv

#endif // NDN_TOOLS_KITE_RV_HPP
