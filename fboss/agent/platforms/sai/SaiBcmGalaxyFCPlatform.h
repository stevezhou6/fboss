/*
 *  Copyright (c) 2004-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */
#pragma once

#include "fboss/agent/platforms/sai/SaiBcmGalaxyPlatform.h"
namespace facebook::fboss {

class TomahawkAsic;

class SaiBcmGalaxyFCPlatform : public SaiBcmGalaxyPlatform {
 public:
  explicit SaiBcmGalaxyFCPlatform(
      std::unique_ptr<PlatformProductInfo> productInfo,
      std::unique_ptr<PlatformMapping> platformMapping);
  ~SaiBcmGalaxyFCPlatform() override;
  std::vector<PortID> masterLogicalPortIds() const override;
  HwAsic* getAsic() const override;

 private:
  std::unique_ptr<TomahawkAsic> asic_;
};

} // namespace facebook::fboss
