#pragma once

// We want the ability to read from Win32 streams.
rw::Stream* RwStreamCreateFromWin32( rw::Interface *engineInterface, IStream *pStream );