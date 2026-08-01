# defines additional rules for integrating the random module

# enhance include path
C_INCLUDE += -I $(MIOS32_PATH)/modules/sam/src


# add modules to thumb sources (TODO: provide makefile option to add code to ARM sources)
THUMB_SOURCE += \
  $(MIOS32_PATH)/modules/sam/src/sam.c \
  $(MIOS32_PATH)/modules/sam/src/debug.c \
  $(MIOS32_PATH)/modules/sam/src/render.c \
  $(MIOS32_PATH)/modules/sam/src/processframes.c \
  $(MIOS32_PATH)/modules/sam/src/createtransitions.c \
  $(MIOS32_PATH)/modules/sam/src/reciter.c


# directories and files that should be part of the distribution (release) package
DIST += $(MIOS32_PATH)/modules/sam/src
