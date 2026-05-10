AS := ../../../../tools/local/vax-binutils/bin/vax-netbsdelf-as
OBJCOPY := ../../../../tools/local/vax-binutils/bin/vax-netbsdelf-objcopy
OBJDUMP := ../../../../tools/local/vax-binutils/bin/vax-netbsdelf-objdump

PATCHES := skip_ram_selftest
BUILD_DIR := build

.PHONY: all clean disasm

all: $(PATCHES:%=$(BUILD_DIR)/%.bin)

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/%.o: %.s | $(BUILD_DIR)
	$(AS) -o $@ $<

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.o
	$(OBJCOPY) -O binary -j .text $< $@

disasm: $(PATCHES:%=$(BUILD_DIR)/%.o)
	$(OBJDUMP) -d $(PATCHES:%=$(BUILD_DIR)/%.o)

clean:
	rm -rf $(BUILD_DIR)
