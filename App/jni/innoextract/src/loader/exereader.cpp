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

#include "loader/exereader.hpp"

#include <istream>
#include <iomanip>
#include <algorithm>
#include <cstring>
#include <vector>

#include <boost/cstdint.hpp>

#include "util/load.hpp"

namespace loader {

namespace {

struct exe_reader_impl : public exe_reader {
	
	struct header {
		
		//! Number of CoffSection structures following this header after optionalHeaderSize bytes
		boost::uint16_t nsections;
		
		//! Offset of the section table in the file
		boost::uint32_t section_table_offset;
		
		//! Virtual memory address of the resource root table
		boost::uint32_t resource_table_address;
		
	};
	
	struct section {
		
		boost::uint32_t virtual_size; //!< Section size in virtual memory
		boost::uint32_t virtual_address; //!< Base virtual memory address
		
		boost::uint32_t raw_address; //!< Base file offset
		
	};
	
	typedef std::vector<section> section_list;
	
	/*!
	 * Find the entry in a resource table with a given ID.
	 * The input stream is expected to be positioned at the start of the table.
	 * The position if the stream after the function call is undefined.
	 *
	 * \return:
	 *   Highest order bit: 1 = points to another resource table
	 *                      0 = points to a resource leaf
	 *   Remaining 31 bits: Offset to the resource table / leaf relative to
	 *                      the directory start.
	 */
	static boost::uint32_t find_resource_entry(std::istream & is, boost::uint32_t id);
	
	static bool load_header(std::istream & is, header & coff);
	
	static bool load_section_list(std::istream & is, const header & coff, section_list & table);
	
	/*!
	 * Convert a memory address to a file offset according to the given section list.
	 */
	static boost::uint32_t to_file_offset(const section_list & sections, boost::uint32_t address);
	
