#
# If your board is a mega 2560 uncomment the following two lines...
#BOARD = mega2560
#GCS = -Wl,--gc-sections

# ...or either mega 1280 or teensy++ 2.0 and quadram...
#BOARD = mega
#BOARD = teensypp2

# ...or if you are using the teensy 3.x comment out these...
EXTRA_FLAGS += -D USE_MULTIPLE_APP_API=16
EXTRA_FLAGS += -D USB_HOST_SERIAL=Serial1
EXTRA_FLAGS += -DLCD_USING_SPI=1 
# ... and uncomment the correct board...
#BOARD = teensy30
#BOARD = teensy31

# ... then set your Arduino PROGRAMMING port here
#PORT = /dev/ttyUSB0
PORT = /dev/ttyACM0

# Adjust to your needs (Should be Zero)
EXTRA_FLAGS += -D CONSOLE=0
# How many files to open at once
EXTRA_FLAGS += -D _FS_LOCK=1
# How many volumes allowed to mount
EXTRA_FLAGS += -D _VOLUMES=4
# How many USB drivers
EXTRA_FLAGS += -D MAX_USB_MS_DRIVERS=1
# Max size of a sector, 512, 1024 or 2048
EXTRA_FLAGS += -D _MAX_SS=512
# Set _FS_TINY to 1 to save RAM, but speed will suffer
EXTRA_FLAGS += -D _FS_TINY=0
# Use fast seek?
# This adds 966 bytes of code.
# It does not seem to help at all, with normal code.
# It is not even faster for pre-allocation.
# It MIGHT help if you do random access.
# Best to keep set to zero.
EXTRA_FLAGS += -D _USE_FASTSEEK=0

# Uncomment to enable debugging of USB HOST code
#EXTRA_FLAGS += -D DEBUG_USB_HOST

# This enables long file names
EXTRA_FLAGS += -D _USE_LFN=3

# And finally, the part that brings everything together for you.
include ../Arduino_Makefile_master/_Makefile.master
