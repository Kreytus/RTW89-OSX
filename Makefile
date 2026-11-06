# SPDX-License-Identifier: GPL-3.0-only
#
# Command-line kext build (no Xcode project). Needs macOS with Xcode or the
# Command Line Tools, plus acidanthera/MacKernelSDK at ./MacKernelSDK
# (`make sdk` fetches the pinned commit; CI does the same).
#
#   make                 DEBUG and RELEASE builds into build/{Debug,Release}
#   make Release         one configuration

VERSION     ?= 0.0.1
SDK_DIR     ?= MacKernelSDK
SDK_COMMIT  ?= 3f750085caa17ec3a7880f11c11bf4f48cd6a164
MIN_MACOS   ?= 14.0
CONFIGS     := Debug Release

CXX         := xcrun -sdk macosx clang++
CC          := xcrun -sdk macosx clang

KEXT_NAME   := AirportRtw89
BUNDLE_ID   := org.rtw89osx.AirportRtw89
SRC_DIR     := AirportRtw89/src
CXX_SRCS    := $(wildcard $(SRC_DIR)/*.cpp)
C_SRCS      := $(wildcard $(SRC_DIR)/*.c)
HDRS        := $(wildcard $(SRC_DIR)/*.hpp)

COMMON_FLAGS := -target x86_64-apple-macos$(MIN_MACOS) \
	-mkernel -nostdlib -fno-builtin -fno-common \
	-isystem $(SDK_DIR)/Headers \
	-DKERNEL -DKERNEL_PRIVATE -DDRIVER_PRIVATE -DAPPLE -DNeXT \
	-DKEXT_BUNDLE_ID=$(BUNDLE_ID) -DKEXT_VERSION=\"$(VERSION)\" \
	-Wall -Wextra -Werror -Wno-unused-parameter
CXX_ONLY      := -std=gnu++17 -fapple-kext -fno-exceptions -fno-rtti
FLAGS_Debug   := -O1 -g -DRTW89_DEBUG=1
FLAGS_Release := -O2 -DRTW89_DEBUG=0

LDFLAGS := -target x86_64-apple-macos$(MIN_MACOS) -nostdlib \
	-Xlinker -kext -Xlinker -export_dynamic \
	-L$(SDK_DIR)/Library/x86_64 -lkmod -lkmodc++ -lcc_kext

.PHONY: all clean sdk $(CONFIGS)
all: $(CONFIGS)

sdk:
	test -d $(SDK_DIR) || git clone https://github.com/acidanthera/MacKernelSDK.git $(SDK_DIR)
	git -C $(SDK_DIR) fetch --depth 1 origin $(SDK_COMMIT)
	git -C $(SDK_DIR) checkout -q $(SDK_COMMIT)

define build_config
OBJ_$(1) := $$(patsubst $(SRC_DIR)/%.cpp,build/obj/$(1)/%.o,$(CXX_SRCS)) \
            $$(patsubst $(SRC_DIR)/%.c,build/obj/$(1)/%.c.o,$(C_SRCS))
KEXT_$(1) := build/$(1)/$(KEXT_NAME).kext

build/obj/$(1)/%.o: $(SRC_DIR)/%.cpp $(HDRS)
	@mkdir -p $$(@D)
	$(CXX) $(COMMON_FLAGS) $(CXX_ONLY) $(FLAGS_$(1)) -c $$< -o $$@

build/obj/$(1)/%.c.o: $(SRC_DIR)/%.c
	@mkdir -p $$(@D)
	$(CC) $(COMMON_FLAGS) -std=gnu11 $(FLAGS_$(1)) -c $$< -o $$@

$(1): $$(OBJ_$(1))
	@mkdir -p $$(KEXT_$(1))/Contents/MacOS
	$(CXX) $(LDFLAGS) $$(OBJ_$(1)) -o $$(KEXT_$(1))/Contents/MacOS/$(KEXT_NAME)
	sed 's/@VERSION@/$(VERSION)/g' AirportRtw89/Info.plist > $$(KEXT_$(1))/Contents/Info.plist
	plutil -lint $$(KEXT_$(1))/Contents/Info.plist
endef
$(foreach c,$(CONFIGS),$(eval $(call build_config,$(c))))

clean:
	rm -rf ./build
