/**
 * Author......: See docs/credits.txt
 * License.....: MIT
 */

#include "common.h"
#include "types.h"
#include "convert.h"

/*
  Source:
   http://www.unicode.org/versions/Unicode7.0.0/UnicodeStandard-7.0.pdf
   page 124, 3.9 "Unicode Encoding Forms", "UTF-8"

  Table 3-7. Well-Formed UTF-8 Byte Sequences
  -----------------------------------------------------------------------------
  |  Code Points        | First Byte | Second Byte | Third Byte | Fourth Byte |
  |  U+0000..U+007F     |     00..7F |             |            |             |
  |  U+0080..U+07FF     |     C2..DF |      80..BF |            |             |
  |  U+0800..U+0FFF     |         E0 |      A0..BF |     80..BF |             |
  |  U+1000..U+CFFF     |     E1..EC |      80..BF |     80..BF |             |
  |  U+D000..U+D7FF     |         ED |      80..9F |     80..BF |             |
  |  U+E000..U+FFFF     |     EE..EF |      80..BF |     80..BF |             |
  |  U+10000..U+3FFFF   |         F0 |      90..BF |     80..BF |      80..BF |
  |  U+40000..U+FFFFF   |     F1..F3 |      80..BF |     80..BF |      80..BF |
  |  U+100000..U+10FFFF |         F4 |      80..8F |     80..BF |      80..BF |
  -----------------------------------------------------------------------------
*/

static bool printable_utf8 (const u8 *buf, const size_t len)
{
  // there's 9 different code point types for utf8 and ...

  const int cp_types[256] =
  {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     2,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  4,  5,  5,
     6,  7,  7,  7,  8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
  };

  // ... they can be directly translated into a fixed length sequence of bytes

  const size_t cp_lens[9] = { 1, 2, 3, 3, 3, 3, 4, 4, 4 };

  for (size_t pos = 0; pos < len; pos++)
  {
    const u8 c0 = buf[pos];

    const int cp_type = cp_types[c0];

    if (cp_type == -1) return false;

    // make sure to not read outside the buffer

    const size_t cp_len = cp_lens[cp_type];

    if ((pos + cp_len) > len) return false;

    // multibyte from here

    if (cp_len >= 2)
    {
      pos++;

      const u8 c1 = buf[pos];

      switch (cp_type)
      {
        case 2:   if ((c1 < 0xa0) || (c1 > 0xbf)) return false; break;
        case 4:   if ((c1 < 0x80) || (c1 > 0x9f)) return false; break;
        case 6:   if ((c1 < 0x90) || (c1 > 0xbf)) return false; break;
        case 8:   if ((c1 < 0x80) || (c1 > 0x8f)) return false; break;
        default:  if ((c1 < 0x80) || (c1 > 0xbf)) return false; break;
      }

      for (size_t j = 2; j < cp_len; j++)
      {
        pos++;

        const u8 cx = buf[pos];

        if ((cx < 0x80) || (cx > 0xbf)) return false;
      }
    }
  }

  return true;
}

static bool printable_ascii (const u8 *buf, const size_t len)
{
  for (size_t i = 0; i < len; i++)
  {
    const u8 c = buf[i];

    if (c < 0x20) return false;
    if (c > 0x7e) return false;
  }

  return true;
}

static bool matches_separator (const u8 *buf, const size_t len, const char separator)
{
  for (size_t i = 0; i < len; i++)
  {
    const char c = (char) buf[i];

    if (c == separator) return true;
  }

  return false;
}

bool is_hexify (const u8 *buf, const size_t len)
{
  if (len < 6) return false; // $HEX[] = 6

  // length of the hex string must be a multiple of 2
  // and the length of "$HEX[]" is 6 (also an even length)
  // Therefore the overall length must be an even number:

  if ((len & 1) == 1) return false;

  if (buf[0]       != '$') return (false);
  if (buf[1]       != 'H') return (false);
  if (buf[2]       != 'E') return (false);
  if (buf[3]       != 'X') return (false);
  if (buf[4]       != '[') return (false);
  if (buf[len - 1] != ']') return (false);

  if (is_valid_hex_string (buf + 5, len - 6) == false) return false;

  return true;
}

