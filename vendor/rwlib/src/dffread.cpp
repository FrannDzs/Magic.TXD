/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/dffread.cpp
*  PURPOSE:     Old DFF implementation
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include <cmath>

#include "StdInc.h"

#include "streamutil.hxx"

#if 0

#ifdef _MSC_VER
#pragma warning(disable: 4267)
#endif //_MSC_VER

namespace rw
{

/*
 * Clump
 */

void Clump::read(std::istream& rw)
{
	HeaderInfo header;

	// TODO: this is only a quick and dirty fix for uv anim dicts
	header.read(rw);

	if (header.getType() != CHUNK_CLUMP)
    {
		rw.seekg(header.getLength(), std::ios::cur);
		header.read(rw);

		if (header.getType() != CHUNK_CLUMP)
        {
			return;
        }
	}

	READ_HEADER(CHUNK_STRUCT);
	uint32 numAtomics = readUInt32(rw);
	uint32 numLights = 0;

	(void)numAtomics;
	(void)numLights;

	if (header.getLength() == 0xC)
    {
		numLights = readUInt32(rw);
		rw.seekg(4, std::ios::cur); /* camera count, unused in gta */
	}

#if 0
	atomicList.resize(numAtomics);

	READ_HEADER(CHUNK_FRAMELIST);

	READ_HEADER(CHUNK_STRUCT);
	uint32 numFrames = readUInt32(rw);
	frameList.resize(numFrames);
	for (uint32 i = 0; i < numFrames; i++)
    {
		frameList[i].readStruct(rw);
    }
	for (uint32 i = 0; i < numFrames; i++)
    {
		frameList[i].readExtension(rw);
    }

	READ_HEADER(CHUNK_GEOMETRYLIST);

	READ_HEADER(CHUNK_STRUCT);
	uint32 numGeometries = readUInt32(rw);
	geometryList.resize(numGeometries);

	for (uint32 i = 0; i < numGeometries; i++)
    {
		geometryList[i].read(rw);
    }

	/* read atomics */
	for (uint32 i = 0; i < numAtomics; i++)
    {
		atomicList[i].read(rw);
    }

	/* skip lights */
	for (uint32 i = 0; i < numLights; i++)
    {
		READ_HEADER(CHUNK_STRUCT);
		rw.seekg(header.getLength(), ios::cur);
		READ_HEADER(CHUNK_LIGHT);
		rw.seekg(header.getLength(), ios::cur);
	}
#endif

	readExtension(rw);
}

void Clump::readExtension(std::istream &rw)
{
	HeaderInfo header;

	READ_HEADER(CHUNK_EXTENSION);

	std::streampos end = rw.tellg();
	end += header.getLength();

	while (rw.tellg() < end)
    {
		header.read(rw);

		switch (header.getType())
        {
		case CHUNK_COLLISIONMODEL:
			rw.seekg(header.getLength(), std::ios::cur);
			break;
		default:
			rw.seekg(header.getLength(), std::ios::cur);
			break;
		}
	}
}

void Clump::dump(bool detailed)
{
#if 0

	string ind = "";
	cout << ind << "Clump {\n";
	ind += "  ";
	cout << ind << "numAtomics: " << atomicList.size() << endl;

	cout << endl << ind << "FrameList {\n";
	ind += "  ";
	cout << ind << "numFrames: " << frameList.size() << endl;
	for (uint32 i = 0; i < frameList.size(); i++)
		frameList[i].dump(i, ind);
	ind = ind.substr(0, ind.size()-2);
	cout << ind << "}\n";

	cout << endl << ind << "GeometryList {\n";
	ind += "  ";
	cout << ind << "numGeometries: " << geometryList.size() << endl;
	for (uint32 i = 0; i < geometryList.size(); i++)
		geometryList[i].dump(i, ind, detailed);

	ind = ind.substr(0, ind.size()-2);
	cout << ind << "}\n\n";

	for (uint32 i = 0; i < atomicList.size(); i++)
		atomicList[i].dump(i, ind);

	ind = ind.substr(0, ind.size()-2);
	cout << ind << "}\n";

#endif
}

void Clump::clear(void)
{
#if 0
	atomicList.clear();
	geometryList.clear();
	frameList.clear();
#endif
}

/*
 * Atomic
 */

void Atomic::read(std::istream &rw)
{
	HeaderInfo header;

	READ_HEADER(CHUNK_ATOMIC);

	READ_HEADER(CHUNK_STRUCT);
	frameIndex = readUInt32(rw);
	geometryIndex = readUInt32(rw);
	rw.seekg(8, std::ios::cur);	// constant

	readExtension(rw);
}

void Atomic::readExtension(std::istream &rw)
{
	HeaderInfo header;

	READ_HEADER(CHUNK_EXTENSION);

	std::streampos end = rw.tellg();
	end += header.getLength();

	while (rw.tellg() < end)
    {
		header.read(rw);

		switch (header.getType())
        {
		case CHUNK_RIGHTTORENDER:
			hasRightToRender = true;
			rightToRenderVal1 = readUInt32(rw);
			rightToRenderVal2 = readUInt32(rw);
//			cout << "atm Righttorender: " << hex << rightToRenderVal1 << " " << rightToRenderVal2 << endl;
			break;
		case CHUNK_PARTICLES:
			hasParticles = true;
			particlesVal = readUInt32(rw);
			break;
		case CHUNK_MATERIALEFFECTS:
			hasMaterialFx = true;
			materialFxVal = readUInt32(rw);
			break;
		case CHUNK_PIPELINESET:
			hasPipelineSet = true;
			pipelineSetVal = readUInt32(rw);
			break;
		default:
			rw.seekg(header.getLength(), std::ios::cur);
			break;
		}
	}
}

void Atomic::dump(uint32 index, std::string ind)
{
	std::cout << ind << "Atomic " << index << " {\n";
	ind += "  ";

	std::cout << ind << "frameIndex: " << frameIndex << std::endl;
	std::cout << ind << "geometryIndex: " << geometryIndex << std::endl;

	if (hasRightToRender)
    {
		std::cout << std::hex;
		std::cout << ind << "Right to Render {\n";
		std::cout << ind << ind << "val1: " << rightToRenderVal1<<std::endl;
		std::cout << ind << ind << "val2: " << rightToRenderVal2<<std::endl;
		std::cout << ind << "}\n";
		std::cout << std::dec;
	}

	if (hasParticles)
    {
		std::cout << ind << "particlesVal: " << particlesVal << std::endl;
    }

	if (hasPipelineSet)
    {
		std::cout << ind << "pipelineSetVal: " << pipelineSetVal << std::endl;
    }

	if (hasMaterialFx)
    {
		std::cout << ind << "materialFxVal: " << materialFxVal << std::endl;
    }

	ind = ind.substr(0, ind.size()-2);
	std::cout << ind << "}\n";
}

/*
 * Frame
 */

// only reads part of the frame struct
void Frame::readStruct(std::istream &rw)
{
	rw.read((char *) rotationMatrix, 9*sizeof(float32));
	rw.read((char *) position, 3*sizeof(float32));
	parent = readInt32(rw);
	rw.seekg(4, std::ios::cur);	// matrix creation flag, unused
}

void Frame::readExtension(std::istream &rw)
{
	HeaderInfo header;

	READ_HEADER(CHUNK_EXTENSION);

	std::streampos end = rw.tellg();
	end += header.getLength();

	while (rw.tellg() < end)
    {
		header.read(rw);

		switch (header.getType())
        {
		case CHUNK_FRAME:
		{
			char *buffer = new char[header.getLength()+1];

			rw.read(buffer, header.getLength());
			buffer[header.getLength()] = '\0';

			name = buffer;

			delete[] buffer;

			break;
		}
		case CHUNK_HANIM:
			hasHAnim = true;

			hAnimUnknown1 = readUInt32(rw);
			hAnimBoneId = readInt32(rw);
			hAnimBoneCount = readUInt32(rw);

			if (hAnimBoneCount != 0)
            {
				hAnimUnknown2 = readUInt32(rw);
				hAnimUnknown3 = readUInt32(rw);
			}
			for (uint32 i = 0; i < hAnimBoneCount; i++)
            {
				hAnimBoneIds.push_back(readInt32(rw));
				hAnimBoneNumbers.push_back(readUInt32(rw));
				uint32 flag = readUInt32(rw);

				if ( (flag&~0x3) != 0 )
                {
					std::cout << flag << std::endl;
                }
				hAnimBoneTypes.push_back(flag);
			}
			break;
		default:
			rw.seekg(header.getLength(), std::ios::cur);
			break;
		}
	}
//	if(hasHAnim)
//		cout << hAnimBoneId << " " << name << endl;
}

void Frame::dump(uint32 index, std::string ind)
{
	std::cout << ind << "Frame " << index << " {\n";
	ind += "  ";

	std::cout << ind << "rotationMatrix: ";

	for ( uint32 i = 0; i < 9; i++ )
    {
		std::cout << rotationMatrix[i] << " ";
    }
	std::cout << std::endl;

	std::cout << ind << "position: ";

	for ( uint32 i = 0; i < 3; i++ )
    {
		std::cout << position[i] << " ";
    }
	std::cout << std::endl;

	std::cout << ind << "parent: " << parent << std::endl;

	std::cout << ind << "name: " << name << std::endl;

	// TODO: hanim

	ind = ind.substr(0, ind.size()-2);
	std::cout << ind << "}\n";
}

/*
 * Geometry
 */

void Geometry::read(std::istream &rw)
{
	HeaderInfo header;

	READ_HEADER(CHUNK_GEOMETRY);

	READ_HEADER(CHUNK_STRUCT);
	flags = readUInt16(rw);
	numUVs = readUInt8(rw);

	if (flags & FLAGS_TEXTURED)
    {
		numUVs = 1;
    }

	hasNativeGeometry = ( readUInt8(rw) != 0 );

	uint32 triangleCount = readUInt32(rw);
	vertexCount = readUInt32(rw);

	rw.seekg(4, std::ios::cur); /* number of morph targets, uninteresting */

	// skip light info
    LibraryVersion libVer = header.getVersion();

	if (libVer.rwLibMinor <= 3)
    {
		rw.seekg(12, std::ios::cur);
	}

	if (!hasNativeGeometry)
    {
		if (flags & FLAGS_PRELIT)
        {
			vertexColors.resize(4*vertexCount);
			rw.read((char *) (&vertexColors[0]), 4*vertexCount*sizeof(uint8));
		}
		if (flags & FLAGS_TEXTURED)
        {
			texCoords[0].resize(2*vertexCount);
			rw.read((char *) (&texCoords[0][0]), 2*vertexCount*sizeof(float32));
		}
		if (flags & FLAGS_TEXTURED2)
        {
			for (uint32 i = 0; i < numUVs; i++)
            {
				texCoords[i].resize(2*vertexCount);
				rw.read((char *) (&texCoords[i][0]), 2*vertexCount*sizeof(float32));
			}
		}
		faces.resize(4*triangleCount);
		rw.read((char *) (&faces[0]), 4*triangleCount*sizeof(uint16));
	}

	/* morph targets, only 1 in gta */
	rw.read((char *)(boundingSphere), 4*sizeof(float32));
	//hasPositions = (flags & FLAGS_POSITIONS) ? 1 : 0;
	hasPositions = readUInt32(rw);
	hasNormals = readUInt32(rw);
	// need to recompute:
	hasPositions = 1;
	hasNormals = (flags & FLAGS_NORMALS) ? 1 : 0;

	if (!hasNativeGeometry)
    {
		vertices.resize(3*vertexCount);
		rw.read((char *) (&vertices[0]), 3*vertexCount*sizeof(float32));

		if (flags & FLAGS_NORMALS)
        {
			normals.resize(3*vertexCount);
			rw.read((char *) (&normals[0]), 3*vertexCount*sizeof(float32));
		}
	}

	READ_HEADER(CHUNK_MATLIST);

	READ_HEADER(CHUNK_STRUCT);
	uint32 numMaterials = readUInt32(rw);
	rw.seekg(numMaterials*4, std::ios::cur);	// constant

#if 0

	materialList.resize(numMaterials);

	for (uint32 i = 0; i < numMaterials; i++)
    {
		materialList[i].read(rw);
    }

#endif

	readExtension(rw);
}

void Geometry::readExtension(std::istream &rw)
{
	HeaderInfo header;

	READ_HEADER(CHUNK_EXTENSION);

	std::streampos end = rw.tellg();
	end += header.getLength();

	while ( rw.tellg() < end )
    {
		header.read(rw);

		switch( header.getType() )
        {
		case CHUNK_BINMESH:
        {
			faceType = readUInt32(rw);

			uint32 numSplits = readUInt32(rw);

			numIndices = readUInt32(rw);
			splits.resize(numSplits);

			bool hasData = header.getLength() > 12+numSplits*8;

			for ( uint32 i = 0; i < numSplits; i++ )
            {
				uint32 numIndices = readUInt32(rw);
				splits[i].matIndex = readUInt32(rw);
				splits[i].indices.resize(numIndices);

				if( hasData )
                {
					/* OpenGL Data */
					if ( hasNativeGeometry )
                    {
					    for( uint32 j = 0; j < numIndices; j++ )
                        {
						    splits[i].indices[j] = readUInt16(rw);
                        }
                    }
					else
                    {
					    for ( uint32 j = 0; j < numIndices; j++ )
                        {
						    splits[i].indices[j] = readUInt32(rw);
                        }
                    }
				}
			}
			break;
		}
        case CHUNK_NATIVEDATA:
        {
			std::streampos beg = rw.tellg();

			uint32 size = header.getLength();
			LibraryVersion ver = header.getVersion();

			header.read(rw);

			if ( header.getVersion() == ver && header.getType() == CHUNK_STRUCT )
            {
				uint32 platform = readUInt32(rw);

				rw.seekg(beg, std::ios::beg);

				if( platform == PLATFORM_PS2 )
                {
					readPs2NativeData(rw);
                }
				else if ( platform == PLATFORM_XBOX )
                {
					readXboxNativeData(rw);
                }
				else
                {
					std::cout << "unknown platform " << platform << std::endl;
                }
            }
            else
            {
				rw.seekg(beg, std::ios::beg);
				readOglNativeData(rw, size);
			}
			break;
		}
		case CHUNK_MESHEXTENSION:
        {
			hasMeshExtension = true;

			meshExtension = new MeshExtension;
			meshExtension->unknown = readUInt32(rw);
			readMeshExtension(rw);
			break;
		}
        case CHUNK_NIGHTVERTEXCOLOR:
        {
			hasNightColors = true;

			nightColorsUnknown = readUInt32(rw);

			if ( nightColors.size() != 0 )
            {
				// native data also has them, so skip
				rw.seekg(header.getLength() - sizeof(uint32), std::ios::cur);
			}
            else
            {
				if ( nightColorsUnknown != 0 )
                {
				    /* TODO: could be better */
					nightColors.resize(header.getLength() - 4);
					rw.read((char *)(&nightColors[0]), header.getLength() - 4);
				}
			}
			break;
		}
        case CHUNK_MORPH:
        {
			hasMorph = true;

			/* always 0 */
			readUInt32(rw);
			break;
		}
        case CHUNK_SKIN:
        {
			if ( hasNativeGeometry )
            {
				std::streampos beg = rw.tellg();
				rw.seekg(0x0c, std::ios::cur);
				uint32 platform = readUInt32(rw);
				rw.seekg(beg, std::ios::beg);
//				streampos end = beg+header.length;

				if ( platform == PLATFORM_OGL || platform == PLATFORM_PS2 )
                {
					hasSkin = true;
					readNativeSkinMatrices(rw);
				}
                else if ( platform == PLATFORM_XBOX )
                {
					hasSkin = true;
					readXboxNativeSkin(rw);
				}
                else
                {
					std::cout << "skin: unknown platform " << platform << std::endl;

					rw.seekg( header.getLength(), std::ios::cur );
				}
			}
            else
            {
				hasSkin = true;
				boneCount = readUInt8(rw);
				specialIndexCount = readUInt8(rw);
				unknown1 = readUInt8(rw);
				unknown2 = readUInt8(rw);

				specialIndices.resize(specialIndexCount);
				rw.read((char *) (&specialIndices[0]), specialIndexCount*sizeof(uint8));

				vertexBoneIndices.resize(vertexCount);
				rw.read((char *) (&vertexBoneIndices[0]), vertexCount*sizeof(uint32));

				vertexBoneWeights.resize(vertexCount*4);
				rw.read((char *) (&vertexBoneWeights[0]), vertexCount*4*sizeof(float32));

				inverseMatrices.resize(boneCount*16);

				for ( uint32 i = 0; i < boneCount; i++ )
                {
					// skip 0xdeaddead
					if (specialIndexCount == 0)
                    {
						rw.seekg(4, std::ios::cur);
                    }

					rw.read((char *)(&inverseMatrices[i*0x10]), 0x10*sizeof(float32));
				}

				// skip some zeroes
				if( specialIndexCount != 0 )
                {
					rw.seekg(0x0C, std::ios::cur);
                }
			}
			break;
		}
		case CHUNK_ADCPLG:
			/* only sa ps2, ignore (not very interesting anyway) */
			rw.seekg(header.getLength(), std::ios::cur);
			break;
		case CHUNK_2DFX:
			rw.seekg(header.getLength(), std::ios::cur);
			break;
		default:
			rw.seekg(header.getLength(), std::ios::cur);
			break;
		}
	}
}

void Geometry::readNativeSkinMatrices(std::istream &rw)
{
	HeaderInfo header;

	READ_HEADER(CHUNK_STRUCT);

	uint32 platform = readUInt32(rw);

	if (platform != PLATFORM_PS2 && platform != PLATFORM_OGL)
    {
		std::cerr << "format error: native skin not in ps2 or ogl format\n";
		return;
	}

	boneCount = readUInt8(rw);
	specialIndexCount = readUInt8(rw);
	unknown1 = readUInt8(rw);
	unknown2 = readUInt8(rw);

	specialIndices.resize(specialIndexCount);
	rw.read((char *) (&specialIndices[0]), specialIndexCount*sizeof(uint8));

	inverseMatrices.resize(boneCount*0x10);

	for (uint32 i = 0; i < boneCount; i++)
    {
		rw.read((char *) (&inverseMatrices[i*0x10]), 0x10*sizeof(float32));
    }

	// skip unknowns
	if ( specialIndexCount != 0 )
    {
		rw.seekg(0x1C, std::ios::cur);
    }
}

void Geometry::readMeshExtension(std::istream &rw)
{
	if ( meshExtension->unknown == 0 )
		return;

	rw.seekg(0x4, std::ios::cur);
	uint32 vertexCount = readUInt32(rw);
	rw.seekg(0xC, std::ios::cur);
	uint32 faceCount = readUInt32(rw);
	rw.seekg(0x8, std::ios::cur);
	uint32 materialCount = readUInt32(rw);
	rw.seekg(0x10, std::ios::cur);

	/* vertices */
	meshExtension->vertices.resize(3*vertexCount);
	rw.read((char *) (&meshExtension->vertices[0]), 3*vertexCount*sizeof(float32));

	/* tex coords */
	meshExtension->texCoords.resize(2*vertexCount);
	rw.read((char *) (&meshExtension->texCoords[0]), 2*vertexCount*sizeof(float32));

	/* vertex colors */
	meshExtension->vertexColors.resize(4*vertexCount);
	rw.read((char *) (&meshExtension->vertexColors[0]), 4*vertexCount*sizeof(uint8));

	/* faces */
	meshExtension->faces.resize(3*faceCount);
	rw.read((char *) (&meshExtension->faces[0]), 3*faceCount*sizeof(uint16));

	/* material assignments */
	meshExtension->assignment.resize(faceCount);
	rw.read((char *) (&meshExtension->assignment[0]), faceCount*sizeof(uint16));

	meshExtension->textureName.resize(materialCount);
	meshExtension->maskName.resize(materialCount);

	char buffer[0x20];

	for (uint32 i = 0; i < materialCount; i++)
    {
		rw.read(buffer, 0x20);
		meshExtension->textureName[i] = buffer;

		rw.read(buffer, 0x20);
		meshExtension->maskName[i] = buffer;

		meshExtension->unknowns.push_back(readFloat32(rw));
		meshExtension->unknowns.push_back(readFloat32(rw));
		meshExtension->unknowns.push_back(readFloat32(rw));
	}
}

bool Geometry::isDegenerateFace(uint32 i, uint32 j, uint32 k)
{
	if (vertices[i*3+0] == vertices[j*3+0] &&
	    vertices[i*3+1] == vertices[j*3+1] &&
	    vertices[i*3+2] == vertices[j*3+2])
		return true;
	if (vertices[i*3+0] == vertices[k*3+0] &&
	    vertices[i*3+1] == vertices[k*3+1] &&
	    vertices[i*3+2] == vertices[k*3+2])
		return true;
	if (vertices[j*3+0] == vertices[k*3+0] &&
	    vertices[j*3+1] == vertices[k*3+1] &&
	    vertices[j*3+2] == vertices[k*3+2])
		return true;

	return false;
}

// native console data doesn't have face information, use this to generate it
void Geometry::generateFaces(void)
{
	faces.clear();

	for ( uint32 i = 0; i < splits.size(); i++ )
    {
		Split &s = splits[i];

        if ( faceType == FACETYPE_STRIP )
        {
			for ( uint32 j = 0; j < s.indices.size()-2; j++ )
            {
				if ( isDegenerateFace(s.indices[j+0], s.indices[j+1], s.indices[j+2]) )
					continue;

				faces.push_back(s.indices[j+1 + (j%2)]);
				faces.push_back(s.indices[j+0]);
				faces.push_back(s.matIndex);
				faces.push_back(s.indices[j+2 - (j%2)]);
			}
        }
		else
        {
			for ( uint32 j = 0; j < s.indices.size()-2; j+=3 )
            {
				faces.push_back(s.indices[j+1]);
				faces.push_back(s.indices[j+0]);
				faces.push_back(s.matIndex);
				faces.push_back(s.indices[j+2]);
			}
        }
	}
}

// these hold the (temporary) cleaned up data
static std::vector<float32>  vertices_new;
static std::vector<float32>  normals_new;
static std::vector<float32>  texCoords_new[8];
static std::vector<uint8>    vertexColors_new;
static std::vector<uint8>    nightColors_new;
static std::vector<uint32>   vertexBoneIndices_new;
static std::vector<float32>  vertexBoneWeights_new;

// used only by Geometry::cleanUp()
// adds new temporary vertex if it isn't already in the list
// and returns the new index of that vertex
uint32 Geometry::addTempVertexIfNew(uint32 index)
{
	// return if we already have the vertex
	for (uint32 i = 0; i < vertices_new.size()/3; i++)
    {
		if (vertices_new[i*3+0] != vertices[index*3+0] ||
		    vertices_new[i*3+1] != vertices[index*3+1] ||
		    vertices_new[i*3+2] != vertices[index*3+2])
			continue;

		if (flags & FLAGS_NORMALS)
        {
			if (normals_new[i*3+0] != normals[index*3+0] ||
			    normals_new[i*3+1] != normals[index*3+1] ||
			    normals_new[i*3+2] != normals[index*3+2])
				continue;
		}
		if (flags & FLAGS_TEXTURED || flags & FLAGS_TEXTURED2)
        {
			for (uint32 j = 0; j < numUVs; j++)
            {
				if (texCoords_new[j][i*2+0] != texCoords[j][index*2+0] ||
				    texCoords_new[j][i*2+1] != texCoords[j][index*2+1])
                {
					goto cont;
                }
            }
		}
		if (flags & FLAGS_PRELIT)
        {
			if (vertexColors_new[i*4+0]!=vertexColors[index*4+0] ||
			    vertexColors_new[i*4+1]!=vertexColors[index*4+1] ||
			    vertexColors_new[i*4+2]!=vertexColors[index*4+2] ||
			    vertexColors_new[i*4+3]!=vertexColors[index*4+3])
				continue;
		}
		if (hasNightColors)
        {
			if (nightColors_new[i*4+0] != nightColors[index*4+0] ||
			    nightColors_new[i*4+1] != nightColors[index*4+1] ||
			    nightColors_new[i*4+2] != nightColors[index*4+2] ||
			    nightColors_new[i*4+3] != nightColors[index*4+3])
				continue;
		}
		if (hasSkin)
        {
			if ( vertexBoneIndices_new[i] != vertexBoneIndices[index] )
				continue;

			if (vertexBoneWeights_new[i*4+0] != vertexBoneWeights[index*4+0] ||
			    vertexBoneWeights_new[i*4+1] != vertexBoneWeights[index*4+1] ||
			    vertexBoneWeights_new[i*4+2] != vertexBoneWeights[index*4+2] ||
			    vertexBoneWeights_new[i*4+3] != vertexBoneWeights[index*4+3])
            {
				continue;
            }
		}
		// this is only reached when the vertex is already in the list
		return i;

		cont: ;
	}

	// else add the vertex
	vertices_new.push_back(vertices[index*3+0]);
	vertices_new.push_back(vertices[index*3+1]);
	vertices_new.push_back(vertices[index*3+2]);

	if (flags & FLAGS_NORMALS)
    {
		normals_new.push_back(normals[index*3+0]);
		normals_new.push_back(normals[index*3+1]);
		normals_new.push_back(normals[index*3+2]);
	}
	if (flags & FLAGS_TEXTURED || flags & FLAGS_TEXTURED2)
    {
		for (uint32 j = 0; j < numUVs; j++)
        {
			texCoords_new[j].push_back(texCoords[j][index*2+0]);
			texCoords_new[j].push_back(texCoords[j][index*2+1]);
		}
	}
	if (flags & FLAGS_PRELIT)
    {
		vertexColors_new.push_back(vertexColors[index*4+0]);
		vertexColors_new.push_back(vertexColors[index*4+1]);
		vertexColors_new.push_back(vertexColors[index*4+2]);
		vertexColors_new.push_back(vertexColors[index*4+3]);
	}
	if (hasNightColors)
    {
		nightColors_new.push_back(nightColors[index*4+0]);
		nightColors_new.push_back(nightColors[index*4+1]);
		nightColors_new.push_back(nightColors[index*4+2]);
		nightColors_new.push_back(nightColors[index*4+3]);
	}
	if (hasSkin)
    {
		vertexBoneIndices_new.push_back(vertexBoneIndices[index]);

		vertexBoneWeights_new.push_back(vertexBoneWeights[index*4+0]);
		vertexBoneWeights_new.push_back(vertexBoneWeights[index*4+1]);
		vertexBoneWeights_new.push_back(vertexBoneWeights[index*4+2]);
		vertexBoneWeights_new.push_back(vertexBoneWeights[index*4+3]);
	}
	return vertices_new.size()/3 - 1;
}

// removes duplicate vertices (only useful with ps2 meshes)
void Geometry::cleanUp(void)
{
	vertices_new.clear();
	normals_new.clear();
	vertexColors_new.clear();
	nightColors_new.clear();

	for (uint32 i = 0; i < 8; i++)
    {
		texCoords_new[i].clear();
    }

	vertexBoneIndices_new.clear();
	vertexBoneWeights_new.clear();

	std::vector<uint32> newIndices;

	// create new vertex list
	for (uint32 i = 0; i < vertices.size()/3; i++)
    {
		newIndices.push_back(addTempVertexIfNew(i));
    }

	vertices = vertices_new;

	if (flags & FLAGS_NORMALS)
    {
		normals = normals_new;
    }
	if (flags & FLAGS_TEXTURED || flags & FLAGS_TEXTURED2)
    {
		for ( uint32 j = 0; j < numUVs; j++ )
        {
			texCoords[j] = texCoords_new[j];
        }
    }
	if (flags & FLAGS_PRELIT)
    {
		vertexColors = vertexColors_new;
    }
	if (hasNightColors)
    {
		nightColors = nightColors_new;
    }
	if (hasSkin)
    {
		vertexBoneIndices = vertexBoneIndices_new;
		vertexBoneWeights = vertexBoneWeights_new;
	}

	// correct indices
	for (uint32 i = 0; i < splits.size(); i++)
    {
		for (uint32 j = 0; j < splits[i].indices.size(); j++)
        {
			splits[i].indices[j] = newIndices[splits[i].indices[j]];
        }
    }

	for (uint32 i = 0; i < faces.size()/4; i++)
    {
		faces[i*4+0] = newIndices[faces[i*4+0]];
		faces[i*4+1] = newIndices[faces[i*4+1]];
		faces[i*4+3] = newIndices[faces[i*4+3]];
	}
}

void Geometry::dump(uint32 index, std::string ind, bool detailed)
{
	std::cout << ind << "Geometry " << index << " {\n";
	ind += "  ";

	std::cout << ind << "flags: " << std::hex << flags << std::endl;
	std::cout << ind << "numUVs: " << std::dec << numUVs << std::endl;
	std::cout << ind << "hasNativeGeometry: " << hasNativeGeometry << std::endl;
	std::cout << ind << "triangleCount: " << faces.size()/4 << std::endl;
	std::cout << ind << "vertexCount: " << vertexCount << std::endl << std::endl;;

	if (flags & FLAGS_PRELIT)
    {
		std::cout << ind << "vertexColors {\n";
		ind += "  ";
		if (!detailed)
        {
			std::cout << ind << "skipping\n";
        }
		else
        {
			for (uint32 i = 0; i < vertexColors.size()/4; i++)
            {
				std::cout << ind << int(vertexColors[i*4+0])<< ", "
					<< int(vertexColors[i*4+1]) << ", "
					<< int(vertexColors[i*4+2]) << ", "
					<< int(vertexColors[i*4+3]) << std::endl;
            }
        }
		ind = ind.substr(0, ind.size()-2);
		std::cout << ind << "}\n\n";
	}

	if (flags & FLAGS_TEXTURED)
    {
		std::cout << ind << "texCoords {\n";
		ind += "  ";

		if (!detailed)
        {
			std::cout << ind << "skipping\n";
        }
		else
        {
            uint32 texCoordSizeHalf = texCoords[0].size() / 2;

			for (uint32 i = 0; i < texCoordSizeHalf; i++)
            {
				std::cout << ind << texCoords[0][i*2+0]<< ", " << texCoords[0][i*2+1] << std::endl;
            }
        }

		ind = ind.substr(0, ind.size()-2);
		std::cout << ind << "}\n\n";
	}
	if (flags & FLAGS_TEXTURED2)
    {
		for ( uint32 j = 0; j < numUVs; j++ )
        {
		    std::cout << ind << "texCoords " << j << " {\n";
		    ind += "  ";

		    if (!detailed)
            {
			    std::cout << ind << "skipping\n";
            }
		    else
            {
                uint32 texCoordSizeHalf = texCoords[0].size() / 2;

			    for ( uint32 i = 0; i < texCoordSizeHalf; i++ )
                {
				    std::cout << ind << texCoords[j][i*2+0]<< ", "
					    << texCoords[j][i*2+1] << std::endl;
                }
            }
		    ind = ind.substr(0, ind.size()-2);
		    std::cout << ind << "}\n\n";
		}
	}

	std::cout << ind << "faces {\n";
	ind += "  ";

	if ( !detailed )
    {
		std::cout << ind << "skipping\n";
    }
	else
    {
		for ( uint32 i = 0; i < faces.size()/4; i++ )
        {
			std::cout << ind << faces[i*4+0] << ", "
			            << faces[i*4+1] << ", "
			            << faces[i*4+2] << ", "
			            << faces[i*4+3] << std::endl;
        }
    }
	ind = ind.substr(0, ind.size()-2);
	std::cout << ind << "}\n\n";

	std::cout << ind << "boundingSphere: ";
	for ( uint32 i = 0; i < 4; i++ )
    {
		std::cout << boundingSphere[i] << " ";
    }

	std::cout << std::endl << std::endl;
	std::cout << ind << "hasPositions: " << hasPositions << std::endl;
	std::cout << ind << "hasNormals: " << hasNormals << std::endl;

	std::cout << ind << "vertices {\n";
	ind += "  ";

	if ( !detailed )
    {
		std::cout << ind << "skipping\n";
    }
	else
    {
		for ( uint32 i = 0; i < vertices.size()/3; i++ )
        {
			std::cout << ind << vertices[i*3+0] << ", "
			            << vertices[i*3+1] << ", "
			            << vertices[i*3+2] << std::endl;
        }
    }

	ind = ind.substr(0, ind.size()-2);
	std::cout << ind << "}\n";

	if (flags & FLAGS_NORMALS)
    {
		std::cout << ind << "normals {\n";
		ind += "  ";

		if (!detailed)
        {
			std::cout << ind << "skipping\n";
        }
		else
        {
			for (uint32 i = 0; i < normals.size()/3; i++)
            {
				std::cout << ind << normals[i*3+0] << ", "
					    << normals[i*3+1] << ", "
					    << normals[i*3+2] << std::endl;
            }
        }

		ind = ind.substr(0, ind.size()-2);
		std::cout << ind << "}\n";
	}

	std::cout << std::endl << ind << "BinMesh {\n";
	ind += "  ";

	std::cout << ind << "faceType: " << faceType << std::endl;
	std::cout << ind << "numIndices: " << numIndices << std::endl;

	for ( uint32 i = 0; i < splits.size(); i++ )
    {
		std::cout << std::endl << ind << "Split " << i << " {\n";
		ind += "  ";

		std::cout << ind << "matIndex: " << splits[i].matIndex << std::endl;
		std::cout << ind << "numIndices: "<<splits[i].indices.size() << std::endl;
		std::cout << ind << "indices {\n";

		if( !detailed )
        {
			std::cout << ind+"  skipping\n";
        }
		else
        {
			for ( uint32 j = 0; j < splits[i].indices.size(); j++ )
            {
				std::cout << ind + " " << splits[i].indices[j] << std::endl;
            }
        }

		std::cout << ind << "}\n";
		ind = ind.substr(0, ind.size()-2);

		std::cout << ind << "}\n";
	}

	ind = ind.substr(0, ind.size()-2);
	std::cout << ind << "}\n";

	std::cout << std::endl << ind << "MaterialList {\n";
	ind += "  ";

	std::cout << ind << "numMaterials: " << materialList.size() << std::endl;

#if 0

	for ( uint32 i = 0; i < materialList.size(); i++ )
    {
		materialList[i].dump(i, ind);
    }

#endif

	ind = ind.substr(0, ind.size()-2);
	std::cout << ind << "}\n";

	ind = ind.substr(0, ind.size()-2);
	std::cout << ind << "}\n";
}

/*
 * Material
 */

void Material::read(std::istream &rw)
{
	HeaderInfo header;

	READ_HEADER(CHUNK_MATERIAL);

	READ_HEADER(CHUNK_STRUCT);
	flags = readUInt32(rw);
	rw.read((char *) (color), 4*sizeof(uint8));
	unknown = readUInt32(rw);;
	hasTex = ( readInt32(rw) != 0 );
	rw.read((char *) (surfaceProps), 3*sizeof(float32));

#if 0
	if (hasTex)
    {
		texture.read(rw);
    }
#endif

	readExtension(rw);
}

void Material::readExtension(std::istream &rw)
{
	HeaderInfo header;

	READ_HEADER(CHUNK_EXTENSION);

	std::streampos end = rw.tellg();
	end += header.getLength();

	while (rw.tellg() < end)
    {
		header.read(rw);

		switch (header.getType())
        {
		case CHUNK_RIGHTTORENDER:
			hasRightToRender = true;
			rightToRenderVal1 = readUInt32(rw);
			rightToRenderVal2 = readUInt32(rw);
//			cout << "mat Righttorender: " << hex << rightToRenderVal1 << " " << rightToRenderVal2 << endl;
			break;
		case CHUNK_MATERIALEFFECTS: {
			hasMatFx = true;
			matFx = new MatFx;
			matFx->type = readUInt32(rw);
			switch (matFx->type) {
			case MATFX_BUMPMAP: {
//cout << filename << " BUMPMAP\n";
				rw.seekg(4, std::ios::cur); // also MATFX_BUMPMAP
				matFx->bumpCoefficient = readFloat32(rw);

#if 0

				matFx->hasTex1 = ( readUInt32(rw) != 0 );
				if (matFx->hasTex1)
					matFx->tex1.read(rw);

				matFx->hasTex2 = ( readUInt32(rw) != 0 );
				if (matFx->hasTex2)
					matFx->tex2.read(rw);

#endif

				rw.seekg(4, std::ios::cur); // 0
				break;
			} case MATFX_ENVMAP: {
				rw.seekg(4, std::ios::cur); // also MATFX_ENVMAP
				matFx->envCoefficient = readFloat32(rw);

#if 0

				matFx->hasTex1 = ( readUInt32(rw) != 0 );
				if (matFx->hasTex1)
					matFx->tex1.read(rw);

				matFx->hasTex2 = ( readUInt32(rw) != 0 );
				if (matFx->hasTex2)
					matFx->tex2.read(rw);

#endif

				rw.seekg(4, std::ios::cur); // 0
				break;
			} case MATFX_BUMPENVMAP: {
				rw.seekg(4, std::ios::cur); // MATFX_BUMPMAP
				matFx->bumpCoefficient = readFloat32(rw);

#if 0

				matFx->hasTex1 = ( readUInt32(rw) != 0 );
				if (matFx->hasTex1)
					matFx->tex1.read(rw);
				// needs to be 0, tex2 will be used
				rw.seekg(4, ios::cur);

				rw.seekg(4, ios::cur); // MATFX_ENVMPMAP
				matFx->envCoefficient = readFloat32(rw);
				// needs to be 0, tex1 is already used
				rw.seekg(4, ios::cur);
				matFx->hasTex2 = ( readUInt32(rw) != 0 );
				if (matFx->hasTex2)
					matFx->tex2.read(rw);

#endif

				break;
			} case MATFX_DUAL: {

				rw.seekg(4, std::ios::cur); // also MATFX_DUAL
				matFx->srcBlend = (float32)readUInt32(rw);
				matFx->destBlend = (float32)readUInt32(rw);

#if 0

				matFx->hasDualPassMap = ( readUInt32(rw) != 0 );
				if (matFx->hasDualPassMap)
					matFx->dualPassMap.read(rw);
				rw.seekg(4, ios::cur); // 0

#endif

				break;
			} case MATFX_UVTRANSFORM: {

				rw.seekg(4, std::ios::cur);//also MATFX_UVTRANSFORM
				rw.seekg(4, std::ios::cur); // 0
				break;
			} case MATFX_DUALUVTRANSFORM: {
				// never observed in gta
				break;
			} default:
				break;
			}
			break;
		} case CHUNK_REFLECTIONMAT:
			hasReflectionMat = true;
			reflectionChannelAmount[0] = readFloat32(rw);
			reflectionChannelAmount[1] = readFloat32(rw);
			reflectionChannelAmount[2] = readFloat32(rw);
			reflectionChannelAmount[3] = readFloat32(rw);
			reflectionIntensity = readFloat32(rw);
			rw.seekg(4, std::ios::cur);
			break;
		case CHUNK_SPECULARMAT: {
			hasSpecularMat = true;
			specularLevel = readFloat32(rw);

			uint32 len = header.getLength() - sizeof(float32) - 4;
			char *name = new char[len];

			rw.read(name, len);

			specularName = name;

			rw.seekg(4, std::ios::cur);

			delete[] name;

			break;
		}
		case CHUNK_UVANIMDICT:
			rw.seekg(header.getLength(), std::ios::cur);
			break;
		default:
			rw.seekg(header.getLength(), std::ios::cur);
			break;
		}
	}
}

void Material::dump(uint32 index, std::string ind)
{
	std::cout << ind << "Material " << index << " {\n";
	ind += "  ";

	// unused
//	cout << ind << "flags: " << hex << flags << endl;
	std::cout << ind << "color: " << std::dec << int(color[0]) << " "
	                         << int(color[1]) << " "
	                         << int(color[2]) << " "
	                         << int(color[3]) << std::endl;
	// unused
//	cout << ind << "unknown: " << hex << unknown << endl;
	std::cout << ind << "surfaceProps: " << surfaceProps[0] << " "
	                                << surfaceProps[1] << " "
	                                << surfaceProps[2] << std::endl;

#if 0
	if(hasTex)
		texture.dump(ind);
#endif

	if(hasMatFx)
		matFx->dump(ind);

	if(hasRightToRender)
    {
		std::cout << std::hex;
		std::cout << ind << "Right to Render {\n";
		std::cout << ind+"  " << "val1: " << rightToRenderVal1<<std::endl;
		std::cout << ind+"  " << "val2: " << rightToRenderVal2<<std::endl;
		std::cout << ind << "}\n";
		std::cout << std::dec;
	}

	if(hasReflectionMat)
    {
		std::cout << ind << "Reflection Material {\n";
		std::cout << ind+"  " << "amount: "
		     << reflectionChannelAmount[0] << " "
		     << reflectionChannelAmount[1] << " "
		     << reflectionChannelAmount[2] << " "
		     << reflectionChannelAmount[3] << std::endl;
		std::cout << ind+"  " << "intensity: " << reflectionIntensity << std::endl;
		std::cout << ind << "}\n";
	}

	if(hasSpecularMat)
    {
		std::cout << ind << "Specular Material {\n";
		std::cout << ind+"  " << "level: " << specularLevel << std::endl;
		std::cout << ind+"  " << "name: " << specularName << std::endl;
		std::cout << ind << "}\n";
	}

	ind = ind.substr(0, ind.size()-2);
	std::cout << ind << "}\n";
}

void MatFx::dump(std::string ind)
{
	static const char *names[] = {
		"INVALID",
		"MATFX_BUMPMAP",
		"MATFX_ENVMAP",
		"MATFX_BUMPENVMAP",
		"MATFX_DUAL",
		"MATFX_UVTRANSFORM",
		"MATFX_DUALUVTRANSFORM"
	};

	std::cout << ind << "MatFX {\n";
	ind += "  ";

	std::cout << ind << "type: " << names[type] << std::endl;

	if ( type == MATFX_BUMPMAP || type == MATFX_BUMPENVMAP )
    {
		std::cout << ind << "bumpCoefficient: " << bumpCoefficient << std::endl;
    }
	if ( type == MATFX_ENVMAP || type == MATFX_BUMPENVMAP )
    {
		std::cout << ind << "envCoefficient: " << envCoefficient << std::endl;
    }
	if ( type == MATFX_DUAL )
    {
		std::cout << ind << "srcBlend: " << srcBlend << std::endl;
		std::cout << ind << "destBlend: " << destBlend << std::endl;
	}

	std::cout << ind << "textures: " << hasTex1 << " " << hasTex2 << " " << hasDualPassMap << std::endl;

#if 0

	if( hasTex1 )
    {
		tex1.dump(ind);
    }
	if ( hasTex2 )
    {
		tex2.dump(ind);
    }
	if ( hasDualPassMap )
    {
		dualPassMap.dump(ind);
    }

#endif

	ind = ind.substr(0, ind.size()-2);
	std::cout << ind << "}\n";
}

MatFx::MatFx(void)
: hasTex1(false), hasTex2(false), hasDualPassMap(false)
{
}


/*
 * Texture
 */

void Texture::read(std::istream &rw)
{
	HeaderInfo header;

	READ_HEADER(CHUNK_TEXTURE);

	READ_HEADER(CHUNK_STRUCT);
	filterFlags = readUInt16(rw);
	rw.seekg(2, std::ios::cur);

    // Read name
    {
	    READ_HEADER(CHUNK_STRING);

        uint32 strLen = header.getLength();

	    char *buffer = new char[strLen + 1];
	    rw.read(buffer, strLen);

	    buffer[strLen] = '\0';

	    name = buffer;
	    delete[] buffer;
    }

    // Read mask name
    {
	    READ_HEADER(CHUNK_STRING);

        uint32 strLen = header.getLength();

	    char *buffer = new char[strLen + 1];
	    rw.read(buffer, strLen);

	    buffer[strLen] = '\0';

	    maskName = buffer;
	    delete[] buffer;
    }

	readExtension(rw);
}

void Texture::readExtension(std::istream &rw)
{
	HeaderInfo header;

	READ_HEADER(CHUNK_EXTENSION);

	std::streampos end = rw.tellg();
	end += header.getLength();

	while (rw.tellg() < end)
    {
		header.read(rw);

		switch (header.getType())
        {
		case CHUNK_SKYMIPMAP:
			hasSkyMipmap = true;
			rw.seekg(header.getLength(), std::ios::cur);
			break;
		default:
			rw.seekg(header.getLength(), std::ios::cur);
			break;
		}
	}
}

void Texture::dump(std::string ind)
{
	std::cout << ind << "Texture {\n";
	ind += "  ";

	std::cout << ind << "filterFlags: " << std::hex << filterFlags << std::dec << std::endl;
	std::cout << ind << "name: " << name << std::endl;
	std::cout << ind << "maskName: " << maskName << std::endl;

	ind = ind.substr(0, ind.size()-2);
	std::cout << ind << "}\n";
}

}

#endif //disabled
