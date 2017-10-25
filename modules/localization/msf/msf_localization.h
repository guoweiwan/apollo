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

/**
 * @file msf_localization.h
 * @brief The class of MSFLocalization
 */

#ifndef MODULES_LOCALIZATION_MSF_LOCALIZATION_H_
#define MODULES_LOCALIZATION_MSF_LOCALIZATION_H_

#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "ros/include/ros/ros.h"
#include "sensor_msgs/PointCloud2.h"

#include "modules/localization/proto/gps.pb.h"
#include "modules/localization/proto/imu.pb.h"
#include "modules/localization/proto/localization.pb.h"
#include "modules/localization/proto/measure.pb.h"
#include "modules/localization/proto/sins_pva.pb.h"

#include "glog/logging.h"
#include "gtest/gtest_prod.h"
#include "modules/common/monitor/monitor.h"
#include "modules/common/status/status.h"
#include "modules/localization/localization_base.h"

/**
 * @namespace apollo::localization
 * @brief apollo::localization
 */
namespace apollo {
namespace localization {

/**
 * @class MSFLocalization
 *
 * @brief generate localization info based on MSF
 */
class MSFLocalization : public LocalizationBase {
 public:
  MSFLocalization();
  virtual ~MSFLocalization() = default;

  /**
   * @brief module start function
   * @return start status
   */
  apollo::common::Status Start() override;

  /**
   * @brief module stop function
   * @return stop status
   */
  apollo::common::Status Stop() override;

 private:
  void OnTimer(const ros::TimerEvent &event);
  void OnPointCloud(const sensor_msgs::PointCloud2& message);
  void OnImu(const localization::Imu &imu_msg);
  void OnGps(const localization::Gps &gps_msg);
  // void PublishLocalization();
  // void RunWatchDog();

 private:
  ros::Timer timer_;
  apollo::common::monitor::Monitor monitor_;
  const std::vector<double> map_offset_;
  double last_received_timestamp_sec_ = 0.0;
  double last_reported_timestamp_sec_ = 0.0;
  bool service_started_ = false;

  // FRIEND_TEST(RTKLocalizationTest, InterpolateIMU);
  // FRIEND_TEST(RTKLocalizationTest, ComposeLocalizationMsg);
};

}  // namespace localization
}  // namespace apollo

#endif  // MODULES_LOCALIZATION_MSF_LOCALIZATION_H_
