// Copyright (c) 2023 Boston Dynamics AI Institute LLC. All rights reserved.

#include <spot_driver_cpp/images/spot_image_publisher.hpp>

#include <rmw/qos_profiles.h>
#include <rclcpp/node.hpp>
#include <sensor_msgs/msg/camera_info.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <spot_driver_cpp/api/default_image_client.hpp>
#include <spot_driver_cpp/api/spot_image_sources.hpp>
#include <spot_driver_cpp/images/images_middleware_handle.hpp>
#include <spot_driver_cpp/interfaces/rclcpp_logger_interface.hpp>
#include <spot_driver_cpp/interfaces/rclcpp_parameter_interface.hpp>
#include <spot_driver_cpp/interfaces/rclcpp_tf_interface.hpp>
#include <spot_driver_cpp/interfaces/rclcpp_wall_timer_interface.hpp>
#include <spot_driver_cpp/types.hpp>

#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>

namespace {
constexpr auto kImageCallbackPeriod = std::chrono::duration<double>{1.0 / 15.0};  // 15 Hz
constexpr auto kDefaultDepthImageQuality = 100.0;
}  // namespace

namespace spot_ros2::images {
::bosdyn::api::GetImageRequest createImageRequest(const std::set<ImageSource>& sources,
                                                  [[maybe_unused]] const bool has_rgb_cameras,
                                                  const double rgb_image_quality, const bool get_raw_rgb_images) {
  ::bosdyn::api::GetImageRequest request_message;

  for (const auto& source : sources) {
    const auto source_name = toSpotImageSourceName(source);

    if (source.type == SpotImageType::RGB) {
      bosdyn::api::ImageRequest* image_request = request_message.add_image_requests();
      image_request->set_image_source_name(source_name);
      // RGB images can have a user-configurable image quality setting.
      image_request->set_quality_percent(rgb_image_quality);
      image_request->set_pixel_format(bosdyn::api::Image_PixelFormat_PIXEL_FORMAT_RGB_U8);
      // RGB images can be either raw or JPEG-compressed.
      image_request->set_image_format(get_raw_rgb_images ? bosdyn::api::Image_Format_FORMAT_RAW
                                                         : bosdyn::api::Image_Format_FORMAT_JPEG);
    } else if (source.type == SpotImageType::DEPTH) {
      bosdyn::api::ImageRequest* image_request = request_message.add_image_requests();
      image_request->set_image_source_name(source_name);
      image_request->set_quality_percent(kDefaultDepthImageQuality);
      image_request->set_image_format(bosdyn::api::Image_Format_FORMAT_RAW);
    } else {
      // SpotImageType::DEPTH_REGISTERED
      bosdyn::api::ImageRequest* image_request = request_message.add_image_requests();
      image_request->set_image_source_name(source_name);
      image_request->set_quality_percent(kDefaultDepthImageQuality);
      image_request->set_image_format(bosdyn::api::Image_Format_FORMAT_RAW);
    }
  }

  return request_message;
}

SpotImagePublisher::SpotImagePublisher(std::shared_ptr<ImageClientInterface> image_client_interface,
                                       std::unique_ptr<MiddlewareHandle> middleware_handle, bool has_arm)
    : image_client_interface_{image_client_interface},
      middleware_handle_{std::move(middleware_handle)},
      has_arm_{has_arm} {}

bool SpotImagePublisher::initialize() {
  // These parameters all fall back to default values if the user did not set them at runtime
  const auto rgb_image_quality = middleware_handle_->parameter_interface()->getRGBImageQuality();
  const auto publish_rgb_images = middleware_handle_->parameter_interface()->getPublishRGBImages();
  const auto publish_depth_images = middleware_handle_->parameter_interface()->getPublishDepthImages();
  const auto publish_depth_registered_images =
      middleware_handle_->parameter_interface()->getPublishDepthRegisteredImages();
  const auto has_rgb_cameras = middleware_handle_->parameter_interface()->getHasRGBCameras();

  // Generate the set of image sources based on which cameras the user has requested that we publish
  const auto sources =
      createImageSources(publish_rgb_images, publish_depth_images, publish_depth_registered_images, has_arm_);

  // Generate the image request message to capture the data from the specified image sources
  image_request_message_ = createImageRequest(sources, has_rgb_cameras, rgb_image_quality, false);

  // Create a publisher for each image source
  middleware_handle_->createPublishers(sources);

  // Create a timer to request and publish images at a fixed rate
  middleware_handle_->timer_interface()->setTimer(kImageCallbackPeriod, [this]() {
    timerCallback();
  });

  return true;
}

void SpotImagePublisher::timerCallback() {
  if (!image_request_message_) {
    middleware_handle_->logger_interface()->logError("No image request message generated. Returning.");
    return;
  }

  const auto image_result = image_client_interface_->getImages(*image_request_message_);
  if (!image_result.has_value()) {
    middleware_handle_->logger_interface()->logError(
        std::string{"Failed to get images: "}.append(image_result.error()));
    return;
  }

  middleware_handle_->publishImages(image_result.value().images_);

  middleware_handle_->tf_interface()->updateStaticTransforms(image_result.value().transforms_);
}
}  // namespace spot_ros2::images
