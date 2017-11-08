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
 * @file localization_gflags.h
 * @brief The gflags used by localization module
 */

#ifndef MODULES_LOCALIZATION_COMMON_LOCALIZATION_GFLAGS_H_
#define MODULES_LOCALIZATION_COMMON_LOCALIZATION_GFLAGS_H_

#include <string>
#include "gflags/gflags.h"
#include "modules/common/util/file.h"
#include "modules/common/util/string_util.h"

DECLARE_string(localization_module_name);

DECLARE_double(localization_publish_freq);

DECLARE_string(localization_config_file);
DECLARE_string(rtk_adapter_config_file);
DECLARE_string(msf_adapter_config_file);

DECLARE_bool(enable_gps_imu_interprolate);
DECLARE_bool(enable_map_reference_unify);
DECLARE_bool(enable_watchdog);

DECLARE_double(gps_time_delay_tolerance);
DECLARE_double(gps_imu_timestamp_sec_diff_tolerance);
DECLARE_double(timestamp_sec_tolerance);

DECLARE_double(map_offset_x);
DECLARE_double(map_offset_y);
DECLARE_double(map_offset_z);

DECLARE_int32(report_threshold_err_num);
DECLARE_double(report_gps_imu_time_diff_threshold);

DECLARE_bool(enable_gps_timestamp);

// lidar module
DECLARE_string(local_map_path);
DECLARE_string(lidar_extrinsic_file);
DECLARE_string(lidar_height_file);
DECLARE_int32(lidar_localization_mode);
DECLARE_double(lidar_imu_max_delay_time);
DECLARE_double(lidar_map_coverage_theshold);
DECLARE_bool(lidar_debug_log_flag);

// integ module
DECLARE_bool(integ_ins_can_self_align);
DECLARE_bool(integ_sins_align_with_vel);
DECLARE_bool(using_raw_gnsspos);
DECLARE_double(vel_threshold_get_yaw);
DECLARE_bool(integ_debug_log_flag);
DECLARE_string(extrinsic_imu_gnss_filename);

// gnss module
DECLARE_bool(enable_ins_aid_rtk);
DECLARE_bool(enable_auto_save_eph_file);
DECLARE_string(eph_buffer_path);

DECLARE_double(imu_rate);
DECLARE_int32(local_utm_zone_id);
DECLARE_bool(trans_gpstime_to_utctime);
DECLARE_int32(gnss_mode);

namespace apollo {
namespace localization {

std::string RosRoot();
std::string TranslatePath(const std::string &src_path);

}  // namespace localization
}  // namespace apollo

#endif  // MODULES_LOCALIZATION_COMMON_LOCALIZATION_GFLAGS_H_
