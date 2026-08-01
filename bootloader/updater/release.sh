# $Id: release.sh 1949 2014-01-28 22:30:15Z tk $

if [[ "$1" == "" ]]; then
  echo "SYNTAX: release.sh <release-directory>"
  exit 1
fi

RELEASE_DIR=$1

if [[ -e $RELEASE_DIR ]]; then
  echo "ERROR: the release directory '$RELEASE_DIR' already exists!"
  exit 1
fi

###############################################################################
echo "Creating $RELEASE_DIR"

mkdir $RELEASE_DIR
cp README.txt $RELEASE_DIR

###############################################################################
echo "Building for MBHP_DIPCOREF4"

make cleanall
export STM32F405xx=1
export MIOS32_FAMILY=STM32F4xx
export MIOS32_PROCESSOR=STM32F405RG
export MIOS32_BOARD=MBHP_DIPCOREF4
export MIOS32_LCD=universal
mkdir -p $RELEASE_DIR/$MIOS32_BOARD
make > $RELEASE_DIR/$MIOS32_BOARD/log.txt
echo FAMILY=$FAMILY
cp project.hex $RELEASE_DIR/$MIOS32_BOARD
cp project_build/project.bin $RELEASE_DIR/$MIOS32_BOARD

###############################################################################
echo "Building for STM32G0GENERIC"

make cleanall
export STM32G050xx=1
export MIOS32_FAMILY=STM32G0xx
export MIOS32_PROCESSOR=STM32G050T8
export MIOS32_BOARD=STM32G0GENERIC
export MIOS32_LCD=universal
mkdir -p $RELEASE_DIR/$MIOS32_BOARD
make > $RELEASE_DIR/$MIOS32_BOARD/log.txt
echo FAMILY=$FAMILY
cp project.hex $RELEASE_DIR/$MIOS32_BOARD
cp project_build/project.bin $RELEASE_DIR/$MIOS32_BOARD

###############################################################################
make cleanall
echo "Done!"

