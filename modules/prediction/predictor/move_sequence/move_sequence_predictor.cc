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

#include "modules/prediction/predictor/move_sequence/move_sequence_predictor.h"

#include <cmath>
#include <utility>
#include <limits>
#include <memory>

#include "Eigen/Dense"
#include "modules/common/adapters/proto/adapter_config.pb.h"
#include "modules/common/log.h"
#include "modules/common/math/math_utils.h"
#include "modules/map/hdmap/hdmap_util.h"
#include "modules/prediction/common/prediction_gflags.h"
#include "modules/prediction/common/prediction_util.h"
#include "modules/prediction/common/prediction_map.h"
#include "modules/prediction/common/road_graph.h"
#include "modules/prediction/container/container_manager.h"
#include "modules/prediction/container/obstacles/obstacles_container.h"
#include "modules/prediction/container/pose/pose_container.h"

namespace apollo {
namespace prediction {

using apollo::common::PathPoint;
using apollo::common::TrajectoryPoint;
using apollo::common::math::KalmanFilter;
using apollo::common::adapter::AdapterConfig;
using apollo::hdmap::LaneInfo;
using apollo::common::Point3D;

namespace {

void WeightedMean(
    const TrajectoryPoint& point1,
    const TrajectoryPoint& point2,
    const double weight1, const double weight2,
    TrajectoryPoint* ret_point) {
  // TODO(all) Double check if the following CHECK is okay.
  CHECK_EQ(point1.relative_time(), point2.relative_time());

  double ret_x = weight1 * point1.path_point().x() +
                 weight2 * point2.path_point().x();
  double ret_y = weight1 * point1.path_point().y() +
                 weight2 * point2.path_point().y();
  double ret_z = 0.0;
  double ret_theta = weight1 * point1.path_point().theta() +
                     weight2 * point2.path_point().theta();
  double ret_v = weight1 * point1.v() + weight2 * point2.v();
  double ret_a = weight1 * point1.a() + weight2 * point2.a();
  double ret_relative_time = point1.relative_time();

  ret_point->mutable_path_point()->set_x(ret_x);
  ret_point->mutable_path_point()->set_y(ret_y);
  ret_point->mutable_path_point()->set_z(ret_z);
  ret_point->mutable_path_point()->set_theta(ret_theta);
  ret_point->set_v(ret_v);
  ret_point->set_a(ret_a);
  ret_point->set_relative_time(ret_relative_time);
}

}  // namespace

void MoveSequencePredictor::Predict(Obstacle* obstacle) {
  Clear();

  CHECK_NOTNULL(obstacle);
  CHECK_GT(obstacle->history_size(), 0);

  const Feature& feature = obstacle->latest_feature();
  if (!feature.has_lane() || !feature.lane().has_lane_graph()) {
    AERROR << "Obstacle [" << obstacle->id() << " has no lane graph.";
    return;
  }

  std::string lane_id = "";
  if (feature.lane().has_lane_feature()) {
    lane_id = feature.lane().lane_feature().lane_id();
  }
  int num_lane_sequence = feature.lane().lane_graph().lane_sequence_size();
  std::vector<bool> enable_lane_sequence(num_lane_sequence, true);
  FilterLaneSequences(feature.lane().lane_graph(), lane_id,
                      &enable_lane_sequence);
  for (int i = 0; i < num_lane_sequence; ++i) {
    const LaneSequence& sequence = feature.lane().lane_graph().lane_sequence(i);
    if (sequence.lane_segment_size() <= 0) {
      AERROR << "Empty lane segments.";
      continue;
    }

    if (!enable_lane_sequence[i]) {
      ADEBUG << "Lane sequence [" << ToString(sequence)
             << "] with probability [" << sequence.probability()
             << "] is disqualified.";
      continue;
    }

    ADEBUG << "Obstacle [" << obstacle->id()
           << "] will draw a lane sequence trajectory [" << ToString(sequence)
           << "] with probability [" << sequence.probability() << "].";

    std::string curr_lane_id = sequence.lane_segment(0).lane_id();
    std::vector<TrajectoryPoint> points;
    DrawLaneSequenceTrajectoryPoints(*obstacle, sequence,
        FLAGS_prediction_duration, FLAGS_prediction_freq, &points);

    Trajectory trajectory = GenerateTrajectory(points);
    trajectory.set_probability(sequence.probability());
    trajectories_.push_back(std::move(trajectory));
  }
  ADEBUG << "Obstacle [" << obstacle->id() << "] has total "
         << trajectories_.size() << " trajectories.";
}

void MoveSequencePredictor::DrawLaneSequenceTrajectoryPoints(
    const Obstacle& obstacle,
    const LaneSequence& lane_sequence,
    const double total_time, const double freq,
    std::vector<TrajectoryPoint>* points) {

  points->clear();
  std::vector<TrajectoryPoint> maneuver_trajectory_points;
  std::vector<TrajectoryPoint> motion_trajectory_points;
  DrawManeuverTrajectoryPoints(obstacle, lane_sequence, total_time, freq,
      &maneuver_trajectory_points);
  DrawMotionTrajectoryPoints(obstacle, total_time, freq,
      &motion_trajectory_points);
  CHECK_EQ(maneuver_trajectory_points.size(),
           motion_trajectory_points.size());
  double t = 0.0;
  for (size_t i = 0; i < maneuver_trajectory_points.size(); ++i) {
    double motion_weight = MotionWeight(t);
    const TrajectoryPoint& maneuver_point = maneuver_trajectory_points[i];
    const TrajectoryPoint& motion_point = motion_trajectory_points[i];
    TrajectoryPoint trajectory_point;
    WeightedMean(maneuver_point, motion_point,
        1 - motion_weight, motion_weight, &trajectory_point);
    points->push_back(trajectory_point);
    t += freq;
  }
}

void MoveSequencePredictor::DrawManeuverTrajectoryPoints(
    const Obstacle& obstacle,
    const LaneSequence& lane_sequence,
    const double total_time, const double freq,
    std::vector<TrajectoryPoint>* points) {

  const Feature& feature = obstacle.latest_feature();
  if (!feature.has_position() || !feature.has_velocity() ||
      !feature.position().has_x() || !feature.position().has_y()) {
    AERROR << "Obstacle [" << obstacle.id()
           << " is missing position or velocity";
    return;
  }

  // TODO(kechxu) take care of enable_kf_tracking
  Eigen::Vector2d position(feature.position().x(), feature.position().y());
  double time_to_lane_center = ComputeTimeToLaneCenter(obstacle, lane_sequence);

  std::array<double, 6> lateral_coeffs;
  std::array<double, 5> longitudinal_coeffs;
  GetLateralPolynomial(obstacle, lane_sequence, time_to_lane_center,
      &lateral_coeffs);
  GetLongitudinalPolynomial(obstacle, lane_sequence, time_to_lane_center,
      &longitudinal_coeffs);

  int lane_segment_index = 0;
  std::string lane_id =
      lane_sequence.lane_segment(lane_segment_index).lane_id();

  PredictionMap* map = PredictionMap::instance();
  std::shared_ptr<const LaneInfo> lane_info = map->LaneById(lane_id);
  double lane_s = 0.0;
  double lane_l = 0.0;
  if (!map->GetProjection(position, lane_info, &lane_s, &lane_l)) {
    AERROR << "Failed in getting lane s and lane l";
    return;
  }

  size_t total_num = static_cast<size_t>(total_time / freq);
  // size_t num_to_center = static_cast<size_t>(time_to_lane_center / freq);
  for (size_t i = 0; i < total_num; ++i) {
    double relative_time = static_cast<double>(i) * freq;
    Eigen::Vector2d point;
    double theta = M_PI;
    // TODO(kechxu) take care of the situation i > num_to_center
    lane_l = EvaluateLateralPolynomial(lateral_coeffs, relative_time, 0);
    double curr_s =
        EvaluateLongitudinalPolynomial(longitudinal_coeffs, relative_time, 0);
    double prev_s = (i > 0) ?
        EvaluateLongitudinalPolynomial(longitudinal_coeffs,
            relative_time - freq, 0) : 0.0;
    lane_s += (curr_s - prev_s);

    if (!map->SmoothPointFromLane(lane_id, lane_s, lane_l, &point, &theta)) {
      AERROR << "Unable to get smooth point from lane [" << lane_id
             << "] with s [" << lane_s << "] and l [" << lane_l << "]";
      break;
    }

    if (points->size() > 0) {
      PathPoint* prev_point = points->back().mutable_path_point();
      double x_diff = point.x() - prev_point->x();
      double y_diff = point.y() - prev_point->y();
      if (std::fabs(x_diff) > std::numeric_limits<double>::epsilon() ||
          std::fabs(y_diff) > std::numeric_limits<double>::epsilon()) {
        theta = std::atan2(y_diff, x_diff);
        prev_point->set_theta(theta);
      } else {
        theta = prev_point->theta();
      }
    }

    double vs = EvaluateLongitudinalPolynomial(longitudinal_coeffs,
                    relative_time, 1);
    double vl = EvaluateLateralPolynomial(lateral_coeffs, relative_time, 1);
    double lane_speed = std::hypot(vs, vl);

    double as = EvaluateLongitudinalPolynomial(longitudinal_coeffs,
                    relative_time, 2);
    double al = EvaluateLateralPolynomial(lateral_coeffs, relative_time, 2);
    double lane_acc = std::hypot(as, al);

    TrajectoryPoint trajectory_point;
    PathPoint path_point;
    path_point.set_x(point.x());
    path_point.set_y(point.y());
    path_point.set_z(0.0);
    path_point.set_theta(theta);
    trajectory_point.mutable_path_point()->CopyFrom(path_point);
    trajectory_point.set_v(lane_speed);
    trajectory_point.set_a(lane_acc);
    trajectory_point.set_relative_time(relative_time);
    points->emplace_back(std::move(trajectory_point));

    while (lane_s > map->LaneById(lane_id)->total_length() &&
           lane_segment_index + 1 < lane_sequence.lane_segment_size()) {
      lane_segment_index += 1;
      lane_s = lane_s - map->LaneById(lane_id)->total_length();
      lane_id = lane_sequence.lane_segment(lane_segment_index).lane_id();
    }
  }
}

void MoveSequencePredictor::GetLongitudinalPolynomial(
    const Obstacle& obstacle,
    const LaneSequence& lane_sequence,
    const double time_to_lane_center,
    std::array<double, 5>* coefficients) {

  CHECK_GT(obstacle.history_size(), 0);
  CHECK_GT(lane_sequence.lane_segment_size(), 0);
  CHECK_GT(lane_sequence.lane_segment(0).lane_point_size(), 0);
  const Feature& feature = obstacle.latest_feature();
  // TODO(kechxu) check speed is large enough to get valid velocity_heading
  double theta = feature.velocity_heading();
  double v = feature.speed();
  double a = feature.acc();
  if (FLAGS_enable_kf_tracking) {
    v = feature.t_speed();
    a = feature.t_acc();
  }
  double lane_heading = lane_sequence.lane_segment(0).lane_point(0).heading();

  double s0 = 0.0;
  double ds0 = v * std::cos(theta - lane_heading);
  double dds0 = a * std::cos(theta - lane_heading);
  double ds1 = v;
  double dds1 = a;
  double p = time_to_lane_center;

  coefficients->operator[](0) = s0;
  coefficients->operator[](1) = ds0;
  coefficients->operator[](2) = 0.5 * dds0;
  double b0 = ds1 - dds0 * p - ds0;
  double b1 = dds1 - dds0;
  double p2 = p * p;
  double p3 = p2 * p;
  coefficients->operator[](3) = b0 / p2 - b1 / 3.0 / p;
  coefficients->operator[](4) = -0.5 / p3 * b0 + 0.25 / p2 * b1;
}

void MoveSequencePredictor::GetLateralPolynomial(
    const Obstacle& obstacle,
    const LaneSequence& lane_sequence,
    const double time_to_lane_center,
    std::array<double, 6>* coefficients) {
  CHECK_GT(obstacle.history_size(), 0);
  CHECK_GT(lane_sequence.lane_segment_size(), 0);
  CHECK_GT(lane_sequence.lane_segment(0).lane_point_size(), 0);
  const Feature& feature = obstacle.latest_feature();
  // TODO(kechxu) check speed is large enough to get valid velocity_heading
  double theta = feature.velocity_heading();
  double v = feature.speed();
  double a = feature.acc();
  Point3D position = feature.position();
  if (FLAGS_enable_kf_tracking) {
    v = feature.t_speed();
    a = feature.t_acc();
    position = feature.t_position();
  }
  const LanePoint& start_lane_point =
      lane_sequence.lane_segment(0).lane_point(0);
  double pos_delta_x = position.x() - start_lane_point.position().x();
  double pos_delta_y = position.y() - start_lane_point.position().y();
  double lane_heading_x = std::cos(start_lane_point.heading());
  double lane_heading_y = std::sin(start_lane_point.heading());
  double cross_prod =
      lane_heading_x * pos_delta_y - lane_heading_y * pos_delta_x;
  double shift = std::hypot(pos_delta_x, pos_delta_y);

  double l0 = (cross_prod > 0) ? shift : -shift;
  double dl0 = v * std::sin(theta - start_lane_point.heading());
  double ddl0 = a * std::sin(theta - start_lane_point.heading());
  double l1 = 0.0;
  double dl1 = 0.0;
  double ddl1 = 0.0;

  coefficients->operator[](0) = l0;
  coefficients->operator[](1) = dl0;
  coefficients->operator[](2) = ddl0 / 2.0;
  double p = time_to_lane_center;
  double p2 = p * p;
  double p3 = p2 * p;
  double c0 = (l1 - 0.5 * p2 * ddl0 - dl0 * p - l0) / p3;
  double c1 = (dl1 - ddl0 * p - dl0) / p2;
  double c2 = (ddl1 - ddl0) / p;

  coefficients->operator[](3) = 0.5 * (20.0 * c0 - 8.0 * c1 + c2);
  coefficients->operator[](4) = (-15.0 * c0 + 7.0 * c1 - c2) / p;
  coefficients->operator[](5) = (6.0 * c0 - 3.0 * c1 + 0.5 * c2) / p2;
}

double MoveSequencePredictor::EvaluateLateralPolynomial(
    const std::array<double, 6>& coeffs, const double t, const uint32_t order) {
  switch (order) {
    case 0: {
      return ((((coeffs[5] * t + coeffs[4]) * t + coeffs[3]) * t +
          coeffs[2]) * t + coeffs[1]) * t + coeffs[0];
    }
    case 1: {
      return (((5.0 * coeffs[5] * t + 4.0 * coeffs[4]) * t +
          3.0 * coeffs[3]) * t + 2.0 * coeffs[2]) * t + coeffs[1];
    }
    case 2: {
      return (((20.0 * coeffs[5] * t + 12.0 * coeffs[4]) * t) +
          6.0 * coeffs[3]) * t + 2.0 * coeffs[2];
    }
    case 3: {
      return (60.0 * coeffs[5] * t + 24.0 * coeffs[4]) * t + 6.0 * coeffs[3];
    }
    case 4: {
      return 120.0 * coeffs[5] * t + 24.0 * coeffs[4];
    }
    case 5: {
      return 120.0 * coeffs[5];
    }
    default:
      return 0.0;
  }
}

double MoveSequencePredictor::EvaluateLongitudinalPolynomial(
    const std::array<double, 5>& coeffs, const double t, const uint32_t order) {
  switch (order) {
    case 0: {
      return (((coeffs[4] * t + coeffs[3]) * t + coeffs[2]) * t +
          coeffs[1]) * t + coeffs[0];
    }
    case 1: {
      return ((4.0 * coeffs[4] * t + 3.0 * coeffs[3]) * t +
          2.0 * coeffs[2]) * t + coeffs[1];
    }
    case 2: {
      return (12.0 * coeffs[4] * t + 6.0 * coeffs[3]) * t + 2.0 * coeffs[2];
    }
    case 3: {
      return 24.0 * coeffs[4] * t + 6.0 * coeffs[3];
    }
    case 4: {
      return 24.0 * coeffs[4];
    }
    default:
      return 0.0;
  }
}

void MoveSequencePredictor::DrawMotionTrajectoryPoints(
    const Obstacle& obstacle,
    const double total_time, const double freq,
    std::vector<TrajectoryPoint>* points) {

  // Apply free_move here
  const Feature& feature = obstacle.latest_feature();
  if (!feature.has_position() || !feature.has_velocity() ||
      !feature.position().has_x() || !feature.position().has_y()) {
    AERROR << "Obstacle [" << obstacle.id()
           << " is missing position or velocity";
    return;
  }

  Eigen::Vector2d position(feature.position().x(), feature.position().y());
  Eigen::Vector2d velocity(feature.velocity().x(), feature.velocity().y());
  Eigen::Vector2d acc(feature.acceleration().x(), feature.acceleration().y());
  if (FLAGS_enable_kf_tracking) {
    position(0) = feature.t_position().x();
    position(1) = feature.t_position().y();
    velocity(0) = feature.t_velocity().x();
    velocity(1) = feature.t_velocity().y();
    acc(0) = feature.t_acceleration().x();
    acc(1) = feature.t_acceleration().y();
  }
  const KalmanFilter<double, 6, 2, 0>& kf = obstacle.kf_motion_tracker();

  Eigen::Matrix<double, 6, 1> state(kf.GetStateEstimate());
  state(0, 0) = 0.0;
  state(1, 0) = 0.0;
  state(2, 0) = velocity(0);
  state(3, 0) = velocity(1);
  state(4, 0) = common::math::Clamp(acc(0), FLAGS_min_acc, FLAGS_max_acc);
  state(5, 0) = common::math::Clamp(acc(1), FLAGS_min_acc, FLAGS_max_acc);

  Eigen::Matrix<double, 6, 6> transition(kf.GetTransitionMatrix());
  transition(0, 2) = freq;
  transition(0, 4) = 0.5 * freq * freq;
  transition(1, 3) = freq;
  transition(1, 5) = 0.5 * freq * freq;
  transition(2, 4) = freq;
  transition(3, 5) = freq;

  size_t num = static_cast<size_t>(total_time / freq);
  apollo::prediction::predictor_util::GenerateFreeMoveTrajectoryPoints(
      &state, transition, num, freq, points);

  for (size_t i = 0; i < points->size(); ++i) {
    apollo::prediction::predictor_util::TranslatePoint(
        position[0], position[1], &(points->operator[](i)));
  }
}

double MoveSequencePredictor::ComputeTimeToLaneCenter(
    const Obstacle& obstacle, const LaneSequence& lane_sequence) {
  std::vector<double> candidate_times;
  GenerateCandidateTimes(&candidate_times);
  if (candidate_times.empty()) {
    AWARN << "No candidate times found, use default value.";
    return 2.0;
  }
  double t_best = candidate_times[0];
  double cost_min = std::numeric_limits<double>::max();
  for (double t : candidate_times) {
    std::array<double, 6> lateral_coeffs;
    std::array<double, 5> longitudinal_coeffs;
    GetLateralPolynomial(obstacle, lane_sequence, t, &lateral_coeffs);
    GetLongitudinalPolynomial(obstacle, lane_sequence, t, &longitudinal_coeffs);
    double cost = Cost(t, lateral_coeffs, longitudinal_coeffs);
    if (cost < cost_min) {
      t_best = t;
      cost_min = cost;
    }
  }
  return t_best;
}

double MoveSequencePredictor::Cost(const double t,
    const std::array<double, 6>& lateral_coeffs,
    const std::array<double, 5>& longitudinal_coeffs) {
  // TODO(kechxu) implement
  return 0.0;
}

void MoveSequencePredictor::GenerateCandidateTimes(
    std::vector<double>* candidate_times) {
  // TODO(kechxu) implement
}

double MoveSequencePredictor::MotionWeight(const double t) {
  // TODO(kechxu) Avoid the following hard-coded constants
  double a = 1.2;
  double b = 5.0;
  double c = 1.5;

  return 1.0 - 1.0 / (1.0 + a * std::exp(-b * (t - c)));
}

void MoveSequencePredictor::FilterLaneSequences(
    const LaneGraph& lane_graph, const std::string& lane_id,
    std::vector<bool>* enable_lane_sequence) {
  int num_lane_sequence = lane_graph.lane_sequence_size();
  std::vector<int> lane_change_type(num_lane_sequence, -1);
  std::pair<int, double> change(-1, -1.0);
  std::pair<int, double> all(-1, -1.0);

  // Get ADC status
  GetADC();

  for (int i = 0; i < num_lane_sequence; ++i) {
    const LaneSequence& sequence = lane_graph.lane_sequence(i);

    // Get lane change type
    lane_change_type[i] = GetLaneChangeType(lane_id, sequence);

    double probability = sequence.probability();

    if (probability > all.second ||
        (probability == all.second && lane_change_type[i] == 0)) {
      all.first = i;
      all.second = probability;
    }
    if (lane_change_type[i] > 0 && probability > change.second) {
      change.first = i;
      change.second = probability;
    }
  }

  for (int i = 0; i < num_lane_sequence; ++i) {
    const LaneSequence& sequence = lane_graph.lane_sequence(i);

    // The obstacle has interference with ADC within a small distance
    if (GetLaneChangeDistanceWithADC(sequence) < FLAGS_lane_change_dist) {
      (*enable_lane_sequence)[i] = false;
      continue;
    }

    double probability = sequence.probability();
    if (probability < FLAGS_lane_sequence_threshold && i != all.first) {
      (*enable_lane_sequence)[i] = false;
    } else if (change.first >= 0 && change.first < num_lane_sequence &&
               lane_change_type[i] > 0 &&
               lane_change_type[i] != lane_change_type[change.first]) {
      (*enable_lane_sequence)[i] = false;
    }
  }
}

void MoveSequencePredictor::GetADC() {
  ObstaclesContainer* container = dynamic_cast<ObstaclesContainer*>(
      ContainerManager::instance()->GetContainer(
          AdapterConfig::PERCEPTION_OBSTACLES));
  if (container == nullptr) {
    AERROR << "Unavailable obstacle container";
    return;
  }

  Obstacle* adc = container->GetObstacle(PoseContainer::ID);
  if (adc != nullptr) {
    const Feature& feature = adc->latest_feature();
    if (feature.has_lane() && feature.lane().has_lane_feature()) {
      adc_lane_id_ = feature.lane().lane_feature().lane_id();
      adc_lane_s_ = feature.lane().lane_feature().lane_s();
    }
    if (feature.has_position()) {
      adc_position_[0] = feature.position().x();
      adc_position_[1] = feature.position().y();
    }
  }
}

int MoveSequencePredictor::GetLaneChangeType(
    const std::string& lane_id, const LaneSequence& lane_sequence) {
  PredictionMap* map = PredictionMap::instance();

  std::string lane_change_id = lane_sequence.lane_segment(0).lane_id();
  if (lane_id == lane_change_id) {
    return 0;
  } else {
    if (map->IsLeftNeighborLane(map->LaneById(lane_change_id),
                                map->LaneById(lane_id))) {
      return 1;
    } else if (map->IsRightNeighborLane(map->LaneById(lane_change_id),
                                        map->LaneById(lane_id))) {
      return 2;
    }
  }
  return -1;
}

double MoveSequencePredictor::GetLaneChangeDistanceWithADC(
    const LaneSequence& lane_sequence) {
  if (adc_lane_id_.empty() || lane_sequence.lane_segment_size() <= 0) {
    return std::numeric_limits<double>::max();
  }

  PredictionMap* map = PredictionMap::instance();
  std::string obstacle_lane_id = lane_sequence.lane_segment(0).lane_id();
  double obstacle_lane_s = lane_sequence.lane_segment(0).start_s();

  if (SameLaneSequence(obstacle_lane_id, obstacle_lane_s)) {
    return std::numeric_limits<double>::max();
  }

  double lane_s = 0.0;
  double lane_l = 0.0;
  if (map->GetProjection(adc_position_, map->LaneById(obstacle_lane_id),
                         &lane_s, &lane_l)) {
    return std::fabs(lane_s - obstacle_lane_s);
  }
  return std::numeric_limits<double>::max();
}

bool MoveSequencePredictor::SameLaneSequence(const std::string& lane_id,
                                             double lane_s) {
  PredictionMap* map = PredictionMap::instance();

  std::shared_ptr<const LaneInfo> obstacle_lane = map->LaneById(lane_id);
  std::shared_ptr<const LaneInfo> adc_lane = map->LaneById(adc_lane_id_);

  if (obstacle_lane != nullptr && adc_lane != nullptr) {
    RoadGraph obstacle_road_graph(lane_s, FLAGS_lane_change_dist,
                                  obstacle_lane);
    LaneGraph obstacle_lane_graph;
    obstacle_road_graph.BuildLaneGraph(&obstacle_lane_graph);

    RoadGraph adc_road_graph(adc_lane_s_, FLAGS_lane_change_dist, adc_lane);
    LaneGraph adc_lane_graph;
    adc_road_graph.BuildLaneGraph(&adc_lane_graph);

    return obstacle_road_graph.IsOnLaneGraph(adc_lane, obstacle_lane_graph) ||
           adc_road_graph.IsOnLaneGraph(obstacle_lane, adc_lane_graph);
  }

  return false;
}

std::string MoveSequencePredictor::ToString(const LaneSequence& sequence) {
  std::string str_lane_sequence = "";
  if (sequence.lane_segment_size() > 0) {
    str_lane_sequence += sequence.lane_segment(0).lane_id();
  }
  for (int i = 1; i < sequence.lane_segment_size(); ++i) {
    str_lane_sequence += ("->" + sequence.lane_segment(i).lane_id());
  }
  return str_lane_sequence;
}

}  // namespace prediction
}  // namespace apollo