size_t exec_unhexify (const u8 *in_buf, const size_t in_len, u8 *out_buf, const size_t out_sz)
{
  size_t i, j;

  for (i = 0, j = 5; j < in_len - 1; i += 1, j += 2)
  {
    const u8 c = hex_to_u8 (&in_buf[j]);

    out_buf[i] = c;
  }

  memset (out_buf + i, 0, out_sz - i);

  return (i);
}

bool need_hexify (const u8 *buf, const size_t len, const char separator, bool always_ascii)
{
  bool rc = false;

  if (always_ascii == true)
  {
    if (printable_ascii (buf, len) == false)
    {
      rc = true;
    }
  }
  else
  {
    if (printable_utf8 (buf, len) == false)
    {
      rc = true;
    }
  }

  if (rc == false)
  {
    if (matches_separator (buf, len, separator) == true)
    {
      rc = true;
    }
  }

  // also test if the password is of the format $HEX[]:

  if (rc == false)
  {
    if (is_hexify (buf, len))
    {
      rc = true;
    }
  }

  return rc;
}

void exec_hexify (const u8 *buf, const size_t len, u8 *out)
{
  const size_t max_len = (len > PW_MAX) ? PW_MAX : len;

  for (int i = (int) max_len - 1, j = i * 2; i >= 0; i -= 1, j -= 2)
  {
    u8_to_hex (buf[i], out + j);
  }

  out[max_len * 2] = 0;
}

bool is_valid_base64a_string (const u8 *s, const size_t len)
{
  for (size_t i = 0; i < len; i++)
  {
    const u8 c = s[i];

    if (is_valid_base64a_char (c) == false) return false;
  }

  return true;
}

bool is_valid_base64a_char (const u8 c)
{
  if ((c >= '0') && (c <= '9')) return true;
  if ((c >= 'A') && (c <= 'Z')) return true;
  if ((c >= 'a') && (c <= 'z')) return true;

  if (c == '+') return true;
  if (c == '/') return true;
  if (c == '=') return true;

  return false;
}

bool is_valid_base64b_string (const u8 *s, const size_t len)
{
  for (size_t i = 0; i < len; i++)
  {
    const u8 c = s[i];

    if (is_valid_base64b_char (c) == false) return false;
  }

  return true;
}

bool is_valid_base64b_char (const u8 c)
{
  if ((c >= '0') && (c <= '9')) return true;
  if ((c >= 'A') && (c <= 'Z')) return true;
  if ((c >= 'a') && (c <= 'z')) return true;

  if (c == '.') return true;
  if (c == '/') return true;
  if (c == '=') return true;

  return false;
}

bool is_valid_base64c_string (const u8 *s, const size_t len)
{
  for (size_t i = 0; i < len; i++)
  {
    const u8 c = s[i];

    if (is_valid_base64c_char (c) == false) return false;
  }

  return true;
}

bool is_valid_base64c_char (const u8 c)
{
  if ((c >= '0') && (c <= '9')) return true;
  if ((c >= 'A') && (c <= 'Z')) return true;
  if ((c >= 'a') && (c <= 'z')) return true;

  if (c == '_') return true;
  if (c == '-') return true;
  if (c == '=') return true;

  return false;
}

bool is_valid_hex_string (const u8 *s, const size_t len)
{
  for (size_t i = 0; i < len; i++)
  {
    const u8 c = s[i];

    if (is_valid_hex_char (c) == false) return false;
  }

  return true;
}

bool is_valid_hex_char (const u8 c)
{
  if ((c >= '0') && (c <= '9')) return true;
  if ((c >= 'A') && (c <= 'F')) return true;
  if ((c >= 'a') && (c <= 'f')) return true;

  return false;
}

bool is_valid_digit_string (const u8 *s, const size_t len)
{
  for (size_t i = 0; i < len; i++)
  {
    const u8 c = s[i];

    if (is_valid_digit_char (c) == false) return false;
  }

  return true;
}

bool is_valid_digit_char (const u8 c)
{
  if ((c >= '0') && (c <= '9')) return true;

  return false;
}

u8 hex_convert (const u8 c)
{
  return (c & 15) + (c >> 6) * 9;
}

u8 hex_to_u8 (const u8 hex[2])
{
  u8 v = 0;

  v |= (hex_convert (hex[1]) << 0);
  v |= (hex_convert (hex[0]) << 4);

  return (v);
}