	static resource find_resource(std::istream & is, boost::uint32_t name,
	                              boost::uint32_t type = TypeData,
	                              boost::uint32_t language = LanguageDefault);
	
};

static const char PE_MAGIC[] = { 'P', 'E', 0, 0 };

bool get_resource_table(boost::uint32_t & entry, boost::uint32_t resource_offset) {
	
	bool is_table = ((entry & (boost::uint32_t(1) << 31)) != 0);
	
	entry &= ~(1 << 31), entry += resource_offset;
	
	return is_table;
}

boost::uint32_t exe_reader_impl::find_resource_entry(std::istream & is, boost::uint32_t needle) {
	
	// skip: characteristics + timestamp + major version + minor version
	if(is.seekg(4 + 4 + 2 + 2, std::ios_base::cur).fail()) {
		return 0;
	}
	
	// Number of named resource entries.
	boost::uint16_t nbnames = util::load<boost::uint16_t>(is);
	
	// Number of id resource entries.
	boost::uint16_t nbids = util::load<boost::uint16_t>(is);
	
	
	// Ignore named resource entries.
	const boost::uint32_t entry_size = 4 + 4; // id / string address + offset
	if(is.seekg(nbnames * entry_size, std::ios_base::cur).fail()) {
		return 0;
	}
	
	for(size_t i = 0; i < nbids; i++) {
		
		boost::uint32_t id = util::load<boost::uint32_t>(is);
		boost::uint32_t offset = util::load<boost::uint32_t>(is);
		if(is.fail()) {
			return 0;
		}
		
		if(id == needle) {
			return offset;
		}
	}
	
	return 0;
}

bool exe_reader_impl::load_header(std::istream & is, header & coff) {
	
	// Skip the DOS stub.
	boost::uint16_t peOffset = util::load<boost::uint16_t>(is.seekg(0x3c));
	if(is.fail()) {
		return false;
	}
	
	char magic[sizeof(PE_MAGIC)];
	if(is.seekg(peOffset).read(magic, std::streamsize(sizeof(magic))).fail()) {
		return false;
	}
	if(std::memcmp(magic, PE_MAGIC, sizeof(PE_MAGIC))) {
		return false;
	}
	
	is.seekg(2, std::ios_base::cur); // machine
	coff.nsections = util::load<boost::uint16_t>(is);
	is.seekg(4 + 4 + 4, std::ios_base::cur); // creation time + symbol table offset + nbsymbols
	boost::uint16_t optional_header_size = util::load<boost::uint16_t>(is);
	is.seekg(2, std::ios_base::cur); // characteristics
	
	coff.section_table_offset = boost::uint32_t(is.tellg()) + optional_header_size;
	
	// Skip the optional header.
	boost::uint16_t optionalHeaderMagic = util::load<boost::uint16_t>(is);
	if(is.fail()) {
		return false;
	}
	if(optionalHeaderMagic == 0x20b) { // PE32+
		is.seekg(106, std::ios_base::cur);
	} else {
		is.seekg(90, std::ios_base::cur);
	}
	
	boost::uint32_t ndirectories = util::load<boost::uint32_t>(is);
	if(is.fail() || ndirectories < 3) {
		return false;
	}
	const boost::uint32_t directory_header_size = 4 + 4; // address + size
	is.seekg(2 * directory_header_size, std::ios_base::cur);
	
	// Virtual memory address and size of the start of resource directory.
	coff.resource_table_address = util::load<boost::uint32_t>(is);
	boost::uint32_t resource_size = util::load<boost::uint32_t>(is);
	if(is.fail() || !coff.resource_table_address || !resource_size) {
		return false;
	}
	
	return true;
}

bool exe_reader_impl::load_section_list(std::istream & is, const header & coff,
                                        section_list & table) {
	
	is.seekg(coff.section_table_offset);
	
	table.resize(coff.nsections);
	for(section_list::iterator i = table.begin(); i != table.end(); ++i) {
		section & section = *i;
		
		is.seekg(8, std::ios_base::cur); // name
		
		section.virtual_size = util::load<boost::uint32_t>(is);
		section.virtual_address = util::load<boost::uint32_t>(is);
		
		is.seekg(4, std::ios_base::cur); // raw size
		section.raw_address = util::load<boost::uint32_t>(is);
		
		// relocation addr + line number addr + relocation count
		// + line number count + characteristics
		is.seekg(4 + 4 + 2 + 2 + 4, std::ios_base::cur);
		
	}
	
	return !is.fail();
}

boost::uint32_t exe_reader_impl::to_file_offset(const section_list & sections,
                                                boost::uint32_t memory) {
	
	for(section_list::const_iterator i = sections.begin(); i != sections.end(); ++i) {
		const section & s = *i;
		if(memory >= s.virtual_address && memory < s.virtual_address + s.virtual_size) {
			return memory + s.raw_address - s.virtual_address;
		}
	}
	
	return 0;
}

exe_reader_impl::resource exe_reader_impl::find_resource(std::istream & is,
                                                         boost::uint32_t name,
                                                         boost::uint32_t type,
                                                         boost::uint32_t language) {
	
	is.seekg(0);
	
	resource result;
	result.offset = result.size = 0;
	
	header coff;
	if(!load_header(is, coff)) {
		return result;
	}
	
	section_list sections;
	if(!load_section_list(is, coff, sections)) {
		return result;
	}
	
	boost::uint32_t resource_offset = to_file_offset(sections, coff.resource_table_address);
	if(!resource_offset) {
		return result;
	}
	
	is.seekg(resource_offset);
	boost::uint32_t type_offset = find_resource_entry(is, type);
	if(!get_resource_table(type_offset, resource_offset)) {
		return result;
	}
	
	is.seekg(type_offset);
	boost::uint32_t name_offset = find_resource_entry(is, name);
	if(!get_resource_table(name_offset, resource_offset)) {
		return result;
	}
	
	is.seekg(name_offset);
	boost::uint32_t leaf_offset = find_resource_entry(is, language);
	if(!leaf_offset || get_resource_table(leaf_offset, resource_offset)) {
		return result;
	}
	
	// Virtual memory address and size of the resource data.
	is.seekg(leaf_offset);
	boost::uint32_t data_address = util::load<boost::uint32_t>(is);
	boost::uint32_t data_size = util::load<boost::uint32_t>(is);
	// ignore codepage and reserved word
	if(is.fail()) {
		return result;
	}
	
	boost::uint32_t data_offset = to_file_offset(sections, data_address);
	if(!data_offset) {
		return result;
	}
	
	result.offset = data_offset;
	result.size = data_size;
	
	return result;
}

} // anonymous namespace

exe_reader::resource exe_reader::find_resource(std::istream & is, boost::uint32_t name,
                                               boost::uint32_t type, boost::uint32_t language) {
	return exe_reader_impl::find_resource(is, name, type, language);
}

} // namespace loader
