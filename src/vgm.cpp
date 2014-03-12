/* vgmtag - a command-line tag editor of VGM/VGZ media files.
Copyright (C) 2013-2014 Dźmitry Laŭčuk

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
#include "vgm.h"
#include <afc/cpu/primitive.h>
#include <memory>
#include <algorithm>

using namespace afc;
using namespace std;

namespace
{
	static const afc::endianness LE = afc::endianness::LE;

	// The minimal normalised header size of supported VGM file formats in octets.
	const size_t SHORT_HEADER_SIZE = 0x40;
	// The maximal normalised header size of supported VGM file formats in octets.
	const size_t LONG_HEADER_SIZE = 0xc0;

	/* For version 1.01 and earlier files:
	 * - the feedback pattern (16 bits) should be assumed to be 0x0009;
	 * - the shift register width (8 bits) should be assumed to be 16;
	 * - reserved 8 bits should be left at zero.
	 * For version 1.51 and earlier files:
	 * - all the flags should not be set.
	 */
	const unsigned char DEFAULT_SN76489[] = {0, 0x09, 16, 0};

	inline void readBytes(unsigned char buf[], const size_t n, InputStream &in, size_t &cursor)
	{
		if (in.read(buf, n) != n) {
			throw MalformedFormatException("Premature end of file");
		}
		cursor += n;
	}

	inline unsigned readTag(u16string &dest, InputStream &src, size_t &cursor)
	{
		unsigned char buf[2];
		for (;;) {
			readBytes(buf, 2, src, cursor);
			const char16_t c = UInt16<>::fromBytes<LE>(buf);
			if (c == 0) {
				break;
			}
			dest.push_back(c);
		}
		return 2*(dest.size()+1);
	}

	inline void writeTag(const u16string &src, OutputStream &out)
	{
		unsigned char buf[2];
		for (size_t i = 0, n = src.size(); i < n; ++i) {
			UInt16<>(src[i]).toBytes<LE>(buf);
			out.write(buf, 2);
		}
		UInt16<>(UInt16<>::type(0)).toBytes<LE>(buf);
		out.write(buf, 2);
	}

	inline uint32_t readUInt32(InputStream &in, size_t &cursor)
	{
		unsigned char buf[4];
		readBytes(buf, 4, in, cursor);
		return UInt32<>::fromBytes<LE>(buf);
	}

	inline void writeUInt32(const uint32_t val, OutputStream &out)
	{
		unsigned char buf[4];
		UInt32<>(val).toBytes<LE>(buf);
		out.write(buf, 4);
	}

	inline void setPos(InputStream &s, const size_t pos, size_t &cursor)
	{
		if (cursor == pos) {
			return;
		}
		if (cursor < pos) {
			s.skip(pos - cursor);
		} else {
			// TODO a more efficient implementation could be here
			s.reset();
			s.skip(pos);
		}
		cursor = pos;
	}
}

inline void vgm::VGMFile::readHeader(InputStream &in, size_t &cursor)
{
	size_t i = 0; // Index of the header element to be read.

	// Reading the base header. It is required for all versions of the VGM format.
	for (size_t n = SHORT_HEADER_SIZE / 4; i < n; ++i) {
		m_header.elements[i] = readUInt32(in, cursor);
	}

	if (m_header.elements[VGMHeader::IDX_ID] != VGMHeader::VGM_FILE_ID) {
		throw MalformedFormatException("Not a VGM/VGZ file");
	}

	const uint32_t ver = version();
	if (ver != VERSION_1_00 &&
			ver != VERSION_1_01 &&
			ver != VERSION_1_10 &&
			ver != VERSION_1_50 &&
			ver != VERSION_1_51 &&
			ver != VERSION_1_60 &&
			ver != VERSION_1_61) {
		throw UnsupportedFormatException("Unsupported VGM version");
	}

	if (ver >= VERSION_1_51) {
		// If the VGM data starts at an offset that is lower than 0xC0, all overlapping header values will be zero.
		const size_t realHeaderElemCount = min(absoluteVgmDataOffset() / 4, VGMHeader::ELEMENT_COUNT);

		for (; i < realHeaderElemCount; ++i) {
			m_header.elements[i] = readUInt32(in, cursor);
		}

		for (; i < VGMHeader::ELEMENT_COUNT; ++i) {
			m_header.elements[i] = 0;
		}
	}
}

inline void vgm::VGMFile::readGD3Info(InputStream &in, size_t &cursor)
{
	setPos(in, VGMHeader::POS_GD3 + m_header.elements[VGMHeader::IDX_GD3_OFFSET], cursor);

	{ // VGM ID
		const uint32_t vgmId = readUInt32(in, cursor);
		if (vgmId != GD3Info::VGM_FILE_GD3_ID) {
			throw MalformedFormatException("Not a VGM file");
		}
	}
	{ // GD3 version
		const uint32_t gd3Version = readUInt32(in, cursor);
		if (gd3Version != GD3Info::VGM_FILE_GD3_VERSION) {
			throw UnsupportedFormatException("Unsupported GD3 version");
		}
	}

	const uint32_t vgmGD3Length = readUInt32(in, cursor);
	unsigned parsedCount = 0;
	// reading tags
	for (size_t i = static_cast<size_t>(Tag::title), n = static_cast<size_t>(Tag::notes); i <= n; ++i) {
		parsedCount += readTag(m_gd3Info.tags[i], in, cursor);
	}
	if (parsedCount != vgmGD3Length) {
		cerr << "skipping last " << vgmGD3Length - parsedCount << " unused bytes of the VGM GD3 header" << endl;
	}
}

