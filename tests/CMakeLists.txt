add_executable(s3d_tests
  test_main.cpp
  trk_test.cpp
  car_test.cpp
  fixed_math_test.cpp
)
target_link_libraries(s3d_tests PRIVATE stunts3d_core)
target_include_directories(s3d_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_test(NAME s3d_tests COMMAND s3d_tests)
