/*
 *  GenericList.cpp
 *
 *  Implement a generic doubly linked list.
 */

#ifndef __GENERIC_LIST_HPP__
#define __GENERIC_LIST_HPP__

#include "freertos/FreeRTOS.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>


/*
 *  Class holding a generic item in the linked list class.
 */
template <typename T>
class LinkListItem
{
public:
    /*
     *  Parent of this item.
     */
    LinkListItem<T> *_parent;

    /*
     *  Child of this item.
     */
    LinkListItem<T> *_child;

    /*
     *  Data item being stored.
     */
    T *_data;
};

/*
 *  
 */
template <typename T>
class GenericLinkList
{
public:
    /*
    *  Function that can be used to find a data item in the linked list
    *  with the matching uint32_t key.
    * 
    *  uint32_t - key to find.
    *  void * - data item that should be checked.
    * 
    *  returns - true if the oject has the matching key, false otherwise.
    */
    typedef bool (*comparison_function_t)(uint32_t, T *);

private:
    /*
     *  Head of the linked list.  
     */
    LinkListItem<T> *_head;

    /*
     *  Tail of the list.
     */
    LinkListItem<T> *_tail;

    /*
     *  Default constructor.
     */
    GenericLinkList();

    /*
     *  Method used to compare a key with the one stored in a list item.
     */
    comparison_function_t _comparisonFunction;

public:
    /*
     *  Constructor.
     */
    explicit GenericLinkList(comparison_function_t comparisonFunction);

    /*
     *  Determine if the linked list is empty.
     */
    bool IsEmpty();

    /*
     *  Use the comparison to find an item in the linked list.
     */
    T *FindItem(uint32_t);

    /*
     *  Remove (and return a pointer to) the first item in the list.
     */
    T *RemoveItemAtHead();

    /*
     *  Remove the specified item (specified by the key) from the list.
     */
    T *RemoveItem(uint32_t);

    /*
     *  Add an item to the head of the list.
     */
    bool AddToHead(T *);

    /*
     *  Add an item to the tail of the list.
     */
    bool AddToTail(T *);
};

/****************************************************************************
 * @brief Constructor
 *
 * Description:
 *  Create a new object with no comparison method.
 *
 ****************************************************************************/
template <typename T>
GenericLinkList<T>::GenericLinkList()
{
    _head = nullptr;
    _tail = nullptr;
    _comparisonFunction = nullptr;
}

/****************************************************************************
 * @brief Constructor
 *
 * Description:
 *  Create a new object with the specified comparison method.
 *
 ****************************************************************************/
template <typename T>
GenericLinkList<T>::GenericLinkList(comparison_function_t comparisonFunction) : GenericLinkList()
{
    _comparisonFunction = comparisonFunction;
}


/****************************************************************************
 * @brief IsEmpty
 *
 * Description:
 *  Check to see if the list is empty.
 *
 * Returned Value:
 *  true if the list is empty, false otherwise.
 *
 ****************************************************************************/
template <typename T>
bool GenericLinkList<T>::IsEmpty()
{
    return(!_head);
}

/****************************************************************************
 * @brief FindItem
 *
 * Description:
 *  Search the list for a data item with the specified uint32_t key.
 *
 * Input Parameters:
 *  key - key value to search for.
 *
 * Returned Value:
 *  Pointer to the data item (as a void *) if a matching key was found,
 *  nullptr if no match was found.
 *
 ****************************************************************************/
template <typename T>
T *GenericLinkList<T>::FindItem(uint32_t key)
{
    LinkListItem<T> *item = _head;
    while (item)
    {
        if ((*_comparisonFunction)(key, item->_data))
        {
            return((T *) item->_data);

        }
        item = item->_child;
    }
    return(nullptr);
}

