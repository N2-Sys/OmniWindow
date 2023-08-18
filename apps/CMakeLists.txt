if (NOT ${IS_SWITCH})
  add_executable(new-tcp-connections-tumbling-host new-tcp-connections/host.cpp)
  target_compile_definitions(new-tcp-connections-tumbling-host PRIVATE -DTUMBLING)
  target_link_libraries(new-tcp-connections-tumbling-host LINK_PRIVATE rdma-tools dpdk-toolchain-cpp)

  add_executable(new-tcp-connections-sliding-host new-tcp-connections/host.cpp)
  target_compile_definitions(new-tcp-connections-sliding-host PRIVATE -DSLIDING)
  target_link_libraries(new-tcp-connections-sliding-host LINK_PRIVATE rdma-tools dpdk-toolchain-cpp)
endif()

add_p4(new-tcp-connections-tumbling-switch new-tcp-connections/switch.p4 "-DP4FLAGS=-DTUMBLING" new-tcp-connections/switch.py)
add_p4(new-tcp-connections-sliding-switch new-tcp-connections/switch.p4 "-DP4FLAGS=-DSLIDING" new-tcp-connections/switch.py)
