/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/renderware.material.h
*  PURPOSE:     Old Material structures
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

namespace rw
{

struct MatFx
{
	uint32 type;

	float32 bumpCoefficient;
	float32 envCoefficient;
	float32 srcBlend;
	float32 destBlend;

	bool hasTex1;
	Texture *tex1;
	bool hasTex2;
	Texture *tex2;
	bool hasDualPassMap;
	Texture *dualPassMap;

	void dump(std::string ind = "");

	MatFx(void);
};

struct Material : public RwObject
{
    inline Material( Interface *engineInterface, void *construction_params ) : RwObject( engineInterface, construction_params )
    {
        this->flags = 0;
        this->unknown = 0;
        this->hasTex = false;
        this->hasRightToRender = false;
        this->rightToRenderVal1 = 0;
        this->rightToRenderVal2 = 0;
        this->hasMatFx = false;
        this->matFx = 0;
        this->hasReflectionMat = false;
        this->reflectionIntensity = 0.0f;
        this->hasSpecularMat = false;
        this->specularLevel = 0.0f;

        this->texture = nullptr;

        for (int i = 0; i < 4; i++)
        {
            color[i] = 0;
        }

        for (int i = 0; i < 3; i++)
        {
            surfaceProps[i] = 0.0f;
            reflectionChannelAmount[i] = 0.0f;
        }
    }

    inline ~Material( void )
    {
        if ( matFx )
        {
            delete matFx;
        }
    }

	uint32 flags;
	uint8 color[4];
	uint32 unknown;
	bool hasTex;
	float32 surfaceProps[3]; /* ambient, specular, diffuse */

	Texture *texture;

	/* Extensions */

	/* right to render */
	bool hasRightToRender;
	uint32 rightToRenderVal1;
	uint32 rightToRenderVal2;

	/* matfx */
	bool hasMatFx;
	MatFx *matFx;

	/* reflection mat */
	bool hasReflectionMat;
	float32 reflectionChannelAmount[4];
	float32 reflectionIntensity;

	/* specular mat */
	bool hasSpecularMat;
	float32 specularLevel;
	std::string specularName;

	/* uv anim */
	// to do

	/* functions */
	void read(std::istream &dff);
	void readExtension(std::istream &dff);
	uint32 write(std::ostream &dff);

	void dump(uint32 index, std::string ind = "");
};

} // namespace rw