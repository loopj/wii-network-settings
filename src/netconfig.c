#include "netconfig.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ogc/isfs.h>

#define NAND_PATH "/shared2/sys/net/02/config.dat"

// IOS file system errors that are expected in normal use
#define FS_EEXIST -105
#define FS_ENOENT -106

// Returned when the file on NAND has the wrong size
#define FS_EBADSIZE -1

// ISFS needs a 32 byte aligned buffer, so the file is staged here
static u8 file_buf[NETCONFIG_SIZE] __attribute__((aligned(32)));

// Order the security types cycle through in the menu
static const int encryption_order[] = {NETCONFIG_ENC_OPEN,     NETCONFIG_ENC_WEP64,    NETCONFIG_ENC_WEP128,
                                       NETCONFIG_ENC_WPA_TKIP, NETCONFIG_ENC_WPA2_AES, NETCONFIG_ENC_WPA_AES};

s32 netconfig_load(struct netconfig *config)
{
  // Open the file, treating a missing one as empty rather than as an error
  memset(config, 0, sizeof(*config));
  s32 rc = ISFS_Initialize();
  if (rc < 0)
    return rc;
  s32 fd = ISFS_Open(NAND_PATH, ISFS_OPEN_READ);
  if (fd < 0) {
    ISFS_Deinitialize();
    return fd == FS_ENOENT ? 0 : fd;
  }

  // Read the whole file in one go
  rc = ISFS_Read(fd, file_buf, NETCONFIG_SIZE);
  ISFS_Close(fd);
  ISFS_Deinitialize();
  if (rc < 0)
    return rc;
  if (rc != NETCONFIG_SIZE)
    return FS_EBADSIZE;

  memcpy(config, file_buf, NETCONFIG_SIZE);
  return 0;
}

s32 netconfig_save(struct netconfig *config)
{
  // Keep a single selected profile and name its media type in the header
  config->version = 0;
  config->media   = 0;
  for (int i = 0; i < NETCONFIG_PROFILES; i++) {
    struct netconfig_profile *p = &config->profile[i];
    if (!(p->flags & NETCONFIG_FLAG_SELECTED))
      continue;
    if (config->media != 0) {
      p->flags &= ~NETCONFIG_FLAG_SELECTED;
      continue;
    }
    config->media = (p->flags & NETCONFIG_FLAG_WIRED) ? 2 : 1;
  }
  if (config->link_timeout == 0)
    config->link_timeout = 7;
  memcpy(file_buf, config, NETCONFIG_SIZE);

  // Create the file if this console never had one
  s32 rc = ISFS_Initialize();
  if (rc < 0)
    return rc;
  rc = ISFS_CreateFile(NAND_PATH, 0, 3, 3, 3);
  if (rc < 0 && rc != FS_EEXIST) {
    ISFS_Deinitialize();
    return rc;
  }

  // Write the whole file in one go
  s32 fd = ISFS_Open(NAND_PATH, ISFS_OPEN_WRITE);
  if (fd < 0) {
    ISFS_Deinitialize();
    return fd;
  }
  rc = ISFS_Write(fd, file_buf, NETCONFIG_SIZE);
  ISFS_Close(fd);
  ISFS_Deinitialize();
  if (rc < 0)
    return rc;

  return rc == NETCONFIG_SIZE ? 0 : FS_EBADSIZE;
}

