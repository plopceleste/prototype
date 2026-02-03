cmake_minimum_required(VERSION 3.6.0)
project(Main)
include_directories(
	${PROJECT_SOURCE_DIR}/include
	${PROJECT_SOURCE_DIR}/src
)
file(GLOB SRCS
	"${PROJECT_SOURCE_DIR}/include/*"
	"${PROJECT_SOURCE_DIR}/src/*"
)
add_executable(Main ${SRCS})
find_package(SDL2 REQUIRED)
target_link_libraries(Main ${SDL2_LIBRARIES})
find_package(SDL2_image REQUIRED)
target_link_libraries(Main ${SDL2_IMAGE_LIBRARIES})
find_package(SDL2_mixer REQUIRED)
target_link_libraries(Main ${SDL2_MIXER_LIBRARIES})
find_package(SDL2_ttf REQUIRED)
target_link_libraries(Main ${SDL2_TTF_LIBRARIES})