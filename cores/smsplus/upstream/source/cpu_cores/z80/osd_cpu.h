/*******************************************************************************
*                                         *
*  Define size independent data types and operations.               *
*                                         *
*   The following types must be supported by all platforms:             *
*                                         *
*  UINT8  - Unsigned 8-bit Integer    INT8  - Signed 8-bit integer           *
*  UINT16 - Unsigned 16-bit Integer  INT16 - Signed 16-bit integer          *
*  UINT32 - Unsigned 32-bit Integer  INT32 - Signed 32-bit integer          *
*  UINT64 - Unsigned 64-bit Integer  INT64 - Signed 64-bit integer          *
*                                         *
*                                         *
*   The macro names for the artithmatic operations are composed as follows:    *
*                                         *
*   XXX_R_A_B, where XXX - 3 letter operation code (ADD, SUB, etc.)         *
*           R   - The type  of the result                 *
*           A   - The type of operand 1                 *
*               B   - The type of operand 2 (if binary operation)       *
*                                         *
*             Each type is one of: U8,8,U16,16,U32,32,U64,64         *
*                                         *
*******************************************************************************/


#ifndef OSD_CPU_H
#define OSD_CPU_H

#include <stdint.h>

/******************************************************************************
 * Union of uint8_t, uint16_t and uint32_t in native endianess of the target
 * This is used to access bytes and words in a machine independent manner.
 * The upper bytes h2 and h3 normally contain zero (16 bit CPU cores)
 * thus PAIR.d can be used to pass arguments to the memory system
 * which expects 'int' really.
 ******************************************************************************/
typedef union {
#ifdef LSB_FIRST
  struct { uint8_t l,h,h2,h3; } b;
  struct { uint16_t l,h; } w;
#else
  struct { uint8_t h3,h2,h,l; } b;
  struct { uint16_t h,l; } w;
#endif
  uint32_t d;
}  PAIR;

#endif  /* defined OSD_CPU_H */
