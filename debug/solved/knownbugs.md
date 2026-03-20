## NVS adapter auto-erase fallback
system/adapters/nvs_adapter.cpp:60-85
InitPartition() 

ESP_ERR_NVS_NO_FREE_PAGES or ESP_ERR_NVS_NEW_VERSION_FOUND

will delete partition and reinitialize. This need revision in future. 


## State catalogue tolerancy
system/handlers/nvs_state_handler.cpp:1144-1244
StateLoadRuntimeCatalog():


## Invalid selector ARDUINO USB MODE / CDC, defined after arduino.h include. This logic needs to be moved in build flags. Solve in later update.
#if HX_ENABLE_MODULE_CONSOLE
#include "system/adapters/console_adapter.h"

#if (HX_BUILD_CONSOLE_ADAPTER == HX_CONSOLE_ADAPTER_ARDUINO_USB_CDC)
  #ifndef ARDUINO_USB_MODE
    #define ARDUINO_USB_MODE 1
  #endif
  #ifndef ARDUINO_USB_CDC_ON_BOOT
    #define ARDUINO_USB_CDC_ON_BOOT 1
  #endif
#else
  #ifndef ARDUINO_USB_CDC_ON_BOOT
    #define ARDUINO_USB_CDC_ON_BOOT 0
  #endif
#endif

#endif