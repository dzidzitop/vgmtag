#include "vgm.h"
#include <afc/cpu/primitive.h>
#include <afc/utils.h>
#include <climits>
#include <memory>

using namespace afc;
using namespace std;

namespace
{
	static const afc::endianness LE = afc::endianness::LE;

	inline void readBytes(unsigned char buf[], const size_t n, InputStream &in)
	{
		if (in.read(buf, n) != n) {
			throw MalformedFormatException("Premature end of file");
		}
	}

	unsigned readTag(u16string &dest, InputStream &src)
	{
		unsigned char buf[2];
		for (;;) {
			readBytes(buf, 2, src);
			const char16_t c = UInt16<>::fromBytes<LE>(buf);
			if (c == 0) {
				break;
			}
			dest.push_back(c);
		}
		return 2*(dest.size()+1);
	}

	void writeTag(const u16string &src, OutputStream &out)
	{
		unsigned char buf[2];
		for (size_t i = 0, n = src.size(); i < n; ++i) {
			UInt16<>(src[i]).toBytes<LE>(buf);
			out.write(buf, 2);
		}
		UInt16<>(UInt16<>::type(0)).toBytes<LE>(buf);
		out.write(buf, 2);
	}

	uint32_t readUInt32(InputStream &in)
	{
		unsigned char buf[4];
		in.read(buf, 4);
		return UInt32<>::fromBytes<LE>(buf);
	}

	void writeUInt32(const uint32_t val, OutputStream &out)
	{
		unsigned char buf[4];
		UInt32<>(val).toBytes<LE>(buf);
		out.write(buf, 4);
	}

	// TODO a more efficient implementation could be here
	void setPos(InputStream &s, const size_t pos)
	{
		s.reset();
		s.skip(pos);
	}
}

void vgm::VGMFile::readHeader(InputStream &in)
{
	{ // VGM ID
		m_header.id = readUInt32(in);
		if (m_header.id != VGMHeader::VGM_FILE_ID) {
			throw MalformedFormatException("Not a VGM file");
		}
	}
	m_header.eofOffset = readUInt32(in);
	{ // VGM version
		m_header.version = readUInt32(in);
		if (m_header.version != VGMHeader::VGM_FILE_VERSION) {
			throw UnsupportedFormatException("Unsupported VGM version");
		}
	}
	m_header.snClock = readUInt32(in);
	m_header.ym2413Clock = readUInt32(in);
	m_header.gd3Offset = readUInt32(in);
	m_header.sampleCount = readUInt32(in);
	m_header.loopOffset = readUInt32(in);
	m_header.loopSampleCount = readUInt32(in);
	m_header.rate = readUInt32(in);
	m_header.snFeedback = readUInt32(in);
	m_header.ym2612Clock = readUInt32(in);
	m_header.ym2151Clock = readUInt32(in);
	m_header.vgmDataOffset = readUInt32(in);
}

void vgm::VGMFile::readGD3Info(InputStream &in)
{
	setPos(in, VGMHeader::POS_GD3 + m_header.gd3Offset);

	{ // VGM ID
		const uint32_t vgmId = readUInt32(in);
		if (vgmId != GD3Info::VGM_FILE_GD3_ID) {
			throw MalformedFormatException("Not a VGM file");
		}
	}
	{ // GD3 version
		const uint32_t gd3Version = readUInt32(in);
		if (gd3Version != GD3Info::VGM_FILE_GD3_VERSION) {
			throw UnsupportedFormatException("Unsupported GD3 version");
		}
	}

	const uint32_t vgmGD3Length = readUInt32(in);
	unsigned parsedCount = 0;
	// reading tags
	for (size_t i = static_cast<size_t>(Tag::title), n = static_cast<size_t>(Tag::notes); i <= n; ++i) {
		parsedCount += readTag(m_gd3Info.tags[i], in);
	}
	if (parsedCount != vgmGD3Length) {
		cerr << "skipping last " << vgmGD3Length - parsedCount << " unused bytes of the VGM GD3 header" << endl;
	}
}

