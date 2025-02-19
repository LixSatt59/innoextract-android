/*
 * Copyright (C) 2011-2013 Daniel Scharrer
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the author(s) be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

// Taken from Crypto++ and modified to fit the project.
// adler32.cpp - written and placed in the public domain by Wei Dai

#include "crypto/adler32.hpp"

namespace crypto {

void adler32::update(const char * input, size_t length) {
	
	const boost::uint_fast32_t base = 65521;
	
	boost::uint_fast32_t s1 = this->s1;
	boost::uint_fast32_t s2 = this->s2;
	
	if(length % 8 != 0) {
		
		do {
			s1 += boost::uint8_t(*input++);
			s2 += s1;
			length--;
		} while(length % 8 != 0);
		
		if(s1 >= base) {
			s1 -= base;
		}
		
		s2 %= base;
	}
	
	while(length > 0) {
		
		s1 += boost::uint8_t(input[0]), s2 += s1;
		s1 += boost::uint8_t(input[1]), s2 += s1;
		s1 += boost::uint8_t(input[2]), s2 += s1;
		s1 += boost::uint8_t(input[3]), s2 += s1;
		s1 += boost::uint8_t(input[4]), s2 += s1;
		s1 += boost::uint8_t(input[5]), s2 += s1;
		s1 += boost::uint8_t(input[6]), s2 += s1;
		s1 += boost::uint8_t(input[7]), s2 += s1;
		
		length -= 8;
		input += 8;
		
		if(s1 >= base) {
			s1 -= base;
		}
		
		if(length % 0x8000 == 0) {
			s2 %= base;
		}
	}
	
	this->s1 = boost::uint16_t(s1);
	this->s2 = boost::uint16_t(s2);
}

} // namespace crypto
