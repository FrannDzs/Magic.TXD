#ifndef _OBJ_DYNAMIC_CONFIG_
#define _OBJ_DYNAMIC_CONFIG_

struct DynObjConfig
{
    static DynObjConfig* LoadConfig( const filePath& path );

private:
    DynObjConfig( void );

public:
    ~DynObjConfig( void );

    struct attrib
    {
        // We only care about standard attributes (for now?).
        std::string objName;
        float mass;
        float turnMass;
        float airResist;
        float elasticity;
        float percSubmerged;
        float uprootLimit;
        float colDmgMult;
    };

    const attrib* GetObjectAttributes( const char *objName ) const;

private:
    std::map <std::string, attrib> attributeMap;
};

#endif //_OBJ_DYNAMIC_CONFIG_