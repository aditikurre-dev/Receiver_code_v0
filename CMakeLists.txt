cmake_minimum_required(VERSION 3.19)
project(ReceiverPayload-V0 LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Network LinguistTools)

qt_standard_project_setup()

qt_add_executable(ReceiverPayload-V0
    main.cpp
)

qt_add_translations(
    TARGETS ReceiverPayload-V0
    TS_FILES ReceiverPayload-V0_en_IN.ts
)

target_link_libraries(ReceiverPayload-V0
    PRIVATE
        Qt::Core
        Qt6::Network
)

install(TARGETS ReceiverPayload-V0
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
