#include <exception>
#include <map>
#include <cstdio>
#include <cwchar>
#include <memory>
#include <locale>
#include <string>

#include <getopt.h>

#include "vgm.h"
#include "stream.h"

#include <afc/Exception.h>
#include <afc/File.h>

using namespace std;
using namespace afc;
using namespace vgm;
using Tag = vgm::VGMFile::Tag;

// TODO resolve it dynamically using argv[0]?
const char * const programName = "vgmtag";

static const struct option options[] = {
	{"title", required_argument, nullptr, 't'},
	{"titleJP", required_argument, nullptr, 'T'},
	{"game", required_argument, nullptr, 'g'},
	{"gameJP", required_argument, nullptr, 'G'},
	{"system", required_argument, nullptr, 's'},
	{"systemJP", required_argument, nullptr, 'S'},
	{"author", required_argument, nullptr, 'a'},
	{"authorJP", required_argument, nullptr, 'A'},
	{"date", required_argument, nullptr, 'd'},
	{"converter", required_argument, nullptr, 'c'},
	{"notes", required_argument, nullptr, 'n'},
	{"help", no_argument, nullptr, 'h'},
	{"version", no_argument, nullptr, 'v'},
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
	wcout << L"vgmtag 0.0.1\n\
Copyright (C) 2013 D\x017Amitry La\x016D\x010Duk.\n\
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.\n\
This is free software: you are free to change and redistribute it.\n\
There is NO WARRANTY, to the extent permitted by law.\n\
\n\
Written by D\x017Amitry La\x016D\x010Duk." << endl;
}

inline wstring stringToWideString(const string &src)
{
	if (src.empty()) {
		return wstring();
	}
	const size_t maxSize = src.size();
	unique_ptr<wchar_t[]> buf(new wchar_t[maxSize]);
	const size_t count = mbstowcs(buf.get(), src.c_str(), maxSize);
	if (count == static_cast<std::size_t>(-1)) {
		throw MalformedFormatException("unsupported character sequence");
	}
	return wstring(buf.get(), count);
}

typedef map<Tag, const wstring> M;
typedef pair<Tag, const wstring> P;

int main(const int argc, char * argv[])
try {
	locale::global(locale(""));
	M tags;

	int c;
	int optionIndex = -1;
	while ((c = ::getopt_long(argc, argv, "h", options, &optionIndex)) != -1) {
		switch (c) {
		case 't':
			tags.insert(P(Tag::title, stringToWideString(::optarg)));
			break;
		case 'T':
			tags.insert(P(Tag::titleJP, stringToWideString(::optarg)));
			break;
		case 'g':
			tags.insert(P(Tag::game, stringToWideString(::optarg)));
			break;
		case 'G':
			tags.insert(P(Tag::gameJP, stringToWideString(::optarg)));
			break;
		case 's':
			tags.insert(P(Tag::system, stringToWideString(::optarg)));
			break;
		case 'S':
			tags.insert(P(Tag::systemJP, stringToWideString(::optarg)));
			break;
		case 'a':
			tags.insert(P(Tag::author, stringToWideString(::optarg)));
			break;
		case 'A':
			tags.insert(P(Tag::authorJP, stringToWideString(::optarg)));
			break;
		case 'd':
			tags.insert(P(Tag::date, stringToWideString(::optarg)));
			break;
		case 'c':
			tags.insert(P(Tag::converter, stringToWideString(::optarg)));
			break;
		case 'n':
			// TODO think about non-Unix platforms which use not \n as the line delimiter. The GD3 1.00 spec requires '\n'
			tags.insert(P(Tag::notes, stringToWideString(::optarg)));
			break;
		case 'h':
			printUsage(true);
			return 0;
		case 'v':
			printVersion();
			return 0;
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

	const File src(argv[optind]);
	const File dest(optind < argc-1 ? argv[optind+1] : argv[optind]);

	VGMFile vgmFile = VGMFile::load(src);
	for (const P &entry : tags) {
		vgmFile.setTag(entry.first, entry.second);
	}
	// TODO determine compress or not using either the dest file extension or the user-defined setting
	vgmFile.save(dest, true);

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
