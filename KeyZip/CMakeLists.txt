cmake_minimum_required(VERSION 3.20)

project(KeyZip LANGUAGES CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PROJECT_NAME}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PROJECT_NAME}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PROJECT_NAME}/bin)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 COMPONENTS Widgets LinguistTools REQUIRED)
find_package(7zip CONFIG REQUIRED)

add_executable(KeyZip WIN32
	resources/resource.qrc

	src/guiddef.cpp
	src/main.cpp
	src/keyzipwindow.h
	src/keyzipwindow.cpp

	src/commonhelper.h
	src/commonhelper.cpp

	src/archiveparser.h
	src/archiveparser.cpp
	src/archiveextractor.h
	src/archiveextractor.cpp

	src/archivetreewidget.h
	src/archivetreewidget.cpp
	src/archivetreewidgetitem.h
	src/archivetreewidgetitem.cpp
	src/archivetree.h
	src/archivetree.cpp
	
	src/instreamwrapper.h
	src/instreamwrapper.cpp
	src/outstreamwrapper.h
	src/outstreamwrapper.cpp
	src/archiveopencallback.h
	src/archiveopencallback.cpp
	src/archiveextractcallback.cpp
	src/archiveextractcallback.h

	src/writebufferqueue.h
	src/writebufferqueue.cpp
	src/asyncoutstreamwrapper.h
	src/asyncoutstreamwrapper.cpp
	src/filewriterthread.h
	src/filewriterthread.cpp
	src/extractworker.h
	src/extractworker.cpp
)

target_link_libraries(KeyZip
	PRIVATE
		Qt5::Widgets
		7zip::7zip
)

# 国际化：lupdate 扫描源码更新 .ts，lrelease 编译生成 .qm
set(TS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/translations)
set(QM_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/translations)

add_custom_target(KeyZip_translations ALL
	COMMAND ${CMAKE_COMMAND} -E make_directory ${TS_DIR}
	COMMAND Qt5::lupdate -extensions cpp,h -no-obsolete ${CMAKE_CURRENT_SOURCE_DIR}/src -ts
		${TS_DIR}/keyzip_zh_CN.ts
		${TS_DIR}/keyzip_en_US.ts
	COMMAND ${CMAKE_COMMAND} -E make_directory ${QM_DIR}
	COMMAND Qt5::lrelease ${TS_DIR}/keyzip_zh_CN.ts -qm ${QM_DIR}/keyzip_zh_CN.qm
	COMMAND Qt5::lrelease ${TS_DIR}/keyzip_en_US.ts -qm ${QM_DIR}/keyzip_en_US.qm
	COMMENT "Updating translations and generating .qm files..."
)

add_dependencies(KeyZip KeyZip_translations)

# 添加此命令以在构建后复制 DLL
add_custom_command(TARGET KeyZip POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
	"$<TARGET_FILE_DIR:7zip::7zip>/../bin/7zip.dll" # 从 .lib 目录找到 .dll
	$<TARGET_FILE_DIR:KeyZip>
	COMMENT "Copying 7z.dll to output directory"
)
