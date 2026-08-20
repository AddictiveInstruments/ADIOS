#
# following variables should be set before including this file:
#   - PROCESSOR e.g.: STM32G070CB
#   - FAMILY    e.g.: STM32G0xx
#   - BOARD     e.g.: STM32G070CB
#   - LCD       e.g.: clcd
#   - LD_FILE   e.g.: $(MIOS32_PATH)/etc/ld/$(FAMILY)/$(PROCESSOR).ld
#   - PROJECT   e.g.: project   # (.lst, .hex, .map, etc... will be added automatically)
#   - THUMB_SOURCE e.g.: main.c (.c only)
#   - THUMB_CPP_SOURCE e.g.: main.cp (.cpp only)
#   - THUMB_AS_SOURCE e.g.: assembly.s (.s only)
#   - ARM_SOURCE e.g.: my_startup.c (.c only)
#   - ARM_CPP_SOURCE e.g.: my_startup.cpp (.cpp only)
#   - ARM_AS_SOURCE e.g.: assembly.s (.s only)
#   - C_INCLUDE     e.g.: -I./ui  # (more include pathes will be added by .mk files)
#   - A_INCLUDE     same for assembly code
#   - DIST      e.g.: ./
#
# Modules can be added by including .mk files from $MIOS32_PATH/modules/*/*.mk
#

# if MIOS32_SHELL environment variable hasn't been set by the user, set it here
# Ubuntu users should set it to /bin/bash from external (-> "export MIOS32_SHELL /bin/bash")
MIOS32_SHELL ?= sh
export MIOS32_SHELL

# select GCC tools
# can be optionally overruled via environment variable
# e.g. for Cortex M3 support provided by CodeSourcery, use MIOS32_GCC_PREFIX=arm-none-eabi
# The usage of arm-elf isn't recommented due to compatibility issues!!!
MIOS32_GCC_PREFIX ?= arm-none-eabi

CC      = $(MIOS32_GCC_PREFIX)-gcc
CPP     = $(MIOS32_GCC_PREFIX)-g++
OBJCOPY = $(MIOS32_GCC_PREFIX)-objcopy
OBJDUMP = $(MIOS32_GCC_PREFIX)-objdump
NM      = $(MIOS32_GCC_PREFIX)-nm
SIZE    = $(MIOS32_GCC_PREFIX)-size

# where should the output files be located
PROJECT_OUT ?= $(PROJECT)_build

# default linker flags
LDFLAGS += -T $(LD_FILE) -mthumb -u _start -Wl,--gc-section  -Xlinker -M -Xlinker -Map=$(PROJECT_OUT)/$(PROJECT).map  -nostartfiles -lstdc++

# for https://launchpad.net/gcc-arm-embedded: enable newlib-nano for better performance
# not compatible with other toolchains (users have to switch to new version, or disable the line below)
LDFLAGS += --specs=nano.specs

# default assembler flags
AFLAGS += $(A_DEFINES) $(A_INCLUDE) -Wa,-adhlns=$(<:.s=.lst)

# define C flags
CFLAGS += $(C_DEFINES) $(C_INCLUDE) -Wall -Wno-format -Wno-switch -Wno-strict-aliasing

# add family specific arguments
ifeq ($(FAMILY),STM32F4xx)
# leads to a crash - reason not analysed yet
#CFLAGS += -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mthumb -mfloat-abi=hard -mlittle-endian -ffunction-sections -fdata-sections -fomit-frame-pointer
# works (but FPU not enabled)
CFLAGS += -mcpu=cortex-m4 -mlittle-endian -ffunction-sections -fdata-sections -fomit-frame-pointer
endif

ifeq ($(FAMILY),STM32G0xx)
CFLAGS += -mcpu=cortex-m0plus -mlittle-endian -ffunction-sections -fdata-sections -fomit-frame-pointer
endif

# define CPP flags
CPPFLAGS += $(CFLAGS) -fno-rtti -fno-exceptions -Wno-write-strings

# to monitor stack usage via $MIOS32_BIN_PATH/avstack.pl
# see also
# - http://dlbeer.co.nz/oss/avstack.html and 
# - https://mcuoneclipse.com/2015/08/21/gnu-static-stack-usage-analysis/
CFLAGS += -fstack-usage

