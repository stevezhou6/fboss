/*
 *  Copyright (c) 2004-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */
#include "fboss/agent/FbossError.h"
#include "fboss/agent/hw/sai/api/SaiApiTable.h"
#include "fboss/agent/hw/sai/fake/FakeSai.h"
#include "fboss/agent/hw/sai/switch/SaiManagerTable.h"
#include "fboss/agent/hw/sai/switch/SaiRouteManager.h"
#include "fboss/agent/hw/sai/switch/tests/ManagerTestBase.h"
#include "fboss/agent/state/Route.h"
#include "fboss/agent/types.h"

using namespace facebook::fboss;
class RouteManagerTest : public ManagerTestBase {
 public:
  void SetUp() override {
    setupStage = SetupStage::PORT | SetupStage::VLAN | SetupStage::INTERFACE |
        SetupStage::NEIGHBOR;
    ManagerTestBase::SetUp();
    d1 = {folly::IPAddress{"42.42.42.42"}, 24};
    d2 = {folly::IPAddress{"43.43.43.43"}, 24};
    tr1.destination = d1;
    tr2.destination = d2;
    tr1.nextHopInterfaces.push_back(testInterfaces.at(0));
    tr1.nextHopInterfaces.push_back(testInterfaces.at(1));
    tr1.nextHopInterfaces.push_back(testInterfaces.at(2));
    tr1.nextHopInterfaces.push_back(testInterfaces.at(3));
  }

  folly::CIDRNetwork d1;
  folly::CIDRNetwork d2;
  TestRoute tr1;
  TestRoute tr2;
};

TEST_F(RouteManagerTest, addRoute) {
  auto r = makeRoute(tr1);
  saiManagerTable->routeManager().addRoute<folly::IPAddressV4>(RouterID(0), r);
}

TEST_F(RouteManagerTest, addRouteSameNextHops) {
  tr2.nextHopInterfaces = tr1.nextHopInterfaces;
  auto r1 = makeRoute(tr1);
  auto r2 = makeRoute(tr2);
  saiManagerTable->routeManager().addRoute<folly::IPAddressV4>(RouterID(0), r1);
  saiManagerTable->routeManager().addRoute<folly::IPAddressV4>(RouterID(0), r2);
}

TEST_F(RouteManagerTest, addRouteDifferentNextHops) {
  auto r1 = makeRoute(tr1);
  tr2.nextHopInterfaces.push_back(testInterfaces.at(1));
  tr2.nextHopInterfaces.push_back(testInterfaces.at(3));
  auto r2 = makeRoute(tr2);
  saiManagerTable->routeManager().addRoute<folly::IPAddressV4>(RouterID(0), r1);
  saiManagerTable->routeManager().addRoute<folly::IPAddressV4>(RouterID(0), r2);
}

TEST_F(RouteManagerTest, addRouteOneNextHop) {
  tr1.nextHopInterfaces = {testInterfaces.at(1)};
  auto r = makeRoute(tr1);
  saiManagerTable->routeManager().addRoute<folly::IPAddressV4>(RouterID(0), r);
}

TEST_F(RouteManagerTest, addToCpuRoute) {
  TestInterface intf = testInterfaces.at(1);
  RouteFields<folly::IPAddressV4>::Prefix destination;
  destination.network = intf.subnet.first.asV4();
  destination.mask = intf.subnet.second;
  auto r = std::make_shared<Route<folly::IPAddressV4>>(destination);
  RouteNextHopEntry entry(
      RouteForwardAction::TO_CPU, AdminDistance::STATIC_ROUTE);
  r->update(ClientID{42}, entry);
  r->setResolved(entry);
  saiManagerTable->routeManager().addRoute<folly::IPAddressV4>(RouterID(0), r);
}

