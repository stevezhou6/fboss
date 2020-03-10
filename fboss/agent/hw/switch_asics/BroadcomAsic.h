// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "fboss/agent/hw/switch_asics/HwAsic.h"

namespace facebook::fboss {

class BroadcomAsic : public HwAsic {
 public:
  bool needsObjectKeyCache() const override {
    return true;
  }
};
} // namespace facebook::fboss