# convert .c/.s -> .o
# Repo sources (the ones prefixed with $(MIOS32_PATH)/) get REPO-RELATIVE
# object paths - the prefix is stripped before $(PROJECT_OUT)/ is prepended
# below, so mios32/common/mios32_midi.c always maps to
# project_build/mios32/common/mios32_midi.o no matter how MIOS32_PATH is
# spelled. Before 2026-08-09 the raw source path was used as-is, with two
# real consequences: an absolute MIOS32_PATH nested a full copy of it inside
# project_build/ ("project_build//e/MIOS32/..."), and a RELATIVE one (e.g.
# "../../..", now the zero-config default in the app Makefiles) was worse -
# mkdir -p/gcc -o resolved the ".." segments as a real upward walk OUT of
# project_build/, scattering stray mios32/drivers/FreeRTOS object trees
# 3 levels up (found twice: bootloader/ 2026-08-04, apps/ 2026-08-09).
# Project-local sources (app.c etc, no MIOS32_PATH prefix) are unaffected.
THUMB_OBJS = $(patsubst $(MIOS32_PATH)/%,%,$(THUMB_SOURCE:.c=.o))
THUMB_CPP_OBJS = $(patsubst $(MIOS32_PATH)/%,%,$(THUMB_CPP_SOURCE:.cpp=.o))
THUMB_AS_OBJS = $(patsubst $(MIOS32_PATH)/%,%,$(THUMB_AS_SOURCE:.s=.o))
ARM_OBJS = $(patsubst $(MIOS32_PATH)/%,%,$(ARM_SOURCE:.c=.o))
ARM_CPP_OBJS = $(patsubst $(MIOS32_PATH)/%,%,$(ARM_CPP_SOURCE:.cpp=.o))
ARM_AS_OBJS = $(patsubst $(MIOS32_PATH)/%,%,$(ARM_AS_SOURCE:.s=.o))

# convert .s -> .lst
THUMB_AS_LST = $(THUMB_AS_SOURCE:.s=.lst)
ARM_AS_LST = $(ARM_AS_SOURCE:.s=.lst)

# list of all objects
ALL_OBJS = $(addprefix $(PROJECT_OUT)/, $(THUMB_OBJS) $(THUMB_CPP_OBJS) $(THUMB_AS_OBJS) $(ARM_OBJS) $(ARM_CPP_OBJS) $(ARM_AS_OBJS))

# list of all dependency files
ALL_DFILES = $(ALL_OBJS:.o=.d)

# which directories contain source files?
DIRS = $(dir $(THUMB_OBJS) $(THUMB_CPP_OBJS) $(THUMB_AS_OBJS) $(ARM_OBJS) $(ARM_CPP_OBJS) $(ARM_AS_OBJS))

# add files for distribution
DIST += $(MIOS32_PATH)/include/makefile/common.mk $(MIOS32_PATH)/include/c
DIST += $(LD_FILE)

# default rule
all: dirs cleanhex $(PROJECT).hex $(PROJECT_OUT)/$(PROJECT).bin $(PROJECT_OUT)/$(PROJECT).lss $(PROJECT_OUT)/$(PROJECT).sym projectinfo

# Static-boundary builds resolve their linker script from the per-family
# template here rather than in core.mk: that file is included
# BEFORE this one, so a target defined there would silently become make's
# default goal and the build would stop right after writing the script.
# (Dynamic-boundary builds get theirs written by gen_bsl_boundary.sh while
# the makefiles are still being parsed, so they need no rule at all.)
ifneq ($(LD_TEMPLATE_S),)
ifneq ($(MIOS32_USE_DYNAMIC_BSL_BOUNDARY),1)
$(CURDIR)/$(PROJECT_OUT)/cpu.ld: $(LD_TEMPLATE_S) $(MIOS32_PATH)/etc/ld/adios_body.ld.inc
	@mkdir -p $(dir $@)
	@echo "Resolving $(notdir $(LD_TEMPLATE_S)) for $(PROCESSOR), $(if $(filter 0,$(MIOS32_USE_BOOTLOADER)),no bootloader - application owns the whole flash,fixed BSL boundary $(ADIOS_LD_BSL_BOUNDARY_K)K) -> $@"
	@$(LD_PREPROCESS) $(LD_TEMPLATE_S) -o $@
