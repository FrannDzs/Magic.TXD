/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/tools/configtree.h
*  PURPOSE:     Configuration tree node system.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#pragma once

#include <string>
#include <map>

struct ConfigNode
{
    inline ConfigNode( void )
    {
        this->parent = nullptr;
    }

    inline ~ConfigNode( void )
    {
        return;
    }

    void SetString( std::string key, std::string value );
    void SetInt( std::string key, int value );
    void SetFloat( std::string key, double value );
    void SetBoolean( std::string key, bool value );

    bool GetString( const std::string& key, std::string& valueOut ) const;
    bool GetInt( const std::string& key, int& valueOut ) const;
    bool GetFloat( const std::string& key, double& valueOut ) const;
    bool GetBoolean( const std::string& key, bool& valueOut ) const;

    void SetParent( const ConfigNode *parent )
    {
        this->parent = parent;
    }

private:
    const ConfigNode *parent;

    enum class eConfigType
    {
        STRING,
        INT,
        FLOAT,
        BOOL
    };

    struct configValueType
    {
        eConfigType type;
        struct
        {
            std::string string;
            int integer;
            double floating;
            bool boolean;
        } data;
    };

    std::map <std::string, configValueType> values;
};

// Helpers.
inline bool GetConfigNodeBoolean( const ConfigNode& cfgNode, const std::string& key, bool defaultValue )
{
    cfgNode.GetBoolean( key, defaultValue );
    return defaultValue;
}

inline int GetConfigNodeInt( const ConfigNode& cfgNode, const std::string& key, int defaultValue )
{
    cfgNode.GetInt( key, defaultValue );
    return defaultValue;
}

inline double GetConfigNodeFloat( const ConfigNode& cfgNode, const std::string& key, double defaultValue )
{
    cfgNode.GetFloat( key, defaultValue );
    return defaultValue;
}

inline std::string GetConfigNodeString( const ConfigNode& cfgNode, const std::string& key, std::string defaultValue )
{
    cfgNode.GetString( key, defaultValue );
    return std::move( defaultValue );
}