u32 hex_to_u32 (const u8 hex[8])
{
  u32 v = 0;

  v |= ((u32) hex_convert (hex[1]) <<  0);
  v |= ((u32) hex_convert (hex[0]) <<  4);
  v |= ((u32) hex_convert (hex[3]) <<  8);
  v |= ((u32) hex_convert (hex[2]) << 12);
  v |= ((u32) hex_convert (hex[5]) << 16);
  v |= ((u32) hex_convert (hex[4]) << 20);
  v |= ((u32) hex_convert (hex[7]) << 24);
  v |= ((u32) hex_convert (hex[6]) << 28);

  return (v);
}

u64 hex_to_u64 (const u8 hex[16])
{
  u64 v = 0;

  v |= ((u64) hex_convert (hex[ 1]) <<  0);
  v |= ((u64) hex_convert (hex[ 0]) <<  4);
  v |= ((u64) hex_convert (hex[ 3]) <<  8);
  v |= ((u64) hex_convert (hex[ 2]) << 12);
  v |= ((u64) hex_convert (hex[ 5]) << 16);
  v |= ((u64) hex_convert (hex[ 4]) << 20);
  v |= ((u64) hex_convert (hex[ 7]) << 24);
  v |= ((u64) hex_convert (hex[ 6]) << 28);
  v |= ((u64) hex_convert (hex[ 9]) << 32);
  v |= ((u64) hex_convert (hex[ 8]) << 36);
  v |= ((u64) hex_convert (hex[11]) << 40);
  v |= ((u64) hex_convert (hex[10]) << 44);
  v |= ((u64) hex_convert (hex[13]) << 48);
  v |= ((u64) hex_convert (hex[12]) << 52);
  v |= ((u64) hex_convert (hex[15]) << 56);
  v |= ((u64) hex_convert (hex[14]) << 60);

  return (v);
}

void u8_to_hex (const u8 v, u8 hex[2])
{
  const u8 tbl[0x10] =
  {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    'a', 'b', 'c', 'd', 'e', 'f',
  };

  hex[1] = tbl[v >>  0 & 15];
  hex[0] = tbl[v >>  4 & 15];
}

void u32_to_hex (const u32 v, u8 hex[8])
{
  const u8 tbl[0x10] =
  {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    'a', 'b', 'c', 'd', 'e', 'f',
  };

  hex[1] = tbl[v >>  0 & 15];
  hex[0] = tbl[v >>  4 & 15];
  hex[3] = tbl[v >>  8 & 15];
  hex[2] = tbl[v >> 12 & 15];
  hex[5] = tbl[v >> 16 & 15];
  hex[4] = tbl[v >> 20 & 15];
  hex[7] = tbl[v >> 24 & 15];
  hex[6] = tbl[v >> 28 & 15];
}

void u64_to_hex (const u64 v, u8 hex[16])
{
  const u8 tbl[0x10] =
  {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    'a', 'b', 'c', 'd', 'e', 'f',
  };

  hex[ 1] = tbl[v >>  0 & 15];
  hex[ 0] = tbl[v >>  4 & 15];
  hex[ 3] = tbl[v >>  8 & 15];
  hex[ 2] = tbl[v >> 12 & 15];
  hex[ 5] = tbl[v >> 16 & 15];
  hex[ 4] = tbl[v >> 20 & 15];
  hex[ 7] = tbl[v >> 24 & 15];
  hex[ 6] = tbl[v >> 28 & 15];
  hex[ 9] = tbl[v >> 32 & 15];
  hex[ 8] = tbl[v >> 36 & 15];
  hex[11] = tbl[v >> 40 & 15];
  hex[10] = tbl[v >> 44 & 15];
  hex[13] = tbl[v >> 48 & 15];
  hex[12] = tbl[v >> 52 & 15];
  hex[15] = tbl[v >> 56 & 15];
  hex[14] = tbl[v >> 60 & 15];
}

u8 int_to_base32 (const u8 c)
{
  const u8 tbl[0x20] =
  {
    0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50,
    0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
  };

  return tbl[c];
}

u8 base32_to_int (const u8 c)
{
  if ((c >= 'A') && (c <= 'Z')) return c - 'A';
  if ((c >= '2') && (c <= '7')) return c - '2' + 26;

  return 0;
}

