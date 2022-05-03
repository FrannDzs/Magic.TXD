/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/languages.hxx
*  PURPOSE:     Internal header of the editor languages system.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#pragma once

#include "guiserialization.hxx"

class MagicLanguage
{
    friend class MagicLanguages;

    QString languageFilePath;
    QString languageFileName;
    
    LanguageInfo info;

    QHash<QString, QString> strings;

    bool lastSearchSuccesfull;

    static QString getStringFormattedWithVars(QString&& string);

    static QString keyNotDefined(QString key);

    static bool getLanguageInfo(QString filepath, LanguageInfo &info);

    bool loadText();

    void clearText();

    QString getText(QString key, bool *found = nullptr);
};

class MagicLanguages
{
public:
    unsigned int getNumberOfLanguages();

    QString getByKey(QString key, bool *found = nullptr);

    void scanForLanguages(QString languagesFolder);

    void updateLanguageContext();

protected:
    bool _selectLanguageByIndex(unsigned int index);
    MagicLanguage* findLanguageByName(const QString& name);
public:
    bool selectLanguageByLanguageName(QString langName);
    bool selectLanguageByLanguageAbbr(QString abbr);
    bool selectLanguageByFileName(QString filename);

    QVector <LanguageInfo> getLanguageInfos( void );
    QString getCurrentLanguageName( void );
    QString getCurrentLanguageFileName( void );
    QString getCurrentLanguageAuthors( void );

    inline MagicLanguages( void )
    {
        this->isInitialized = false;

        this->mainWnd = nullptr;
        this->execMan = nullptr;
        this->lockAtomic = nullptr;

        this->currentLanguage = -1;
    }

    inline void Initialize( MainWindow *mainWnd )
    {
        this->mainWnd = mainWnd;

        NativeExecutive::CExecutiveManager *execMan = (NativeExecutive::CExecutiveManager*)rw::GetThreadingNativeManager( mainWnd->GetEngine() );
        this->execMan = execMan;

        this->lockAtomic = execMan->CreateReadWriteLock();

        assert( this->lockAtomic != nullptr );

        // read languages
        scanForLanguages(mainWnd->makeAppPath("languages"));

        bool hasAcquiredLanguage = false;

        // First we try detecting a language by the system locale.
        if ( !hasAcquiredLanguage )
        {
            QLocale::Language lang = QLocale::system().language();

            QString langString = QLocale::languageToString( lang );

            hasAcquiredLanguage = selectLanguageByLanguageName( std::move( langString ) );
        }

        if ( !hasAcquiredLanguage )
        {
            // Select the default language then.
            if ( selectLanguageByLanguageName(DEFAULT_LANGUAGE) )
            {
                hasAcquiredLanguage = true;
            }
        }

        // If anything else failed, we just try selecting the first language that is registered.
        if ( !hasAcquiredLanguage )
        {
            if ( _selectLanguageByIndex( 0 ) )
            {
                hasAcquiredLanguage = true;
            }
        }

        if ( !hasAcquiredLanguage )
        {
            // If we have not acquired a language, we must initialize using the placeholders.
            updateLanguageContext();
        }

        // Allow initialization of language items during registration.
        this->isInitialized = true;
    }

    inline void Shutdown( MainWindow *mainWnd )
    {
        this->isInitialized = false;

        this->execMan->CloseReadWriteLock( this->lockAtomic );

        this->execMan = nullptr;
        this->lockAtomic = nullptr;

        this->mainWnd = nullptr;
    }

    void Load( MainWindow *mainWnd, rw::BlockProvider& configBlock )
    {
        rw::rwStaticString <wchar_t> langfile_str;

        langfile_str = RwReadUnicodeString( configBlock );

        mainWnd->lastLanguageFileName = wide_to_qt( langfile_str );

        // Load the language.
        bool loadedLanguage = selectLanguageByFileName( mainWnd->lastLanguageFileName );
        (void)loadedLanguage;

        // Since loading the configuration is optional, we do not care if we failed to load a language here.
    }

    void Save( const MainWindow *mainWnd, rw::BlockProvider& configBlock ) const
    {
        RwWriteUnicodeString( configBlock, qt_to_widerw( mainWnd->lastLanguageFileName ) );
    }

    NativeExecutive::CExecutiveManager *execMan;
    NativeExecutive::CReadWriteLock *lockAtomic;

    bool isInitialized;

    MainWindow *mainWnd;

    QVector<MagicLanguage> languages;

    int currentLanguage; // index of current language in languages array, -1 if not defined

    localizations_t culturalItems;
};

extern optional_struct_space <MagicLanguages> ourLanguages;