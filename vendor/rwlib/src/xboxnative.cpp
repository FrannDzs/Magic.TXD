/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/xboxnative.cpp
*  PURPOSE:     Old XBOX stuff
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#include <cstdio>
#include <cstdlib>

#include "streamutil.hxx"

#ifdef _MSC_VER
#pragma warning(disable: 4267)
#pragma warning(disable: 4244)
#endif //_MSC_VER

namespace rw
{

void Geometry::readXboxNativeSkin(std::istream &rw)
{
	HeaderInfo header;

	READ_HEADER(CHUNK_STRUCT);

	if (readUInt32(rw) != PLATFORM_XBOX) {
		std::cerr << "error: native data not in xbox format\n";
		return;
	}

	// don't know if correct
	boneCount = readUInt32(rw);
	specialIndexCount = 0;
	unknown1 = 0;
	unknown2 = 0;

	uint32 vertexCount = vertices.GetCount()/3;
	int32 boneTab1[0x100];
	int32 boneTab2[0x100];
	uint32 skinHeader[4];

	rw.read((char *) boneTab1, 0x100*sizeof(int32));
	rw.read((char *) boneTab2, 0x100*sizeof(int32));
	rw.read((char *) skinHeader, 4*sizeof(uint32));
	// skinheader:
	// 0: number of used bones (tab1)
	// 1: number of weights
	// 2: pointer?
	// 3: skin data size per vertex (3*numweights)
	// tab1 maps indices to bones tab2 maps bones to indices
	uint32 numWeights = skinHeader[1];

	float32 weights[4];
	uint8 indices[4];
	for (uint32 i = 0; i < vertexCount; i++) {
		weights[0] = weights[1] = weights[2] = weights[3] = 0.0;
		indices[0] = indices[1] = indices[2] = indices[3] = 0;

		for (uint32 j = 0; j < 4; j++) {
			if (j < numWeights) {
				weights[j] = readUInt8(rw);
				weights[j] /= 255.0;
			}
			vertexBoneWeights.AddToBack(weights[j]);
		}

		for (uint32 j = 0; j < numWeights; j++) {
			indices[j] = (readUInt16(rw)/3);
			indices[j] = boneTab1[indices[j]];
		}
		vertexBoneIndices.AddToBack( indices[3] << 24 | indices[2] << 16 | indices[1] << 8 | indices[0] );
	}

	inverseMatrices.Resize(boneCount*0x10);
	for (uint32 i = 0; i < boneCount; i++)
    {
		rw.read((char *) (&inverseMatrices[i*0x10]),0x10*sizeof(float32));
    }
}

void Geometry::readXboxNativeData(std::istream &rw)
{
	HeaderInfo header;

	READ_HEADER(CHUNK_STRUCT);

	if (readUInt32(rw) != PLATFORM_XBOX) {
		std::cerr << "error: native data not in xbox format\n";
		return;
	}

	uint32 vertexPosition = rw.tellg();
	vertexPosition += readUInt32(rw);

	/* Header */
	rw.seekg(2, std::ios::cur);
	uint32 splitCount = readUInt16(rw);
	splits.Resize(splitCount);
	/* from here the index blocks are 0x10 byte aligned */
	uint32 blockStart = rw.tellg();
	uint32 flag = readUInt32(rw);
	if (flag & 1) {
		faceType = FACETYPE_LIST;
		flags &= ~FLAGS_TRISTRIP;
	} else {	// 2
		faceType = FACETYPE_STRIP;
		flags |= FLAGS_TRISTRIP;
	}
	uint32 vertexCount = readUInt32(rw);
	uint32 vertexSize = readUInt32(rw);
	rw.seekg(16, std::ios::cur);

	/* Splits */
	for (uint32 i = 0; i < splitCount; i++) {
		rw.seekg(8, std::ios::cur);
		splits[i].indices.Resize(readUInt32(rw));
		rw.seekg(12, std::ios::cur);
	}

	/* Indices */
	for (uint32 i = 0; i < splitCount; i++) {
		/* skip padding */
		uint32 pos = rw.tellg();
		if ((pos - blockStart) % 0x10 != 0)
			rw.seekg(0x10 - (pos - blockStart) % 0x10, std::ios::cur);
		for (uint32 j = 0; j < splits[i].indices.GetCount(); j++)
			splits[i].indices[j] = readUInt16(rw);
	}

	/* Vertices */
	rw.seekg(vertexPosition, std::ios::beg);

	/* known vertex sizes: 0x28, 0x20, 0x1c, 0x18, 0x14, 0x10, 0x0c */

	// only vertex size 0x28 has 3*float normals
	bool compNormal = vertexSize != 0x28;

	for (uint32 i = 0; i < vertexCount; i++) {
		vertices.AddToBack(readFloat32(rw));
		vertices.AddToBack(readFloat32(rw));
		vertices.AddToBack(readFloat32(rw));

		if (flags & FLAGS_NORMALS) {
			uint32 compNormal = readUInt32(rw);
			int32 normal[3];
			normal[0] = compNormal & 0x7FF;
			normal[1] = (compNormal & 0x3FF800) >> 11;
			normal[2] = (compNormal & 0xFFC00000) >> 22;
			if (normal[0] & 0x400) normal[0] -= 0x800;
			if (normal[1] & 0x400) normal[1] -= 0x800;
			if (normal[2] & 0x200) normal[2] -= 0x400;
			normals.AddToBack((float) normal[0] / 0x3FF);
			normals.AddToBack((float) normal[1] / 0x3FF);
			normals.AddToBack((float) normal[2] / 0x1FF);
		}

		if (flags & FLAGS_PRELIT) {
			uint8 color[4];
			rw.read(reinterpret_cast <char *> (color), 4*sizeof(uint8));
			vertexColors.AddToBack(color[2]);
			vertexColors.AddToBack(color[1]);
			vertexColors.AddToBack(color[0]);
			vertexColors.AddToBack(color[3]);
		}

		if (flags & FLAGS_TEXTURED) {
			texCoords[0].AddToBack(readFloat32(rw));
			texCoords[0].AddToBack(readFloat32(rw));
		}

		if (flags & FLAGS_TEXTURED2) {
			// TODO: don't know if this is correct
			for (uint32 j = 0; j < numUVs; j++) {
				texCoords[j].AddToBack(readFloat32(rw));
				texCoords[j].AddToBack(readFloat32(rw));
			}
		}

		if (!compNormal) {
			normals[i*3+0] = readFloat32(rw);
			normals[i*3+1] = readFloat32(rw);
			normals[i*3+2] = readFloat32(rw);
		}
	}
}

}
