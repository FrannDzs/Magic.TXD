/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/qtinteroputils.hxx
*  PURPOSE:     Interop utilities of magic-rw and the editor.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#ifndef _QT_INTEROP_UTILS_INTERNAL_
#define _QT_INTEROP_UTILS_INTERNAL_

#include <QtWidgets/QComboBox>

AINLINE bool qstring_native_compare( const QString& left, const char *right )
{
    int leftLen = left.length();

    const char *rightPtr = right;

    for ( int n = 0; n < leftLen; n++ )
    {
        char c = *rightPtr;

        if ( c == '\0' )
        {
            return false;
        }

        QChar leftChar = QChar::fromLatin1( c );
        QChar rightChar = left.at( n );

        if ( leftChar != rightChar )
        {
            return false;
        }

        rightPtr++;
    }

    return ( *rightPtr == '\0' );
}

template <typename modeType>
struct naturalModeList : public std::list <modeType>
{
    inline naturalModeList( std::initializer_list <modeType> list ) : std::list <modeType> ( list )
    {}

    typedef decltype( modeType::mode ) mode_t;

    inline bool getNaturalFromMode( mode_t mode, QString& naturalOut ) const
    {
        auto iter = std::find( this->begin(), this->end(), mode );

        if ( iter == this->end() )
            return false;

        naturalOut = QString::fromStdString( iter->natural );
        return true;
    }

    inline bool getModeFromNatural( const QString& natural, mode_t& modeOut ) const
    {
        auto iter = std::find( this->begin(), this->end(), natural );

        if ( iter == this->end() )
            return false;

        modeOut = iter->mode;
        return true;
    }

    inline void selectCurrent( QComboBox *box, mode_t curMode ) const
    {
        QString nat;

        bool gotNatural = getNaturalFromMode( curMode, nat );

        if ( gotNatural )
        {
            box->setCurrentText( nat );
        }
    }

    inline void getCurrent( QComboBox *box, mode_t& modeOut ) const
    {
        QString currentText = box->currentText();

        getModeFromNatural( currentText, modeOut );
    }

    inline void putDown( QComboBox *box ) const
    {
        for ( const modeType& item : *this )
        {
            box->addItem( QString::fromStdString( item.natural ) );
        }
    }
};

#endif //_QT_INTEROP_UTILS_INTERNAL_
