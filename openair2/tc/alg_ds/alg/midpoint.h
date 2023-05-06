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


// Similar algorithm to std::midpoint introduced in C++20
// https://en.cppreference.com/w/cpp/numeric/midpoint


#ifndef MIR_MIDPOINT_H
#define MIR_MIDPOINT_H 

#include <stdint.h>


// Unsigned

uint8_t midpoint_u8(uint8_t a, uint8_t b);

uint16_t midpoint_u16(uint16_t a, uint16_t b);

uint32_t midpoint_u32(uint32_t a, uint32_t b);

uint64_t midpoint_u64(uint64_t a, uint64_t b);


// Signed

int8_t midpoint_i8(int8_t a, int8_t b);

int16_t midpoint_i16(int16_t a, int16_t b);

int32_t midpoint_i32(int32_t a, int32_t b);

int64_t midpoint_i64(int64_t a, int64_t b);



#endif

