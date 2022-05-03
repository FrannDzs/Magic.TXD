/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/renderware.utils.h
*  PURPOSE:     Common useful definitions.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

// Here you find useful utilities that you do not want to redo (but you can) if you are working with rwlib.
// TODO: very similar to QoL actually, should refactor this.

namespace rw
{

namespace utils
{

struct bufferedWarningManager : public WarningManagerInterface
{
    void OnWarning( rwStaticString <wchar_t>&& message ) override;

    void forward( Interface *engineInterface );

private:
    rwStaticVector <rwStaticString <wchar_t>> messages;
};

struct stacked_warnman_scope
{
    inline stacked_warnman_scope( Interface *engineInterface, WarningManagerInterface *newMan )
    {
        this->prevWarnMan = engineInterface->GetWarningManager();
        
        engineInterface->SetWarningManager( newMan );

        this->rwEngine = engineInterface;
    }

    inline ~stacked_warnman_scope( void )
    {
        this->rwEngine->SetWarningManager( this->prevWarnMan );
    }

private:
    Interface *rwEngine;

    WarningManagerInterface *prevWarnMan;
};

struct stacked_warnlevel_scope
{
    inline stacked_warnlevel_scope( Interface *engineInterface, int level )
    {
        this->prevLevel = engineInterface->GetWarningLevel();

        engineInterface->SetWarningLevel( level );

        this->engineInterface = engineInterface;
    }

    inline ~stacked_warnlevel_scope( void )
    {
        this->engineInterface->SetWarningLevel( this->prevLevel );
    }

private:
    Interface *engineInterface;

    int prevLevel;
};

// Standardized string chunk utilities.
void writeStringChunkANSI( Interface *engineInterface, BlockProvider& outputProvider, const char *string, size_t strLen );
void readStringChunkANSI( Interface *engineInterface, BlockProvider& inputProvider, rwStaticString <char>& stringOut );

}

} // namespace rw