// Functions to handle .cvs files
#include "StdInc.h"

#include <stdio.h>

/*======================
	Init
======================*/

CCSV*   CreateCSV( const filePath& filename )
{
    CFile *file = fileRoot->Open( filename, "w" );

    if ( !file )
        return NULL;

    return new CCSV( file );
}

// Inits .csv files
CCSV*	LoadCSV( const filePath& filename )
{
	CFile *file = fileRoot->Open( filename, "r" );

	if (!file)
		return NULL;

    return new CCSV( file );
}

CCSV::CCSV( CFile *ioptr, bool closeIOOnDelete, char seperator )
{
    m_currentLine = 0;
    m_numItems = 0;
    m_file = ioptr;
    m_closeIOOnDelete = closeIOOnDelete;
    m_row = NULL;
    m_seperator = seperator;
}

CCSV::~CCSV()
{
	// Freeing memory used by row
	FreeRow();

    if ( this->m_closeIOOnDelete )
    {
	    delete m_file;
    }
}

/*======================
	CSV Functions
======================*/

fsOffsetNumber_t CCSV::GetCursor( void )
{
    return m_file->TellNative();
}

void CCSV::SetCursor( fsOffsetNumber_t cursor )
{
    m_file->SeekNative( cursor, SEEK_SET );
}

unsigned int	CCSV::GetItemCount()
{
	return m_numItems;
}

unsigned int	CCSV::GetCurrentLine()
{
	return m_currentLine;
}

// Writes a line
void    CCSV::WriteNextRow( const std::vector <std::string>& items )
{
    std::vector <std::string>::const_iterator iter = items.begin();

    while ( iter != items.end() )
    {
        const std::string& item = *iter;
        m_file->Write( item.c_str(), item.size() );

        iter++;

        if ( iter != items.end() )
        {
            m_file->WriteByte( this->m_seperator );
        }
    }

    m_file->WriteByte( '\n' );

	m_currentLine++;
}

// Gets a line
bool	CCSV::ReadNextRow()
{
	unsigned int seek = 0;
	unsigned int numSep = 1;
	unsigned int buffpos = 0;
	char itembuff[65537];
	char linebuff[65537];
	char lastchr = 1;

    // Free memory from last row.
    FreeRow();

	// Get a complete line, until a NULL termination appears, and check, how many seperation are in this line
	while (seek < 65536 && lastchr != 0 && lastchr != '\n' && lastchr != -1)
	{
		if (!m_file->Read(&lastchr, 1))
			return false;

		linebuff[seek] = lastchr;

		if (lastchr == this->m_seperator)
			numSep++;

		seek++;
	}

	linebuff[seek++] = 0;

	if (lastchr == -1 && seek == 2)
		return false;

	m_currentLine++;

    if ( numSep > 0 )
    {
	    // Malloc first memory
	    m_row = (char**)calloc(numSep, sizeof(const char*));
    }

	// Filter all things from the linebuff
	for (unsigned int n=0; n<seek; n++)
	{
		lastchr = linebuff[n];

		switch(lastchr)
		{
		case '	':
		case 13:
		case 10:
		case ' ':
			break;
		default:
            if ( lastchr == this->m_seperator || lastchr == 0 )
            {
                if ( m_numItems != 0 || buffpos != 0 )
                {
			        itembuff[buffpos] = 0;

                    size_t itemLen = buffpos;
			        buffpos = 0;

                    char*& rowItem = m_row[m_numItems];

			        rowItem = (char*)malloc(itemLen + 1);
			        memcpy(rowItem, itembuff, itemLen);

                    rowItem[itemLen] = '\0';

			        m_numItems++;
                }
            }
            else
            {
			    itembuff[buffpos++] = lastchr;
            }

			break;
		}
	}

	return true;
}

const char*	CCSV::GetRowItem(unsigned int id)
{
	if (!m_row)
		return NULL;

	if (id > m_numItems)
		return NULL;

	return m_row[id];
}

const char**	CCSV::GetRow()
{
	return (const char**)m_row;
}

void	CCSV::FreeRow()
{
	unsigned int n;

	if (!m_row)
		return;

	for (n=0; n < m_numItems; n++)
		free(m_row[n]);

	free(m_row);

	m_numItems = 0;
	m_row = NULL;
}

void CCSV::ParsingError( const char *msg )
{
    auto ansiPath = m_file->GetPath().convert_ansi <FileSysCommonAllocator> ();
    auto fileNameItem = FileSystem::GetFileNameItem <FileSysCommonAllocator> ( ansiPath.GetConstString(), true );

    fileNameItem.transform_to <char> ();

    printf( "CSV ERROR - %s (line %u): %s\n", fileNameItem.to_char <char> (), m_currentLine, msg );
}

bool CCSV::ExpectTokenCount( unsigned int numTokens )
{
    unsigned int actualItemCount = GetItemCount();

	if (actualItemCount < numTokens)
    {
        std::string errorMsg = "expected " + std::to_string( numTokens ) + " items, got " + std::to_string( actualItemCount );

        ParsingError( errorMsg.c_str() );
		return false;
    }

    return true;
}
