project(msf_sensors)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules/")

find_package(YamlCpp REQUIRED)
message(STATUS "yaml-cpp Found: " ${YAMLCPP_FOUND})
include_directories(${YAMLCPP_INCLUDE_DIR})
link_directories(${YAMLCPP_LIBRARY_DIRS})

find_package(PCL COMPONENTS io REQUIRED)
include_directories(${PCL_INCLUDE_DIRS})
link_directories(${PCL_LIBRARY_DIRS})
# add_definitions(${PCL_DEFINITIONS})

add_subdirectory(velodyne)

add_library(${PROJECT_NAME} ${APOLLO_BUILD_TYPE} $<TARGET_OBJECTS:velodyne>)
target_link_libraries(${PROJECT_NAME} ${PCL_LIBRARIES} ${Boost_LIBRARIES} ${YAMLCPP_LIBRARY})

install(TARGETS ${PROJECT_NAME} DESTINATION lib)
