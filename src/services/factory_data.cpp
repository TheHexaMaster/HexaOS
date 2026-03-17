#include "hexaos.h"

bool FactoryDataInit() {
  LogInfo("FACT: init");
  return EspNvsOpenFactory();
}