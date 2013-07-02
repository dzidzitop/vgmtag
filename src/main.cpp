/* vgmtag - a command-line tag editor of VGM/VGZ media files.
Copyright (C) 2013 Dzmitry Liauchuk

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
#include <exception>
#include <map>
#include <iostream>
#include <memory>
#include <locale>
#include <string>
#include <langinfo.h>

#include <getopt.h>

#include "version.h"
#include "vgm.h"

#include <afc/Exception.h>
#include <afc/math_utils.h>
#include <afc/utils.h>

using namespace std;
using namespace afc;
using namespace vgm;
using Tag = vgm::VGMFile::Tag;
using Format = vgm::VGMFile::Format;

namespace {
// TODO resolve it dynamically using argv[0]?
const char * const programName = "vgmtag";
const int getopt_tagStartValue = 1000;
static const char * systemEncoding;

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
	if (!success) {
		cout << "Try '" << programName << " --help' for more information." << endl;
	} else {
		cout <<
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
Report " << programName << " bugs to dzidzitop@lavabit.com" << endl;
	}
}

void printVersion()
{
	string author;
	try {
		author = utf16leToString(u"D\u017Amitry La\u016D\u010Duk", systemEncoding);
	}
	catch (MalformedFormatException &ex) {
		author = "Dzmitry Liauchuk";
	}
	cout << PROGRAM_NAME << " " << PROGRAM_VERSION << "\n\
Copyright (C) 2013 " << author << ".\n\
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.\n\
This is free software: you are free to change and redistribute it.\n\
There is NO WARRANTY, to the extent permitted by law.\n\
\n\
Written by " << author << '.' << endl;
}

void printOutputFormatConflict()
{
	cerr << "Cannot force both VGM and VGZ output formats." << endl;
}

void printInfo(const VGMFile &vgmFile, const bool failSafeInfo)
{
	string encoding(systemEncoding);
	if (failSafeInfo) {
		encoding += "//TRANSLIT";
	}
	const string title(utf16leToString(vgmFile.getTag(Tag::title), encoding.c_str()));
	const string titleJP(utf16leToString(vgmFile.getTag(Tag::titleJP), encoding.c_str()));
	const string game(utf16leToString(vgmFile.getTag(Tag::game), encoding.c_str()));
	const string gameJP(utf16leToString(vgmFile.getTag(Tag::gameJP), encoding.c_str()));
	const string system(utf16leToString(vgmFile.getTag(Tag::system), encoding.c_str()));
	const string systemJP(utf16leToString(vgmFile.getTag(Tag::systemJP), encoding.c_str()));
	const string author(utf16leToString(vgmFile.getTag(Tag::author), encoding.c_str()));
	const string authorJP(utf16leToString(vgmFile.getTag(Tag::authorJP), encoding.c_str()));
	const string date(utf16leToString(vgmFile.getTag(Tag::date), encoding.c_str()));
	const string converter(utf16leToString(vgmFile.getTag(Tag::converter), encoding.c_str()));
	const string notes(utf16leToString(vgmFile.getTag(Tag::notes), encoding.c_str()));

	cout << "File format:\t\t" << (vgmFile.getFormat() == Format::vgm ? "VGM" : "VGZ") << '\n';
	cout << "--------\n";
	cout << "Title (Latin):\t\t" << title << '\n';
	cout << "Title (Japanese):\t" << titleJP << '\n';
	cout << "Game (Latin):\t\t" << game << '\n';
	cout << "Game (Japanese):\t" << gameJP << '\n';
	cout << "System (Latin):\t\t" << system << '\n';
	cout << "System (Japanese):\t" << systemJP << '\n';
	cout << "Author (Latin):\t\t" << author << '\n';
	cout << "Author (Japanese):\t" << authorJP << '\n';
	cout << "Date:\t\t\t" << date << '\n';
	cout << "Converter:\t\t" << converter << '\n';
	cout << "Notes:\t\t\t" << notes << endl;
}

void initLocaleContext()
{
	locale::global(locale(""));
	systemEncoding = nl_langinfo(CODESET);
}

typedef map<Tag, const u16string> M;
typedef M::value_type P;
}

int main(const int argc, char * argv[])
try {
	initLocaleContext();

	M tags;

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
			tags.insert(P(tag, stringToUTF16LE(::optarg, systemEncoding)));
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
				cerr << "Unhandled option: ";
				if (optionIndex == -1) {
					cerr << '-' << static_cast<char>(c);
				} else {
					cerr << "--" << options[optionIndex].name;
				}
				cerr << endl;
				return 1;
			}
		}
		optionIndex = -1;
	}
	if (optind == argc) {
		cerr << "No SOURCE file." << endl;
		printUsage(false);
		return 1;
	}
	if (optind < argc-2) {
		cerr << "Only SOURCE and DEST files can be specified." << endl;
		printUsage(false);
		return 1;
	}

	const char * const src(argv[optind]);

	bool saveToSameFile;
	const char *destFile;
	if (optind < argc-1) { // there is DEST file
		if (showInfo) {
			cerr << "Only SOURCE can be specified with --info or --info-failsafe." << endl;
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
			cerr << "No other options can be specified with --info or --info-failsafe." << endl;
			return 1;
		}
		try {
			// TODO load only GD3 in the --info mode
			printInfo(VGMFile(src), failSafeInfo);
		}
		catch (MalformedFormatException &ex) {
			cerr << "There are characters in the GD3 tags that cannot be mapped to the system encoding (" << systemEncoding <<
					"). Try to run the program with the --info-failsafe option." << endl;
			return 1;
		}
		return 0;
	}

	VGMFile vgmFile(src);
	for (const P &entry : tags) {
		vgmFile.setTag(entry.first, entry.second);
	}

	Format outputFormat;
	if (forceVGM) {
		outputFormat = Format::vgm;
	} else if (forceVGZ) {
		outputFormat = Format::vgz;
	} else if (saveToSameFile) {
		outputFormat = vgmFile.getFormat();
	} else if (endsWith(destFile, ".vgz")) {
		outputFormat = Format::vgz;
	} else {
		outputFormat = Format::vgm;
	}

	vgmFile.save(destFile, outputFormat);

	return 0;
}
#ifdef VGM_DEBUG
catch (Exception &ex) {
	ex.printStackTrace(cerr);
	return 1;
}
#endif
catch (exception &ex) {
	cerr << ex.what() << endl;
	return 1;
}
catch (const char * const ex) {
	cerr << ex << endl;
	return 1;
}
