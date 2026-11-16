export SOURCE_DIR := $(abspath .)
export SRC_DIR := $(abspath src)
export LIB_DIR := $(abspath lib)
export ARCH_DIR := $(abspath arch)
export BUILD_DIR := $(abspath .build)

export KERNEL_ELF := $(BUILD_DIR)/kernel.elf
export KERNEL_LD := $(SRC_DIR)/kernel.ld
export KERNEL_MAP := $(BUILD_DIR)/kernel.map
export COMPILE_FLAGS_TXT := $(SOURCE_DIR)/compile_flags.txt

export CC := clang
export CC_FLAGS := $(filter-out -xc, $(strip $(file < $(COMPILE_FLAGS_TXT)))) -fmacro-prefix-map=$(SOURCE_DIR)/=

export QEMU := qemu-system-riscv64
export QEMU_FLAGS := -machine virt -bios default -serial mon:stdio --no-reboot