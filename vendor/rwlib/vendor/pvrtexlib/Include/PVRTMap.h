/*!****************************************************************************

 @file         PVRTMap.h
 @copyright    Copyright (c) Imagination Technologies Limited.
 @brief        A simple and easy-to-use implementation of a map.

******************************************************************************/
#ifndef __PVRTMAP_H__
#define __PVRTMAP_H__

#include "PVRTArray.h"

/*!***************************************************************************
 @class		CPVRTMap
 @brief		Expanding map template class.
 @details   A simple and easy-to-use implementation of a map.
*****************************************************************************/
template <typename KeyType, typename DataType>
class CPVRTMap
{
public:

	/*!***********************************************************************
	 @brief      	Constructor for a CPVRTMap.
	 @return		A new CPVRTMap.
	*************************************************************************/
	CPVRTMap();

	/*!***********************************************************************
	 @brief      	Destructor for a CPVRTMap.
	*************************************************************************/
	~CPVRTMap();

	EPVRTError Reserve(const PVRTuint32 uiSize);

	/*!***********************************************************************
	 @brief      	Returns the number of meaningful members in the map.
	 @return		Number of meaningful members in the map.
	*************************************************************************/
	PVRTuint32 GetSize() const;

	/*!***********************************************************************
	 @brief      	Gets the position of a particular key/data within the map.
					If the return value is exactly equal to the value of 
					GetSize() then the item has not been found.
	 @param[in]		key     Key type
	 @return		The index value for a mapped item.
	*************************************************************************/
	PVRTuint32 GetIndexOf(const KeyType key) const;

	/*!***********************************************************************
	 @brief      	Returns a pointer to the Data at a particular index. 
					If the index supplied is not valid, NULL is returned 
					instead. Deletion of data at this pointer will lead
					to undefined behaviour.
	 @param[in]		uiIndex     Index number
	 @return		Data type at the specified position.
	*************************************************************************/
	const DataType* GetDataAtIndex(const PVRTuint32 uiIndex) const;

	/*!***********************************************************************
	 @brief      	If a mapping already exists for 'key' then it will return 
					the associated data. If no mapping currently exists, a new 
					element is created in place.
	 @param[in]		key     Key type
	 @return		Data that is mapped to 'key'.
	*************************************************************************/
	DataType& operator[] (const KeyType key);

	/*!***********************************************************************
	 @brief      	Removes an element from the map if it exists.
	 @param[in]		key     Key type
	 @return		Returns PVR_FAIL if item doesn't exist. 
					Otherwise returns PVR_SUCCESS.
	*************************************************************************/
	EPVRTError Remove(const KeyType key);

	/*!***********************************************************************
	 @brief      	Clears the Map of all data values.
	*************************************************************************/
	void Clear();

	/*!***********************************************************************
	 @brief      	Checks whether or not data exists for the specified key.
	 @param[in]		key     Key type
	 @return		Whether data exists for the specified key or not.
	*************************************************************************/
	bool Exists(const KeyType key) const;

private:

	
	CPVRTArray<KeyType> m_Keys; /*!< Array of all the keys. Indices match m_Data. */

	CPVRTArray<DataType> m_Data; /*!< Array of pointers to all the allocated data. */

	PVRTuint32 m_uiSize; /*!< The number of meaningful members in the map. */
};

#endif // __PVRTMAP_H__

/*****************************************************************************
End of file (PVRTMap.h)
*****************************************************************************/

