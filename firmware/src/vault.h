#pragma once
#include <Arduino.h>

bool vault_init();
bool vault_set_pin(const String &pin);
bool vault_check_pin(const String &pin);
bool vault_store_password(const String &label, const String &password);
bool vault_retrieve_password(const String &label, String &out);
bool vault_type_password(const String &label);
bool vault_has_pin();
