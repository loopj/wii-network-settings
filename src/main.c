#include <gccore.h>
#include <network.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "input.h"
#include "keyboard.h"
#include "netconfig.h"

enum field {
  FIELD_SELECTED,
  FIELD_WIRED,
  FIELD_SSID,
  FIELD_ENCRYPTION,
  FIELD_KEY,
  FIELD_DHCP_IP,
  FIELD_IP,
  FIELD_NETMASK,
  FIELD_GATEWAY,
  FIELD_DHCP_DNS,
  FIELD_DNS1,
  FIELD_DNS2,
  FIELD_MTU,
  FIELD_SAVE,
  FIELD_TEST,
  FIELD_CLEAR,
  FIELD_BACK,
  FIELD_COUNT,
};

static struct netconfig config;

static void video_init(void)
{
  // Put a text console on the preferred video mode
  VIDEO_Init();
  GXRModeObj *rmode = VIDEO_GetPreferredMode(NULL);
  void *xfb         = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
  console_init(xfb, 20, 20, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth * VI_DISPLAY_PIX_SZ);

  // Show it
  VIDEO_Configure(rmode);
  VIDEO_SetNextFramebuffer(xfb);
  VIDEO_SetBlack(FALSE);
  VIDEO_Flush();
  VIDEO_WaitVSync();
  if (rmode->viTVMode & VI_NON_INTERLACE)
    VIDEO_WaitVSync();
}

static void show_message(const char *fmt, ...)
{
  // Print the message below whatever is on screen
  va_list ap;
  va_start(ap, fmt);
  printf("\n");
  vprintf(fmt, ap);
  va_end(ap);
  printf("\n\nPress A to continue\n");

  // Wait for it to be dismissed
  while (!(input_wait_press() & (INPUT_A | INPUT_B)))
    ;
}

static bool confirm(const char *question)
{
  printf("\n%s\n\nPress A to confirm, B to go back\n", question);
  for (;;) {
    u32 b = input_wait_press();
    if (b & INPUT_A)
      return true;
    if (b & INPUT_B)
      return false;
  }
}

static void describe_profile(const struct netconfig_profile *p, char *out, int size)
{
  // An empty slot has no flags and no SSID
  if (p->flags == 0 && p->ssid_len == 0) {
    snprintf(out, size, "Not set");
    return;
  }

  // Summarize the profile on one line
  char ssid[33] = {0};
  memcpy(ssid, p->ssid, p->ssid_len < 32 ? p->ssid_len : 32);
  if (p->flags & NETCONFIG_FLAG_WIRED) {
    snprintf(out, size, "Wired, %s IP%s", (p->flags & NETCONFIG_FLAG_DHCP_IP) ? "automatic" : "manual",
             (p->flags & NETCONFIG_FLAG_SELECTED) ? " (in use)" : "");
  } else {
    snprintf(out, size, "Wireless \"%s\", %s%s", ssid, netconfig_encryption_name(p->encryption),
             (p->flags & NETCONFIG_FLAG_SELECTED) ? " (in use)" : "");
  }
}

static int list_visible_fields(const struct netconfig_form *f, enum field *out)
{
  // Hide the fields that do not apply to the current choices
  int n    = 0;
  out[n++] = FIELD_SELECTED;
  out[n++] = FIELD_WIRED;
  if (!f->wired) {
    out[n++] = FIELD_SSID;
    out[n++] = FIELD_ENCRYPTION;
    if (f->encryption != NETCONFIG_ENC_OPEN)
      out[n++] = FIELD_KEY;
  }
  out[n++] = FIELD_DHCP_IP;
  if (!f->dhcp_ip) {
    out[n++] = FIELD_IP;
    out[n++] = FIELD_NETMASK;
    out[n++] = FIELD_GATEWAY;
  }
  out[n++] = FIELD_DHCP_DNS;
  if (!f->dhcp_dns) {
    out[n++] = FIELD_DNS1;
    out[n++] = FIELD_DNS2;
  }
  out[n++] = FIELD_MTU;

  // The actions always follow
  out[n++] = FIELD_SAVE;
  out[n++] = FIELD_TEST;
  out[n++] = FIELD_CLEAR;
  out[n++] = FIELD_BACK;

  return n;
}

static const char *field_label(enum field f)
{
  switch (f) {
    case FIELD_SELECTED:
      return "Use this connection";
    case FIELD_WIRED:
      return "Connection type";
    case FIELD_SSID:
      return "SSID";
    case FIELD_ENCRYPTION:
      return "Security";
    case FIELD_KEY:
      return "Key";
    case FIELD_DHCP_IP:
      return "IP address";
    case FIELD_IP:
      return "  Address";
    case FIELD_NETMASK:
      return "  Subnet mask";
    case FIELD_GATEWAY:
      return "  Gateway";
    case FIELD_DHCP_DNS:
      return "DNS";
    case FIELD_DNS1:
      return "  Primary DNS";
    case FIELD_DNS2:
      return "  Secondary DNS";
    case FIELD_MTU:
      return "MTU";
    case FIELD_SAVE:
      return "Save";
    case FIELD_TEST:
      return "Save and test connection";
    case FIELD_CLEAR:
      return "Clear this connection";
    case FIELD_BACK:
      return "Back";
    default:
      return "";
  }
}

