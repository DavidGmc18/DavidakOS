include config.mk

.PHONY: build clean run

SRC := $(shell find $(SRC_DIR) $(LIB_DIR) -type f -name '*.c')
OBJ := $(patsubst $(SRC_DIR)/%,$(BUILD_DIR)/src/%.o,$(filter $(SRC_DIR)/%,$(SRC))) \
       $(patsubst $(LIB_DIR)/%,$(BUILD_DIR)/lib/%.o,$(filter $(LIB_DIR)/%,$(SRC))) \
	   $(patsubst $(ARCH_DIR)/%,$(BUILD_DIR)/arch/%.o,$(filter $(ARCH_DIR)/%,$(SRC)))
DEP := $(OBJ:.o=.d)

-include $(DEP)

build: $(KERNEL_ELF)

$(KERNEL_ELF): $(OBJ) $(KERNEL_LD)
	@mkdir -p $(dir $@)
	@$(CC) $(CC_FLAGS) -fuse-ld=lld -T$(KERNEL_LD) -Wl,-Map=$(KERNEL_MAP) -o $@ $(OBJ)

$(BUILD_DIR)/%.c.o: $(SOURCE_DIR)/%.c
	@mkdir -p $(dir $@)
	@$(CC) $(CC_FLAGS) -MMD -MP -c $< -o $@

clean:
	@if [ -d "$(BUILD_DIR)" ]; then rm -rf -- "$(BUILD_DIR)"; fi

run:
	@$(QEMU) $(QEMU_FLAGS) -kernel $(KERNEL_ELF)