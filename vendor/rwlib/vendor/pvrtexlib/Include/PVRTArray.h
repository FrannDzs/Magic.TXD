/*!****************************************************************************

 @file         PVRTArray.h
 @copyright    Copyright (c) Imagination Technologies Limited.
 @brief        Expanding array template class. Allows appending and direct
               access. Mixing access methods should be approached with caution.

******************************************************************************/
#ifndef __PVRTARRAY_H__
#define __PVRTARRAY_H__

#include "PVRTGlobal.h"
#include "PVRTError.h"

/******************************************************************************
**  Classes
******************************************************************************/

/*!***************************************************************************
 @class       CPVRTArray
 @brief       Expanding array template class.
*****************************************************************************/
template<typename T>
class CPVRTArray
{
public:
	/*!***************************************************************************
	@brief     Blank constructor. Makes a default sized array.
	*****************************************************************************/
	CPVRTArray();

	/*!***************************************************************************
	@brief  	Constructor taking initial size of array in elements.
	@param[in]	uiSize	intial size of array
	*****************************************************************************/
	CPVRTArray(const unsigned int uiSize);

	/*!***************************************************************************
	@brief      Copy constructor.
	@param[in]	original	the other dynamic array
	*****************************************************************************/
	CPVRTArray(const CPVRTArray& original);

	/*!***************************************************************************
	@brief      constructor from ordinary array.
	@param[in]	pArray		an ordinary array
	@param[in]	uiSize		number of elements passed
	*****************************************************************************/
	CPVRTArray(const T* const pArray, const unsigned int uiSize);

	/*!***************************************************************************
	@brief      constructor from a capacity and initial value.
	@param[in]	uiSize		initial capacity
	@param[in]	val			value to populate with
	*****************************************************************************/
	CPVRTArray(const unsigned int uiSize, const T& val);

	/*!***************************************************************************
	@brief      Destructor.
	*****************************************************************************/
	virtual ~CPVRTArray();

	/*!***************************************************************************
	@brief      Inserts an element into the array, expanding it
				if necessary.
	@param[in]	pos		The position to insert the new element at
	@param[in]	addT	The element to insert
	@return 	The index of the new item or -1 on failure.
	*****************************************************************************/
	int Insert(const unsigned int pos, const T& addT);

	/*!***************************************************************************
	@brief      Appends an element to the end of the array, expanding it
				if necessary.
	@param[in]	addT	The element to append
	@return 	The index of the new item.
	*****************************************************************************/
	unsigned int Append(const T& addT);

	/*!***************************************************************************
	@brief      Creates space for a new item, but doesn't add. Instead
				returns the index of the new item.
	@return 	The index of the new item.
	*****************************************************************************/
	unsigned int Append();

	/*!***************************************************************************
	@brief      Clears the array.
	*****************************************************************************/
	void Clear();

	/*!***************************************************************************
	@brief      Changes the array to the new size
	@param[in]	uiSize		New size of array
	*****************************************************************************/
	EPVRTError Resize(const unsigned int uiSize);

	/*!***************************************************************************
	@brief      Expands array to new capacity
	@param[in]	uiSize		New capacity of array
	*****************************************************************************/
	EPVRTError SetCapacity(const unsigned int uiSize);

	/*!***************************************************************************
	@fn     	Copy
	@brief      A copy function. Will attempt to copy from other CPVRTArrays
				if this is possible.
	@param[in]	other	The CPVRTArray needing copied
	*****************************************************************************/
	template<typename T2>
	void Copy(const CPVRTArray<T2>& other);

	/*!***************************************************************************
	@brief      assignment operator.
	@param[in]	other	The CPVRTArray needing copied
	*****************************************************************************/
	CPVRTArray& operator=(const CPVRTArray<T>& other);

	/*!***************************************************************************
	@brief      appends an existing CPVRTArray on to this one.
	@param[in]	other		the array to append.
	*****************************************************************************/
	CPVRTArray& operator+=(const CPVRTArray<T>& other);

	/*!***************************************************************************
	@brief      Indexed access into array. Note that this has no error
				checking whatsoever
	@param[in]	uiIndex	index of element in array
	@return 	the element indexed
	*****************************************************************************/
	T& operator[](const unsigned int uiIndex);

	/*!***************************************************************************
	@brief      Indexed access into array. Note that this has no error checking whatsoever
	@param[in]	uiIndex	    index of element in array
	@return 	The element indexed
	*****************************************************************************/
	const T& operator[](const unsigned int uiIndex) const;

	/*!***************************************************************************
	@return 	Size of array
	@brief      Gives current size of array/number of elements
	*****************************************************************************/
	unsigned int GetSize() const;

	/*!***************************************************************************
	@brief      Gives the default size of array/number of elements
	@return 	Default size of array
	*****************************************************************************/
	static unsigned int GetDefaultSize();

