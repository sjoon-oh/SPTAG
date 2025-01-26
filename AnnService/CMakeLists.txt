# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

set(AnnService ${PROJECT_SOURCE_DIR}/AnnService)
set(Zstd ${PROJECT_SOURCE_DIR}/ThirdParty/zstd)

# Add the gentle-pekin-duck
set(GentlePekinDuck ${PROJECT_SOURCE_DIR}/ThirdParty/gentle-pekin-duck)
set(TopKache ${PROJECT_SOURCE_DIR}/ThirdParty/TopKache)

include_directories(${AnnService})
include_directories(${Zstd}/lib)

# Add the gentle-pekin-duck
include_directories(${GentlePekinDuck}/inc)
include_directories(${GentlePekinDuck}/extern/spdlog/include)
include_directories(${GentlePekinDuck}/extern)

# Add the TopKache
include_directories(${TopKache}/inc)
include_directories(${TopKache}/extern/spdlog/include)
include_directories(${TopKache}/extern)

# add_library(libzstd_shared SHARED ${Sources} ${Headers} ${PlatformDependResources})
file(GLOB_RECURSE PDUCK_HDR
    ${GentlePekinDuck}/inc/*.hh
    )
file(GLOB_RECURSE PDUCK_SRC
    ${GentlePekinDuck}/src/*.cc
    )

file(GLOB_RECURSE TOPKACHE_HDR
    ${TopKache}/inc/*.hh
    )
file(GLOB_RECURSE TOPKACHE_SRC
    ${TopKache}/src/*.cc
    )

# Slight add
file(GLOB_RECURSE HDR_FILES 
    ${AnnService}/inc/Core/*.h  
    ${AnnService}/inc/Helper/*.h 
    ${AnnService}/inc/Extension/*.hh
    )
file(GLOB_RECURSE SRC_FILES 
    ${AnnService}/src/Core/*.cpp 
    ${AnnService}/src/Helper/*.cpp
    ${AnnService}/src/Extension/*.cc
    )

list(REMOVE_ITEM HDR_FILES
    ${AnnService}/inc/Core/Common/DistanceUtils.h
    ${AnnService}/inc/Core/Common/SIMDUtils.h 
    ${AnnService}/inc/Core/Common/InstructionUtils.h
    ${AnnService}/inc/Core/Common/CommonUtils.h
    )

list(REMOVE_ITEM SRC_FILES
    ${AnnService}/src/Core/Common/DistanceUtils.cpp
    ${AnnService}/src/Core/Common/SIMDUtils.cpp
    ${AnnService}/src/Core/Common/InstructionUtils.cpp
    )

add_library (DistanceUtils STATIC 
    inc/Core/Common/DistanceUtils.h
    inc/Core/Common/SIMDUtils.h
    inc/Core/Common/InstructionUtils.h
    inc/Core/Common/CommonUtils.h
    src/Core/Common/DistanceUtils.cpp
    src/Core/Common/SIMDUtils.cpp
    src/Core/Common/InstructionUtils.cpp
    )

if(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
    target_compile_options(DistanceUtils PRIVATE -mavx2 -mavx -msse -msse2 -mavx512f -mavx512bw -mavx512dq -fPIC)
endif()

add_library (SPTAGLib SHARED ${SRC_FILES} ${HDR_FILES} ${PDUCK_HDR} ${PDUCK_SRC} ${TOPKACHE_HDR} ${TOPKACHE_SRC})
# link_directories(SPTAGLib ${GentlePekinDuck}/build/lib) 
target_link_libraries (SPTAGLib DistanceUtils libzstd_shared ${NUMA_LIBRARY} ${Boost_LIBRARIES})

add_library (SPTAGLibStatic STATIC ${SRC_FILES} ${HDR_FILES} ${PDUCK_HDR} ${PDUCK_SRC} ${TOPKACHE_HDR} ${TOPKACHE_SRC})
target_link_libraries (SPTAGLibStatic DistanceUtils libzstd_static ${NUMA_LIBRARY_STATIC} ${Boost_LIBRARIES} 
    ${GentlePekinDuck}/build/lib ${TopKache}/build/lib
)
if(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
    target_compile_options(SPTAGLibStatic PRIVATE -fPIC
        -lduck -ltopkache
    )
endif()

install(TARGETS SPTAGLib SPTAGLibStatic
  RUNTIME DESTINATION bin
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib)

# Disable the options for additionals
#  : For blanc
# 
# if (NOT LIBRARYONLY)
#     file(GLOB SERVER_HDR_FILES ${AnnService}/inc/Server/*.h ${AnnService}/inc/Socket/*.h)
#     file(GLOB SERVER_FILES ${AnnService}/src/Server/*.cpp ${AnnService}/src/Socket/*.cpp)
#     add_executable (server ${SERVER_FILES} ${SERVER_HDR_FILES})
#     target_link_libraries(server ${Boost_LIBRARIES} SPTAGLibStatic)

#     file(GLOB CLIENT_HDR_FILES ${AnnService}/inc/Client/*.h ${AnnService}/inc/Socket/*.h)
#     file(GLOB CLIENT_FILES ${AnnService}/src/Client/*.cpp ${AnnService}/src/Socket/*.cpp)
#     add_executable (client ${CLIENT_FILES} ${CLIENT_HDR_FILES})
#     target_link_libraries(client ${Boost_LIBRARIES} SPTAGLibStatic)

#     file(GLOB AGG_HDR_FILES ${AnnService}/inc/Aggregator/*.h ${AnnService}/inc/Socket/*.h ${AnnService}/inc/Server/QueryParser.h)
#     file(GLOB AGG_FILES ${AnnService}/src/Aggregator/*.cpp ${AnnService}/src/Socket/*.cpp ${AnnService}/src/Server/QueryParser.cpp)
#     add_executable (aggregator ${AGG_FILES} ${AGG_HDR_FILES})
#     target_link_libraries(aggregator ${Boost_LIBRARIES} SPTAGLibStatic)

#     file(GLOB BUILDER_FILES ${AnnService}/src/IndexBuilder/*.cpp)
#     add_executable (indexbuilder ${BUILDER_FILES})
#     target_link_libraries(indexbuilder ${Boost_LIBRARIES} SPTAGLibStatic)

#     file(GLOB SEARCHER_FILES ${AnnService}/src/IndexSearcher/*.cpp)
#     add_executable (indexsearcher ${SEARCHER_FILES})
#     target_link_libraries(indexsearcher ${Boost_LIBRARIES} SPTAGLibStatic)
    
#     file(GLOB QUANTIZER_HDR_FILES ${AnnService}/inc/Quantizer/*.h)
#     file(GLOB QUANTIZER_FILES ${AnnService}/src/Quantizer/*.cpp)
#     add_executable (quantizer ${QUANTIZER_FILES} ${QUANTIZER_HDR_FILES})
#     target_link_libraries(quantizer ${Boost_LIBRARIES} SPTAGLibStatic)

#     install(TARGETS server client aggregator indexbuilder indexsearcher quantizer
#       RUNTIME DESTINATION bin
#       ARCHIVE DESTINATION lib
#       LIBRARY DESTINATION lib)
# endif()

file(GLOB_RECURSE SSD_SERVING_HDR_FILES ${AnnService}/inc/SSDServing/*.h)
file(GLOB_RECURSE SSD_SERVING_FILES ${AnnService}/src/SSDServing/*.cpp)

add_executable(ssdserving ${SSD_SERVING_HDR_FILES} ${SSD_SERVING_FILES})
target_link_libraries(ssdserving SPTAGLibStatic ${Boost_LIBRARIES})
target_compile_definitions(ssdserving PRIVATE _exe)

# for Test
add_library(ssdservingLib ${SSD_SERVING_HDR_FILES} ${SSD_SERVING_FILES})
target_link_libraries(ssdservingLib SPTAGLibStatic ${Boost_LIBRARIES})

find_package(MPI)
if (MPI_FOUND)
    message (STATUS "Found MPI.")
    message (STATUS "MPI Include Path: ${MPI_CXX_INCLUDE_PATH}")
    message (STATUS "MPI Libraries: ${MPI_CXX_LIBRARIES}")
    include_directories (${MPI_CXX_INCLUDE_PATH})
    file(GLOB PARTITION_FILES ${AnnService}/src/BalancedDataPartition/*.cpp)
    add_executable(balanceddatapartition ${PARTITION_FILES})
    target_link_libraries(balanceddatapartition SPTAGLibStatic ${Boost_LIBRARIES} ${MPI_CXX_LIBRARIES})
endif()

install(TARGETS ssdservingLib ssdserving
  RUNTIME DESTINATION bin
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib)
