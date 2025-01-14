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

#include "modules/common/adapters/adapter_gflags.h"

DEFINE_bool(enable_adapter_dump, false,
            "Whether enable dumping the messages to "
            "/tmp/adapters/<topic_name>/<seq_num>.txt for debugging purposes.");
DEFINE_string(gps_topic, "/apollo/sensor/gnss/odometry", "GPS topic name");
DEFINE_string(imu_topic, "/apollo/sensor/gnss/corrected_imu", "IMU topic name");
DEFINE_string(chassis_topic, "/apollo/canbus/chassis", "chassis topic name");
DEFINE_string(chassis_detail_topic, "/apollo/canbus/chassis_detail",
              "chassis detail topic name");
DEFINE_string(localization_topic, "/apollo/localization/pose",
              "localization topic name");
DEFINE_string(planning_trajectory_topic, "/apollo/planning",
              "planning trajectory topic name");
DEFINE_string(monitor_topic, "/apollo/monitor", "ROS topic for monitor");
DEFINE_string(pad_topic, "/apollo/control/pad",
              "control pad message topic name");
DEFINE_string(control_command_topic, "/apollo/control",
              "control command topic name");
DEFINE_string(pointcloud_topic, "/apollo/compensator/pointcloud",
              "pointcloud topic name");
DEFINE_string(prediction_topic, "/apollo/prediction", "prediction topic name");
DEFINE_string(perception_obstacle_topic, "/apollo/perception/obstacles",
              "perception obstacle topic name");
DEFINE_string(traffic_light_detection_topic, "/apollo/perception/traffic_light",
              "traffic light detection topic name");
DEFINE_string(routing_request_topic, "/apollo/routing_request",
              "routing request topic name");
DEFINE_string(routing_response_topic, "/apollo/routing_response",
              "routing response topic name");
DEFINE_string(relative_odometry_topic, "/apollo/calibration/relative_odometry",
              "relative odometry topic name");
DEFINE_string(ins_stat_topic, "/apollo/sensor/gnss/ins_stat",
              "ins stat topic name");
DEFINE_string(hmi_command_topic, "/apollo/hmi_command",
              "HMI command topic name");
DEFINE_string(hmi_status_topic, "/apollo/hmi_status", "HMI status topic name");
DEFINE_string(mobileye_topic, "/apollo/mobileye", "mobileye topic name");
DEFINE_string(delphi_esr_topic, "/apollo/delphi_esr",
              "delphi esr radar topic name");
// TODO(Authors): Change the topic name
DEFINE_string(compressed_image_topic, "camera/image_raw",
              "CompressedImage topic name");
DEFINE_string(gnss_rtk_obs_topic, "/apollo/sensor/gnss/rtk_obs",
              "Gnss rtk observation topic name");
DEFINE_string(gnss_rtk_eph_topic, "/apollo/sensor/gnss/rtk_eph",
              "Gnss rtk ephemeris topic name");
DEFINE_string(gnss_best_pose_topic, "/apollo/sensor/gnss/best_pose",
              "Gnss rtk best gnss pose");
DEFINE_string(localization_measure_gnss_topic,
              "/apollo/localization/measure_gnss",
              "Gnss localization measurement topic name");
DEFINE_string(localization_measure_lidar_topic,
              "/apollo/localization/measure_lidar",
              "Lidar localization measurement topic name");
DEFINE_string(localization_sins_pva_topic, "/apollo/localization/sins_pva",
              "Localization sins pva topic name");
