/*
 * Copyright (C) 2011-2014 Daniel Scharrer
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

/*!
 * \file
 *
 * Structures for deletion entries stored in Inno Setup files.
 */
#ifndef INNOEXTRACT_SETUP_DELETE_HPP
#define INNOEXTRACT_SETUP_DELETE_HPP

#include <string>
#include <iosfwd>

#include "setup/item.hpp"
#include "util/enum.hpp"

namespace setup {

struct version;

struct delete_entry : public item {
	
	enum target_type {
		Files,
		FilesAndSubdirs,
		DirIfEmpty,
	};
	
	std::string name;
	
	target_type type;
	
	void load(std::istream & is, const version & version);
	
};

} // namespace setup

NAMED_ENUM(setup::delete_entry::target_type)

#endif // INNOEXTRACT_SETUP_DELETE_HPP
