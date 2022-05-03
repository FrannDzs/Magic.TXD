#include "StdInc.h"

#include <memory>

DynObjConfig* DynObjConfig::LoadConfig( const filePath& path )
{
    // Loading this should be pretty straight forward.
    std::unique_ptr <CFile> sourceFile( fileRoot->Open( path, "r" ) );

    if ( !sourceFile )
        return NULL;

    DynObjConfig *config = new DynObjConfig();

    if ( !config )
        return NULL;

    // Read the file.
    while ( !sourceFile->IsEOF() )
    {
        std::string cfgline;

        bool gotLine = Config::GetConfigLine( sourceFile.get(), cfgline );

        if ( !gotLine || cfgline.empty() )
            continue;

        char c = cfgline.front();

        if ( c == '\0' || c == ';' )
            continue;

        if ( c == '*' )
            break;

        // Load our config.
        char name[0x100];
        unsigned int CDEff;
        unsigned int SpCDR;
        unsigned int CamAv;
        unsigned int Expl;
        unsigned int fxType;
        char fxName[0x100];

        attrib attribIn;

        float fxOff_x, fxOff_y, fxOff_z;
        float smashMult;
        float smashVel_x, smashVel_y, smashVel_z;
        float smashRand;
        unsigned int b_gun, b_spk;

        int numParse = sscanf( cfgline.c_str(),
            "%255s %f %f %f %f %f %f %f %d %d %d %d %d %f %f %f %255s %f %f %f %f %f %d %d",    // fixed gun mode and spark parameters (R* slip-up)
            name, &attribIn.mass, &attribIn.turnMass, &attribIn.airResist, &attribIn.elasticity,
            &attribIn.percSubmerged, &attribIn.uprootLimit, &attribIn.colDmgMult, &CDEff, &SpCDR, &CamAv, &Expl, &fxType,
            &fxOff_x, &fxOff_y, &fxOff_z, fxName,
            &smashMult, &smashVel_x, &smashVel_y, &smashVel_z, &smashRand,
            &b_gun, &b_spk
        );

        // We at least need the default params.
        if ( numParse >= 8 )
        {
            // Finalize the storage.
            attribIn.objName = name;

            // Store this.
            config->attributeMap.insert( std::make_pair( name, std::move( attribIn ) ) );
        }
    }

    return config;
}

DynObjConfig::DynObjConfig( void )
{
    return;
}

DynObjConfig::~DynObjConfig( void )
{
    return;
}

const DynObjConfig::attrib* DynObjConfig::GetObjectAttributes( const char *objName ) const
{
    auto findIter = this->attributeMap.find( objName );

    if ( findIter == this->attributeMap.end() )
        return NULL;

    return &findIter->second;
}