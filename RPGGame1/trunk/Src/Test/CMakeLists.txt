cmake_minimum_required(VERSION 2.6)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}                    src_test)
aux_source_directory(${PROJECT_SOURCE_DIR}/Src/Common/MGHttp        src_mghttp)

add_executable (Test 
    ${src_test}
    ${src_mghttp}
)   

target_link_libraries(Test luawrapper lua530 logger pthread network readline rt)
