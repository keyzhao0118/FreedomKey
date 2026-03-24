cmake_minimum_required(VERSION 3.20)

project(QuickExtract LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 COMPONENTS Widgets REQUIRED)
find_package(7zip CONFIG REQUIRED)

add_executable(QuickExtract WIN32
	main.cpp
)

target_link_libraries(QuickExtract
	PRIVATE
		Qt5::Widgets
		7zip::7zip
)

# 添加此命令以在构建后复制 DLL
add_custom_command(TARGET QuickExtract POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
	"$<TARGET_FILE_DIR:7zip::7zip>/../bin/7zip.dll" # 从 .lib 目录找到 .dll
	$<TARGET_FILE_DIR:QuickExtract>
	COMMENT "Copying 7z.dll to output directory"
)