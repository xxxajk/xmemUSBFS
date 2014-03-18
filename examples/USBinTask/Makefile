#
# These are set for a mega 1280 + quadram.
# If your board is a mega 2560 uncomment the following two lines
#BOARD = mega2560
#GCS = -Wl,--gc-sections
# ...and then comment out the following line
#BOARD = mega
BOARD = teensypp2
# set your Arduino tty port here
#PORT = /dev/ttyUSB0
PORT = /dev/ttyACM0

# Adjust to your needs
#145040 uS
EXTRA_FLAGS = -DLCD_USING_SPI=1 
# How many files to open at once
EXTRA_FLAGS += -D _FS_LOCK=1
# How many volumes allowed to mount
EXTRA_FLAGS += -D _VOLUMES=4
# How many USB drivers
EXTRA_FLAGS += -D MAX_USB_MS_DRIVERS=1
# Max size of a sector, 512, 1024 or 2048
EXTRA_FLAGS += -D _MAX_SS=2048
# Set _FS_TINY to 1 to save RAM, but speed will suffer
EXTRA_FLAGS += -D _FS_TINY=0
# Use fast seek?
# This adds 966 bytes of code.
# It does not seem to help at all, not even for pre-allocation.
# Might help if you do random access.
EXTRA_FLAGS += -D _USE_FASTSEEK=0

# Uncomment to enable debugging of USB HOST code
#EXTRA_FLAGS += -D DEBUG_USB_HOST

# Leave these alone.
EXTRA_FLAGS += -D _USE_LFN=3
EXTRA_FLAGS += -D USE_MULTIPLE_APP_API=16
EXTRA_FLAGS += -D USB_HOST_SERIAL=Serial1
#EXTRA_FLAGS += -D _RAM_COPY_SZ=64
#EXTRA_FLAGS += -D _RAM_COPY_SZ=128
#EXTRA_FLAGS += -D _RAM_COPY_SZ=256
#EXTRA_FLAGS += -D _RAM_COPY_SZ=512

# You should not need to change this, but I place it here if you want to play.
# These are the defaults for the optimization of the flash and ram
#OPT_FLAGS = -Os -fno-exceptions -ffunction-sections -fdata-sections -MMD

# The following are the libraries used.
#LIB_DIRS =
#LIB_DIRS += ../libraries/xmem
#LIB_DIRS += $(ARD_HOME)/libraries/Wire
#LIB_DIRS += $(ARD_HOME)/libraries/Wire/utility
#LIB_DIRS += $(ARD_HOME)/libraries/SPI
#LIB_DIRS += ../libraries/ColorLCDShield
#LIB_DIRS += ../libraries/RTClib
#LIB_DIRS += ../libraries/USB_Host_Shield_2_0
#LIB_DIRS += ../libraries/generic_storage
#LIB_DIRS += ../libraries/xmemUSBFS
#LIB_DIRS += ../libraries/xmemUSB

# And finally, the part that brings everything together for you.
include ../Arduino_Makefile_master/_Makefile.master
