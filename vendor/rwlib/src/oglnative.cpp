/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/oglnative.cpp
*  PURPOSE:     Old OpenGL stuff
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#include <cstring>

#include "streamutil.hxx"

#ifdef _MSC_VER
#pragma warning(disable: 4244)
#endif //_MSC_VER

namespace rw {

static void
convertattrib(float *dst, char *data, int type, int normalized, int n)
{
	unsigned char *uc;
	short *sh;
	unsigned short *ush;
	enum {
		FLOAT = 0,
		BYTE,
		UBYTE,
		SHORT,
		USHORT
	};

	switch(type){
	case FLOAT:
		memcpy(dst, data, n*sizeof(float));
		break;

	case BYTE:
		for(int i = 0; i < n; i++){
			dst[i] = data[i];
			if(normalized)
				dst[i] /= 128.0f;
		}
		break;

	case UBYTE:
		uc = (unsigned char*) data;
		for(int i = 0; i < n; i++){
			dst[i] = uc[i];
			if(normalized)
				dst[i] /= 255.0f;
		}
		break;

	case SHORT:
		sh = (short*) data;
		for(int i = 0; i < n; i++){
			dst[i] = sh[i];
			if(normalized)
				dst[i] /= 32768.0f;
		}
		break;

	case USHORT:
		ush = (unsigned short*) data;
		for(int i = 0; i < n; i++){
			dst[i] = ush[i];
			if(normalized)
				dst[i] /= 65536.0f;
		}
		break;
	}
}

void
Geometry::readOglNativeData(std::istream &rw, int size)
{
	uint32 nattribs;
	uint32 *attribs, *ap;
	char *data, *vdata, *dp;
	float f[4];

	enum {
		VERTICES = 0,
		UVS,
		NORMALS,
		COLORS,
		WEIGHTS,
		INDICES
	};

	/*
	 * attrib data contains the following for each attribute:
	 *   attribute type
	 *   data type
	 *   normalized
	 *   number of elements
	 *   stride
	 *   offset
	 */

	nattribs = readUInt32(rw);
	data = new char[size-sizeof(uint32)];
	rw.read(data, size-sizeof(uint32));
	attribs = (uint32*)data;
	vdata = data + nattribs*6*sizeof(uint32);

	ap = attribs;
	for(uint32 i = 0; i < nattribs; i++, ap += 6){
		dp = vdata + ap[5];
		switch(ap[0]){
		case VERTICES:
			for(uint32 j = 0; j < vertexCount; j++){
				convertattrib(f, dp, ap[1], ap[2], ap[3]);
				vertices.AddToBack(f[0]);
				vertices.AddToBack(f[1]);
				vertices.AddToBack(f[2]);
				dp += ap[4];
			}
			break;

		case UVS:
			for(uint32 j = 0; j < vertexCount; j++){
				convertattrib(f, dp, ap[1], ap[2], ap[3]);
				texCoords[0].AddToBack(f[0]/512.0f);
				texCoords[0].AddToBack(f[1]/512.0f);
				dp += ap[4];
			}
			break;

		case NORMALS:
			for(uint32 j = 0; j < vertexCount; j++){
				convertattrib(f, dp, ap[1], ap[2], ap[3]);
				normals.AddToBack(f[0]);
				normals.AddToBack(f[1]);
				normals.AddToBack(f[2]);
				dp += ap[4];
			}
			break;

		case COLORS:
			for(uint32 j = 0; j < vertexCount; j++){
				convertattrib(f, dp, ap[1], ap[2], ap[3]);
				vertexColors.AddToBack(f[0]);
				vertexColors.AddToBack(f[1]);
				vertexColors.AddToBack(f[2]);
				vertexColors.AddToBack(f[3]);
				dp += ap[4];
			}
			break;

		case WEIGHTS:
			for(uint32 j = 0; j < vertexCount; j++){
				convertattrib(f, dp, ap[1], ap[2], ap[3]);
				vertexBoneWeights.AddToBack(f[0]);
				vertexBoneWeights.AddToBack(f[1]);
				vertexBoneWeights.AddToBack(f[2]);
				vertexBoneWeights.AddToBack(f[3]);
				dp += ap[4];
			}
			break;

		case INDICES:
			for(uint32 j = 0; j < vertexCount; j++){
				convertattrib(f, dp, ap[1], ap[2], ap[3]);
				uint32 idx = ((int)f[3] << 24) |
				             ((int)f[2] << 16) |
				             ((int)f[1] << 8) | (int)f[0];
				vertexBoneIndices.AddToBack(idx);
				dp += ap[4];
			}
			break;
		}
	}
	delete[] attribs;

/*
	for(uint32 i = 0; i < vertexCount; i++){
		vertices.push_back(readFloat32(rw));
		vertices.push_back(readFloat32(rw));
		vertices.push_back(readFloat32(rw));

		if(flags & FLAGS_TEXTURED){
			texCoords[0].push_back(readUInt16(rw)/512.0f);
			texCoords[0].push_back(readUInt16(rw)/512.0f);
		}

		if(flags & FLAGS_PRELIT){
			uint8 color[4];
			rw.read(reinterpret_cast <char *>
				 (color), 4*sizeof(uint8));
			vertexColors.push_back(color[0]);
			vertexColors.push_back(color[1]);
			vertexColors.push_back(color[2]);
			vertexColors.push_back(color[3]);
		}

		if(flags & FLAGS_NORMALS){
			int8 n[4];
			rw.read(reinterpret_cast <char *>(n), 4*sizeof(int8));
			normals.push_back(n[0]/128.0f);
			normals.push_back(n[1]/128.0f);
			normals.push_back(n[2]/128.0f);
		}

		if(type == 0x5){
			int8 w[4];
			rw.read(reinterpret_cast <char *>(w), 4*sizeof(int8));
			vertexBoneWeights.push_back(w[0]/255.0f);
			vertexBoneWeights.push_back(w[1]/255.0f);
			vertexBoneWeights.push_back(w[2]/255.0f);
			vertexBoneWeights.push_back(w[3]/255.0f);
			vertexBoneIndices.push_back(readUInt32(rw));
		}
	}
*/
}

}