inline void vgm::VGMFile::readData(InputStream &in, size_t &cursor)
{
	const size_t absDataOffset = absoluteVgmDataOffset();
	const size_t absoluteEOFOffset = m_header.elements[VGMHeader::IDX_EOF_OFFSET] + VGMHeader::POS_EOF;
	const size_t gd3Offset = m_header.elements[VGMHeader::IDX_GD3_OFFSET];
	if (gd3Offset == 0) { // header -> data -> eof
		m_dataSize = absoluteEOFOffset - absDataOffset;
	} else { // GD3 info exists, the data takes the space between the header and GD3
		const size_t absoluteGD3Offset = gd3Offset + VGMHeader::POS_GD3;
		if (absoluteGD3Offset > absDataOffset) { // header -> data -> gd3 -> eof
			m_dataSize = absoluteGD3Offset - absDataOffset;
		} else { // header -> gd3 -> data -> eof
			m_dataSize = absoluteEOFOffset - absDataOffset;
		}
	}
	setPos(in, absDataOffset, cursor);
	m_data = new unsigned char[m_dataSize];
	readBytes(m_data, m_dataSize, in, cursor);
}

vgm::VGMFile::VGMFile(const char * const srcFile)
try
	: m_data(0), m_dataSize(0)
{
	unique_ptr<InputStream> inPtr(new FileInputStream(srcFile));
	unsigned char buf[4];
	if (inPtr->read(buf, 4) != 4) {
		throw MalformedFormatException("Not a VGM/VGZ file"); // the file is too short to be either a VGM or VGZ file
	}
	if (buf[0] == 0x1f && buf[1] == 0x8b) { // a VGZ (GZip) file. GZip file magic header is {0x1f, 0x8b}
		/* Ensure that the file is not opened twice at the same time.
		   In addition, exceptions while closing could be caught by the caller,
		   which is not the case with destructors. */
		inPtr->close();
		inPtr.reset(new GZipFileInputStream(srcFile));
		m_format = Format::vgz;
	} else if (UInt32<>::fromBytes<LE>(buf) == VGMHeader::VGM_FILE_ID) { // a GVM file
		inPtr->reset();
		m_format = Format::vgm;
	}

	// cursor is used to indicate the current position within the file. Knowing the current position allows setPos()
	// to move cursor forward faster for stream input
	size_t cursor = 0;
	readHeader(*inPtr, cursor);
	readData(*inPtr, cursor);
	readGD3Info(*inPtr, cursor);

	inPtr->close(); // if close generates an exception it is not suppressed, as destructors must do.
}
catch (...) {
	delete[] m_data;
	throw;
}

inline void vgm::VGMFile::writeContent(OutputStream &out) const
{
	const size_t headerElemCount = version() < VERSION_1_51 ? SHORT_HEADER_SIZE / 4 : VGMHeader::ELEMENT_COUNT;
	// Writing the base header. It is required for all versions of the VGM format.
	for (size_t i = 0; i < headerElemCount; ++i) {
		writeUInt32(m_header.elements[i], out);
	}

	// write data
	out.write(m_data, m_dataSize);

	// write GD3 info
	writeUInt32(GD3Info::VGM_FILE_GD3_ID, out);
	writeUInt32(GD3Info::VGM_FILE_GD3_VERSION, out);
	writeUInt32(m_gd3Info.dataSize, out);
	for (size_t i = static_cast<size_t>(Tag::title), n = static_cast<size_t>(Tag::notes); i <= n; ++i) {
		writeTag(m_gd3Info.tags[i], out);
	}
}

void vgm::VGMFile::save(const char * const dest, const Format format)
{
	normalise();

	if (format == Format::vgz) {
		GZipFileOutputStream out(dest);
		writeContent(out);
		out.close(); // if close generates an exception it is not suppressed, as destructors must do.
	} else {
		FileOutputStream out(dest);
		writeContent(out);
		out.close(); // if close generates an exception it is not suppressed, as destructors must do.
	}
}

inline void vgm::VGMFile::normalise()
{
	size_t tagCharCount = 0;
	for (size_t i = static_cast<size_t>(Tag::title), n = static_cast<size_t>(Tag::notes); i <= n; ++i) {
		tagCharCount += m_gd3Info.tags[i].size() + 1; // '\0' must be counted too
	}
	m_gd3Info.dataSize = tagCharCount * 2; // UTF16-LE is used for GD3

	const uint32_t ver = version();

	const size_t headerSize = ver < VERSION_1_51 ? SHORT_HEADER_SIZE : LONG_HEADER_SIZE;
	const size_t fileSize = headerSize + m_dataSize + GD3Info::HEADER_SIZE + m_gd3Info.dataSize;
	m_header.elements[VGMHeader::IDX_EOF_OFFSET] = fileSize - VGMHeader::POS_EOF;

	if (ver < VERSION_1_01) {
		// VGM 1.00 files will have a value of 0. Overriding the real value in this case.
		m_header.elements[VGMHeader::IDX_RATE] = 0;
	}
	if (ver < VERSION_1_10) {
		// For version 1.01 and earlier files, the YM2413 clock rate should be used for the clock rate of the YM2612.
		m_header.elements[VGMHeader::IDX_YM2612_CLOCK] = 0;
		// For version 1.01 and earlier files, the YM2413 clock rate should be used for the clock rate of the YM2151.
		m_header.elements[VGMHeader::IDX_YM2151_CLOCK] = 0;
	}
	// TODO does this affect gd3Offset? If yes then gd3Offset must be normalised, too.
	/* Forcing the VGM data to start at minimal absolute offset allowed for the given version of the VGM format.
	 * For versions prior to 1.50, it should be 0 and the VGM data must start at offset 0x40.
	 */
	m_header.elements[VGMHeader::IDX_VGM_DATA_OFFSET] = ver < VERSION_1_50 ?
			0 : headerSize - VGMHeader::POS_VGM_DATA;
}
