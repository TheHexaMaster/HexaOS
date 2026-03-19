#pragma once

bool FilesInit();
bool FilesMount();
bool FilesExists(const char* path);
String FilesReadText(const char* path);
bool FilesWriteText(const char* path, const char* text);