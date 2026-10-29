# Pixeon Engine - クロスプラットフォーム部分のビルド定義
#
# Visual Studio 用のソリューション (Pixeon_Engine.sln) がエンジン本体の主ビルドです。
# この CMake は OS に依存しない共通ライブラリ・ツール・テストと、
# RHI (Vulkan バックエンド) の検証用ビルドを提供します。
#
#   cmake -S . -B build -G Ninja
#   cmake --build build
#   ctest --test-dir build --output-on-failure
cmake_minimum_required(VERSION 3.20)
project(PixeonEngineTools LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
  add_compile_options(/utf-8 /W3)
else()
  add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

# ---- 共通ライブラリ -------------------------------------------------------
add_library(PixCommon STATIC
  Common/AssetType.cpp
  Common/FileUtil.cpp
  Common/GameManifest.cpp
  Common/HashUtil.cpp
  Common/PixArchive.cpp
  Common/VirtualFileSystem.cpp
)
target_include_directories(PixCommon PUBLIC Common Pixeon_Engine)
find_package(Threads REQUIRED)
target_link_libraries(PixCommon PUBLIC Threads::Threads)

# ---- アセットパッキングツール ---------------------------------------------
add_executable(PixAssetPacker "Asset packaging tool/main.cpp")
target_link_libraries(PixAssetPacker PRIVATE PixCommon)

# ---- テスト ---------------------------------------------------------------
enable_testing()
add_executable(PixCommonTests Tests/CommonTests.cpp)
target_link_libraries(PixCommonTests PRIVATE PixCommon)
add_test(NAME PixCommonTests COMMAND PixCommonTests WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
