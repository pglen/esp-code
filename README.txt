                                README

 Common directory for utilities, wifi and time related stuff.

 Files:
 
    captdns.c .h            DNS Server
    httpd.c .h              HTTP Server
    utils.c  .h             Misc helpers
    wifi.c .h               Wifi Helpers
    
  Include them in main project like this:  (adjust path)
 
 COMPONENT_SRCDIRS:=$(abspath ../common/v000/)
 COMPONENT_ADD_INCLUDEDIRS:=$(abspath ../common/v000/)
 COMPONENT_SUBMODULES:=$(abspath ../common/v000/)
 EXTRA_COMPONENT_DIRS:=$(abspath ../common/v000/)
 


