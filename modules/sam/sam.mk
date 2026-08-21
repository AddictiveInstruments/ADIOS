# defines additional rules for integrating the random module

# enhance include path
C_INCLUDE += -I $(ADIOS_PATH)/modules/sam/src


# add modules to thumb sources (TODO: provide makefile option to add code to ARM sources)
THUMB_SOURCE += \
  $(ADIOS_PATH)/modules/sam/src/sam.c \
  $(ADIOS_PATH)/modules/sam/src/debug.c \
  $(ADIOS_PATH)/modules/sam/src/render.c \
  $(ADIOS_PATH)/modules/sam/src/processframes.c \
  $(ADIOS_PATH)/modules/sam/src/createtransitions.c \
  $(ADIOS_PATH)/modules/sam/src/reciter.c


# directories and files that should be part of the distribution (release) package
DIST += $(ADIOS_PATH)/modules/sam/src