TEST_F(RouteManagerTest, addDropRoute) {
  TestInterface intf = testInterfaces.at(1);
  RouteFields<folly::IPAddressV4>::Prefix destination;
  destination.network = intf.subnet.first.asV4();
  destination.mask = intf.subnet.second;
  auto r = std::make_shared<Route<folly::IPAddressV4>>(destination);
  RouteNextHopEntry entry(
      RouteForwardAction::DROP, AdminDistance::STATIC_ROUTE);
  r->update(ClientID{42}, entry);
  r->setResolved(entry);
  saiManagerTable->routeManager().addRoute<folly::IPAddressV4>(RouterID(0), r);
}

TEST_F(RouteManagerTest, addSubnetRoute) {
  TestInterface intf = testInterfaces.at(1);
  RouteFields<folly::IPAddressV4>::Prefix destination;
  destination.network = intf.subnet.first.asV4();
  destination.mask = intf.subnet.second;
  ResolvedNextHop nh{intf.routerIp, InterfaceID(intf.id), ECMP_WEIGHT};
  RouteNextHopEntry::NextHopSet swNextHops{nh};
  RouteNextHopEntry entry(swNextHops, AdminDistance::DIRECTLY_CONNECTED);
  auto r = std::make_shared<Route<folly::IPAddressV4>>(destination);
  r->update(ClientID{42}, entry);
  r->setResolved(entry);
  r->setConnected();
  saiManagerTable->routeManager().addRoute<folly::IPAddressV4>(RouterID(0), r);
}

TEST_F(RouteManagerTest, getRoute) {
  auto r = makeRoute(tr1);
  saiManagerTable->routeManager().addRoute<folly::IPAddressV4>(RouterID(0), r);
  auto entry =
      saiManagerTable->routeManager().routeEntryFromSwRoute(RouterID(0), r);
  SaiRoute* saiRoute = saiManagerTable->routeManager().getRoute(entry);
  EXPECT_TRUE(saiRoute);
}

TEST_F(RouteManagerTest, removeRoute) {
  auto r = makeRoute(tr1);
  saiManagerTable->routeManager().addRoute<folly::IPAddressV4>(RouterID(0), r);
  auto entry =
      saiManagerTable->routeManager().routeEntryFromSwRoute(RouterID(0), r);
  SaiRoute* saiRoute = saiManagerTable->routeManager().getRoute(entry);
  EXPECT_TRUE(saiRoute);
  saiManagerTable->routeManager().removeRoute(RouterID(0), r);
  saiRoute = saiManagerTable->routeManager().getRoute(entry);
  EXPECT_FALSE(saiRoute);
}

TEST_F(RouteManagerTest, addDupRoute) {
  auto r = makeRoute(tr1);
  saiManagerTable->routeManager().addRoute<folly::IPAddressV4>(RouterID(0), r);
  EXPECT_THROW(
      saiManagerTable->routeManager().addRoute<folly::IPAddressV4>(
          RouterID(0), r),
      FbossError);
}

TEST_F(RouteManagerTest, getNonexistentRoute) {
  auto r1 = makeRoute(tr1);
  tr2.nextHopInterfaces = tr1.nextHopInterfaces;
  auto r2 = makeRoute(tr2);
  saiManagerTable->routeManager().addRoute<folly::IPAddressV4>(RouterID(0), r1);
  auto entry =
      saiManagerTable->routeManager().routeEntryFromSwRoute(RouterID(0), r2);
  SaiRoute* saiRoute = saiManagerTable->routeManager().getRoute(entry);
  EXPECT_FALSE(saiRoute);
}

TEST_F(RouteManagerTest, removeNonexistentRoute) {
  auto r1 = makeRoute(tr1);
  tr2.nextHopInterfaces = tr1.nextHopInterfaces;
  auto r2 = makeRoute(tr2);
  saiManagerTable->routeManager().addRoute<folly::IPAddressV4>(RouterID(0), r1);
  EXPECT_THROW(
      saiManagerTable->routeManager().removeRoute(RouterID(0), r2), FbossError);
}
