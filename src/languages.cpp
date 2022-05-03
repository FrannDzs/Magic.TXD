/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/languages.cpp
*  PURPOSE:     Language system of the editor.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#include "mainwindow.h"

#include "languages.h"
#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include "testmessage.h"

#include "guiserialization.hxx"

#include "languages.hxx"

// Since there can be only one instance of Magic.TXD per application, we can use a global.
optional_struct_space <MagicLanguages> ourLanguages;

struct magicLanguagesMainWindowEnv : public magicSerializationProvider
{
    inline void Initialize( MainWindow *mainWnd )
    {
        ourLanguages.get().Initialize( mainWnd );

        // Register us as serialization extension.
        RegisterMainWindowSerialization( mainWnd, MAGICSERIALIZE_LANGUAGE, this );
    }

    inline void Shutdown( MainWindow *mainWnd )
    {
        // Unregister us again.
        UnregisterMainWindowSerialization( mainWnd, MAGICSERIALIZE_LANGUAGE );

        ourLanguages.get().Shutdown( mainWnd );
    }

    void Load( MainWindow *mainWnd, rw::BlockProvider& configBlock ) override
    {
        ourLanguages.get().Load( mainWnd, configBlock );
    }

    void Save( const MainWindow *mainWnd, rw::BlockProvider& configBlock ) const override
    {
        ourLanguages.get().Save( mainWnd, configBlock );
    }
};

static optional_struct_space <PluginDependantStructRegister <magicLanguagesMainWindowEnv, mainWindowFactory_t>> magicLanguagesMainWindowRegister;

void InitializeMagicLanguages( void )
{
    ourLanguages.Construct();
    magicLanguagesMainWindowRegister.Construct( mainWindowFactory );
}

void ShutdownMagicLanguages( void )
{
    magicLanguagesMainWindowRegister.Destroy();
    ourLanguages.Destroy();
}

bool RegisterTextLocalizationItem( magicTextLocalizationItem *provider )
{
    bool success = false;
    bool doUpdate = false;
    {
        NativeExecutive::CReadWriteWriteContextSafe <> ctx_set( ourLanguages.get().lockAtomic );

        // Register this.
        ourLanguages.get().culturalItems.push_back( provider );

        // Need to be initialized to update any content.
        if ( ourLanguages.get().isInitialized )
        {
            // Update the text in the language item.
            // We do that all the time.
            //if ( ourLanguages.currentLanguage != -1 )
            {
                doUpdate = true;
            }
        }

        success = true;
    }

    if ( doUpdate )
    {
        provider->updateContent( ourLanguages.get().mainWnd );
    }

    return success;
}

bool UnregisterTextLocalizationItem( magicTextLocalizationItem *provider )
{
    bool success = false;
    {
        NativeExecutive::CReadWriteWriteContextSafe <> ctx_set( ourLanguages.get().lockAtomic );

        // Try to find this item.
        localizations_t::const_iterator iter = std::find( ourLanguages.get().culturalItems.begin(), ourLanguages.get().culturalItems.end(), provider );

        if ( iter != ourLanguages.get().culturalItems.end() )
        {
            // Remove it.
            ourLanguages.get().culturalItems.erase( iter );

            success = true;
        }
    }

    return success;
}

localizations_t GetTextLocalizationItems( void )
{
    NativeExecutive::CReadWriteReadContext <> ctx_get( ourLanguages.get().lockAtomic );

    return ourLanguages.get().culturalItems;
}

QString getLanguageItemByKey( QString token, bool *found )  // RIP mainWnd param
{
    return ourLanguages.get().getByKey( token, found );
}

struct magic_value_item_t
{
    const char *key;
    const char *value;
};

static magic_value_item_t valueVars[] =
{
    { "_PARAM_1",        "%1" },
    { "_PARAM_2",        "%2" },
    { "_MAGIC_TXD_NAME", "Magic.TXD" },
    { "_AUTHOR_NAME_1",  "DK22Pac" },
    { "_AUTHOR_NAME_2",  "The_GTA" },
};

