#ifndef _IPL_
#define _IPL_

enum class eInstanceVersion
{
    GTA3,
    GTAVC,
    GTASA
};

class CInstance
{
	friend class CIPL;

public:
	unsigned int	m_modelID;
	char*			m_name;

	unsigned int	m_interior;

	double			m_position[3];
    double			m_quat[4];

	long			m_flags;

	int				m_lod;
	int				m_lodID;

    // Meta-data.
    unsigned int    m_instIndex;

    eInstanceVersion m_version;
};

typedef std::list <CInstance*> instanceList_t;
typedef	std::map <unsigned int, CInstance*> instanceMap_t;

class CIPL
{
	friend CIPL*	LoadIPL( const filePath& filename );
public:
					CIPL(CCSV *csv);
					~CIPL();

	bool			IsLOD(unsigned int id);
	CInstance*		GetLod(CInstance *obj);

	instanceMap_t	m_instances;
private:
	void			ReadInstances();

	CCSV*			m_csv;

    typedef std::list <int> lodList_t;

    lodList_t       m_isLod;
};

CIPL*	LoadIPL(const filePath& filename);

#endif
