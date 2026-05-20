#!/usr/bin/env python3
"""
Generates PlatformIO build flags:
  - VERSION from git
  - WIFI_SSID / WIFI_PASS / OTA_PASS from pio_secrets.py (gitignored)

Secrets setup:
  cp pio_secrets_example.py pio_secrets.py
  # edit pio_secrets.py with your credentials
"""
import subprocess
import datetime
import sys
import os

def _git(*args):
    try:
        return subprocess.check_output(
            ['git'] + list(args),
            stderr=subprocess.DEVNULL
        ).decode().strip()
    except Exception:
        return None

build_time = datetime.datetime.now().strftime("%Y-%m-%dT%H:%M:%S")
ver = _git('describe', '--abbrev=7', '--always', '--tags', '--match', 'v*') or 'unknown'

print(f"'-DVERSION=\"{ver} ({build_time})\"'")

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
try:
    import pio_secrets
    ssid = getattr(pio_secrets, 'WIFI_SSID', None)
    pw   = getattr(pio_secrets, 'WIFI_PASS', None)
    if ssid:    print(f"'-DWIFI_SSID=\"{ssid}\"'")
    if pw:      print(f"'-DWIFI_PASS=\"{pw}\"'")
    ota_pass = getattr(pio_secrets, 'OTA_PASS', None)
    if ota_pass: print(f"'-DOTA_PASS=\"{ota_pass}\"'")
    mqtt_host = getattr(pio_secrets, 'MQTT_HOST', None)
    mqtt_port = getattr(pio_secrets, 'MQTT_PORT', None)
    mqtt_user = getattr(pio_secrets, 'MQTT_USER', None)
    mqtt_pass = getattr(pio_secrets, 'MQTT_PASS', None)
    mqtt_base  = getattr(pio_secrets, 'MQTT_BASE',  None)
    mqtt_topic = getattr(pio_secrets, 'MQTT_TOPIC', None)
    if mqtt_host:  print(f"'-DMQTT_HOST=\"{mqtt_host}\"'")
    if mqtt_port:  print(f"'-DMQTT_PORT={mqtt_port}'")
    if mqtt_user:  print(f"'-DMQTT_USER=\"{mqtt_user}\"'")
    if mqtt_pass:  print(f"'-DMQTT_PASS=\"{mqtt_pass}\"'")
    if mqtt_base:  print(f"'-DMQTT_BASE=\"{mqtt_base}\"'")
    if mqtt_topic: print(f"'-DMQTT_TOPIC=\"{mqtt_topic}\"'")
except ImportError:
    pass  # no pio_secrets.py — credentials fall back to defaults
