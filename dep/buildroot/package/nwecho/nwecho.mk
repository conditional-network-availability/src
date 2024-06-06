################################################################################
#
# nwecho
#
################################################################################

# Note: Buildroot does not support to build a LKM only, so we need some wrapper
# application that is build alongside the module. In this case, this is just
# a hello world application.
# More information can be retrieved at:
# https://buildroot.org/downloads/manual/manual.html#_infrastructure_for_packages_building_kernel_modules

NWECHO_VERSION = 1.0.0
NWECHO_LICENSE = GPL-2.0
NWECHO_LICENSE_FILES = COPYING

NWECHO_SITE = ../../src/nw/nwecho
NWECHO_SITE_METHOD = local

NWECHO_MODULE_SUBDIRS = lkm

NWECHO_INSTALL_STAGING = NO
NWECHO_INSTALL_TARGET = YES

define NWECHO_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/nwecho $(TARGET_DIR)/usr/bin/nwecho
endef

$(eval $(cmake-package))