	/*!***************************************************************************
	@brief      Gives current allocated size of array/number of elements
	@return 	Capacity of array
	*****************************************************************************/
	unsigned int GetCapacity() const;

	/*!***************************************************************************
	@brief      Indicates whether the given object resides inside the array. 
	@param[in]	object		The object to check in the array
	@return 	true if object is contained in this array.
	*****************************************************************************/
	bool Contains(const T& object) const;

	/*!***************************************************************************
	@brief     	Attempts to find the object in the array and returns a
				pointer if it is found, or NULL if not found. The time
				taken is O(N).
	@param[in]	object		The object to check in the array
	@return 	Pointer to the found object or NULL.
	*****************************************************************************/
	T* Find(const T& object) const;

	/*!***************************************************************************
	@brief      Performs a merge-sort on the array. Pred should be an object that
				defines a bool operator().
	@param[in]	predicate		The object which defines "bool operator()"
	*****************************************************************************/
	template<class Pred>
	void Sort(Pred predicate);

	/*!***************************************************************************
	@brief      Removes an element from the array.
	@param[in]	uiIndex		The index to remove
	@return 	success or failure
	*****************************************************************************/
	virtual EPVRTError Remove(unsigned int uiIndex);

	/*!***************************************************************************
	@brief    	Removes the last element. Simply decrements the size value
	@return 	success or failure
	*****************************************************************************/
	virtual EPVRTError RemoveLast();

protected:
	enum eBounds
	{
		eLowerBounds,
		eUpperBounds,
	};

	/*!***************************************************************************
	 @brief     Internal sort algorithm
	 @param[in]	first		The beginning index of the array
	 @param[in]	last		The last index of the array
	 @param[in]	predicate	A functor object to perform the comparison
	 *****************************************************************************/
	template<class Pred>
	void _Sort(unsigned int first, unsigned int last, Pred predicate);

	/*!***************************************************************************
	 @brief     Internal sort algorithm - in-place merge method
	 @param[in]	first		The beginning index of the array
	 @param[in]	middle		The middle index of the array
	 @param[in]	last		The last index of the array
	 @param[in]	len1		Length of first half of the array
	 @param[in]	len2		Length of the second half of the array
	 @param[in]	predicate	A functor object to perform the comparison
	 *****************************************************************************/
	template<class Pred>
	void _SortMerge(unsigned int first, unsigned int middle, unsigned int last, int len1, int len2, Pred predicate);

	/*!***************************************************************************
	 @brief     Internal sort algorithm - returns the bounded index of the range
	 @param[in]	first		The beginning index of the array
	 @param[in]	last		The last index of the array
	 @param[in]	v           Comparison object
	 @param[in]	bounds		Which bound to check (upper or lower)
	 @param[in]	predicate	A functor object to perform the comparison
	 *****************************************************************************/
	template<class Pred>
	unsigned int _SortBounds(unsigned int first, unsigned int last, const T& v, eBounds bounds, Pred predicate);

	/*!***************************************************************************
	 @brief     Internal sort algorithm - rotates the contents of the array such
	            that the middle becomes the first element
	 @param[in]	first		The beginning index of the array
	 @param[in]	middle		The middle index of the array
	 @param[in]	last        The last index of the array
	 *****************************************************************************/
	int _SortRotate(unsigned int first, unsigned int middle, unsigned int last);

protected:
	unsigned int 	m_uiSize;		/*!< Current size of contents of array */
	unsigned int	m_uiCapacity;	/*!< Currently allocated size of array */
	T				*m_pArray;		/*!< The actual array itself */
};

// note "this" is required for ISO standard, C++ and gcc complains otherwise
// http://lists.apple.com/archives/Xcode-users//2005/Dec/msg00644.html

/*!***************************************************************************
 @class       CPVRTArrayManagedPointers
 @brief       Maintains an array of managed pointers.
*****************************************************************************/
template<typename T>
class CPVRTArrayManagedPointers : public CPVRTArray<T*>
{
public:
	/*!***************************************************************************
	@brief     Destructor.
	*****************************************************************************/
    virtual ~CPVRTArrayManagedPointers();

	/*!***************************************************************************
	@brief      Removes an element from the array.
	@param[in]	uiIndex		The index to remove.
	@return 	success or failure
	*****************************************************************************/
	virtual EPVRTError Remove(unsigned int uiIndex);

	/*!***************************************************************************
	@brief      Removes the last element. Simply decrements the size value
	@return 	success or failure
	*****************************************************************************/
	virtual EPVRTError RemoveLast();
};

#endif // __PVRTARRAY_H__

/*****************************************************************************
End of file (PVRTArray.h)
*****************************************************************************/

