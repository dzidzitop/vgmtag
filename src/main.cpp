/* vgmtag - a command-line tag editor of VGM/VGZ media files.
Copyright (C) 2013-2016 Dźmitry Laŭčuk

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>. */
#include <array>
#include <cassert>
#include <clocale>
#include <cstring>
#include <exception>
#include <iostream>
#include <ostream>
#include <utility>

#include <getopt.h>

#include "version.h"
#include "vgm.h"

#include <afc/Exception.h>
#include <afc/FastStringBuffer.hpp>
#include <afc/SimpleString.hpp>
#include <afc/string_util.hpp>
#include <afc/StringRef.hpp>
#include <afc/utils.h>

using namespace vgm;
using Tag = vgm::VGMFile::Tag;
using Format = vgm::VGMFile::Format;
using afc::operator"" _s;

namespace {
// TODO resolve it dynamically using argv[0]?
const char * const programName = "vgmtag";
const int getopt_tagStartValue = 1000;
static afc::String systemEncoding;

static const struct option options[] = {
	{"title", required_argument, nullptr, getopt_tagStartValue + static_cast<int>(Tag::title)},
	{"titleJP", required_argument, nullptr, getopt_tagStartValue + static_cast<int>(Tag::titleJP)},
	{"game", required_argument, nullptr, getopt_tagStartValue + static_cast<int>(Tag::game)},
	{"gameJP", required_argument, nullptr, getopt_tagStartValue + static_cast<int>(Tag::gameJP)},
	{"system", required_argument, nullptr, getopt_tagStartValue + static_cast<int>(Tag::system)},
	{"systemJP", required_argument, nullptr, getopt_tagStartValue + static_cast<int>(Tag::systemJP)},
	{"author", required_argument, nullptr, getopt_tagStartValue + static_cast<int>(Tag::author)},
	{"authorJP", required_argument, nullptr, getopt_tagStartValue + static_cast<int>(Tag::authorJP)},
	{"date", required_argument, nullptr, getopt_tagStartValue + static_cast<int>(Tag::date)},
	{"converter", required_argument, nullptr, getopt_tagStartValue + static_cast<int>(Tag::converter)},
	{"notes", required_argument, nullptr, getopt_tagStartValue + static_cast<int>(Tag::notes)},
	{"help", no_argument, nullptr, 'h'},
	{"version", no_argument, nullptr, 'v'},
	{"info", no_argument, nullptr, 'i'},
	{"info-failsafe", no_argument, nullptr, 's'},
	{0}
};

void printUsage(bool success, const char * const programName = ::programName)
{
	using std::operator<<;

	if (!success) {
		std::cout << "Try '" << programName << " --help' for more information." << std::endl;
	} else {
		std::cout <<
"Usage: " << programName << " [OPTION]... SOURCE [DEST]\n\
Updates GD3 tags of the SOURCE file of the VGM or VGZ format and saves the\n\
result to the DEST file (or to SOURCE if DEST is omitted).\n\
\n\
All options are optional. If the tag is omitted then it is not updated.\n\
An empty string as a tag argument indicates that the tag is to be cleared.\n\
Only the 'notes' tag can be multi-line.\n\
      --title\t\ttrack name in Latin\n\
      --titleJP\t\ttrack name in Japanese\n\
      --game\t\tgame name in Latin\n\
      --gameJP\t\tgame name in Japanese\n\
      --system\t\tsystem name in Latin\n\
      --systemJP\tsystem name in Japanese\n\
      --author\t\tname of original track author in English\n\
      --authorJP\tname of original track author in Japanese\n\
      --date\t\tdate of game release written in the form yyyy/mm/dd,\n\
            \t\t  or yyyy/mm, or yyyy if month and day is not known\n\
      --converter\tname of person who converted this track to a VGM file\n\
      --notes\t\tnotes to this track\n\
\n\
  -m  \t\t\tforce VMG output format. Cannot be used together with -z\n\
  -z  \t\t\tforce VMZ (compressed) output format. Cannot be used\n\
      \t\t\t  together with -m\n\
      --info\t\tdisplay SOURCE file format and GD3 info and exit\n\
      --info-failsafe\tdisplay SOURCE file format and GD3 info (transliterating\n\
      \t\t\t  unmappable characters, if needed) and exit\n\
  -h, --help\t\tdisplay this help and exit\n\
      --version\t\tdisplay version information and exit\n\
\n\
If the output format is not specified then:\n\
  1) DEST is defined:\n\
    a) its extension is .vgz -> the VGZ format is used,\n\
    b) the VGM format is used otherwise;\n\
  2) DEST is undefined -> the format of SOURCE is preserved.\n\
\n\
The system name should be written in a standard form (keeping spelling, spacing\n\
and capitalisation the same). Here are some standard system names:\n\
\n\
  Sega Master System\n\
  Sega Game Gear\n\
  Sega Master System / Game Gear\n\
  Sega Mega Drive / Genesis\n\
  Sega Game 1000\n\
  Sega Computer 3000\n\
  Sega System 16\n\
  Capcom Play System 1\n\
  Colecovision\n\
  BBC Model B\n\
  BBC Model B+\n\
  BBC Master 128\n\
\n\
Report " << programName << " bugs to dzidzitop@vfemail.net" << std::endl;
	}
}

void printVersion()
{
	using std::operator<<;

	afc::String author;
	try {
		const char16_t name[] = u"D\u017Amitry La\u016D\u010Duk";
		author = afc::utf16leToString(name, sizeof(name) - 1, systemEncoding.c_str());
	}
	catch (afc::Exception &ex) {
		author = "Dzmitry Liauchuk"_s;
	}
	const char * const authorPtr = author.c_str();
	std::cout << PROGRAM_NAME << " " << PROGRAM_VERSION << "\n\
Copyright (C) 2013-2016 " << authorPtr << ".\n\
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.\n\
This is free software: you are free to change and redistribute it.\n\
There is NO WARRANTY, to the extent permitted by law.\n\
\n\
Written by " << authorPtr << '.' << std::endl;
}

void printOutputFormatConflict()
{
	using std::operator<<;

	std::cerr << "Cannot force both VGM and VGZ output formats." << std::endl;
}

void printInfo(const VGMFile &vgmFile, const bool failSafeInfo)
{
	using std::operator<<;

	afc::FastStringBuffer<char, afc::AllocMode::accurate> encoding(
			systemEncoding.size() + (failSafeInfo ? "//TRANSLIT"_s.size() : 0));
	encoding.append(systemEncoding.data(), systemEncoding.size());
	if (failSafeInfo) {
		encoding.append("//TRANSLIT"_s);
	}

	const char * const encodingStr = encoding.c_str();

	const afc::String title(utf16leToString(vgmFile.getTag(Tag::title), encodingStr));
	const afc::String titleJP(utf16leToString(vgmFile.getTag(Tag::titleJP), encodingStr));
	const afc::String game(utf16leToString(vgmFile.getTag(Tag::game), encodingStr));
	const afc::String gameJP(utf16leToString(vgmFile.getTag(Tag::gameJP), encodingStr));
	const afc::String system(utf16leToString(vgmFile.getTag(Tag::system), encodingStr));
	const afc::String systemJP(utf16leToString(vgmFile.getTag(Tag::systemJP), encodingStr));
	const afc::String author(utf16leToString(vgmFile.getTag(Tag::author), encodingStr));
	const afc::String authorJP(utf16leToString(vgmFile.getTag(Tag::authorJP), encodingStr));
	const afc::String date(utf16leToString(vgmFile.getTag(Tag::date), encodingStr));
	const afc::String converter(utf16leToString(vgmFile.getTag(Tag::converter), encodingStr));
	const afc::String notes(utf16leToString(vgmFile.getTag(Tag::notes), encodingStr));

	std::cout << "File format:\t\t" << (vgmFile.getFormat() == Format::vgm ? "VGM" : "VGZ") << '\n';
	std::cout << "--------\n";
	std::cout << "Title (Latin):\t\t" << title.c_str() << '\n';
	std::cout << "Title (Japanese):\t" << titleJP.c_str() << '\n';
	std::cout << "Game (Latin):\t\t" << game.c_str() << '\n';
	std::cout << "Game (Japanese):\t" << gameJP.c_str() << '\n';
	std::cout << "System (Latin):\t\t" << system.c_str() << '\n';
	std::cout << "System (Japanese):\t" << systemJP.c_str() << '\n';
	std::cout << "Author (Latin):\t\t" << author.c_str() << '\n';
	std::cout << "Author (Japanese):\t" << authorJP.c_str() << '\n';
	std::cout << "Date:\t\t\t" << date.c_str() << '\n';
	std::cout << "Converter:\t\t" << converter.c_str() << '\n';
	std::cout << "Notes:\t\t\t" << notes.c_str() << std::endl;
}

void initLocaleContext()
{
	std::setlocale(LC_ALL, "");
	systemEncoding = afc::systemCharset();
}

VGMFile loadFile(const char * const src)
{
	try {
		return VGMFile(src);
	}
	catch (afc::Exception &ex) {
		throw afc::Exception("Unable to load VGM/VGZ data."_s, &ex);
	}
}

using TagValue = afc::Optional<afc::U16String>;
using TagArray = std::array<TagValue, static_cast<int>(Tag::notes) - static_cast<int>(Tag::title) + 1>;
}

