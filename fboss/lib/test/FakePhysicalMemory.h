/*
 *  Copyright (c) 2018-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */
#pragma once

#include "fboss/lib/PhysicalMemory.h"

namespace facebook {
namespace fboss {

class FakePhysicalMemory : public PhysicalMemory {
 public:
  using PhysicalMemory::PhysicalMemory;
  ~FakePhysicalMemory() {
    munmap();
  }
  void mmap() {
    if (data_) {
      delete[] data_;
    }
    data_ = new uint8_t[getSize()]();
    setVirtualAddress(data_);
  }

 private:
  void munmap() noexcept {
    if (data_) {
      delete[] data_;
      data_ = nullptr;
    }
    setVirtualAddress(nullptr);
  }
  uint8_t* data_{nullptr};
};

using FakePhysicalMemory8 = PhysicalMemory8<FakePhysicalMemory>;
using FakePhysicalMemory16 = PhysicalMemory16<FakePhysicalMemory>;
using FakePhysicalMemory32 = PhysicalMemory32<FakePhysicalMemory>;

} // namespace fboss
} // namespace facebook
