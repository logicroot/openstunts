add_executable(s3d_tests
  test_main.cpp
  trk_test.cpp
  car_test.cpp
  fixed_math_test.cpp
  compression_test.cpp
  shape3d_test.cpp
  shape_mesh_test.cpp
  palette_test.cpp
  track_objects_test.cpp
  track_shapes_test.cpp
  car_mesh_test.cpp
  car_engine_test.cpp
  car_steering_test.cpp
  car_motion_test.cpp
  track_collision_test.cpp
)
target_link_libraries(s3d_tests PRIVATE stunts3d_core)
target_include_directories(s3d_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_test(NAME s3d_tests COMMAND s3d_tests)
