#pragma once

#include <gctypes.h>
#include <stdbool.h>

// Number of connection profiles in the file
#define NETCONFIG_PROFILES 3

// Size of the file on NAND
#define NETCONFIG_SIZE 0x1b5c

// Profile flag bits
#define NETCONFIG_FLAG_WIRED 0x01
#define NETCONFIG_FLAG_DHCP_IP 0x02
#define NETCONFIG_FLAG_DHCP_DNS 0x04
#define NETCONFIG_FLAG_PROXY 0x10
#define NETCONFIG_FLAG_TESTED 0x20
#define NETCONFIG_FLAG_PMTU 0x40
#define NETCONFIG_FLAG_SELECTED 0x80

// Wireless encryption types stored in the profile
enum netconfig_encryption {
  NETCONFIG_ENC_OPEN     = 0,
  NETCONFIG_ENC_WEP64    = 1,
  NETCONFIG_ENC_WEP128   = 2,
  NETCONFIG_ENC_WPA_TKIP = 4,
  NETCONFIG_ENC_WPA2_AES = 5,
  NETCONFIG_ENC_WPA_AES  = 6,
};

// HTTP proxy settings, stored once for HTTP and once for SSL
struct netconfig_proxy {
  u8 enabled;      // 1 when the proxy is used
  u8 use_auth;     // 1 when the username and password are sent
  u8 pad0[2];      // zero
  u8 host[255];    // proxy host name
  u8 pad1;         // zero
  u16 port;        // proxy port
  u8 username[32]; // proxy username
  u8 pad2;         // zero
  u8 password[32]; // proxy password
} __attribute__((packed));

// One connection profile, 0x91c bytes
struct netconfig_profile {
  u8 flags;                         // NETCONFIG_FLAG bits
  u8 pad0[3];                       // zero
  u8 ip[4];                         // manual IP address
  u8 netmask[4];                    // manual subnet mask
  u8 gateway[4];                    // manual gateway
  u8 dns1[4];                       // manual primary DNS
  u8 dns2[4];                       // manual secondary DNS
  u32 mtu;                          // 0 for the default, otherwise 68 to 1500
  u32 tcp_timeout;                  // 0 for the default
  u8 pad1[4];                       // zero
  struct netconfig_proxy proxy;     // HTTP proxy
  u8 pad2;                          // zero
  struct netconfig_proxy proxy_ssl; // SSL proxy
  u8 pad3[1293];                    // zero
  u16 rateset;                      // wireless rate set, 0 for the default
  u8 method;                        // 0 for a normal profile, 1 for the Nintendo USB access point
  u8 pad4;                          // zero
  u8 ssid[32];                      // SSID without a terminator
  u16 ssid_len;                     // SSID length in bytes
  u16 pad5;                         // zero
  u16 encryption;                   // enum netconfig_encryption
  u16 pad6;                         // zero
  u16 key_len;                      // passphrase length for WPA, WEP key index for WEP
  u8 pad7[2];                       // zero
  u8 key[64];                       // WPA passphrase, or the WEP key bytes repeated four times
  u8 pad8[236];                     // zero
} __attribute__((packed));

// The whole file, 0x1b5c bytes
struct netconfig {
  u32 version;                                          // 0
  u8 media;                                             // 0 none, 1 wireless, 2 wired
  u8 nwc24;                                             // WiiConnect24 permission bits, 0 to 7
  u8 link_timeout;                                      // link timeout in seconds, 7 on a stock console
  u8 pad;                                               // zero
  struct netconfig_profile profile[NETCONFIG_PROFILES]; // connection slots 1 to 3
} __attribute__((packed));

// Editable view of one profile with every value as text
struct netconfig_form {
  bool selected;    // this profile is the one the console uses
  bool wired;       // wired instead of wireless
  char ssid[33];    // SSID
  int encryption;   // enum netconfig_encryption
  char key[65];     // WPA passphrase, or 10 or 26 hex digits for WEP
  bool dhcp_ip;     // obtain the IP address automatically
  char ip[16];      // manual IP address
  char netmask[16]; // manual subnet mask
  char gateway[16]; // manual gateway
  bool dhcp_dns;    // obtain DNS automatically
  char dns1[16];    // manual primary DNS
  char dns2[16];    // manual secondary DNS
  char mtu[5];      // MTU, empty or 0 for the default
};

// Reads the file from NAND, or returns an empty configuration when there is none
s32 netconfig_load(struct netconfig *config);

// Fixes up the header for the selected profile and writes the file to NAND
s32 netconfig_save(struct netconfig *config);

// Fills a form from a profile
void netconfig_profile_to_form(const struct netconfig_profile *profile, struct netconfig_form *form);

// Writes a form into a profile and returns NULL, or returns an error message and leaves the profile alone
const char *netconfig_form_to_profile(const struct netconfig_form *form, struct netconfig_profile *profile);

// Returns the display name of an encryption type
const char *netconfig_encryption_name(int encryption);

// Returns the encryption type that follows the given one in the menu order
int netconfig_encryption_next(int encryption);
