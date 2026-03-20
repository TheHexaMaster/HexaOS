## NVS adapter auto-erase fallback
system/adapters/nvs_adapter.cpp:60-85
InitPartition() 

ESP_ERR_NVS_NO_FREE_PAGES or ESP_ERR_NVS_NEW_VERSION_FOUND

will delete partition and reinitialize. This need revision in future. 


## State catalogue tolerancy
system/handlers/nvs_state_handler.cpp:1144-1244
StateLoadRuntimeCatalog():
