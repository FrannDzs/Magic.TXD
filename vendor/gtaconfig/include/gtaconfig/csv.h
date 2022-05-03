/*	CSV reading, MYSQL type environment (as these files are MYSQL database backups)
	General Notice: Always close .csv files if you dont need them anymore.
*/

#ifndef _CSV_
#define _CSV_

#include <vector>

#define CSVERR_FILEOPEN 0

#define CSV_DEFAULT_SEPERATION ','

typedef unsigned char **CSV_ROW;

class CCSV
{
    friend CCSV*    CreateCSV( const filePath& filename );
	friend CCSV*	LoadCSV( const filePath& filename );

public:
					CCSV( CFile *ioptr, bool closeIOOnDelete = true, char seperator = CSV_DEFAULT_SEPERATION );
					~CCSV();

	unsigned int	GetCurrentLine();

    fsOffsetNumber_t GetCursor();
    void            SetCursor( fsOffsetNumber_t cursor );

    void            WriteNextRow( const std::vector <std::string>& items );
	bool			ReadNextRow();
	unsigned int	GetItemCount();
	const char*		GetRowItem(unsigned int id);
	const char**	GetRow();
	void			FreeRow();

    void            ParsingError( const char *msg );
    
    // Helper functions.
    bool            ExpectTokenCount( unsigned int numTokens );

private:
	CFile*		    m_file;
    bool            m_closeIOOnDelete;
	unsigned int	m_currentLine;
	unsigned int	m_numItems;
	char**			m_row;
    char            m_seperator;
};

// Global defines
CCSV*   CreateCSV( const filePath& filename );
CCSV*	LoadCSV( const filePath& filename );

#endif
