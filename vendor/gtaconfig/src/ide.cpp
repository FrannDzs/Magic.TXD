#include "StdInc.h"

// Get all possible ids from the IDE files
CIDE*	LoadIDE(const filePath& filename)
{
	CCSV *csv = LoadCSV(filename);

	if (!csv)
		return NULL;

	return new CIDE(csv);
}

CIDE::CIDE(CCSV *csv, bool isAllocated)
{
	m_csv = csv;
    m_isCSVAllocated = isAllocated;

	while (csv->ReadNextRow())
	{
        if ( csv->GetItemCount() == 0 )
            continue;

		const char *token = csv->GetRowItem(0);

		if (strcmp(token, "objs") == 0)
			ReadObjects();
	}
}

CIDE::~CIDE()
{
    IDE::objectList_t::iterator iter;

	for (iter = m_objects.begin(); iter != m_objects.end(); iter++)
    {
        IDE::CObject *object = *iter;

        free( (void*)object->m_modelName );
        free( (void*)object->m_textureName );

		delete object;
    }

    if ( this->m_isCSVAllocated )
    {
        delete m_csv;
    }
}

void	CIDE::ReadObjects()
{
	while (m_csv->ReadNextRow())
	{
		const char **row = m_csv->GetRow();

        if ( m_csv->GetItemCount() == 0 )
            continue;

		if (strcmp(row[0], "end") == 0)
			break;

        if ( **row == '#' )
            continue;

        unsigned int tokenCount = m_csv->GetItemCount();

        if ( tokenCount == 5 )
        {
            // San Andreas Type 4.
            IDE::CObject *object = new IDE::CObject();

		    object->m_modelID = atoi(row[0]);
            object->m_modelName = strdup(row[1]);
		    object->m_textureName = strdup(row[2]);
		    object->m_drawDistance = atof(row[3]);
		    object->m_flags = atol(row[4]);

		    m_objects.push_back(object);
        }
        else if ( tokenCount == 4 )
        {
            // Type 0.
            IDE::CObject *object = new IDE::CObject();

            object->m_modelID = atoi(row[0]);
            object->m_modelName = strdup(row[1]);
            object->m_textureName = strdup(row[2]);
            object->m_drawDistance = 1500;
            object->m_flags = 0;

            m_objects.push_back(object);
        }
        else if ( tokenCount == 6 )
        {
            // Type 1.
            IDE::CObject *object = new IDE::CObject();

            object->m_modelID = atoi(row[0]);
            object->m_modelName = strdup(row[1]);
            object->m_textureName = strdup(row[2]);
            object->m_drawDistance = atof(row[4]);
            object->m_flags = atol(row[5]);

            m_objects.push_back(object);
        }
        else if ( tokenCount == 7 )
        {
            // Type 2.
            IDE::CObject *object = new IDE::CObject();

            object->m_modelID = atoi(row[0]);
            object->m_modelName = strdup(row[1]);
            object->m_textureName = strdup(row[2]);
            object->m_drawDistance = atof(row[4]);
            object->m_flags = atol(row[6]);

            m_objects.push_back(object);
        }
        else if ( tokenCount == 8 )
        {
            // Type 4.
            IDE::CObject *object = new IDE::CObject();

            object->m_modelID = atoi(row[0]);
            object->m_modelName = strdup(row[1]);
            object->m_textureName = strdup(row[2]);
            object->m_drawDistance = atof(row[4]);
            object->m_flags = atol(row[7]);

            m_objects.push_back(object);
        }
        else
        {
            m_csv->ParsingError( "unknown token count" );
        }
	}
}
