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

#ifndef NDN_TOOLS_KITE_PRODUCER_HPP
#define NDN_TOOLS_KITE_PRODUCER_HPP

#include "core/common.hpp"

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/lp/nack.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/util/scheduler.hpp>
#include <ndn-cxx/util/signal.hpp>

namespace ndn::kite::producer {

/// Options for Producer.
struct Options
{
  Name rvPrefix;                         ///< RV prefix to announce under
  PartialName producerSuffix;            ///< producer suffix under rvPrefix
  time::milliseconds interval{1000};     ///< Request retransmission interval
  time::milliseconds lifetime{1000};     ///< Request expiration period
};

/**
 * @brief KITE mobile producer.
 *
 * On start(), registers an InterestFilter on `<rvPrefix>/<producerSuffix>` so
 * the producer can serve incoming consumer Interests with an echo-back Data
 * signed by its own identity, and immediately sends an initial KITE Request to
 * the RV. The Request is retransmitted every Options::interval milliseconds to
 * keep the reverse-path RIB entry installed by the RV alive across producer
 * mobility events.
 */
class Producer : noncopyable
{
public:
  Producer(Face& face, KeyChain& keyChain, const Options& options);

  /// Emitted on every received consumer Interest (Name passed in).
  signal::Signal<Producer, Name> afterReceive;

  /**
   * @brief Register the prefix filter, send the first KITE Request, and start
   *        the retransmission loop.
   * @note Non-blocking; caller must drive face.processEvents().
   */
  void
  start();

  /// Cancel the InterestFilter and stop the retransmission loop.
  void
  stop();

  /// Send one KITE Request immediately; does not affect the periodic schedule.
  void
  sendKiteRequest();

private:
  Name
  producerPrefix() const
  {
    return Name(m_options.rvPrefix).append(m_options.producerSuffix);
  }

  void
  onInterest(const InterestFilter& filter, const Interest& interest);

  void
  onAck(const Interest& interest, const Data& data);

  void
  onNack(const Interest& interest, const lp::Nack& nack);

  void
  onTimeout(const Interest& interest);

  void
  scheduleNextRequest();

private:
  const Options& m_options;
  Face& m_face;
  KeyChain& m_keyChain;
  RegisteredPrefixHandle m_registeredPrefix;
  Scheduler m_scheduler;
  scheduler::ScopedEventId m_nextEvent;
};

} // namespace ndn::kite::producer

#endif // NDN_TOOLS_KITE_PRODUCER_HPP