u8 int_to_itoa32 (const u8 c)
{
  const u8 tbl[0x20] =
  {
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
    0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76,
  };

  return tbl[c];
}

u8 itoa32_to_int (const u8 c)
{
  if ((c >= '0') && (c <= '9')) return c - '0';
  if ((c >= 'a') && (c <= 'v')) return c - 'a' + 10;

  return 0;
}

u8 int_to_itoa64 (const u8 c)
{
  const u8 tbl[0x40] =
  {
    0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x41, 0x42, 0x43, 0x44,
    0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54,
    0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a,
    0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a,
  };

  return tbl[c];
}

u8 itoa64_to_int (const u8 c)
{
  const u8 tbl[0x100] =
  {
    0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21,
    0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31,
    0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x00, 0x01,
    0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
    0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a,
    0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x20, 0x21, 0x22, 0x23, 0x24,
    0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34,
    0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x00, 0x01, 0x02, 0x03, 0x04,
    0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14,
    0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24,
    0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34,
    0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x00, 0x01, 0x02, 0x03, 0x04,
    0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14,
    0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24,
    0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34,
    0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x00, 0x01, 0x02, 0x03, 0x04,
  };

  return tbl[c];
}

u8 int_to_base64 (const u8 c)
{
  const u8 tbl[0x40] =
  {
    0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50,
    0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
    0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76,
    0x77, 0x78, 0x79, 0x7a, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x2b, 0x2f,
  };

  return tbl[c];
}

u8 base64_to_int (const u8 c)
{
  const u8 tbl[0x100] =
  {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x3f,
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
    0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };

  return tbl[c];
}

u8 int_to_base64url (const u8 c)
{
  const u8 tbl[0x40] =
  {
    0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50,
    0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
    0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76,
    0x77, 0x78, 0x79, 0x7a, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x2d, 0x5f,
  };

  return tbl[c];
}

u8 base64url_to_int (const u8 c)
{
  const u8 tbl[0x100] =
  {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00,
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
    0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x00, 0x00, 0x00, 0x00, 0x3f,
    0x00, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };

  return tbl[c];
}

u8 int_to_bf64 (const u8 c)
{
  const u8 tbl[0x40] =
  {
    0x2e, 0x2f, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e,
    0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x61, 0x62, 0x63, 0x64,
    0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74,
    0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
  };

  return tbl[c];
}

u8 bf64_to_int (const u8 c)
{
  const u8 tbl[0x100] =
  {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
    0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a,
    0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };

  return tbl[c];
}

u8 int_to_lotus64 (const u8 c)
{
  if (c  < 10) return '0' + c;
  if (c  < 36) return 'A' + c - 10;
  if (c  < 62) return 'a' + c - 36;
  if (c == 62) return '+';
  if (c == 63) return '/';

  return 0;
}

u8 lotus64_to_int (const u8 c)
{
  if ((c >= '0') && (c <= '9')) return c - '0';
  if ((c >= 'A') && (c <= 'Z')) return c - 'A' + 10;
  if ((c >= 'a') && (c <= 'z')) return c - 'a' + 36;
  if (c == '+') return 62;
  if (c == '/') return 63;

  return 0;
}

