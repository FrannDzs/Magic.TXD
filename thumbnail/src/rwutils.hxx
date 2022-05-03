#pragma once

inline rw::TextureBase* GetFirstTexture( rw::TexDictionary *txd )
{
    rw::TexDictionary::texIter_t iter( txd->GetTextureIterator() );

    if ( iter.IsEnd() )
        return NULL;

    return iter.Resolve();
}