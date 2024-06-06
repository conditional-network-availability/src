################################################################################
#
# nwsender
#
################################################################################

# Note: Buildroot does not support to build a LKM only, so we need some wrapper
# application that is build alongside the module. In this case, this is just
# a hello world application.
# More information can be retrieved at:
# https://buildroot.org/downloads/manual/manual.html#_infrastructure_for_packages_building_kernel_modules

NWSENDER_VERSION = 1.0.0
NWSENDER_LICENSE = GPL-2.0
NWSENDER_LICENSE_FILES = COPYING

NWSENDER_SITE = ../../src/nw/nwsender
NWSENDER_SITE_METHOD = local

NWSENDER_MODULE_SUBDIRS = lkm

NWSENDER_INSTALL_STAGING = NO
NWSENDER_INSTALL_TARGET = YES

define NWSENDER_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/nwsender $(TARGET_DIR)/usr/bin/nwsender
endef

$(eval $(cmake-package))