size_t base32_decode (u8 (*f) (const u8), const u8 *in_buf, const size_t in_len, u8 *out_buf)
{
  const u8 *in_ptr = in_buf;

  u8 *out_ptr = out_buf;

  for (size_t i = 0; i < in_len; i += 8)
  {
    const u8 f0 = ((i + 0) < in_len) ? in_ptr[0] : 0;
    const u8 f1 = ((i + 1) < in_len) ? in_ptr[1] : 0;
    const u8 f2 = ((i + 2) < in_len) ? in_ptr[2] : 0;
    const u8 f3 = ((i + 3) < in_len) ? in_ptr[3] : 0;
    const u8 f4 = ((i + 4) < in_len) ? in_ptr[4] : 0;
    const u8 f5 = ((i + 5) < in_len) ? in_ptr[5] : 0;
    const u8 f6 = ((i + 6) < in_len) ? in_ptr[6] : 0;
    const u8 f7 = ((i + 7) < in_len) ? in_ptr[7] : 0;

    const u8 out_val0 = f (f0 & 0x7f);
    const u8 out_val1 = f (f1 & 0x7f);
    const u8 out_val2 = f (f2 & 0x7f);
    const u8 out_val3 = f (f3 & 0x7f);
    const u8 out_val4 = f (f4 & 0x7f);
    const u8 out_val5 = f (f5 & 0x7f);
    const u8 out_val6 = f (f6 & 0x7f);
    const u8 out_val7 = f (f7 & 0x7f);

    out_ptr[0] =                            ((out_val0 << 3) & 0xf8) | ((out_val1 >> 2) & 0x07);
    out_ptr[1] = ((out_val1 << 6) & 0xc0) | ((out_val2 << 1) & 0x3e) | ((out_val3 >> 4) & 0x01);
    out_ptr[2] =                            ((out_val3 << 4) & 0xf0) | ((out_val4 >> 1) & 0x0f);
    out_ptr[3] = ((out_val4 << 7) & 0x80) | ((out_val5 << 2) & 0x7c) | ((out_val6 >> 3) & 0x03);
    out_ptr[4] =                            ((out_val6 << 5) & 0xe0) | ((out_val7 >> 0) & 0x1f);

    in_ptr  += 8;
    out_ptr += 5;
  }

  size_t tmp_len = 0;

  for (size_t i = 0; i < in_len; i++, tmp_len++)
  {
    if (in_buf[i] != '=') continue;

    break;
  }

  size_t out_len = (tmp_len * 5) / 8;

  return out_len;
}

size_t base32_encode (u8 (*f) (const u8), const u8 *in_buf, const size_t in_len, u8 *out_buf)
{
  const u8 *in_ptr = in_buf;

  u8 *out_ptr = out_buf;

  for (size_t i = 0; i < in_len; i += 5)
  {
    const u8 f0 = ((i + 0) < in_len) ? in_ptr[0] : 0;
    const u8 f1 = ((i + 1) < in_len) ? in_ptr[1] : 0;
    const u8 f2 = ((i + 2) < in_len) ? in_ptr[2] : 0;
    const u8 f3 = ((i + 3) < in_len) ? in_ptr[3] : 0;
    const u8 f4 = ((i + 4) < in_len) ? in_ptr[4] : 0;

    const u8 out_val0 = f (                     ((f0 >> 3) & 0x1f));
    const u8 out_val1 = f (((f0 << 2) & 0x1c) | ((f1 >> 6) & 0x03));
    const u8 out_val2 = f (                     ((f1 >> 1) & 0x1f));
    const u8 out_val3 = f (((f1 << 4) & 0x10) | ((f2 >> 4) & 0x0f));
    const u8 out_val4 = f (((f2 << 1) & 0x1e) | ((f3 >> 7) & 0x01));
    const u8 out_val5 = f (                     ((f3 >> 2) & 0x1f));
    const u8 out_val6 = f (((f3 << 3) & 0x18) | ((f4 >> 5) & 0x07));
    const u8 out_val7 = f (                     ((f4 >> 0) & 0x1f));

    out_ptr[0] = out_val0 & 0x7f;
    out_ptr[1] = out_val1 & 0x7f;
    out_ptr[2] = out_val2 & 0x7f;
    out_ptr[3] = out_val3 & 0x7f;
    out_ptr[4] = out_val4 & 0x7f;
    out_ptr[5] = out_val5 & 0x7f;
    out_ptr[6] = out_val6 & 0x7f;
    out_ptr[7] = out_val7 & 0x7f;

    in_ptr  += 5;
    out_ptr += 8;
  }

  int out_len = (int) (((0.5 + in_len) * 8) / 5); // ceil (in_len * 8 / 5)

  while (out_len % 8)
  {
    out_buf[out_len] = '=';

    out_len++;
  }

  return out_len;
}