endif
endif

# define debug/release target for easier use in codeblocks
debug: all
Debug: all
release: all
Release: all

################################################################################
# Automatic bootloader/app production deliverables (opt-in - set
# MIOS32_USE_DYNAMIC_BSL_BOUNDARY = 1 in the app's own Makefile before
# including core.mk, see that file's own comment and
# etc/gen_bsl_boundary.sh for the full mechanism). Two files, named after
# this app's own directory (not the CPU, not the generic $(PROJECT) name):
#   1) <app>_full_bsl_app.bin - combined bootloader+app image for one-shot SWD
#      flashing of a fresh board. project_build/$(PROJECT).bin already IS
#      this: mios32_bsl.c embeds an exact copy of the bootloader ahead of the
#      real app code at the generated boundary - copied here under an
#      explicit name so it's never confused with the app-only artifact below.
#   2) <app>_app_only.hex - app code only (embedded bootloader copy excluded)
#      for injecting via MIOS Studio without touching SWD, the normal
#      end-user/field update path.
# This "all:" statement has no recipe body - Make merges its prerequisite
# list into the "all:" rule above instead of conflicting with it.
################################################################################
ifeq ($(MIOS32_USE_DYNAMIC_BSL_BOUNDARY),1)
DYNAMIC_BSL_APP_NAME = $(notdir $(CURDIR))

all: $(DYNAMIC_BSL_APP_NAME)_full_bsl_app.bin $(DYNAMIC_BSL_APP_NAME)_app_only.hex $(DYNAMIC_BSL_APP_NAME)_bsl_updater.hex

$(DYNAMIC_BSL_APP_NAME)_full_bsl_app.bin: project_build/$(PROJECT).bin
	cp $< $@

$(DYNAMIC_BSL_APP_NAME)_app_only.hex: project_build/$(PROJECT).elf
	$(OBJCOPY) --remove-section=.mios32_bsl -O ihex $< $@

# 3) <app>_bsl_updater.hex - the complete one-file BSL update, matched to
#    THIS app's chip and boundary (the updater is as chip/boundary-bound as
#    the app itself, so it ships from here under the app's own name). It
#    carries BOTH the update tool and the new bootloader image in disjoint
#    address ranges - MIOS Studio runs the whole two-stage sequence from it
#    automatically (see bootloader/updater/Makefile). Built by the updater's
#    own Makefile for $(PROCESSOR); MIOS32_PATH is passed relative to THAT
#    directory (always 2 levels deep), not inherited - an app-relative or
#    CubeIDE-env value would resolve wrongly from there. BSL_RELAY_SRC points
#    the updater build back at THIS app's mios32_config.h: the board's MIDI
#    wiring lives there (BSL_RELAY block), and the updater must come up on
#    the same connector as the bootloader it replaces.
#    MIOS32_DEVICE_ID_PERSIST travels the same way, and it MUST: that sub-make
#    re-runs gen_bsl_boundary.sh, which rebuilds bootloader/src in place and
#    REGENERATES the embedded .inc this application links. Without the switch
#    here, the bootloader that ends up in the combined image is the one built
#    by this second pass - i.e. one that never looks for the stored device ID,
#    silently undoing what the application's own pass produced (2026-08-11).
#    MIOS32_USERDATA_PAGES travels for its own reason: the update tool is given
#    everything from its origin to the top of usable flash, so it has to know
#    which pages up there belong to the application's data and are not flash it
#    may be linked over.
$(DYNAMIC_BSL_APP_NAME)_bsl_updater.hex: project_build/$(PROJECT).elf
	+$(MAKE) -C $(MIOS32_PATH)/bootloader/updater MIOS32_PATH=../.. PROCESSOR=$(PROCESSOR) BSL_RELAY_SRC=$(CURDIR) MIOS32_DEVICE_ID_PERSIST=$(MIOS32_DEVICE_ID_PERSIST) MIOS32_USERDATA_PAGES=$(MIOS32_USERDATA_PAGES) MIOS32_BSL_PADDING=$(MIOS32_BSL_PADDING)
	cp $(MIOS32_PATH)/bootloader/updater/updater_$(PROCESSOR).hex $@
