/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/ps2native.cpp
*  PURPOSE:     Old PlayStation2 stuff
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#include <cstdio>

#include "streamutil.hxx"

#if 0

#ifdef _MSC_VER
#pragma warning(disable: 4244)
#pragma warning(disable: 4267)
#pragma warning(disable: 4390)
#endif //_MSC_VER

using namespace std;

namespace rw {

#define NORMALSCALE (1.0/128.0)
#define	VERTSCALE1 (1.0/128.0)	/* normally used */
#define	VERTSCALE2 (1.0/1024.0)	/* used by objects with normals */
#define	UVSCALE (1.0/4096.0)

static uint32 index;

void Geometry::readPs2NativeData(istream &rw)
{
	HeaderInfo header;

	READ_HEADER(CHUNK_STRUCT); /* wrong size */

	if (readUInt32(rw) != PLATFORM_PS2) {
		cerr << "error: native data not in ps2 format\n";
		return;
	}

	index = 0;
	vector<uint32> typesRead;
	numIndices = 0;
	for (uint32 i = 0; i < splits.size(); i++) {
		uint32 splitSize = readUInt32(rw);
		rw.seekg(4, ios::cur);	// bool: hasNoSectionAData

		uint32 numIndices = splits[i].indices.size();
		splits[i].indices.clear();
		uint32 end = splitSize + rw.tellg();

		uint8 chunk8[16];
		uint32 *chunk32 = (uint32 *) chunk8;

		uint32 blockStart = rw.tellg();
		bool reachedEnd;
		bool sectionALast = false;
		bool sectionBLast = false;
		bool dataAread = false;
		while (rw.tellg() < end) {
			/* sectionA */
			reachedEnd = false;
			while (!reachedEnd && !sectionALast) {
				rw.read((char *) chunk8, 0x10);
				switch (chunk8[3]) {
				case 0x30: {
					/* read all split data when we find the
					 * first data block and ignore all
					 * other blocks */
					if (dataAread) {
						/* skip dummy data */
						rw.seekg(0x10, ios::cur);
						break;
					}
					uint32 oldPos = rw.tellg();
					uint32 dataPos = blockStart +
					                   chunk32[1]*0x10;
					rw.seekg(dataPos, ios::beg);
					readData(numIndices,chunk32[3], i, rw);
					rw.seekg(oldPos + 0x10, ios::beg);
					break;
				}
				case 0x60:
					sectionALast = true;
					/* fall through */
				case 0x10:
					reachedEnd = true;
					dataAread = true;
					break;
				default:
					break;
				}
			}

			/* sectionB */
			reachedEnd = false;
			while (!reachedEnd && !sectionBLast) {
				rw.read((char *) chunk8, 0x10);
				switch (chunk8[3]) {
				case 0x00:
				case 0x07:
					readData(chunk8[14],chunk32[3], i, rw);
					/* remember what sort of data we read */
					typesRead.push_back(chunk32[3]);
					break;
				case 0x04:
					if (chunk8[7] == 0x15)
						;//first
					else if (chunk8[7] == 0x17)
						;//not first

					if ((chunk8[11] == 0x11 &&
					      chunk8[15] == 0x11) ||
					    (chunk8[11] == 0x11 &&
					      chunk8[15] == 0x06)) {
						// last
						rw.seekg(end, ios::beg);
						typesRead.clear();
						sectionBLast = true;
					} else if (chunk8[11] == 0 &&
					           chunk8[15] == 0 &&
					           faceType == FACETYPE_STRIP) {
						deleteOverlapping(typesRead, i);
						typesRead.clear();
						// not last
					}
					reachedEnd = true;
					break;
				default:
					break;
				}
			}
		}
		this->numIndices += splits[i].indices.size();
	}

	int nverts = vertices.size()/3;
	// this happens in some objects
	if(flags & FLAGS_NORMALS && normals.size() == 0)
		normals.resize(vertices.size());
	// never seen this but be careful
	if(flags & FLAGS_PRELIT && vertexColors.size() == 0)
		vertexColors.resize(nverts*4);
	// this also happens
	if(flags & FLAGS_TEXTURED || flags & FLAGS_TEXTURED2)
		for(uint32 i = 0; i < numUVs; i++)
			if(texCoords[i].size() == 0)
				texCoords[i].resize(nverts*2);
}


void Geometry::readData(uint32 vertexCount, uint32 type,
                        uint32 split, istream &rw)
{
	float32 vertexScale = (flags & FLAGS_PRELIT) ? (float32)VERTSCALE1 : (float32)VERTSCALE2;

	uint32 size = 0;
	type &= 0xFF00FFFF;
	/* TODO: read greater chunks */
	switch (type) {
	/* Vertices */
	case 0x68008000: {
		size = 3 * sizeof(float32);
		for (uint32 j = 0; j < vertexCount; j++) {
			vertices.push_back(readFloat32(rw));
			vertices.push_back(readFloat32(rw));
			vertices.push_back(readFloat32(rw));
			splits[split].indices.push_back(index++);
		}
		break;
	} case 0x6D008000: {
		size = 4 * sizeof(int16);
		int16 vertex[4];
		for (uint32 j = 0; j < vertexCount; j++) {
			rw.read((char *) (vertex), size);
			uint32 flag = vertex[3] & 0xFFFF;
			vertices.push_back(vertex[0] * vertexScale);
			vertices.push_back(vertex[1] * vertexScale);
			vertices.push_back(vertex[2] * vertexScale);
			if (flag == 0x8000)
				splits[split].indices.push_back(index-1);
			splits[split].indices.push_back(index++);
		}
		break;
	/* Texture coordinates */
	} case 0x64008001: {
		size = 2 * sizeof(float32);
		for (uint32 j = 0; j < vertexCount; j++) {
			texCoords[0].push_back(readFloat32(rw));
			texCoords[0].push_back(readFloat32(rw));
		}
		for (uint32 i = 1; i < numUVs; i++) {
			for (uint32 j = 0; j < vertexCount; j++) {
				texCoords[i].push_back(0);
				texCoords[i].push_back(0);
			}
		}
		break;
	} case 0x6D008001: {
		size = 2 * sizeof(int16);
		int16 texCoord[2];
		for (uint32 j = 0; j < vertexCount; j++) {
			for (uint32 i = 0; i < numUVs; i++) {
				rw.read((char *) (texCoord), size);
				texCoords[i].push_back(texCoord[0] * (float32)UVSCALE);
				texCoords[i].push_back(texCoord[1] * (float32)UVSCALE);
			}
		}
		size *= numUVs;
		break;
	} case 0x65008001: {
		size = 2 * sizeof(int16);
		int16 texCoord[2];
		for (uint32 j = 0; j < vertexCount; j++) {
			rw.read((char *) (texCoord), size);
			texCoords[0].push_back(texCoord[0] * (float32)UVSCALE);
			texCoords[0].push_back(texCoord[1] * (float32)UVSCALE);
		}
		for (uint32 i = 1; i < numUVs; i++) {
			for (uint32 j = 0; j < vertexCount; j++) {
				texCoords[i].push_back(0);
				texCoords[i].push_back(0);
			}
		}
		break;
	/* Vertex colors */
	} case 0x6D00C002: {
		size = 8 * sizeof(uint8);
		for (uint32 j = 0; j < vertexCount; j++) {
			vertexColors.push_back(readUInt8(rw));
			nightColors.push_back(readUInt8(rw));
			vertexColors.push_back(readUInt8(rw));
			nightColors.push_back(readUInt8(rw));
			vertexColors.push_back(readUInt8(rw));
			nightColors.push_back(readUInt8(rw));
			vertexColors.push_back(readUInt8(rw));
			nightColors.push_back(readUInt8(rw));
		}
		break;
	} case 0x6E00C002: {
		size = 4 * sizeof(uint8);
		for (uint32 j = 0; j < vertexCount; j++) {
			vertexColors.push_back(readUInt8(rw));
			vertexColors.push_back(readUInt8(rw));
			vertexColors.push_back(readUInt8(rw));
			vertexColors.push_back(readUInt8(rw));
		}
		break;
	/* Normals */
	} case 0x6E008002: case 0x6E008003: {
		size = 4 * sizeof(int8);
		int8 normal[4];
		for (uint32 j = 0; j < vertexCount; j++) {
			rw.read((char *) (normal), size);
			normals.push_back(normal[0] * (float32)NORMALSCALE);
			normals.push_back(normal[1] * (float32)NORMALSCALE);
			normals.push_back(normal[2] * (float32)NORMALSCALE);
		}
		break;
	} case 0x6A008003: {
		size = 3 * sizeof(int8);
		int8 normal[3];
		for (uint32 j = 0; j < vertexCount; j++) {
			rw.read((char *) (normal), size);
			normals.push_back(normal[0] * (float32)NORMALSCALE);
			normals.push_back(normal[1] * (float32)NORMALSCALE);
			normals.push_back(normal[2] * (float32)NORMALSCALE);
		}
		break;
	/* Skin weights and indices */
	} case 0x6C008004: case 0x6C008003: case 0x6C008001: {
		size = 4 * sizeof(float32);
		float32 weight[4];
		uint32 *w = (uint32 *) weight;
		uint8 indices[4];;
		for (uint32 j = 0; j < vertexCount; j++) {
			rw.read((char *) (weight), size);
			for (uint32 i = 0; i < 4; i++) {
				vertexBoneWeights.push_back(weight[i]);
				indices[i] = w[i] >> 2;
				if (indices[i] != 0)
					indices[i] -= 1;
			}
			vertexBoneIndices.push_back(indices[3] << 24 |
			                            indices[2] << 16 |
			                            indices[1] << 8 |
			                            indices[0]);
		}
		break;
	}
	default:
		cout << "unknown data type: " << hex << type;
		cout << " " << hex << rw.tellg() << endl;
		break;
	}

	/* skip padding */
	if (vertexCount*size & 0xF)
		rw.seekg(0x10 - (vertexCount*size & 0xF), ios::cur);
}

void Geometry::deleteOverlapping(vector<uint32> &typesRead, uint32 split)
{
	uint32 size;
	for (uint32 i = 0; i < typesRead.size(); i++) {
		switch (typesRead[i] & 0xFF00FFFF) {
		/* Vertices */
		case 0x68008000:
		case 0x6D008000:
			size = vertices.size();
			vertices.resize(size-2*3);

			size = splits[split].indices.size();
			splits[split].indices.resize(size-2);
			index -= 2;
			break;
		/* Texture coordinates */
		case 0x64008001:
		case 0x65008001:
//			size = texCoords[0].size();
//			texCoords[0].resize(size-2*2);
//			break;
		case 0x6D008001:
			for (uint32 j = 0; j < numUVs; j++) {
				size = texCoords[j].size();
				texCoords[j].resize(size-2*2);
			}
			break;
		/* Vertex colors */
		case 0x6D00C002:
			size = nightColors.size();
			nightColors.resize(size-2*4);
			/* fall through */
		case 0x6E00C002:
			size = vertexColors.size();
			vertexColors.resize(size-2*4);
			break;
		/* Normals */
		case 0x6E008002:
		case 0x6E008003:
		case 0x6A008003:
			size = normals.size();
			normals.resize(size-2*3);
			break;
		/* Skin weights and indices*/
		case 0x6C008004:
		case 0x6C008003:
		case 0x6C008001:
			size = vertexBoneWeights.size();
			vertexBoneWeights.resize(size-2*4);

			size = vertexBoneIndices.size();
			vertexBoneIndices.resize(size-2);
			break;
		default:
			cout << "unknown data type: ";
			cout << hex <<typesRead[i] << endl;
			break;
		}
	}
}

}

#endif //disabled.