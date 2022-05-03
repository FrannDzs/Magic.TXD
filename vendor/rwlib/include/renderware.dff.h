/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/renderware.blockapi.h
*  PURPOSE:     Old DFF structures
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

namespace rw
{

struct Atomic : public RwObject
{
    Atomic( Interface *engineInterface, void *construction_params ) : RwObject( engineInterface, construction_params )
    {
        this->frameIndex = -1;
        this->geometryIndex = -1;
        this->hasRightToRender = false;
        this->rightToRenderVal1 = 0;
        this->rightToRenderVal2 = 0;
        this->hasParticles = false;
        this->particlesVal = 0;
        this->hasPipelineSet = false;
        this->pipelineSetVal = 0;
        this->hasMaterialFx = false;
        this->materialFxVal = 0;
    }

	int32 frameIndex;
	int32 geometryIndex;

	/* Extensions */

	/* right to render */
	bool hasRightToRender;
	uint32 rightToRenderVal1;
	uint32 rightToRenderVal2;

	/* particles */
	bool hasParticles;
	uint32 particlesVal;

	/* pipelineset */
	bool hasPipelineSet;
	uint32 pipelineSetVal;

	/* material fx */
	bool hasMaterialFx;
	uint32 materialFxVal;

	/* functions */
	void read(std::istream &dff);
	void readExtension(std::istream &dff);
	uint32 write(std::ostream &dff);
	void dump(uint32 index, std::string ind = "");
};

struct MeshExtension
{
	uint32 unknown;

	rwStaticVector<float32> vertices;
	rwStaticVector<float32> texCoords;
	rwStaticVector<uint8> vertexColors;
	rwStaticVector<uint16> faces;
	rwStaticVector<uint16> assignment;

	rwStaticVector<rwStaticString <char>> textureName;
	rwStaticVector<rwStaticString <char>> maskName;
	rwStaticVector<float32> unknowns;
};

struct Split
{
	uint32 matIndex;
	rwStaticVector<uint32> indices;	
};

struct Geometry : public RwObject
{
    inline Geometry( Interface *engineInterface, void *construction_params ) : RwObject( engineInterface, construction_params )
    {
        this->flags = 0;
        this->numUVs = 0;
        this->hasNativeGeometry = false;
        this->vertexCount = 0;
        this->hasNormals = false;
        this->faceType = 0;
        this->numIndices = 0;
        this->hasSkin = false;
        this->boneCount = 0;
        this->specialIndexCount = 0;
        this->unknown1 = 0;
        this->unknown2 = 0;
        this->hasMeshExtension = false;
        this->meshExtension = 0;
        this->hasNightColors = false;
        this->nightColorsUnknown = 0;
        this->hasMorph = false;
    }

    inline ~Geometry( void )
    {
        if ( this->meshExtension )
        {
            delete this->meshExtension;
        }
    }

	uint32 flags;
	uint32 numUVs;
	bool hasNativeGeometry;

	uint32 vertexCount;
	rwStaticVector<uint16> faces;
	rwStaticVector<uint8> vertexColors;
	rwStaticVector<float32> texCoords[8];

	/* morph target (only one) */
	float32 boundingSphere[4];
	uint32 hasPositions;
	uint32 hasNormals;
	rwStaticVector<float32> vertices;
	rwStaticVector<float32> normals;

	rwStaticVector<Material*> materialList;

	/* Extensions */

	/* bin mesh */
	uint32 faceType;
	uint32 numIndices;
	rwStaticVector<Split> splits;

	/* skin */
	bool hasSkin;
	uint32 boneCount;
	uint32 specialIndexCount;
	uint32 unknown1;
	uint32 unknown2;
	rwStaticVector<uint8> specialIndices;
	rwStaticVector<uint32> vertexBoneIndices;
	rwStaticVector<float32> vertexBoneWeights;
	rwStaticVector<float32> inverseMatrices;

	/* mesh extension */
	bool hasMeshExtension;
	MeshExtension *meshExtension;

	/* night vertex colors */
	bool hasNightColors;
	uint32 nightColorsUnknown;
	rwStaticVector<uint8> nightColors;

	/* 2dfx */
	// to do

	/* morph (only flag) */
	bool hasMorph;

	/* functions */
	void read(std::istream &dff);
	void readExtension(std::istream &dff);
	void readMeshExtension(std::istream &dff);
	uint32 write(std::ostream &dff);
	uint32 writeMeshExtension(std::ostream &dff);

	void cleanUp(void);

	void dump(uint32 index, std::string ind = "", bool detailed = false);
private:
	void readPs2NativeData(std::istream &dff);
	void readXboxNativeData(std::istream &dff);
	void readXboxNativeSkin(std::istream &dff);
	void readOglNativeData(std::istream &dff, int size);
	void readNativeSkinMatrices(std::istream &dff);
	bool isDegenerateFace(uint32 i, uint32 j, uint32 k);
	void generateFaces(void);
	void deleteOverlapping(rwStaticVector<uint32> &typesRead, uint32 split);
	void readData(uint32 vertexCount, uint32 type, // native data block
                      uint32 split, std::istream &dff);

	uint32 addTempVertexIfNew(uint32 index);
};

struct Clump : public RwObject
{
#if 0
	rwStaticVector<Atomic> atomicList;
	rwStaticVector<Frame> frameList;
	rwStaticVector<Geometry> geometryList;
#endif

	/* Extensions */
	/* collision file */
	// to do

	/* functions */
	void read(std::istream &dff);
	void readExtension(std::istream &dff);
	uint32 write(std::ostream &dff);
	void dump(bool detailed = false);
	void clear(void);
};

} // namespace rw