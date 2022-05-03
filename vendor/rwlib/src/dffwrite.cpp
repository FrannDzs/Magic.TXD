/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/dffwrite.cpp
*  PURPOSE:     Old DFF implementation
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include <cstring>

#include "StdInc.h"

#include "streamutil.hxx"

#if 0

#ifdef _MSC_VER
#pragma warning(disable: 4244)
#pragma warning(disable: 4267)
#endif //_MSC_VER

namespace rw {

/*
 * Clump
 */

uint32 Clump::write(ostream &rw)
{
    LibraryVersion version = this->engineInterface->GetVersion();

	HeaderInfo header;
    header.setVersion( version );
	uint32 writtenBytesReturn;

	/*
	 * writtenBytesReturn will always contain the number of bytes
	 * written in a sub-block, it is used like a return value.
	 */

	// Clump
	SKIP_HEADER();

#if 0

	// Struct
	{
		SKIP_HEADER();
		bytesWritten += writeUInt32(atomicList.size(), rw);

        if (version.rwLibMinor != 0 && version.rwLibMinor != 2)
        {
			bytesWritten += writeUInt32(0, rw);
			bytesWritten += writeUInt32(0, rw);
		}
		WRITE_HEADER(CHUNK_STRUCT);
	}
	bytesWritten += writtenBytesReturn;

	// Frame List
	{
		SKIP_HEADER();

		// Struct
		{
			SKIP_HEADER();
			bytesWritten += writeUInt32(frameList.size(), rw);
			for (uint32 i = 0; i < frameList.size(); i++)
				bytesWritten += frameList[i].writeStruct(rw);
			WRITE_HEADER(CHUNK_STRUCT);
		}
		bytesWritten += writtenBytesReturn;

		// Extensions
		for (uint32 i = 0; i < frameList.size(); i++)
			bytesWritten += frameList[i].writeExtension(rw);

		WRITE_HEADER(CHUNK_FRAMELIST);
	}
	bytesWritten += writtenBytesReturn;

	// Geometry List
	{
		SKIP_HEADER();

		// Struct
		{
			SKIP_HEADER();
			bytesWritten += writeUInt32(geometryList.size(), rw);
			WRITE_HEADER(CHUNK_STRUCT);
		}
		bytesWritten += writtenBytesReturn;

		// Geometries
		for (uint32 i = 0; i < geometryList.size(); i++)
			bytesWritten += geometryList[i].write(rw);

		WRITE_HEADER(CHUNK_GEOMETRYLIST);
	}
	bytesWritten += writtenBytesReturn;

	// Atomics
	for (uint32 i = 0; i < atomicList.size(); i++)
    {
		bytesWritten += atomicList[i].write(rw);
    }

#endif

	// Extension
	{
		SKIP_HEADER();
		WRITE_HEADER(CHUNK_EXTENSION);
	}
	bytesWritten += writtenBytesReturn;

	WRITE_HEADER(CHUNK_CLUMP);

	return bytesWritten;
}

/*
 * Atomic
 */

uint32 Atomic::write(ostream &rw)
{
	HeaderInfo header;
    header.setVersion( this->engineInterface->GetVersion() );
	uint32 writtenBytesReturn;

	// Atomic
	SKIP_HEADER();

	// Struct
	{
		SKIP_HEADER();
		bytesWritten += writeUInt32(frameIndex, rw);
		bytesWritten += writeUInt32(geometryIndex, rw);
		bytesWritten += writeUInt32(5, rw);
		bytesWritten += writeUInt32(0, rw);
		WRITE_HEADER(CHUNK_STRUCT);
	}
	bytesWritten += writtenBytesReturn;

	// Extension
	{
		SKIP_HEADER();

		// Right To Render
		writtenBytesReturn = 0;
		if (hasRightToRender) {
			SKIP_HEADER();
			bytesWritten += writeUInt32(rightToRenderVal1, rw);
			bytesWritten += writeUInt32(rightToRenderVal2, rw);
			WRITE_HEADER(CHUNK_RIGHTTORENDER);
		}
		bytesWritten += writtenBytesReturn;

		// Particles
		writtenBytesReturn = 0;
		if (hasParticles) {
			SKIP_HEADER();
			bytesWritten += writeUInt32(particlesVal, rw);
			WRITE_HEADER(CHUNK_PARTICLES);
		}
		bytesWritten += writtenBytesReturn;

		// Pipeline set
		writtenBytesReturn = 0;
		if (hasPipelineSet) {
			SKIP_HEADER();
			bytesWritten += writeUInt32(pipelineSetVal, rw);
			WRITE_HEADER(CHUNK_PIPELINESET);
		}
		bytesWritten += writtenBytesReturn;

		// Material Fx
		writtenBytesReturn = 0;
		if (hasMaterialFx) {
			SKIP_HEADER();
			bytesWritten += writeUInt32(materialFxVal, rw);
			WRITE_HEADER(CHUNK_MATERIALEFFECTS);
		}
		bytesWritten += writtenBytesReturn;

		WRITE_HEADER(CHUNK_EXTENSION);
	}
	bytesWritten += writtenBytesReturn;

	WRITE_HEADER(CHUNK_ATOMIC);

	return bytesWritten;
}

/*
 * Frame
 */

// only writes part of the frame struct
uint32 Frame::writeStruct(ostream &rw)
{
	uint32 bytesWritten = 0;
	rw.write((char *) (rotationMatrix), 9*sizeof(float32));
	bytesWritten += 9*sizeof(float32);
	rw.write((char *) (position), 3*sizeof(float32));
	bytesWritten += 3*sizeof(float32);
	bytesWritten += writeInt32(parent, rw);
	/* matrix creation flags; not used, only written */
	bytesWritten += writeInt32(0, rw);
	return bytesWritten;
}

uint32 Frame::writeExtension(ostream &rw)
{
    HeaderInfo header;
    header.setVersion( this->engineInterface->GetVersion() );
	uint32 writtenBytesReturn;

	// Extension
	SKIP_HEADER();

	// Frame
	writtenBytesReturn = 0;
	if (name.length() > 0) {
		SKIP_HEADER();
		rw.write(name.c_str(), name.length());
		bytesWritten += name.length();
		WRITE_HEADER(CHUNK_FRAME);
	}
	bytesWritten += writtenBytesReturn;

	// HAnim
	writtenBytesReturn = 0;
	if (hasHAnim) {
		SKIP_HEADER();
		bytesWritten += writeUInt32(hAnimUnknown1, rw);
		bytesWritten += writeInt32(hAnimBoneId, rw);
		bytesWritten += writeUInt32(hAnimBoneCount, rw);
		if (hAnimBoneCount != 0) {
			bytesWritten += writeUInt32(hAnimUnknown2, rw);
			bytesWritten += writeUInt32(hAnimUnknown3, rw);
		}
		for (uint32 i = 0; i < hAnimBoneCount; i++) {
			bytesWritten += writeInt32(hAnimBoneIds[i], rw);
			bytesWritten += writeUInt32(hAnimBoneNumbers[i], rw);
			bytesWritten += writeUInt32(hAnimBoneTypes[i], rw);
		}
		WRITE_HEADER(CHUNK_HANIM);
	}
	bytesWritten += writtenBytesReturn;

	WRITE_HEADER(CHUNK_EXTENSION);
	return bytesWritten;
}

/*
 * Geometry
 */

uint32 Geometry::write(ostream &rw)
{
    LibraryVersion version = this->engineInterface->GetVersion();

    HeaderInfo header;
    header.setVersion( version );
	uint32 writtenBytesReturn;

	// Geometry
	SKIP_HEADER();

	// Struct
	{
		SKIP_HEADER();

		if (faces.size() == 0)
			generateFaces();

		bytesWritten += writeUInt16(flags, rw);
		if (flags & FLAGS_TEXTURED2)
			bytesWritten += writeUInt8(numUVs, rw);
		else
			bytesWritten += writeUInt8(0, rw);

		/* we can't write native geometry */
		bytesWritten += writeUInt8(0, rw);

		uint32 triangleCount = faces.size() / 4;
		vertexCount = vertices.size() / 3;
		bytesWritten += writeUInt32(triangleCount, rw);
		bytesWritten += writeUInt32(vertexCount, rw);
		/* morph targets are always just 1 */
		bytesWritten += writeUInt32(1, rw);

		if (version.rwLibMinor == 0 || version.rwLibMinor == 1 || version.rwLibMinor == 3)
        {
			bytesWritten += writeFloat32(1.0f, rw);
			bytesWritten += writeFloat32(1.0f, rw);
			bytesWritten += writeFloat32(1.0f, rw);
		}

		if (flags & FLAGS_PRELIT) {
			rw.write((char *) (&vertexColors[0]),
			         4*vertexCount*sizeof(uint8));
			bytesWritten += 4*vertexCount*sizeof(uint8);
		}
		if (flags & FLAGS_TEXTURED) {
			rw.write((char *) (&texCoords[0][0]),
			         2*vertexCount*sizeof(float32));
			bytesWritten += 2*vertexCount*sizeof(float32);
		}
		if (flags & FLAGS_TEXTURED2) {
			for (uint32 i = 0; i < numUVs; i++) {
				rw.write((char *)
				          (&texCoords[i][0]),
					  2*vertexCount*sizeof(float32));
				bytesWritten += 2*vertexCount*sizeof(float32);
			}
		}
		rw.write((char *) (&faces[0]),
		         4*triangleCount*sizeof(uint16));
		bytesWritten += 4*triangleCount*sizeof(uint16);

		// Morph Targets (always 1)
		// Bounding Sphere
		rw.write((char *) boundingSphere, 4*sizeof(float32));
		bytesWritten += 4*sizeof(float32);

		bytesWritten += writeUInt32(hasPositions, rw);
		bytesWritten += writeUInt32(hasNormals, rw);
		rw.write((char *) (&vertices[0]),
		         3*vertexCount*sizeof(float32));
		bytesWritten += 3*vertexCount*sizeof(float32);

		if (flags & FLAGS_NORMALS) {
			rw.write((char *) (&normals[0]),
				 3*vertexCount*sizeof(float32));
			bytesWritten += 3*vertexCount*sizeof(float32);
		}

		WRITE_HEADER(CHUNK_STRUCT);
	}
	bytesWritten += writtenBytesReturn;

	// Material List
	{
		SKIP_HEADER();

		// Struct
		{
			SKIP_HEADER();
			bytesWritten += writeUInt32(materialList.size(), rw);
			for (uint32 i = 0; i < materialList.size(); i++)
				bytesWritten += writeInt32(-1, rw);
			WRITE_HEADER(CHUNK_STRUCT);
		}
		bytesWritten += writtenBytesReturn;

#if 0

		// Materials
		for (uint32 i = 0; i < materialList.size(); i++)
			bytesWritten += materialList[i].write(rw);

#endif

		WRITE_HEADER(CHUNK_MATLIST);
	}
	bytesWritten += writtenBytesReturn;

	// Extensions
	{
		SKIP_HEADER();

		// Bin Mesh
		{
			SKIP_HEADER();
			bytesWritten += writeUInt32(faceType, rw);
			bytesWritten += writeUInt32(splits.size(), rw);
			bytesWritten += writeUInt32(numIndices, rw);
			for (uint32 i = 0; i < splits.size(); i++) {
				uint32 indexCount = splits[i].indices.size();
				bytesWritten += writeUInt32(indexCount, rw);
				bytesWritten += writeUInt32(splits[i].matIndex,
				                            rw);
				for (uint32 j = 0; j < indexCount; j++)
					bytesWritten += writeUInt32(
					  splits[i].indices[j], rw);
			}
			WRITE_HEADER(CHUNK_BINMESH);
		}
		bytesWritten += writtenBytesReturn;

		// Mesh extension
		if (hasMeshExtension)
			bytesWritten += writeMeshExtension(rw);

		// Night vertex Colors
		writtenBytesReturn = 0;
		if (hasNightColors) {
			SKIP_HEADER();
			bytesWritten += writeUInt32(nightColorsUnknown, rw);
			if (nightColorsUnknown != 0) {
				rw.write((char *) (&nightColors[0]),
				   nightColors.size()*sizeof(uint8));
				bytesWritten+= nightColors.size()*sizeof(uint8);
			}
			WRITE_HEADER(CHUNK_NIGHTVERTEXCOLOR);
		}
		bytesWritten += writtenBytesReturn;

		// 2dfx

		// Skin
		writtenBytesReturn = 0;
		if (hasSkin) {
			SKIP_HEADER();
			bytesWritten += writeUInt8(boneCount, rw);
			bytesWritten += writeUInt8(specialIndexCount, rw);
			bytesWritten += writeUInt8(unknown1, rw);
			bytesWritten += writeUInt8(unknown2, rw);

			rw.write((char *) (&specialIndices[0]),
				 specialIndexCount*sizeof(uint8));
			bytesWritten += specialIndexCount*sizeof(uint8);

			rw.write((char *) (&vertexBoneIndices[0]),
				 vertexCount*sizeof(uint32));
			bytesWritten += vertexCount*sizeof(uint32);

			rw.write((char *) (&vertexBoneWeights[0]),
				 vertexCount*4*sizeof(float32));
			bytesWritten += vertexCount*4*sizeof(float32);

			for (uint32 i = 0; i < boneCount; i++) {
				if (specialIndexCount == 0)
					bytesWritten +=
					  writeUInt32(0xdeaddead, rw);
				rw.write((char *) (&inverseMatrices[i*16]),
					 16*sizeof(float32));
				bytesWritten += 16*sizeof(float32);
			}

			if (specialIndexCount != 0) {
				rw.seekp(0x0C, ios::cur);
				bytesWritten += 0x0C;
			}

			WRITE_HEADER(CHUNK_SKIN);
		}
		bytesWritten += writtenBytesReturn;

		// Morph
		writtenBytesReturn = 0;
		if (hasMorph) {
			SKIP_HEADER();
			bytesWritten += writeUInt32(0, rw);
			WRITE_HEADER(CHUNK_MORPH);
		}
		bytesWritten += writtenBytesReturn;

		WRITE_HEADER(CHUNK_EXTENSION);
	}
	bytesWritten += writtenBytesReturn;

	WRITE_HEADER(CHUNK_GEOMETRY);

	return bytesWritten;
}

uint32 Geometry::writeMeshExtension(ostream &rw)
{
    HeaderInfo header;
    header.setVersion( this->engineInterface->GetVersion() );
	uint32 writtenBytesReturn;

	(void)writtenBytesReturn;

	SKIP_HEADER();

	bytesWritten += writeUInt32(meshExtension->unknown, rw);
	if (meshExtension->unknown != 0) {
		uint32 vertexCount = meshExtension->vertices.size() / 3;
		uint32 faceCount = meshExtension->faces.size() / 3;
		uint32 materialCount = meshExtension->textureName.size();

		bytesWritten += writeUInt32(1, rw);
		bytesWritten += writeUInt32(vertexCount, rw);
		rw.seekp(0xC, ios::cur);
		bytesWritten += 0xC;
		bytesWritten += writeUInt32(faceCount, rw);
		rw.seekp(0x8, ios::cur);
		bytesWritten += 0x8;
		bytesWritten += writeUInt32(materialCount, rw);
		rw.seekp(0x10, ios::cur);
		bytesWritten += 0x10;

		rw.write((char *) (&meshExtension->vertices[0]),
			  3*vertexCount*sizeof(float32));
		bytesWritten += 3*vertexCount*sizeof(float32);
		rw.write((char *) (&meshExtension->texCoords[0]),
			  2*vertexCount*sizeof(float32));
		bytesWritten += 2*vertexCount*sizeof(float32);
		rw.write((char *) (&meshExtension->vertexColors[0]),
			  4*vertexCount*sizeof(uint8));
		bytesWritten += 4*vertexCount*sizeof(uint8);
		rw.write((char *) (&meshExtension->faces[0]),
			  3*faceCount*sizeof(uint16));
		bytesWritten += 3*faceCount*sizeof(uint16);
		rw.write((char *) (&meshExtension->assignment[0]),
			  faceCount*sizeof(uint16));
		bytesWritten += faceCount*sizeof(uint16);

		char buffer[0x20];
		for (uint32 i = 0; i < materialCount; i++) {
			memset(buffer, 0, 0x20);
			strncpy(buffer, meshExtension->textureName[i].c_str(),
			        0x20);
			rw.write(buffer, 0x20);
			bytesWritten += 0x20;

			memset(buffer, 0, 0x20);
			strncpy(buffer, meshExtension->maskName[i].c_str(),
			        0x20);
			rw.write(buffer, 0x20);
			bytesWritten += 0x20;

			bytesWritten +=
			  writeFloat32(meshExtension->unknowns[i*3+0], rw);
			bytesWritten +=
			  writeFloat32(meshExtension->unknowns[i*3+1], rw);
			bytesWritten +=
			  writeFloat32(meshExtension->unknowns[i*3+2], rw);
		}
	}

	WRITE_HEADER(CHUNK_MESHEXTENSION);

	return bytesWritten;
}

/*
 * Material
 */

uint32 Material::write(ostream &rw)
{
    HeaderInfo header;
    header.setVersion( this->engineInterface->GetVersion() );
	uint32 writtenBytesReturn;

	// Material
	SKIP_HEADER();

	// Struct
	{
		SKIP_HEADER();
		bytesWritten += writeUInt32(flags, rw);
		rw.write((char *) (color), 4*sizeof(uint8));
		bytesWritten += 4*sizeof(uint8);
		bytesWritten += writeInt32(unknown, rw);
		bytesWritten += writeInt32(hasTex, rw);
		rw.write((char *) (surfaceProps),
		          3*sizeof(float32));
		bytesWritten += 3*sizeof(float32);

		WRITE_HEADER(CHUNK_STRUCT);
	}
	bytesWritten += writtenBytesReturn;

#if 0
	// Texture
	if (hasTex)
    {
		bytesWritten += texture.write(rw);
    }
#endif

	// Extensions
	{
		SKIP_HEADER();

		// Right To Render
		writtenBytesReturn = 0;
		if (hasRightToRender) {
			SKIP_HEADER();
			bytesWritten += writeUInt32(rightToRenderVal1, rw);
			bytesWritten += writeUInt32(rightToRenderVal2, rw);
			WRITE_HEADER(CHUNK_RIGHTTORENDER);
		}
		bytesWritten += writtenBytesReturn;

		// Mat fx
		writtenBytesReturn = 0;
		if (hasMatFx) {
			SKIP_HEADER();
			switch (matFx->type) {
			case MATFX_BUMPMAP: {
				bytesWritten += writeUInt32(MATFX_BUMPMAP, rw);
				bytesWritten += writeUInt32(MATFX_BUMPMAP, rw);
				bytesWritten += writeFloat32(
				                  matFx->bumpCoefficient, rw);

#if 0

				bytesWritten += writeUInt32(matFx->hasTex1,rw);
				if (matFx->hasTex1)
					bytesWritten += matFx->tex1.write(rw);

				bytesWritten += writeUInt32(matFx->hasTex2,rw);
				if (matFx->hasTex2)
					bytesWritten += matFx->tex2.write(rw);

#endif

				bytesWritten += writeUInt32(0, rw);
				break;
			} case MATFX_ENVMAP: {
				bytesWritten += writeUInt32(MATFX_ENVMAP, rw);
				bytesWritten += writeUInt32(MATFX_ENVMAP, rw);
				bytesWritten += writeFloat32(
				                  matFx->envCoefficient, rw);

#if 0

				bytesWritten += writeUInt32(matFx->hasTex1,rw);
				if (matFx->hasTex1)
					bytesWritten += matFx->tex1.write(rw);

				bytesWritten += writeUInt32(matFx->hasTex2,rw);
				if (matFx->hasTex2)
					bytesWritten += matFx->tex2.write(rw);

#endif

				bytesWritten += writeUInt32(0, rw);
				break;
			} case MATFX_BUMPENVMAP: {
				bytesWritten += writeUInt32(MATFX_BUMPENVMAP,rw);

#if 0
				bytesWritten += writeUInt32(MATFX_BUMPMAP, rw);
				bytesWritten += writeFloat32(matFx->bumpCoefficient, rw);
				bytesWritten += writeUInt32(matFx->hasTex1,rw);
				if (matFx->hasTex1)
					bytesWritten += matFx->tex1.write(rw);
				bytesWritten += writeUInt32(0, rw);

				bytesWritten += writeUInt32(MATFX_ENVMAP, rw);
				bytesWritten += writeFloat32(matFx->envCoefficient, rw);
				bytesWritten += writeUInt32(0, rw);
				bytesWritten += writeUInt32(matFx->hasTex2,rw);
				if (matFx->hasTex2)
					bytesWritten += matFx->tex2.write(rw);

#endif

				break;
			} case MATFX_DUAL: {
				bytesWritten += writeUInt32(MATFX_DUAL, rw);
				bytesWritten += writeUInt32(MATFX_DUAL, rw);
				bytesWritten += writeFloat32(matFx->srcBlend,rw);
				bytesWritten += writeFloat32(matFx->destBlend,rw);

#if 0

				bytesWritten += writeUInt32(matFx->hasDualPassMap, rw);
				if (matFx->hasDualPassMap)
                {
					bytesWritten += matFx->dualPassMap.write(rw);
                }
				bytesWritten += writeUInt32(0, rw);

#endif

				break;
			} case MATFX_UVTRANSFORM: {
				bytesWritten += writeUInt32(MATFX_UVTRANSFORM,
				                            rw);
				bytesWritten += writeUInt32(0, rw);
				break;
			} default:
				break;
			}
			WRITE_HEADER(CHUNK_MATERIALEFFECTS);
		}
		bytesWritten += writtenBytesReturn;

		// Reflection Mat
		writtenBytesReturn = 0;
		if (hasReflectionMat) {
			SKIP_HEADER();
			bytesWritten += writeFloat32(reflectionChannelAmount[0],
			                             rw);
			bytesWritten += writeFloat32(reflectionChannelAmount[1],
			                             rw);
			bytesWritten += writeFloat32(reflectionChannelAmount[2],
			                             rw);
			bytesWritten += writeFloat32(reflectionChannelAmount[3],
			                             rw);
			bytesWritten += writeFloat32(reflectionIntensity, rw);
			bytesWritten += writeFloat32(0, rw);
			WRITE_HEADER(CHUNK_REFLECTIONMAT);
		}
		bytesWritten += writtenBytesReturn;

		// Specular Mat
		writtenBytesReturn = 0;
		if (hasSpecularMat) {
			SKIP_HEADER();
			bytesWritten += writeFloat32(specularLevel, rw);
			uint32 len = specularName.length()+1;
			rw.write(specularName.c_str(), len);
			bytesWritten += len;
			if (len % 4 != 0) {
				rw.seekp(4 - len % 4, ios::cur);
				bytesWritten += 4 - len % 4;
			}
			rw.seekp(4, ios::cur);
			bytesWritten += 4;
			WRITE_HEADER(CHUNK_SPECULARMAT);
		}
		bytesWritten += writtenBytesReturn;

		// UV Anim

		WRITE_HEADER(CHUNK_EXTENSION);
	}
	bytesWritten += writtenBytesReturn;


	WRITE_HEADER(CHUNK_MATERIAL);

	return bytesWritten;
}

/*
 * Texture
 */

uint32 Texture::write(ostream &rw)
{
	HeaderInfo header;
    header.setVersion( this->engineInterface->GetVersion() );
	uint32 writtenBytesReturn;

	// Material
	SKIP_HEADER();

	// Struct
	{
		SKIP_HEADER();
		bytesWritten += writeUInt16(filterFlags, rw);
		bytesWritten += writeUInt16(0, rw);
		WRITE_HEADER(CHUNK_STRUCT);
	}
	bytesWritten += writtenBytesReturn;

	// String -- Texture name
	{
		SKIP_HEADER();
		uint32 len = name.length()+1;
		rw.write(name.c_str(), len);
		bytesWritten += len;
		if (len % 4 != 0) {
			rw.seekp(4 - len % 4, ios::cur);
			bytesWritten += 4 - len % 4;
		}
		WRITE_HEADER(CHUNK_STRING);
	}
	bytesWritten += writtenBytesReturn;

	// String -- Mask name
	{
		SKIP_HEADER();
		uint32 len = maskName.length()+1;
		rw.write(maskName.c_str(), len);
		bytesWritten += len;
		if (len % 4 != 0) {
			rw.seekp(4 - len % 4, ios::cur);
			bytesWritten += 4 - len % 4;
		}
		WRITE_HEADER(CHUNK_STRING);
	}
	bytesWritten += writtenBytesReturn;

	// Extensions
	{
		SKIP_HEADER();

		// Sky Mipmap Val
		writtenBytesReturn = 0;
		if (hasSkyMipmap) {
			SKIP_HEADER();
			bytesWritten += writeUInt32(0xFC0, rw);
			WRITE_HEADER(CHUNK_SKYMIPMAP);
		}
		bytesWritten += writtenBytesReturn;

		WRITE_HEADER(CHUNK_EXTENSION);
	}
	bytesWritten += writtenBytesReturn;

	WRITE_HEADER(CHUNK_TEXTURE);

	return bytesWritten;
}

};

#endif //disabled