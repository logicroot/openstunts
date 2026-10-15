add_executable(s3d_tests
  test_main.cpp
  trk_test.cpp
  car_test.cpp
  fixed_math_test.cpp
  fixed_timestep_test.cpp
  compression_test.cpp
  shape3d_test.cpp
  shape_mesh_test.cpp
  palette_test.cpp
  track_objects_test.cpp
  track_shapes_test.cpp
  track_route_test.cpp
  material_test.cpp
  car_mesh_test.cpp
  car_engine_test.cpp
  car_steering_test.cpp
  car_motion_test.cpp
  car_collision_test.cpp
  opponent_test.cpp
  opponent_ai_test.cpp
  car_debris_test.cpp
  track_collision_test.cpp
  ramp_jump_test.cpp
  font_test.cpp
  sprite2d_test.cpp
  crash_overlay_test.cpp
  race_state_test.cpp
  camera_test.cpp
  skybox_test.cpp
  dashboard_test.cpp
  menu_test.cpp
  block_texture_test.cpp
  frontend_opponent_test.cpp
)
target_link_libraries(s3d_tests PRIVATE stunts3d_core)
target_include_directories(s3d_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_test(NAME s3d_tests COMMAND s3d_tests)