endif

# create the output directories
dirs:
	@-if [ ! -e $(PROJECT_OUT) ]; then mkdir $(PROJECT_OUT); fi;
	@-$(foreach DIR,$(DIRS), if [ ! -e $(PROJECT_OUT)/$(DIR) ]; \
	 then mkdir -p $(PROJECT_OUT)/$(DIR); fi; )


# rule to create a .hex and .bin file
%.bin : $(PROJECT_OUT)/$(PROJECT).elf
	@$(OBJCOPY) $< -O binary $@
%.hex : $(PROJECT_OUT)/$(PROJECT).elf
	@$(OBJCOPY) $< -O ihex $@

# rule to create a listing file from .elf
%.lss: $(PROJECT_OUT)/$(PROJECT).elf
	@$(OBJDUMP) -w -h -S -C $< > $@

# rule to create a symbol table from .elf
%.sym: $(PROJECT_OUT)/$(PROJECT).elf
	@$(NM) -n $< > $@

# rule to create .elf file. $(LD_FILE) is a prerequisite because it is
# GENERATED now (resolved from the per-family template, see
# core/core.mk) - without it, a static
# build would try to link before its linker script exists.
$(PROJECT_OUT)/$(PROJECT).elf: $(ALL_OBJS) $(LD_FILE)
	@$(CC) $(CFLAGS) $(ALL_OBJS) $(LIBS) $(LDFLAGS) -o$@


# rule to output project informations
projectinfo:
	@echo "-------------------------------------------------------------------------------"
	@echo "Application successfully built for:"
	@echo "Processor: $(PROCESSOR)"
	@echo "Family:    $(FAMILY)"
	@echo "Board:     $(BOARD)"
	@echo "LCD:       $(LCD)"
	@echo "-------------------------------------------------------------------------------"
	$(SIZE) $(PROJECT_OUT)/$(PROJECT).elf
	@grep -E '__ram_start|__ram_end' project_build/project.sym

# default rule for compiling .c programs
# inspired from the "super makefile" published at http://gpwiki.org/index.php/Make
# Rule for creating object file and .d file, the sed magic is to add
# the object path at the start of the file because the files gcc
# outputs assume it will be in the same dir as the source file.
#
# Two pattern rules per source kind since 2026-08-09: repo sources get
# repo-relative object paths (see the THUMB_OBJS comment above), so their
# real file lives under $(MIOS32_PATH)/ - first rule. Project-local sources
# (app.c etc) keep their path as-is - second rule. GNU make picks whichever
# rule's prerequisite actually exists for a given .o.
$(PROJECT_OUT)/%.o: $(MIOS32_PATH)/%.c
	@echo Creating object file for $(notdir $<)
	@$(CC) -Wp,-MMD,$(PROJECT_OUT)/$*.dd $(CFLAGS) -mthumb -c $< -o $@
	@sed -e '1s/^\(.*\)$$/$(subst /,\/,$(dir $@))\1/' $(PROJECT_OUT)/$*.dd > $(PROJECT_OUT)/$*.d
	@rm -f $(PROJECT_OUT)/$*.dd

$(PROJECT_OUT)/%.o: %.c
	@echo Creating object file for $(notdir $<)
	@$(CC) -Wp,-MMD,$(PROJECT_OUT)/$*.dd $(CFLAGS) -mthumb -c $< -o $@
	@sed -e '1s/^\(.*\)$$/$(subst /,\/,$(dir $@))\1/' $(PROJECT_OUT)/$*.dd > $(PROJECT_OUT)/$*.d
	@rm -f $(PROJECT_OUT)/$*.dd

