macro(add_app APP_NAME)
  if (NOT ${IS_SWITCH})
    add_executable(${APP_NAME}-tumbling-host ${APP_NAME}/host.cpp)
    target_compile_definitions(${APP_NAME}-tumbling-host PRIVATE -DTUMBLING)
    target_link_libraries(${APP_NAME}-tumbling-host LINK_PRIVATE rdma-tools dpdk-toolchain-cpp)

    add_executable(${APP_NAME}-sliding-host ${APP_NAME}/host.cpp)
    target_compile_definitions(${APP_NAME}-sliding-host PRIVATE -DSLIDING)
    target_link_libraries(${APP_NAME}-sliding-host LINK_PRIVATE rdma-tools dpdk-toolchain-cpp)
  endif()
  add_p4(${APP_NAME}-tumbling-switch ${APP_NAME}/switch.p4 "-DP4FLAGS=-DTUMBLING" ${APP_NAME}/switch.py)
  add_p4(${APP_NAME}-sliding-switch ${APP_NAME}/switch.p4 "-DP4FLAGS=-DSLIDING" ${APP_NAME}/switch.py)
endmacro(add_app APP_NAME)

add_app(new-tcp-connections)
add_app(ddos)
add_app(port-scan)
add_app(superspreader)
add_app(ssh-brute)
add_app(completed-flow)
add_app(syn-flood)
add_app(slowloris-attack)
