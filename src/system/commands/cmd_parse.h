/*
  HexaOS - cmd_parse.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Shared command argument parsing utilities for HexaOS built-in commands.
  Internal header used only by cmd_*.cpp domain files. Not part of the
  public command engine API.
*/

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

static inline const char* CmdSkipWs(const char* text) {
  if (!text) {
    return "";
  }
  while ((*text == ' ') || (*text == '\t')) {
    text++;
  }
  return text;
}

static inline bool CmdExtractToken(const char** text_io, char* out, size_t out_size) {
  if (!text_io || !out || (out_size == 0)) {
    return false;
  }

  const char* text = CmdSkipWs(*text_io);
  if (!text[0]) {
    return false;
  }

  const char* end = text;
  while (*end && (*end != ' ') && (*end != '\t')) {
    end++;
  }

  size_t len = (size_t)(end - text);
  if ((len == 0) || (len >= out_size)) {
    return false;
  }

  memcpy(out, text, len);
  out[len] = '\0';

  *text_io = CmdSkipWs(end);
  return true;
}

static inline bool CmdParseInt32Token(const char** text_io, int32_t* value_out) {
  char token[32];

  if (!CmdExtractToken(text_io, token, sizeof(token))) {
    return false;
  }

  char* endptr = nullptr;
  long value = strtol(token, &endptr, 10);
  if ((endptr == token) || (*endptr != '\0')) {
    return false;
  }

  if ((value < INT32_MIN) || (value > INT32_MAX)) {
    return false;
  }

  *value_out = (int32_t)value;
  return true;
}

static inline bool CmdParseFloatToken(const char** text_io, float* value_out) {
  char token[32];

  if (!CmdExtractToken(text_io, token, sizeof(token))) {
    return false;
  }

  char* endptr = nullptr;
  float value = strtof(token, &endptr);
  if ((endptr == token) || (*endptr != '\0')) {
    return false;
  }

  *value_out = value;
  return true;
}

static inline bool CmdSplitKeyValue(const char* text, char* key_out, size_t key_size, const char** value_out) {
  if (!text || !key_out || (key_size == 0) || !value_out) {
    return false;
  }

  while ((*text == ' ') || (*text == '\t')) {
    text++;
  }

  if (!text[0]) {
    return false;
  }

  const char* sep = text;
  while (*sep && (*sep != ' ') && (*sep != '\t')) {
    sep++;
  }

  size_t key_len = (size_t)(sep - text);
  if ((key_len == 0) || (key_len >= key_size)) {
    return false;
  }

  memcpy(key_out, text, key_len);
  key_out[key_len] = '\0';

  while ((*sep == ' ') || (*sep == '\t')) {
    sep++;
  }

  *value_out = sep;
  return (sep[0] != '\0');
}

static inline bool CmdExtractSingleKey(const char* text, char* key_out, size_t key_size) {
  if (!text || !key_out || (key_size == 0)) {
    return false;
  }

  while ((*text == ' ') || (*text == '\t')) {
    text++;
  }

  if (!text[0]) {
    return false;
  }

  const char* end = text;
  while (*end && (*end != ' ') && (*end != '\t')) {
    end++;
  }

  while ((*end == ' ') || (*end == '\t')) {
    end++;
  }

  if (*end != '\0') {
    return false;
  }

  size_t len = (size_t)(end - text);
  while ((len > 0) && ((text[len - 1] == ' ') || (text[len - 1] == '\t'))) {
    len--;
  }

  if ((len == 0) || (len >= key_size)) {
    return false;
  }

  memcpy(key_out, text, len);
  key_out[len] = '\0';
  return true;
}
