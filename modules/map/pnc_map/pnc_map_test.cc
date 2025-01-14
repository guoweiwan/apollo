/******************************************************************************
 * Copyright 2017 The Apollo Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

#include <algorithm>
#include <vector>

#include "gflags/gflags.h"
#include "gtest/gtest.h"

#include "modules/common/util/file.h"
#include "modules/common/util/string_util.h"
#include "modules/map/hdmap/hdmap.h"
#include "modules/map/hdmap/hdmap_util.h"
#include "modules/routing/proto/routing.pb.h"

#define private public
#include "modules/map/pnc_map/pnc_map.h"

namespace apollo {
namespace hdmap {

DEFINE_string(test_map_file, "modules/map/data/sunnyvale_loop/base_map.xml",
              "The test map file");
DEFINE_string(
    test_routing_file,
    "modules/map/pnc_map/testdata/sample_sunnyvale_loop_routing.pb.txt",
    "The test map file");

class PncMapTest : public ::testing::Test {
 public:
  static void SetUpTestCase() {
    AINFO << "map file: " << FLAGS_test_map_file;
    if (hdmap_.LoadMapFromFile(FLAGS_test_map_file) != 0) {
      AERROR << "Failed to load map: " << FLAGS_test_map_file;
      CHECK(false);
    }
    pnc_map_.reset(new PncMap(&hdmap_));
    if (!common::util::GetProtoFromFile(FLAGS_test_routing_file, &routing_)) {
      AERROR << "Failed to load routing: " << FLAGS_test_routing_file;
      CHECK(false);
    }
    pnc_map_->UpdateRoutingResponse(routing_);
  }

  static double RouteLength(const RouteSegments& segments) {
    double s = 0.0;
    for (const auto& seg : segments) {
      s += seg.end_s - seg.start_s;
    }
    return s;
  }

  static routing::RoutingResponse routing_;
  static std::unique_ptr<PncMap> pnc_map_;
  static hdmap::HDMap hdmap_;
};

std::unique_ptr<PncMap> PncMapTest::pnc_map_;
hdmap::HDMap PncMapTest::hdmap_;
routing::RoutingResponse PncMapTest::routing_;

TEST_F(PncMapTest, RouteSegments_GetProjection) {
  auto lane1 = hdmap_.GetLaneById(hdmap::MakeMapId("9_1_-1"));
  RouteSegments route_segments;
  route_segments.emplace_back(lane1, 10, 20);
  LaneWaypoint waypoint;
  auto point = lane1->GetSmoothPoint(5);
  double s = 0.0;
  double l = 0.0;
  EXPECT_FALSE(route_segments.GetProjection(point, &s, &l, &waypoint));
  point = lane1->GetSmoothPoint(10);
  EXPECT_TRUE(route_segments.GetProjection(point, &s, &l, &waypoint));
  EXPECT_EQ(lane1, waypoint.lane);
  EXPECT_NEAR(10.0, waypoint.s, 1e-4);
  EXPECT_NEAR(0.0, s, 1e-4);
  EXPECT_NEAR(0.0, l, 1e-4);
  point = lane1->GetSmoothPoint(15);
  EXPECT_TRUE(route_segments.GetProjection(point, &s, &l, &waypoint));
  EXPECT_EQ(lane1, waypoint.lane);
  EXPECT_NEAR(15.0, waypoint.s, 1e-4);
  EXPECT_NEAR(5.0, s, 1e-4);
  EXPECT_NEAR(0.0, l, 1e-4);
  point = lane1->GetSmoothPoint(25);
  EXPECT_FALSE(route_segments.GetProjection(point, &s, &l, &waypoint));
  auto lane2 = hdmap_.GetLaneById(hdmap::MakeMapId("13_1_-1"));
  route_segments.emplace_back(lane2, 20, 30);
  EXPECT_FALSE(route_segments.GetProjection(point, &s, &l, &waypoint));
  point = lane2->GetSmoothPoint(0);
  EXPECT_FALSE(route_segments.GetProjection(point, &s, &l, &waypoint));
  point = lane2->GetSmoothPoint(25);
  EXPECT_TRUE(route_segments.GetProjection(point, &s, &l, &waypoint));
  EXPECT_EQ(lane2, waypoint.lane);
  EXPECT_NEAR(25.0, waypoint.s, 1e-4);
  EXPECT_NEAR(15.0, s, 1e-4);
  EXPECT_NEAR(0.0, l, 1e-4);
  point = lane2->GetSmoothPoint(31);
  EXPECT_FALSE(route_segments.GetProjection(point, &s, &l, &waypoint));
}

TEST_F(PncMapTest, GetNearestPointFromRouting) {
  common::PointENU point;
  point.set_x(587174.662136);
  point.set_y(4140933.06302);
  LaneWaypoint waypoint;
  pnc_map_->GetNearestPointFromRouting(point, &waypoint);
  EXPECT_EQ("9_1_-1", waypoint.lane->id().id());
  EXPECT_FLOAT_EQ(60.757099, waypoint.s);
}

TEST_F(PncMapTest, GetWaypointIndex) {
  auto lane = hdmap_.GetLaneById(hdmap::MakeMapId("9_1_-1"));
  ASSERT_TRUE(lane);
  LaneWaypoint waypoint(lane, 60.757099);
  auto result = pnc_map_->GetWaypointIndex(waypoint);
  ASSERT_EQ(3, result.size());
  EXPECT_EQ(0, result[0]);
  EXPECT_EQ(2, result[1]);
  EXPECT_EQ(0, result[2]);
}

TEST_F(PncMapTest, GetRouteSegments) {
  common::PointENU point;
  point.set_x(587174.662136);
  point.set_y(4140933.06302);
  std::vector<RouteSegments> segments;
  bool result = pnc_map_->GetRouteSegments(point, 10, 30, &segments);
  ASSERT_TRUE(result);
  ASSERT_EQ(2, segments.size());
  EXPECT_NEAR(40, RouteLength(segments[0]), 1e-4);
  EXPECT_EQ(routing::FORWARD, segments[0].change_lane_type());
  EXPECT_NEAR(40, RouteLength(segments[1]), 1e-4);
  EXPECT_EQ(routing::RIGHT, segments[1].change_lane_type());
}

TEST_F(PncMapTest, GetNeighborPassages) {
  const auto& road0 = routing_.road(0);
  {
    auto result = pnc_map_->GetNeighborPassages(road0, 0);
    EXPECT_EQ(2, result.size());
    EXPECT_EQ(0, result[0].first);
    EXPECT_EQ(routing::FORWARD, result[0].second);
    EXPECT_EQ(1, result[1].first);
    EXPECT_EQ(routing::RIGHT, result[1].second);
  }
  {
    auto result = pnc_map_->GetNeighborPassages(road0, 1);
    EXPECT_EQ(3, result.size());

    EXPECT_EQ(1, result[0].first);
    EXPECT_EQ(routing::FORWARD, result[0].second);

    EXPECT_EQ(0, result[1].first);
    EXPECT_EQ(routing::LEFT, result[1].second);

    EXPECT_EQ(2, result[2].first);
    EXPECT_EQ(routing::LEFT, result[1].second);
  }
  {
    auto result = pnc_map_->GetNeighborPassages(road0, 2);
    EXPECT_EQ(3, result.size());

    EXPECT_EQ(2, result[0].first);
    EXPECT_EQ(routing::FORWARD, result[0].second);

    EXPECT_EQ(1, result[1].first);
    EXPECT_EQ(routing::RIGHT, result[1].second);

    EXPECT_EQ(3, result[2].first);
    EXPECT_EQ(routing::RIGHT, result[2].second);
  }
  {
    auto result = pnc_map_->GetNeighborPassages(road0, 3);
    EXPECT_EQ(1, result.size());

    EXPECT_EQ(3, result[0].first);
    EXPECT_EQ(routing::FORWARD, result[0].second);
  }
}

}  // namespace hdmap
}  // namespace apollo
