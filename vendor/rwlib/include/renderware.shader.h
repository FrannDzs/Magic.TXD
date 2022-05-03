/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/renderware.shared.h
*  PURPOSE:     API for managing RenderWare GPU programs.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

namespace rw
{

enum eDriverProgType
{
    DRIVER_PROG_VERTEX,
    DRIVER_PROG_FRAGMENT,
    DRIVER_PROG_HULL
};

struct DriverProgram
{
    const void* GetBytecodeBuffer( void ) const;
    size_t GetBytecodeSize( void ) const;
};

// Compilation API.
DriverProgram* CompileNativeProgram( Interface *engineInterface, const char *nativeName, const char *entryPointName, eDriverProgType progType, const char *shaderSource, size_t shaderSize );
void DeleteDriverProgram( DriverProgram *handle );

} // namespace rw