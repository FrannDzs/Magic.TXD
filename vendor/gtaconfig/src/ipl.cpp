#include "StdInc.h"

// Load up some IPL file
CIPL*	LoadIPL(const filePath& filename)
{
	CCSV *csv = LoadCSV(filename);

	if (!csv)
		return NULL;

	return new CIPL(csv);
}

CIPL::CIPL(CCSV *csv)
{
	m_csv = csv;

	// We need to catch the inst instruction
	while (csv->ReadNextRow())
	{
        if ( csv->GetItemCount() == 0 )
            continue;

		const char *tag = csv->GetRowItem(0);

		if ( strcmp(tag, "inst") == 0 )
        {
			ReadInstances();
        }
	}
}

CIPL::~CIPL()
{
	instanceMap_t::iterator iter;

	for (iter = m_instances.begin(); iter != m_instances.end(); iter++)
    {
        CInstance *inst = iter->second;

        free( inst->m_name );

		delete inst;
    }

	delete m_csv;
}

void	CIPL::ReadInstances()
{
    unsigned int instCount = 0;

	while (m_csv->ReadNextRow())
	{
		const char **row = m_csv->GetRow();

        if ( m_csv->GetItemCount() == 0 )
            continue;

		if (strcmp(row[0], "end") == 0)
			break;

        if ( **row == '#' )
            continue;

        unsigned int itemCount = m_csv->GetItemCount();

        CInstance *inst = NULL;

        if ( itemCount == 11 )  // San Andreas map line.
        {
		    inst = new CInstance();

		    inst->m_modelID = atoi(row[0]);

		    inst->m_name = strdup( row[1] );

		    inst->m_interior = atoi(row[2]);

		    inst->m_position[0] = atof(row[3]);
		    inst->m_position[1] = atof(row[4]);
		    inst->m_position[2] = atof(row[5]);

            // Since quaternion conversion is very difficult we leave
            // this task up to the processing tool.
		    inst->m_quat[0] = atof(row[6]);
		    inst->m_quat[1] = atof(row[7]);
		    inst->m_quat[2] = atof(row[8]);
		    inst->m_quat[3] = atof(row[9]);

		    inst->m_lod = atoi(row[10]);
            inst->m_instIndex = instCount;

		    if ( inst->m_lod != -1 )
            {
			    m_isLod.push_back( inst->m_lod );
            }

            inst->m_version = eInstanceVersion::GTASA;
        }
        else if ( itemCount == 12 ) // GTA3 and GTAVC map line.
        {
		    inst = new CInstance();

		    inst->m_modelID = atoi(row[0]);

		    inst->m_name = strdup( row[1] );

		    inst->m_position[0] = atof(row[2]);
		    inst->m_position[1] = atof(row[3]);
		    inst->m_position[2] = atof(row[4]);

            // Since quaternion conversion is very difficult we leave
            // this task up to the processing tool.
		    inst->m_quat[0] = atof(row[8]);
		    inst->m_quat[1] = atof(row[9]);
		    inst->m_quat[2] = atof(row[10]);
		    inst->m_quat[3] = atof(row[11]);

		    inst->m_lod = -1;
            inst->m_instIndex = instCount;

            inst->m_version = eInstanceVersion::GTA3;
        }
        else if ( itemCount == 13 ) // GTAVC map line.
        {
		    inst = new CInstance();

		    inst->m_modelID = atoi(row[0]);

		    inst->m_name = strdup( row[1] );

            inst->m_interior = atoi( row[2] );

		    inst->m_position[0] = atof(row[3]);
		    inst->m_position[1] = atof(row[4]);
		    inst->m_position[2] = atof(row[5]);

            // Since quaternion conversion is very difficult we leave
            // this task up to the processing tool.
		    inst->m_quat[0] = atof(row[9]);
		    inst->m_quat[1] = atof(row[10]);
		    inst->m_quat[2] = atof(row[11]);
		    inst->m_quat[3] = atof(row[12]);

		    inst->m_lod = -1;
            inst->m_instIndex = instCount;

            inst->m_version = eInstanceVersion::GTAVC;
        }

        if ( inst == NULL )
        {
            // Unknown instance line.
            m_csv->ParsingError( "unknown instance line format" );
        }
        else
        {
	        m_instances[ instCount ] = inst;
        }

        instCount++;
	}
}

bool	CIPL::IsLOD(unsigned int id)
{
	return std::find( m_isLod.begin(), m_isLod.end(), id ) != m_isLod.end();
}

CInstance*	CIPL::GetLod(CInstance *scene)
{
    instanceMap_t::iterator iter = m_instances.find( scene->m_lod );

    if ( iter != m_instances.end() )
        return iter->second;

    return NULL;
}
