/*
 * Copyright (C) 2011-2015 Daniel Scharrer
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

#include "setup/data.hpp"

#include "setup/version.hpp"
#include "util/load.hpp"
#include "util/log.hpp"
#include "util/output.hpp"
#include "util/storedenum.hpp"
#include "util/time.hpp"

namespace setup {

void data_entry::load(std::istream & is, const version & version) {
	
	chunk.first_slice = util::load<boost::uint32_t>(is, version.bits);
	chunk.last_slice = util::load<boost::uint32_t>(is, version.bits);
	if(version < INNO_VERSION(4, 0, 0)) {
		if(chunk.first_slice < 1 || chunk.last_slice < 1) {
			log_warning << "Unexpected slice number: " << chunk.first_slice
			            << " to " << chunk.last_slice;
		} else {
			chunk.first_slice--, chunk.last_slice--;
		}
	}
	
	chunk.offset = util::load<boost::uint32_t>(is);
	
	if(version >= INNO_VERSION(4, 0, 1)) {
		file.offset = util::load<boost::uint64_t>(is);
	} else {
		file.offset = 0;
	}
	
	if(version >= INNO_VERSION(4, 0, 0)) {
		file.size = util::load<boost::uint64_t>(is);
		chunk.size = util::load<boost::uint64_t>(is);
	} else {
		file.size = util::load<boost::uint32_t>(is);
		chunk.size = util::load<boost::uint32_t>(is);
	}
	
	if(version >= INNO_VERSION(5, 3, 9)) {
		is.read(file.checksum.sha1, std::streamsize(sizeof(file.checksum.sha1)));
		file.checksum.type = crypto::SHA1;
	} else if(version >= INNO_VERSION(4, 2, 0)) {
		is.read(file.checksum.md5, std::streamsize(sizeof(file.checksum.md5)));
		file.checksum.type = crypto::MD5;
	} else if(version >= INNO_VERSION(4, 0, 1)) {
		file.checksum.crc32 = util::load<boost::uint32_t>(is);
		file.checksum.type = crypto::CRC32;
	} else {
		file.checksum.adler32 = util::load<boost::uint32_t>(is);
		file.checksum.type = crypto::Adler32;
	}
	
	if(version.bits == 16) {
		
		// 16-bit installers use the FAT filetime format
		
		boost::uint16_t time = util::load<boost::uint16_t>(is);
		boost::uint16_t date = util::load<boost::uint16_t>(is);
		
		struct tm t;
		t.tm_sec  = util::get_bits(time,  0,  4) * 2;           // [0, 58]
		t.tm_min  = util::get_bits(time,  5, 10);               // [0, 59]
		t.tm_hour = util::get_bits(time, 11, 15);               // [0, 23]
		t.tm_mday = util::get_bits(date,  0,  4);               // [1, 31]
		t.tm_mon  = util::get_bits(date,  5,  8) - 1;           // [0, 11]
		t.tm_year = util::get_bits(date,  9, 15) + 1980 - 1900; // [80, 199]
		
		timestamp = util::parse_time(t);
		timestamp_nsec = 0;
		
	} else {
		
		// 32-bit installers use the Win32 FILETIME format
		
		boost::int64_t filetime = util::load<boost::int64_t>(is);
		
		static const boost::int64_t FiletimeOffset = 0x19DB1DED53E8000ll;
		if(filetime < FiletimeOffset) {
			log_warning << "Unexpected filetime: " << filetime;
		}
		filetime -= FiletimeOffset;
		
		timestamp = filetime / 10000000;
		timestamp_nsec = boost::uint32_t(filetime % 10000000) * 100;
		
	}
	
	boost::uint32_t file_version_ms = util::load<boost::uint32_t>(is);
	boost::uint32_t file_version_ls = util::load<boost::uint32_t>(is);
	file_version = (boost::uint64_t(file_version_ms) << 32)
	             |  boost::uint64_t(file_version_ls);
	
	options = 0;
	
	stored_flag_reader<flags> flagreader(is, version.bits);
	
	flagreader.add(VersionInfoValid);
	flagreader.add(VersionInfoNotValid);
	if(version >= INNO_VERSION(2, 0, 17) && version < INNO_VERSION(4, 0, 1)) {
		flagreader.add(BZipped);
	}
	if(version >= INNO_VERSION(4, 0, 10)) {
		flagreader.add(TimeStampInUTC);
	}
	if(version >= INNO_VERSION(4, 1, 0)) {
		flagreader.add(IsUninstallerExe);
	}
	if(version >= INNO_VERSION(4, 1, 8)) {
		flagreader.add(CallInstructionOptimized);
	}
	if(version >= INNO_VERSION(4, 2, 0)) {
		flagreader.add(Touch);
	}
	if(version >= INNO_VERSION(4, 2, 2)) {
		flagreader.add(ChunkEncrypted);
	}
	if(version >= INNO_VERSION(4, 2, 5)) {
		flagreader.add(ChunkCompressed);
	} else {
		options |= ChunkCompressed;
	}
	if(version >= INNO_VERSION(5, 1, 13)) {
		flagreader.add(SolidBreak);
	}
	
	options |= flagreader;
	
	if(options & ChunkCompressed) {
		chunk.compression = stream::UnknownCompression;
	} else {
		chunk.compression = stream::Stored;
	}
	if(options & BZipped) {
		options |= ChunkCompressed;
		chunk.compression = stream::BZip2;
	}
	
	chunk.encrypted = ((options & ChunkEncrypted) != 0);
	
	if(options & CallInstructionOptimized) {
		if(version < INNO_VERSION(5, 2, 0)) {
			file.filter = stream::InstructionFilter4108;
		} else if(version < INNO_VERSION(5, 3, 9)) {
			file.filter = stream::InstructionFilter5200;
		} else {
			file.filter = stream::InstructionFilter5309;
		}
	} else {
		file.filter = stream::NoFilter;
	}
}

} // namespace setup;

NAMES(setup::data_entry::flags, "File Location Option",
	"version info valid",
	"version info not valid",
	"timestamp in UTC",
	"is uninstaller exe",
	"call instruction optimized",
	"touch",
	"chunk encrypted",
	"chunk compressed",
	"solid break",
	"bzipped",
)
