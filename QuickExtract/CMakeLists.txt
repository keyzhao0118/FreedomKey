cmake_minimum_required(VERSION 3.20)

project(QuickExtract LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 COMPONENTS Widgets LinguistTools REQUIRED)
find_package(7zip CONFIG REQUIRED)

add_executable(QuickExtract WIN32
	src/guiddef.cpp
	src/main.cpp

	src/mainwindow.h
	src/mainwindow.cpp

	src/archivehelper.h
	src/archivehelper.cpp

	src/extractengine.h
	src/extractengine.cpp

	src/compressengine.h
	src/compressengine.cpp

	src/instreamwrapper.h
	src/instreamwrapper.cpp
	src/outstreamwrapper.h
	src/outstreamwrapper.cpp
	src/compressoutstream.h
	src/compressoutstream.cpp

	src/opencallback.h
	src/opencallback.cpp
	src/extractcallback.h
	src/extractcallback.cpp
	src/updatecallback.h
	src/updatecallback.cpp

	src/compressoptionsdialog.h
	src/compressoptionsdialog.cpp
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

# 国际化：lupdate 扫描源码更新 .ts，lrelease 编译生成 .qm
set(TS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/translations)
set(QM_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/translations)

add_custom_target(QuickExtract_translations ALL
	COMMAND ${CMAKE_COMMAND} -E make_directory ${TS_DIR}
	COMMAND Qt5::lupdate -extensions cpp,h -no-obsolete ${CMAKE_CURRENT_SOURCE_DIR}/src -ts
		${TS_DIR}/quickextract_zh_CN.ts
		${TS_DIR}/quickextract_en_US.ts
	COMMAND ${CMAKE_COMMAND} -E make_directory ${QM_DIR}
	COMMAND Qt5::lrelease ${TS_DIR}/quickextract_zh_CN.ts -qm ${QM_DIR}/quickextract_zh_CN.qm
	COMMAND Qt5::lrelease ${TS_DIR}/quickextract_en_US.ts -qm ${QM_DIR}/quickextract_en_US.qm
	COMMENT "Updating translations and generating .qm files..."
)

add_dependencies(QuickExtract QuickExtract_translations)
