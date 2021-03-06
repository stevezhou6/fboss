/*
 *  Copyright (c) 2004-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */
#include "fboss/agent/hw/bcm/BcmStatUpdater.h"

#include "fboss/agent/hw/bcm/BcmAclStat.h"
#include "fboss/agent/hw/bcm/BcmAddressFBConvertors.h"
#include "fboss/agent/hw/bcm/BcmError.h"
#include "fboss/agent/hw/bcm/BcmFieldProcessorFBConvertors.h"
#include "fboss/agent/hw/bcm/BcmSwitch.h"

#include <boost/container/flat_map.hpp>

extern "C" {
#include <bcm/field.h>
}

namespace facebook::fboss {

using std::chrono::duration_cast;
using std::chrono::seconds;
using std::chrono::system_clock;

using facebook::fboss::bcmCheckError;

BcmStatUpdater::BcmStatUpdater(BcmSwitch* hw, bool isAlpmEnabled)
    : hw_(hw),
      bcmTableStatsManager_(
          std::make_unique<BcmHwTableStatManager>(hw, isAlpmEnabled)) {}

std::string BcmStatUpdater::counterTypeToString(cfg::CounterType type) {
  switch (type) {
    case cfg::CounterType::PACKETS:
      return "packets";
    case cfg::CounterType::BYTES:
      return "bytes";
  }
  throw FbossError("Unsupported Counter Type");
}

void BcmStatUpdater::toBeAddedAclStat(
    BcmAclStatHandle handle,
    const std::string& name,
    const std::vector<cfg::CounterType>& counterTypes) {
  for (auto type : counterTypes) {
    std::string counterName = name + "." + counterTypeToString(type);
    toBeAddedAclStats_.emplace(counterName, AclCounterDescriptor(handle, type));
  }
}

void BcmStatUpdater::toBeRemovedAclStat(BcmAclStatHandle handle) {
  toBeRemovedAclStats_.emplace(handle);
}

void BcmStatUpdater::refreshPostBcmStateChange(const StateDelta& delta) {
  refreshHwTableStats(delta);
  refreshAclStats();
}

void BcmStatUpdater::updateStats() {
  updateAclStats();
  updateHwTableStats();
}

void BcmStatUpdater::updateAclStats() {
  auto now = duration_cast<seconds>(system_clock::now().time_since_epoch());
  auto lockedAclStats = aclStats_.wlock();
  for (auto& entry : *lockedAclStats) {
    updateAclStat(
        hw_->getUnit(),
        entry.first.handle,
        entry.first.counterType,
        now,
        entry.second.get());
  }
}

void BcmStatUpdater::updateHwTableStats() {
  bcmTableStatsManager_->publish(*tableStats_.rlock());
}

size_t BcmStatUpdater::getCounterCount() const {
  return aclStats_.rlock()->size();
}

MonotonicCounter* FOLLY_NULLABLE
BcmStatUpdater::getCounterIf(BcmAclStatHandle handle, cfg::CounterType type) {
  auto lockedAclStats = aclStats_.rlock();
  auto iter = lockedAclStats->find(AclCounterDescriptor(handle, type));
  return iter != lockedAclStats->end() ? iter->second.get() : nullptr;
}

void BcmStatUpdater::clearPortStats(
    const std::unique_ptr<std::vector<int32_t>>& ports) {
  // XXX clear per queue stats and, BST stats as well.
  for (auto port : *ports) {
    auto ret = bcm_stat_clear(hw_->getUnit(), port);
    if (BCM_FAILURE(ret)) {
      XLOG(ERR) << "Clear Failed for port " << port << " :" << bcm_errmsg(ret);
      return;
    }
  }
}

void BcmStatUpdater::refreshHwTableStats(const StateDelta& delta) {
  auto stats = tableStats_.wlock();
  bcmTableStatsManager_->refresh(delta, &(*stats));
}

void BcmStatUpdater::refreshAclStats() {
  if (toBeRemovedAclStats_.empty() && toBeAddedAclStats_.empty()) {
    return;
  }

  auto lockedAclStats = aclStats_.wlock();

  while (!toBeRemovedAclStats_.empty()) {
    BcmAclStatHandle handle = toBeRemovedAclStats_.front();
    auto itr = lockedAclStats->begin();
    while (itr != lockedAclStats->end()) {
      if (itr->first.handle == handle) {
        lockedAclStats->erase(itr++);
      } else {
        ++itr;
      }
    }
    toBeRemovedAclStats_.pop();
  }

  while (!toBeAddedAclStats_.empty()) {
    const std::string& name = toBeAddedAclStats_.front().first;
    auto aclCounterDescriptor = toBeAddedAclStats_.front().second;
    auto inserted = lockedAclStats->emplace(
        aclCounterDescriptor,
        std::make_unique<MonotonicCounter>(name, fb303::SUM, fb303::RATE));
    if (!inserted.second) {
      throw FbossError(
          "Duplicate ACL stat handle, handle=",
          aclCounterDescriptor.handle,
          ", name=",
          name);
    }
    toBeAddedAclStats_.pop();
  }
}

void BcmStatUpdater::updateAclStat(
    int unit,
    BcmAclStatHandle handle,
    cfg::CounterType counterType,
    std::chrono::seconds now,
    MonotonicCounter* counter) {
  uint64_t value;
  bcm_field_stat_t type = utility::cfgCounterTypeToBcmCounterType(counterType);
  auto rv = bcm_field_stat_get(unit, handle, type, &value);
  bcmCheckError(rv, "Failed to update stat=", handle);
  counter->updateValue(now, value);
}

} // namespace facebook::fboss