void vgm::VGMFile::readData(InputStream &in)
{
	const size_t absoluteDataOffset = m_header.vgmDataOffset + VGMHeader::POS_VGM_DATA;
	const size_t absoluteEOFOffset = m_header.eofOffset + VGMHeader::POS_EOF;
	if (m_header.gd3Offset == 0) { // header -> data -> eof
		m_dataSize = absoluteEOFOffset - absoluteDataOffset;
	} else { // GD3 info exists, the data takes the space between the header and GD3
		const size_t absoluteGD3Offset = m_header.gd3Offset + VGMHeader::POS_GD3;
		if (absoluteGD3Offset > absoluteDataOffset) { // header -> data -> gd3 -> eof
			m_dataSize = absoluteGD3Offset - absoluteDataOffset;
		} else { // header -> gd3 -> data -> eof
			m_dataSize = absoluteEOFOffset - absoluteDataOffset;
		}
	}
	setPos(in, absoluteDataOffset);
	m_data = new unsigned char[m_dataSize];
	in.read(m_data, m_dataSize);
}

vgm::VGMFile vgm::VGMFile::load(const File &src)
{
	Format fmt;
	auto_ptr<InputStream> inPtr;
	// TODO use file headers to resolve the file format
	if (endsWith(src.path(), ".vgz")) {
		inPtr.reset(new GZipFileInputStream(src));
		fmt = Format::vgz;
	} else {
		inPtr.reset(new FileInputStream(src));
		fmt = Format::vgm;
	}
	VGMFile vgm;
	vgm.m_format = fmt;
	vgm.readHeader(*inPtr);
	vgm.readData(*inPtr);
	vgm.readGD3Info(*inPtr);
	return vgm;
}

void vgm::VGMFile::writeHeader(OutputStream &out) const
{
	writeUInt32(m_header.id, out);
	writeUInt32(m_header.eofOffset, out);
	writeUInt32(m_header.version, out);
	writeUInt32(m_header.snClock, out);
	writeUInt32(m_header.ym2413Clock, out);
	writeUInt32(m_header.gd3Offset, out);
	writeUInt32(m_header.sampleCount, out);
	writeUInt32(m_header.loopOffset, out);
	writeUInt32(m_header.loopSampleCount, out);
	writeUInt32(m_header.rate, out);
	writeUInt32(m_header.snFeedback, out);
	writeUInt32(m_header.ym2612Clock, out);
	writeUInt32(m_header.ym2151Clock, out);
	writeUInt32(m_header.vgmDataOffset, out);
	writeUInt32(0, out); // reserved
	writeUInt32(0, out); // reserved
}

void vgm::VGMFile::writeData(OutputStream &out) const
{
	out.write(m_data, m_dataSize);
}

void vgm::VGMFile::writeGD3Info(OutputStream &out) const
{
	writeUInt32(GD3Info::VGM_FILE_GD3_ID, out);
	writeUInt32(GD3Info::VGM_FILE_GD3_VERSION, out);
	writeUInt32(m_gd3Info.dataSize, out);
	for (size_t i = static_cast<size_t>(Tag::title), n = static_cast<size_t>(Tag::notes); i <= n; ++i) {
		writeTag(m_gd3Info.tags[i], out);
	}
}

void vgm::VGMFile::save(const File &dest, const Format format)
{
	normalise();

	auto_ptr<OutputStream> outPtr;
	if (format == Format::vgz) {
		outPtr.reset(new GZipFileOutputStream(dest));
	} else {
		outPtr.reset(new FileOutputStream(dest));
	}
	writeHeader(*outPtr);
	writeData(*outPtr);
	writeGD3Info(*outPtr);
}

void vgm::VGMFile::normalise()
{
	unsigned size = 0;
	for (size_t i = static_cast<size_t>(Tag::title), n = static_cast<size_t>(Tag::notes); i <= n; ++i) {
		size += m_gd3Info.tags[i].size() + 1; // '\0' must be counted too
	}
	m_gd3Info.dataSize = size * 2; // UTF16-LE is used for GD3

	m_header.vgmDataOffset = VGMHeader::HEADER_SIZE - VGMHeader::POS_VGM_DATA; // forcing the VGM data to start at absolute offset 0x40

	const size_t fileSize = VGMHeader::HEADER_SIZE + m_dataSize + GD3Info::HEADER_SIZE + m_gd3Info.dataSize;
	m_header.eofOffset = fileSize - VGMHeader::POS_EOF;
}

void vgm::VGMFile::setTag(const Tag name, const u16string &value)
{
	m_gd3Info.tags[static_cast<size_t>(name)] = value;
}

const u16string &vgm::VGMFile::getTag(const Tag name) const
{
	return m_gd3Info.tags[static_cast<size_t>(name)];
}
