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
  Common/GameBuilder.cpp
  Common/GameManifest.cpp
  Common/HashUtil.cpp
  Common/PixArchive.cpp
  Common/ShaderCompiler.cpp
  Common/VirtualFileSystem.cpp
)
target_include_directories(PixCommon PUBLIC Common Pixeon_Engine)
find_package(Threads REQUIRED)
target_link_libraries(PixCommon PUBLIC Threads::Threads)

# ---- アセットパッキングツール ---------------------------------------------
add_executable(PixAssetPacker "Asset packaging tool/main.cpp")
target_link_libraries(PixAssetPacker PRIVATE PixCommon)

# ---- コマンドラインビルドツール ------------------------------------------
add_executable(PixeonBuild BuildExe/main.cpp)
target_link_libraries(PixeonBuild PRIVATE PixCommon)

# ---- テスト ---------------------------------------------------------------
enable_testing()
add_executable(PixCommonTests Tests/CommonTests.cpp)
target_link_libraries(PixCommonTests PRIVATE PixCommon)
add_test(NAME PixCommonTests COMMAND PixCommonTests WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# ---- RHI Vulkan バックエンドの検証 (Linux / Windows) ----------------------
# GPU が無い環境でも Mesa の lavapipe で実行できる
find_package(Vulkan QUIET)
if(Vulkan_FOUND)
  add_library(PixRHIVulkan STATIC
    Pixeon_Engine/RHI/RHICommon.cpp
    Pixeon_Engine/RHI/RHIImGui.cpp
    Pixeon_Engine/RHI/Vulkan/SpirvReflect.cpp
    Pixeon_Engine/RHI/Vulkan/VulkanDevice.cpp
    Pixeon_Engine/IMGUI/imgui.cpp
    Pixeon_Engine/IMGUI/imgui_draw.cpp
    Pixeon_Engine/IMGUI/imgui_tables.cpp
    Pixeon_Engine/IMGUI/imgui_widgets.cpp
  )
  target_compile_definitions(PixRHIVulkan PUBLIC PIXEON_RHI_VULKAN)
  target_include_directories(PixRHIVulkan PUBLIC Pixeon_Engine)
  target_link_libraries(PixRHIVulkan PUBLIC Vulkan::Vulkan)
  if(NOT MSVC)
    target_compile_options(PixRHIVulkan PRIVATE -Wno-missing-field-initializers)
  endif()

  add_executable(PixRHIVulkanTests Tests/RHIVulkanTests.cpp)
  target_link_libraries(PixRHIVulkanTests PRIVATE PixRHIVulkan)
  target_compile_definitions(PixRHIVulkanTests PRIVATE
    PIXEON_SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Exe/SceneRoot/Shader/spv")
  add_test(NAME PixRHIVulkanTests COMMAND PixRHIVulkanTests)
endif()
