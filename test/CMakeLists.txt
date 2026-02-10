find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(TINIX_TEST_RUNNER "${CMAKE_CURRENT_LIST_DIR}/run_tinix_tests.py")

add_test(
  NAME tinix_shell_help
  COMMAND "${Python3_EXECUTABLE}" "${TINIX_TEST_RUNNER}"
          --exe "$<TARGET_FILE:tinix>"
          --repo "${CMAKE_SOURCE_DIR}"
          --case shell_help
)

add_test(
  NAME tinix_fs_persistence
  COMMAND "${Python3_EXECUTABLE}" "${TINIX_TEST_RUNNER}"
          --exe "$<TARGET_FILE:tinix>"
          --repo "${CMAKE_SOURCE_DIR}"
          --case fs_persistence
)

add_test(
  NAME tinix_fs_paths
  COMMAND "${Python3_EXECUTABLE}" "${TINIX_TEST_RUNNER}"
          --exe "$<TARGET_FILE:tinix>"
          --repo "${CMAKE_SOURCE_DIR}"
          --case fs_paths
)

add_test(
  NAME tinix_fs_rm_recreate
  COMMAND "${Python3_EXECUTABLE}" "${TINIX_TEST_RUNNER}"
          --exe "$<TARGET_FILE:tinix>"
          --repo "${CMAKE_SOURCE_DIR}"
          --case fs_rm_recreate
)

add_test(
  NAME tinix_shell_script
  COMMAND "${Python3_EXECUTABLE}" "${TINIX_TEST_RUNNER}"
          --exe "$<TARGET_FILE:tinix>"
          --repo "${CMAKE_SOURCE_DIR}"
          --case shell_script
)

add_test(
  NAME tinix_complex_fs_swap_roundtrip
  COMMAND "${Python3_EXECUTABLE}" "${TINIX_TEST_RUNNER}"
          --exe "$<TARGET_FILE:tinix>"
          --repo "${CMAKE_SOURCE_DIR}"
          --case complex_fs_swap_roundtrip
)

add_test(
  NAME tinix_complex_two_proc_cross_evict
  COMMAND "${Python3_EXECUTABLE}" "${TINIX_TEST_RUNNER}"
          --exe "$<TARGET_FILE:tinix>"
          --repo "${CMAKE_SOURCE_DIR}"
          --case complex_two_proc_cross_evict
)

add_test(
  NAME tinix_device_queue_wakeup
  COMMAND "${Python3_EXECUTABLE}" "${TINIX_TEST_RUNNER}"
          --exe "$<TARGET_FILE:tinix>"
          --repo "${CMAKE_SOURCE_DIR}"
          --case device_queue_wakeup
)

add_test(
  NAME tinix_swap_partition
  COMMAND "${Python3_EXECUTABLE}" "${TINIX_TEST_RUNNER}"
          --exe "$<TARGET_FILE:tinix>"
          --repo "${CMAKE_SOURCE_DIR}"
          --case swap_partition
)

add_test(
  NAME tinix_kernel_reformat_mismatch
  COMMAND "${Python3_EXECUTABLE}" "${TINIX_TEST_RUNNER}"
          --exe "$<TARGET_FILE:tinix>"
          --repo "${CMAKE_SOURCE_DIR}"
          --case kernel_reformat_mismatch
)

add_test(
  NAME tinix_fs_superblock_accounting
  COMMAND "${Python3_EXECUTABLE}" "${TINIX_TEST_RUNNER}"
          --exe "$<TARGET_FILE:tinix>"
          --repo "${CMAKE_SOURCE_DIR}"
          --case fs_superblock_accounting
)

add_test(
  NAME tinix_pc_file_ops_explicit_fd
  COMMAND "${Python3_EXECUTABLE}" "${TINIX_TEST_RUNNER}"
          --exe "$<TARGET_FILE:tinix>"
          --repo "${CMAKE_SOURCE_DIR}"
          --case pc_file_ops_explicit_fd
)

add_test(
  NAME tinix_pc_file_ops_auto_fd_cleanup
  COMMAND "${Python3_EXECUTABLE}" "${TINIX_TEST_RUNNER}"
          --exe "$<TARGET_FILE:tinix>"
          --repo "${CMAKE_SOURCE_DIR}"
          --case pc_file_ops_auto_fd_cleanup
)

add_test(
  NAME tinix_pc_file_ops_invalid_fd
  COMMAND "${Python3_EXECUTABLE}" "${TINIX_TEST_RUNNER}"
          --exe "$<TARGET_FILE:tinix>"
          --repo "${CMAKE_SOURCE_DIR}"
          --case pc_file_ops_invalid_fd
)

set_tests_properties(
  tinix_shell_help
  tinix_fs_persistence
  tinix_fs_paths
  tinix_fs_rm_recreate
  tinix_shell_script
  tinix_complex_fs_swap_roundtrip
  tinix_complex_two_proc_cross_evict
  tinix_device_queue_wakeup
  tinix_swap_partition
  tinix_kernel_reformat_mismatch
  tinix_fs_superblock_accounting
  tinix_pc_file_ops_explicit_fd
  tinix_pc_file_ops_auto_fd_cleanup
  tinix_pc_file_ops_invalid_fd
  PROPERTIES TIMEOUT 20
)