static const char *field_value(const struct netconfig_form *f, enum field field)
{
  switch (field) {
    case FIELD_SELECTED:
      return f->selected ? "Yes" : "No";
    case FIELD_WIRED:
      return f->wired ? "Wired" : "Wireless";
    case FIELD_SSID:
      return f->ssid;
    case FIELD_ENCRYPTION:
      return netconfig_encryption_name(f->encryption);
    case FIELD_KEY:
      return f->key;
    case FIELD_DHCP_IP:
      return f->dhcp_ip ? "Automatic" : "Manual";
    case FIELD_IP:
      return f->ip;
    case FIELD_NETMASK:
      return f->netmask;
    case FIELD_GATEWAY:
      return f->gateway;
    case FIELD_DHCP_DNS:
      return f->dhcp_dns ? "Automatic" : "Manual";
    case FIELD_DNS1:
      return f->dns1;
    case FIELD_DNS2:
      return f->dns2;
    case FIELD_MTU:
      return f->mtu[0] ? f->mtu : "Default";
    default:
      return NULL;
  }
}

static void draw_profile_editor(int index, const struct netconfig_form *f, const enum field *fields, int n, int cursor)
{
  printf("\x1b[2JConnection %d\n\n", index + 1);

  // List the fields with their values and the actions without, marking the cursor
  for (int i = 0; i < n; i++) {
    const char *value = field_value(f, fields[i]);
    if (fields[i] == FIELD_SAVE)
      printf("\n");
    if (value) {
      printf("%c %-24s %s\n", i == cursor ? '>' : ' ', field_label(fields[i]), value);
    } else {
      printf("%c %s\n", i == cursor ? '>' : ' ', field_label(fields[i]));
    }
  }

  printf("\nD-pad move, A change, B back\n");
}

static bool save_profile(int index, struct netconfig_form *f)
{
  // Validate and encode the form into its slot
  const char *err = netconfig_form_to_profile(f, &config.profile[index]);
  if (err) {
    show_message("%s", err);
    return false;
  }

  // Only one profile can be in use at a time
  if (f->selected)
    for (int i = 0; i < NETCONFIG_PROFILES; i++)
      if (i != index)
        config.profile[i].flags &= ~NETCONFIG_FLAG_SELECTED;

  // Write the file
  s32 rc = netconfig_save(&config);
  if (rc < 0) {
    show_message("Writing the settings to NAND failed (%d)", (int)rc);
    return false;
  }

  return true;
}

static bool clear_profile(int index)
{
  if (!confirm("Clear this connection?"))
    return false;

  // Zero the slot and write the file
  memset(&config.profile[index], 0, sizeof(config.profile[index]));
  s32 rc = netconfig_save(&config);
  if (rc < 0) {
    show_message("Writing the settings to NAND failed (%d)", (int)rc);
    return false;
  }

  return true;
}

static void test_connection(int index)
{
  // Bring the network up with the profile that was just saved
  printf("\x1b[2JConnecting with connection %d, this can take up to a minute...\n", index + 1);
  s32 rc = net_init();
  if (rc < 0) {
    net_deinit();
    show_message("Connection failed (%d)", (int)rc);
    return;
  }

  // Note the address it got and bring the network back down
  u32 host = net_gethostip();
  net_deinit();
  char ip[16];
  snprintf(ip, sizeof(ip), "%u.%u.%u.%u", (unsigned)(host >> 24) & 0xff, (unsigned)(host >> 16) & 0xff,
           (unsigned)(host >> 8) & 0xff, (unsigned)host & 0xff);

  // Record the pass the same way the System Menu does
  config.profile[index].flags |= NETCONFIG_FLAG_TESTED;
  rc = netconfig_save(&config);
  if (rc < 0) {
    show_message("Connected as %s but writing the settings to NAND failed (%d)", ip, (int)rc);
  } else {
    show_message("Connection test passed, IP address %s", ip);
  }
}

static void edit_text_field(const char *title, char *text, int capacity, enum keyboard_mode mode)
{
  // Edit a copy so cancelling leaves the field alone
  char copy[65];
  strncpy(copy, text, sizeof(copy) - 1);
  copy[sizeof(copy) - 1] = 0;
  if (keyboard_edit(title, copy, capacity, mode))
    strcpy(text, copy);
}

