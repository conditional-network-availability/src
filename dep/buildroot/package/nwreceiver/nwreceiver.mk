################################################################################
#
# nwreceiver
#
################################################################################

# Note: Buildroot does not support to build a LKM only, so we need some wrapper
# application that is build alongside the module. In this case, this is just
# a hello world application.
# More information can be retrieved at:
# https://buildroot.org/downloads/manual/manual.html#_infrastructure_for_packages_building_kernel_modules

NWRECEIVER_VERSION = 1.0.0
NWRECEIVER_LICENSE = GPL-2.0
NWRECEIVER_LICENSE_FILES = COPYING

NWRECEIVER_SITE = ../../src/nw/nwreceiver
NWRECEIVER_SITE_METHOD = local

NWRECEIVER_MODULE_SUBDIRS = lkm

NWRECEIVER_INSTALL_STAGING = NO
NWRECEIVER_INSTALL_TARGET = YES

define NWRECEIVER_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/nwreceiver $(TARGET_DIR)/usr/bin/nwreceiver
endef

$(eval $(cmake-package))
