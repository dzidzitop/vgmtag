#include <exception>
#include <map>
#include <cstdio>
#include <memory>
#include <locale>
#include <string>
#include <langinfo.h>
#include <iconv.h>

#include <getopt.h>

#include "vgm.h"
#include "stream.h"

#include <afc/Exception.h>
#include <afc/File.h>
#include <afc/math_utils.h>

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

static const char * charmap;

void initLocaleContext()
{
	locale::global(locale(""));
	charmap = nl_langinfo(CODESET);
}

inline u16string stringToUTF16LE(const string &src)
{
	if (src.empty()) {
		return u16string();
	}
	iconv_t state = iconv_open("UTF-16LE", charmap);
	char * srcBuf = const_cast<char *>(src.c_str()); // for some reason iconv takes non-const source buffer
	size_t srcSize = src.size();
	const size_t destSize = 4 * srcSize; // max length of a UTF16-LE character is 4 bytes
	size_t destCharsLeft = destSize;
	unique_ptr<char[]> destBuf(new char[destSize]);
	char * mutableDestBuf = destBuf.get(); // iconv modifies the pointers to the buffers
	const size_t count = iconv(state, &srcBuf, &srcSize, &mutableDestBuf, &destCharsLeft);
	iconv_close(state); // TODO call this in a destructor. Create a wrapper for iconv
	if (count == static_cast<size_t>(-1)) {
		// TODO handle errors using errno
		// The following errors can occur, among others:
		// E2BIG There is not sufficient room at *outbuf.
		// EILSEQ An invalid multibyte sequence has been encountered in the input.
		// EINVAL An incomplete multibyte sequence has been encountered in the input.
		throw MalformedFormatException("unsupported character sequence");
	}
	const size_t bufSize = destSize - destCharsLeft;
	if (isOdd(bufSize)) {
		throw MalformedFormatException("unsupported character sequence");
	}

	const char * const buf = destBuf.get();
	u16string result;
	result.reserve(bufSize/2);
	for (size_t i = 0; i < bufSize; i+=2) {
		// TODO support big-endian platforms. The line below assumes that the target platform is little-endian
		const char16_t codePoint = static_cast<unsigned char>(buf[i]) + (static_cast<unsigned char>(buf[i+1])<<8);
		result.push_back(codePoint);
	}
	return result;
}

typedef map<Tag, const u16string> M;
typedef M::value_type P;

int main(const int argc, char * argv[])
try {
	initLocaleContext();

	M tags;

	int c;
	int optionIndex = -1;
	while ((c = ::getopt_long(argc, argv, "h", options, &optionIndex)) != -1) {
		switch (c) {
		case 't':
			tags.insert(P(Tag::title, stringToUTF16LE(::optarg)));
			break;
		case 'T':
			tags.insert(P(Tag::titleJP, stringToUTF16LE(::optarg)));
			break;
		case 'g':
			tags.insert(P(Tag::game, stringToUTF16LE(::optarg)));
			break;
		case 'G':
			tags.insert(P(Tag::gameJP, stringToUTF16LE(::optarg)));
			break;
		case 's':
			tags.insert(P(Tag::system, stringToUTF16LE(::optarg)));
			break;
		case 'S':
			tags.insert(P(Tag::systemJP, stringToUTF16LE(::optarg)));
			break;
		case 'a':
			tags.insert(P(Tag::author, stringToUTF16LE(::optarg)));
			break;
		case 'A':
			tags.insert(P(Tag::authorJP, stringToUTF16LE(::optarg)));
			break;
		case 'd':
			tags.insert(P(Tag::date, stringToUTF16LE(::optarg)));
			break;
		case 'c':
			tags.insert(P(Tag::converter, stringToUTF16LE(::optarg)));
			break;
		case 'n':
			// TODO think about non-Unix platforms which use not \n as the line delimiter. The GD3 1.00 spec requires '\n'
			tags.insert(P(Tag::notes, stringToUTF16LE(::optarg)));
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