static void ip_to_str(const u8 ip[4], char *out)
{
  // An unset address shows as an empty field
  if (ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0) {
    out[0] = 0;
  } else {
    sprintf(out, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  }
}

static bool str_to_ip(const char *s, u8 ip[4])
{
  // An empty field stores as an unset address
  memset(ip, 0, 4);
  if (s[0] == 0)
    return true;

  // Parse four dotted octets with nothing trailing
  for (int i = 0; i < 4; i++) {
    char *end;
    long v = strtol(s, &end, 10);
    if (end == s || v < 0 || v > 255)
      return false;
    ip[i] = v;
    s     = end;
    if (i < 3) {
      if (*s != '.')
        return false;
      s++;
    }
  }

  return *s == 0;
}

static bool is_wep(int encryption, int *bytes)
{
  *bytes = encryption == NETCONFIG_ENC_WEP64 ? 5 : 13;
  return encryption == NETCONFIG_ENC_WEP64 || encryption == NETCONFIG_ENC_WEP128;
}

void netconfig_profile_to_form(const struct netconfig_profile *profile, struct netconfig_form *form)
{
  // Pull the flags apart, treating an empty slot as a fresh DHCP profile
  memset(form, 0, sizeof(*form));
  form->selected = profile->flags & NETCONFIG_FLAG_SELECTED;
  form->wired    = profile->flags & NETCONFIG_FLAG_WIRED;
  form->dhcp_ip  = profile->flags & NETCONFIG_FLAG_DHCP_IP;
  form->dhcp_dns = profile->flags & NETCONFIG_FLAG_DHCP_DNS;
  if (profile->flags == 0) {
    form->dhcp_ip  = true;
    form->dhcp_dns = true;
  }

  // Copy the SSID, which is stored without a terminator
  int n = profile->ssid_len < 32 ? profile->ssid_len : 32;
  memcpy(form->ssid, profile->ssid, n);
  form->encryption = profile->encryption;

  // Show a WEP key as hex digits and a WPA key as its passphrase
  int bytes;
  if (is_wep(form->encryption, &bytes)) {
    for (int i = 0; i < bytes; i++)
      sprintf(form->key + i * 2, "%02X", profile->key[i]);
  } else if (form->encryption != NETCONFIG_ENC_OPEN) {
    n = profile->key_len < 64 ? profile->key_len : 64;
    memcpy(form->key, profile->key, n);
  }

  // Format the addresses and the MTU
  ip_to_str(profile->ip, form->ip);
  ip_to_str(profile->netmask, form->netmask);
  ip_to_str(profile->gateway, form->gateway);
  ip_to_str(profile->dns1, form->dns1);
  ip_to_str(profile->dns2, form->dns2);
  if (profile->mtu != 0)
    sprintf(form->mtu, "%u", (unsigned)(profile->mtu > 9999 ? 9999 : profile->mtu));
}

const char *netconfig_form_to_profile(const struct netconfig_form *form, struct netconfig_profile *profile)
{
  // Work on a copy so a validation failure leaves the profile untouched
  struct netconfig_profile p = *profile;

  // Rebuild the flags, dropping the test result since the settings changed
  p.flags &= NETCONFIG_FLAG_PROXY | NETCONFIG_FLAG_PMTU;
  if (form->selected)
    p.flags |= NETCONFIG_FLAG_SELECTED;
  if (form->wired)
    p.flags |= NETCONFIG_FLAG_WIRED;
  if (form->dhcp_ip)
    p.flags |= NETCONFIG_FLAG_DHCP_IP;
  if (form->dhcp_dns)
    p.flags |= NETCONFIG_FLAG_DHCP_DNS;

  // Parse the addresses and check that the manual modes have what they need
  if (!str_to_ip(form->ip, p.ip) || !str_to_ip(form->netmask, p.netmask) || !str_to_ip(form->gateway, p.gateway))
    return "IP address, subnet mask or gateway is not valid";
  if (!str_to_ip(form->dns1, p.dns1) || !str_to_ip(form->dns2, p.dns2))
    return "DNS address is not valid";
  if (!form->dhcp_ip && (form->ip[0] == 0 || form->netmask[0] == 0))
    return "Manual IP needs an address and a subnet mask";
  if (!form->dhcp_dns && form->dns1[0] == 0)
    return "Manual DNS needs a primary DNS address";

  // Parse the MTU, where 0 or an empty field means the default
  p.mtu = 0;
  if (form->mtu[0] != 0) {
    char *end;
    long mtu = strtol(form->mtu, &end, 10);
    if (*end != 0 || mtu < 0 || (mtu != 0 && (mtu < 68 || mtu > 1500)))
      return "MTU must be 0 or between 68 and 1500";
    p.mtu = mtu;
  }

  // Clear the wireless fields, which stay zero on a wired profile
  p.rateset = 0;
  p.method  = 0;
  memset(p.ssid, 0, sizeof(p.ssid));
  p.ssid_len   = 0;
  p.encryption = 0;
  p.key_len    = 0;
  memset(p.key, 0, sizeof(p.key));
  if (form->wired) {
    *profile = p;
    return NULL;
  }

  // Store the SSID without a terminator
  size_t ssid_len = strlen(form->ssid);
  if (ssid_len == 0 || ssid_len > 32)
    return "SSID must be 1 to 32 characters";
  memcpy(p.ssid, form->ssid, ssid_len);
  p.ssid_len   = ssid_len;
  p.encryption = form->encryption;

  // Store a WEP key as raw bytes in all four key slots, or a WPA key as its passphrase
  size_t key_len = strlen(form->key);
  int bytes;
  if (is_wep(form->encryption, &bytes)) {
    if (key_len != (size_t)bytes * 2)
      return bytes == 5 ? "WEP 64 key must be 10 hex digits" : "WEP 128 key must be 26 hex digits";
    for (int i = 0; i < bytes; i++) {
      if (!isxdigit((unsigned char)form->key[i * 2]) || !isxdigit((unsigned char)form->key[i * 2 + 1]))
        return "WEP key must be hex digits only";
      char pair[3] = {form->key[i * 2], form->key[i * 2 + 1], 0};
      p.key[i]     = strtol(pair, NULL, 16);
    }
    for (int i = 1; i < 4; i++)
      memcpy(p.key + i * bytes, p.key, bytes);
  } else if (form->encryption != NETCONFIG_ENC_OPEN) {
    if (key_len < 8 || key_len > 63)
      return "WPA passphrase must be 8 to 63 characters";
    memcpy(p.key, form->key, key_len);
    p.key_len = key_len;
  }

  *profile = p;
  return NULL;
}

const char *netconfig_encryption_name(int encryption)
{
  switch (encryption) {
    case NETCONFIG_ENC_OPEN:
      return "None";
    case NETCONFIG_ENC_WEP64:
      return "WEP 64";
    case NETCONFIG_ENC_WEP128:
      return "WEP 128";
    case NETCONFIG_ENC_WPA_TKIP:
      return "WPA-PSK (TKIP)";
    case NETCONFIG_ENC_WPA2_AES:
      return "WPA2-PSK (AES)";
    case NETCONFIG_ENC_WPA_AES:
      return "WPA-PSK (AES)";
    default:
      return "Unknown";
  }
}

int netconfig_encryption_next(int encryption)
{
  // Step to the next entry in the menu order, wrapping at the end
  int count = sizeof(encryption_order) / sizeof(encryption_order[0]);
  for (int i = 0; i < count; i++)
    if (encryption_order[i] == encryption)
      return encryption_order[(i + 1) % count];

  return NETCONFIG_ENC_OPEN;
}
