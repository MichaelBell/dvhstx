export BOARD_NAME=dvhstx
CI_USE_ENV=1
BASE_DIR=`pwd`
CI_PROJECT_ROOT=$BASE_DIR/dvhstx
CI_BUILD_ROOT=$BASE_DIR
. $CI_PROJECT_ROOT/ci/micropython.sh
cmake_configure $BOARD_NAME
cmake_build $BOARD_NAME
cd ..
