#include "vault.h"
#include <Preferences.h>

static Preferences prefs;
static const char *NAMESPACE = "vault";

bool vault_init() {
  prefs.begin(NAMESPACE, false);
  // Clear old data on init for development
  prefs.clear();
  return true;
}

// Very small XOR obfuscation for prototype only
static String xor_obfuscate(const String &s, const String &key) {
  String out = s;
  for (size_t i = 0; i < s.length(); ++i) {
    out[i] = s[i] ^ key[i % key.length()];
  }
  return out;
}

bool vault_set_pin(const String &pin) {
  prefs.putString("pin", pin);
  return true;
}

bool vault_check_pin(const String &pin) {
  String stored = prefs.getString("pin", "");
  return stored == pin;
}

bool vault_store_password(const String &label, const String &password) {
  String ob = xor_obfuscate(password, "static_key");
  String key = String("pw_") + label;
  prefs.putString(key.c_str(), ob);
  return true;
}

bool vault_retrieve_password(const String &label, String &out) {
  String key = String("pw_") + label;
  String ob = prefs.getString(key.c_str(), "");
  if (ob.length() == 0) return false;
  out = xor_obfuscate(ob, "static_key");
  return true;
}

bool vault_type_password(const String &label) {
  String pw;
  if (!vault_retrieve_password(label, pw)) return false;
  // For now, print to Serial as a stand-in for USB HID typing
  Serial.print("TYPE:"); Serial.println(pw);
  return true;
}

bool vault_has_pin() {
  String stored = prefs.getString("pin", "");
  return stored.length() > 0;
}
