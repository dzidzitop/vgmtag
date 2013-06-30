#include <exception>
#include <map>
#include <cstdio>
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
  -m  \t\tforce VMG output format. Cannot be used together with -z\n\
  -z  \t\tforce VMZ (compressed) output format. Cannot be used\n\
      \t\t  together with -m\n\
      --info\tdisplay file format and GD3 info and exit\n\
  -h, --help\tdisplay this help and exit\n\
      --version\tdisplay version information and exit\n\
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
	wcout << PROGRAM_NAME << " " << PROGRAM_VERSION << L"\n\
Copyright (C) 2013 D\x017Amitry La\x016D\x010Duk.\n\
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.\n\
This is free software: you are free to change and redistribute it.\n\
There is NO WARRANTY, to the extent permitted by law.\n\
\n\
Written by D\x017Amitry La\x016D\x010Duk." << endl;
}

void printOutputFormatConflict()
{
	cerr << "Cannot force both VGM and VGZ output formats." << endl;
}

void printInfo(const VGMFile &vgmFile, const char * const outputEncoding)
{
	cout << "File format:\t\t" << (vgmFile.getFormat() == Format::vgm ? "VGM" : "VGZ") << '\n';
	cout << "--------\n";
	cout << "Title (Latin):\t\t" << utf16leToString(vgmFile.getTag(Tag::title), outputEncoding) << '\n';
	cout << "Title (Japanese):\t" << utf16leToString(vgmFile.getTag(Tag::titleJP), outputEncoding) << '\n';
	cout << "Game (Latin):\t\t" << utf16leToString(vgmFile.getTag(Tag::game), outputEncoding) << '\n';
	cout << "Game (Japanese):\t" << utf16leToString(vgmFile.getTag(Tag::gameJP), outputEncoding) << '\n';
	cout << "System (Latin):\t\t" << utf16leToString(vgmFile.getTag(Tag::system), outputEncoding) << '\n';
	cout << "System (Japanese):\t" << utf16leToString(vgmFile.getTag(Tag::systemJP), outputEncoding) << '\n';
	cout << "Author (Latin):\t\t" << utf16leToString(vgmFile.getTag(Tag::author), outputEncoding) << '\n';
	cout << "Author (Japanese):\t" << utf16leToString(vgmFile.getTag(Tag::authorJP), outputEncoding) << '\n';
	cout << "Date:\t\t\t" << utf16leToString(vgmFile.getTag(Tag::date), outputEncoding) << '\n';
	cout << "Converter:\t\t" << utf16leToString(vgmFile.getTag(Tag::converter), outputEncoding) << '\n';
	cout << "Notes:\t\t\t" << utf16leToString(vgmFile.getTag(Tag::notes), outputEncoding) << endl;
}

static const char * charmap;

void initLocaleContext()
{
	locale::global(locale(""));
	charmap = nl_langinfo(CODESET);
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
	int c;
	int optionIndex = -1;
	while ((c = ::getopt_long(argc, argv, "hmz", options, &optionIndex)) != -1) {
		if (c >= getopt_tagStartValue + static_cast<int>(Tag::title) &&
				c <= getopt_tagStartValue + static_cast<int>(Tag::notes)) { // processing a tag argument
			nonInfoSpecified = true;
			const Tag tag = static_cast<Tag>(c - getopt_tagStartValue);
			// TODO for Tag::notes - think about non-Unix platforms which use not \n as the line delimiter. The GD3 1.00 spec requires '\n'
			tags.insert(P(tag, stringToUTF16LE(::optarg, charmap)));
		} else {
			switch (c) {
			case 'i':
				showInfo = true;
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
			cerr << "Only SOURCE can be specified with --info." << endl;
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
			cerr << "No other options can be specified with --info." << endl;
			return 1;
		}
		// TODO load only GD3 in the --info mode
		printInfo(VGMFile(src), charmap);
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
catch (Exception &ex) {
	ex.printStackTrace(cerr);
	return 1;
}
catch (exception &ex) {
	cerr << ex.what() << endl;
	return 1;
}
catch (const char * const ex) {
	cerr << ex << endl;
	return 1;
}
