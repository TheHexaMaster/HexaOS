#pragma once

typedef struct {
  const char* name;
  bool (*init)();
  void (*start)();
  void (*loop)();
  void (*every_100ms)();
  void (*every_1s)();
} HxModule;

extern const HxModule ModuleSystem;
extern const HxModule ModuleConsole;
extern const HxModule ModuleStorage;
extern const HxModule ModuleBerry;
extern const HxModule ModuleWeb;
extern const HxModule ModuleLvgl;