size_t base64_decode (u8 (*f) (const u8), const u8 *in_buf, const size_t in_len, u8 *out_buf)
{
  const u8 *in_ptr = in_buf;

  u8 *out_ptr = out_buf;

  for (size_t i = 0; i < in_len; i += 4)
  {
    const u8 f0 = ((i + 0) < in_len) ? in_ptr[0] : 0;
    const u8 f1 = ((i + 1) < in_len) ? in_ptr[1] : 0;
    const u8 f2 = ((i + 2) < in_len) ? in_ptr[2] : 0;
    const u8 f3 = ((i + 3) < in_len) ? in_ptr[3] : 0;

    const u8 out_val0 = f (f0 & 0x7f);
    const u8 out_val1 = f (f1 & 0x7f);
    const u8 out_val2 = f (f2 & 0x7f);
    const u8 out_val3 = f (f3 & 0x7f);

    out_ptr[0] = ((out_val0 << 2) & 0xfc) | ((out_val1 >> 4) & 0x03);
    out_ptr[1] = ((out_val1 << 4) & 0xf0) | ((out_val2 >> 2) & 0x0f);
    out_ptr[2] = ((out_val2 << 6) & 0xc0) | ((out_val3 >> 0) & 0x3f);

    in_ptr  += 4;
    out_ptr += 3;
  }

  size_t tmp_len = 0;

  for (size_t i = 0; i < in_len; i++, tmp_len++)
  {
    if (in_buf[i] != '=') continue;

    break;
  }

  size_t out_len = (tmp_len * 6) / 8;

  return out_len;
}

size_t base64_encode (u8 (*f) (const u8), const u8 *in_buf, const size_t in_len, u8 *out_buf)
{
  const u8 *in_ptr = in_buf;

  u8 *out_ptr = out_buf;

  for (size_t i = 0; i < in_len; i += 3)
  {
    const u8 f0 = ((i + 0) < in_len) ? in_ptr[0] : 0;
    const u8 f1 = ((i + 1) < in_len) ? in_ptr[1] : 0;
    const u8 f2 = ((i + 2) < in_len) ? in_ptr[2] : 0;

    const u8 out_val0 = f (                     ((f0 >> 2) & 0x3f));
    const u8 out_val1 = f (((f0 << 4) & 0x30) | ((f1 >> 4) & 0x0f));
    const u8 out_val2 = f (((f1 << 2) & 0x3c) | ((f2 >> 6) & 0x03));
    const u8 out_val3 = f (                     ((f2 >> 0) & 0x3f));

    out_ptr[0] = out_val0 & 0x7f;
    out_ptr[1] = out_val1 & 0x7f;
    out_ptr[2] = out_val2 & 0x7f;
    out_ptr[3] = out_val3 & 0x7f;

    in_ptr  += 3;
    out_ptr += 4;
  }

  int out_len = (int) (((0.5 + in_len) * 8) / 6); // ceil (in_len * 8 / 6)

  while (out_len % 4)
  {
    out_buf[out_len] = '=';

    out_len++;
  }

  return out_len;
}

void lowercase (u8 *buf, const size_t len)
{
  for (size_t i = 0; i < len; i++) buf[i] = (u8) tolower ((int) buf[i]);
}

void uppercase (u8 *buf, const size_t len)
{
  for (size_t i = 0; i < len; i++) buf[i] = (u8) toupper ((int) buf[i]);
}

u16 v16a_from_v32 (const u32 v32)
{
  vconv32_t v;

  v.v32 = v32;

  return v.v16a;
}

u16 v16b_from_v32 (const u32 v32)
{
  vconv32_t v;

  v.v32 = v32;

  return v.v16b;
}

u32 v32_from_v16ab (const u16 v16a, const u16 v16b)
{
  vconv32_t v;

  v.v16a = v16a;
  v.v16b = v16b;

  return v.v32;
}

u32 v32a_from_v64 (const u64 v64)
{
  vconv64_t v;

  v.v64 = v64;

  return v.v32a;
}

u32 v32b_from_v64 (const u64 v64)
{
  vconv64_t v;

  v.v64 = v64;

  return v.v32b;
}

u64 v64_from_v32ab (const u32 v32a, const u32 v32b)
{
  vconv64_t v;

  v.v32a = v32a;
  v.v32b = v32b;

  return v.v64;
}

int hex_decode (const u8 *in_buf, const int in_len, u8 *out_buf)
{
  for (int i = 0, j = 0; i < in_len; i += 2, j += 1)
  {
    out_buf[j] = hex_to_u8 (&in_buf[i]);
  }

  return in_len / 2;
}

int hex_encode (const u8 *in_buf, const int in_len, u8 *out_buf)
{
  for (int i = 0, j = 0; i < in_len; i += 1, j += 2)
  {
    u8_to_hex (in_buf[i], &out_buf[j]);
  }

  return in_len * 2;
}
