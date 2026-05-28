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

#include "producer.hpp"

#include <ndn-cxx/interest-filter.hpp>
#include <ndn-cxx/kite/ack.hpp>
#include <ndn-cxx/kite/request.hpp>
#include <ndn-cxx/security/signing-helpers.hpp>
#include <ndn-cxx/util/logger.hpp>

namespace ndn::kite::producer {

NDN_LOG_INIT(kite.producer);

Producer::Producer(Face& face, KeyChain& keyChain, const Options& options)
  : m_options(options)
  , m_face(face)
  , m_keyChain(keyChain)
  , m_scheduler(m_face.getIoContext())
{
}

void
Producer::start()
{
  m_registeredPrefix = m_face.setInterestFilter(
    producerPrefix(),
    [this] (const auto& filter, const auto& interest) { this->onInterest(filter, interest); },
    [] (const auto&, const auto& reason) {
      NDN_THROW(std::runtime_error("Failed to register prefix: " + reason));
    });

  sendKiteRequest();
  scheduleNextRequest();
}

void
Producer::stop()
{
  m_nextEvent.cancel();
  m_registeredPrefix.cancel();
}

void
Producer::sendKiteRequest()
{
  Request req;
  req.setRvPrefix(m_options.rvPrefix);
  req.setProducerSuffix(m_options.producerSuffix);
  req.setExpiration(m_options.lifetime);

  Interest interest;
  try {
    interest = req.makeInterest(m_keyChain,
                                security::signingByIdentity(producerPrefix()));
  }
  catch (const std::exception& e) {
    NDN_LOG_WARN("Failed to sign KITE Request for " << producerPrefix() << ": " << e.what());
    return;
  }

  NDN_LOG_DEBUG("Sending KITE Request " << interest.getName());

  m_face.expressInterest(
    interest,
    [this] (const auto& i, const auto& d) { this->onAck(i, d); },
    [this] (const auto& i, const auto& n) { this->onNack(i, n); },
    [this] (const auto& i) { this->onTimeout(i); });
}

void
Producer::scheduleNextRequest()
{
  m_nextEvent = m_scheduler.schedule(m_options.interval, [this] {
    sendKiteRequest();
    scheduleNextRequest();
  });
}

void
Producer::onInterest(const InterestFilter& filter, const Interest& interest)
{
  afterReceive(interest.getName());

  NDN_LOG_DEBUG("Received Interest " << interest.getName()
                << " under filter " << filter.getPrefix());

  auto data = std::make_shared<Data>(interest.getName());
  data->setContent(interest.wireEncode());
  try {
    m_keyChain.sign(*data, security::signingByIdentity(producerPrefix()));
  }
  catch (const std::exception& e) {
    NDN_LOG_WARN("Failed to sign Data for " << interest.getName() << ": " << e.what());
    return;
  }
  m_face.put(*data);
}

void
Producer::onAck(const Interest& interest, const Data& data)
{
  try {
    Ack ack(data);
    Request req(interest);
    NDN_LOG_INFO("Received KITE Ack for " << req.getProducerPrefix());
  }
  catch (const std::exception& e) {
    NDN_LOG_WARN("Malformed KITE Ack for " << interest.getName() << ": " << e.what());
  }
}

void
Producer::onNack(const Interest& interest, const lp::Nack& nack)
{
  NDN_LOG_WARN("KITE Request " << interest.getName()
               << " NACKed, reason=" << nack.getReason());
}

void
Producer::onTimeout(const Interest& interest)
{
  NDN_LOG_DEBUG("KITE Request " << interest.getName() << " timed out");
}

} // namespace ndn::kite::producer