// TODO add support of migrating to another VGM file version.
int main(const int argc, char * argv[])
try {
	using std::operator<<;

	initLocaleContext();

	TagArray tags = {TagValue::none(), TagValue::none(), TagValue::none(), TagValue::none(), TagValue::none(),
			TagValue::none(), TagValue::none(), TagValue::none(), TagValue::none(), TagValue::none(), TagValue::none()};
	assert(!tags.back().hasValue()); // Ensuring that the array is initialised completely.

	bool nonInfoSpecified = false;
	bool forceVGM = false;
	bool forceVGZ = false;
	bool showInfo = false;
	bool failSafeInfo = false;
	int c;
	int optionIndex = -1;
	while ((c = ::getopt_long(argc, argv, "hmz", options, &optionIndex)) != -1) {
		if (c >= getopt_tagStartValue + static_cast<int>(Tag::title) &&
				c <= getopt_tagStartValue + static_cast<int>(Tag::notes)) { // processing a tag argument
			nonInfoSpecified = true;
			const Tag tag = static_cast<Tag>(c - getopt_tagStartValue);
			// TODO for Tag::notes - think about non-Unix platforms which use not \n as the line delimiter. The GD3 1.00 spec requires '\n'
			tags[static_cast<int>(tag)] = TagValue(afc::stringToUTF16LE(::optarg, systemEncoding.c_str()));
		} else {
			switch (c) {
			case 'i':
				showInfo = true;
				break;
			case 's':
				showInfo = true;
				failSafeInfo = true;
				break;
			case 'h':
				printUsage(true);
				return 0;
			case 'v':
				printVersion();
				return 0;
			case 'm':
				nonInfoSpecified = true;
				if (forceVGZ) {
					printOutputFormatConflict();
					return 1;
				}
				forceVGM = true;
				break;
			case 'z':
				nonInfoSpecified = true;
				if (forceVGM) {
					printOutputFormatConflict();
					return 1;
				}
				forceVGZ = true;
				break;
			case '?':
				// getopt_long takes care of informing the user about the error option
				printUsage(false);
				return 1;
			default:
				std::cerr << "Unhandled option: ";
				if (optionIndex == -1) {
					std::cerr << '-' << static_cast<char>(c);
				} else {
					std::cerr << "--" << options[optionIndex].name;
				}
				std::cerr << std::endl;
				return 1;
			}
		}
		optionIndex = -1;
	}
	if (optind == argc) {
		std::cerr << "No SOURCE file." << std::endl;
		printUsage(false);
		return 1;
	}
	if (optind < argc-2) {
		std::cerr << "Only SOURCE and DEST files can be specified." << std::endl;
		printUsage(false);
		return 1;
	}

	const char * const src(argv[optind]);

	bool saveToSameFile;
	const char *destFile;
	if (optind < argc-1) { // there is DEST file
		if (showInfo) {
			std::cerr << "Only SOURCE can be specified with --info or --info-failsafe." << std::endl;
			return 1;
		}
		saveToSameFile = false;
		destFile = argv[optind+1];
	} else { // there is no DEST file
		saveToSameFile = true;
		destFile = argv[optind];
	}

	if (showInfo) {
		if (nonInfoSpecified) {
			std::cerr << "No other options can be specified with --info or --info-failsafe." << std::endl;
			return 1;
		}
		// TODO load only GD3 in the --info mode
		VGMFile vgmFile = loadFile(src);
		try {
			printInfo(vgmFile, failSafeInfo);
		}
		catch (afc::Exception &ex) {
			std::cerr << "There are characters in the GD3 tags that cannot be mapped to the system encoding (" <<
					systemEncoding.c_str() << "). Try to run the program with the --info-failsafe option." << std::endl;
			return 1;
		}
		return 0;
	}

	VGMFile vgmFile = loadFile(src);

	for (std::size_t i = 0, n = tags.size(); i < n; ++i) {
		const TagValue &entry = tags[i];
		if (entry.hasValue()) {
			vgmFile.setTag(static_cast<Tag>(i), std::move(entry.value()));
		}
	}

	afc::ConstStringRef vgzExt = ".vgz"_s;

	Format outputFormat;
	if (forceVGM) {
		outputFormat = Format::vgm;
	} else if (forceVGZ) {
		outputFormat = Format::vgz;
	} else if (saveToSameFile) {
		outputFormat = vgmFile.getFormat();
	} else if (afc::endsWith(destFile, destFile + std::strlen(destFile), vgzExt.begin(), vgzExt.end())) {
		outputFormat = Format::vgz;
	} else {
		outputFormat = Format::vgm;
	}

	try {
		vgmFile.save(destFile, outputFormat);
	}
	catch (afc::Exception &ex) {
		std::cerr << "Unable to save VGM/VGZ data to '" << destFile << "':\n  " << ex.what() << std::endl;
		return 1;
	}

	return 0;
}
#ifdef VGM_DEBUG
catch (afc::Exception &ex) {
	ex.printStackTrace(cerr);
	return 1;
}
#endif
catch (std::exception &ex) {
	using std::operator<<;

	std::cerr << ex.what() << std::endl;
	return 1;
}
catch (const char * const ex) {
	using std::operator<<;

	std::cerr << ex << std::endl;
	return 1;
}
