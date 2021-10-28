## @file
# Redfish DSC include file for [LibraryClasses*] section of all Architectures.
#
# This file can be included to the [LibraryClasses*] section(s) of a platform DSC file
# by using "!include RedfishPkg/RedfisLibs.dsc.inc" to specify the library instances
# of EDKII network library classes.
#
# (C) Copyright 2021 Hewlett Packard Enterprise Development LP<BR>
#
#    SPDX-License-Identifier: BSD-2-Clause-Patent
#
##
!if $(REDFISH_CLIENT_ALL_AUTOGENED) == TRUE
 !include RedfishClientPkg/RedfishJsonStructureLib.dsc.inc
!endif

  NetLib|NetworkPkg/Library/DxeNetLib/DxeNetLib.inf
  HttpLib|NetworkPkg/Library/DxeHttpLib/DxeHttpLib.inf
  RedfishLib|RedfishClientPkg/PrivateLibrary/RedfishLib/RedfishLib.inf
  RedfishFeatureUtilityLib|RedfishClientPkg/Library/RedfishFeatureUtilityLib/RedfishFeatureUtilityLib.inf
  RedfishPlatformConfigLib|RedfishPkg/Library/RedfishPlatformConfigLib/RedfishPlatformConfigLib.inf
  RedfishContentCodingLib|RedfishPkg/Library/RedfishContentCodingLibNull/RedfishContentCodingLibNull.inf
  ConverterCommonLib|RedfishClientPkg/ConverterLib/edk2library/ConverterCommonLib/ConverterCommonLib.inf
