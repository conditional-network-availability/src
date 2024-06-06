################################################################################
#
# nwlatency
#
################################################################################

# Note: Buildroot does not support to build a LKM only, so we need some wrapper
# application that is build alongside the module. In this case, this is just
# a hello world application.
# More information can be retrieved at:
# https://buildroot.org/downloads/manual/manual.html#_infrastructure_for_packages_building_kernel_modules

NWLATENCY_VERSION = 1.0.0
NWLATENCY_LICENSE = GPL-2.0
NWLATENCY_LICENSE_FILES = COPYING

NWLATENCY_SITE = ../../src/nw/nwlatency
NWLATENCY_SITE_METHOD = local

NWLATENCY_MODULE_SUBDIRS = lkm

NWLATENCY_INSTALL_STAGING = NO
NWLATENCY_INSTALL_TARGET = YES

define NWLATENCY_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/nwlatency $(TARGET_DIR)/usr/bin/nwlatency
endef

$(eval $(cmake-package))
