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

#include "rv.hpp"

#include <ndn-cxx/kite/ack.hpp>
#include <ndn-cxx/kite/request.hpp>
#include <ndn-cxx/prefix-announcement.hpp>
#include <ndn-cxx/security/signing-helpers.hpp>
#include <ndn-cxx/util/logger.hpp>

#include <optional>

namespace ndn::kite::rv {

NDN_LOG_INIT(kite.rv);

Rv::Rv(Face& face, KeyChain& keyChain, const Options& options)
  : m_options(options)
  , m_face(face)
  , m_keyChain(keyChain)
  , m_validator(face)
{
  m_validator.load(m_options.validatorConfigFile);
}

void
Rv::start()
{
  m_registeredPrefix = m_face.setInterestFilter(
    m_options.prefixes[0],
    [this] (const auto&, const auto& interest) { this->onInterest(interest); },
    [] (const auto&, const auto& reason) {
      NDN_THROW(std::runtime_error("Failed to register prefix: " + reason));
    });
}

void
Rv::stop()
{
  m_registeredPrefix.cancel();
}

void
Rv::onInterest(const Interest& interest)
{
  afterReceive(interest.getName());

  m_validator.validate(interest,
                       [this] (const auto& i) { this->onValidationSuccess(i); },
                       [this] (const auto& i, const auto& e) { this->onValidationFailure(i, e); });
}

void
Rv::onValidationSuccess(const Interest& interest)
{
  NDN_LOG_DEBUG("KITE Request verified: " << interest.getName());

  std::optional<Request> req;
  try {
    req.emplace(interest);
  }
  catch (const std::invalid_argument& e) {
    NDN_LOG_WARN("Malformed KITE Request " << interest.getName() << ": " << e.what());
    return;
  }

  PrefixAnnouncement pa;
  pa.setAnnouncedName(req->getProducerPrefix());
  if (req->getExpiration()) {
    NDN_LOG_DEBUG("Request expiration=" << req->getExpiration()->count() << "ms");
    pa.setExpiration(*req->getExpiration());
  }
  else {
    pa.setExpiration(1_s);
  }

  Ack ack;
  ack.setPrefixAnnouncement(std::move(pa));

  try {
    m_face.put(ack.makeData(interest, m_keyChain,
                            security::signingByIdentity(m_options.prefixes[0])));
  }
  catch (const std::exception& e) {
    NDN_LOG_ERROR("Failed to send KITE Ack for " << interest.getName() << ": " << e.what());
  }
}

void
Rv::onValidationFailure(const Interest& interest, const security::ValidationError& error)
{
  NDN_LOG_DEBUG("KITE Request validation failed: " << interest.getName() << " error=" << error);
}

} // namespace ndn::kite::rv
