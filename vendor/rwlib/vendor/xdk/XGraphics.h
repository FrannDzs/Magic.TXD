#ifndef _XGRAPHICS_LITE_
#define _XGRAPHICS_LITE_

typedef std::uint32_t DWORD;

// Taken from the XDK.
typedef DWORD SWIZNUM;

class Swizzler
{
public:

    // Dimensions of the texture
    DWORD m_Width;
    DWORD m_Height;
    DWORD m_Depth;

    // Internal mask for each coordinate
    DWORD m_MaskU;
    DWORD m_MaskV;
    DWORD m_MaskW;

    // Swizzled texture coordinates
    DWORD m_u;
    DWORD m_v;
    DWORD m_w;

    Swizzler(): m_Width(0), m_Height(0), m_Depth(0),
        m_MaskU(0), m_MaskV(0), m_MaskW(0),
        m_u(0), m_v(0), m_w(0)
        { }

    // Initializes the swizzler
    Swizzler(
        DWORD width,
        DWORD height,
        DWORD depth
        )
    {
        Init(width, height, depth);
    }

    void Init(
        DWORD width,
        DWORD height,
        DWORD depth
        )
    {
        m_Width = width;
        m_Height = height;
        m_Depth = depth;
        m_MaskU = 0;
        m_MaskV = 0;
        m_MaskW = 0;
        m_u = 0;
        m_v = 0;
        m_w = 0;

        DWORD i = 1;
        DWORD j = 1;
        DWORD k;

        do
        {
            k = 0;
            if (i < width)
            {
                m_MaskU |= j;
                k = (j<<=1);
            }

            if (i < height)
            {
                m_MaskV |= j;
                k = (j<<=1);
            }

            if (i < depth)
            {
                 m_MaskW |= j;
                 k = (j<<=1);
            }

            i <<= 1;
        }
        while (k);
    }

    // Swizzles a texture coordinate
    SWIZNUM SwizzleU(
        DWORD num
        )
    {
        SWIZNUM r = 0;

        for (DWORD i = 1; i <= m_MaskU; i <<= 1)
        {
            if (m_MaskU & i)
            {
                r |= (num & i);
            }
            else
            {
                num <<= 1;
            }
        }

        return r;
    }

    SWIZNUM SwizzleV(
        DWORD num
        )
    {
        SWIZNUM r = 0;

        for (DWORD i = 1; i <= m_MaskV; i <<= 1)
        {
            if (m_MaskV & i)
            {
                r |= (num & i);
            }
            else
            {
                num <<= 1;
            }
        }

        return r;
    }

    SWIZNUM SwizzleW(
        DWORD num
        )
    {
        SWIZNUM r = 0;

        for (DWORD i = 1; i <= m_MaskW; i <<= 1)
        {
            if (m_MaskW & i)
            {
                r |= (num & i);
            }
            else
            {
                num <<= 1;
            }
        }

        return r;
    }

    SWIZNUM Swizzle(
        DWORD u,
        DWORD v,
        DWORD w
        )
    {
        return SwizzleU(u) | SwizzleV(v) | SwizzleW(w);
    }

    // Unswizzles a texture coordinate
    DWORD UnswizzleU(
        SWIZNUM num
        )
    {
        DWORD r = 0;

        for (DWORD i = 1, j = 1; i; i <<= 1)
        {
            if (m_MaskU & i)
            {
                r |= (num & j);
                j <<= 1;
            }
            else
            {
                num >>= 1;
            }
        }

        return r;
    }

    DWORD UnswizzleV(
        SWIZNUM num
        )
    {
        DWORD r = 0;

        for (DWORD i = 1, j = 1; i; i <<= 1)
        {
            if (m_MaskV & i)
            {
                r |= (num & j);
                j <<= 1;
            }
            else
            {
                num >>= 1;
            }
        }

        return r;
    }

    DWORD UnswizzleW(
        SWIZNUM num
        )
    {
        DWORD r = 0;

        for (DWORD i = 1, j = 1; i; i <<= 1)
        {
            if (m_MaskW & i)
            {
                r |= (num & j);
                j <<= 1;
            }
            else
            {
                num >>= 1;
            }
        }

        return r;
    }

    // Sets a texture coordinate
    AINLINE SWIZNUM SetU(SWIZNUM num) { return m_u = num /* & m_MaskU */; }
    AINLINE SWIZNUM SetV(SWIZNUM num) { return m_v = num /* & m_MaskV */; }
    AINLINE SWIZNUM SetW(SWIZNUM num) { return m_w = num /* & m_MaskW */; }

    // Adds a value to a texture coordinate
    AINLINE SWIZNUM AddU(SWIZNUM num) { return m_u = ( m_u - ( (0-num) & m_MaskU ) ) & m_MaskU; }
    AINLINE SWIZNUM AddV(SWIZNUM num) { return m_v = ( m_v - ( (0-num) & m_MaskV ) ) & m_MaskV; }
    AINLINE SWIZNUM AddW(SWIZNUM num) { return m_w = ( m_w - ( (0-num) & m_MaskW ) ) & m_MaskW; }

    // Subtracts a value from a texture coordinate
    AINLINE SWIZNUM SubU(SWIZNUM num) { return m_u = ( m_u - num /* & m_MaskU */ ) & m_MaskU; }
    AINLINE SWIZNUM SubV(SWIZNUM num) { return m_v = ( m_v - num /* & m_MaskV */ ) & m_MaskV; }
    AINLINE SWIZNUM SubW(SWIZNUM num) { return m_w = ( m_w - num /* & m_MaskW */ ) & m_MaskW; }

    // Increments a texture coordinate
    AINLINE SWIZNUM IncU()              { return m_u = ( m_u - m_MaskU ) & m_MaskU; }
    AINLINE SWIZNUM IncV()              { return m_v = ( m_v - m_MaskV ) & m_MaskV; }
    AINLINE SWIZNUM IncW()              { return m_w = ( m_w - m_MaskW ) & m_MaskW; }

    // Decrements a texture coordinate
    AINLINE SWIZNUM DecU()              { return m_u = ( m_u - 1 ) & m_MaskU; }
    AINLINE SWIZNUM DecV()              { return m_v = ( m_v - 1 ) & m_MaskV; }
    AINLINE SWIZNUM DecW()              { return m_w = ( m_w - 1 ) & m_MaskW; }

    // Gets the current swizzled address for a 2D or 3D texture
    AINLINE SWIZNUM Get2D()          { return m_u | m_v; }
    AINLINE SWIZNUM Get3D()          { return m_u | m_v | m_w; }
};

#endif //_XGRAPHICS_LITE_
