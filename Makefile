HOMEKIT_PATH ?= $(abspath $(IDF_PATH)/../esp-homekit-sdk)
COMMON_COMPONENT_PATH ?= $(HOMEKIT_PATH)/examples/common

PROJECT_NAME := esp-homekit-door
EXTRA_COMPONENT_DIRS += $(HOMEKIT_PATH)/components/
EXTRA_COMPONENT_DIRS += $(HOMEKIT_PATH)/components/homekit
EXTRA_COMPONENT_DIRS += $(COMMON_COMPONENT_PATH)

include $(IDF_PATH)/make/project.mk