/****************************************************************************
 * @brief RemoveItem
 *
 * Description:
 *  Search the list for a data item with the specified uint32_t key.  If the
 *  item is found then unlink it from the list and return a pointer to the
 *  data item.
 *
 * Input Parameters:
 *  key - key value to search for.
 *
 * Returned Value:
 *  Pointer to the data item (as a void *) if a matching key was found,
 *  nullptr if no match was found.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
template <typename T>
T *GenericLinkList<T>::RemoveItem(uint32_t key)
{
    if (!_comparisonFunction)
    {
        return(nullptr);
    }

    LinkListItem<T> *item = _head;
    T *data = nullptr;
    while (item && !data)
    {
        if ((*_comparisonFunction)(key, item->_data))
        {
            data = item->_data;
            if (item == _head)
            {
                if (_head == _tail)
                {
                    /*
                    *  Item is the only item in the list so the head and the tail are the same.
                    *  Simply empty the list by setting the head and tail to nullptr;
                    */
                    _head = nullptr;
                    _tail = nullptr;
                }
                else
                {
                    /*
                    *  Item is the head of a list with more than one item.
                    *  The child of the head of the list becomes the head of the list
                    *  and it then has no parent.
                    */
                    _head = _head->_child;
                    _head->_parent = nullptr;
                }
            }
            else
            {
                if (item == _tail)
                {
                    /*
                    *  Item is the tail of a list with more than one item.
                    *  Note that if this had been the only item in the list then
                    *  the if clause above would have caught this case.
                    * 
                    *  The parent of the tail becomes the tail and it has no children.
                    */
                    _tail = _tail->_parent;
                    _tail->_child = nullptr;
                }
                else
                {
                    /*
                    *  Item is in the middle of a list and there is more than one
                    *  item in the list.
                    */
                   item->_parent->_child = item->_child;
                   item->_child->_parent = item->_parent;
                }
            }
            vPortFree(item);
        }
        else
        {
            item = item->_child;     /* No match so move on to next item in the list */
        }
    }

    return(data);
}


/****************************************************************************
 * @brief RemoveItemAtHead
 *
 * Description:
 *  Remove the item at the head of the linked list.
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  Pointer (as held in the linked list structure) to the data item at the
 *  head of the list.  This is a generic void * pointer and it is the
 *  responsibility of the calling application to cast the pointer to the
 *  correct type.
 * 
 *  nullptr will be returned if the list is empty.
 *
 ****************************************************************************/
template <typename T>
T *GenericLinkList<T>::RemoveItemAtHead()
{
    T *result = nullptr;

    if (_head != nullptr)
    {
        LinkListItem<T> *item = _head;
        if (_tail == _head)
        {
            /*
             *  Only one item in the list so clear the list.
             */
            _head = nullptr;
            _tail = nullptr;                
        }
        else
        {
            _head = item->_child;           /* Head is now the second item. */
            _head->_parent = nullptr;       /* Head of a list has no parents. */
        }
        result = item->_data;
        vPortFree(item);
    }
    return(result);
}

/****************************************************************************
 * @brief AddToHead
 *
 * Description:
 *  Add a data item to the head of the list.  The current head of the list
 *  will become a child of the newly created linked list entry,
 *
 * Input Parameters:
 *  data - data item to add to the linked list.
 *
 * Returned Value:
 *  true if the item is added successfully, false otherwise.
 *
 ****************************************************************************/
template <typename T>
bool GenericLinkList<T>::AddToHead(T *data)
{
    if (!data)
    {
        return(false);
    }

    LinkListItem<T> *item = static_cast<LinkListItem<T> *>(pvPortMalloc(sizeof(LinkListItem<T>)));

    if (item)
    {
        item->_data = data;
        item->_parent = nullptr;
        item->_child = _head;
        if (_head)
        {
            _head->_parent = item;
        }
        else
        {
            _tail = item;
        }
        _head = item;
    }

    return(true);
}

/****************************************************************************
 * @brief AddToTail
 *
 * Description:
 *  Add a data item to the tail of the list.  The current tail item of the list
 *  will become a parent of the newly created linked list entry,
 *
 * Input Parameters:
 *  data - data item to add to the linked list.
 *
 * Returned Value:
 *  true if the item could be added to the end of the list, false otherwise.
 *
 ****************************************************************************/
template <typename T>
bool GenericLinkList<T>::AddToTail(T *data)
{
    if (!data)
    {
        return(false);
    }

    LinkListItem<T> *item = static_cast<LinkListItem<T> *>(pvPortMalloc(sizeof(LinkListItem<T>)));

    if (item)
    {
        item->_data = data;
        item->_child = nullptr;
        if (_tail)
        {
            _tail->_child = item;
        }
        else
        {
            _head = item;
        }
        item->_parent = _tail;
        _tail = item;
    }

    return(true);
}

#endif  /* __GENERIC_LIST_HPP__ */