$(PROJECT_OUT)/%.o: $(MIOS32_PATH)/%.cpp
	@echo Creating object file for $(notdir $<)
	@$(CC) -Wp,-MMD,$(PROJECT_OUT)/$*.dd $(CPPFLAGS) -mthumb -c $< -o $@
	@sed -e '1s/^\(.*\)$$/$(subst /,\/,$(dir $@))\1/' $(PROJECT_OUT)/$*.dd > $(PROJECT_OUT)/$*.d
	@rm -f $(PROJECT_OUT)/$*.dd

$(PROJECT_OUT)/%.o: %.cpp
	@echo Creating object file for $(notdir $<)
	@$(CC) -Wp,-MMD,$(PROJECT_OUT)/$*.dd $(CPPFLAGS) -mthumb -c $< -o $@
	@sed -e '1s/^\(.*\)$$/$(subst /,\/,$(dir $@))\1/' $(PROJECT_OUT)/$*.dd > $(PROJECT_OUT)/$*.d
	@rm -f $(PROJECT_OUT)/$*.dd

$(PROJECT_OUT)/%.o: $(MIOS32_PATH)/%.s
	@echo Creating object file for $(notdir $<)
	@$(CC) -Wp,-MMD,$(PROJECT_OUT)/$*.dd $(ASFLAGS) -mthumb -c $< -o $@
	@sed -e '1s/^\(.*\)$$/$(subst /,\/,$(dir $@))\1/' $(PROJECT_OUT)/$*.dd > $(PROJECT_OUT)/$*.d
	@rm -f $(PROJECT_OUT)/$*.dd

$(PROJECT_OUT)/%.o: %.s
	@echo Creating object file for $(notdir $<)
	@$(CC) -Wp,-MMD,$(PROJECT_OUT)/$*.dd $(ASFLAGS) -mthumb -c $< -o $@
	@sed -e '1s/^\(.*\)$$/$(subst /,\/,$(dir $@))\1/' $(PROJECT_OUT)/$*.dd > $(PROJECT_OUT)/$*.d
	@rm -f $(PROJECT_OUT)/$*.dd

# Includes the .d files so it knows the exact dependencies for every
# source.
-include $(ALL_DFILES)

# TODO: solution to differ between THUMB and ARM objects!
# we could search in the ARM*OBJS list and prevent the usage of -mthumb in this case
#$(ARM_OBJS) : %.o : %.c
#	$(CC) -c $(CFLAGS) $< -o $@

#$(ARM_CPP_OBJS) : %.o : %.cpp
#	$(CPP) -c $(CPPFLAGS) $< -o $@

#$(ARM_AS_OBJS) : %.o : %.s
#	$(CC) -c $(AFLAGS) $< -o $@


# clean temporary files
clean:
	rm -rf $(PROJECT_OUT)

# clean project image
cleanhex:
	rm -f $(PROJECT).hex

# clean temporary files + project image
cleanall: clean cleanhex


# for use with graphviz and egypt
callgraph: egyptall egypt_tidy egypt_all egypt_project


egyptall: CFLAGS += -dr
egyptall: all

egypt_tidy:
	@rm -fR egypt
	@echo "-------------------------------------------------------------------------------"
	@echo "Generating call graphs in .dot files"
	@echo "-------------------------------------------------------------------------------"
	@mkdir egypt
	@mv *.expand egypt/

egypt_all:
	@perl $(MIOS32_PATH)/etc/egypt/egypt egypt/*.expand > egypt/$(PROJECT).dot


egypt_project: EGYPTFILES = find egypt/*.expand -maxdepth 1 ! -name "mios32_*.expand" ! -name "stm32f10x*.expand" ! -name "usb_*.expand" ! -name "printf-stdarg.c*.expand" ! -name "crt0_STM32x.c*.expand" ! -name "app_lcd.c*.expand" ! -name "heap_*.expand" ! -name "list.c*.expand" ! -name "port.c*.expand" ! -name "queue.c*.expand" ! -name "main.c*.expand"
egypt_project:
	@perl $(MIOS32_PATH)/etc/egypt/egypt `$(EGYPTFILES)` > egypt/$(PROJECT)_NoMIOS.dot


callgraph_convert:
	@$(MIOS32_SHELL) $(MIOS32_PATH)/etc/egypt/dot_output.sh

callgraph_all: callgraph callgraph_convert

callgraph_clean: 
	@rm -fR egypt
