// Copyright (c) 2023 Boston Dynamics AI Institute LLC. All rights reserved.

#pragma once

#include <spot_driver_cpp/api/spot_api.hpp>

#include <bosdyn/client/robot/robot.h>
#include <bosdyn/client/sdk/client_sdk.h>
#include <spot_driver_cpp/api/time_sync_api.hpp>

#include <memory>
#include <string>

namespace spot_ros2 {

class DefaultSpotApi : public SpotApi {
 public:
  explicit DefaultSpotApi(const std::string& sdk_client_name);

  tl::expected<void, std::string> createRobot(const std::string& ip_address, const std::string& robot_name) override;
  tl::expected<void, std::string> authenticate(const std::string& username, const std::string& password) override;
  tl::expected<bool, std::string> hasArm() const override;
  std::shared_ptr<ImageClientInterface> image_client_interface() const override;

 private:
  std::unique_ptr<::bosdyn::client::ClientSdk> client_sdk_;
  std::unique_ptr<::bosdyn::client::Robot> robot_;
  std::shared_ptr<ImageClientInterface> image_client_interface_;
  std::shared_ptr<TimeSyncApi> time_sync_api_;
  std::string robot_name_;
};
}  // namespace spot_ros2
