#ifndef _INI_
#define _INI_

#include "syntax.h"

#include <sdk/Vector.h>

class CINI
{
					CINI(const char *buffer);

public:
					~CINI();

	friend CINI*	LoadINI(CFile *stream);

	class Entry
	{
	public:
		class Setting
		{
		public:
			~Setting()
			{
				free(key);
				free(value);
			}

			char *key;
			char *value;
		};

		Entry(char *name)
		{
			m_name = name;
		}

		~Entry()
		{
			for ( Setting *set : this->settings )
			{
				delete set;
			}
			free(m_name);
		}

		Setting*	Find(const char *key)
		{
			for ( Setting *set : this->settings )
			{
				if (strcmp(set->key, key) == 0)
					return set;
			}

			return nullptr;
		}

		void		Set(const char *key, const char *value)
		{
			Setting *set = Find(key);

			if (set)
			{
				free(set->value);

				set->value = _strdup(value);
				return;
			}

			set = new Setting();

			set->key = _strdup(key);
			set->value = _strdup(value);

			settings.AddToBack( set );
		}

		inline const char*	Get(const char *key)
		{
			Setting *set = Find(key);

			if (!set)
				return NULL;

			return set->value;
		}

		inline int GetInt(const char *key, int defaultValue = 0)
		{
			const char *value = Get(key);

			if (!value)
				return defaultValue;

			return atoi(value);
		}

		inline double GetFloat(const char *key, double defaultValue = 0.0f)
		{
			const char *value = Get(key);

			if (!value)
				return defaultValue;

			return atof(value);
		}

		inline bool GetBool(const char *key, bool defaultValue = false)
		{
			const char *value = Get(key);

			if (!value)
				return defaultValue;

			return (strcmp(value, "true") == 0) || (unsigned int)atoi(value) != 0;
		}

        template <typename callbackType>
        inline void ForAllEntries( const callbackType& cb ) const
        {
            for ( const Setting *cfg : this->settings )
            {
                cb( *cfg );
            }
        }

		char*		m_name;

	private:
		typedef eir::Vector <Setting*, CRTHeapAllocator> settingList_t;

		settingList_t settings;
	};

	Entry*			GetEntry(const char *entry);

	const char*		Get(const char *entry, const char *key);
	int				GetInt(const char *entry, const char *key);
	double			GetFloat(const char *entry, const char *key);

    template <typename callbackType>
    inline void     ForAllEntries( const char *entryName, callbackType& cb )
    {
        if ( Entry *section = this->GetEntry( entryName ) )
        {
            section->ForAllEntries(
                [&]( Entry::Setting& cfg )
            {
                cb( cfg.key );
            });
        }
    }

private:
	typedef eir::Vector <Entry*, CRTHeapAllocator> entryList_t;

	entryList_t		entries;
};

CINI*	LoadINI(CFile *stream);

#endif //_INI_
