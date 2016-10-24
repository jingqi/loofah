/**
 * See http://www.snip2code.com/Snippet/609281/This-provides-the-endian-conversion-func
 */

#ifndef ___HEADFILE_431A1781_93AB_4754_A960_AE54BBB09855_
#define ___HEADFILE_431A1781_93AB_4754_A960_AE54BBB09855_

#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_WINDOWS
#   if NUT_PLATFORM_CC_MINGW
#       include <endian.h>
#   else
#       include <stdint.h>
#       include <winsock.h>
        static_assert(sizeof(short) == sizeof(int16_t), "整数长度问题");
        static_assert(sizeof(long) == sizeof(int32_t), "整数长度问题");
#       define htobe16(x) htons(x)
#       define be16toh(x) ntohs(x)
#       define htobe32(x) htonl(x)
#       define be32toh(x) ntohl(x)
#   endif
#elif NUT_PLATFORM_OS_MAC
#   include <libkern/OSByteOrder.h>

#   define htobe16(x) OSSwapHostToBigInt16(x)
#   define htobe32(x) OSSwapHostToBigInt32(x)
#   define htobe64(x) OSSwapHostToBigInt64(x)

#   define be16toh(x) OSSwapBigToHostInt16(x)
#   define be32toh(x) OSSwapBigToHostInt32(x)
#   define be64toh(x) OSSwapBigToHostInt64(x)

#else
#   include <endian.h>
#endif

#endif
