# Build the network settings homebrew with devkitPPC and libogc
ifeq ($(strip $(DEVKITPPC)),)
$(error "Set DEVKITPPC in your environment, for example /opt/devkitpro/devkitPPC")
endif

include $(DEVKITPPC)/wii_rules

TARGET  := wii-network-settings
BUILD   := build
SOURCES := src

CFLAGS  = -g -O2 -Wall -Wextra $(MACHDEP) $(INCLUDE)
LDFLAGS = -g $(MACHDEP) -Wl,-Map,$(notdir $@).map
LIBS    := -lwiiuse -lbte -logc -lm

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT := $(CURDIR)/$(TARGET)
export VPATH  := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)
CFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
export LD := $(CC)
export OFILES := $(CFILES:.c=.o)
export INCLUDE := -I$(CURDIR)/$(BUILD) -I$(LIBOGC_INC)
export LIBPATHS := -L$(LIBOGC_LIB)

.PHONY: $(BUILD) clean dist

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@rm -fr $(BUILD) $(OUTPUT).elf $(OUTPUT).dol dist

# Lay out the Homebrew Channel folder under dist for copying to an SD card
dist: $(BUILD)
	@mkdir -p dist/apps/$(TARGET)
	@cp $(OUTPUT).dol dist/apps/$(TARGET)/boot.dol
	@cp meta.xml icon.png dist/apps/$(TARGET)/

else

DEPENDS := $(OFILES:.o=.d)

$(OUTPUT).dol: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)

-include $(DEPENDS)

endif
