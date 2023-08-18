find_package(PkgConfig REQUIRED)
pkg_search_module(DPDK REQUIRED libdpdk)

add_library(dpdk-toolchain-cpp dpdk_toolchain_cpp.cpp)
target_link_libraries(dpdk-toolchain-cpp PUBLIC ${DPDK_LIBRARIES})

add_library(rdma-tools RDMA.cpp)
target_link_libraries(rdma-tools PUBLIC ibverbs)
