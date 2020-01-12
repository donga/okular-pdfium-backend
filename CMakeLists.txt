project(okular-backend-pdfium)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules ${CMAKE_MODULE_PATH})
cmake_minimum_required(VERSION 2.8.12)

find_package(PkgConfig REQUIRED)

find_package(ECM 5.19.0 CONFIG REQUIRED)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH})
include(FeatureSummary)
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDEFrameworkCompilerSettings NO_POLICY_SCOPE)

include_directories(${PDFIUM_INCLUDE_DIR})

find_package(Okular5 REQUIRED)
find_package(KF5 REQUIRED COMPONENTS
    CoreAddons
    I18n
)

set(okularGenerator_pdfium_SRCS
    pdfium_utils.cpp
    document.cpp
    page.cpp
    generator_pdfium.cpp
)

kcoreaddons_add_plugin(okularGenerator_pdfium
    JSON "libokularGenerator_pdfium.json"
    INSTALL_NAMESPACE "okular/generators"
    SOURCES ${okularGenerator_pdfium_SRCS}
)

target_link_libraries(okularGenerator_pdfium
    Okular::Core
    KF5::I18n
    Qt5::Widgets
    Qt5::Xml
    pdfium
)

install( FILES okularPDFium.desktop  DESTINATION  ${KDE_INSTALL_KSERVICES5DIR} )
install( FILES org.kde.okular-pdfium.metainfo.xml DESTINATION ${KDE_INSTALL_METAINFODIR} )
