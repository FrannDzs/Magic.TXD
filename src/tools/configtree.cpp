/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/tools/configtree.cpp
*  PURPOSE:     Configuration tree implementation for tools.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#include "mainwindow.h"

#include "configtree.h"

void ConfigNode::SetString( std::string key, std::string value )
{
    configValueType data;
    data.type = eConfigType::STRING;
    data.data.string = std::move( value );

    this->values[ std::move( key ) ] = std::move( data );
}

void ConfigNode::SetInt( std::string key, int value )
{
    configValueType data;
    data.type = eConfigType::INT;
    data.data.integer = value;

    this->values[ std::move( key ) ] = std::move( data );
}

void ConfigNode::SetFloat( std::string key, double value )
{
    configValueType data;
    data.type = eConfigType::FLOAT;
    data.data.floating = value;

    this->values[ std::move( key ) ] = std::move( data );
}

void ConfigNode::SetBoolean( std::string key, bool value )
{
    configValueType data;
    data.type = eConfigType::BOOL;
    data.data.boolean = value;

    this->values[ std::move( key ) ] = std::move( data );
}

bool ConfigNode::GetString( const std::string& key, std::string& valueOut ) const
{
    auto findIter = this->values.find( key );

    if ( findIter != this->values.end() )
    {
        const configValueType& data = findIter->second;

        eConfigType cfgType = data.type;

        if ( cfgType == eConfigType::BOOL )
        {
            valueOut = ( data.data.boolean ) ? "true" : "false";
            return true;
        }
        else if ( cfgType == eConfigType::INT )
        {
            valueOut = std::to_string( data.data.integer );
            return true;
        }
        else if ( cfgType == eConfigType::FLOAT )
        {
            valueOut = std::to_string( data.data.floating );
            return true;
        }
        else if ( cfgType == eConfigType::STRING )
        {
            valueOut = data.data.string;
            return true;
        }
    }
    else if ( const ConfigNode *parent = this->parent )
    {
        return parent->GetString( key, valueOut );
    }

    return false;
}

bool ConfigNode::GetInt( const std::string& key, int& valueOut ) const
{
    auto findIter = this->values.find( key );

    if ( findIter != this->values.end() )
    {
        const configValueType& data = findIter->second;

        eConfigType cfgType = data.type;

        if ( cfgType == eConfigType::BOOL )
        {
            valueOut = ( data.data.boolean ) ? 1 : 0;
            return true;
        }
        else if ( cfgType == eConfigType::INT )
        {
            valueOut = data.data.integer;
            return true;
        }
        else if ( cfgType == eConfigType::FLOAT )
        {
            valueOut = (int)data.data.floating;
            return true;
        }
        else if ( cfgType == eConfigType::STRING )
        {
            try
            {
                valueOut = std::stol( data.data.string );
                return true;
            }
            catch( ... )
            {
                // Wrongly formatted number.
                return false;
            }
        }
    }
    else if ( const ConfigNode *parent = this->parent )
    {
        return parent->GetInt( key, valueOut );
    }

    return false;
}

bool ConfigNode::GetFloat( const std::string& key, double& valueOut ) const
{
    auto findIter = this->values.find( key );

    if ( findIter != this->values.end() )
    {
        const configValueType& data = findIter->second;

        eConfigType cfgType = data.type;

        if ( cfgType == eConfigType::BOOL )
        {
            valueOut = ( data.data.boolean ) ? 1.0 : 0.0;
            return true;
        }
        else if ( cfgType == eConfigType::INT )
        {
            valueOut = (double)data.data.integer;
            return true;
        }
        else if ( cfgType == eConfigType::FLOAT )
        {
            valueOut = data.data.floating;
            return true;
        }
        else if ( cfgType == eConfigType::STRING )
        {
            try
            {
                valueOut = std::stod( data.data.string );
                return true;
            }
            catch( ... )
            {
                // Wrongly formatted number.
                return false;
            }
        }
    }
    else if ( const ConfigNode *parent = this->parent )
    {
        return parent->GetFloat( key, valueOut );
    }

    return false;
}

bool ConfigNode::GetBoolean( const std::string& key, bool& valueOut ) const
{
    auto findIter = this->values.find( key );

    if ( findIter != this->values.end() )
    {
        const configValueType& data = findIter->second;

        eConfigType cfgType = data.type;

        if ( cfgType == eConfigType::BOOL )
        {
            valueOut = data.data.boolean;
            return true;
        }
        else if ( cfgType == eConfigType::INT )
        {
            valueOut = ( data.data.integer != 0 );
            return true;
        }
        else if ( cfgType == eConfigType::FLOAT )
        {
            valueOut = ( data.data.floating != 0 );
            return true;
        }
        else if ( cfgType == eConfigType::STRING )
        {
            if ( data.data.string == "true" )
            {
                valueOut = true;
                return true;
            }
            else if ( data.data.string == "false" )
            {
                valueOut = false;
                return true;
            }

            return false;
        }
    }
    else if ( const ConfigNode *parent = this->parent )
    {
        return parent->GetBoolean( key, valueOut );
    }

    return false;
}