static void edit_profile(int index)
{
  struct netconfig_form f;
  netconfig_profile_to_form(&config.profile[index], &f);
  enum field fields[FIELD_COUNT];
  int cursor = 0;
  for (;;) {
    // Redraw, since the visible fields depend on the current choices
    int n = list_visible_fields(&f, fields);
    if (cursor >= n)
      cursor = n - 1;
    draw_profile_editor(index, &f, fields, n, cursor);
    u32 b = input_wait_press();

    // Move the cursor or leave
    if (b & INPUT_UP)
      cursor = cursor > 0 ? cursor - 1 : n - 1;
    if (b & INPUT_DOWN)
      cursor = cursor < n - 1 ? cursor + 1 : 0;
    if (b & (INPUT_B | INPUT_HOME))
      return;
    if (!(b & INPUT_A))
      continue;

    // Change the field under the cursor
    switch (fields[cursor]) {
      case FIELD_SELECTED:
        f.selected = !f.selected;
        break;
      case FIELD_WIRED:
        f.wired = !f.wired;
        break;
      case FIELD_SSID:
        edit_text_field("SSID", f.ssid, sizeof(f.ssid), KEYBOARD_MODE_TEXT);
        break;
      case FIELD_ENCRYPTION:
        f.encryption = netconfig_encryption_next(f.encryption);
        break;
      case FIELD_KEY:
        edit_text_field(f.encryption == NETCONFIG_ENC_WEP64 || f.encryption == NETCONFIG_ENC_WEP128
                          ? "WEP key (hex digits)"
                          : "WPA passphrase",
                        f.key, sizeof(f.key), KEYBOARD_MODE_TEXT);
        break;
      case FIELD_DHCP_IP:
        f.dhcp_ip = !f.dhcp_ip;
        break;
      case FIELD_IP:
        edit_text_field("IP address", f.ip, sizeof(f.ip), KEYBOARD_MODE_NUMERIC);
        break;
      case FIELD_NETMASK:
        edit_text_field("Subnet mask", f.netmask, sizeof(f.netmask), KEYBOARD_MODE_NUMERIC);
        break;
      case FIELD_GATEWAY:
        edit_text_field("Gateway", f.gateway, sizeof(f.gateway), KEYBOARD_MODE_NUMERIC);
        break;
      case FIELD_DHCP_DNS:
        f.dhcp_dns = !f.dhcp_dns;
        break;
      case FIELD_DNS1:
        edit_text_field("Primary DNS", f.dns1, sizeof(f.dns1), KEYBOARD_MODE_NUMERIC);
        break;
      case FIELD_DNS2:
        edit_text_field("Secondary DNS", f.dns2, sizeof(f.dns2), KEYBOARD_MODE_NUMERIC);
        break;
      case FIELD_MTU:
        edit_text_field("MTU", f.mtu, sizeof(f.mtu), KEYBOARD_MODE_NUMERIC);
        break;
      case FIELD_SAVE:
        if (save_profile(index, &f))
          show_message("Settings saved");
        break;
      case FIELD_TEST:
        // The console only connects with the profile in use
        f.selected = true;
        if (save_profile(index, &f))
          test_connection(index);
        break;
      case FIELD_CLEAR:
        if (clear_profile(index))
          return;
        break;
      case FIELD_BACK:
        return;
      default:
        break;
    }
  }
}

static void main_menu(void)
{
  int cursor = 0;
  for (;;) {
    // List the three slots and the exit item, marking the cursor
    printf("\x1b[2JWii Network Settings\n\n");
    for (int i = 0; i < NETCONFIG_PROFILES; i++) {
      char text[80];
      describe_profile(&config.profile[i], text, sizeof(text));
      printf("%c Connection %d  %s\n", cursor == i ? '>' : ' ', i + 1, text);
    }
    printf("\n%c Exit\n\nD-pad move, A open, HOME exit\n", cursor == NETCONFIG_PROFILES ? '>' : ' ');
    u32 b = input_wait_press();

    // Move the cursor, open a slot or leave
    if (b & INPUT_UP)
      cursor = cursor > 0 ? cursor - 1 : NETCONFIG_PROFILES;
    if (b & INPUT_DOWN)
      cursor = cursor < NETCONFIG_PROFILES ? cursor + 1 : 0;
    if ((b & INPUT_HOME) || ((b & INPUT_A) && cursor == NETCONFIG_PROFILES))
      return;
    if (b & INPUT_A)
      edit_profile(cursor);
  }
}

static void quit(void)
{
  // Dolphin crashes in libogc's exit path, so power off there instead, which stops the emulation
  s32 fd = IOS_Open("/dev/dolphin", 0);
  if (fd < 0)
    return;
  IOS_Close(fd);
  SYS_ResetSystem(SYS_POWEROFF, 0, 0);
}

int main(void)
{
  video_init();
  input_init();

  // Load the file, then edit until the user leaves
  printf("\x1b[2JWii Network Settings\n\nReading settings from NAND...\n");
  s32 rc = netconfig_load(&config);
  if (rc < 0) {
    show_message("Reading the network settings from NAND failed (%d)", (int)rc);
  } else {
    main_menu();
  }

  quit();
  return 0;
}