QString MagicLanguage::getStringFormattedWithVars(QString&& string) {
    QString result = std::move( string );
    for (unsigned int i = 0; i < NUMELMS(valueVars); i++)
    {
        const magic_value_item_t& valuePair = valueVars[ i ];
        result.replace( valuePair.key, valuePair.value );
    }
    return result;
}

bool MagicLanguage::loadText()
{
    static const QRegularExpression regExp1("[\\S]");
    static const QRegularExpression regExp2("[\\s]");

    QFile file(languageFilePath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QTextStream in(&file);
    in.setCodec( "UTF-8" );         // spotted by The_Hero!
    in.setAutoDetectUnicode( true );

    in.readLine(); // skip header line

    while (!in.atEnd())
    {
        QString line = in.readLine();

        if ( line.isEmpty() )
            continue;

        int key_start = line.indexOf(regExp1);

        if (key_start == -1)
            continue;

        // Ignore commented lines.
        if (line.at(key_start) == '#')
            continue;

        QRegExp regExpTokenEnclose("\\[(\\S+)\\]");

        // Check what kind of type of line we have.
        int token_enclose_start = line.indexOf( regExpTokenEnclose );

        if (token_enclose_start != -1)
        {
            QString keyToken = getStringFormattedWithVars( regExpTokenEnclose.cap( 1 ) );

            // In this we read lines until we find the end token.
            bool didHaveLine = false;

            QString localeItem;

            while ( !in.atEnd() )
            {
                QString locale_line = in.readLine();

                // If we found the ending token, we quit.
                int token_end_start = locale_line.indexOf( regExpTokenEnclose );

                if ( token_end_start != -1 && regExpTokenEnclose.cap( 1 ).compare( "END", Qt::CaseInsensitive ) == 0 )
                {
                    // We are at the end, so quit.
                    break;
                }

                // Add a new locale line.
                if ( didHaveLine )
                {
                    localeItem += '\n';
                }

                localeItem += locale_line;

                didHaveLine = true;
            }

            // Register this item.
            strings.insert(
                keyToken,
                getStringFormattedWithVars( std::move( localeItem ) )
            );
        }
        else
        {
            int key_end = line.indexOf(regExp2, key_start);

            rw::rwStaticString <wchar_t> debug = qt_to_widerw( line );

            if (key_end != -1)
            {
                int value_start = line.indexOf(regExp1, key_end);
                if (value_start != -1)
                {
                    QString keyToken = line.mid(key_start, key_end - key_start);
                    QString valueToken = line.mid(value_start);

                    if ( keyToken.isEmpty() == false && valueToken.isEmpty() == false )
                    {
                        strings.insert(
                            getStringFormattedWithVars(std::move( keyToken )),
                            getStringFormattedWithVars(std::move( valueToken ))
                        );
                        //TestMessage(L"key: \"%s\" value: \"%s\"", getStringFormattedWithVars(line.mid(key_start, key_end - key_start)).toStdWString().c_str(),
                        //    getStringFormattedWithVars(line.mid(value_start)).toStdWString().c_str());
                    }
                }
            }
        }
    }
    return true;
}

bool MagicLanguage::getLanguageInfo(QString filepath, LanguageInfo &info) {
    QFile file(filepath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {

        QTextStream in(&file);
        in.setCodec( "UTF-8" );         // spotted by The_Hero!
        in.setAutoDetectUnicode( true );

        if (!in.atEnd()) {
            QString line = in.readLine();
            QStringList strList = line.split('|');

            // MAGL|1|English|English|ENG|Magic.TXD Team

            if (strList.size() >= 6 && strList[0] == "MAGL"){
                int version = strList[1].toInt();
                if (version >= MINIMUM_SUPPORTED_MAGL_VERSION && version <= CURRENT_MAGL_VERSION) {
                    info.version = version;
                    if ( version == 1 )
                    {
                        info.name = strList[2];
                        info.nameInOriginal = strList[3];
                        info.abbr = strList[4];
                        info.authors = strList[5];
                        info.rwlangkey = info.abbr;
                        info.parentlangname = "none";
                    }
                    else if ( version == 2 )
                    {
                        info.name = strList[2];
                        info.nameInOriginal = strList[3];
                        info.abbr = strList[4];
                        info.rwlangkey = strList[5];
                        info.parentlangname = strList[6];
                        info.authors = strList[7];
                    }

                    if ( info.name == "none" )
                    {
                        return false;
                    }

                    //TestMessage(L"vesrion: %d, name: %s, original: %s, abbr: %s, authors: %s", info.version,
                    //    info.name.toStdWString().c_str(), info.nameInOriginal.toStdWString().c_str(),
                    //    info.abbr.toStdWString().c_str(), info.authors.toStdWString().c_str());

                    return true;
                }
            }
        }
    }
    return false;
}

void MagicLanguage::clearText() {
    strings.clear();
}

QString MagicLanguage::keyNotDefined(QString key) {
    if (key.size() > 0)
        return "N_" + key;
    else
        return "EMPTY_KEY";
}

QString MagicLanguage::getText(QString key, bool *found) {
    auto i = strings.find(key);
    if (i != strings.end()) {
        if (found)
            *found = true;
        return i.value();
    }
    else {
        if (found)
            *found = false;
        return keyNotDefined(key);
    }
}

unsigned int MagicLanguages::getNumberOfLanguages() {
    NativeExecutive::CReadWriteReadContext <> ctx_get( this->lockAtomic );
    return languages.size();
}

QString MagicLanguages::getByKey(QString key, bool *found) {
    NativeExecutive::CReadWriteReadContext <> ctx_get( this->lockAtomic );
    if (currentLanguage != -1)
    {
        MagicLanguage *tryLang = &languages[currentLanguage];

        size_t try_idx = 0;

        do
        {
            bool cfound;
            QString text = tryLang->getText(key, &cfound);

            if ( cfound )
            {
                if ( found )
                {
                    *found = true;
                }
                return text;
            }

            try_idx++;

            // Just try up to five times to prevent infinite recursion.
            if ( try_idx == 5 )
            {
                break;
            }

            // Try the parent language next
            tryLang = findLanguageByName(tryLang->info.parentlangname);
        }
        while ( tryLang != nullptr );
    }

    if (found)
        *found = false;
    return MagicLanguage::keyNotDefined(key);
}

void MagicLanguages::scanForLanguages(QString languagesFolder)
{
    NativeExecutive::CReadWriteWriteContext <> ctx_set( this->lockAtomic );

    QDirIterator dirIt(languagesFolder);

    while (dirIt.hasNext())
    {
        dirIt.next();

        QFileInfo fileInfo( dirIt.fileInfo() );

        if (fileInfo.isFile())
        {
            if (fileInfo.suffix() == "magl")
            {
                LanguageInfo info;
                QString filePath = dirIt.filePath();

                if (MagicLanguage::getLanguageInfo(filePath, info))
                {
                    int newLangIndex = languages.size();

                    languages.resize(newLangIndex + 1);

                    MagicLanguage& theLang = languages[ newLangIndex ];
                    theLang.languageFilePath = filePath;
                    theLang.languageFileName = dirIt.fileName();
                    theLang.info = info;
                }
            }
        }
    }
}

void MagicLanguages::updateLanguageContext( void )
{
    // We want to update all language sensitive items.
    MainWindow *mainWnd = this->mainWnd;

    for ( magicTextLocalizationItem *localItem : this->culturalItems )
    {
        // Go ahead and tell our customer!
        localItem->updateContent( mainWnd );
    }
}

bool MagicLanguages::_selectLanguageByIndex(unsigned int index) {
    const unsigned int numLanguages = (unsigned int)ourLanguages.get().languages.size();
    if (numLanguages > 0 && index < numLanguages) {
        if (currentLanguage != -1)
        {
            MagicLanguage *clearLang = &languages[currentLanguage];

            do
            {
                clearLang->clearText();
                clearLang = findLanguageByName(clearLang->info.parentlangname);
            }
            while ( clearLang != nullptr );
        }
        currentLanguage = index;

        MagicLanguage *loadLang = &languages[currentLanguage];

        do
        {
            loadLang->loadText();
            loadLang = findLanguageByName(loadLang->info.parentlangname);
        }
        while ( loadLang != nullptr );

        // Load any parent languages aswell.

        return true;
    }
    return false;
}

MagicLanguage* MagicLanguages::findLanguageByName(const QString& name)
{
    if ( name == "none" )
    {
        return nullptr;
    }

    for ( MagicLanguage& lang : this->languages )
    {
        if ( lang.info.name == name )
        {
            return &lang;
        }
    }
    return nullptr;
}

bool MagicLanguages::selectLanguageByLanguageName(QString langName) {
    bool hasUpdated = false;
    if (langName != "none")
    {
        NativeExecutive::CReadWriteWriteContext <> ctx_set( this->lockAtomic );
        const unsigned int numLanguages = (unsigned int)ourLanguages.get().languages.size();
        for (unsigned int i = 0; i < numLanguages; i++) {
            if (languages[i].info.name == langName)
            {
                hasUpdated = _selectLanguageByIndex(i);
                break;
            }
        }
    }
    if ( hasUpdated )
    {
        // Update GUI.
        updateLanguageContext();
    }
    return hasUpdated;
}

bool MagicLanguages::selectLanguageByLanguageAbbr(QString abbr) {
    bool hasUpdated = false;
    {
        NativeExecutive::CReadWriteWriteContext <> ctx_set( this->lockAtomic );
        const unsigned int numLanguages = (unsigned int)ourLanguages.get().languages.size();
        for (unsigned int i = 0; i < numLanguages; i++) {
            if (languages[i].info.abbr == abbr)
            {
                hasUpdated = _selectLanguageByIndex(i);
                break;
            }
        }
    }
    if ( hasUpdated )
    {
        // Update GUI.
        updateLanguageContext();
    }
    return hasUpdated;
}

bool MagicLanguages::selectLanguageByFileName(QString filename) {
    bool hasUpdated = false;
    {
        NativeExecutive::CReadWriteWriteContext <> ctx_set( this->lockAtomic );
        const unsigned int numLanguages = (unsigned int)ourLanguages.get().languages.size();
        for (unsigned int i = 0; i < numLanguages; i++) {
            if (languages[i].languageFileName == filename)
            {
                hasUpdated = _selectLanguageByIndex(i);
                break;
            }
        }
    }
    if ( hasUpdated )
    {
        // Update GUI.
        updateLanguageContext();
    }
    return hasUpdated;
}

QVector <LanguageInfo> MagicLanguages::getLanguageInfos( void )
{
    NativeExecutive::CReadWriteReadContext <> ctx_get( this->lockAtomic );

    QVector <LanguageInfo> infos;

    for ( MagicLanguage& lang : this->languages )
    {
        infos.push_back( lang.info );
    }

    return infos;
}

QString MagicLanguages::getCurrentLanguageName( void )
{
    NativeExecutive::CReadWriteReadContext <> ctx_get( this->lockAtomic );

    if ( this->currentLanguage != -1 )
    {
        return this->languages[ this->currentLanguage ].info.name;
    }

    return "";
}

QString MagicLanguages::getCurrentLanguageFileName( void )
{
    NativeExecutive::CReadWriteReadContext <> ctx_get( this->lockAtomic );

    if ( this->currentLanguage != -1 )
    {
        return this->languages[ this->currentLanguage ].languageFileName;
    }

    return "";
}

QString MagicLanguages::getCurrentLanguageAuthors( void )
{
    NativeExecutive::CReadWriteReadContext <> ctx_get( this->lockAtomic );

    if ( this->currentLanguage != -1 )
    {
        return this->languages[ this->currentLanguage ].info.authors;
    }

    return "";
}

unsigned int GetTextWidthInPixels(QString &text, unsigned int fontSize)
{
    QFont font("Segoe UI Light");
    font.setPixelSize(fontSize);
    QFontMetrics fm(font);
    return fm.horizontalAdvance(text);
}
