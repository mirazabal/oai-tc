/*
MIT License

Copyright (c) 2022 Mikel Irazabal

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "midpoint.h"

// From GCC std::midpoint function

//_Tp midpoint(_Tp __a, _Tp __b) noexcept
//{
//    using _Up = std::make_unsigned_t<_Tp>;
//    constexpr _Up __bitshift = std::numeric_limits<_Up>::digits - 1;

//    _Up __diff = _Up(__b) - _Up(__a);
//    _Up __sign_bit = __b < __a;

//    _Up __half_diff = (__diff / 2) + (__sign_bit << __bitshift) + (__sign_bit & __diff);

//    return __a + __half_diff;
//}


// Unsigned

uint8_t midpoint_u8(uint8_t a, uint8_t b)
{
  uint8_t const bitshift = 7;   
  uint8_t diff = b - a;
  uint8_t sign_bit = b < a;

  uint8_t half_diff = (diff / 2) + (sign_bit << bitshift) + (sign_bit & diff);

  return a + half_diff;
}

uint16_t midpoint_u16(uint16_t a, uint16_t b)
{
  uint16_t const bitshift = 15;   
  uint16_t diff = b - a;
  uint16_t sign_bit = b < a;

  uint16_t half_diff = (diff / 2) + (sign_bit << bitshift) + (sign_bit & diff);

  return a + half_diff;
}

uint32_t midpoint_u32(uint32_t a, uint32_t b)
{
  uint32_t const bitshift = 31;   
  uint32_t diff = b - a;
  uint32_t sign_bit = b < a;

  uint32_t half_diff = (diff / 2) + (sign_bit << bitshift) + (sign_bit & diff);

  return a + half_diff;
}

uint64_t midpoint_u64(uint64_t a, uint64_t b)
{
  uint64_t const bitshift = 63;   
  uint64_t diff = b - a;
  uint64_t sign_bit = b < a;

  uint64_t half_diff = (diff / 2) + (sign_bit << bitshift) + (sign_bit & diff);

  return a + half_diff;
}

// Signed

int8_t midpoint_i8(int8_t a, int8_t b)
{
  typedef uint8_t uint_t; 
  uint_t const bitshift = (uint_t) (sizeof(uint_t)*8 - 1) ;

  uint_t diff = (uint_t)b - (uint_t)a; 
  uint_t sign_bit = b < a;

  uint_t half_diff = (diff / 2) + (sign_bit << bitshift) + (sign_bit + diff);  

  return a + half_diff;
}

int16_t midpoint_i16(int16_t a, int16_t b)
{
  typedef uint16_t uint_t; 
  uint_t const bitshift = (uint_t) (sizeof(uint_t)*8 - 1) ;

  uint_t diff = (uint_t)b - (uint_t)a; 
  uint_t sign_bit = b < a;

  uint_t half_diff = (diff / 2) + (sign_bit << bitshift) + (sign_bit + diff);  

  return a + half_diff;
}

int32_t midpoint_i32(int32_t a, int32_t b)
{
  typedef uint32_t uint_t; 
  uint_t const bitshift = (uint_t) (sizeof(uint_t)*8 - 1) ;

  uint_t diff = (uint_t)b - (uint_t)a; 
  uint_t sign_bit = b < a;

  uint_t half_diff = (diff / 2) + (sign_bit << bitshift) + (sign_bit + diff);  

  return a + half_diff;
}

int64_t midpoint_i64(int64_t a, int64_t b)
{
  typedef uint64_t uint_t; 
  uint_t const bitshift = (uint_t) (sizeof(uint_t)*8 - 1) ;

  uint_t diff = (uint_t)b - (uint_t)a; 
  uint_t sign_bit = b < a;

  uint_t half_diff = (diff / 2) + (sign_bit << bitshift) + (sign_bit + diff);  

  return a + half_diff;
}


